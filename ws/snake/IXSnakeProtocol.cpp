/*
 *  IXSnakeProtocol.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXSnakeProtocol.h"

#include <ixwebsocket/IXWebSocket.h>
#include <ixcrypto/IXHMac.h>

#include "IXSnakeConnectionState.h"
#include "IXAppConfig.h"

#include "nlohmann/json.hpp"
#include <sstream>

namespace snake
{
    void handleError(
        const std::string& action,
        std::shared_ptr<ix::WebSocket> ws,
        nlohmann::json pdu,
        const std::string& errMsg)
    {
        std::string actionError(action);
        actionError += "/error";

        nlohmann::json response = {
            {"action", actionError},
            {"id", pdu.value("id", 1)},
            {"body", {
                {"reason", errMsg}
            }}
        };
        ws->sendText(response.dump());
    }

    void handleHandshake(
        std::shared_ptr<SnakeConnectionState> state,
        std::shared_ptr<ix::WebSocket> ws,
        const nlohmann::json& pdu)
    {
        std::string role = pdu["body"]["data"]["role"];

        state->setNonce(generateNonce());
        state->setRole(role);

        nlohmann::json response = {
            {"action", "auth/handshake/ok"},
            {"id", pdu.value("id", 1)},
            {"body", {
                {"data", {
                    {"nonce", state->getNonce()},
                    {"connection_id", state->getId()}
                }},
            }}
        };

        auto serializedResponse = response.dump();
        std::cout << "response = " << serializedResponse << std::endl;

        ws->sendText(serializedResponse);
    }

    void handleAuth(
        std::shared_ptr<SnakeConnectionState> state,
        std::shared_ptr<ix::WebSocket> ws,
        const AppConfig& appConfig,
        const nlohmann::json& pdu)
    {
        auto secret = getRoleSecret(appConfig, state->appkey(), state->role());
        std::cout << "secret = " << secret << std::endl;

        if (secret.empty())
        {
            nlohmann::json response = {
                {"action", "auth/authenticate/error"},
                {"id", pdu.value("id", 1)},
                {"body", {
                    {"error", "authentication_failed"},
                    {"reason", "invalid secret"}
                }}
            };
            ws->sendText(response.dump());
            return;
        }

        auto nonce = state->getNonce();
        auto serverHash = ix::hmac(nonce, secret);
        std::string clientHash = pdu["body"]["credentials"]["hash"];

        if (appConfig.verbose)
        {
            std::cout << serverHash << std::endl;
            std::cout << clientHash << std::endl;
        }

        if (serverHash != clientHash)
        {
            nlohmann::json response = {
                {"action", "auth/authenticate/error"},
                {"id", pdu.value("id", 1)},
                {"body", {
                    {"error", "authentication_failed"},
                    {"reason", "invalid hash"}
                }}
            };
            ws->sendText(response.dump());
            return;
        }

        nlohmann::json response = {
            {"action", "auth/authenticate/ok"},
            {"id", pdu.value("id", 1)},
            {"body", {}}
        };

        ws->sendText(response.dump());
    }

    void handlePublish(
        std::shared_ptr<SnakeConnectionState> state,
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
            ss << state->appkey()
               << "::"
               << channel;

            std::string errMsg;
            if (!state->redisClient().publish(ss.str(), pdu.dump(), errMsg))
            {
                std::stringstream ss;
                ss << "Cannot publish to redis host " << errMsg;
                handleError("rtm/publish", ws, pdu, ss.str());
                return;
            }
        }
    }

    //
    // FIXME: this is not cancellable. We should be able to cancel the redis subscription
    //
    void handleRedisSubscription(
        std::shared_ptr<SnakeConnectionState> state,
        std::shared_ptr<ix::WebSocket> ws,
        const AppConfig& appConfig,
        const nlohmann::json& pdu)
    {
        std::string channel = pdu["body"]["channel"];
        std::string subscriptionId = channel;

        std::stringstream ss;
        ss << state->appkey()
           << "::"
           << channel;

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

        std::cout << "Connected to redis host " << hostname << ":" << port << std::endl;

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
            std::cout << "Auth response: " << authResponse << ":" << port << std::endl;
        }

        int id = 0;
        auto callback = [ws, &id, &subscriptionId](const std::string& messageStr)
        {
            auto msg = nlohmann::json::parse(messageStr);

            nlohmann::json response = {
                {"action", "rtm/subscription/data"},
                {"id", id++},
                {"body", {
                    {"subscription_id", subscriptionId},
                    {"messages", {{msg}}}
                }}
            };

            ws->sendText(response.dump());
        };

        auto responseCallback = [ws, pdu, &subscriptionId](const std::string& redisResponse)
        {
            std::cout << "Redis subscribe response: " << redisResponse << std::endl;

            // Success
            nlohmann::json response = {
                {"action", "rtm/subscribe/ok"},
                {"id", pdu.value("id", 1)},
                {"body", {
                    {"subscription_id", subscriptionId}
                }}
            };
            ws->sendText(response.dump());
        };

        std::cerr << "Subscribing to " << appChannel << "..." << std::endl;
        if (!redisClient.subscribe(appChannel, responseCallback, callback))
        {
            std::stringstream ss;
            ss << "Error subscribing to channel " << appChannel;
            handleError("rtm/subscribe", ws, pdu, ss.str());
            return;
        }
    }

    void handleSubscribe(
        std::shared_ptr<SnakeConnectionState> state,
        std::shared_ptr<ix::WebSocket> ws,
        const AppConfig& appConfig,
        const nlohmann::json& pdu)
    {
        state->fut = std::async(std::launch::async,
                                handleRedisSubscription,
                                state,
                                ws,
                                appConfig,
                                pdu);
    }

    void processCobraMessage(
        std::shared_ptr<SnakeConnectionState> state,
        std::shared_ptr<ix::WebSocket> ws,
        const AppConfig& appConfig,
        const std::string& str)
    {
        auto pdu = nlohmann::json::parse(str);
        std::cout << "Got " << str << std::endl;

        auto action = pdu["action"];
        std::cout << "action = " << action << std::endl;

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
        else
        {
            std::cerr << "Unhandled action: " << action << std::endl;
        }
    }
}
