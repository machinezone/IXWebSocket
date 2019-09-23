/*
 *  ws_cobra_to_statsd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <ixcobra/IXCobraConnection.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <statsd_client.h>
#endif

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
                                bool verbose)
    {
        ix::CobraConnection conn;
        conn.configure(
            appkey, endpoint, rolename, rolesecret, ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        auto tokens = parseFields(fields);

        // statsd client
        // test with netcat as a server: `nc -ul 8125`
        bool statsdBatch = true;
#ifndef _WIN32
        statsd::StatsdClient statsdClient(host, port, prefix, statsdBatch);
#else
        int statsdClient;
#endif

        Json::FastWriter jsonWriter;
        uint64_t msgCount = 0;

        conn.setEventCallback([&conn,
                               &channel,
                               &filter,
                               &jsonWriter,
                               &statsdClient,
                               verbose,
                               &tokens,
                               &prefix,
                               &msgCount](ix::CobraConnectionEventType eventType,
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
                               [&jsonWriter, &statsdClient, verbose, &tokens, &prefix, &msgCount](
                                   const Json::Value& msg) {
                                   if (verbose)
                                   {
                                       spdlog::info(jsonWriter.write(msg));
                                   }

                                   std::string id;
                                   for (auto&& attr : tokens)
                                   {
                                       id += ".";
                                       id += extractAttr(attr, msg);
                                   }

                                   spdlog::info("{} {}{}", msgCount++, prefix, id);

#ifndef _WIN32
                                   statsdClient.count(id, 1);
#endif
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
            std::chrono::duration<double, std::milli> duration(1000);
            std::this_thread::sleep_for(duration);
        }

        return 0;
    }
} // namespace ix
