/*
 *  IXCobraBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraBot.h"

#include <ixcobra/IXCobraConnection.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <ixwebsocket/IXSetThreadName.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

namespace ix
{
    int64_t CobraBot::run(const CobraBotConfig& botConfig)
    {
        auto config = botConfig.cobraConfig;
        auto channel = botConfig.channel;
        auto filter = botConfig.filter;
        auto position = botConfig.position;
        auto enableHeartbeat = botConfig.enableHeartbeat;
        auto heartBeatTimeout = botConfig.heartBeatTimeout;
        auto runtime = botConfig.runtime;
        auto maxEventsPerMinute = botConfig.maxEventsPerMinute;
        auto limitReceivedEvents = botConfig.limitReceivedEvents;
        auto batchSize = botConfig.batchSize;

        ix::CobraConnection conn;
        conn.configure(config);
        conn.connect();

        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        uint64_t sentCountTotal(0);
        uint64_t receivedCountTotal(0);
        uint64_t sentCountPerSecs(0);
        uint64_t receivedCountPerSecs(0);
        std::atomic<int> receivedCountPerMinutes(0);
        std::atomic<bool> stop(false);
        std::atomic<bool> throttled(false);
        std::atomic<bool> fatalCobraError(false);
        std::atomic<bool> stalledConnection(false);
        int minuteCounter = 0;

        auto timer = [&sentCount,
                      &receivedCount,
                      &sentCountTotal,
                      &receivedCountTotal,
                      &sentCountPerSecs,
                      &receivedCountPerSecs,
                      &receivedCountPerMinutes,
                      &minuteCounter,
                      &conn,
                      &stop] {
            setThreadName("Bot progress");
            while (!stop)
            {
                //
                // We cannot write to sentCount and receivedCount
                // as those are used externally, so we need to introduce
                // our own counters
                //
                std::stringstream ss;
                ss << "messages received "
                   << receivedCountPerSecs
                   << " "
                   << receivedCountTotal
                   << " sent " 
                   << sentCountPerSecs
                   << " "
                   << sentCountTotal;

                if (conn.isAuthenticated())
                {
                    CoreLogger::info(ss.str());
                }

                receivedCountPerSecs = receivedCount - receivedCountTotal;
                sentCountPerSecs = sentCount - sentCountTotal;

                receivedCountTotal += receivedCountPerSecs;
                sentCountTotal += sentCountPerSecs;

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);

                if (minuteCounter++ == 60)
                {
                    receivedCountPerMinutes = 0;
                    minuteCounter = 0;
                }
            }

            CoreLogger::info("timer thread done");
        };

        std::thread t1(timer);

        auto heartbeat = [&sentCount,
                          &receivedCount,
                          &stop,
                          &enableHeartbeat,
                          &heartBeatTimeout,
                          &stalledConnection]
        {
            setThreadName("Bot heartbeat");
            std::string state("na");

            if (!enableHeartbeat) return;

            while (!stop)
            {
                std::stringstream ss;
                ss << "messages received " << receivedCount;
                ss << "messages sent " << sentCount;

                std::string currentState = ss.str();

                if (currentState == state)
                {
                    ss.str("");
                    ss << "no messages received or sent for "
                       << heartBeatTimeout << " seconds, reconnecting";

                    CoreLogger::error(ss.str());
                    stalledConnection = true;
                }
                state = currentState;

                auto duration = std::chrono::seconds(heartBeatTimeout);
                std::this_thread::sleep_for(duration);
            }

            CoreLogger::info("heartbeat thread done");
        };

        std::thread t2(heartbeat);

        std::string subscriptionPosition(position);

        conn.setEventCallback([this,
                               &conn,
                               &channel,
                               &filter,
                               &subscriptionPosition,
                               &throttled,
                               &receivedCount,
                               &receivedCountPerMinutes,
                               maxEventsPerMinute,
                               limitReceivedEvents,
                               batchSize,
                               &fatalCobraError,
                               &sentCount](const CobraEventPtr& event) {
            if (event->type == ix::CobraEventType::Open)
            {
                CoreLogger::info("Subscriber connected");

                for (auto&& it : event->headers)
                {
                    CoreLogger::info(it.first + ": " + it.second);
                }
            }
            else if (event->type == ix::CobraEventType::Closed)
            {
                CoreLogger::info("Subscriber closed: {}" + event->errMsg);
            }
            else if (event->type == ix::CobraEventType::Authenticated)
            {
                CoreLogger::info("Subscriber authenticated");
                CoreLogger::info("Subscribing to " + channel);
                CoreLogger::info("Subscribing at position " + subscriptionPosition);
                CoreLogger::info("Subscribing with filter " + filter);
                conn.subscribe(channel, filter, subscriptionPosition, batchSize,
                    [&sentCount, &receivedCountPerMinutes,
                     maxEventsPerMinute, limitReceivedEvents,
                     &throttled, &receivedCount,
                     &subscriptionPosition, &fatalCobraError,
                     this](const Json::Value& msg, const std::string& position) {
                        subscriptionPosition = position;
                        ++receivedCount;

                        ++receivedCountPerMinutes;
                        if (limitReceivedEvents)
                        {
                            if (receivedCountPerMinutes > maxEventsPerMinute)
                            {
                                return;
                            }
                        }

                        // If we cannot send to sentry fast enough, drop the message
                        if (throttled)
                        {
                            return;
                        }

                        _onBotMessageCallback(
                            msg, position, throttled,
                            fatalCobraError, sentCount);
                    });
            }
            else if (event->type == ix::CobraEventType::Subscribed)
            {
                CoreLogger::info("Subscriber: subscribed to channel " + event->subscriptionId);
            }
            else if (event->type == ix::CobraEventType::UnSubscribed)
            {
                CoreLogger::info("Subscriber: unsubscribed from channel " + event->subscriptionId);
            }
            else if (event->type == ix::CobraEventType::Error)
            {
                CoreLogger::error("Subscriber: error " + event->errMsg);
            }
            else if (event->type == ix::CobraEventType::Published)
            {
                CoreLogger::error("Published message hacked: " + std::to_string(event->msgId));
            }
            else if (event->type == ix::CobraEventType::Pong)
            {
                CoreLogger::info("Received websocket pong: " + event->errMsg);
            }
            else if (event->type == ix::CobraEventType::HandshakeError)
            {
                CoreLogger::error("Subscriber: Handshake error: " + event->errMsg);
                fatalCobraError = true;
            }
            else if (event->type == ix::CobraEventType::AuthenticationError)
            {
                CoreLogger::error("Subscriber: Authentication error: " + event->errMsg);
                fatalCobraError = true;
            }
            else if (event->type == ix::CobraEventType::SubscriptionError)
            {
                CoreLogger::error("Subscriber: Subscription error: " + event->errMsg);
                fatalCobraError = true;
            }
        });

        // Run forever
        if (runtime == -1)
        {
            while (true)
            {
                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);

                if (fatalCobraError) break;

                if (stalledConnection)
                {
                    conn.disconnect();
                    conn.connect();
                    stalledConnection = false;
                }
            }
        }
        // Run for a duration, used by unittesting now
        else
        {
            for (int i = 0; i < runtime; ++i)
            {
                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);

                if (fatalCobraError) break;

                if (stalledConnection)
                {
                    conn.disconnect();
                    conn.connect();
                    stalledConnection = false;
                }
            }
        }

        //
        // Cleanup.
        // join all the bg threads and stop them.
        //
        conn.disconnect();
        stop = true;

        // progress thread
        t1.join();

        // heartbeat thread
        if (t2.joinable()) t2.join();

        return fatalCobraError ? -1 : (int64_t) sentCount;
    }

    void CobraBot::setOnBotMessageCallback(const OnBotMessageCallback& callback)
    {
        _onBotMessageCallback = callback;
    }

    std::string CobraBot::getDeviceIdentifier(const Json::Value& msg)
    {
        std::string deviceId("na");

        auto osName = msg["device"]["os_name"];
        if (osName == "Android")
        {
            deviceId = msg["device"]["model"].asString();
        }
        else if (osName == "iOS")
        {
            deviceId = msg["device"]["hardware_model"].asString();
        }

        return deviceId;
    }

} // namespace ix
