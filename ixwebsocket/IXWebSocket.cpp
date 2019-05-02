/*
 *  IXWebSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocket.h"
#include "IXSetThreadName.h"
#include "IXWebSocketHandshake.h"

#include <iostream>
#include <cmath>
#include <cassert>

namespace
{
    uint64_t calculateRetryWaitMilliseconds(uint64_t retry_count)
    {
        // This will overflow quite fast for large value of retry_count
        // and will become 0, in which case the wait time will be none
        // and we'll be constantly retrying to connect.
        uint64_t wait_time = ((uint64_t) std::pow(2, retry_count) * 100L);

        // cap the wait time to 10s, or to retry_count == 10 for which wait_time > 10s
        uint64_t tenSeconds = 10 * 1000;
        return (wait_time > tenSeconds || retry_count > 10) ? tenSeconds : wait_time;
    }
}

namespace ix
{
    OnTrafficTrackerCallback WebSocket::_onTrafficTrackerCallback = nullptr;
    const int WebSocket::kDefaultHandShakeTimeoutSecs(60);
    const int WebSocket::kDefaultPingIntervalSecs(-1);
    const int WebSocket::kDefaultPingTimeoutSecs(-1);
    const bool WebSocket::kDefaultEnablePong(true);

    WebSocket::WebSocket() :
        _onMessageCallback(OnMessageCallback()),
        _stop(false),
        _automaticReconnection(true),
        _handshakeTimeoutSecs(kDefaultHandShakeTimeoutSecs),
        _enablePong(kDefaultEnablePong),
        _pingIntervalSecs(kDefaultPingIntervalSecs),
        _pingTimeoutSecs(kDefaultPingTimeoutSecs)
    {
        _ws.setOnCloseCallback(
            [this](uint16_t code, const std::string& reason, size_t wireSize, bool remote)
            {
                _onMessageCallback(WebSocket_MessageType_Close, "", wireSize,
                                   WebSocketErrorInfo(), WebSocketOpenInfo(),
                                   WebSocketCloseInfo(code, reason, remote));
            }
        );
    }

    WebSocket::~WebSocket()
    {
        stop();
    }

    void WebSocket::setUrl(const std::string& url)
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _url = url;
    }

    const std::string& WebSocket::getUrl() const
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        return _url;
    }

    void WebSocket::setPerMessageDeflateOptions(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions)
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _perMessageDeflateOptions = perMessageDeflateOptions;
    }

    const WebSocketPerMessageDeflateOptions& WebSocket::getPerMessageDeflateOptions() const
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        return _perMessageDeflateOptions;
    }

    void WebSocket::setHeartBeatPeriod(int heartBeatPeriodSecs)
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _pingIntervalSecs = heartBeatPeriodSecs;
    }

    int WebSocket::getHeartBeatPeriod() const
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        return _pingIntervalSecs;
    }

    void WebSocket::setPingInterval(int pingIntervalSecs)
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _pingIntervalSecs = pingIntervalSecs;
    }

    int WebSocket::getPingInterval() const
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        return _pingIntervalSecs;
    }

    void WebSocket::setPingTimeout(int pingTimeoutSecs)
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _pingTimeoutSecs = pingTimeoutSecs;
    }

    int WebSocket::getPingTimeout() const
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        return _pingTimeoutSecs;
    }

    void WebSocket::enablePong()
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _enablePong = true;
    }

    void WebSocket::disablePong()
    {
        std::lock_guard<std::mutex> lock(_configMutex);
        _enablePong = false;
    }

    void WebSocket::start()
    {
        if (_thread.joinable()) return; // we've already been started

        _thread = std::thread(&WebSocket::run, this);
    }

    void WebSocket::stop()
    {
        bool automaticReconnection = _automaticReconnection;

        // This value needs to be forced when shutting down, it is restored later
        _automaticReconnection = false;

        close();

        if (!_thread.joinable())
        {
            _automaticReconnection = automaticReconnection;
            return;
        }

        _stop = true;
        _thread.join();
        _stop = false;

        _automaticReconnection = automaticReconnection;
    }

    WebSocketInitResult WebSocket::connect(int timeoutSecs)
    {
        {
            std::lock_guard<std::mutex> lock(_configMutex);
            _ws.configure(_perMessageDeflateOptions,
                          _enablePong,
                          _pingIntervalSecs,
                          _pingTimeoutSecs);
        }

        WebSocketInitResult status = _ws.connectToUrl(_url, timeoutSecs);
        if (!status.success)
        {
            return status;
        }

        _onMessageCallback(WebSocket_MessageType_Open, "", 0,
                           WebSocketErrorInfo(),
                           WebSocketOpenInfo(status.uri, status.headers),
                           WebSocketCloseInfo());
        return status;
    }

    WebSocketInitResult WebSocket::connectToSocket(int fd, int timeoutSecs)
    {
        {
            std::lock_guard<std::mutex> lock(_configMutex);
            _ws.configure(_perMessageDeflateOptions,
                          _enablePong,
                          _pingIntervalSecs,
                          _pingTimeoutSecs);
        }

        WebSocketInitResult status = _ws.connectToSocket(fd, timeoutSecs);
        if (!status.success)
        {
            return status;
        }

        _onMessageCallback(WebSocket_MessageType_Open, "", 0,
                           WebSocketErrorInfo(),
                           WebSocketOpenInfo(status.uri, status.headers),
                           WebSocketCloseInfo());
        return status;
    }

    bool WebSocket::isConnected() const
    {
        return getReadyState() == WebSocket_ReadyState_Open;
    }

    bool WebSocket::isClosing() const
    {
        return getReadyState() == WebSocket_ReadyState_Closing;
    }

    void WebSocket::close()
    {
        _ws.close();
    }

    void WebSocket::reconnectPerpetuallyIfDisconnected()
    {
        uint64_t retries = 0;
        WebSocketErrorInfo connectErr;
        ix::WebSocketInitResult status;
        using millis = std::chrono::duration<double, std::milli>;
        millis duration;

        // Try to connect only once when we don't have automaticReconnection setup
        if (!isConnected() && !isClosing() && !_stop && !_automaticReconnection)
        {
            status = connect(_handshakeTimeoutSecs);

            if (!status.success)
            {
                duration = millis(calculateRetryWaitMilliseconds(retries++));

                connectErr.retries = retries;
                connectErr.wait_time = duration.count();
                connectErr.reason = status.errorStr;
                connectErr.http_status = status.http_status;
                _onMessageCallback(WebSocket_MessageType_Error, "", 0,
                                   connectErr, WebSocketOpenInfo(),
                                   WebSocketCloseInfo());
            }
        }
        else
        {
            // Otherwise try to reconnect perpertually
            while (true)
            {
                if (isConnected() || isClosing() || _stop || !_automaticReconnection)
                {
                    break;
                }

                status = connect(_handshakeTimeoutSecs);

                if (!status.success)
                {
                    duration = millis(calculateRetryWaitMilliseconds(retries++));

                    connectErr.retries = retries;
                    connectErr.wait_time = duration.count();
                    connectErr.reason = status.errorStr;
                    connectErr.http_status = status.http_status;
                    _onMessageCallback(WebSocket_MessageType_Error, "", 0,
                                       connectErr, WebSocketOpenInfo(),
                                       WebSocketCloseInfo());

                    // Only sleep if we aren't in the middle of stopping
                    if (!_stop)
                    {
                        std::this_thread::sleep_for(duration);
                    }
                }
            }
        }
    }

    void WebSocket::run()
    {
        setThreadName(getUrl());

        while (true)
        {
            if (_stop) return;

            // 1. Make sure we are always connected
            reconnectPerpetuallyIfDisconnected();

            if (_stop) return;

            // 2. Poll to see if there's any new data available
            _ws.poll();

            if (_stop) return;

            // 3. Dispatch the incoming messages
            _ws.dispatch(
                [this](const std::string& msg,
                       size_t wireSize,
                       bool decompressionError,
                       WebSocketTransport::MessageKind messageKind)
                {
                    WebSocketMessageType webSocketMessageType;
                    switch (messageKind)
                    {
                        case WebSocketTransport::MSG:
                        {
                            webSocketMessageType = WebSocket_MessageType_Message;
                        } break;

                        case WebSocketTransport::PING:
                        {
                            webSocketMessageType = WebSocket_MessageType_Ping;
                        } break;

                        case WebSocketTransport::PONG:
                        {
                            webSocketMessageType = WebSocket_MessageType_Pong;
                        } break;

                        case WebSocketTransport::FRAGMENT:
                        {
                            webSocketMessageType = WebSocket_MessageType_Fragment;
                        } break;
                    }

                    WebSocketErrorInfo webSocketErrorInfo;
                    webSocketErrorInfo.decompressionError = decompressionError;

                    _onMessageCallback(webSocketMessageType, msg, wireSize,
                                       webSocketErrorInfo, WebSocketOpenInfo(),
                                       WebSocketCloseInfo());

                    WebSocket::invokeTrafficTrackerCallback(msg.size(), true);
                });

            // If we aren't trying to reconnect automatically, exit if we aren't connected
            if (!isConnected() && !_automaticReconnection) return;
        }
    }

    void WebSocket::setOnMessageCallback(const OnMessageCallback& callback)
    {
        _onMessageCallback = callback;
    }

    void WebSocket::setTrafficTrackerCallback(const OnTrafficTrackerCallback& callback)
    {
        _onTrafficTrackerCallback = callback;
    }

    void WebSocket::resetTrafficTrackerCallback()
    {
        setTrafficTrackerCallback(nullptr);
    }

    void WebSocket::invokeTrafficTrackerCallback(size_t size, bool incoming)
    {
        if (_onTrafficTrackerCallback)
        {
            _onTrafficTrackerCallback(size, incoming);
        }
    }

    WebSocketSendInfo WebSocket::send(const std::string& text,
                                      const OnProgressCallback& onProgressCallback)
    {
        return sendMessage(text, SendMessageKind::Binary, onProgressCallback);
    }

    WebSocketSendInfo WebSocket::sendText(const std::string& text,
                                          const OnProgressCallback& onProgressCallback)
    {
        return sendMessage(text, SendMessageKind::Text, onProgressCallback);
    }

    WebSocketSendInfo WebSocket::ping(const std::string& text)
    {
        // Standard limit ping message size
        constexpr size_t pingMaxPayloadSize = 125;
        if (text.size() > pingMaxPayloadSize) return WebSocketSendInfo(false);

        return sendMessage(text, SendMessageKind::Ping);
    }

    WebSocketSendInfo WebSocket::sendMessage(const std::string& text,
                                             SendMessageKind sendMessageKind,
                                             const OnProgressCallback& onProgressCallback)
    {
        if (!isConnected()) return WebSocketSendInfo(false);

        //
        // It is OK to read and write on the same socket in 2 different threads.
        // https://stackoverflow.com/questions/1981372/are-parallel-calls-to-send-recv-on-the-same-socket-valid
        //
        // This makes it so that messages are sent right away, and we dont need
        // a timeout while we poll to keep wake ups to a minimum (which helps
        // with battery life), and use the system select call to notify us when
        // incoming messages are arriving / there's data to be received.
        //
        std::lock_guard<std::mutex> lock(_writeMutex);
        WebSocketSendInfo webSocketSendInfo;

        switch (sendMessageKind)
        {
            case SendMessageKind::Text:
            {
                webSocketSendInfo = _ws.sendText(text, onProgressCallback);
            } break;

            case SendMessageKind::Binary:
            {
                webSocketSendInfo = _ws.sendBinary(text, onProgressCallback);
            } break;

            case SendMessageKind::Ping:
            {
                webSocketSendInfo = _ws.sendPing(text);
            } break;
        }

        WebSocket::invokeTrafficTrackerCallback(webSocketSendInfo.wireSize, false);

        return webSocketSendInfo;
    }

    ReadyState WebSocket::getReadyState() const
    {
        switch (_ws.getReadyState())
        {
            case ix::WebSocketTransport::OPEN: return WebSocket_ReadyState_Open;
            case ix::WebSocketTransport::CONNECTING: return WebSocket_ReadyState_Connecting;
            case ix::WebSocketTransport::CLOSING: return WebSocket_ReadyState_Closing;
            case ix::WebSocketTransport::CLOSED: return WebSocket_ReadyState_Closed;
            default: return WebSocket_ReadyState_Closed;
        }
    }

    std::string WebSocket::readyStateToString(ReadyState readyState)
    {
        switch (readyState)
        {
            case WebSocket_ReadyState_Open: return "OPEN";
            case WebSocket_ReadyState_Connecting: return "CONNECTING";
            case WebSocket_ReadyState_Closing: return "CLOSING";
            case WebSocket_ReadyState_Closed: return "CLOSED";
            default: return "CLOSED";
        }
    }

    void WebSocket::enableAutomaticReconnection()
    {
        _automaticReconnection = true;
    }

    void WebSocket::disableAutomaticReconnection()
    {
        _automaticReconnection = false;
    }

    size_t WebSocket::bufferedAmount() const
    {
        return _ws.bufferedAmount();
    }
}
