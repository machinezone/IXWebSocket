/*
 *  IXSocket.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>
#include <functional>
#include <mutex>

namespace ix 
{
    class Socket {
    public:
        using OnPollCallback = std::function<void()>;

        Socket();
        virtual ~Socket();

        static int hostname_connect(const std::string& hostname,
                                    int port,
                                    std::string& errMsg);
        void configure();

        virtual void poll(const OnPollCallback& onPollCallback);
        virtual void wakeUpFromPoll();

        // Virtual methods
        virtual bool connect(const std::string& url, 
                             int port,
                             std::string& errMsg);
        virtual void close();

        virtual int send(char* buffer, size_t length);
        virtual int send(const std::string& buffer);
        virtual int recv(void* buffer, size_t length);

    protected:
        void wakeUpFromPollApple();
        void wakeUpFromPollLinux();

        std::atomic<int> _sockfd;
        int _eventfd;
        std::mutex _socketMutex;
    };

}
