/*
 *  IXSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>
#include <unistd.h> // pipe

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "IXCancellationRequest.h"
#include "IXProgressCallback.h"
#include "IXEventFd.h"

namespace ix
{
    enum PollResultType
    {
        PollResultType_ReadyForRead = 0,
        PollResultType_Timeout = 1,
        PollResultType_Error = 2,
        PollResultType_SendRequest = 3,
        PollResultType_CloseRequest = 4
    };

    class Socket {
    public:
        using OnPollCallback = std::function<void(PollResultType)>;

        Socket(int fd = -1);
        virtual ~Socket();

        void configure();

        PollResultType select(int timeoutSecs, int timeoutMs);
        virtual void poll(const OnPollCallback& onPollCallback,
                          int timeoutSecs = kDefaultPollTimeout);
        virtual bool wakeUpFromPoll(uint8_t wakeUpCode);

        // Virtual methods
        virtual bool connect(const std::string& url,
                             int port,
                             std::string& errMsg,
                             const CancellationRequest& isCancellationRequested);
        virtual void close();

        virtual ssize_t send(char* buffer, size_t length);
        virtual ssize_t send(const std::string& buffer);
        virtual ssize_t recv(void* buffer, size_t length);

        // Blocking and cancellable versions, working with socket that can be set
        // to non blocking mode. Used during HTTP upgrade.
        bool readByte(void* buffer,
                      const CancellationRequest& isCancellationRequested);
        bool writeBytes(const std::string& str,
                        const CancellationRequest& isCancellationRequested);

        std::pair<bool, std::string> readLine(
            const CancellationRequest& isCancellationRequested);
        std::pair<bool, std::string> readBytes(
            size_t length,
            const OnProgressCallback& onProgressCallback,
            const CancellationRequest& isCancellationRequested);

        static int getErrno();
        static bool init(); // Required on Windows to initialize WinSocket
        static void cleanup(); // Required on Windows to cleanup WinSocket

        // Used as special codes for pipe communication
        static const uint8_t kSendRequest;
        static const uint8_t kCloseRequest;

    protected:
        void closeSocket(int fd);

        std::atomic<int> _sockfd;
        std::mutex _socketMutex;

    private:
        static const int kDefaultPollTimeout;
        static const int kDefaultPollNoTimeout;

        // Buffer for reading from our socket. That buffer is never resized.
        std::vector<uint8_t> _readBuffer;
        static constexpr size_t kChunkSize = 1 << 15;

        EventFd _eventfd;
    };
}
