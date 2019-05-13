/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2012, 2013 <dhbaird@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 *  IXWebSocketTransport.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
 */

//
// Adapted from https://github.com/dhbaird/easywsclient
//

#include "IXWebSocketTransport.h"
#include "IXWebSocketHandshake.h"
#include "IXWebSocketHttpHeaders.h"
#include "IXUrlParser.h"
#include "IXSocketFactory.h"

#include <string.h>
#include <stdlib.h>

#include <cstdlib>
#include <vector>
#include <string>
#include <cstdarg>
#include <sstream>
#include <chrono>
#include <thread>


namespace
{
    int greatestCommonDivisor(int a, int b)
    {
        while (b != 0)
        {
            int t = b;
            b = a % b;
            a = t;
        }

        return a;
    }
}

namespace ix
{
    const std::string WebSocketTransport::kPingMessage("ixwebsocket::heartbeat");
    const int WebSocketTransport::kDefaultPingIntervalSecs(-1);
    const int WebSocketTransport::kDefaultPingTimeoutSecs(-1);
    const bool WebSocketTransport::kDefaultEnablePong(true);
    const int WebSocketTransport::kClosingMaximumWaitingDelayInMs(200);
    constexpr size_t WebSocketTransport::kChunkSize;

    const uint16_t WebSocketTransport::kInternalErrorCode(1011);
    const uint16_t WebSocketTransport::kAbnormalCloseCode(1006);
    const uint16_t WebSocketTransport::kProtocolErrorCode(1002);
    const uint16_t WebSocketTransport::kNoStatusCodeErrorCode(1005);
    const std::string WebSocketTransport::kInternalErrorMessage("Internal error");
    const std::string WebSocketTransport::kAbnormalCloseMessage("Abnormal closure");
    const std::string WebSocketTransport::kPingTimeoutMessage("Ping timeout");
    const std::string WebSocketTransport::kProtocolErrorMessage("Protocol error");
    const std::string WebSocketTransport::kNoStatusCodeErrorMessage("No status code");

    WebSocketTransport::WebSocketTransport() :
        _useMask(true),
        _readyState(ReadyState::CLOSED),
        _closeCode(kInternalErrorCode),
        _closeReason(kInternalErrorMessage),
        _closeWireSize(0),
        _closeRemote(false),
        _enablePerMessageDeflate(false),
        _requestInitCancellation(false),
        _closingTimePoint(std::chrono::steady_clock::now()),
        _enablePong(kDefaultEnablePong),
        _pingIntervalSecs(kDefaultPingIntervalSecs),
        _pingTimeoutSecs(kDefaultPingTimeoutSecs),
        _pingIntervalOrTimeoutGCDSecs(-1),
        _nextGCDTimePoint(std::chrono::steady_clock::now()),
        _lastSendPingTimePoint(std::chrono::steady_clock::now()),
        _lastReceivePongTimePoint(std::chrono::steady_clock::now())
    {
        _readbuf.resize(kChunkSize);
    }

    WebSocketTransport::~WebSocketTransport()
    {
        ;
    }

    void WebSocketTransport::configure(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions,
                                       bool enablePong,
                                       int pingIntervalSecs,
                                       int pingTimeoutSecs)
    {
        _perMessageDeflateOptions = perMessageDeflateOptions;
        _enablePerMessageDeflate = _perMessageDeflateOptions.enabled();
        _enablePong = enablePong;
        _pingIntervalSecs = pingIntervalSecs;
        _pingTimeoutSecs = pingTimeoutSecs;

        if (pingIntervalSecs > 0 && pingTimeoutSecs > 0)
        {
            _pingIntervalOrTimeoutGCDSecs = greatestCommonDivisor(pingIntervalSecs,
                                                                  pingTimeoutSecs);
        }
        else if (_pingTimeoutSecs > 0)
        {
            _pingIntervalOrTimeoutGCDSecs = pingTimeoutSecs;
        }
        else
        {
            _pingIntervalOrTimeoutGCDSecs = pingIntervalSecs;
        }
    }

