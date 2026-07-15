/*
 *  IXSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "IXCancellationRequest.h"
#include "IXNetSystem.h"
#include "IXProgressCallback.h"
#include "IXSelectInterrupt.h"

namespace ix
{
    enum class PollResultType
    {
        ReadyForRead = 0,
        ReadyForWrite = 1,
        Timeout = 2,
        Error = 3,
        SendRequest = 4,
        CloseRequest = 5
    };

    class Socket
    {
    public:
        Socket(int fd = -1);
        virtual ~Socket();
        bool init(std::string& errorMsg);

        // Functions to check whether there is activity on the socket
        PollResultType poll(int timeoutMs = kDefaultPollTimeout);
        bool wakeUpFromPoll(uint64_t wakeUpCode);
        bool isWakeUpFromPollSupported();

        PollResultType isReadyToWrite(int timeoutMs);
        PollResultType isReadyToRead(int timeoutMs);

        // Virtual methods
        virtual bool accept(std::string& errMsg);

        virtual bool connect(const std::string& host,
                             int port,
                             std::string& errMsg,
                             const CancellationRequest& isCancellationRequested);
        virtual void close();

        virtual std::ptrdiff_t send(char* buffer, size_t length);
        std::ptrdiff_t send(const std::string& buffer);
        virtual std::ptrdiff_t recv(void* buffer, size_t length);

        // Blocking and cancellable versions, working with socket that can be set
        // to non blocking mode. Used during HTTP upgrade.
        bool readByte(void* buffer, const CancellationRequest& isCancellationRequested);
        bool writeBytes(const std::string& str, const CancellationRequest& isCancellationRequested);

        std::pair<bool, std::string> readLine(const CancellationRequest& isCancellationRequested);
        std::pair<bool, std::string> readBytes(size_t length,
                                               const OnProgressCallback& onProgressCallback,
                                               const OnChunkCallback& onChunkCallback,
                                               const CancellationRequest& isCancellationRequested);

        static int getErrno();
        static void setErrno(int err);
        static bool isWaitNeeded();
        static void closeSocket(socket_t fd);

        static PollResultType poll(bool readyToRead,
                                   int timeoutMs,
                                   socket_t sockfd,
                                   const SelectInterruptPtr& selectInterrupt);

    protected:
        std::atomic<int> _sockfd;
        std::mutex _socketMutex;

        static bool readSelectInterruptRequest(const SelectInterruptPtr& selectInterrupt,
                                               PollResultType* pollResult);

    private:
        static const int kDefaultPollTimeout;
        static const int kDefaultPollNoTimeout;

        SelectInterruptPtr _selectInterrupt;
    };
} // namespace ix
