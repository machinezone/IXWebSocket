/*
 *  IXWebSocketMessageQueue.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketMessageQueue.h"

namespace ix
{
    WebSocketMessageQueue::WebSocketMessageQueue(WebSocket* websocket)
    {
        bindWebsocket(websocket);
    }

    WebSocketMessageQueue::~WebSocketMessageQueue()
    {
        if (!_messages.empty())
        {
            // not handled all messages
        }

        bindWebsocket(nullptr);
    }

    void WebSocketMessageQueue::bindWebsocket(WebSocket* websocket)
    {
        if (_websocket == websocket) return;

        // unbind old
        if (_websocket)
        {
            // set dummy callback just to avoid crash
            _websocket->setOnMessageCallback([](const WebSocketMessagePtr&) {});
        }

        _websocket = websocket;

        // bind new
        if (_websocket)
        {
            _websocket->setOnMessageCallback([this](const WebSocketMessagePtr& msg) {
                std::lock_guard<std::mutex> lock(_messagesMutex);
                _messages.emplace_back(std::move(msg));
            });
        }
    }

    void WebSocketMessageQueue::setOnMessageCallback(const OnMessageCallback& callback)
    {
        _onMessageUserCallback = callback;
    }

    void WebSocketMessageQueue::setOnMessageCallback(OnMessageCallback&& callback)
    {
        _onMessageUserCallback = std::move(callback);
    }

    WebSocketMessagePtr WebSocketMessageQueue::popMessage()
    {
        WebSocketMessagePtr message;
        std::lock_guard<std::mutex> lock(_messagesMutex);

        if (!_messages.empty())
        {
            message = std::move(_messages.front());
            _messages.pop_front();
        }

        return message;
    }

    void WebSocketMessageQueue::poll(int count)
    {
        if (!_onMessageUserCallback) return;

        WebSocketMessagePtr message;

        while (count > 0 && (message = popMessage()))
        {
            _onMessageUserCallback(message);
            --count;
        }
    }

} // namespace ix
