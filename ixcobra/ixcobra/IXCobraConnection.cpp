/*
 *  IXCobraConnection.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone. All rights reserved.
 */

#include "IXCobraConnection.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <ixcrypto/IXHMac.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocket.h>
#include <sstream>
#include <stdexcept>


namespace ix
{
    TrafficTrackerCallback CobraConnection::_trafficTrackerCallback = nullptr;
    PublishTrackerCallback CobraConnection::_publishTrackerCallback = nullptr;
    constexpr size_t CobraConnection::kQueueMaxSize;
    constexpr CobraConnection::MsgId CobraConnection::kInvalidMsgId;
    constexpr int CobraConnection::kPingIntervalSecs;

    CobraConnection::CobraConnection()
        : _webSocket(new WebSocket())
        , _publishMode(CobraConnection_PublishMode_Immediate)
        , _authenticated(false)
        , _eventCallback(nullptr)
        , _id(1)
    {
        _pdu["action"] = "rtm/publish";

        _webSocket->addSubProtocol("json");
        initWebSocketOnMessageCallback();
    }

    CobraConnection::~CobraConnection()
    {
        disconnect();
        setEventCallback(nullptr);
    }

    void CobraConnection::setTrafficTrackerCallback(const TrafficTrackerCallback& callback)
    {
        _trafficTrackerCallback = callback;
    }

    void CobraConnection::resetTrafficTrackerCallback()
    {
        setTrafficTrackerCallback(nullptr);
    }

    void CobraConnection::invokeTrafficTrackerCallback(size_t size, bool incoming)
    {
        if (_trafficTrackerCallback)
        {
            _trafficTrackerCallback(size, incoming);
        }
    }

    void CobraConnection::setPublishTrackerCallback(const PublishTrackerCallback& callback)
    {
        _publishTrackerCallback = callback;
    }

    void CobraConnection::resetPublishTrackerCallback()
    {
        setPublishTrackerCallback(nullptr);
    }

    void CobraConnection::invokePublishTrackerCallback(bool sent, bool acked)
    {
        if (_publishTrackerCallback)
        {
            _publishTrackerCallback(sent, acked);
        }
    }

    void CobraConnection::setEventCallback(const EventCallback& eventCallback)
    {
        std::lock_guard<std::mutex> lock(_eventCallbackMutex);
        _eventCallback = eventCallback;
    }

    void CobraConnection::invokeEventCallback(ix::CobraEventType eventType,
                                              const std::string& errorMsg,
                                              const WebSocketHttpHeaders& headers,
                                              const std::string& subscriptionId,
                                              CobraConnection::MsgId msgId)
    {
        std::lock_guard<std::mutex> lock(_eventCallbackMutex);
        if (_eventCallback)
        {
            _eventCallback(
                std::make_unique<CobraEvent>(eventType, errorMsg, headers, subscriptionId, msgId));
        }
    }

    void CobraConnection::invokeErrorCallback(const std::string& errorMsg,
                                              const std::string& serializedPdu)
    {
        std::stringstream ss;
        ss << errorMsg << " : received pdu => " << serializedPdu;
        invokeEventCallback(ix::CobraEventType::Error, ss.str());
    }

    void CobraConnection::disconnect()
    {
        _authenticated = false;
        _webSocket->stop();
    }

