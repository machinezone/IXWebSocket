/*
 *  IXSnakeProtocol.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSnakeProtocol.h"

#include "IXAppConfig.h"
#include "IXSnakeConnectionState.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <ixcore/utils/IXCoreLogger.h>
#include <ixcrypto/IXHMac.h>
#include <ixwebsocket/IXWebSocket.h>
#include <sstream>

namespace snake
{
    void handleError(const std::string& action,
                     ix::WebSocket& ws,
                     uint64_t pduId,
                     const std::string& errMsg)
    {
        std::string actionError(action);
        actionError += "/error";

        nlohmann::json response = {
            {"action", actionError}, {"id", pduId}, {"body", {{"reason", errMsg}}}};
        ws.sendText(response.dump());
    }

    void handleHandshake(std::shared_ptr<SnakeConnectionState> state,
                         ix::WebSocket& ws,
                         const nlohmann::json& pdu,
                         uint64_t pduId)
    {
        std::string role = pdu["body"]["data"]["role"];

        state->setNonce(generateNonce());
        state->setRole(role);

        nlohmann::json response = {
            {"action", "auth/handshake/ok"},
            {"id", pduId},
            {"body",
             {
                 {"data", {{"nonce", state->getNonce()}, {"connection_id", state->getId()}}},
             }}};

        auto serializedResponse = response.dump();

        ws.sendText(serializedResponse);
    }

    void handleAuth(std::shared_ptr<SnakeConnectionState> state,
                    ix::WebSocket& ws,
                    const AppConfig& appConfig,
                    const nlohmann::json& pdu,
                    uint64_t pduId)
    {
        auto secret = getRoleSecret(appConfig, state->appkey(), state->role());

        if (secret.empty())
        {
            nlohmann::json response = {
                {"action", "auth/authenticate/error"},
                {"id", pduId},
                {"body", {{"error", "authentication_failed"}, {"reason", "invalid secret"}}}};
            ws.sendText(response.dump());
            return;
        }

        auto nonce = state->getNonce();
        auto serverHash = ix::hmac(nonce, secret);
        std::string clientHash = pdu["body"]["credentials"]["hash"];

        if (serverHash != clientHash)
        {
            nlohmann::json response = {
                {"action", "auth/authenticate/error"},
                {"id", pdu.value("id", 1)},
                {"body", {{"error", "authentication_failed"}, {"reason", "invalid hash"}}}};
            ws.sendText(response.dump());
            return;
        }

        nlohmann::json response = {
            {"action", "auth/authenticate/ok"}, {"id", pdu.value("id", 1)}, {"body", {}}};

        ws.sendText(response.dump());
    }

    void handlePublish(std::shared_ptr<SnakeConnectionState> state,
                       ix::WebSocket& ws,
                       const AppConfig& appConfig,
                       const nlohmann::json& pdu,
                       uint64_t pduId)
    {
        std::vector<std::string> channels;

        auto body = pdu["body"];
        if (body.find("channels") != body.end())
        {
            for (auto&& channel : body["channels"])
            {
                channels.push_back(channel);
            }
        }
        else if (body.find("channel") != body.end())
        {
            channels.push_back(body["channel"]);
        }
        else
        {
            std::stringstream ss;
            ss << "Missing channels or channel field in publish data";
            handleError("rtm/publish", ws, pduId, ss.str());
            return;
        }

        // add an extra channel if the config has one specified
        if (!appConfig.republishChannel.empty())
        {
            channels.push_back(appConfig.republishChannel);
        }

        for (auto&& channel : channels)
        {
            std::stringstream ss;
            ss << state->appkey() << "::" << channel;

            std::string errMsg;
            if (!state->redisClient().publish(ss.str(), pdu.dump(), errMsg))
            {
                std::stringstream ss;
                ss << "Cannot publish to redis host " << errMsg;
                handleError("rtm/publish", ws, pduId, ss.str());
                return;
            }
        }

        nlohmann::json response = {
            {"action", "rtm/publish/ok"}, {"id", pdu.value("id", 1)}, {"body", {}}};

        ws.sendText(response.dump());
    }

    //
    // FIXME: this is not cancellable. We should be able to cancel the redis subscription
    //
    void handleSubscribe(std::shared_ptr<SnakeConnectionState> state,
                         ix::WebSocket& ws,
                         const AppConfig& appConfig,
                         const nlohmann::json& pdu,
                         uint64_t pduId)
    {
        std::string channel = pdu["body"]["channel"];
        state->subscriptionId = channel;

        std::stringstream ss;
        ss << state->appkey() << "::" << channel;

        state->appChannel = ss.str();

        ix::RedisClient& redisClient = state->subscriptionRedisClient;
        int port = appConfig.redisPort;

        auto urls = appConfig.redisHosts;
        std::string hostname(urls[0]);

        // Connect to redis first
        if (!redisClient.connect(hostname, port))
        {
            std::stringstream ss;
            ss << "Cannot connect to redis host " << hostname << ":" << port;
            handleError("rtm/subscribe", ws, pduId, ss.str());
            return;
        }

        // Now authenticate, if needed
        if (!appConfig.redisPassword.empty())
        {
            std::string authResponse;
            if (!redisClient.auth(appConfig.redisPassword, authResponse))
            {
                std::stringstream ss;
                ss << "Cannot authenticated to redis";
                handleError("rtm/subscribe", ws, pduId, ss.str());
                return;
            }
        }

        std::string filterStr;
        if (pdu["body"].find("filter") != pdu["body"].end())
        {
            std::string filterStr = pdu["body"]["filter"];
        }
        state->streamSql = std::make_unique<StreamSql>(filterStr);
        state->id = 0;
        state->onRedisSubscribeCallback = [&ws, state](const std::string& messageStr) {
            auto msg = nlohmann::json::parse(messageStr);

            msg = msg["body"]["message"];

            if (state->streamSql->valid() && !state->streamSql->match(msg))
            {
                return;
            }

            nlohmann::json response = {
                {"action", "rtm/subscription/data"},
                {"id", state->id++},
                {"body",
                 {{"subscription_id", state->subscriptionId}, {"position", "0-0"}, {"messages", {msg}}}}};

            ws.sendText(response.dump());
        };

        state->onRedisSubscribeResponseCallback = [&ws, state, pduId](const std::string& redisResponse) {
            std::stringstream ss;
            ss << "Redis Response: " << redisResponse << "...";
            ix::CoreLogger::log(ss.str().c_str());

            // Success
            nlohmann::json response = {{"action", "rtm/subscribe/ok"},
                                       {"id", pduId},
                                       {"body", {{"subscription_id", state->subscriptionId}}}};
            ws.sendText(response.dump());
        };

        {
            std::stringstream ss;
            ss << "Subscribing to " << state->appChannel << "...";
            ix::CoreLogger::log(ss.str().c_str());
        }

        auto subscription = [&redisClient, state, &ws, pduId]
        {
            if (!redisClient.subscribe(state->appChannel, 
                                       state->onRedisSubscribeResponseCallback,
                                       state->onRedisSubscribeCallback))
            {
                std::stringstream ss;
                ss << "Error subscribing to channel " << state->appChannel;
                handleError("rtm/subscribe", ws, pduId, ss.str());
                return;
            }
        };

        state->subscriptionThread = std::thread(subscription);
    }

    void handleUnSubscribe(std::shared_ptr<SnakeConnectionState> state,
                           ix::WebSocket& ws,
                           const nlohmann::json& pdu,
                           uint64_t pduId)
    {
        // extract subscription_id
        auto body = pdu["body"];
        auto subscriptionId = body["subscription_id"];

        state->stopSubScriptionThread();

        nlohmann::json response = {{"action", "rtm/unsubscribe/ok"},
                                   {"id", pduId},
                                   {"body", {{"subscription_id", subscriptionId}}}};
        ws.sendText(response.dump());
    }

    void processCobraMessage(std::shared_ptr<SnakeConnectionState> state,
                             ix::WebSocket& ws,
                             const AppConfig& appConfig,
                             const std::string& str)
    {
        nlohmann::json pdu;
        try
        {
            pdu = nlohmann::json::parse(str);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            std::stringstream ss;
            ss << "malformed json pdu: " << e.what() << " -> " << str << "";

            nlohmann::json response = {{"body", {{"error", "invalid_json"}, {"reason", ss.str()}}}};
            ws.sendText(response.dump());
            return;
        }

        auto action = pdu["action"];
        uint64_t pduId = pdu.value("id", 1);

        if (action == "auth/handshake")
        {
            handleHandshake(state, ws, pdu, pduId);
        }
        else if (action == "auth/authenticate")
        {
            handleAuth(state, ws, appConfig, pdu, pduId);
        }
        else if (action == "rtm/publish")
        {
            handlePublish(state, ws, appConfig, pdu, pduId);
        }
        else if (action == "rtm/subscribe")
        {
            handleSubscribe(state, ws, appConfig, pdu, pduId);
        }
        else if (action == "rtm/unsubscribe")
        {
            handleUnSubscribe(state, ws, pdu, pduId);
        }
        else
        {
            std::cerr << "Unhandled action: " << action << std::endl;
        }
    }
} // namespace snake
