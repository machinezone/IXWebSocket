/*
 *  IXRedisClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXRedisClient.h"
#include <ixwebsocket/IXSocketFactory.h>
#include <ixwebsocket/IXSocket.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>

namespace ix
{
    bool RedisClient::connect(const std::string& hostname, int port)
    {
        bool tls = false;
        std::string errorMsg;
        _socket = createSocket(tls, errorMsg);

        if (!_socket)
        {
            return false;
        }

        std::cout << "Connecting to " << hostname << ":" << port << std::endl;

        std::string errMsg;
        return _socket->connect(hostname, port, errMsg, nullptr);
    }

    bool RedisClient::publish(const std::string& channel,
                              const std::string& message)
    {
        if (!_socket) return false;

        std::stringstream ss;
        ss << "PUBLISH ";
        ss << channel;
        ss << " ";
        ss << message;
        ss << "\r\n";

        bool sent = _socket->writeBytes(ss.str(), nullptr);
        if (!sent)
        {
            return false;
        }

        auto pollResult = _socket->isReadyToRead(1000);

        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        std::cout << "line: " << line << std::endl;

        return lineValid;
    }
}
