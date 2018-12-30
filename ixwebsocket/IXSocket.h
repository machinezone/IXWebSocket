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

struct addrinfo;

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

        int getErrno() const;
        static bool init(); // Required on Windows to initialize WinSocket
        static void cleanup(); // Required on Windows to cleanup WinSocket

    protected:
        void closeSocket(int fd);

        std::atomic<int> _sockfd;
        std::mutex _socketMutex;
        EventFd _eventfd;

    private:
    };

}
