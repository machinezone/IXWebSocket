/*
 *  ws_cobra_to_statsd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include "IXCobraConnection.h"

#include <statsd_client.h>

namespace ix
{
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

    std::string extractAttr(const std::string& attr,
                            const Json::Value& jsonValue)
    {
        // Split by .
        std::string token;
        std::stringstream tokenStream(attr);

        Json::Value val(jsonValue);

        int i = 0;
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
                                const std::string& host,
                                int port,
                                const std::string& prefix,
                                const std::string& fields,
                                bool verbose)
    {
        ix::CobraConnection conn;
        conn.configure(appkey, endpoint,
                       rolename, rolesecret,
                       ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        auto tokens = parseFields(fields);

        // statsd client
        // test with netcat as a server: `nc -ul 8125`
        statsd::StatsdClient statsdClient(host, port, prefix, true);

        Json::FastWriter jsonWriter;
        uint64_t msgCount = 0;

        conn.setEventCallback(
            [&conn, &channel, &jsonWriter, &statsdClient, verbose, &tokens, &prefix, &msgCount]
            (ix::CobraConnectionEventType eventType,
             const std::string& errMsg,
             const ix::WebSocketHttpHeaders& headers,
             const std::string& subscriptionId)
            {
                if (eventType == ix::CobraConnection_EventType_Open)
                {
                    std::cout << "Subscriber: connected" << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_Authenticated)
                {
                    std::cout << "Subscriber authenticated" << std::endl;
                    conn.subscribe(channel,
                                   [&jsonWriter, &statsdClient, &channel,
                                    verbose, &tokens, &prefix, &msgCount]
                                   (const Json::Value& msg)
                                   {
                                       if (verbose)
                                       {
                                           std::cout << jsonWriter.write(msg) << std::endl;
                                       }

                                       std::string id;
                                       for (auto&& attr : tokens)
                                       {
                                           id += ".";
                                           id += extractAttr(attr, msg);
                                       }

                                       std::cout << msgCount++ << " " << prefix << id << std::endl;

                                       statsdClient.count(id, 1);
                                   });
                }
                else if (eventType == ix::CobraConnection_EventType_Subscribed)
                {
                    std::cout << "Subscriber: subscribed to channel " << subscriptionId << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
                {
                    std::cout << "Subscriber: unsubscribed from channel " << subscriptionId << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_Error)
                {
                    std::cout << "Subscriber: error" << errMsg << std::endl;
                }
            }
        );

        while (true)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
        }

        return 0;
    }
}
