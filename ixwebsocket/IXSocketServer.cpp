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
#include <assert.h>

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
        _stop(false),
        _connectionStateFactory(&ConnectionState::createConnectionState)
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

            Socket::closeSocket(_serverFd);
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

            Socket::closeSocket(_serverFd);
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

            Socket::closeSocket(_serverFd);
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

    void SocketServer::stopAcceptingConnections()
    {
        _stop = true;
    }

    void SocketServer::stop()
    {
        while (true)
        {
            if (closeTerminatedThreads()) break;

            // wait 10ms and try again later.
            // we could have a timeout, but if we exit of here
            // we leaked threads, it is quite bad.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!_thread.joinable()) return; // nothing to do

        _stop = true;
        _thread.join();
        _stop = false;

        _conditionVariable.notify_one();
        Socket::closeSocket(_serverFd);
    }

    void SocketServer::setConnectionStateFactory(
        const ConnectionStateFactory& connectionStateFactory)
    {
        _connectionStateFactory = connectionStateFactory;
    }

    //
    // join the threads for connections that have been closed
    //
    // When a connection is closed by a client, the connection state terminated
    // field becomes true, and we can use that to know that we can join that thread
    // and remove it from our _connectionsThreads data structure (a list).
    //
    bool SocketServer::closeTerminatedThreads()
    {
        std::lock_guard<std::mutex> lock(_connectionsThreadsMutex);
        auto it = _connectionsThreads.begin();
        auto itEnd  = _connectionsThreads.end();

        while (it != itEnd)
        {
            auto& connectionState = it->first;
            auto& thread = it->second;

            if (!connectionState->isTerminated())
            {
                ++it;
                continue;
            }

            if (thread.joinable()) thread.join();
            it = _connectionsThreads.erase(it);
        }

        return _connectionsThreads.empty();
    }

    void SocketServer::run()
    {
        // Set the socket to non blocking mode, so that accept calls are not blocking
        SocketConnect::configure(_serverFd);

        for (;;)
        {
            if (_stop) return;

            // Garbage collection to shutdown/join threads for closed connections.
            // We could run this in its own thread, so that we dont need to accept
            // a new connection to close a thread.
            // We could also use a condition variable to be notify when we need to do this
            closeTerminatedThreads();

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
            socklen_t addressLen = sizeof(client);
            memset(&client, 0, sizeof(client));

            if ((clientFd = accept(_serverFd, (struct sockaddr *)&client, &addressLen)) < 0)
            {
                if (!Socket::isWaitNeeded())
                {
                    // FIXME: that error should be propagated
                    int err = Socket::getErrno();
                    std::stringstream ss;
                    ss << "SocketServer::run() error accepting connection: "
                       << err << ", " << strerror(err);
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

                Socket::closeSocket(clientFd);

                continue;
            }

            std::shared_ptr<ConnectionState> connectionState;
            if (_connectionStateFactory)
            {
                connectionState = _connectionStateFactory();
            }

            if (_stop) return;

            // Launch the handleConnection work asynchronously in its own thread.
            std::lock_guard<std::mutex> lock(_connectionsThreadsMutex);
            _connectionsThreads.push_back(std::make_pair(
                    connectionState,
                    std::thread(&SocketServer::handleConnection,
                                this,
                                clientFd,
                                connectionState)));
        }
    }
}

