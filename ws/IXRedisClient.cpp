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

        std::string errMsg;
        return _socket->connect(hostname, port, errMsg, nullptr);
    }

    bool RedisClient::auth(const std::string& password,
                           std::string& response)
    {
        response.clear();

        if (!_socket) return false;

        std::stringstream ss;
        ss << "AUTH ";
        ss << password;
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

        response = line;
        return lineValid;
    }

    std::string RedisClient::writeString(const std::string& str)
    {
        std::stringstream ss;
        ss << "$";
        ss << str.size();
        ss << "\r\n";
        ss << str;
        ss << "\r\n";

        return ss.str();
    }

    bool RedisClient::publish(const std::string& channel,
                              const std::string& message,
                              std::string& errMsg)
    {
        errMsg.clear();

        if (!_socket)
        {
            errMsg = "socket is not initialized";
            return false;
        }

        std::stringstream ss;
        ss << "*3\r\n";
        ss << writeString("PUBLISH");
        ss << writeString(channel);
        ss << writeString(message);

        bool sent = _socket->writeBytes(ss.str(), nullptr);
        if (!sent)
        {
            errMsg = "Cannot write bytes to socket";
            return false;
        }

        auto pollResult = _socket->isReadyToRead(-1);
        if (pollResult == PollResultType::Error)
        {
            errMsg = "Error while polling for result";
            return false;
        }

        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        // A successful response starts with a :
        if (line.empty() || line[0] != ':')
        {
            errMsg = line;
            return false;
        }

        return lineValid;
    }

    //
    // FIXME: we assume that redis never return errors...
    //
    bool RedisClient::subscribe(const std::string& channel,
                                const OnRedisSubscribeResponseCallback& responseCallback,
                                const OnRedisSubscribeCallback& callback)
    {
        _stop = false;

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

        // build the response as a single string
        std::stringstream oss;

        // Read the first line of the response
        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;
        oss << line;

        if (!lineValid) return false;

        // There are 5 items for the subscribe reply
        for (int i = 0; i < 5; ++i)
        {
            auto lineResult = _socket->readLine(nullptr);
            auto lineValid = lineResult.first;
            auto line = lineResult.second;
            oss << line;

            if (!lineValid) return false;
        }

        responseCallback(oss.str());

        // Wait indefinitely for new messages
        while (true)
        {
            if (_stop) break;

            // Wait until something is ready to read
            int timeoutMs = 10;
            auto pollResult = _socket->isReadyToRead(timeoutMs);
            if (pollResult == PollResultType::Error)
            {
                return false;
            }

            if (pollResult == PollResultType::Timeout)
            {
                continue;
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

    void RedisClient::stop()
    {
        _stop = true;
    }
}
