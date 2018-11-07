/*
 *  IXSatoriConnection.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone. All rights reserved.
 */

#pragma once

#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "jsoncpp/json/json.h"
#include <ixwebsocket/IXWebSocket.h>

namespace ix
{
    using SubscriptionCallback = std::function<void(const Json::Value&)>;
    using AuthenticatedCallback = std::function<void()>;
    using OnTrafficTrackerCallback = std::function<void(size_t size, bool incoming)>;

    class SatoriConnection
    {
    public:
        SatoriConnection();
        ~SatoriConnection();

        /// Configuration / set keys, etc...
        /// All input data but the channel name is encrypted with rc4
        void configure(const std::string& appkey,
                       const std::string& endpoint,
                       const std::string& rolename,
                       const std::string& rolesecret);

        /// Set the traffic tracker callback
        static void setTrafficTrackerCallback(const OnTrafficTrackerCallback& callback);

        /// Reset the traffic tracker callback to an no-op one.
        static void resetTrafficTrackerCallback();

        /// Reset the traffic tracker callback to an no-op one.
        void setAuthenticatedCallback(const AuthenticatedCallback& authenticatedCallback);

        /// Start the worker thread, used for background publishing
        void start();

        /// Publish a message to a channel
        ///
        /// No-op if the connection is not established
        bool publish(const std::string& channel,
                     const Json::Value& msg);

        // Subscribe to a channel, and execute a callback when an incoming
        // message arrives.
        void subscribe(const std::string& channel, SubscriptionCallback cb);

        /// Unsubscribe from a channel
        void unsubscribe(const std::string& channel);

        /// Close the RTM connection and free the RTM handle memory
        void disconnect();

        /// Connect to Satori and authenticate the connection
        bool connect();

        /// Returns true only if we're connected
        bool isConnected() const;

        void logError(const std::string& error);
        
    private:
        bool sendHandshakeMessage();
        bool handleHandshakeResponse(const Json::Value& data);
        bool sendAuthMessage(const std::string& nonce);
        bool handleSubscriptionData(const Json::Value& pdu);

        bool publishMessage(const std::string& serializedJson);
        bool flushQueue();
        void enqueue(const std::string& msg);

        /// Invoke the traffic tracker callback
        static void invokeTrafficTrackerCallback(size_t size, bool incoming);

        /// Invoke the authenticated callback
        void invokeAuthenticatedCallback();

        ///
        /// Member variables
        /// 
        WebSocket _webSocket;

        /// Configuration data
        std::string _appkey;
        std::string _endpoint;
        std::string _role_name;
        std::string _role_secret;
        uint32_t _history;

        // Can be set on control+background thread, protecting with an atomic
        std::atomic<bool> _authenticated;

        // Keep some objects around
        Json::Value _body;
        Json::Value _pdu;

        /// Traffic tracker callback
        static OnTrafficTrackerCallback _onTrafficTrackerCallback;

        /// Callback invoked when we are authenticated
        AuthenticatedCallback _authenticatedCallback;

        /// Subscription callbacks, only one per channel
        std::unordered_map<std::string, SubscriptionCallback> _cbs;
        mutable std::mutex _cbsMutex;

        // Message Queue can be touched on control+background thread, 
        // protecting with a mutex.
        //
        // Message queue is used when there are problems sending messages so 
        // that sending can be retried later.
        std::deque<std::string> _messageQueue;
        mutable std::mutex _queueMutex;

        // Cap the queue size (100 elems so far -> ~100k)
        static constexpr size_t kQueueMaxSize = 100;
    };
    
} // namespace ix
