/*
 *  IXWebSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocket.h"
#include "IXSetThreadName.h"
#include "IXWebSocketHandshake.h"

#include <cmath>
#include <cassert>

namespace
{
    uint64_t calculateRetryWaitMilliseconds(uint32_t retry_count)
    {
        uint64_t wait_time;

        if (retry_count <= 6)
        {
            // max wait_time is 6400 ms (2 ^ 6 = 64)
            wait_time = ((uint64_t)std::pow(2, retry_count) * 100L);
        }
        else
        {
            wait_time = 10 * 1000; // 10 sec
        }

        return wait_time;
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
                _onMessageCallback(WebSocketMessageType::Close, "", wireSize,
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

    void WebSocket::stop(uint16_t code,
                         const std::string& reason)
    {
        close(code, reason);

        if (_thread.joinable())
        {
            // wait until working thread will exit
            // it will exit after close operation is finished
            _stop = true;
            _thread.join();
            _stop = false;
        }
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

        _onMessageCallback(WebSocketMessageType::Open, "", 0,
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

        _onMessageCallback(WebSocketMessageType::Open, "", 0,
                           WebSocketErrorInfo(),
                           WebSocketOpenInfo(status.uri, status.headers),
                           WebSocketCloseInfo());
        return status;
    }

    bool WebSocket::isConnected() const
    {
        return getReadyState() == ReadyState::Open;
    }

    bool WebSocket::isClosing() const
    {
        return getReadyState() == ReadyState::Closing;
    }

    void WebSocket::close(uint16_t code,
                          const std::string& reason)
    {
        _ws.close(code, reason);
    }

    void WebSocket::checkConnection(bool firstConnectionAttempt)
    {
        using millis = std::chrono::duration<double, std::milli>;

        uint32_t retries = 0;
        millis duration;

        // Try to connect perpertually
        while (true)
        {
            if (isConnected() || isClosing() || _stop)
            {
                break;
            }

            if (!firstConnectionAttempt && !_automaticReconnection)
            {
                // Do not attempt to reconnect
                break;
            }

            firstConnectionAttempt = false;

            // Only sleep if we are retrying
            if (duration.count() > 0)
            {
                // to do: make sleeping conditional
                std::this_thread::sleep_for(duration);
            }

            // Try to connect synchronously
            ix::WebSocketInitResult status = connect(_handshakeTimeoutSecs);

            if (!status.success)
            {
                WebSocketErrorInfo connectErr;

                if (_automaticReconnection)
                {
                    duration = millis(calculateRetryWaitMilliseconds(retries++));

                    connectErr.wait_time = duration.count();
                    connectErr.retries = retries;
                }

                connectErr.reason      = status.errorStr;
                connectErr.http_status = status.http_status;

                _onMessageCallback(WebSocketMessageType::Error, "", 0,
                                   connectErr, WebSocketOpenInfo(),
                                   WebSocketCloseInfo());
            }
        }
    }

    void WebSocket::run()
    {
        setThreadName(getUrl());

        bool firstConnectionAttempt = true;

        while (true)
        {
            // 1. Make sure we are always connected
            checkConnection(firstConnectionAttempt);

            firstConnectionAttempt = false;

            // if here we are closed then checkConnection was not able to connect
            if (getReadyState() == ReadyState::Closed)
            {
                break;
            }

            // We can avoid to poll if we want to stop and are not closing
            if (_stop && !isClosing()) break;

            // 2. Poll to see if there's any new data available
            WebSocketTransport::PollResult pollResult = _ws.poll();

            // 3. Dispatch the incoming messages
            _ws.dispatch(
                pollResult,
                [this](const std::string& msg,
                       size_t wireSize,
                       bool decompressionError,
                       WebSocketTransport::MessageKind messageKind)
                {
                    WebSocketMessageType webSocketMessageType;
                    switch (messageKind)
                    {
                        default:
                        case WebSocketTransport::MessageKind::MSG:
                        {
                            webSocketMessageType = WebSocketMessageType::Message;
                        } break;

                        case WebSocketTransport::MessageKind::PING:
                        {
                            webSocketMessageType = WebSocketMessageType::Ping;
                        } break;

                        case WebSocketTransport::MessageKind::PONG:
                        {
                            webSocketMessageType = WebSocketMessageType::Pong;
                        } break;

                        case WebSocketTransport::MessageKind::FRAGMENT:
                        {
                            webSocketMessageType = WebSocketMessageType::Fragment;
                        } break;
                    }

                    WebSocketErrorInfo webSocketErrorInfo;
                    webSocketErrorInfo.decompressionError = decompressionError;

                    _onMessageCallback(webSocketMessageType, msg, wireSize,
                                       webSocketErrorInfo, WebSocketOpenInfo(),
                                       WebSocketCloseInfo());

                    WebSocket::invokeTrafficTrackerCallback(msg.size(), true);
                });
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

    WebSocketSendInfo WebSocket::send(const std::string& data,
                                      const OnProgressCallback& onProgressCallback)
    {
        return sendMessage(data, SendMessageKind::Binary, onProgressCallback);
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
            case ix::WebSocketTransport::ReadyState::OPEN      : return ReadyState::Open;
            case ix::WebSocketTransport::ReadyState::CONNECTING: return ReadyState::Connecting;
            case ix::WebSocketTransport::ReadyState::CLOSING   : return ReadyState::Closing;
            case ix::WebSocketTransport::ReadyState::CLOSED    : return ReadyState::Closed;
            default: return ReadyState::Closed;
        }
    }

    std::string WebSocket::readyStateToString(ReadyState readyState)
    {
        switch (readyState)
        {
            case ReadyState::Open      : return "OPEN";
            case ReadyState::Connecting: return "CONNECTING";
            case ReadyState::Closing   : return "CLOSING";
            case ReadyState::Closed    : return "CLOSED";
            default: return "UNKNOWN";
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

    bool WebSocket::isAutomaticReconnectionEnabled() const
    {
        return _automaticReconnection;
    }

    size_t WebSocket::bufferedAmount() const
    {
        return _ws.bufferedAmount();
    }
}
