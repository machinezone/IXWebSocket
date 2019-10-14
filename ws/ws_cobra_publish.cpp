/*
 *  ws_cobra_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <jsoncpp/json/json.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>

namespace ix
{
    int ws_cobra_publish_main(const std::string& appkey,
                              const std::string& endpoint,
                              const std::string& rolename,
                              const std::string& rolesecret,
                              const std::string& channel,
                              const std::string& path)
    {
        std::ifstream f(path);
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(str, data))
        {
            spdlog::info("Input file is not a JSON file");
            return 1;
        }

        ix::CobraConnection conn;
        conn.configure(
            appkey, endpoint, rolename, rolesecret, ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        // Display incoming messages
        std::atomic<bool> authenticated(false);
        std::atomic<bool> messageAcked(false);

        conn.setEventCallback([&conn, &channel, &data, &authenticated, &messageAcked](
                                  ix::CobraConnectionEventType eventType,
                                  const std::string& errMsg,
                                  const ix::WebSocketHttpHeaders& headers,
                                  const std::string& subscriptionId,
                                  CobraConnection::MsgId msgId) {
            if (eventType == ix::CobraConnection_EventType_Open)
            {
                spdlog::info("Publisher connected");

                for (auto it : headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (eventType == ix::CobraConnection_EventType_Authenticated)
            {
                spdlog::info("Publisher authenticated");
                authenticated = true;

                Json::Value channels;
                channels[0] = channel;
                auto msgId = conn.publish(channels, data);

                spdlog::info("Published msg {}", msgId);
            }
            else if (eventType == ix::CobraConnection_EventType_Subscribed)
            {
                spdlog::info("Publisher: subscribed to channel {}", subscriptionId);
            }
            else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
            {
                spdlog::info("Publisher: unsubscribed from channel {}", subscriptionId);
            }
            else if (eventType == ix::CobraConnection_EventType_Error)
            {
                spdlog::error("Publisher: error {}", errMsg);
            }
            else if (eventType == ix::CobraConnection_EventType_Published)
            {
                spdlog::info("Published message id {} acked", msgId);
                messageAcked = true;
            }
        });

        while (!authenticated)
            ;
        while (!messageAcked)
            ;

        return 0;
    }
} // namespace ix
