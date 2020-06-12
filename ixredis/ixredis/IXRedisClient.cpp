/*
 *  IXRedisClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXRedisClient.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketFactory.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <sstream>
#include <vector>

namespace ix
{
    bool RedisClient::connect(const std::string& hostname, int port)
    {
        bool tls = false;
        std::string errorMsg;
        SocketTLSOptions tlsOptions;
        _socket = createSocket(tls, -1, errorMsg, tlsOptions);

        if (!_socket)
        {
            return false;
        }

        CancellationRequest cancellationRequest = []() -> bool { return false; };

        std::string errMsg;
        return _socket->connect(hostname, port, errMsg, cancellationRequest);
    }

    void RedisClient::stop()
    {
        _stop = true;
    }

    bool RedisClient::auth(const std::string& password, std::string& response)
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
        ss << "*2\r\n";
        ss << writeString("SUBSCRIBE");
        ss << writeString(channel);

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
                ss << line.substr(1, line.size() - 1);
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
                ss << line.substr(1, line.size() - 1);
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

    std::string RedisClient::prepareXaddCommand(const std::string& stream,
                                                const std::string& message,
                                                int maxLen)
    {
        std::stringstream ss;
        ss << "*8\r\n";
        ss << writeString("XADD");
        ss << writeString(stream);
        ss << writeString("MAXLEN");
        ss << writeString("~");
        ss << writeString(std::to_string(maxLen));
        ss << writeString("*");
        ss << writeString("field");
        ss << writeString(message);

        return ss.str();
    }

    std::string RedisClient::xadd(const std::string& stream,
                                  const std::string& message,
                                  int maxLen,
                                  std::string& errMsg)
    {
        errMsg.clear();

        if (!_socket)
        {
            errMsg = "socket is not initialized";
            return std::string();
        }

        std::string command = prepareXaddCommand(stream, message, maxLen);

        bool sent = _socket->writeBytes(command, nullptr);
        if (!sent)
        {
            errMsg = "Cannot write bytes to socket";
            return std::string();
        }

        return readXaddReply(errMsg);
    }

    std::string RedisClient::readXaddReply(std::string& errMsg)
    {
        // Read result
        auto pollResult = _socket->isReadyToRead(-1);
        if (pollResult == PollResultType::Error)
        {
            errMsg = "Error while polling for result";
            return std::string();
        }

        // First line is the string length
        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            errMsg = "Error while polling for result";
            return std::string();
        }

        int stringSize;
        {
            std::stringstream ss;
            ss << line.substr(1, line.size() - 1);
            ss >> stringSize;
        }

        // Read the result, which is the stream id computed by the redis server
        lineResult = _socket->readLine(nullptr);
        lineValid = lineResult.first;
        line = lineResult.second;

        std::string streamId = line.substr(0, stringSize - 1);
        return streamId;
    }

    bool RedisClient::sendCommand(const std::string& commands,
                                  int commandsCount,
                                  std::string& errMsg)
    {
        bool sent = _socket->writeBytes(commands, nullptr);
        if (!sent)
        {
            errMsg = "Cannot write bytes to socket";
            return false;
        }

        bool success = true;

        for (int i = 0; i < commandsCount; ++i)
        {
            auto reply = readXaddReply(errMsg);
            if (reply == std::string())
            {
                success = false;
            }
        }

        return success;
    }

    std::pair<RespType, std::string> RedisClient::send(
        const std::vector<std::string>& args,
        std::string& errMsg)
    {
        std::stringstream ss;
        ss << "*";
        ss << std::to_string(args.size());
        ss << "\r\n";

        for (auto&& arg : args)
        {
            ss << writeString(arg);
        }

        bool sent = _socket->writeBytes(ss.str(), nullptr);
        if (!sent)
        {
            errMsg = "Cannot write bytes to socket";
            return std::make_pair(RespType::Error, "");
        }

        return readResponse(errMsg);
    }

    std::pair<RespType, std::string> RedisClient::readResponse(std::string& errMsg)
    {
        // Read result
        auto pollResult = _socket->isReadyToRead(-1);
        if (pollResult == PollResultType::Error)
        {
            errMsg = "Error while polling for result";
            return std::make_pair(RespType::Error, "");
        }

        // First line is the string length
        auto lineResult = _socket->readLine(nullptr);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            errMsg = "Error while polling for result";
            return std::make_pair(RespType::Error, "");
        }

        std::string response;

        if (line[0] == '+') // Simple string
        {
            std::stringstream ss;
            response = line.substr(1, line.size() - 3);
            return std::make_pair(RespType::String, response);
        }
        else if (line[0] == '-') // Errors
        {
            std::stringstream ss;
            response = line.substr(1, line.size() - 3);
            return std::make_pair(RespType::Error, response);
        }
        else if (line[0] == ':') // Integers
        {
            std::stringstream ss;
            response = line.substr(1, line.size() - 3);
            return std::make_pair(RespType::Integer, response);
        }
        else if (line[0] == '$') // Bulk strings
        {
            int stringSize;
            {
                std::stringstream ss;
                ss << line.substr(1, line.size() - 1);
                ss >> stringSize;
            }

            // Read the result, which is the stream id computed by the redis server
            lineResult = _socket->readLine(nullptr);
            lineValid = lineResult.first;
            line = lineResult.second;

            std::string str = line.substr(0, stringSize);
            return std::make_pair(RespType::String, str);
        }
        else
        {
            errMsg = "Unhandled response type";
            return std::make_pair(RespType::Unknown, std::string());
        }
    }

    std::string RedisClient::getRespTypeDescription(RespType respType)
    {
        switch (respType)
        {
            case RespType::Integer: return "integer";
            case RespType::Error: return "error";
            case RespType::String: return "string";
            default: return "unknown";
        }
    }
} // namespace ix
