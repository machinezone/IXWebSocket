/*
 *  IXCobraToStatsdBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToStatsdBot.h"
#include "IXQueueManager.h"
#include "IXStatsdClient.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ixcobra/IXCobraConnection.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

namespace ix
{
    // fields are command line argument that can be specified multiple times
    std::vector<std::string> parseFields(const std::string& fields)
    {
        std::vector<std::string> tokens;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(fields);

        while (std::getline(tokenStream, token))
        {
            tokens.push_back(token);
        }

        return tokens;
    }

    //
    // Extract an attribute from a Json Value.
    // extractAttr("foo.bar", {"foo": {"bar": "baz"}}) => baz
    //
    Json::Value extractAttr(const std::string& attr, const Json::Value& jsonValue)
    {
        // Split by .
        std::string token;
        std::stringstream tokenStream(attr);

        Json::Value val(jsonValue);

        while (std::getline(tokenStream, token, '.'))
        {
            val = val[token];
        }

        return val;
    }

    int cobra_to_statsd_bot(const ix::CobraConfig& config,
                            const std::string& channel,
                            const std::string& filter,
                            const std::string& position,
                            StatsdClient& statsdClient,
                            const std::string& fields,
                            const std::string& gauge,
                            const std::string& timer,
                            bool verbose,
                            size_t maxQueueSize,
                            bool enableHeartbeat,
                            int runtime)
    {
        ix::CobraConnection conn;
        conn.configure(config);
        conn.connect();

        auto tokens = parseFields(fields);

        Json::FastWriter jsonWriter;
        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        std::atomic<bool> stop(false);
        std::atomic<bool> fatalCobraError(false);

        QueueManager queueManager(maxQueueSize);

        auto progress = [&sentCount, &receivedCount, &stop] {
            while (!stop)
            {
                spdlog::info("messages received {} sent {}", receivedCount, sentCount);

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }

            spdlog::info("timer thread done");
        };

        std::thread t1(progress);

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

        auto statsdSender = [&statsdClient, &queueManager, &sentCount, &tokens, &stop, &gauge, &timer, &fatalCobraError, &verbose] {
            while (true)
            {
                Json::Value msg = queueManager.pop();

                if (stop) return;
                if (msg.isNull()) continue;

                std::string id;
                for (auto&& attr : tokens)
                {
                    id += ".";
                    auto val = extractAttr(attr, msg);
                    id += val.asString();
                }

                if (gauge.empty() && timer.empty())
                {
                    statsdClient.count(id, 1);
                }
                else
                {
                    std::string attrName = (!gauge.empty()) ? gauge : timer;
                    auto val = extractAttr(attrName, msg);
                    size_t x;

                    if (val.isInt())
                    {
                        x = (size_t) val.asInt();
                    }
                    else if (val.isInt64())
                    {
                        x = (size_t) val.asInt64();
                    }
                    else if (val.isUInt())
                    {
                        x = (size_t) val.asUInt();
                    }
                    else if (val.isUInt64())
                    {
                        x = (size_t) val.asUInt64();
                    }
                    else if (val.isDouble())
                    {
                        x = (size_t) val.asUInt64();
                    }
                    else
                    {
                        spdlog::error("Gauge {} is not a numberic type", gauge);
                        fatalCobraError = true;
                        break;
                    }

                    if (verbose)
                    {
                        spdlog::info("{} - {} -> {}", id, attrName, x);
                    }

                    if (!gauge.empty())
                    {
                        statsdClient.gauge(id, x);
                    }
                    else
                    {
                        statsdClient.timing(id, x);
                    }
                }

                sentCount += 1;
            }
        };

        std::thread t3(statsdSender);

        conn.setEventCallback(
            [&conn, &channel, &filter, &position, &jsonWriter, verbose, &queueManager, &receivedCount, &fatalCobraError](const CobraEventPtr& event)
            {
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
                    conn.subscribe(channel,
                                   filter,
                                   position,
                                   [&jsonWriter, &queueManager, verbose, &receivedCount](
                                       const Json::Value& msg, const std::string& position) {
                                       if (verbose)
                                       {
                                           spdlog::info("Subscriber received message {} -> {}", position, jsonWriter.write(msg));
                                       }

                                       receivedCount++;

                                       ++receivedCount;
                                       queueManager.add(msg);
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
                    spdlog::info("Received websocket pong");
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
            for (int i = 0 ; i < runtime; ++i)
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

        // statsd sender thread
        t3.join();

        return fatalCobraError ? -1 : (int) sentCount;
    }
} // namespace ix
