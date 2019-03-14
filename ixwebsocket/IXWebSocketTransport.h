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
#include <list>

#include "IXWebSocketSendInfo.h"
#include "IXWebSocketPerMessageDeflate.h"
#include "IXWebSocketPerMessageDeflateOptions.h"
#include "IXWebSocketHttpHeaders.h"
#include "IXCancellationRequest.h"
#include "IXWebSocketHandshake.h"
#include "IXProgressCallback.h"

namespace ix
{
    class Socket;

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
            PONG,
            FRAGMENT
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

        void configure(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions,
                       int hearBeatPeriod);

        WebSocketInitResult connectToUrl(const std::string& url, // Client
                                         int timeoutSecs);
        WebSocketInitResult connectToSocket(int fd,              // Server
                                            int timeoutSecs);

        void poll();
        WebSocketSendInfo sendBinary(const std::string& message,
                                     const OnProgressCallback& onProgressCallback);
        WebSocketSendInfo sendPing(const std::string& message);
        void close();
        ReadyStateValues getReadyState() const;
        void setReadyState(ReadyStateValues readyStateValue);
        void setOnCloseCallback(const OnCloseCallback& onCloseCallback);
        void dispatch(const OnMessageCallback& onMessageCallback);
        size_t bufferedAmount() const;

    private:
        std::string _url;

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

        // Buffer for reading from our socket. That buffer is never resized.
        std::vector<uint8_t> _readbuf;

        // Contains all messages that were fetched in the last socket read.
        // This could be a mix of control messages (Close, Ping, etc...) and
        // data messages. That buffer
        std::vector<uint8_t> _rxbuf;

        // Contains all messages that are waiting to be sent
        std::vector<uint8_t> _txbuf;
        mutable std::mutex _txbufMutex;

        // Hold fragments for multi-fragments messages in a list. We support receiving very large
        // messages (tested messages up to 700M) and we cannot put them in a single
        // buffer that is resized, as this operation can be slow when a buffer has its
        // size increased 2 fold, while appending to a list has a fixed cost.
        std::list<std::vector<uint8_t>> _chunks;

        // Fragments are 32K long
        static constexpr size_t kChunkSize = 1 << 15;

        // Underlying TCP socket
        std::shared_ptr<Socket> _socket;

        // Hold the state of the connection (OPEN, CLOSED, etc...)
        std::atomic<ReadyStateValues> _readyState;

        OnCloseCallback _onCloseCallback;
        uint16_t _closeCode;
        std::string _closeReason;
        size_t _closeWireSize;
        mutable std::mutex _closeDataMutex;

        // Data used for Per Message Deflate compression (with zlib)
        WebSocketPerMessageDeflate _perMessageDeflate;
        WebSocketPerMessageDeflateOptions _perMessageDeflateOptions;
        std::atomic<bool> _enablePerMessageDeflate;

        // Used to cancel dns lookup + socket connect + http upgrade
        std::atomic<bool> _requestInitCancellation;

        // Optional Heartbeat
        int _heartBeatPeriod;
        static const int kDefaultHeartBeatPeriod;
        const static std::string kHeartBeatPingMessage;
        mutable std::mutex _lastSendTimePointMutex;
        std::chrono::time_point<std::chrono::steady_clock> _lastSendTimePoint;

        // No data was send through the socket for longer that the hearbeat period
        bool heartBeatPeriodExceeded();

        void sendOnSocket();
        WebSocketSendInfo sendData(wsheader_type::opcode_type type,
                                   const std::string& message,
                                   bool compress,
                                   const OnProgressCallback& onProgressCallback = nullptr);

        void sendFragment(wsheader_type::opcode_type type,
                          bool fin,
                          std::string::const_iterator begin,
                          std::string::const_iterator end,
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

        std::string getMergedChunks() const;
    };
}
