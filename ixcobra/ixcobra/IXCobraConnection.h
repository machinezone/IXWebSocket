/*
 *  IXCobraConnection.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone. All rights reserved.
 */

#pragma once

#include "IXCobraConfig.h"
#include "IXCobraEvent.h"
#include "IXCobraEventType.h"
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <ixwebsocket/IXWebSocketPerMessageDeflateOptions.h>
#include <json/json.h>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#ifdef max
#undef max
#endif

namespace ix
{
    class WebSocket;
    struct SocketTLSOptions;

    enum CobraConnectionPublishMode
    {
        CobraConnection_PublishMode_Immediate = 0,
        CobraConnection_PublishMode_Batch = 1
    };

    using SubscriptionCallback = std::function<void(const Json::Value&, const std::string&)>;
    using EventCallback = std::function<void(const CobraEventPtr&)>;

    using TrafficTrackerCallback = std::function<void(size_t size, bool incoming)>;
    using PublishTrackerCallback = std::function<void(bool sent, bool acked)>;

    class CobraConnection
    {
    public:
        using MsgId = uint64_t;

        CobraConnection();
        ~CobraConnection();

        /// Configuration / set keys, etc...
        /// All input data but the channel name is encrypted with rc4
        void configure(const std::string& appkey,
                       const std::string& endpoint,
                       const std::string& rolename,
                       const std::string& rolesecret,
                       const WebSocketPerMessageDeflateOptions& webSocketPerMessageDeflateOptions,
                       const SocketTLSOptions& socketTLSOptions);

        void configure(const ix::CobraConfig& config);

        /// Set the traffic tracker callback
        static void setTrafficTrackerCallback(const TrafficTrackerCallback& callback);

        /// Reset the traffic tracker callback to an no-op one.
        static void resetTrafficTrackerCallback();

        /// Set the publish tracker callback
        static void setPublishTrackerCallback(const PublishTrackerCallback& callback);

        /// Reset the publish tracker callback to an no-op one.
        static void resetPublishTrackerCallback();

        /// Set the closed callback
        void setEventCallback(const EventCallback& eventCallback);

        /// Start the worker thread, used for background publishing
        void start();

        /// Publish a message to a channel
        ///
        /// No-op if the connection is not established
        MsgId publish(const Json::Value& channels, const Json::Value& msg);

        // Subscribe to a channel, and execute a callback when an incoming
        // message arrives.
        void subscribe(const std::string& channel,
                       const std::string& filter = std::string(),
                       const std::string& position = std::string(),
                       SubscriptionCallback cb = nullptr);

        /// Unsubscribe from a channel
        void unsubscribe(const std::string& channel);

        /// Close the connection
        void disconnect();

        /// Connect to Cobra and authenticate the connection
        bool connect();

        /// Returns true only if we're connected
        bool isConnected() const;

        /// Returns true only if we're authenticated
        bool isAuthenticated() const;

        /// Flush the publish queue
        bool flushQueue();

        /// Set the publish mode
        void setPublishMode(CobraConnectionPublishMode publishMode);

        /// Query the publish mode
        CobraConnectionPublishMode getPublishMode();

        /// Lifecycle management. Free resources when backgrounding
        void suspend();
        void resume();

        /// Prepare a message for transmission
        /// (update the pdu, compute a msgId, serialize json to a string)
        std::pair<CobraConnection::MsgId, std::string> prePublish(const Json::Value& channels,
                                                                  const Json::Value& msg,
                                                                  bool addToQueue);

        /// Attempt to send next message from the internal queue
        bool publishNext();

        // An invalid message id, signifying an error.
        static constexpr MsgId kInvalidMsgId = 0;

    private:
        bool sendHandshakeMessage();
        bool handleHandshakeResponse(const Json::Value& data);
        bool sendAuthMessage(const std::string& nonce);
        bool handleSubscriptionData(const Json::Value& pdu);
        bool handleSubscriptionResponse(const Json::Value& pdu);
        bool handleUnsubscriptionResponse(const Json::Value& pdu);
        bool handlePublishResponse(const Json::Value& pdu);

        void initWebSocketOnMessageCallback();

        bool publishMessage(const std::string& serializedJson);
        void enqueue(const std::string& msg);
        std::string serializeJson(const Json::Value& pdu);

        /// Invoke the traffic tracker callback
        static void invokeTrafficTrackerCallback(size_t size, bool incoming);

        /// Invoke the publish tracker callback
        static void invokePublishTrackerCallback(bool sent, bool acked);

        /// Invoke event callbacks
        void invokeEventCallback(CobraEventType eventType,
                                 const std::string& errorMsg = std::string(),
                                 const WebSocketHttpHeaders& headers = WebSocketHttpHeaders(),
                                 const std::string& subscriptionId = std::string(),
                                 uint64_t msgId = std::numeric_limits<uint64_t>::max());
        void invokeErrorCallback(const std::string& errorMsg, const std::string& serializedPdu);

        /// Tells whether the internal queue is empty or not
        bool isQueueEmpty();

        ///
        /// Member variables
        ///
        std::unique_ptr<WebSocket> _webSocket;

        /// Configuration data
        std::string _roleName;
        std::string _roleSecret;
        std::atomic<CobraConnectionPublishMode> _publishMode;

        // Can be set on control+background thread, protecting with an atomic
        std::atomic<bool> _authenticated;

        // Keep some objects around
        Json::Value _body;
        Json::Value _pdu;
        Json::FastWriter _jsonWriter;
        mutable std::mutex _jsonWriterMutex;
        std::mutex _prePublishMutex;

        /// Traffic tracker callback
        static TrafficTrackerCallback _trafficTrackerCallback;

        /// Publish tracker callback
        static PublishTrackerCallback _publishTrackerCallback;

        /// Cobra events callbacks
        EventCallback _eventCallback;
        mutable std::mutex _eventCallbackMutex;

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
        static constexpr size_t kQueueMaxSize = 256;

        // Each pdu sent should have an incremental unique id
        std::atomic<uint64_t> _id;

        // Frequency at which we send a websocket ping to the backing cobra connection
        static constexpr int kPingIntervalSecs = 30;
    };

} // namespace ix
