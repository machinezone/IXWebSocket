/*
 *  IXWebSocketTransport.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

//
// Adapted from https://github.com/dhbaird/easywsclient
//

#include "IXWebSocketTransport.h"

#include "IXSocket.h"
#ifdef __APPLE__
# include "IXSocketAppleSSL.h"
#else
# include "IXSocketOpenSSL.h"
#endif

#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <cstdlib>
#include <vector>
#include <string>
#include <cstdarg>
#include <iostream>
#include <sstream>
#include <regex>


namespace ix {

    WebSocketTransport::WebSocketTransport() :
        _readyState(CLOSED)
    {
        ;
    }

    WebSocketTransport::~WebSocketTransport()
    {
        ;
    }

    void WebSocketTransport::configure(const std::string& url)
    {
        _url = url;
    }

    bool WebSocketTransport::parseUrl(const std::string& url,
                                      std::string& protocol,
                                      std::string& host,
                                      std::string& path,
                                      std::string& query,
                                      int& port)
    {
        std::regex ex("(ws|wss)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
        std::cmatch what;
        if (!regex_match(url.c_str(), what, ex))
        {
            return false;
        }

        std::string portStr;

        protocol = std::string(what[1].first, what[1].second);
        host     = std::string(what[2].first, what[2].second);
        portStr  = std::string(what[3].first, what[3].second);
        path     = std::string(what[4].first, what[4].second);
        query    = std::string(what[5].first, what[5].second);

        if (portStr.empty())
        {
            if (protocol == "ws")
            {
                port = 80;
            }
            else if (protocol == "wss")
            {
                port = 443;
            }
        }
        else
        {
            std::stringstream ss;
            ss << portStr;
            ss >> port;
        }

        if (path.empty())
        {
            path = "/";
        }
        else if (path[0] != '/')
        {
            path = '/' + path;
        }

        if (!query.empty())
        {
            path += "?";
            path += query;
        }

        return true;
    }

    void WebSocketTransport::printUrl(const std::string& url)
    {
        std::string protocol, host, path, query;
        int port {0};

        if (!WebSocketTransport::parseUrl(url, protocol, host,
                                          path, query, port))
        {
            return;
        }

        std::cout << "[" << url << "]" << std::endl;
        std::cout << protocol << std::endl;
        std::cout << host << std::endl;
        std::cout << port << std::endl;
        std::cout << path << std::endl;
        std::cout << query << std::endl;
        std::cout << "-------------------------------" << std::endl;
    }

    WebSocketInitResult WebSocketTransport::init()
    {
        std::string protocol, host, path, query;
        int port;

        if (!WebSocketTransport::parseUrl(_url, protocol, host,
                                          path, query, port))
        {
            return WebSocketInitResult(false, 0, "Could not parse URL");
        }

        if (protocol == "wss")
        {
            _socket.reset();
#ifdef __APPLE__
            _socket = std::make_shared<SocketAppleSSL>();
#else
            _socket = std::make_shared<SocketOpenSSL>();
#endif
        }
        else
        {
            _socket.reset();
            _socket = std::make_shared<Socket>();
        }

        std::string errMsg;
        bool success = _socket->connect(host, port, errMsg);
        if (!success)
        {
            std::stringstream ss;
            ss << "Unable to connect to " << host
               << " on port " << port
               << ", error: " << errMsg;
            return WebSocketInitResult(false, 0, ss.str());
        }

        char line[256];
        int status;
        int i;
        snprintf(line, 256,
            "GET %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n",
            path.c_str(), host.c_str(), port);

        // XXX: this should be done non-blocking,

        size_t lineSize = strlen(line);
        if (_socket->send(line, lineSize) != lineSize)
        {
            return WebSocketInitResult(false, 0, std::string("Failed sending GET request to ") + _url);

        }

        for (i = 0; i < 2 || (i < 255 && line[i-2] != '\r' && line[i-1] != '\n'); ++i)
        {
            if (_socket->recv(line+i, 1) == 0)
            {
                return WebSocketInitResult(false, 0, std::string("Failed reading HTTP status line from ") + _url);
            } 
        }
        line[i] = 0;
        if (i == 255)
        {
            return WebSocketInitResult(false, 0, std::string("Got bad status line connecting to ") + _url);
        }

        // HTTP/1.0 is too old.
        if (sscanf(line, "HTTP/1.0 %d", &status) == 1)
        {
            std::stringstream ss;
            ss << "Server version is HTTP/1.0. Rejecting connection to " << host
               << ", status: " << status
               << ", HTTP Status line: " << line;
            return WebSocketInitResult(false, status, ss.str());
        }

        // We want an 101 HTTP status
        if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101)
        {
            std::stringstream ss;
            ss << "Got bad status connecting to " << host
               << ", status: " << status
               << ", HTTP Status line: " << line;
            return WebSocketInitResult(false, status, ss.str());
        }

        // TODO: verify response headers,
        while (true) 
        {
            for (i = 0;
                 i < 2 || (i < 255 && line[i-2] != '\r' && line[i-1] != '\n');
                 ++i)
            {
                if (_socket->recv(line+i, 1) == 0)
                {
                    return WebSocketInitResult(false, status, std::string("Failed reading response header from ") + _url);
                }
            }
            if (line[0] == '\r' && line[1] == '\n')
            {
                break;
            }
        }

        _socket->configure();
        setReadyState(OPEN);

        return WebSocketInitResult(true, status, "");
    }

    WebSocketTransport::ReadyStateValues WebSocketTransport::getReadyState() const 
    {
        return _readyState;
    }

    void WebSocketTransport::setReadyState(ReadyStateValues readyStateValue)
    {
        _readyState = readyStateValue;
        _onStateChangeCallback(readyStateValue);
    }

    void WebSocketTransport::setOnStateChangeCallback(const OnStateChangeCallback& onStateChangeCallback)
    {
        _onStateChangeCallback = onStateChangeCallback; 
    }

    void WebSocketTransport::poll()
    {
        _socket->poll(
            [this]()
            {
                while (true) 
                {
                    int N = (int) _rxbuf.size();

                    ssize_t ret;
                    _rxbuf.resize(N + 1500);
                    ret = _socket->recv((char*)&_rxbuf[0] + N, 1500);

                    if (ret < 0 && (errno == EWOULDBLOCK || 
                                    errno == EAGAIN)) {
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
        // TODO: consider acquiring a lock on _rxbuf...
        while (true) {
            wsheader_type ws;
            if (_rxbuf.size() < 2) return; /* Need at least 2 */
            const uint8_t * data = (uint8_t *) &_rxbuf[0]; // peek, but don't consume
            ws.fin = (data[0] & 0x80) == 0x80;
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
                if (ws.mask)
                {
                    for (size_t j = 0; j != ws.N; ++j)
                    {
                        _rxbuf[j+ws.header_size] ^= ws.masking_key[j&0x3];
                    }
                }
                _receivedData.insert(_receivedData.end(),
                                     _rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size+(size_t)ws.N);// just feed
                if (ws.fin)
                {
                    // fire callback with a string message
                    std::string stringMessage(_receivedData.begin(),
                                              _receivedData.end());
                    onMessageCallback(stringMessage);

                    _receivedData.clear();
                }
            }
            else if (ws.opcode == wsheader_type::PING)
            {
                if (ws.mask)
                {
                    for (size_t j = 0; j != ws.N; ++j)
                    {
                        _rxbuf[j+ws.header_size] ^= ws.masking_key[j&0x3];
                    }
                }

                std::string pingData(_rxbuf.begin()+ws.header_size,
                                     _rxbuf.begin()+ws.header_size + (size_t) ws.N);

                sendData(wsheader_type::PONG, pingData.size(),
                         pingData.begin(), pingData.end());
            }
            else if (ws.opcode == wsheader_type::PONG) { }
            else if (ws.opcode == wsheader_type::CLOSE) { close(); }
            else { close(); }

            _rxbuf.erase(_rxbuf.begin(),
                         _rxbuf.begin() + ws.header_size + (size_t) ws.N);
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

    void WebSocketTransport::sendData(wsheader_type::opcode_type type, 
                                      uint64_t message_size, 
                                      std::string::const_iterator message_begin, 
                                      std::string::const_iterator message_end) 
    {
        if (_readyState == CLOSING || _readyState == CLOSED)
        {
            return;
        }

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
    }

    void WebSocketTransport::sendPing()
    {
        std::string empty;
        sendData(wsheader_type::PING, empty.size(), empty.begin(), empty.end());
    }

    void WebSocketTransport::sendBinary(const std::string& message) 
    {
        sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
    }

    void WebSocketTransport::sendOnSocket()
    {
        std::lock_guard<std::mutex> lock(_txbufMutex);

        while (_txbuf.size())
        {
            int ret = _socket->send((char*)&_txbuf[0], _txbuf.size());

            if (ret < 0 && (errno == EWOULDBLOCK || 
                            errno == EAGAIN))
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
        if (_readyState == CLOSING || _readyState == CLOSED) return;

        setReadyState(CLOSING);
        uint8_t closeFrame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00}; // last 4 bytes are a masking key
        std::vector<uint8_t> header(closeFrame, closeFrame+6);
        appendToSendBuffer(header);

        sendOnSocket();

        _socket->wakeUpFromPoll();
    }

} // namespace ix
