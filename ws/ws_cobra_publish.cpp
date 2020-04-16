/*
 *  ws_cobra_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <fstream>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <jsoncpp/json/json.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>

namespace ix
{
    int ws_cobra_publish_main(const ix::CobraConfig& config,
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
        conn.configure(config);

        // Display incoming messages
        std::atomic<bool> authenticated(false);
        std::atomic<bool> messageAcked(false);

        conn.setEventCallback(
            [&conn, &channel, &data, &authenticated, &messageAcked](const CobraEventPtr& event) {
                if (event->type == ix::CobraEventType::Open)
                {
                    spdlog::info("Publisher connected");

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
                    spdlog::info("Publisher authenticated");
                    authenticated = true;

                    Json::Value channels;
                    channels[0] = channel;
                    auto msgId = conn.publish(channels, data);

                    spdlog::info("Published msg {}", msgId);
                }
                else if (event->type == ix::CobraEventType::Subscribed)
                {
                    spdlog::info("Publisher: subscribed to channel {}", event->subscriptionId);
                }
                else if (event->type == ix::CobraEventType::UnSubscribed)
                {
                    spdlog::info("Publisher: unsubscribed from channel {}", event->subscriptionId);
                }
                else if (event->type == ix::CobraEventType::Error)
                {
                    spdlog::error("Publisher: error {}", event->errMsg);
                }
                else if (event->type == ix::CobraEventType::Published)
                {
                    spdlog::info("Published message id {} acked", event->msgId);
                    messageAcked = true;
                }
                else if (event->type == ix::CobraEventType::Pong)
                {
                    spdlog::info("Received websocket pong");
                }
                else if (event->type == ix::CobraEventType::HandshakeError)
                {
                    spdlog::error("Subscriber: Handshake error: {}", event->errMsg);
                }
                else if (event->type == ix::CobraEventType::AuthenticationError)
                {
                    spdlog::error("Subscriber: Authentication error: {}", event->errMsg);
                }
            });

        conn.connect();

        while (!authenticated)
            ;
        while (!messageAcked)
            ;

        return 0;
    }
} // namespace ix
