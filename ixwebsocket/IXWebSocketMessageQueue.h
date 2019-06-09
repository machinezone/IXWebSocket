/*
 *  IXWebSocketMessageQueue.h
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXWebSocket.h"
#include <list>
#include <memory>
#include <thread>

namespace ix
{
    //
    // A helper class to dispatch websocket message callbacks in your thread.
    //
    class WebSocketMessageQueue
    {
    public:
        WebSocketMessageQueue(WebSocket* websocket = nullptr);
        ~WebSocketMessageQueue();

        void bindWebsocket(WebSocket* websocket);

        void setOnMessageCallback(const OnMessageCallback& callback);
        void setOnMessageCallback(OnMessageCallback&& callback);

        void poll(int count = 512);

    protected:
        WebSocketMessagePtr popMessage();

    private:
        WebSocket* _websocket = nullptr;
        OnMessageCallback _onMessageUserCallback;
        std::mutex _messagesMutex;
        std::list<WebSocketMessagePtr> _messages;
    };
} // namespace ix
