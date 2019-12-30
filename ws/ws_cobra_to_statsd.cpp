/*
 *  ws_cobra_to_statsd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ixcobra/IXCobraConnection.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <statsd_client.h>
#endif

namespace
{
    class QueueManager
    {
    public:
        QueueManager(size_t maxQueueSize, std::atomic<bool>& stop)
            : _maxQueueSize(maxQueueSize)
            , _stop(stop)
        {
        }

        Json::Value pop();
        void add(Json::Value msg);

    private:
        std::queue<Json::Value> _queue;
        std::mutex _mutex;
        std::condition_variable _condition;
        size_t _maxQueueSize;
        std::atomic<bool>& _stop;
    };

    Json::Value QueueManager::pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_queue.empty())
        {
            Json::Value val;
            return val;
        }

        _condition.wait(lock, [this] { return !_stop; });

        auto msg = _queue.front();
        _queue.pop();
        return msg;
    }

    void QueueManager::add(Json::Value msg)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        // if the sending is not fast enough there is no point
        // in queuing too many events.
        if (_queue.size() < _maxQueueSize)
        {
            _queue.push(msg);
            _condition.notify_one();
        }
    }
} // namespace

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
    std::string extractAttr(const std::string& attr, const Json::Value& jsonValue)
    {
        // Split by .
        std::string token;
        std::stringstream tokenStream(attr);

        Json::Value val(jsonValue);

        while (std::getline(tokenStream, token, '.'))
        {
            val = val[token];
        }

        return val.asString();
    }

    int ws_cobra_to_statsd_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                const std::string& filter,
                                const std::string& host,
                                int port,
                                const std::string& prefix,
                                const std::string& fields,
                                bool verbose,
                                const ix::SocketTLSOptions& tlsOptions)
    {
        ix::CobraConnection conn;
        conn.configure(appkey,
                       endpoint,
                       rolename,
                       rolesecret,
                       ix::WebSocketPerMessageDeflateOptions(true),
                       tlsOptions);
        conn.connect();

        auto tokens = parseFields(fields);

        Json::FastWriter jsonWriter;
        std::atomic<uint64_t> sentCount(0);
        std::atomic<uint64_t> receivedCount(0);
        std::atomic<bool> stop(false);

        size_t maxQueueSize = 1000;
        QueueManager queueManager(maxQueueSize, stop);

        auto timer = [&sentCount, &receivedCount] {
            while (true)
            {
                spdlog::info("messages received {} sent {}", receivedCount, sentCount);

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t1(timer);

        auto statsdSender = [&queueManager, &host, &port, &sentCount, &tokens, &prefix, &stop] {
            // statsd client
            // test with netcat as a server: `nc -ul 8125`
            bool statsdBatch = true;
#ifndef _WIN32
            statsd::StatsdClient statsdClient(host, port, prefix, statsdBatch);
#else
            int statsdClient;
#endif
            while (true)
            {
                Json::Value msg = queueManager.pop();

                if (msg.isNull()) continue;
                if (stop) return;

                std::string id;
                for (auto&& attr : tokens)
                {
                    id += ".";
                    id += extractAttr(attr, msg);
                }

                sentCount += 1;

#ifndef _WIN32
                statsdClient.count(id, 1);
#endif
            }
        };

        std::thread t2(statsdSender);

        conn.setEventCallback(
            [&conn, &channel, &filter, &jsonWriter, verbose, &queueManager, &receivedCount](
                ix::CobraConnectionEventType eventType,
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
                    spdlog::info("Subscriber authenticated");
                    conn.subscribe(channel,
                                   filter,
                                   [&jsonWriter, &queueManager, verbose, &receivedCount](
                                       const Json::Value& msg) {
                                       if (verbose)
                                       {
                                           spdlog::info(jsonWriter.write(msg));
                                       }

                                       receivedCount++;

                                       ++receivedCount;
                                       queueManager.add(msg);
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
                else if (eventType == ix::CobraConnection_EventType_Pong)
                {
                    spdlog::info("Received websocket pong");
                }
            });

        while (true)
        {
            std::chrono::duration<double, std::milli> duration(1000);
            std::this_thread::sleep_for(duration);
        }

        return 0;
    }
} // namespace ix
