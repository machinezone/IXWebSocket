/*
 *  IXWebSocketTransport.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

//
// Adapted from https://github.com/dhbaird/easywsclient
//

#include "IXWebSocketTransport.h"
#include "IXWebSocketHandshake.h"
#include "IXWebSocketHttpHeaders.h"

#ifdef IXWEBSOCKET_USE_TLS
# ifdef __APPLE__
#  include "IXSocketAppleSSL.h"
# else
#  include "IXSocketOpenSSL.h"
# endif
#endif

#include <string.h>
#include <stdlib.h>

#include <cstdlib>
#include <vector>
#include <string>
#include <cstdarg>
#include <iostream>
#include <sstream>


namespace ix
{
    WebSocketTransport::WebSocketTransport() :
        _readyState(CLOSED),
        _closeCode(0),
        _closeWireSize(0),
        _enablePerMessageDeflate(false),
        _requestInitCancellation(false)
    {

    }

    WebSocketTransport::~WebSocketTransport()
    {
        ;
    }

    void WebSocketTransport::configure(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions)
    {
        _perMessageDeflateOptions = perMessageDeflateOptions;
        _enablePerMessageDeflate = _perMessageDeflateOptions.enabled();
    }

    // Client
    WebSocketInitResult WebSocketTransport::connectToUrl(const std::string& url,
                                                         int timeoutSecs)
    {
        std::string protocol, host, path, query;
        int port;

        if (!WebSocketHandshake::parseUrl(url, protocol, host,
                                          path, query, port))
        {
            return WebSocketInitResult(false, 0,
                                       std::string("Could not parse URL ") + url);
        }

        if (protocol == "wss")
        {
            _socket.reset();
#ifdef IXWEBSOCKET_USE_TLS
# ifdef __APPLE__
             _socket = std::make_shared<SocketAppleSSL>();
# else
             _socket = std::make_shared<SocketOpenSSL>();
# endif
#else
            return WebSocketInitResult(false, 0, "TLS is not supported.");
#endif
        }
        else
        {
            _socket.reset();
            _socket = std::make_shared<Socket>();
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
            setReadyState(OPEN);
        }
        return result;
    }

    // Server
    WebSocketInitResult WebSocketTransport::connectToSocket(int fd, int timeoutSecs)
    {
        _socket.reset();
        _socket = std::make_shared<Socket>(fd);

        WebSocketHandshake webSocketHandshake(_requestInitCancellation,
                                              _socket,
                                              _perMessageDeflate,
                                              _perMessageDeflateOptions,
                                              _enablePerMessageDeflate);

        auto result = webSocketHandshake.serverHandshake(fd, timeoutSecs);
        if (result.success)
        {
            setReadyState(OPEN);
        }
        return result;
    }

    WebSocketTransport::ReadyStateValues WebSocketTransport::getReadyState() const 
    {
        return _readyState;
    }

    void WebSocketTransport::setReadyState(ReadyStateValues readyStateValue)
    {
        // No state change, return
        if (_readyState == readyStateValue) return;

        if (readyStateValue == CLOSED)
        {
            std::lock_guard<std::mutex> lock(_closeDataMutex);
            _onCloseCallback(_closeCode, _closeReason, _closeWireSize);
            _closeCode = 0;
            _closeReason = std::string();
            _closeWireSize = 0;
        }

        _readyState = readyStateValue;
    }

    void WebSocketTransport::setOnCloseCallback(const OnCloseCallback& onCloseCallback)
    {
        _onCloseCallback = onCloseCallback; 
    }

    void WebSocketTransport::poll()
    {
        _socket->poll(
            [this]()
            {
                while (true) 
                {
                    int N = (int) _rxbuf.size();

                    int ret;
                    _rxbuf.resize(N + 1500);
                    ret = _socket->recv((char*)&_rxbuf[0] + N, 1500);

                    if (ret < 0 && (_socket->getErrno() == EWOULDBLOCK || 
                                    _socket->getErrno() == EAGAIN)) {
                        _rxbuf.resize(N);
                        break;
                    }
                    else if (ret <= 0) 
                    {
                        _rxbuf.resize(N);

                        _socket->close();
                        setReadyState(CLOSED);
                        break;
                    }
                    else 
                    {
                        _rxbuf.resize(N + ret);
                    }
                }

                if (isSendBufferEmpty() && _readyState == CLOSING) 
                {
                    _socket->close();
                    setReadyState(CLOSED);
                }
            });
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

        // Masking
        for (size_t i = 0; i != (size_t) message_size; ++i)
        {
            *(_txbuf.end() - (size_t) message_size + i) ^= masking_key[i&0x3];
        }
    }

    void WebSocketTransport::appendToSendBuffer(const std::vector<uint8_t>& buffer)
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);
        _txbuf.insert(_txbuf.end(), buffer.begin(), buffer.end());
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
    void WebSocketTransport::dispatch(const OnMessageCallback& onMessageCallback)
    {
        while (true) 
        {
            wsheader_type ws;
            if (_rxbuf.size() < 2) return; /* Need at least 2 */
            const uint8_t * data = (uint8_t *) &_rxbuf[0]; // peek, but don't consume
            ws.fin = (data[0] & 0x80) == 0x80;
            ws.rsv1 = (data[0] & 0x40) == 0x40;
            ws.opcode = (wsheader_type::opcode_type) (data[0] & 0x0f);
            ws.mask = (data[1] & 0x80) == 0x80;
            ws.N0 = (data[1] & 0x7f);
            ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 8 : 0) + (ws.mask? 4 : 0);
            if (_rxbuf.size() < ws.header_size) return; /* Need: ws.header_size - _rxbuf.size() */
            
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
                _receivedData.insert(_receivedData.end(),
                                     _rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size+(size_t)ws.N);// just feed
                if (ws.fin)
                {
                    // fire callback with a string message
                    std::string stringMessage(_receivedData.begin(),
                                              _receivedData.end());

                    emitMessage(MSG, stringMessage, ws, onMessageCallback);
                    _receivedData.clear();
                }
            }
            else if (ws.opcode == wsheader_type::PING)
            {
                unmaskReceiveBuffer(ws);
                std::string pingData(_rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size + (size_t) ws.N);

                // Reply back right away
                bool compress = false;
                sendData(wsheader_type::PONG, pingData, compress);

                emitMessage(PING, pingData, ws, onMessageCallback);
            }
            else if (ws.opcode == wsheader_type::PONG)
            {
                unmaskReceiveBuffer(ws);
                std::string pongData(_rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size + (size_t) ws.N);

                emitMessage(PONG, pongData, ws, onMessageCallback);
            }
            else if (ws.opcode == wsheader_type::CLOSE)
            {
                unmaskReceiveBuffer(ws);

                // Extract the close code first, available as the first 2 bytes
                uint16_t code = 0;
                code |= ((uint64_t) _rxbuf[ws.header_size])   << 8;
                code |= ((uint64_t) _rxbuf[ws.header_size+1]) << 0;

                // Get the reason.
                std::string reason(_rxbuf.begin()+ws.header_size + 2,
                                   _rxbuf.begin()+ws.header_size + 2 + (size_t) ws.N);

                {
                    std::lock_guard<std::mutex> lock(_closeDataMutex);
                    _closeCode = code;
                    _closeReason = reason;
                    _closeWireSize = _rxbuf.size();
                }

                close();
            }
            else
            {
                close();
            }

            _rxbuf.erase(_rxbuf.begin(),
                         _rxbuf.begin() + ws.header_size + (size_t) ws.N);
        }
    }

    void WebSocketTransport::emitMessage(MessageKind messageKind, 
                                         const std::string& message,
                                         const wsheader_type& ws,
                                         const OnMessageCallback& onMessageCallback)
    {
        size_t wireSize = message.size();

        // When the RSV1 bit is 1 it means the message is compressed
        if (_enablePerMessageDeflate && ws.rsv1)
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

    WebSocketSendInfo WebSocketTransport::sendData(wsheader_type::opcode_type type, 
                                                   const std::string& message,
                                                   bool compress)
    {
        if (_readyState == CLOSING || _readyState == CLOSED)
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
            bool success = _perMessageDeflate.compress(message, compressedMessage);
            compressionError = !success;
            wireSize = compressedMessage.size();

            message_begin = compressedMessage.begin();
            message_end = compressedMessage.end();
        }

        uint64_t message_size = wireSize;

        unsigned x = getRandomUnsigned();
        uint8_t masking_key[4] = {};
        masking_key[0] = (x >> 24);
        masking_key[1] = (x >> 16) & 0xff;
        masking_key[2] = (x >> 8)  & 0xff;
        masking_key[3] = (x)       & 0xff;

        std::vector<uint8_t> header;
        header.assign(2 +
                      (message_size >= 126 ? 2 : 0) +
                      (message_size >= 65536 ? 6 : 0) + 4, 0);
        header[0] = 0x80 | type;

        // This bit indicate that the frame is compressed
        if (compress)
        {
            header[0] |= 0x40;
        }

        if (message_size < 126)
        {
            header[1] = (message_size & 0xff) | 0x80;

            header[2] = masking_key[0];
            header[3] = masking_key[1];
            header[4] = masking_key[2];
            header[5] = masking_key[3];
        }
        else if (message_size < 65536) 
        {
            header[1] = 126 | 0x80;
            header[2] = (message_size >> 8) & 0xff;
            header[3] = (message_size >> 0) & 0xff;

            header[4] = masking_key[0];
            header[5] = masking_key[1];
            header[6] = masking_key[2];
            header[7] = masking_key[3];
        }
        else
        { // TODO: run coverage testing here
            header[1] = 127 | 0x80;
            header[2] = (message_size >> 56) & 0xff;
            header[3] = (message_size >> 48) & 0xff;
            header[4] = (message_size >> 40) & 0xff;
            header[5] = (message_size >> 32) & 0xff;
            header[6] = (message_size >> 24) & 0xff;
            header[7] = (message_size >> 16) & 0xff;
            header[8] = (message_size >>  8) & 0xff;
            header[9] = (message_size >>  0) & 0xff;

            header[10] = masking_key[0];
            header[11] = masking_key[1];
            header[12] = masking_key[2];
            header[13] = masking_key[3];
        }

        // _txbuf will keep growing until it can be transmitted over the socket:
        appendToSendBuffer(header, message_begin, message_end,
                           message_size, masking_key);

        // Now actually send this data
        sendOnSocket();

        return WebSocketSendInfo(true, compressionError, payloadSize, wireSize);
    }

    WebSocketSendInfo WebSocketTransport::sendPing(const std::string& message)
    {
        bool compress = false;
        return sendData(wsheader_type::PING, message, compress);
    }

    WebSocketSendInfo WebSocketTransport::sendBinary(const std::string& message) 
    {
        return sendData(wsheader_type::BINARY_FRAME, message, _enablePerMessageDeflate);
    }

    void WebSocketTransport::sendOnSocket()
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);

        while (_txbuf.size())
        {
            int ret = _socket->send((char*)&_txbuf[0], _txbuf.size());

            if (ret < 0 && (_socket->getErrno() == EWOULDBLOCK || 
                            _socket->getErrno() == EAGAIN))
            {
                break;
            }
            else if (ret <= 0)
            {
                _socket->close();

                setReadyState(CLOSED);
                break;
            }
            else
            {
                _txbuf.erase(_txbuf.begin(), _txbuf.begin() + ret);
            }
        }
    }

    void WebSocketTransport::close()
    {
        _requestInitCancellation = true;

        if (_readyState == CLOSING || _readyState == CLOSED) return;

        // See list of close events here:
        // https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
        // We use 1000: normal closure.
        //
        // >>> struct.pack('!H', 1000)
        // b'\x03\xe8'
        //
        const std::string normalClosure = std::string("\x03\xe8");
        bool compress = false;
        sendData(wsheader_type::CLOSE, normalClosure, compress);
        setReadyState(CLOSING);

        _socket->wakeUpFromPoll();
        _socket->close();
    }

} // namespace ix
