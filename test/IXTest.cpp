/*
 *  IXTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <fstream>
#include <iostream>
#include <stdlib.h>

namespace ix
{
    std::atomic<size_t> incomingBytes(0);
    std::atomic<size_t> outgoingBytes(0);
    std::mutex Logger::_mutex;

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

}
