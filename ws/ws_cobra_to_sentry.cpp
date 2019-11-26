/*
 *  ws_cobra_to_sentry.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <ixcobra/IXCobraConnection.h>
#include <ixsentry/IXSentryClient.h>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

namespace ix
{
    int ws_cobra_to_sentry_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                const std::string& filter,
                                const std::string& dsn,
                                bool verbose,
                                bool strict,
                                int jobs)
    {
        ix::CobraConnection conn;
        conn.configure(
            appkey, endpoint, rolename, rolesecret, ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        Json::FastWriter jsonWriter;
        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        std::atomic<bool> errorSending(false);
        std::atomic<bool> stop(false);

        std::condition_variable condition;
        std::mutex conditionVariableMutex;
        std::queue<Json::Value> queue;

        auto timer = [&sentCount, &receivedCount] {
            while (true)
            {
                spdlog::info("messages received {} sent {}", receivedCount, sentCount);

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t1(timer);

        auto sentrySender = [&condition,
                             &conditionVariableMutex,
                             &queue,
                             verbose,
                             &errorSending,
                             &sentCount,
                             &stop,
                             &dsn] {
            SentryClient sentryClient(dsn);

            while (true)
            {
                Json::Value msg;

                {
                    std::unique_lock<std::mutex> lock(conditionVariableMutex);
                    condition.wait(lock, [&queue, &stop] { return !queue.empty() && !stop; });

                    msg = queue.front();
                    queue.pop();
                }

                auto ret = sentryClient.send(msg, verbose);
                HttpResponsePtr response = ret.first;

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
                }
                else
                {
                    ++sentCount;
                }

                if (stop) return;
            }
        };

        // Create a thread pool
        std::cerr << "Starting " << jobs << " sentry sender jobs" << std::endl;
        std::vector<std::thread> pool;
        for (int i = 0; i < jobs; i++)
        {
            pool.push_back(std::thread(sentrySender));
        }

        conn.setEventCallback([&conn,
                               &channel,
                               &filter,
                               &jsonWriter,
                               verbose,
                               &receivedCount,
                               &sentCount,
                               &condition,
                               &conditionVariableMutex,
                               &queue](ix::CobraConnectionEventType eventType,
                                       const std::string& errMsg,
                                       const ix::WebSocketHttpHeaders& headers,
                                       const std::string& subscriptionId,
                                       CobraConnection::MsgId msgId) {
            if (eventType == ix::CobraConnection_EventType_Open)
            {
                spdlog::info("Subscriber connected");

                for (auto it : headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            if (eventType == ix::CobraConnection_EventType_Closed)
            {
                spdlog::info("Subscriber closed");
            }
            else if (eventType == ix::CobraConnection_EventType_Authenticated)
            {
                std::cerr << "Subscriber authenticated" << std::endl;
                conn.subscribe(channel,
                               filter,
                               [&jsonWriter,
                                verbose,
                                &sentCount,
                                &receivedCount,
                                &condition,
                                &conditionVariableMutex,
                                &queue](const Json::Value& msg) {
                                   if (verbose)
                                   {
                                       spdlog::info(jsonWriter.write(msg));
                                   }

                                   // If we cannot send to sentry fast enough, drop the message
                                   const uint64_t scaleFactor = 2;

                                   if (sentCount != 0 && receivedCount != 0 &&
                                       (sentCount * scaleFactor < receivedCount))
                                   {
                                       spdlog::warn("message dropped: sending is backlogged !");

                                       condition.notify_one();
                                       return;
                                   }

                                   ++receivedCount;

                                   {
                                       std::unique_lock<std::mutex> lock(conditionVariableMutex);
                                       queue.push(msg);
                                   }

                                   condition.notify_one();
                               });
            }
            else if (eventType == ix::CobraConnection_EventType_Subscribed)
            {
                spdlog::info("Subscriber: subscribed to channel {}", subscriptionId);
            }
            else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
            {
                spdlog::info("Subscriber: unsubscribed from channel {}", subscriptionId);
            }
            else if (eventType == ix::CobraConnection_EventType_Error)
            {
                spdlog::error("Subscriber: error {}", errMsg);
            }
            else if (eventType == ix::CobraConnection_EventType_Published)
            {
                spdlog::error("Published message hacked: {}", msgId);
            }
        });

        while (true)
        {
            auto duration = std::chrono::seconds(1);
            std::this_thread::sleep_for(duration);

            if (strict && errorSending) break;
        }

        conn.disconnect();

        // join all the bg threads and stop them.
        stop = true;
        for (int i = 0; i < jobs; i++)
        {
            spdlog::error("joining thread {}", i);
            pool[i].join();
        }

        return (strict && errorSending) ? 1 : 0;
    }
} // namespace ix
