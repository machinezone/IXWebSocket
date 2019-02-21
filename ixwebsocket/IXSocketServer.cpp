/*
 *  IXSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketServer.h"
#include "IXSocket.h"
#include "IXSocketConnect.h"
#include "IXNetSystem.h"

#include <iostream>
#include <sstream>
#include <future>
#include <string.h>

namespace ix
{
    const int SocketServer::kDefaultPort(8080);
    const std::string SocketServer::kDefaultHost("127.0.0.1");
    const int SocketServer::kDefaultTcpBacklog(5);
    const size_t SocketServer::kDefaultMaxConnections(32);

    SocketServer::SocketServer(int port,
                               const std::string& host,
                               int backlog,
                               size_t maxConnections) :
        _port(port),
        _host(host),
        _backlog(backlog),
        _maxConnections(maxConnections),
        _stop(false)
    {

    }

    SocketServer::~SocketServer()
    {
        stop();
    }

    void SocketServer::logError(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        std::cerr << str << std::endl;
    }

    void SocketServer::logInfo(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        std::cout << str << std::endl;
    }

    std::pair<bool, std::string> SocketServer::listen()
    {
        struct sockaddr_in server; // server address information

        // Get a socket for accepting connections.
        if ((_serverFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error creating socket): "
               << strerror(Socket::getErrno());

            return std::make_pair(false, ss.str());
        }

        // Make that socket reusable. (allow restarting this server at will)
        int enable = 1;
        if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR,
                       (char*) &enable, sizeof(enable)) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error calling setsockopt(SO_REUSEADDR) "
               << "at address " << _host << ":" << _port
               << " : " << strerror(Socket::getErrno());

            ::close(_serverFd);
            return std::make_pair(false, ss.str());
        }

        // Bind the socket to the server address.
        server.sin_family = AF_INET;
        server.sin_port   = htons(_port);

        // Using INADDR_ANY trigger a pop-up box as binding to any address is detected
        // by the osx firewall. We need to codesign the binary with a self-signed cert
        // to allow that, but this is a bit of a pain. (this is what node or python would do).
        //
        // Using INADDR_LOOPBACK also does not work ... while it should.
        // We default to 127.0.0.1 (localhost)
        //
        server.sin_addr.s_addr = inet_addr(_host.c_str());

        if (bind(_serverFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error calling bind "
               << "at address " << _host << ":" << _port
               << " : " << strerror(Socket::getErrno());

            ::close(_serverFd);
            return std::make_pair(false, ss.str());
        }

        //
        // Listen for connections. Specify the tcp backlog.
        //
        if (::listen(_serverFd, _backlog) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error calling listen "
               << "at address " << _host << ":" << _port
               << " : " << strerror(Socket::getErrno());

            ::close(_serverFd);
            return std::make_pair(false, ss.str());
        }

        return std::make_pair(true, "");
    }

    void SocketServer::start()
    {
        if (_thread.joinable()) return; // we've already been started

        _thread = std::thread(&SocketServer::run, this);
    }

    void SocketServer::wait()
    {
        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _conditionVariable.wait(lock);
    }

    void SocketServer::stop()
    {
        if (!_thread.joinable()) return; // nothing to do

        _stop = true;
        _thread.join();
        _stop = false;

        _conditionVariable.notify_one();
        ::close(_serverFd);
    }

    void SocketServer::run()
    {
        // Set the socket to non blocking mode, so that accept calls are not blocking
        SocketConnect::configure(_serverFd);

        // Return value of std::async, ignored
        std::future<void> f;

        for (;;)
        {
            if (_stop) return;

            // Use select to check whether a new connection is in progress
            fd_set rfds;
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 10 * 1000; // 10ms timeout

            FD_ZERO(&rfds);
            FD_SET(_serverFd, &rfds);

            if (select(_serverFd + 1, &rfds, nullptr, nullptr, &timeout) < 0 &&
                (errno == EBADF || errno == EINVAL))
            {
                std::stringstream ss;
                ss << "SocketServer::run() error in select: "
                   << strerror(Socket::getErrno());
                logError(ss.str());
                continue;
            }

            if (!FD_ISSET(_serverFd, &rfds))
            {
                // We reached the select timeout, and no new connections are pending
                continue;
            }

            // Accept a connection.
            struct sockaddr_in client; // client address information
            int clientFd;              // socket connected to client
            socklen_t addressLen = sizeof(socklen_t);
            memset(&client, 0, sizeof(client));

            if ((clientFd = accept(_serverFd, (struct sockaddr *)&client, &addressLen)) < 0)
            {
                if (Socket::getErrno() != EWOULDBLOCK)
                {
                    // FIXME: that error should be propagated
                    std::stringstream ss;
                    ss << "SocketServer::run() error accepting connection: "
                       << strerror(Socket::getErrno());
                    logError(ss.str());
                }
                continue;
            }

            if (getConnectedClientsCount() >= _maxConnections)
            {
                std::stringstream ss;
                ss << "SocketServer::run() reached max connections = "
                   << _maxConnections << ". "
                   << "Not accepting connection";
                logError(ss.str());

                ::close(clientFd);

                continue;
            }

            // Launch the handleConnection work asynchronously in its own thread.
            //
            // the destructor of a future returned by std::async blocks,
            // so we need to declare it outside of this loop
            f = std::async(std::launch::async,
                           &SocketServer::handleConnection,
                           this,
                           clientFd);
        }
    }
}

