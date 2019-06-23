/*
 *  IXTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>

#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <stack>
#include <iomanip>
#include <random>


namespace ix
{
    std::atomic<size_t> incomingBytes(0);
    std::atomic<size_t> outgoingBytes(0);
    std::mutex Logger::_mutex;
    std::stack<int> freePorts;

    void setupWebSocketTrafficTrackerCallback()
    {
        ix::WebSocket::setTrafficTrackerCallback(
            [](size_t size, bool incoming)
            {
                if (incoming)
                {
                    incomingBytes += size;
                }
                else
                {
                    outgoingBytes += size;
                }
            }
        );
    }

    void reportWebSocketTraffic()
    {
        Logger() << incomingBytes;
        Logger() << "Incoming bytes: " << incomingBytes;
        Logger() << "Outgoing bytes: " << outgoingBytes;
    }

    void msleep(int ms)
    {
        std::chrono::duration<double, std::milli> duration(ms);
        std::this_thread::sleep_for(duration);
    }

    std::string generateSessionId()
    {
        auto now = std::chrono::system_clock::now();
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();

        return std::to_string(seconds);
    }

    void log(const std::string& msg)
    {
        Logger() << msg;
    }

    void hexDump(const std::string& prefix,
                 const std::string& s)
    {
        std::ostringstream ss;
        bool upper_case = false;

        for (std::string::size_type i = 0; i < s.length(); ++i)
        {
            ss << std::hex
               << std::setfill('0')
               << std::setw(2)
               << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];
        }

        std::cout << prefix << ": " << s << " => " << ss.str() << std::endl;
    }

    bool startWebSocketEchoServer(ix::WebSocketServer& server)
    {
        server.setOnConnectionCallback(
            [&server](std::shared_ptr<ix::WebSocket> webSocket,
                      std::shared_ptr<ConnectionState> connectionState)
            {
                webSocket->setOnMessageCallback(
                    [webSocket, connectionState, &server](const ix::WebSocketMessagePtr& msg)
                    {
                        if (msg->type == ix::WebSocketMessageType::Open)
                        {
                            Logger() << "New connection";
                            Logger() << "Uri: " << msg->openInfo.uri;
                            Logger() << "Headers:";
                            for (auto it : msg->openInfo.headers)
                            {
                                Logger() << it.first << ": " << it.second;
                            }
                        }
                        else if (msg->type == ix::WebSocketMessageType::Close)
                        {
                            Logger() << "Closed connection";
                        }
                        else if (msg->type == ix::WebSocketMessageType::Message)
                        {
                            for (auto&& client : server.getClients())
                            {
                                if (client != webSocket)
                                {
                                    client->send(msg->str, msg->binary);
                                }
                            }
                        }
                    }
                );
            }
        );

        auto res = server.listen();
        if (!res.first)
        {
            Logger() << res.second;
            return false;
        }

        server.start();
        return true;
    }
}
