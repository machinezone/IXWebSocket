/*
 *  IXWebSocketTransport.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

//
// Adapted from https://github.com/dhbaird/easywsclient
//

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

#include "IXWebSocketSendInfo.h"
#include "IXWebSocketPerMessageDeflate.h"
#include "IXWebSocketPerMessageDeflateOptions.h"
#include "IXWebSocketHttpHeaders.h"

namespace ix 
{
    class Socket;

    struct WebSocketInitResult
    {
        bool success;
        int http_status;
        std::string errorStr;
        WebSocketHttpHeaders headers;

        WebSocketInitResult(bool s = false,
                            int status = 0,
                            const std::string& e = std::string(),
                            WebSocketHttpHeaders h = WebSocketHttpHeaders())
        {
            success = s;
            http_status = status;
            errorStr = e;
            headers = h;
        }
    };

    class WebSocketTransport
    {
    public:
        enum ReadyStateValues
        {
            CLOSING,
            CLOSED,
            CONNECTING,
            OPEN
        };

        enum MessageKind
        {
            MSG,
            PING,
            PONG
        };

        using OnMessageCallback = std::function<void(const std::string&,
                                                     size_t,
                                                     bool,
                                                     MessageKind)>;
        using OnCloseCallback = std::function<void(uint16_t,
                                                   const std::string&,
                                                   size_t)>;

        WebSocketTransport();
        ~WebSocketTransport();

        // Client
        void configure(const std::string& url,
                       const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions);
        WebSocketInitResult init();

        // Server
        WebSocketInitResult initFromSocket(int fd);

        void poll();
        WebSocketSendInfo sendBinary(const std::string& message);
        WebSocketSendInfo sendPing(const std::string& message);
        void close();
        ReadyStateValues getReadyState() const;
        void setReadyState(ReadyStateValues readyStateValue);
        void setOnCloseCallback(const OnCloseCallback& onCloseCallback);
        void dispatch(const OnMessageCallback& onMessageCallback);

        static void printUrl(const std::string& url);
        static bool parseUrl(const std::string& url,
                             std::string& protocol,
                             std::string& host,
                             std::string& path,
                             std::string& query,
                             int& port);

    private:
        std::string _url;
        std::string _origin;

        struct wsheader_type {
            unsigned header_size;
            bool fin;
            bool rsv1;
            bool mask;
            enum opcode_type {
                CONTINUATION = 0x0,
                TEXT_FRAME = 0x1,
                BINARY_FRAME = 0x2,
                CLOSE = 8,
                PING = 9,
                PONG = 0xa,
            } opcode;
            int N0;
            uint64_t N;
            uint8_t masking_key[4];
        };

        std::vector<uint8_t> _rxbuf;
        std::vector<uint8_t> _txbuf;
        mutable std::mutex _txbufMutex;
        std::vector<uint8_t> _receivedData;

        std::shared_ptr<Socket> _socket;

        std::atomic<ReadyStateValues> _readyState;

        OnCloseCallback _onCloseCallback;
        uint16_t _closeCode;
        std::string _closeReason;
        size_t _closeWireSize;
        mutable std::mutex _closeDataMutex;

        WebSocketPerMessageDeflate _perMessageDeflate;
        WebSocketPerMessageDeflateOptions _perMessageDeflateOptions;
        std::atomic<bool> _enablePerMessageDeflate;

        // Used to cancel dns lookup + socket connect + http upgrade
        std::atomic<bool> _requestInitCancellation;

        void sendOnSocket();
        WebSocketSendInfo sendData(wsheader_type::opcode_type type, 
                                   const std::string& message,
                                   bool compress);

        void emitMessage(MessageKind messageKind, 
                         const std::string& message,
                         const wsheader_type& ws,
                         const OnMessageCallback& onMessageCallback);

        bool isSendBufferEmpty() const;
        void appendToSendBuffer(const std::vector<uint8_t>& header,
                                std::string::const_iterator begin,
                                std::string::const_iterator end,
                                uint64_t message_size,
                                uint8_t masking_key[4]);
        void appendToSendBuffer(const std::vector<uint8_t>& buffer);

        unsigned getRandomUnsigned();
        void unmaskReceiveBuffer(const wsheader_type& ws);
        std::string genRandomString(const int len);

        // Non blocking versions of read/write, used during http upgrade
        bool readByte(void* buffer);
        bool writeBytes(const std::string& str);

        // Parse HTTP headers
        std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders();
    };
}
