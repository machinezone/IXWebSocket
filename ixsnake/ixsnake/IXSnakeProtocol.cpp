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
                     std::shared_ptr<ix::WebSocket> ws,
                     nlohmann::json pdu,
                     const std::string& errMsg)
    {
        std::string actionError(action);
        actionError += "/error";

        nlohmann::json response = {
            {"action", actionError}, {"id", pdu.value("id", 1)}, {"body", {{"reason", errMsg}}}};
        ws->sendText(response.dump());
    }

    void handleHandshake(std::shared_ptr<SnakeConnectionState> state,
                         std::shared_ptr<ix::WebSocket> ws,
                         const nlohmann::json& pdu)
    {
        std::string role = pdu["body"]["data"]["role"];

        state->setNonce(generateNonce());
        state->setRole(role);

        nlohmann::json response = {
            {"action", "auth/handshake/ok"},
            {"id", pdu.value("id", 1)},
            {"body",
             {
                 {"data", {{"nonce", state->getNonce()}, {"connection_id", state->getId()}}},
             }}};

        auto serializedResponse = response.dump();

        ws->sendText(serializedResponse);
    }

    void handleAuth(std::shared_ptr<SnakeConnectionState> state,
                    std::shared_ptr<ix::WebSocket> ws,
                    const AppConfig& appConfig,
                    const nlohmann::json& pdu)
    {
        auto secret = getRoleSecret(appConfig, state->appkey(), state->role());

        if (secret.empty())
        {
            nlohmann::json response = {
                {"action", "auth/authenticate/error"},
                {"id", pdu.value("id", 1)},
                {"body", {{"error", "authentication_failed"}, {"reason", "invalid secret"}}}};
            ws->sendText(response.dump());
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
            ws->sendText(response.dump());
            return;
        }

        nlohmann::json response = {
            {"action", "auth/authenticate/ok"}, {"id", pdu.value("id", 1)}, {"body", {}}};

        ws->sendText(response.dump());
    }

    void handlePublish(std::shared_ptr<SnakeConnectionState> state,
                       std::shared_ptr<ix::WebSocket> ws,
                       const nlohmann::json& pdu)
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
            handleError("rtm/publish", ws, pdu, ss.str());
            return;
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
                handleError("rtm/publish", ws, pdu, ss.str());
                return;
            }
        }

        nlohmann::json response = {
            {"action", "rtm/publish/ok"}, {"id", pdu.value("id", 1)}, {"body", {}}};

        ws->sendText(response.dump());
    }

    //
    // FIXME: this is not cancellable. We should be able to cancel the redis subscription
    //
    void handleRedisSubscription(std::shared_ptr<SnakeConnectionState> state,
                                 std::shared_ptr<ix::WebSocket> ws,
                                 const AppConfig& appConfig,
                                 const nlohmann::json& pdu)
    {
        std::string channel = pdu["body"]["channel"];
        std::string subscriptionId = channel;

        std::stringstream ss;
        ss << state->appkey() << "::" << channel;

        std::string appChannel(ss.str());

        ix::RedisClient redisClient;
        int port = appConfig.redisPort;

        auto urls = appConfig.redisHosts;
        std::string hostname(urls[0]);

        // Connect to redis first
        if (!redisClient.connect(hostname, port))
        {
            std::stringstream ss;
            ss << "Cannot connect to redis host " << hostname << ":" << port;
            handleError("rtm/subscribe", ws, pdu, ss.str());
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
                handleError("rtm/subscribe", ws, pdu, ss.str());
                return;
            }
        }

        int id = 0;
        auto callback = [ws, &id, &subscriptionId](const std::string& messageStr) {
            auto msg = nlohmann::json::parse(messageStr);

            msg = msg["body"]["message"];

            nlohmann::json response = {
                {"action", "rtm/subscription/data"},
                {"id", id++},
                {"body",
                 {{"subscription_id", subscriptionId}, {"position", "0-0"}, {"messages", {msg}}}}};

            ws->sendText(response.dump());
        };

        auto responseCallback = [ws, pdu, &subscriptionId](const std::string& redisResponse) {
            std::stringstream ss;
            ss << "Redis Response: " << redisResponse << "...";
            ix::CoreLogger::log(ss.str().c_str());

            // Success
            nlohmann::json response = {{"action", "rtm/subscribe/ok"},
                                       {"id", pdu.value("id", 1)},
                                       {"body", {{"subscription_id", subscriptionId}}}};
            ws->sendText(response.dump());
        };

        {
            std::stringstream ss;
            ss << "Subscribing to " << appChannel << "...";
            ix::CoreLogger::log(ss.str().c_str());
        }

        if (!redisClient.subscribe(appChannel, responseCallback, callback))
        {
            std::stringstream ss;
            ss << "Error subscribing to channel " << appChannel;
            handleError("rtm/subscribe", ws, pdu, ss.str());
            return;
        }
    }

    void handleSubscribe(std::shared_ptr<SnakeConnectionState> state,
                         std::shared_ptr<ix::WebSocket> ws,
                         const AppConfig& appConfig,
                         const nlohmann::json& pdu)
    {
        state->fut =
            std::async(std::launch::async, handleRedisSubscription, state, ws, appConfig, pdu);
    }

    void handleUnSubscribe(std::shared_ptr<SnakeConnectionState> state,
                           std::shared_ptr<ix::WebSocket> ws,
                           const nlohmann::json& pdu)
    {
        // extract subscription_id
        auto body = pdu["body"];
        auto subscriptionId = body["subscription_id"];

        state->redisClient().stop();

        nlohmann::json response = {{"action", "rtm/unsubscribe/ok"},
                                   {"id", pdu.value("id", 1)},
                                   {"body", {{"subscription_id", subscriptionId}}}};
        ws->sendText(response.dump());
    }

    void processCobraMessage(std::shared_ptr<SnakeConnectionState> state,
                             std::shared_ptr<ix::WebSocket> ws,
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
            ws->sendText(response.dump());
            return;
        }

        auto action = pdu["action"];

        if (action == "auth/handshake")
        {
            handleHandshake(state, ws, pdu);
        }
        else if (action == "auth/authenticate")
        {
            handleAuth(state, ws, appConfig, pdu);
        }
        else if (action == "rtm/publish")
        {
            handlePublish(state, ws, pdu);
        }
        else if (action == "rtm/subscribe")
        {
            handleSubscribe(state, ws, appConfig, pdu);
        }
        else if (action == "rtm/unsubscribe")
        {
            handleUnSubscribe(state, ws, pdu);
        }
        else
        {
            std::cerr << "Unhandled action: " << action << std::endl;
        }
    }
} // namespace snake
