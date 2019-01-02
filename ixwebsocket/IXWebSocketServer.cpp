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

#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

namespace ix 
{
    const std::string WebSocketServer::kDefaultHost("127.0.0.1");

    WebSocketServer::WebSocketServer(int port, const std::string& host, int backlog) :
        _port(port),
        _host(host),
        _backlog(backlog),
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
               << strerror(errno);

            return std::make_pair(false, ss.str());
        }

        // Make that socket reusable. (allow restarting this server at will)
        int enable = 1;
        if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
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
               << strerror(errno);

            return std::make_pair(false, ss.str());
        }

        /*
         * Listen for connections. Specify the tcp backlog.
         */
        if (::listen(_serverFd, _backlog) != 0)
        {
            std::stringstream ss;
            ss << "WebSocketServer::listen() error calling listen: "
               << strerror(errno);

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

    // FIXME: we should cancel all the async per connections tasks
    void WebSocketServer::stop()
    {
        if (!_thread.joinable()) return; // nothing to do

        _stop = true;
        _thread.join();
        _stop = false;

        _conditionVariable.notify_one();
    }

    void WebSocketServer::run()
    {
        // Set the socket to non blocking mode, so that accept calls are not blocking
        SocketConnect::configure(_serverFd);

        std::future<void> f;

        for (;;)
        {
            if (_stop) return;

            // Accept a connection.
            struct sockaddr_in client; // client address information
            int clientFd;              // socket connected to client
            socklen_t addressLen = sizeof(socklen_t);

            if ((clientFd = accept(_serverFd, (struct sockaddr *)&client, &addressLen)) < 0)
            {
                if (errno == EWOULDBLOCK)
                {
                    std::chrono::duration<double, std::milli> wait(10);
                    std::this_thread::sleep_for(wait);
                }
                else
                {
                    // FIXME: that error should be propagated
                    std::stringstream ss;
                    ss << "WebSocketServer::run() error accepting connection: "
                       << strerror(errno);
                    logError(ss.str());
                }
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

    //
    // FIXME: make sure we never run into reconnectPerpetuallyIfDisconnected
    //
    void WebSocketServer::handleConnection(int fd)
    {
        std::shared_ptr<WebSocket> webSocket(new WebSocket);
        _onConnectionCallback(webSocket);

        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.insert(webSocket);
        }

        webSocket->start();
        auto status = webSocket->connectToSocket(fd);
        if (!status.success)
        {
            std::stringstream ss;
            ss << "WebSocketServer::handleConnection() error: "
               << status.errorStr;
            logError(ss.str());
            return;
        }

        // We can do better than this busy loop, with a condition variable.
        while (webSocket->isConnected())
        {
            std::chrono::duration<double, std::milli> wait(10);
            std::this_thread::sleep_for(wait);
        }

        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.erase(webSocket);
        }

        logInfo("WebSocketServer::handleConnection() done");
    }

    std::set<std::shared_ptr<WebSocket>> WebSocketServer::getClients()
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        return _clients;
    }
}
