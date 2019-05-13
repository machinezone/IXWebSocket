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

    enum class SendMessageKind
    {
        Text,
        Binary,
        Ping
    };

    class WebSocketTransport
    {
    public:
        enum class ReadyState
        {
            CLOSING,
            CLOSED,
            CONNECTING,
            OPEN
        };

        enum class MessageKind
        {
            MSG,
            PING,
            PONG,
            FRAGMENT
        };

        enum class PollResult
        {
            Succeeded,
            AbnormalClose
        };

        using OnMessageCallback = std::function<void(const std::string&,
                                                     size_t,
                                                     bool,
                                                     MessageKind)>;
        using OnCloseCallback = std::function<void(uint16_t,
                                                   const std::string&,
                                                   size_t,
                                                   bool)>;

        WebSocketTransport();
        ~WebSocketTransport();

        void configure(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions,
                       bool enablePong,
                       int pingIntervalSecs,
                       int pingTimeoutSecs);

        WebSocketInitResult connectToUrl(const std::string& url, // Client
                                         int timeoutSecs);
        WebSocketInitResult connectToSocket(int fd,              // Server
                                            int timeoutSecs);

        PollResult poll();
        WebSocketSendInfo sendBinary(const std::string& message,
                                     const OnProgressCallback& onProgressCallback);
        WebSocketSendInfo sendText(const std::string& message,
                                   const OnProgressCallback& onProgressCallback);
        WebSocketSendInfo sendPing(const std::string& message);

        void close(uint16_t code = 1000,
                   const std::string& reason = "Normal closure",
                   size_t closeWireSize = 0,
                   bool remote = false);

        ReadyState getReadyState() const;
        void setReadyState(ReadyState readyState);
        void setOnCloseCallback(const OnCloseCallback& onCloseCallback);
        void dispatch(PollResult pollResult,
                      const OnMessageCallback& onMessageCallback);
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
                TEXT_FRAME   = 0x1,
                BINARY_FRAME = 0x2,
                CLOSE        = 8,
                PING         = 9,
                PONG         = 0xa,
            } opcode;
            int N0;
            uint64_t N;
            uint8_t masking_key[4];
        };

        // Tells whether we should mask the data we send.
        // client should mask but server should not
        std::atomic<bool> _useMask;

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
        std::atomic<ReadyState> _readyState;

        OnCloseCallback _onCloseCallback;
        uint16_t _closeCode;
        std::string _closeReason;
        size_t _closeWireSize;
        bool _closeRemote;
        mutable std::mutex _closeDataMutex;

        // Data used for Per Message Deflate compression (with zlib)
        WebSocketPerMessageDeflate _perMessageDeflate;
        WebSocketPerMessageDeflateOptions _perMessageDeflateOptions;
        std::atomic<bool> _enablePerMessageDeflate;

        // Used to cancel dns lookup + socket connect + http upgrade
        std::atomic<bool> _requestInitCancellation;
              
        mutable std::mutex _closingTimePointMutex;
        std::chrono::time_point<std::chrono::steady_clock>_closingTimePoint;
        static const int kClosingMaximumWaitingDelayInMs;

        // Constants for dealing with closing conneections
        static const uint16_t kInternalErrorCode;
        static const uint16_t kAbnormalCloseCode;
        static const uint16_t kProtocolErrorCode;
        static const uint16_t kNoStatusCodeErrorCode;
        static const std::string kInternalErrorMessage;
        static const std::string kAbnormalCloseMessage;
        static const std::string kPingTimeoutMessage;
        static const std::string kProtocolErrorMessage;
        static const std::string kNoStatusCodeErrorMessage;

        // enable auto response to ping
        std::atomic<bool> _enablePong;
        static const bool kDefaultEnablePong;

        // Optional ping and pong timeout
        // if both ping interval and timeout are set (> 0),
        // then use GCD of these value to wait for the lowest time
        int _pingIntervalSecs;
        int _pingTimeoutSecs;
        int _pingIntervalOrTimeoutGCDSecs;

        static const int kDefaultPingIntervalSecs;
        static const int kDefaultPingTimeoutSecs;
        static const std::string kPingMessage;

        // Record time step for ping/ ping timeout to ensure we wait for the right left duration
        std::chrono::time_point<std::chrono::steady_clock> _nextGCDTimePoint;

        // We record when ping are being sent so that we can know when to send the next one
        // We also record when pong are being sent as a reply to pings, to close the connections
        // if no pong were received sufficiently fast.
        mutable std::mutex _lastSendPingTimePointMutex;
        mutable std::mutex _lastReceivePongTimePointMutex;
        std::chrono::time_point<std::chrono::steady_clock> _lastSendPingTimePoint;
        std::chrono::time_point<std::chrono::steady_clock> _lastReceivePongTimePoint;

        // If this function returns true, it is time to send a new ping
        bool pingIntervalExceeded();

        // No PONG data was received through the socket for longer than ping timeout delay
        bool pingTimeoutExceeded();

        // after calling close(), if no CLOSE frame answer is received back from the remote, we should close the connexion
        bool closingDelayExceeded();

        void initTimePointsAndGCDAfterConnect();

        void sendCloseFrame(uint16_t code, const std::string& reason);

        void closeSocketAndSwitchToClosedState(uint16_t code,
                                               const std::string& reason,
                                               size_t closeWireSize,
                                               bool remote);

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

        unsigned getRandomUnsigned();
        void unmaskReceiveBuffer(const wsheader_type& ws);

        std::string getMergedChunks() const;
    };
}
