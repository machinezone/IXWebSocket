/*
 *  IXRedisClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXRedisClient.h"
#include <ixwebsocket/IXSocketFactory.h>
#include <ixwebsocket/IXSocket.h>

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

        auto pollResult = _socket->isReadyToRead(-1);
        if (pollResult == PollResultType::Error)
        {
            return false;
        }

        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        return lineValid;
    }

    //
    // FIXME: we assume that redis never return errors...
    //
    bool RedisClient::subscribe(const std::string& channel,
                                const OnRedisSubscribeCallback& callback)
    {
        if (!_socket) return false;

        std::stringstream ss;
        ss << "SUBSCRIBE ";
        ss << channel;
        ss << "\r\n";

        bool sent = _socket->writeBytes(ss.str(), nullptr);
        if (!sent)
        {
            return false;
        }

        // Wait 1s for the response
        auto pollResult = _socket->isReadyToRead(-1);
        if (pollResult == PollResultType::Error)
        {
            return false;
        }

        // Read the first line of the response
        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid) return false;

        // There are 5 items for the subscribe repply
        for (int i = 0; i < 5; ++i)
        {
            auto lineResult = _socket->readLine(nullptr);
            auto lineValid = lineResult.first;
            auto line = lineResult.second;

            if (!lineValid) return false;
        }

        // Wait indefinitely for new messages
        while (true)
        {
            // Wait until something is ready to read
            auto pollResult = _socket->isReadyToRead(-1);
            if (pollResult == PollResultType::Error)
            {
                return false;
            }

            // The first line of the response describe the return type,
            // => *3 (an array of 3 elements)
            auto lineResult = _socket->readLine(nullptr);
            auto lineValid = lineResult.first;
            auto line = lineResult.second;

            if (!lineValid) return false;

            int arraySize;
            {
                std::stringstream ss;
                ss << line.substr(1, line.size()-1);
                ss >> arraySize;
            }

            // There are 6 items for each received message
            for (int i = 0; i < arraySize; ++i)
            {
                auto lineResult = _socket->readLine(nullptr);
                auto lineValid = lineResult.first;
                auto line = lineResult.second;

                if (!lineValid) return false;

                // Messages are string, which start with a string size
                // => $7 (7 bytes)
                int stringSize;
                std::stringstream ss;
                ss << line.substr(1, line.size()-1);
                ss >> stringSize;

                auto readResult = _socket->readBytes(stringSize, nullptr, nullptr);
                if (!readResult.first) return false;

                if (i == 2)
                {
                    // The message is the 3rd element.
                    callback(readResult.second);
                }

                // read last 2 bytes (\r\n)
                char c;
                _socket->readByte(&c, nullptr);
                _socket->readByte(&c, nullptr);
            }
        }

        return true;
    }
}