    // Client
    WebSocketInitResult WebSocketTransport::connectToUrl(const std::string& url,
                                                         int timeoutSecs)
    {
        std::string protocol, host, path, query;
        int port;

        if (!UrlParser::parse(url, protocol, host, path, query, port))
        {
            return WebSocketInitResult(false, 0,
                                       std::string("Could not parse URL ") + url);
        }

        bool tls = protocol == "wss";
        std::string errorMsg;
        _socket = createSocket(tls, errorMsg);

        if (!_socket)
        {
            return WebSocketInitResult(false, 0, errorMsg);
        }

        WebSocketHandshake webSocketHandshake(_requestInitCancellation,
                                              _socket,
                                              _perMessageDeflate,
                                              _perMessageDeflateOptions,
                                              _enablePerMessageDeflate);

        auto result = webSocketHandshake.clientHandshake(url, host, path, port,
                                                         timeoutSecs);
        if (result.success)
        {
            setReadyState(ReadyState::OPEN);
        }
        return result;
    }

    // Server
    WebSocketInitResult WebSocketTransport::connectToSocket(int fd, int timeoutSecs)
    {
        // Server should not mask the data it sends to the client
        _useMask = false;

        std::string errorMsg;
        _socket = createSocket(fd, errorMsg);

        if (!_socket)
        {
            return WebSocketInitResult(false, 0, errorMsg);
        }

        WebSocketHandshake webSocketHandshake(_requestInitCancellation,
                                              _socket,
                                              _perMessageDeflate,
                                              _perMessageDeflateOptions,
                                              _enablePerMessageDeflate);

        auto result = webSocketHandshake.serverHandshake(fd, timeoutSecs);
        if (result.success)
        {
            setReadyState(ReadyState::OPEN);
        }
        return result;
    }

    WebSocketTransport::ReadyState WebSocketTransport::getReadyState() const
    {
        return _readyState;
    }

    void WebSocketTransport::setReadyState(ReadyState readyState)
    {
        // No state change, return
        if (_readyState == readyState) return;

        if (readyState == ReadyState::CLOSED)
        {
            std::lock_guard<std::mutex> lock(_closeDataMutex);
            _onCloseCallback(_closeCode, _closeReason, _closeWireSize, _closeRemote);
            _closeCode = kInternalErrorCode;
            _closeReason = kInternalErrorMessage;
            _closeWireSize = 0;
            _closeRemote = false;
        }
        else if (readyState == ReadyState::OPEN)
        {
            initTimePointsAndGCDAfterConnect();
        }

        _readyState = readyState;
    }

    void WebSocketTransport::setOnCloseCallback(const OnCloseCallback& onCloseCallback)
    {
        _onCloseCallback = onCloseCallback;
    }

    void WebSocketTransport::initTimePointsAndGCDAfterConnect()
    {
        {
            std::lock_guard<std::mutex> lock(_lastSendPingTimePointMutex);
            _lastSendPingTimePoint = std::chrono::steady_clock::now();
        } 
        {
            std::lock_guard<std::mutex> lock(_lastReceivePongTimePointMutex);
            _lastReceivePongTimePoint = std::chrono::steady_clock::now();
        }

        if (_pingIntervalOrTimeoutGCDSecs > 0)
        {
            _nextGCDTimePoint = std::chrono::steady_clock::now() + std::chrono::seconds(_pingIntervalOrTimeoutGCDSecs);
        }
    }

    // Only consider send PING time points for that computation.
    bool WebSocketTransport::pingIntervalExceeded()
    {
        if (_pingIntervalSecs <= 0)
            return false;

        std::lock_guard<std::mutex> lock(_lastSendPingTimePointMutex);
        auto now = std::chrono::steady_clock::now();
        return now - _lastSendPingTimePoint > std::chrono::seconds(_pingIntervalSecs);
    }

