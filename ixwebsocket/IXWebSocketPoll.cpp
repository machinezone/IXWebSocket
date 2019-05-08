/*
 *  IXWebSocketPoll.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketPoll.h"

namespace ix
{

    WebSocketPoll::WebSocketPoll(WebSocket* websocket)
    {
        bindWebsocket(websocket);
    }

    WebSocketPoll::~WebSocketPoll()
    {
        if (!_messages.empty())
        {
            // not handled all messages
        }

        bindWebsocket(nullptr);
    }

    void WebSocketPoll::bindWebsocket(WebSocket * websocket)
    {
        if (_websocket != websocket)
        {
            // unbind old
            if (_websocket)
            {
                _websocket->setOnMessageCallback(nullptr);
            }

            _websocket = websocket;

            // bind new
            if (_websocket)
            {
                _websocket->setOnMessageCallback([this](
                    WebSocketMessageType type,
                    const std::string& str,
                    size_t wireSize,
                    const WebSocketErrorInfo& errorInfo,
                    const WebSocketOpenInfo& openInfo,
                    const WebSocketCloseInfo& closeInfo)
                {
                    MessageDataPtr message(new Message());

                    message->type      = type;
                    message->str       = str;
                    message->wireSize  = wireSize;
                    message->errorInfo = errorInfo;
                    message->openInfo  = openInfo;
                    message->closeInfo = closeInfo;

                    _messagesMutex.lock();
                    _messages.emplace_back(std::move(message));
                    _messagesMutex.unlock();
                });
            }
        }
    }

    void WebSocketPoll::setOnMessageCallback(const OnMessageCallback& callback)
    {
        _onMessageUserCallback = callback;
    }
    
    WebSocketPoll::MessageDataPtr WebSocketPoll::popMessage()
    {
        MessageDataPtr message;

        _messagesMutex.lock();
        if (!_messages.empty())
        {
            message = std::move(_messages.front());
            _messages.pop_front();
        }
        _messagesMutex.unlock();

        return message;
    }

    void WebSocketPoll::poll(int count)
    {
        if (!_onMessageUserCallback)
            return;

        MessageDataPtr message;

        while (count > 0 && (message = popMessage()))
        {
            _onMessageUserCallback(
                message->type,
                message->str,
                message->wireSize,
                message->errorInfo,
                message->openInfo,
                message->closeInfo
            );

            --count;
        }
    }

}
