/*
 *  ws_echo_client.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <ixcore/utils/IXCoreLogger.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSetThreadName.h>
#include <ixwebsocket/IXWebSocket.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>

namespace ix
{
    int ws_echo_client(const std::string& url,
                       bool disablePerMessageDeflate,
                       bool binaryMode,
                       const ix::SocketTLSOptions& tlsOptions,
                       const std::string& subprotocol,
                       int pingIntervalSecs,
                       const std::string& sendMsg,
                       bool noSend)
    {
        // Our websocket object
        ix::WebSocket webSocket;

        webSocket.setUrl(url);
        webSocket.setTLSOptions(tlsOptions);
        webSocket.setPingInterval(pingIntervalSecs);

        if (disablePerMessageDeflate)
        {
            webSocket.disablePerMessageDeflate();
        }

        if (!subprotocol.empty())
        {
            webSocket.addSubProtocol(subprotocol);
        }

        std::atomic<uint64_t> receivedCount(0);
        uint64_t receivedCountTotal(0);
        uint64_t receivedCountPerSecs(0);

        // Setup a callback to be fired (in a background thread, watch out for race conditions !)
        // when a message or an event (open, close, error) is received
        webSocket.setOnMessageCallback([&webSocket, &receivedCount, &sendMsg, noSend, binaryMode](
                                           const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                if (!noSend)
                {
                    webSocket.send(msg->str, msg->binary);
                }
                receivedCount++;
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                spdlog::info("ws_echo_client: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }

                webSocket.send(sendMsg, binaryMode);
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                spdlog::info("Received pong {}", msg->str);
            }
        });

        auto timer = [&receivedCount, &receivedCountTotal, &receivedCountPerSecs] {
            setThreadName("Timer");
            while (true)
            {
                //
                // We cannot write to sentCount and receivedCount
                // as those are used externally, so we need to introduce
                // our own counters
                //
                std::stringstream ss;
                ss << "messages received: " << receivedCountPerSecs << " per second "
                   << receivedCountTotal << " total";

                CoreLogger::info(ss.str());

                receivedCountPerSecs = receivedCount - receivedCountTotal;
                receivedCountTotal += receivedCountPerSecs;

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t1(timer);

        // Now that our callback is setup, we can start our background thread and receive messages
        std::cout << "Connecting to " << url << "..." << std::endl;
        webSocket.start();

        // Send a message to the server (default to TEXT mode)
        webSocket.send("hello world");

        while (true)
        {
            std::string text;
            std::cout << "> " << std::flush;
            std::getline(std::cin, text);

            webSocket.send(text);
        }

        return 0;
    }
} // namespace ix
