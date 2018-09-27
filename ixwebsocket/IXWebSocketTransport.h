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

namespace ix 
{
    class Socket;

    struct WebSocketInitResult
    {
        bool success;
        int http_status;
        std::string errorStr;

        WebSocketInitResult(bool s, int h, std::string e)
        {
            success = s;
            http_status = h;
            errorStr = e;
        }

        // need to define a default
        WebSocketInitResult()
        {
            success = false;
            http_status = 0;
            errorStr = "";
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

        using OnMessageCallback = std::function<void(const std::string&)>;
        using OnStateChangeCallback = std::function<void(ReadyStateValues)>;

        WebSocketTransport();
        ~WebSocketTransport();

        void configure(const std::string& url);
        WebSocketInitResult init();

        void poll();
        void send(const std::string& message);
        void sendBinary(const std::string& message);
        void sendBinary(const std::vector<uint8_t>& message);
        void sendPing();
        void close();
        ReadyStateValues getReadyState() const;
        void setReadyState(ReadyStateValues readyStateValue);
        void setOnStateChangeCallback(const OnStateChangeCallback& onStateChangeCallback);
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

        OnStateChangeCallback _onStateChangeCallback;

        void sendOnSocket();
        void sendData(wsheader_type::opcode_type type, 
                      uint64_t message_size, 
                      std::string::const_iterator message_begin, 
                      std::string::const_iterator message_end);

        bool isSendBufferEmpty() const;
        void appendToSendBuffer(const std::vector<uint8_t>& header,
                                std::string::const_iterator begin,
                                std::string::const_iterator end,
                                uint64_t message_size,
                                uint8_t masking_key[4]);
        void appendToSendBuffer(const std::vector<uint8_t>& buffer);

        unsigned getRandomUnsigned();
    };
}
