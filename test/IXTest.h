/*
 *  IXTest.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace ix
{
    // Sleep for ms milliseconds.
    void msleep(int ms);

    // Generate a relatively random string
    std::string generateSessionId();

    // Record and report websocket traffic
    void setupWebSocketTrafficTrackerCallback();
    void reportWebSocketTraffic();
}
