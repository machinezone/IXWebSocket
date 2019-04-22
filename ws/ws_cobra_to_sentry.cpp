/*
 *  ws_cobra_to_sentry.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <ixcobra/IXCobraConnection.h>

#include "IXSentryClient.h"

namespace ix
{
    int ws_cobra_to_sentry_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                const std::string& dsn,
                                bool verbose,
                                bool strict,
                                int jobs)
    {
        ix::CobraConnection conn;
        conn.configure(appkey, endpoint,
                       rolename, rolesecret,
                       ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        Json::FastWriter jsonWriter;
        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        std::atomic<bool> errorSending(false);
        std::atomic<bool> stop(false);

        std::mutex conditionVariableMutex;
        std::condition_variable condition;
        std::condition_variable progressCondition;
        std::queue<Json::Value> queue;

        auto sentrySender = [&condition, &progressCondition, &conditionVariableMutex,
                             &queue, verbose, &errorSending, &sentCount,
                             &stop, &dsn]
        {
            while (true)
            {
                Json::Value msg;

                {
                    std::unique_lock<std::mutex> lock(conditionVariableMutex);
                    condition.wait(lock, [&queue, &stop]{ return !queue.empty() && !stop; });

                    msg = queue.front();
                    queue.pop();
                }

                SentryClient sc(dsn);

                if (!sc.send(msg, verbose))
                {
                    errorSending = true;
                }
                else
                {
                    ++sentCount;
                }

                progressCondition.notify_one();

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

        conn.setEventCallback(
            [&conn, &channel, &jsonWriter,
             verbose, &receivedCount, &sentCount,
             &condition, &conditionVariableMutex,
             &progressCondition, &queue]
            (ix::CobraConnectionEventType eventType,
             const std::string& errMsg,
             const ix::WebSocketHttpHeaders& headers,
             const std::string& subscriptionId)
            {
                if (eventType == ix::CobraConnection_EventType_Open)
                {
                    std::cerr << "Subscriber: connected" << std::endl;

                    for (auto it : headers)
                    {
                        std::cerr << it.first << ": " << it.second << std::endl;
                    }
                }
                if (eventType == ix::CobraConnection_EventType_Closed)
                {
                    std::cerr << "Subscriber: closed" << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_Authenticated)
                {
                    std::cerr << "Subscriber authenticated" << std::endl;
                    conn.subscribe(channel,
                                   [&jsonWriter, verbose,
                                    &sentCount, &receivedCount,
                                    &condition, &conditionVariableMutex,
                                    &progressCondition, &queue]
                                   (const Json::Value& msg)
                                   {
                                       if (verbose)
                                       {
                                           std::cerr << jsonWriter.write(msg) << std::endl;
                                       }

                                       // If we cannot send to sentry fast enough, drop the message
                                       const uint64_t scaleFactor = 2;

                                       if (sentCount != 0 &&
                                           receivedCount != 0 &&
                                           (sentCount * scaleFactor < receivedCount))
                                       {
                                           std::cerr << "message dropped: sending is backlogged !"
                                                     << std::endl;

                                           condition.notify_one();
                                           progressCondition.notify_one();
                                           return;
                                       }

                                       ++receivedCount;

                                       {
                                           std::unique_lock<std::mutex> lock(conditionVariableMutex);
                                           queue.push(msg);
                                       }

                                       condition.notify_one();
                                       progressCondition.notify_one();
                                   });
                }
                else if (eventType == ix::CobraConnection_EventType_Subscribed)
                {
                    std::cerr << "Subscriber: subscribed to channel " << subscriptionId << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
                {
                    std::cerr << "Subscriber: unsubscribed from channel " << subscriptionId << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_Error)
                {
                    std::cerr << "Subscriber: error" << errMsg << std::endl;
                }
            }
        );

        std::mutex progressConditionVariableMutex;
        while (true)
        {
            std::unique_lock<std::mutex> lock(progressConditionVariableMutex);
            progressCondition.wait(lock);

            std::cout << "messages"
                      << " received " << receivedCount
                      << " sent " << sentCount
                      << std::endl;

            if (strict && errorSending) break;
        }

        conn.disconnect();

        // FIXME: join all the bg threads and stop them.

        return 0;
    }
}
