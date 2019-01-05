/*
 *  IXWebSocketServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketServer.h"
#include "IXWebSocketTransport.h"
#include "IXWebSocket.h"
#include "IXSocketConnect.h"

#include <sstream>
#include <future>

#ifdef _WIN32
# include <basetsd.h>
# include <WinSock2.h>
# include <ws2def.h>
# include <WS2tcpip.h>
# include <io.h>
#else
# include <unistd.h>
# include <netdb.h>
# include <stdio.h>
# include <arpa/inet.h>
# include <sys/socket.h>
#endif

#include <string.h>

namespace ix 
{
    const int WebSocketServer::kDefaultPort(8080);
    const std::string WebSocketServer::kDefaultHost("127.0.0.1");
    const int WebSocketServer::kDefaultTcpBacklog(5);
    const size_t WebSocketServer::kDefaultMaxConnections(32);
    const int WebSocketServer::kDefaultHandShakeTimeoutSecs(3); // 3 seconds

    WebSocketServer::WebSocketServer(int port,
                                     const std::string& host,
                                     int backlog,
                                     size_t maxConnections,
                                     int handshakeTimeoutSecs) :
        _port(port),
        _host(host),
        _backlog(backlog),
        _maxConnections(maxConnections),
        _handshakeTimeoutSecs(handshakeTimeoutSecs),
        _stop(false)
    {

    }

    WebSocketServer::~WebSocketServer()
    {
        stop();
    }

    void WebSocketServer::setOnConnectionCallback(const OnConnectionCallback& callback)
    {
        _onConnectionCallback = callback;
    }

    void WebSocketServer::logError(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        std::cerr << str << std::endl;
    }

    void WebSocketServer::logInfo(const std::string& str)
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        std::cout << str << std::endl;
    }

    std::pair<bool, std::string> WebSocketServer::listen()
    {
        struct sockaddr_in server; // server address information

        // Get a socket for accepting connections.
        if ((_serverFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error creating socket): "
               << strerror(Socket::getErrno());

            return std::make_pair(false, ss.str());
        }

        // Make that socket reusable. (allow restarting this server at will)
        int enable = 1;
        if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR,
                       (char*) &enable, sizeof(enable)) < 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error calling setsockopt(SO_REUSEADDR): "
               << strerror(errno);

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
            ss << "WebSocketServer::listen() error calling bind: "
               << strerror(Socket::getErrno());

            return std::make_pair(false, ss.str());
        }

        /*
         * Listen for connections. Specify the tcp backlog.
         */
        if (::listen(_serverFd, _backlog) != 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error calling listen: "
               << strerror(Socket::getErrno());

            return std::make_pair(false, ss.str());
        }

        return std::make_pair(true, "");
    }

    void WebSocketServer::start()
    {
        if (_thread.joinable()) return; // we've already been started

        _thread = std::thread(&WebSocketServer::run, this);
    }

    void WebSocketServer::wait()
    {
        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _conditionVariable.wait(lock);
    }

    void WebSocketServer::stop()
    {
        if (!_thread.joinable()) return; // nothing to do

        auto clients = getClients();
        for (auto client : clients)
        {
            client->close();
        }

        _stop = true;
        _thread.join();
        _stop = false;

        _conditionVariable.notify_one();
    }

    void WebSocketServer::run()
    {
        // Set the socket to non blocking mode, so that accept calls are not blocking
        SocketConnect::configure(_serverFd);

        // Return value of std::async, ignored
        std::future<void> f;

        // Select arguments
        fd_set rfds;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10 * 1000; // 10ms

        for (;;)
        {
            if (_stop) return;

            FD_ZERO(&rfds);
            FD_SET(_serverFd, &rfds);
            select(_serverFd + 1, &rfds, nullptr, nullptr, &timeout);

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
                    ss << "WebSocketServer::run() error accepting connection: "
                       << strerror(Socket::getErrno());
                    logError(ss.str());
                }
                continue;
            }

            if (getConnectedClientsCount() >= _maxConnections)
            {
                std::stringstream ss;
                ss << "WebSocketServer::run() reached max connections = "
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
                           &WebSocketServer::handleConnection,
                           this,
                           clientFd);
        }
    }

    void WebSocketServer::handleConnection(int fd)
    {
        std::shared_ptr<WebSocket> webSocket(new WebSocket);
        _onConnectionCallback(webSocket);

        webSocket->disableAutomaticReconnection();

        // Add this client to our client set
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.insert(webSocket);
        }

        auto status = webSocket->connectToSocket(fd, _handshakeTimeoutSecs);
        if (status.success)
        {
            // Process incoming messages and execute callbacks 
            // until the connection is closed
            webSocket->run();
        }
        else
        {
            std::stringstream ss;
            ss << "WebSocketServer::handleConnection() error: "
               << status.http_status
               << " error: "
               << status.errorStr;
            logError(ss.str());
        }

        // Remove this client from our client set
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            if (_clients.erase(webSocket) != 1)
            {
                logError("Cannot delete client");
            }
        }

        logInfo("WebSocketServer::handleConnection() done");
    }

    std::set<std::shared_ptr<WebSocket>> WebSocketServer::getClients()
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        return _clients;
    }

    size_t WebSocketServer::getConnectedClientsCount()
    {
        return getClients().size();
    }
}
