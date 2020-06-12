/*
 *  IXRedisClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <ixwebsocket/IXSocket.h>

namespace ix
{
    enum class RespType : int
    {
        String = 0,
        Error = 1,
        Integer = 2,
        Unknown = 3
    };

    class RedisClient
    {
    public:
        using OnRedisSubscribeResponseCallback = std::function<void(const std::string&)>;
        using OnRedisSubscribeCallback = std::function<void(const std::string&)>;

        RedisClient()
            : _stop(false)
        {
        }
        ~RedisClient() = default;

        bool connect(const std::string& hostname, int port);

        bool auth(const std::string& password, std::string& response);

        // Publish / Subscribe
        bool publish(const std::string& channel, const std::string& message, std::string& errMsg);

        bool subscribe(const std::string& channel,
                       const OnRedisSubscribeResponseCallback& responseCallback,
                       const OnRedisSubscribeCallback& callback);

        // XADD
        std::string xadd(const std::string& channel,
                         const std::string& message,
                         int maxLen,
                         std::string& errMsg);
        std::string prepareXaddCommand(const std::string& stream,
                                       const std::string& message,
                                       int maxLen);
        std::string readXaddReply(std::string& errMsg);
        bool sendCommand(
            const std::string& commands, int commandsCount, std::string& errMsg);

        // Arbitrary commands
        std::pair<RespType, std::string> send(
            const std::vector<std::string>& args,
            std::string& errMsg);
        std::pair<RespType, std::string> readResponse(std::string& errMsg);

        std::string getRespTypeDescription(RespType respType);

        void stop();

    private:
        std::string writeString(const std::string& str);

        std::unique_ptr<Socket> _socket;
        std::atomic<bool> _stop;
    };
} // namespace ix
