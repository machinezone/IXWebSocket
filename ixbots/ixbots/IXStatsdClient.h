/*
 *  IXStatsdClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <deque>
#include <ixwebsocket/IXUdpSocket.h>
#include <mutex>
#include <string>
#include <thread>

namespace ix
{
    class StatsdClient
    {
    public:
        StatsdClient(const std::string& host = "127.0.0.1",
                     int port = 8125,
                     const std::string& prefix = "",
                     bool verbose = false);
        ~StatsdClient();

        bool init(std::string& errMsg);
        int inc(const std::string& key);
        int dec(const std::string& key);
        int count(const std::string& key, size_t value);
        int gauge(const std::string& key, size_t value);
        int timing(const std::string& key, size_t ms);

    private:
        void enqueue(const std::string& message);

        /* (Low Level Api) manually send a message
         * type = "c", "g" or "ms"
         */
        int send(std::string key, size_t value, const std::string& type);

        void cleanup(std::string& key);
        void flushQueue();

        UdpSocket _socket;

        std::string _host;
        int _port;
        std::string _prefix;

        std::atomic<bool> _stop;
        std::thread _thread;
        std::mutex _mutex; // for the queue

        std::deque<std::string> _queue;
        bool _verbose;
    };

} // end namespace ix
