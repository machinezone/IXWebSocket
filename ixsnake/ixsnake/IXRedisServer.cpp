/*
 *  IXRedisServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXRedisServer.h"

#include <fstream>
#include <ixwebsocket/IXCancellationRequest.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketConnect.h>
#include <sstream>
#include <vector>

namespace ix
{
    RedisServer::RedisServer(
        int port, const std::string& host, int backlog, size_t maxConnections, int addressFamily)
        : SocketServer(port, host, backlog, maxConnections, addressFamily)
        , _connectedClientsCount(0)
        , _stopHandlingConnections(false)
    {
        ;
    }

    RedisServer::~RedisServer()
    {
        stop();
    }

    void RedisServer::stop()
    {
        stopAcceptingConnections();

        _stopHandlingConnections = true;
        while (_connectedClientsCount != 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        _stopHandlingConnections = false;

        SocketServer::stop();
    }

    void RedisServer::handleConnection(std::unique_ptr<Socket> socket,
                                       std::shared_ptr<ConnectionState> connectionState)
    {
        _connectedClientsCount++;

        while (!_stopHandlingConnections)
        {
            std::vector<std::string> tokens;
            if (!parseRequest(socket, tokens))
            {
                if (_stopHandlingConnections)
                {
                    logError("Cancellation requested");
                }
                else
                {
                    logError("Error parsing request");
                }
                break;
            }

            bool success = false;

            // publish
            if (tokens[0] == "COMMAND")
            {
                success = handleCommand(socket, tokens);
            }
            else if (tokens[0] == "PUBLISH")
            {
                success = handlePublish(socket, tokens);
            }
            else if (tokens[0] == "SUBSCRIBE")
            {
                success = handleSubscribe(socket, tokens);
            }

            if (!success)
            {
                if (_stopHandlingConnections)
                {
                    logError("Cancellation requested");
                }
                else
                {
                    logError("Error processing request for command: " + tokens[0]);
                }
                break;
            }
        }

        cleanupSubscribers(socket);

        logInfo("Connection closed for connection id " + connectionState->getId());
        connectionState->setTerminated();

        _connectedClientsCount--;
    }

    void RedisServer::cleanupSubscribers(std::unique_ptr<Socket>& socket)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        for (auto&& it : _subscribers)
        {
            it.second.erase(socket.get());
        }

        for (auto it : _subscribers)
        {
            std::stringstream ss;
            ss << "Subscription id: " << it.first << " #subscribers: " << it.second.size();

            logInfo(ss.str());
        }
    }

    size_t RedisServer::getConnectedClientsCount()
    {
        return _connectedClientsCount;
    }

    bool RedisServer::startsWith(const std::string& str, const std::string& start)
    {
        return str.compare(0, start.length(), start) == 0;
    }

    std::string RedisServer::writeString(const std::string& str)
    {
        std::stringstream ss;
        ss << "$";
        ss << str.size();
        ss << "\r\n";
        ss << str;
        ss << "\r\n";

        return ss.str();
    }

    bool RedisServer::parseRequest(std::unique_ptr<Socket>& socket,
                                   std::vector<std::string>& tokens)
    {
        // Parse first line
        auto cb = makeCancellationRequestWithTimeout(30, _stopHandlingConnections);
        auto lineResult = socket->readLine(cb);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid) return false;

        std::string str = line.substr(1);
        std::stringstream ss;
        ss << str;
        int count;
        ss >> count;

        for (int i = 0; i < count; ++i)
        {
            auto lineResult = socket->readLine(cb);
            auto lineValid = lineResult.first;
            auto line = lineResult.second;

            if (!lineValid) return false;

            int stringSize;
            std::stringstream ss;
            ss << line.substr(1, line.size() - 1);
            ss >> stringSize;

            auto readResult = socket->readBytes(stringSize, nullptr, nullptr);

            if (!readResult.first) return false;

            // read last 2 bytes (\r\n)
            char c;
            socket->readByte(&c, nullptr);
            socket->readByte(&c, nullptr);

            tokens.push_back(readResult.second);
        }

        return true;
    }

    bool RedisServer::handleCommand(std::unique_ptr<Socket>& socket,
                                    const std::vector<std::string>& tokens)
    {
        if (tokens.size() != 1) return false;

        auto cb = makeCancellationRequestWithTimeout(30, _stopHandlingConnections);
        std::stringstream ss;

        // return 2 nested arrays
        ss << "*2\r\n";

        //
        // publish
        //
        ss << "*6\r\n";
        ss << writeString("publish"); // 1
        ss << ":3\r\n";               // 2
        ss << "*0\r\n";               // 3
        ss << ":1\r\n";               // 4
        ss << ":2\r\n";               // 5
        ss << ":1\r\n";               // 6

        //
        // subscribe
        //
        ss << "*6\r\n";
        ss << writeString("subscribe"); // 1
        ss << ":2\r\n";                 // 2
        ss << "*0\r\n";                 // 3
        ss << ":1\r\n";                 // 4
        ss << ":1\r\n";                 // 5
        ss << ":1\r\n";                 // 6

        socket->writeBytes(ss.str(), cb);

        return true;
    }

    bool RedisServer::handleSubscribe(std::unique_ptr<Socket>& socket,
                                      const std::vector<std::string>& tokens)
    {
        if (tokens.size() != 2) return false;

        auto cb = makeCancellationRequestWithTimeout(30, _stopHandlingConnections);
        std::string channel = tokens[1];

        // Respond
        socket->writeBytes("*3\r\n", cb);
        socket->writeBytes(writeString("subscribe"), cb);
        socket->writeBytes(writeString(channel), cb);
        socket->writeBytes(":1\r\n", cb);

        std::lock_guard<std::mutex> lock(_mutex);
        _subscribers[channel].insert(socket.get());

        return true;
    }

    bool RedisServer::handlePublish(std::unique_ptr<Socket>& socket,
                                    const std::vector<std::string>& tokens)
    {
        if (tokens.size() != 3) return false;

        auto cb = makeCancellationRequestWithTimeout(30, _stopHandlingConnections);
        std::string channel = tokens[1];
        std::string data = tokens[2];

        // now dispatch the message to subscribers (write custom method)
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _subscribers.find(channel);
        if (it == _subscribers.end())
        {
            // return the number of clients that received the message, 0 in that case
            socket->writeBytes(":0\r\n", cb);
            return true;
        }

        auto subscribers = it->second;
        for (auto jt : subscribers)
        {
            jt->writeBytes("*3\r\n", cb);
            jt->writeBytes(writeString("message"), cb);
            jt->writeBytes(writeString(channel), cb);
            jt->writeBytes(writeString(data), cb);
        }

        // return the number of clients that received the message.
        std::stringstream ss;
        ss << ":" << std::to_string(subscribers.size()) << "\r\n";
        socket->writeBytes(ss.str(), cb);

        return true;
    }

} // namespace ix
