/*
 *  IXWebSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 *
 *  WebSocket RFC
 *  https://tools.ietf.org/html/rfc6455
 */

#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "IXWebSocketTransport.h"

namespace ix 
{
    // https://developer.mozilla.org/en-US/docs/Web/API/WebSocket#Ready_state_constants
    enum ReadyState 
    {
        WebSocket_ReadyState_Connecting = 0,
        WebSocket_ReadyState_Open = 1,
        WebSocket_ReadyState_Closing = 2,
        WebSocket_ReadyState_Closed = 3
    };

    enum WebSocketMessageType
    {
        WebSocket_MessageType_Message = 0,
        WebSocket_MessageType_Open = 1,
        WebSocket_MessageType_Close = 2,
        WebSocket_MessageType_Error = 3,
        WebSocket_MessageType_Ping = 4,
        WebSocket_MessageType_Pong = 5
    };

    struct WebSocketErrorInfo
    {
        uint64_t retries;
        double wait_time;
        int http_status;
        std::string reason;
    };

    struct CloseInfo
    {
        uint16_t code;
        std::string reason;

        CloseInfo(uint64_t c, const std::string& r)
        {
            code = c;
            reason = r;
        }

        CloseInfo()
        {
            code = 0;
            reason = "";
        }
    };

    using OnMessageCallback = std::function<void(WebSocketMessageType,
                                                 const std::string&,
                                                 const WebSocketErrorInfo,
                                                 const CloseInfo)>;
    using OnTrafficTrackerCallback = std::function<void(size_t size, bool incoming)>;

    class WebSocket 
    {
    public:
        WebSocket();
        ~WebSocket();

        void configure(const std::string& url);
        void start();
        void stop();
        bool send(const std::string& text);
        bool ping(const std::string& text);
        void close();

        void setOnMessageCallback(const OnMessageCallback& callback);
        static void setTrafficTrackerCallback(const OnTrafficTrackerCallback& callback);
        static void resetTrafficTrackerCallback();

        const std::string& getUrl() const;
        ReadyState getReadyState() const;

    private:
        void run();

        bool sendMessage(const std::string& text, bool ping);

        WebSocketInitResult connect();
        bool isConnected() const;
        bool isClosing() const;
        void reconnectPerpetuallyIfDisconnected();
        std::string readyStateToString(ReadyState readyState);
        static void invokeTrafficTrackerCallback(size_t size, bool incoming);

        WebSocketTransport _ws;

        std::string _url;
        mutable std::mutex _urlMutex;

        OnMessageCallback _onMessageCallback;
        static OnTrafficTrackerCallback _onTrafficTrackerCallback;

        std::atomic<bool> _stop;
        std::atomic<bool> _automaticReconnection;
        std::thread _thread;
        std::mutex _writeMutex;
    };
}
