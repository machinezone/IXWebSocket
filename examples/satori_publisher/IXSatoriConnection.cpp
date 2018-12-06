/*
 *  IXSatoriConnection.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone. All rights reserved.
 */

#include "IXSatoriConnection.h"
#include <ixcrypto/IXHMac.h>

#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <cstring>


namespace ix
{
    TrafficTrackerCallback SatoriConnection::_trafficTrackerCallback = nullptr;
    constexpr size_t SatoriConnection::kQueueMaxSize;

    SatoriConnection::SatoriConnection() :
        _authenticated(false),
        _eventCallback(nullptr),
        _publishMode(SatoriConnection_PublishMode_Immediate)
    {
        _pdu["action"] = "rtm/publish";

        initWebSocketOnMessageCallback();
    }

    SatoriConnection::~SatoriConnection()
    {
        disconnect();
    }

    void SatoriConnection::setTrafficTrackerCallback(const TrafficTrackerCallback& callback)
    {
        _trafficTrackerCallback = callback;
    }

    void SatoriConnection::resetTrafficTrackerCallback()
    {
        setTrafficTrackerCallback(nullptr);
    }

    void SatoriConnection::invokeTrafficTrackerCallback(size_t size, bool incoming)
    {
        if (_trafficTrackerCallback)
        {
            _trafficTrackerCallback(size, incoming);
        }
    }

    void SatoriConnection::setEventCallback(const EventCallback& eventCallback)
    {
        std::lock_guard<std::mutex> lock(_eventCallbackMutex);
        _eventCallback = eventCallback;
    }

    void SatoriConnection::invokeEventCallback(ix::SatoriConnectionEventType eventType,
                                               const std::string& errorMsg,
                                               const WebSocketHttpHeaders& headers)
    {
        std::lock_guard<std::mutex> lock(_eventCallbackMutex);
        if (_eventCallback)
        {
            _eventCallback(eventType, errorMsg, headers);
        }
    }

    void SatoriConnection::invokeErrorCallback(const std::string& errorMsg)
    {
        invokeEventCallback(ix::SatoriConnection_EventType_Error, errorMsg);
    }

    void SatoriConnection::disconnect()
    {
        _authenticated = false;
        _webSocket.stop();
    }

    void SatoriConnection::initWebSocketOnMessageCallback()
    {
        _webSocket.setOnMessageCallback(
            [this](ix::WebSocketMessageType messageType,
                   const std::string& str,
                   size_t wireSize,
                   const ix::WebSocketErrorInfo& error,
                   const ix::WebSocketCloseInfo& closeInfo,
                   const ix::WebSocketHttpHeaders& headers)
            {
                SatoriConnection::invokeTrafficTrackerCallback(wireSize, true);

                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    invokeEventCallback(ix::SatoriConnection_EventType_Open,
                                        std::string(),
                                        headers);
                    sendHandshakeMessage();
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    _authenticated = false;

                    std::stringstream ss;
                    ss << "Close code " << closeInfo.code;
                    ss << " reason " << closeInfo.reason;
                    invokeEventCallback(ix::SatoriConnection_EventType_Closed,
                                        ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    Json::Value data;
                    Json::Reader reader;
                    if (!reader.parse(str, data))
                    {
                        invokeErrorCallback(std::string("Invalid json: ") + str);
                        return;
                    }

                    if (!data.isMember("action"))
                    {
                        invokeErrorCallback("Missing action");
                        return;
                    }

                    auto action = data["action"].asString();

                    if (action == "auth/handshake/ok")
                    {
                        if (!handleHandshakeResponse(data))
                        {
                            invokeErrorCallback("Error extracting nonce from handshake response");
                        }
                    }
                    else if (action == "auth/handshake/error")
                    {
                        invokeErrorCallback("Handshake error."); // print full message ?
                    }
                    else if (action == "auth/authenticate/ok")
                    {
                        _authenticated = true;
                        invokeEventCallback(ix::SatoriConnection_EventType_Authenticated);
                        flushQueue();
                    }
                    else if (action == "auth/authenticate/error")
                    {
                        invokeErrorCallback("Authentication error."); // print full message ?
                    }
                    else if (action == "rtm/subscription/data")
                    {
                        handleSubscriptionData(data);
                    }
                    else
                    {
                        invokeErrorCallback(std::string("Un-handled message type: ") + action);
                    }
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    std::stringstream ss;
                    ss << "Connection error: " << error.reason      << std::endl;
                    ss << "#retries: "         << error.retries     << std::endl;
                    ss << "Wait time(ms): "    << error.wait_time   << std::endl;
                    ss << "HTTP Status: "      << error.http_status << std::endl;
                    invokeErrorCallback(ss.str());
                }
        });
    }

    void SatoriConnection::setPublishMode(SatoriConnectionPublishMode publishMode)
    {
        _publishMode = publishMode;
    }