    void CobraConnection::initWebSocketOnMessageCallback()
    {
        _webSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            CobraConnection::invokeTrafficTrackerCallback(msg->wireSize, true);

            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                invokeEventCallback(ix::CobraEventType::Open, std::string(), msg->openInfo.headers);
                sendHandshakeMessage();
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                _authenticated = false;

                std::stringstream ss;
                ss << "Close code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason;
                invokeEventCallback(ix::CobraEventType::Closed, ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                Json::Value data;
                Json::Reader reader;
                if (!reader.parse(msg->str, data))
                {
                    invokeErrorCallback("Invalid json", msg->str);
                    return;
                }

                if (!data.isMember("action"))
                {
                    invokeErrorCallback("Missing action", msg->str);
                    return;
                }

                auto action = data["action"].asString();

                if (action == "auth/handshake/ok")
                {
                    if (!handleHandshakeResponse(data))
                    {
                        invokeErrorCallback("Error extracting nonce from handshake response",
                                            msg->str);
                    }
                }
                else if (action == "auth/handshake/error")
                {
                    invokeEventCallback(ix::CobraEventType::HandshakeError, msg->str);
                }
                else if (action == "auth/authenticate/ok")
                {
                    _authenticated = true;
                    invokeEventCallback(ix::CobraEventType::Authenticated);
                    flushQueue();
                }
                else if (action == "auth/authenticate/error")
                {
                    invokeEventCallback(ix::CobraEventType::AuthenticationError, msg->str);
                }
                else if (action == "rtm/subscription/data")
                {
                    handleSubscriptionData(data);
                }
                else if (action == "rtm/subscribe/ok")
                {
                    if (!handleSubscriptionResponse(data))
                    {
                        invokeErrorCallback("Error processing subscribe response", msg->str);
                    }
                }
                else if (action == "rtm/subscribe/error")
                {
                    invokeEventCallback(ix::CobraEventType::SubscriptionError, msg->str);
                }
                else if (action == "rtm/unsubscribe/ok")
                {
                    if (!handleUnsubscriptionResponse(data))
                    {
                        invokeErrorCallback("Error processing unsubscribe response", msg->str);
                    }
                }
                else if (action == "rtm/unsubscribe/error")
                {
                    invokeErrorCallback("Unsubscription error", msg->str);
                }
                else if (action == "rtm/publish/ok")
                {
                    if (!handlePublishResponse(data))
                    {
                        invokeErrorCallback("Error processing publish response", msg->str);
                    }
                }
                else if (action == "rtm/publish/error")
                {
                    invokeErrorCallback("Publish error", msg->str);
                }
                else
                {
                    invokeErrorCallback("Un-handled message type", msg->str);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                invokeErrorCallback(ss.str(), std::string());
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                invokeEventCallback(ix::CobraEventType::Pong, msg->str);
            }
        });
    }

    void CobraConnection::setPublishMode(CobraConnectionPublishMode publishMode)
    {
        _publishMode = publishMode;
    }

    CobraConnectionPublishMode CobraConnection::getPublishMode()
    {
        return _publishMode;
    }

    void CobraConnection::configure(
        const std::string& appkey,
        const std::string& endpoint,
        const std::string& rolename,
        const std::string& rolesecret,
        const WebSocketPerMessageDeflateOptions& webSocketPerMessageDeflateOptions,
        const SocketTLSOptions& socketTLSOptions)
    {
        _roleName = rolename;
        _roleSecret = rolesecret;

        std::stringstream ss;
        ss << endpoint;
        ss << "/v2?appkey=";
        ss << appkey;

        std::string url = ss.str();
        _webSocket->setUrl(url);
        _webSocket->setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);
        _webSocket->setTLSOptions(socketTLSOptions);

        // Send a websocket ping every N seconds (N = 30) now
        // This should keep the connection open and prevent some load balancers such as
        // the Amazon one from shutting it down
        _webSocket->setPingInterval(kPingIntervalSecs);
    }

    void CobraConnection::configure(const ix::CobraConfig& config)
    {
        configure(config.appkey,
                  config.endpoint,
                  config.rolename,
                  config.rolesecret,
                  config.webSocketPerMessageDeflateOptions,
                  config.socketTLSOptions);
    }

    //
    // Handshake message schema.
    //
    // handshake = {
    //     "action": "auth/handshake",
    //     "body": {
    //         "data": {
    //             "role": role
    //         },
    //         "method": "role_secret"
    //     },
    // }
    //
    //
    bool CobraConnection::sendHandshakeMessage()
    {
        Json::Value data;
        data["role"] = _roleName;

        Json::Value body;
        body["data"] = data;
        body["method"] = "role_secret";

        Json::Value pdu;
        pdu["action"] = "auth/handshake";
        pdu["body"] = body;
        pdu["id"] = Json::UInt64(_id++);

        std::string serializedJson = serializeJson(pdu);
        CobraConnection::invokeTrafficTrackerCallback(serializedJson.size(), false);

        return _webSocket->send(serializedJson).success;
    }

    //
    // Extract the nonce from the handshake response
    // use it to compute a hash during authentication
    //
    // {
    //     "action": "auth/handshake/ok",
    //     "body": {
    //         "data": {
    //             "nonce": "MTI0Njg4NTAyMjYxMzgxMzgzMg==",
    //             "version": "0.0.24"
    //         }
    //     }
    // }
    //
    bool CobraConnection::handleHandshakeResponse(const Json::Value& pdu)
    {
        if (!pdu.isObject()) return false;

        if (!pdu.isMember("body")) return false;
        Json::Value body = pdu["body"];

        if (!body.isMember("data")) return false;
        Json::Value data = body["data"];

        if (!data.isMember("nonce")) return false;
        Json::Value nonce = data["nonce"];

        if (!nonce.isString()) return false;

        return sendAuthMessage(nonce.asString());
    }

    //
    // Authenticate message schema.
    //
    // challenge = {
    //     "action": "auth/authenticate",
    //     "body": {
    //         "method": "role_secret",
    //         "credentials": {
    //             "hash": computeHash(secret, nonce)
    //         }
    //     },
    // }
    //
    bool CobraConnection::sendAuthMessage(const std::string& nonce)
    {
        Json::Value credentials;
        credentials["hash"] = hmac(nonce, _roleSecret);

        Json::Value body;
        body["credentials"] = credentials;
        body["method"] = "role_secret";

        Json::Value pdu;
        pdu["action"] = "auth/authenticate";
        pdu["body"] = body;
        pdu["id"] = Json::UInt64(_id++);

        std::string serializedJson = serializeJson(pdu);
        CobraConnection::invokeTrafficTrackerCallback(serializedJson.size(), false);

        return _webSocket->send(serializedJson).success;
    }

    bool CobraConnection::handleSubscriptionResponse(const Json::Value& pdu)
    {
        if (!pdu.isObject()) return false;

        if (!pdu.isMember("body")) return false;
        Json::Value body = pdu["body"];

        if (!body.isMember("subscription_id")) return false;
        Json::Value subscriptionId = body["subscription_id"];

        if (!subscriptionId.isString()) return false;

        invokeEventCallback(ix::CobraEventType::Subscribed,
                            std::string(),
                            WebSocketHttpHeaders(),
                            subscriptionId.asString());
        return true;
    }

    bool CobraConnection::handleUnsubscriptionResponse(const Json::Value& pdu)
    {
        if (!pdu.isObject()) return false;

        if (!pdu.isMember("body")) return false;
        Json::Value body = pdu["body"];

        if (!body.isMember("subscription_id")) return false;
        Json::Value subscriptionId = body["subscription_id"];

        if (!subscriptionId.isString()) return false;

        invokeEventCallback(ix::CobraEventType::UnSubscribed,
                            std::string(),
                            WebSocketHttpHeaders(),
                            subscriptionId.asString());
        return true;
    }

    bool CobraConnection::handleSubscriptionData(const Json::Value& pdu)
    {
        if (!pdu.isObject()) return false;

        if (!pdu.isMember("body")) return false;
        Json::Value body = pdu["body"];

        // Identify subscription_id, so that we can find
        // which callback to execute
        if (!body.isMember("subscription_id")) return false;
        Json::Value subscriptionId = body["subscription_id"];

        std::lock_guard<std::mutex> lock(_cbsMutex);
        auto cb = _cbs.find(subscriptionId.asString());
        if (cb == _cbs.end()) return false; // cannot find callback

        // Extract messages now
        if (!body.isMember("messages")) return false;
        Json::Value messages = body["messages"];

        if (!body.isMember("position")) return false;
        std::string position = body["position"].asString();

        for (auto&& msg : messages)
        {
            cb->second(msg, position);
        }

        return true;
    }

    bool CobraConnection::handlePublishResponse(const Json::Value& pdu)
    {
        if (!pdu.isObject()) return false;

        if (!pdu.isMember("id")) return false;
        Json::Value id = pdu["id"];

        if (!id.isUInt64()) return false;

        uint64_t msgId = id.asUInt64();

        invokeEventCallback(ix::CobraEventType::Published,
                            std::string(),
                            WebSocketHttpHeaders(),
                            std::string(),
                            msgId);

        invokePublishTrackerCallback(false, true);

        return true;
    }

    bool CobraConnection::connect()
    {
        _webSocket->start();
        return true;
    }

    bool CobraConnection::isConnected() const
    {
        return _webSocket->getReadyState() == ix::ReadyState::Open;
    }

    bool CobraConnection::isAuthenticated() const
    {
        return isConnected() && _authenticated;
    }

    std::string CobraConnection::serializeJson(const Json::Value& value)
    {
        std::lock_guard<std::mutex> lock(_jsonWriterMutex);
        return _jsonWriter.write(value);
    }

    std::pair<CobraConnection::MsgId, std::string> CobraConnection::prePublish(
        const Json::Value& channels, const Json::Value& msg, bool addToQueue)
    {
        std::lock_guard<std::mutex> lock(_prePublishMutex);

        invokePublishTrackerCallback(true, false);

        CobraConnection::MsgId msgId = _id;

        _body["channels"] = channels;
        _body["message"] = msg;
        _pdu["body"] = _body;
        _pdu["id"] = Json::UInt64(_id++);

        std::string serializedJson = serializeJson(_pdu);

        if (addToQueue)
        {
            enqueue(serializedJson);
        }

        return std::make_pair(msgId, serializedJson);
    }

    bool CobraConnection::publishNext()
    {
        std::lock_guard<std::mutex> lock(_queueMutex);

        if (_messageQueue.empty()) return true;

        auto&& msg = _messageQueue.back();
        if (!_authenticated || !publishMessage(msg))
        {
            return false;
        }
        _messageQueue.pop_back();
        return true;
    }

    //
    // publish is not thread safe as we are trying to reuse some Json objects.
    //
    CobraConnection::MsgId CobraConnection::publish(const Json::Value& channels,
                                                    const Json::Value& msg)
    {
        auto p = prePublish(channels, msg, false);
        auto msgId = p.first;
        auto serializedJson = p.second;

        //
        // 1. When we use batch mode, we just enqueue and will do the flush explicitely
        // 2. When we aren't authenticated yet to the cobra server, we need to enqueue
        //    and retry later
        // 3. If the network connection was droped (WebSocket::send will return false),
        //    it means the message won't be sent so we need to enqueue as well.
        //
        // The order of the conditionals is important.
        //
        if (_publishMode == CobraConnection_PublishMode_Batch || !_authenticated ||
            !publishMessage(serializedJson))
        {
            enqueue(serializedJson);
        }

        return msgId;
    }

    void CobraConnection::subscribe(const std::string& channel,
                                    const std::string& filter,
                                    const std::string& position,
                                    int batchSize,
                                    SubscriptionCallback cb)
    {
        // Create and send a subscribe pdu
        Json::Value body;
        body["channel"] = channel;
        body["batch_size"] = batchSize;

        if (!filter.empty())
        {
            body["filter"] = filter;
        }

        if (!position.empty())
        {
            body["position"] = position;
        }

        Json::Value pdu;
        pdu["action"] = "rtm/subscribe";
        pdu["body"] = body;
        pdu["id"] = Json::UInt64(_id++);

        _webSocket->send(pdu.toStyledString());

        // Set the callback
        std::lock_guard<std::mutex> lock(_cbsMutex);
        _cbs[channel] = cb;
    }

    void CobraConnection::unsubscribe(const std::string& channel)
    {
        {
            std::lock_guard<std::mutex> lock(_cbsMutex);
            auto cb = _cbs.find(channel);
            if (cb == _cbs.end()) return;

            _cbs.erase(cb);
        }

        // Create and send an unsubscribe pdu
        Json::Value body;
        body["subscription_id"] = channel;

        Json::Value pdu;
        pdu["action"] = "rtm/unsubscribe";
        pdu["body"] = body;
        pdu["id"] = Json::UInt64(_id++);

        _webSocket->send(pdu.toStyledString());
    }

    //
    // Enqueue strategy drops old messages when we are at full capacity
    //
    // If we want to keep only 3 items max in the queue:
    //
    // enqueue(A) -> [A]
    // enqueue(B) -> [B, A]
    // enqueue(C) -> [C, B, A]
    // enqueue(D) -> [D, C, B] -- now we drop A, the oldest message,
    //                         -- and keep the 'fresh ones'
    //
    void CobraConnection::enqueue(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(_queueMutex);

        if (_messageQueue.size() == CobraConnection::kQueueMaxSize)
        {
            _messageQueue.pop_back();
        }
        _messageQueue.push_front(msg);
    }

    //
    // We process messages back (oldest) to front (newest) to respect ordering
    // when sending them. If we fail to send something, we put it back in the queue
    // at the end we picked it up originally (at the end).
    //
    bool CobraConnection::flushQueue()
    {
        while (!isQueueEmpty())
        {
            bool ok = publishNext();
            if (!ok) return false;
        }

        return true;
    }

    bool CobraConnection::isQueueEmpty()
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        return _messageQueue.empty();
    }

    bool CobraConnection::publishMessage(const std::string& serializedJson)
    {
        auto webSocketSendInfo = _webSocket->send(serializedJson);
        CobraConnection::invokeTrafficTrackerCallback(webSocketSendInfo.wireSize, false);
        return webSocketSendInfo.success;
    }

    void CobraConnection::suspend()
    {
        disconnect();
    }

    void CobraConnection::resume()
    {
        connect();
    }

} // namespace ix
