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

struct addrinfo;

namespace ix 
{
    class Socket {
    public:
        using OnPollCallback = std::function<void()>;

        Socket();
        virtual ~Socket();

	static bool init();

        int hostname_connect(const std::string& hostname,
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

        int getErrno() const;

    protected:
        void wakeUpFromPollApple();
        void wakeUpFromPollLinux();

        void closeSocket(int fd);

        std::atomic<int> _sockfd;
        int _eventfd;
        std::mutex _socketMutex;

    private:
        bool connectToAddress(const struct addrinfo *address, 
                              int& sockfd,
                              std::string& errMsg);
    };

}