    void SatoriConnection::configure(const std::string& appkey,
                                     const std::string& endpoint,
                                     const std::string& rolename,
                                     const std::string& rolesecret,
                                     WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions)
    {
        _appkey = appkey;
        _endpoint = endpoint;
        _role_name = rolename;
        _role_secret = rolesecret;

        std::stringstream ss;
        ss << _endpoint;
        ss << "/v2?appkey=";
        ss << _appkey;

        std::string url = ss.str();
        _webSocket.setUrl(url);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);
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
    bool SatoriConnection::sendHandshakeMessage()
    {
        Json::Value data;
        data["role"] = _role_name;

        Json::Value body;
        body["data"] = data;
        body["method"] = "role_secret";

        Json::Value pdu;
        pdu["action"] = "auth/handshake";
        pdu["body"] = body;

        std::string serializedJson = serializeJson(pdu);
        SatoriConnection::invokeTrafficTrackerCallback(serializedJson.size(), false);

        return _webSocket.send(serializedJson).success;
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
    bool SatoriConnection::handleHandshakeResponse(const Json::Value& pdu)
    {
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
    bool SatoriConnection::sendAuthMessage(const std::string& nonce)
    {
        Json::Value credentials;
        credentials["hash"] = hmac(nonce, _role_secret);

        Json::Value body;
        body["credentials"] = credentials;
        body["method"] = "role_secret";

        Json::Value pdu;
        pdu["action"] = "auth/authenticate";
        pdu["body"] = body;

        std::string serializedJson = serializeJson(pdu);
        SatoriConnection::invokeTrafficTrackerCallback(serializedJson.size(), false);

        return _webSocket.send(serializedJson).success;
    }


    bool SatoriConnection::handleSubscriptionData(const Json::Value& pdu)
    {
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

        for (auto&& msg : messages)
        {
            cb->second(msg);
        }

        return true;
    }

    bool SatoriConnection::connect()
    {
        _webSocket.start();
        return true;
    }

    bool SatoriConnection::isConnected() const
    {
        return _webSocket.getReadyState() == ix::WebSocket_ReadyState_Open;
    }

    std::string SatoriConnection::serializeJson(const Json::Value& value)
    {
        std::lock_guard<std::mutex> lock(_jsonWriterMutex);
        return _jsonWriter.write(value);
    }

    //
    // publish is not thread safe as we are trying to reuse some Json objects.
    //
    bool SatoriConnection::publish(const Json::Value& channels,
                                   const Json::Value& msg)
    {
        _body["channels"] = channels;
        _body["message"] = msg;
        _pdu["body"] = _body;

        std::string serializedJson = serializeJson(_pdu);

        if (_publishMode == SatoriConnection_PublishMode_Batch)
        {
            enqueue(serializedJson);
            return true;
        }

        //
        // Fast path. We are authenticated and the publishing succeed
        //            This should happen for 99% of the cases.
        //
        if (_authenticated && publishMessage(serializedJson))
        {
            return true;
        }
        else // Or else we enqueue
             // Slow code path is when we haven't connected yet (startup),
             // or when the connection drops for some reason.
        {
            enqueue(serializedJson);
            return false;
        }
    }

    void SatoriConnection::subscribe(const std::string& channel,
                                     SubscriptionCallback cb)
    {
        // Create and send a subscribe pdu
        Json::Value body;
        body["channel"] = channel;

        Json::Value pdu;
        pdu["action"] = "rtm/subscribe";
        pdu["body"] = body;

        _webSocket.send(pdu.toStyledString());

        // Set the callback
        std::lock_guard<std::mutex> lock(_cbsMutex);
        _cbs[channel] = cb;
    }

    void SatoriConnection::unsubscribe(const std::string& channel)
    {
        {
            std::lock_guard<std::mutex> lock(_cbsMutex);
            auto cb = _cbs.find(channel);
            if (cb == _cbs.end()) return;

            _cbs.erase(cb);
        }

        // Create and send an unsubscribe pdu
        Json::Value body;
        body["channel"] = channel;

        Json::Value pdu;
        pdu["action"] = "rtm/unsubscribe";
        pdu["body"] = body;

        _webSocket.send(pdu.toStyledString());
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
    void SatoriConnection::enqueue(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(_queueMutex);

        if (_messageQueue.size() == SatoriConnection::kQueueMaxSize)
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
    bool SatoriConnection::flushQueue()
    {
        std::lock_guard<std::mutex> lock(_queueMutex);

        while (!_messageQueue.empty())
        {
            auto&& msg = _messageQueue.back();
            if (!publishMessage(msg))
            {
                _messageQueue.push_back(msg);
                return false;
            }
            _messageQueue.pop_back();
        }

        return true;
    }

    bool SatoriConnection::publishMessage(const std::string& serializedJson)
    {
        auto webSocketSendInfo = _webSocket.send(serializedJson);
        SatoriConnection::invokeTrafficTrackerCallback(webSocketSendInfo.wireSize,
                                                       false);
        return webSocketSendInfo.success;
    }

    void SatoriConnection::suspend()
    {
        disconnect();
    }

    void SatoriConnection::resume()
    {
        connect();
    }
    
} // namespace ix
