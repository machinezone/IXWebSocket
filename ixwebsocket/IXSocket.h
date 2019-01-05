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

#include "IXEventFd.h"
#include "IXCancellationRequest.h"

namespace ix 
{
    class Socket {
    public:
        using OnPollCallback = std::function<void()>;

        Socket(int fd = -1);
        virtual ~Socket();

        void configure();

        virtual void poll(const OnPollCallback& onPollCallback);
        virtual void wakeUpFromPoll();

        // Virtual methods
        virtual bool connect(const std::string& url, 
                             int port,
                             std::string& errMsg,
                             const CancellationRequest& isCancellationRequested);
        virtual void close();

        virtual int send(char* buffer, size_t length);
        virtual int send(const std::string& buffer);
        virtual int recv(void* buffer, size_t length);

        // Blocking and cancellable versions, working with socket that can be set
        // to non blocking mode. Used during HTTP upgrade.
        bool readByte(void* buffer,
                      const CancellationRequest& isCancellationRequested);
        bool writeBytes(const std::string& str,
                        const CancellationRequest& isCancellationRequested);
        std::pair<bool, std::string> readLine(const CancellationRequest& isCancellationRequested);

        static int getErrno();
        static bool init(); // Required on Windows to initialize WinSocket
        static void cleanup(); // Required on Windows to cleanup WinSocket

    protected:
        void closeSocket(int fd);

        std::atomic<int> _sockfd;
        std::mutex _socketMutex;
        EventFd _eventfd;
    };
}
