/*
 *  IXWebSocketPoll.h
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXWebSocket.h"
#include <thread>
#include <list>
#include <memory>

namespace ix
{
    /**
     * A helper class to dispatch websocket message callbacks in your thread.
     */
    class WebSocketPoll
    {
    public:
        WebSocketPoll(WebSocket* websocket = nullptr);
        ~WebSocketPoll();

        void bindWebsocket(WebSocket* websocket);

        void setOnMessageCallback(const OnMessageCallback& callback);

        void poll(int count = 512);

    protected:
        struct Message
        {
            WebSocketMessageType type;
            std::string str;
            size_t wireSize;
            WebSocketErrorInfo errorInfo;
            WebSocketOpenInfo openInfo;
            WebSocketCloseInfo closeInfo;
        };

        using MessageDataPtr = std::shared_ptr<Message>;

        MessageDataPtr popMessage();

    private:
        WebSocket* _websocket = nullptr;
        OnMessageCallback _onMessageUserCallback;
        std::mutex _messagesMutex;
        std::list<MessageDataPtr> _messages;
    };
}
