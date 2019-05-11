/*
 *  IXTest.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <mutex>

namespace ix
{
    // Sleep for ms milliseconds.
    void msleep(int ms);

    // Generate a relatively random string
    std::string generateSessionId();

    // Record and report websocket traffic
    void setupWebSocketTrafficTrackerCallback();
    void reportWebSocketTraffic();

    struct Logger
    {
        public:
            template <typename T>
            Logger& operator<<(T const& obj)
            {
                std::lock_guard<std::mutex> lock(_mutex);

                std::cerr << obj;
                std::cerr << std::endl;
                return *this;
            }

        private:
            static std::mutex _mutex;
    };

    void log(const std::string& msg);

    int getFreePort();
}
