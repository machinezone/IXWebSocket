/*
 * Copyright (c) 2014, Rex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the {organization} nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  IXStatsdClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

// Adapted from statsd-client-cpp
// test with netcat as a server: `nc -ul 8125`

#include "IXStatsdClient.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSetThreadName.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>

namespace ix
{
    StatsdClient::StatsdClient(const std::string& host,
                               int port,
                               const std::string& prefix,
                               bool verbose)
        : _host(host)
        , _port(port)
        , _prefix(prefix)
        , _stop(false)
        , _verbose(verbose)
    {
        _thread = std::thread([this] {
            setThreadName("Statsd");

            while (!_stop)
            {
                flushQueue();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }

    StatsdClient::~StatsdClient()
    {
        _stop = true;
        if (_thread.joinable()) _thread.join();

        _socket.close();
    }

    bool StatsdClient::init(std::string& errMsg)
    {
        return _socket.init(_host, _port, errMsg);
    }

    /* will change the original string */
    void StatsdClient::cleanup(std::string& key)
    {
        size_t pos = key.find_first_of(":|@");
        while (pos != std::string::npos)
        {
            key[pos] = '_';
            pos = key.find_first_of(":|@");
        }
    }

    int StatsdClient::dec(const std::string& key)
    {
        return count(key, -1);
    }

    int StatsdClient::inc(const std::string& key)
    {
        return count(key, 1);
    }

    int StatsdClient::count(const std::string& key, size_t value)
    {
        return send(key, value, "c");
    }

    int StatsdClient::gauge(const std::string& key, size_t value)
    {
        return send(key, value, "g");
    }

    int StatsdClient::timing(const std::string& key, size_t ms)
    {
        return send(key, ms, "ms");
    }

    int StatsdClient::send(std::string key, size_t value, const std::string& type)
    {
        cleanup(key);

        std::stringstream ss;
        ss << _prefix << "." << key << ":" << value << "|" << type;

        if (_verbose)
        {
            CoreLogger::info(ss.str());
        }

        enqueue(ss.str() + "\n");
        return 0;
    }

    void StatsdClient::enqueue(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(message);
    }

    void StatsdClient::flushQueue()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        while (!_queue.empty())
        {
            auto message = _queue.front();
            auto ret = _socket.sendto(message);
            if (ret == -1)
            {
                CoreLogger::error(std::string("statsd error: ") + strerror(UdpSocket::getErrno()));
            }

            // we always dequeue regardless of the ability to send the message
            // so that we keep our queue size under control
            _queue.pop_front();
        }
    }
} // end namespace ix
