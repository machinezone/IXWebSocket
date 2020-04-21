/*
 *  IXCobraBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraBot.h"

#include "IXQueueManager.h"
#include <algorithm>
#include <chrono>
#include <ixcobra/IXCobraConnection.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

namespace ix
{
    int64_t CobraBot::run(const CobraConfig& config,
                          const std::string& channel,
                          const std::string& filter,
                          const std::string& position,
                          bool verbose,
                          size_t maxQueueSize,
                          bool useQueue,
                          bool enableHeartbeat,
                          int runtime)
    {
        ix::CobraConnection conn;
        conn.configure(config);
        conn.connect();

        Json::FastWriter jsonWriter;
        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        std::atomic<uint64_t> sentCountTotal(0);
        std::atomic<uint64_t> receivedCountTotal(0);
        std::atomic<uint64_t> sentCountPerSecs(0);
        std::atomic<uint64_t> receivedCountPerSecs(0);
        std::atomic<bool> stop(false);
        std::atomic<bool> throttled(false);
        std::atomic<bool> fatalCobraError(false);

        QueueManager queueManager(maxQueueSize);

        auto timer = [&sentCount,
                      &receivedCount,
                      &sentCountTotal,
                      &receivedCountTotal,
                      &sentCountPerSecs,
                      &receivedCountPerSecs,
                      &stop] {
            while (!stop)
            {
                //
                // We cannot write to sentCount and receivedCount
                // as those are used externally, so we need to introduce
                // our own counters
                //
                spdlog::info("messages received {} {} sent {} {}",
                             receivedCountPerSecs,
                             receivedCountTotal,
                             sentCountPerSecs,
                             sentCountTotal);
                receivedCountPerSecs = receivedCount - receivedCountTotal;
                sentCountPerSecs = sentCount - receivedCountTotal;

                receivedCountTotal += receivedCountPerSecs;
                sentCountTotal += sentCountPerSecs;

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }

            spdlog::info("timer thread done");
        };

        std::thread t1(timer);

        auto heartbeat = [&sentCount, &receivedCount, &stop, &enableHeartbeat] {
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
                    spdlog::error("no messages received or sent for 1 minute, exiting");
                    exit(1);
                }
                state = currentState;

                auto duration = std::chrono::minutes(1);
                std::this_thread::sleep_for(duration);
            }

            spdlog::info("heartbeat thread done");
        };

        std::thread t2(heartbeat);

        auto sender =
            [this, &queueManager, verbose, &sentCount, &stop, &throttled, &fatalCobraError] {
                while (true)
                {
                    auto data = queueManager.pop();
                    Json::Value msg = data.first;
                    std::string position = data.second;

                    if (stop) break;
                    if (msg.isNull()) continue;

                    if (_onBotMessageCallback &&
                        _onBotMessageCallback(msg, position, verbose, throttled, fatalCobraError))
                    {
                        // That might be too noisy
                        if (verbose)
                        {
                            spdlog::info("cobra bot: sending succesfull");
                        }
                        ++sentCount;
                    }
                    else
                    {
                        spdlog::error("cobra bot: error sending");
                    }

                    if (stop) break;
                }

                spdlog::info("sender thread done");
            };

        std::thread t3(sender);

        std::string subscriptionPosition(position);

        conn.setEventCallback([this,
                               &conn,
                               &channel,
                               &filter,
                               &subscriptionPosition,
                               &jsonWriter,
                               verbose,
                               &throttled,
                               &receivedCount,
                               &fatalCobraError,
                               &useQueue,
                               &queueManager,
                               &sentCount](const CobraEventPtr& event) {
            if (event->type == ix::CobraEventType::Open)
            {
                spdlog::info("Subscriber connected");

                for (auto&& it : event->headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (event->type == ix::CobraEventType::Closed)
            {
                spdlog::info("Subscriber closed: {}", event->errMsg);
            }
            else if (event->type == ix::CobraEventType::Authenticated)
            {
                spdlog::info("Subscriber authenticated");
                spdlog::info("Subscribing to {} at position {}", channel, subscriptionPosition);
                spdlog::info("Using filter: {}", filter);
                conn.subscribe(channel,
                               filter,
                               subscriptionPosition,
                               [this,
                                &jsonWriter,
                                verbose,
                                &throttled,
                                &receivedCount,
                                &queueManager,
                                &useQueue,
                                &subscriptionPosition,
                                &fatalCobraError,
                                &sentCount](const Json::Value& msg, const std::string& position) {
                                   if (verbose)
                                   {
                                       spdlog::info("Subscriber received message {} -> {}",
                                                    position,
                                                    jsonWriter.write(msg));
                                   }

                                   subscriptionPosition = position;

                                   // If we cannot send to sentry fast enough, drop the message
                                   if (throttled)
                                   {
                                       return;
                                   }

                                   ++receivedCount;

                                   if (useQueue)
                                   {
                                       queueManager.add(msg, position);
                                   }
                                   else
                                   {
                                       if (_onBotMessageCallback &&
                                           _onBotMessageCallback(
                                               msg, position, verbose, throttled, fatalCobraError))
                                       {
                                           // That might be too noisy
                                           if (verbose)
                                           {
                                               spdlog::info("cobra bot: sending succesfull");
                                           }
                                           ++sentCount;
                                       }
                                       else
                                       {
                                           spdlog::error("cobra bot: error sending");
                                       }
                                   }
                               });
            }
            else if (event->type == ix::CobraEventType::Subscribed)
            {
                spdlog::info("Subscriber: subscribed to channel {}", event->subscriptionId);
            }
            else if (event->type == ix::CobraEventType::UnSubscribed)
            {
                spdlog::info("Subscriber: unsubscribed from channel {}", event->subscriptionId);
            }
            else if (event->type == ix::CobraEventType::Error)
            {
                spdlog::error("Subscriber: error {}", event->errMsg);
            }
            else if (event->type == ix::CobraEventType::Published)
            {
                spdlog::error("Published message hacked: {}", event->msgId);
            }
            else if (event->type == ix::CobraEventType::Pong)
            {
                spdlog::info("Received websocket pong: {}", event->errMsg);
            }
            else if (event->type == ix::CobraEventType::HandshakeError)
            {
                spdlog::error("Subscriber: Handshake error: {}", event->errMsg);
                fatalCobraError = true;
            }
            else if (event->type == ix::CobraEventType::AuthenticationError)
            {
                spdlog::error("Subscriber: Authentication error: {}", event->errMsg);
                fatalCobraError = true;
            }
            else if (event->type == ix::CobraEventType::SubscriptionError)
            {
                spdlog::error("Subscriber: Subscription error: {}", event->errMsg);
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

        // sentry sender thread
        t3.join();

        return fatalCobraError ? -1 : (int64_t) sentCount;
    }

    void CobraBot::setOnBotMessageCallback(const OnBotMessageCallback& callback)
    {
        _onBotMessageCallback = callback;
    }
} // namespace ix
