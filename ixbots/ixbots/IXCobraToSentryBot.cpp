/*
 *  IXCobraToSentryBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToSentryBot.h"
#include "IXQueueManager.h"

#include <chrono>
#include <ixcobra/IXCobraConnection.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

namespace ix
{
    int cobra_to_sentry_bot(const CobraConfig& config,
                            const std::string& channel,
                            const std::string& filter,
                            const std::string& position,
                            SentryClient& sentryClient,
                            bool verbose,
                            bool strict,
                            size_t maxQueueSize,
                            bool enableHeartbeat,
                            int runtime)
    {
        ix::CobraConnection conn;
        conn.configure(config);
        conn.connect();

        Json::FastWriter jsonWriter;
        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        std::atomic<bool> errorSending(false);
        std::atomic<bool> stop(false);
        std::atomic<bool> throttled(false);
        std::atomic<bool> fatalCobraError(false);

        QueueManager queueManager(maxQueueSize);

        auto timer = [&sentCount, &receivedCount, &stop] {
            while (!stop)
            {
                spdlog::info("messages received {} sent {}", receivedCount, sentCount);

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

        auto sentrySender =
            [&queueManager, verbose, &errorSending, &sentCount, &stop, &throttled, &sentryClient] {

                while (true)
                {
                    Json::Value msg = queueManager.pop();

                    if (stop) break;
                    if (msg.isNull()) continue;

                    auto ret = sentryClient.send(msg, verbose);
                    HttpResponsePtr response = ret.first;

                    if (!response)
                    {
                        spdlog::warn("Null HTTP Response");
                        continue;
                    }

                    if (verbose)
                    {
                        for (auto it : response->headers)
                        {
                            spdlog::info("{}: {}", it.first, it.second);
                        }

                        spdlog::info("Upload size: {}", response->uploadSize);
                        spdlog::info("Download size: {}", response->downloadSize);

                        spdlog::info("Status: {}", response->statusCode);
                        if (response->errorCode != HttpErrorCode::Ok)
                        {
                            spdlog::info("error message: {}", response->errorMsg);
                        }

                        if (response->headers["Content-Type"] != "application/octet-stream")
                        {
                            spdlog::info("payload: {}", response->payload);
                        }
                    }

                    if (response->statusCode != 200)
                    {
                        spdlog::error("Error sending data to sentry: {}", response->statusCode);
                        spdlog::error("Body: {}", ret.second);
                        spdlog::error("Response: {}", response->payload);
                        errorSending = true;

                        // Error 429 Too Many Requests
                        if (response->statusCode == 429)
                        {
                            auto retryAfter = response->headers["Retry-After"];
                            std::stringstream ss;
                            ss << retryAfter;
                            int seconds;
                            ss >> seconds;

                            if (!ss.eof() || ss.fail())
                            {
                                seconds = 30;
                                spdlog::warn("Error parsing Retry-After header. "
                                             "Using {} for the sleep duration",
                                             seconds);
                            }

                            spdlog::warn("Error 429 - Too Many Requests. ws will sleep "
                                         "and retry after {} seconds",
                                         retryAfter);

                            throttled = true;
                            auto duration = std::chrono::seconds(seconds);
                            std::this_thread::sleep_for(duration);
                            throttled = false;
                        }
                    }
                    else
                    {
                        ++sentCount;
                    }

                    if (stop) break;
                }

                spdlog::info("sentrySender thread done");
            };

        std::thread t3(sentrySender);

        conn.setEventCallback([&conn,
                               &channel,
                               &filter,
                               &position,
                               &jsonWriter,
                               verbose,
                               &throttled,
                               &receivedCount,
                               &fatalCobraError,
                               &queueManager](ix::CobraEventType eventType,
                                              const std::string& errMsg,
                                              const ix::WebSocketHttpHeaders& headers,
                                              const std::string& subscriptionId,
                                              CobraConnection::MsgId msgId) {
            if (eventType == ix::CobraEventType::Open)
            {
                spdlog::info("Subscriber connected");

                for (auto it : headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            if (eventType == ix::CobraEventType::Closed)
            {
                spdlog::info("Subscriber closed");
            }
            else if (eventType == ix::CobraEventType::Authenticated)
            {
                spdlog::info("Subscriber authenticated");
                conn.subscribe(channel,
                               filter,
                               position,
                               [&jsonWriter, verbose, &throttled, &receivedCount, &queueManager](
                                   const Json::Value& msg, const std::string& position) {
                                   if (verbose)
                                   {
                                       spdlog::info("Subscriber received message {} -> {}", position, jsonWriter.write(msg));
                                   }

                                   // If we cannot send to sentry fast enough, drop the message
                                   if (throttled)
                                   {
                                       return;
                                   }

                                   ++receivedCount;
                                   queueManager.add(msg);
                               });
            }
            else if (eventType == ix::CobraEventType::Subscribed)
            {
                spdlog::info("Subscriber: subscribed to channel {}", subscriptionId);
            }
            else if (eventType == ix::CobraEventType::UnSubscribed)
            {
                spdlog::info("Subscriber: unsubscribed from channel {}", subscriptionId);
            }
            else if (eventType == ix::CobraEventType::Error)
            {
                spdlog::error("Subscriber: error {}", errMsg);
            }
            else if (eventType == ix::CobraEventType::Published)
            {
                spdlog::error("Published message hacked: {}", msgId);
            }
            else if (eventType == ix::CobraEventType::Pong)
            {
                spdlog::info("Received websocket pong");
            }
            else if (eventType == ix::CobraEventType::HandshakeError)
            {
                spdlog::error("Subscriber: Handshake error: {}", errMsg);
                fatalCobraError = true;
            }
            else if (eventType == ix::CobraEventType::AuthenticationError)
            {
                spdlog::error("Subscriber: Authentication error: {}", errMsg);
                fatalCobraError = true;
            }
            else if (eventType == ix::CobraEventType::SubscriptionError)
            {
                spdlog::error("Subscriber: Subscription error: {}", errMsg);
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

                if (strict && errorSending) break;
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

                if (strict && errorSending) break;
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

        return ((strict && errorSending) || fatalCobraError) ? -1 : (int) sentCount;
    }
} // namespace ix