    bool WebSocketTransport::pingTimeoutExceeded()
    {
        if (_pingTimeoutSecs <= 0)
            return false;

        std::lock_guard<std::mutex> lock(_lastReceivePongTimePointMutex);
        auto now = std::chrono::steady_clock::now();
        return now - _lastReceivePongTimePoint > std::chrono::seconds(_pingTimeoutSecs);
    }

    bool WebSocketTransport::closingDelayExceeded()
    {
        std::lock_guard<std::mutex> lock(_closingTimePointMutex);
        auto now = std::chrono::steady_clock::now();
        return now - _closingTimePoint > std::chrono::milliseconds(kClosingMaximumWaitingDelayInMs);
    }

    WebSocketTransport::PollResult WebSocketTransport::poll()
    {
        if (_readyState == ReadyState::OPEN)
        {
            // if (1) ping timeout is enabled and (2) duration since last received
            // ping response (PONG) exceeds the maximum delay, then close the connection
            if (pingTimeoutExceeded())
            {
                close(kInternalErrorCode, kPingTimeoutMessage);
            }
            // If ping is enabled and no ping has been sent for a duration
            // exceeding our ping interval, send a ping to the server.
            else if (pingIntervalExceeded())
            {
                std::stringstream ss;
                ss << kPingMessage << "::" << _pingIntervalSecs << "s";
                sendPing(ss.str());
            }
        }
        
        // No timeout if state is not OPEN, otherwise computed
        // pingIntervalOrTimeoutGCD (equals to -1 if no ping and no ping timeout are set)
        int lastingTimeoutDelayInMs = (_readyState != ReadyState::OPEN) ? 0 : _pingIntervalOrTimeoutGCDSecs;

        if (_pingIntervalOrTimeoutGCDSecs > 0)
        {
            // compute lasting delay to wait for next ping / timeout, if at least one set
            auto now = std::chrono::steady_clock::now();

            if (now >= _nextGCDTimePoint)
            {
                _nextGCDTimePoint = now + std::chrono::seconds(_pingIntervalOrTimeoutGCDSecs);
            
                lastingTimeoutDelayInMs = _pingIntervalOrTimeoutGCDSecs * 1000;
            }
            else 
            {
                lastingTimeoutDelayInMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(_nextGCDTimePoint - now).count();
            }
        }

#ifdef _WIN32
        if (lastingTimeoutDelayInMs <= 0) lastingTimeoutDelayInMs = 20;
#endif

        // poll the socket
        PollResultType pollResult = _socket->poll(lastingTimeoutDelayInMs);

        // Make sure we send all the buffered data
        // there can be a lot of it for large messages.
        if (pollResult == PollResultType::SendRequest)
        {
            while (!isSendBufferEmpty() && !_requestInitCancellation)
            {
                // Wait with a 10ms timeout until the socket is ready to write.
                // This way we are not busy looping
                PollResultType result = _socket->isReadyToWrite(10);

                if (result == PollResultType::Error)
                {
                    _socket->close();
                    setReadyState(ReadyState::CLOSED);
                    break;
                }
                else if (result == PollResultType::ReadyForWrite)
                {
                    sendOnSocket();
                }
            }
        }
        else if (pollResult == PollResultType::ReadyForRead)
        {
            while (true)
            {
                ssize_t ret = _socket->recv((char*)&_readbuf[0], _readbuf.size());

                if (ret < 0 && Socket::isWaitNeeded())
                {
                    break;
                }
                else if (ret <= 0)
                {
                    // if there are received data pending to be processed, then delay the abnormal closure
                    // to after dispatch (other close code/reason could be read from the buffer)
                    
                    _socket->close();

                    return PollResult::AbnormalClose;
                }
                else
                {
                    _rxbuf.insert(_rxbuf.end(),
                                  _readbuf.begin(),
                                  _readbuf.begin() + ret);
                }
            }
        }
        else if (pollResult == PollResultType::Error)
        {
            _socket->close();
        }
        else if (pollResult == PollResultType::CloseRequest)
        {
            _socket->close();
        }

        if (_readyState == ReadyState::CLOSING && closingDelayExceeded())
        {
            _rxbuf.clear();
            // close code and reason were set when calling close()
            _socket->close();
            setReadyState(ReadyState::CLOSED);
        }

        return PollResult::Succeeded;
    }

