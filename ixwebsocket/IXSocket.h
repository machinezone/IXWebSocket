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
#include <memory>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "IXCancellationRequest.h"
#include "IXProgressCallback.h"

namespace ix
{
    class SelectInterrupt;

    enum PollResultType
    {
        PollResultType_ReadyForRead = 0,
        PollResultType_ReadyForWrite = 1,
        PollResultType_Timeout = 2,
        PollResultType_Error = 3,
        PollResultType_SendRequest = 4,
        PollResultType_CloseRequest = 5
    };

    class Socket {
    public:
        using OnPollCallback = std::function<void(PollResultType)>;

        Socket(int fd = -1);
        virtual ~Socket();
        bool init(std::string& errorMsg);

        void configure();

        // Functions to check whether there is activity on the socket
        void poll(const OnPollCallback& onPollCallback,
                  int timeoutSecs = kDefaultPollTimeout);
        bool wakeUpFromPoll(uint8_t wakeUpCode);

        PollResultType isReadyToWrite(int timeoutMs);
        PollResultType isReadyToRead(int timeoutMs);

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

        // Used as special codes for pipe communication
        static const uint64_t kSendRequest;
        static const uint64_t kCloseRequest;

    protected:
        void closeSocket(int fd);

        std::atomic<int> _sockfd;
        std::mutex _socketMutex;

    private:
        PollResultType select(bool readyToRead, int timeoutMs);

        static const int kDefaultPollTimeout;
        static const int kDefaultPollNoTimeout;

        // Buffer for reading from our socket. That buffer is never resized.
        std::vector<uint8_t> _readBuffer;
        static constexpr size_t kChunkSize = 1 << 15;

        std::shared_ptr<SelectInterrupt> _selectInterrupt;
    };
}
