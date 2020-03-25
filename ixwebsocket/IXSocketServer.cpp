/*
 *  IXSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocketServer.h"

#include "IXNetSystem.h"
#include "IXSelectInterrupt.h"
#include "IXSetThreadName.h"
#include "IXSocket.h"
#include "IXSocketConnect.h"
#include "IXSocketFactory.h"
#include <assert.h>
#include <sstream>
#include <stdio.h>
#include <string.h>

namespace ix
{
    const int SocketServer::kDefaultPort(8080);
    const std::string SocketServer::kDefaultHost("127.0.0.1");
    const int SocketServer::kDefaultTcpBacklog(5);
    const size_t SocketServer::kDefaultMaxConnections(32);
    const int SocketServer::kDefaultAddressFamily(AF_INET);

    SocketServer::SocketServer(
        int port, const std::string& host, int backlog, size_t maxConnections, int addressFamily)
        : _port(port)
        , _host(host)
        , _backlog(backlog)
        , _maxConnections(maxConnections)
        , _addressFamily(addressFamily)
        , _serverFd(-1)
        , _stop(false)
        , _stopGc(false)
        , _connectionStateFactory(&ConnectionState::createConnectionState)
    {
    }

    SocketServer::~SocketServer()
    {
        stop();
    }

    void SocketServer::logError(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        fprintf(stderr, "%s\n", str.c_str());
    }

    void SocketServer::logInfo(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        fprintf(stdout, "%s\n", str.c_str());
    }

    std::pair<bool, std::string> SocketServer::listen()
    {
        if (_addressFamily != AF_INET && _addressFamily != AF_INET6)
        {
            std::string errMsg("SocketServer::listen() AF_INET and AF_INET6 are currently "
                               "the only supported address families");
            return std::make_pair(false, errMsg);
        }

        // Get a socket for accepting connections.
        if ((_serverFd = socket(_addressFamily, SOCK_STREAM, 0)) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error creating socket): " << strerror(Socket::getErrno());

            return std::make_pair(false, ss.str());
        }

        // Make that socket reusable. (allow restarting this server at will)
        int enable = 1;
        if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable)) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error calling setsockopt(SO_REUSEADDR) "
               << "at address " << _host << ":" << _port << " : " << strerror(Socket::getErrno());

            Socket::closeSocket(_serverFd);
            return std::make_pair(false, ss.str());
        }

        if (_addressFamily == AF_INET)
        {
            struct sockaddr_in server;
            server.sin_family = _addressFamily;
            server.sin_port = htons(_port);

            if (inet_pton(_addressFamily, _host.c_str(), &server.sin_addr.s_addr) <= 0)
            {
                std::stringstream ss;
                ss << "SocketServer::listen() error calling inet_pton "
                   << "at address " << _host << ":" << _port << " : "
                   << strerror(Socket::getErrno());

                Socket::closeSocket(_serverFd);
                return std::make_pair(false, ss.str());
            }

            // Bind the socket to the server address.
            if (bind(_serverFd, (struct sockaddr*) &server, sizeof(server)) < 0)
            {
                std::stringstream ss;
                ss << "SocketServer::listen() error calling bind "
                   << "at address " << _host << ":" << _port << " : "
                   << strerror(Socket::getErrno());

                Socket::closeSocket(_serverFd);
                return std::make_pair(false, ss.str());
            }
        }
        else // AF_INET6
        {
            struct sockaddr_in6 server;
            server.sin6_family = _addressFamily;
            server.sin6_port = htons(_port);

            if (inet_pton(_addressFamily, _host.c_str(), &server.sin6_addr) <= 0)
            {
                std::stringstream ss;
                ss << "SocketServer::listen() error calling inet_pton "
                   << "at address " << _host << ":" << _port << " : "
                   << strerror(Socket::getErrno());

                Socket::closeSocket(_serverFd);
                return std::make_pair(false, ss.str());
            }

            // Bind the socket to the server address.
            if (bind(_serverFd, (struct sockaddr*) &server, sizeof(server)) < 0)
            {
                std::stringstream ss;
                ss << "SocketServer::listen() error calling bind "
                   << "at address " << _host << ":" << _port << " : "
                   << strerror(Socket::getErrno());

                Socket::closeSocket(_serverFd);
                return std::make_pair(false, ss.str());
            }
        }

        //
        // Listen for connections. Specify the tcp backlog.
        //
        if (::listen(_serverFd, _backlog) < 0)
        {
            std::stringstream ss;
            ss << "SocketServer::listen() error calling listen "
               << "at address " << _host << ":" << _port << " : " << strerror(Socket::getErrno());

            Socket::closeSocket(_serverFd);
            return std::make_pair(false, ss.str());
        }

        return std::make_pair(true, "");
    }

    void SocketServer::start()
    {
        _stop = false;

        if (!_thread.joinable())
        {
            _thread = std::thread(&SocketServer::run, this);
        }

        if (!_gcThread.joinable())
        {
            _gcThread = std::thread(&SocketServer::runGC, this);
        }
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
        // Stop accepting connections, and close the 'accept' thread
        if (_thread.joinable())
        {
            _stop = true;
            _thread.join();
            _stop = false;
        }

        // Join all threads and make sure that all connections are terminated
        if (_gcThread.joinable())
        {
            _stopGc = true;
            _gcThread.join();
            _stopGc = false;
        }

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
    void SocketServer::closeTerminatedThreads()
    {
        std::lock_guard<std::mutex> lock(_connectionsThreadsMutex);
        auto it = _connectionsThreads.begin();
        auto itEnd = _connectionsThreads.end();

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
    }

    void SocketServer::run()
    {
        // Set the socket to non blocking mode, so that accept calls are not blocking
        SocketConnect::configure(_serverFd);

        setThreadName("SocketServer::listen");

        for (;;)
        {
            if (_stop) return;

            // Use poll to check whether a new connection is in progress
            int timeoutMs = 10;
            bool readyToRead = true;
            auto selectInterrupt = std::make_unique<SelectInterrupt>();
            PollResultType pollResult =
                Socket::poll(readyToRead, timeoutMs, _serverFd, selectInterrupt);

            if (pollResult == PollResultType::Error)
            {
                std::stringstream ss;
                ss << "SocketServer::run() error in select: " << strerror(Socket::getErrno());
                logError(ss.str());
                continue;
            }

            if (pollResult != PollResultType::ReadyForRead)
            {
                continue;
            }

            // Accept a connection.
            struct sockaddr_in client; // client address information
            int clientFd;              // socket connected to client
            socklen_t addressLen = sizeof(client);
            memset(&client, 0, sizeof(client));

            if ((clientFd = accept(_serverFd, (struct sockaddr*) &client, &addressLen)) < 0)
            {
                if (!Socket::isWaitNeeded())
                {
                    // FIXME: that error should be propagated
                    int err = Socket::getErrno();
                    std::stringstream ss;
                    ss << "SocketServer::run() error accepting connection: " << err << ", "
                       << strerror(err);
                    logError(ss.str());
                }
                continue;
            }

            if (getConnectedClientsCount() >= _maxConnections)
            {
                std::stringstream ss;
                ss << "SocketServer::run() reached max connections = " << _maxConnections << ". "
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

            // create socket
            std::string errorMsg;
            bool tls = _socketTLSOptions.tls;
            auto socket = createSocket(tls, clientFd, errorMsg, _socketTLSOptions);

            if (socket == nullptr)
            {
                logError("SocketServer::run() cannot create socket: " + errorMsg);
                Socket::closeSocket(clientFd);
                continue;
            }

            // Set the socket to non blocking mode + other tweaks
            SocketConnect::configure(clientFd);

            if (!socket->accept(errorMsg))
            {
                logError("SocketServer::run() tls accept failed: " + errorMsg);
                Socket::closeSocket(clientFd);
                continue;
            }

            // Launch the handleConnection work asynchronously in its own thread.
            std::lock_guard<std::mutex> lock(_connectionsThreadsMutex);
            _connectionsThreads.push_back(std::make_pair(
                connectionState,
                std::thread(
                    &SocketServer::handleConnection, this, std::move(socket), connectionState)));
        }
    }

    size_t SocketServer::getConnectionsThreadsCount()
    {
        std::lock_guard<std::mutex> lock(_connectionsThreadsMutex);
        return _connectionsThreads.size();
    }

    void SocketServer::runGC()
    {
        setThreadName("SocketServer::GC");

        for (;;)
        {
            // Garbage collection to shutdown/join threads for closed connections.
            closeTerminatedThreads();

            // We quit this thread if all connections are closed and we received
            // a stop request by setting _stopGc to true.
            if (_stopGc && getConnectionsThreadsCount() == 0)
            {
                break;
            }

            // Sleep a little bit then keep cleaning up
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void SocketServer::setTLSOptions(const SocketTLSOptions& socketTLSOptions)
    {
        _socketTLSOptions = socketTLSOptions;
    }
} // namespace ix