    bool WebSocketTransport::isSendBufferEmpty() const
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);
        return _txbuf.empty();
    }

    void WebSocketTransport::appendToSendBuffer(const std::vector<uint8_t>& header,
                                                std::string::const_iterator begin,
                                                std::string::const_iterator end,
                                                uint64_t message_size,
                                                uint8_t masking_key[4])
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);

        _txbuf.insert(_txbuf.end(), header.begin(), header.end());
        _txbuf.insert(_txbuf.end(), begin, end);

        if (_useMask)
        {
            for (size_t i = 0; i != (size_t) message_size; ++i)
            {
                *(_txbuf.end() - (size_t) message_size + i) ^= masking_key[i&0x3];
            }
        }
    }

    void WebSocketTransport::unmaskReceiveBuffer(const wsheader_type& ws)
    {
        if (ws.mask)
        {
            for (size_t j = 0; j != ws.N; ++j)
            {
                _rxbuf[j+ws.header_size] ^= ws.masking_key[j&0x3];
            }
        }
    }

    //
    // http://tools.ietf.org/html/rfc6455#section-5.2  Base Framing Protocol
    //
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-------+-+-------------+-------------------------------+
    // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    // | |1|2|3|       |K|             |                               |
    // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    // |     Extended payload length continued, if payload len == 127  |
    // + - - - - - - - - - - - - - - - +-------------------------------+
    // |                               |Masking-key, if MASK set to 1  |
    // +-------------------------------+-------------------------------+
    // | Masking-key (continued)       |          Payload Data         |
    // +-------------------------------- - - - - - - - - - - - - - - - +
    // :                     Payload Data continued ...                :
    // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    // |                     Payload Data continued ...                |
    // +---------------------------------------------------------------+
    //
    void WebSocketTransport::dispatch(WebSocketTransport::PollResult pollResult,
                                      const OnMessageCallback& onMessageCallback)
    {
        while (true)
        {
            wsheader_type ws;
            if (_rxbuf.size() < 2) break; /* Need at least 2 */
            const uint8_t * data = (uint8_t *) &_rxbuf[0]; // peek, but don't consume
            ws.fin = (data[0] & 0x80) == 0x80;
            ws.rsv1 = (data[0] & 0x40) == 0x40;
            ws.opcode = (wsheader_type::opcode_type) (data[0] & 0x0f);
            ws.mask = (data[1] & 0x80) == 0x80;
            ws.N0 = (data[1] & 0x7f);
            ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 8 : 0) + (ws.mask? 4 : 0);
            if (_rxbuf.size() < ws.header_size) break; /* Need: ws.header_size - _rxbuf.size() */

            //
            // Calculate payload length:
            // 0-125 mean the payload is that long.
            // 126 means that the following two bytes indicate the length,
            // 127 means the next 8 bytes indicate the length.
            //
            int i = 0;
            if (ws.N0 < 126)
            {
                ws.N = ws.N0;
                i = 2;
            }
            else if (ws.N0 == 126)
            {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 8;
                ws.N |= ((uint64_t) data[3]) << 0;
                i = 4;
            }
            else if (ws.N0 == 127)
            {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 56;
                ws.N |= ((uint64_t) data[3]) << 48;
                ws.N |= ((uint64_t) data[4]) << 40;
                ws.N |= ((uint64_t) data[5]) << 32;
                ws.N |= ((uint64_t) data[6]) << 24;
                ws.N |= ((uint64_t) data[7]) << 16;
                ws.N |= ((uint64_t) data[8]) << 8;
                ws.N |= ((uint64_t) data[9]) << 0;
                i = 10;
            }
            else
            {
                // invalid payload length according to the spec. bail out
                return;
            }

            if (ws.mask)
            {
                ws.masking_key[0] = ((uint8_t) data[i+0]) << 0;
                ws.masking_key[1] = ((uint8_t) data[i+1]) << 0;
                ws.masking_key[2] = ((uint8_t) data[i+2]) << 0;
                ws.masking_key[3] = ((uint8_t) data[i+3]) << 0;
            }
            else
            {
                ws.masking_key[0] = 0;
                ws.masking_key[1] = 0;
                ws.masking_key[2] = 0;
                ws.masking_key[3] = 0;
            }

            if (_rxbuf.size() < ws.header_size+ws.N)
            {
                return; /* Need: ws.header_size+ws.N - _rxbuf.size() */
            }

            // We got a whole message, now do something with it:
            if (
                   ws.opcode == wsheader_type::TEXT_FRAME
                || ws.opcode == wsheader_type::BINARY_FRAME
                || ws.opcode == wsheader_type::CONTINUATION
            ) {
                unmaskReceiveBuffer(ws);

                //
                // Usual case. Small unfragmented messages
                //
                if (ws.fin && _chunks.empty())
                {
                    emitMessage(MessageKind::MSG,
                                std::string(_rxbuf.begin()+ws.header_size,
                                            _rxbuf.begin()+ws.header_size+(size_t) ws.N),
                                ws,
                                onMessageCallback);
                }
                else
                {
                    //
                    // Add intermediary message to our chunk list.
                    // We use a chunk list instead of a big buffer because resizing
                    // large buffer can be very costly when we need to re-allocate
                    // the internal buffer which is slow and can let the internal OS
                    // receive buffer fill out.
                    //
                    _chunks.emplace_back(
                        std::vector<uint8_t>(_rxbuf.begin()+ws.header_size,
                                             _rxbuf.begin()+ws.header_size+(size_t)ws.N));
                    if (ws.fin)
                    {
                        emitMessage(MessageKind::MSG, getMergedChunks(), ws, onMessageCallback);
                        _chunks.clear();
                    }
                    else
                    {
                        emitMessage(MessageKind::FRAGMENT, std::string(), ws, onMessageCallback);
                    }
                }
            }
            else if (ws.opcode == wsheader_type::PING)
            {
                unmaskReceiveBuffer(ws);

                std::string pingData(_rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size + (size_t) ws.N);

                if (_enablePong)
                {
                    // Reply back right away
                    bool compress = false;
                    sendData(wsheader_type::PONG, pingData, compress);
                }

                emitMessage(MessageKind::PING, pingData, ws, onMessageCallback);
            }
            else if (ws.opcode == wsheader_type::PONG)
            {
                unmaskReceiveBuffer(ws);
                std::string pongData(_rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size + (size_t) ws.N);

                std::lock_guard<std::mutex> lck(_lastReceivePongTimePointMutex);
                _lastReceivePongTimePoint = std::chrono::steady_clock::now();

                emitMessage(MessageKind::PONG, pongData, ws, onMessageCallback);
            }
            else if (ws.opcode == wsheader_type::CLOSE)
            {
                std::string reason;
                uint16_t code = 0;

                unmaskReceiveBuffer(ws);

                if (ws.N >= 2)
                {
                    // Extract the close code first, available as the first 2 bytes
                    code |= ((uint64_t) _rxbuf[ws.header_size])   << 8;
                    code |= ((uint64_t) _rxbuf[ws.header_size+1]) << 0;

                    // Get the reason.
                    if (ws.N > 2)
                    {
                        reason.assign(_rxbuf.begin()+ws.header_size + 2,
                                      _rxbuf.begin()+ws.header_size + (size_t) ws.N);
                    }
                }
                else
                {
                    // no close code received
                    code = kNoStatusCodeErrorCode;
                    reason = kNoStatusCodeErrorMessage;
                }

                // We receive a CLOSE frame from remote and are NOT the ones who triggered the close
                if (_readyState != ReadyState::CLOSING)
                {
                    // send back the CLOSE frame
                    sendCloseFrame(code, reason);

                    _socket->wakeUpFromPoll(Socket::kCloseRequest);

                    bool remote = true;
                    closeSocketAndSwitchToClosedState(code, reason, _rxbuf.size(), remote);
                }
                else
                {
                    // we got the CLOSE frame answer from our close, so we can close the connection if
                    // the code/reason are the same
                    bool identicalReason;
                    {
                        std::lock_guard<std::mutex> lock(_closeDataMutex);
                        identicalReason = _closeCode == code && _closeReason == reason;
                    }

                    if (identicalReason)
                    {
                        bool remote = false;
                        closeSocketAndSwitchToClosedState(code, reason, _rxbuf.size(), remote);
                    }
                }
            }
            else
            {
                // Unexpected frame type

                close(kProtocolErrorCode, kProtocolErrorMessage, _rxbuf.size());
            }

            // Erase the message that has been processed from the input/read buffer
            _rxbuf.erase(_rxbuf.begin(),
                         _rxbuf.begin() + ws.header_size + (size_t) ws.N);
        }

        // if an abnormal closure was raised in poll, and nothing else triggered a CLOSED state in
        // the received and processed data then close the connection
        if (pollResult == PollResult::AbnormalClose)
        {
            _rxbuf.clear();

            // if we previously closed the connection (CLOSING state), then set state to CLOSED (code/reason were set before)
            if (_readyState == ReadyState::CLOSING)
            {
                _socket->close();
                setReadyState(ReadyState::CLOSED);
            }
            // if we weren't closing, then close using abnormal close code and message 
            else if (_readyState != ReadyState::CLOSED)
            {
                closeSocketAndSwitchToClosedState(kAbnormalCloseCode, kAbnormalCloseMessage, 0, false);
            }
        }
    }

    std::string WebSocketTransport::getMergedChunks() const
    {
        size_t length = 0;
        for (auto&& chunk : _chunks)
        {
            length += chunk.size();
        }

        std::string msg;
        msg.reserve(length);

        for (auto&& chunk : _chunks)
        {
            std::string str(chunk.begin(), chunk.end());
            msg += str;
        }

        return msg;
    }

    void WebSocketTransport::emitMessage(MessageKind messageKind,
                                         const std::string& message,
                                         const wsheader_type& ws,
                                         const OnMessageCallback& onMessageCallback)
    {
        size_t wireSize = message.size();

        // When the RSV1 bit is 1 it means the message is compressed
        if (_enablePerMessageDeflate && ws.rsv1 && messageKind != MessageKind::FRAGMENT)
        {
            std::string decompressedMessage;
            bool success = _perMessageDeflate.decompress(message, decompressedMessage);
            onMessageCallback(decompressedMessage, wireSize, !success, messageKind);
        }
        else
        {
            onMessageCallback(message, wireSize, false, messageKind);
        }
    }

    unsigned WebSocketTransport::getRandomUnsigned()
    {
        auto now = std::chrono::system_clock::now();
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
        return static_cast<unsigned>(seconds);
    }

    WebSocketSendInfo WebSocketTransport::sendData(
        wsheader_type::opcode_type type,
        const std::string& message,
        bool compress,
        const OnProgressCallback& onProgressCallback)
    {
        if (_readyState != ReadyState::OPEN)
        {
            return WebSocketSendInfo();
        }

        size_t payloadSize = message.size();
        size_t wireSize = message.size();
        std::string compressedMessage;
        bool compressionError = false;

        std::string::const_iterator message_begin = message.begin();
        std::string::const_iterator message_end = message.end();

        if (compress)
        {
            if (!_perMessageDeflate.compress(message, compressedMessage))
            {
                bool success = false;
                compressionError = true;
                payloadSize = 0;
                wireSize = 0;
                return WebSocketSendInfo(success, compressionError, payloadSize, wireSize);
            }
            compressionError = false;
            wireSize = compressedMessage.size();

            message_begin = compressedMessage.begin();
            message_end = compressedMessage.end();
        }

        // Common case for most message. No fragmentation required.
        if (wireSize < kChunkSize)
        {
            sendFragment(type, true, message_begin, message_end, compress);
        }
        else
        {
            //
            // Large messages need to be fragmented
            //
            // Rules:
            // First message needs to specify a proper type (BINARY or TEXT)
            // Intermediary and last messages need to be of type CONTINUATION
            // Last message must set the fin byte.
            //
            auto steps = wireSize / kChunkSize;

            std::string::const_iterator begin = message_begin;
            std::string::const_iterator end = message_end;

            for (uint64_t i = 0 ; i < steps; ++i)
            {
                bool firstStep = i == 0;
                bool lastStep = (i+1) == steps;
                bool fin = lastStep;

                end = begin + kChunkSize;
                if (lastStep)
                {
                    end = message_end;
                }

                auto opcodeType = type;
                if (!firstStep)
                {
                    opcodeType = wsheader_type::CONTINUATION;
                }

                // Send message
                sendFragment(opcodeType, fin, begin, end, compress);

                if (onProgressCallback && !onProgressCallback((int)i, (int) steps))
                {
                    break;
                }

                begin += kChunkSize;
            }
        }

        // Request to flush the send buffer on the background thread if it isn't empty
        if (!isSendBufferEmpty())
        {
            _socket->wakeUpFromPoll(Socket::kSendRequest);
        }

        return WebSocketSendInfo(true, compressionError, payloadSize, wireSize);
    }

    void WebSocketTransport::sendFragment(wsheader_type::opcode_type type,
                                          bool fin,
                                          std::string::const_iterator message_begin,
                                          std::string::const_iterator message_end,
                                          bool compress)
    {
        uint64_t message_size = static_cast<uint64_t>(message_end - message_begin);

        unsigned x = getRandomUnsigned();
        uint8_t masking_key[4] = {};
        masking_key[0] = (x >> 24);
        masking_key[1] = (x >> 16) & 0xff;
        masking_key[2] = (x >> 8)  & 0xff;
        masking_key[3] = (x)       & 0xff;

        std::vector<uint8_t> header;
        header.assign(2 +
                      (message_size >= 126 ? 2 : 0) +
                      (message_size >= 65536 ? 6 : 0) +
                      (_useMask ? 4 : 0), 0);
        header[0] = type;

        // The fin bit indicate that this is the last fragment. Fin is French for end.
        if (fin)
        {
            header[0] |= 0x80;
        }

        // This bit indicate that the frame is compressed
        if (compress)
        {
            header[0] |= 0x40;
        }

        if (message_size < 126)
        {
            header[1] = (message_size & 0xff) | (_useMask ? 0x80 : 0);

            if (_useMask)
            {
                header[2] = masking_key[0];
                header[3] = masking_key[1];
                header[4] = masking_key[2];
                header[5] = masking_key[3];
            }
        }
        else if (message_size < 65536)
        {
            header[1] = 126 | (_useMask ? 0x80 : 0);
            header[2] = (message_size >> 8) & 0xff;
            header[3] = (message_size >> 0) & 0xff;

            if (_useMask)
            {
                header[4] = masking_key[0];
                header[5] = masking_key[1];
                header[6] = masking_key[2];
                header[7] = masking_key[3];
            }
        }
        else
        { // TODO: run coverage testing here
            header[1] = 127 | (_useMask ? 0x80 : 0);
            header[2] = (message_size >> 56) & 0xff;
            header[3] = (message_size >> 48) & 0xff;
            header[4] = (message_size >> 40) & 0xff;
            header[5] = (message_size >> 32) & 0xff;
            header[6] = (message_size >> 24) & 0xff;
            header[7] = (message_size >> 16) & 0xff;
            header[8] = (message_size >>  8) & 0xff;
            header[9] = (message_size >>  0) & 0xff;

            if (_useMask)
            {
                header[10] = masking_key[0];
                header[11] = masking_key[1];
                header[12] = masking_key[2];
                header[13] = masking_key[3];
            }
        }

        // _txbuf will keep growing until it can be transmitted over the socket:
        appendToSendBuffer(header, message_begin, message_end,
                           message_size, masking_key);

        // Now actually send this data
        sendOnSocket();
    }

    WebSocketSendInfo WebSocketTransport::sendPing(const std::string& message)
    {
        bool compress = false;
        WebSocketSendInfo info = sendData(wsheader_type::PING, message, compress);

        if (info.success)
        {
            std::lock_guard<std::mutex> lck(_lastSendPingTimePointMutex);
            _lastSendPingTimePoint = std::chrono::steady_clock::now();
        }

        return info;
    }

    WebSocketSendInfo WebSocketTransport::sendBinary(
        const std::string& message,
        const OnProgressCallback& onProgressCallback)

    {
        return sendData(wsheader_type::BINARY_FRAME, message,
                        _enablePerMessageDeflate, onProgressCallback);
    }

    WebSocketSendInfo WebSocketTransport::sendText(
        const std::string& message,
        const OnProgressCallback& onProgressCallback)

    {
        return sendData(wsheader_type::TEXT_FRAME, message,
                        _enablePerMessageDeflate, onProgressCallback);
    }

    void WebSocketTransport::sendOnSocket()
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);

        while (_txbuf.size())
        {
            ssize_t ret = _socket->send((char*)&_txbuf[0], _txbuf.size());

            if (ret < 0 && Socket::isWaitNeeded())
            {
                break;
            }
            else if (ret <= 0)
            {
                _socket->close();

                setReadyState(ReadyState::CLOSED);
                break;
            }
            else
            {
                _txbuf.erase(_txbuf.begin(), _txbuf.begin() + ret);
            }
        }
    }

    void WebSocketTransport::sendCloseFrame(uint16_t code, const std::string& reason)
    {
        bool compress = false;

        // if a status is set/was read
        if (code != kNoStatusCodeErrorCode)
        {
            // See list of close events here:
            // https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
            std::string closure{(char)(code >> 8), (char)(code & 0xff)};

            // copy reason after code
            closure.append(reason);

            sendData(wsheader_type::CLOSE, closure, compress);
        }
        else
        {
            // no close code/reason set
            sendData(wsheader_type::CLOSE, "", compress);
        }
    }

    void WebSocketTransport::closeSocketAndSwitchToClosedState(uint16_t code, const std::string& reason, size_t closeWireSize, bool remote)
    {
        _socket->close();
        {
            std::lock_guard<std::mutex> lock(_closeDataMutex);
            _closeCode = code;
            _closeReason = reason;
            _closeWireSize = closeWireSize;
            _closeRemote = remote;
        }
        setReadyState(ReadyState::CLOSED);
    }

    void WebSocketTransport::close(uint16_t code, const std::string& reason, size_t closeWireSize, bool remote)
    {
        _requestInitCancellation = true;

        if (_readyState == ReadyState::CLOSING || _readyState == ReadyState::CLOSED) return;

        sendCloseFrame(code, reason);
        {
            std::lock_guard<std::mutex> lock(_closeDataMutex);
            _closeCode = code;
            _closeReason = reason;
            _closeWireSize = closeWireSize;
            _closeRemote = remote;
        }
        {
            std::lock_guard<std::mutex> lock(_closingTimePointMutex);
            _closingTimePoint = std::chrono::steady_clock::now();
        }
        setReadyState(ReadyState::CLOSING);

        // wake up the poll, but do not close yet
        _socket->wakeUpFromPoll(Socket::kSendRequest);
    }

    size_t WebSocketTransport::bufferedAmount() const
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);
        return _txbuf.size();
    }

} // namespace ix
