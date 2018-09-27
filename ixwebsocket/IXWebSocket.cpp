/*
 *  IXWebSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocket.h"

#include <iostream>
#include <cmath>
#include <cassert>

namespace {

    // FIXME: put this in a shared location, and use it in 
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

namespace ix {

    OnTrafficTrackerCallback WebSocket::_onTrafficTrackerCallback = nullptr;

    WebSocket::WebSocket() :
        _verbose(false),
        _onMessageCallback(OnMessageCallback()),
        _stop(false),
        _automaticReconnection(true)
    {
    }

    WebSocket::~WebSocket() 
    {
        stop();
    }

    void WebSocket::configure(const std::string& url)
    {
        std::lock_guard<std::mutex> lock(_urlMutex);
        _url = url;
    }

    void WebSocket::start()
    {
        if (_thread.joinable()) return; // we've already been started

        _thread = std::thread(&WebSocket::run, this);
    }

    void WebSocket::stop()
    {
        _automaticReconnection = false;

        close();

        if (!_thread.joinable())
        {
            _automaticReconnection = true;
            return;
        }

        _stop = true;
        _thread.join();
        _stop = false;

        _automaticReconnection = true;
    }

    WebSocketInitResult WebSocket::connect()
    {
        {
            std::lock_guard<std::mutex> lock(_urlMutex);
            _ws.configure(_url);
        }

        _ws.setOnStateChangeCallback(
            [this](WebSocketTransport::ReadyStateValues readyStateValue)
            {
                if (readyStateValue == WebSocketTransport::CLOSED)
                {
                    _onMessageCallback(WebSocket_MessageType_Close, "", WebSocketErrorInfo());
                }

                if (_verbose)
                {
                    std::cout << "connection state changed -> " 
                              << readyStateToString(getReadyState()) 
                              << std::endl;
                }
            }
        );

        WebSocketInitResult status = _ws.init();
        if (!status.success)
        {
            return status;
        }

        _onMessageCallback(WebSocket_MessageType_Open, "", WebSocketErrorInfo());
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

        while (true) 
        {
            if (isConnected() || isClosing() || _stop || !_automaticReconnection)
            {
                break;
            }

            status = connect();

            if (!status.success && !_stop)
            {
                duration = millis(calculateRetryWaitMilliseconds(retries++));

                connectErr.retries = retries;
                connectErr.wait_time = duration.count();
                connectErr.reason = status.errorStr;
                connectErr.http_status = status.http_status;
                _onMessageCallback(WebSocket_MessageType_Error, "", connectErr);

                if (_verbose) std::cout << "Sleeping for " << duration.count() << "ms" << std::endl;

                std::this_thread::sleep_for(duration);
            }
        }
    }

    void WebSocket::run()
    {
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
                [this](const std::string& msg)
                {
                    _onMessageCallback(WebSocket_MessageType_Message, msg, WebSocketErrorInfo());

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

    bool WebSocket::send(const std::string& text)
    {
        if (!isConnected()) return false;

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
        _ws.sendBinary(text);

        WebSocket::invokeTrafficTrackerCallback(text.size(), false);

        return true;
    }

    ReadyState WebSocket::getReadyState() const
    {
        switch (_ws.getReadyState()) 
        {
            case ix::WebSocketTransport::OPEN: return WebSocket_ReadyState_Open;
            case ix::WebSocketTransport::CONNECTING: return WebSocket_ReadyState_Connecting;
            case ix::WebSocketTransport::CLOSING: return WebSocket_ReadyState_Closing;
            case ix::WebSocketTransport::CLOSED: return WebSocket_ReadyState_Closed;
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
        }
    }

    const std::string& WebSocket::getUrl() const
    {
        std::lock_guard<std::mutex> lock(_urlMutex);
        return _url;
    }
}
