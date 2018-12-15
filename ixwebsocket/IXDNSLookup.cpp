/*
 *  IXDNSLookup.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXDNSLookup.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <chrono>

namespace ix 
{
    // 60s timeout, see IXSocketConnect.cpp
    const int64_t DNSLookup::kDefaultTimeout = 60 * 1000; // ms
    const int64_t DNSLookup::kDefaultWait = 10;           // ms

    DNSLookup::DNSLookup(const std::string& hostname, int port, int wait) :
        _hostname(hostname),
        _port(port),
        _res(nullptr),
        _done(false),
        _wait(wait)
    {
        
    }

    DNSLookup::~DNSLookup()
    {
        ;
    }

    struct addrinfo* DNSLookup::getAddrInfo(const std::string& hostname, 
                                            int port,
                                            std::string& errMsg)
    {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string sport = std::to_string(port);

        struct addrinfo* res;
        int getaddrinfo_result = getaddrinfo(hostname.c_str(), sport.c_str(), 
                                             &hints, &res);
        if (getaddrinfo_result)
        {
            errMsg = gai_strerror(getaddrinfo_result);
            res = nullptr;
        }
        return res;
    }

    struct addrinfo* DNSLookup::resolve(std::string& errMsg,
                                        const CancellationRequest& isCancellationRequested,
                                        bool blocking)
    {
        return blocking ? resolveBlocking(errMsg, isCancellationRequested)
                        : resolveAsync(errMsg, isCancellationRequested);
    }

    struct addrinfo* DNSLookup::resolveBlocking(std::string& errMsg,
                                                const CancellationRequest& isCancellationRequested)
    {
        errMsg = "no error";

        // Maybe a cancellation request got in before the background thread terminated ?
        if (isCancellationRequested())
        {
            errMsg = "cancellation requested";
            return nullptr;
        }

        return getAddrInfo(_hostname, _port, errMsg);
    }

    struct addrinfo* DNSLookup::resolveAsync(std::string& errMsg,
                                             const CancellationRequest& isCancellationRequested)
    {
        errMsg = "no error";

        // Can only be called once, otherwise we would have to manage a pool
        // of background thread which is overkill for our usage.
        if (_done)
        {
            return nullptr; // programming error, create a second DNSLookup instance
                            // if you need a second lookup.
        }

        // 
        // Good resource on thread forced termination
        // https://www.bo-yang.net/2017/11/19/cpp-kill-detached-thread
        //
        _thread = std::thread(&DNSLookup::run, this);
        _thread.detach();

        int64_t timeout = kDefaultTimeout;
        std::unique_lock<std::mutex> lock(_mutex);

        while (!_done)
        {
            // Wait for 10 milliseconds on the condition variable, to see
            // if the bg thread has terminated.
            if (_condition.wait_for(lock, std::chrono::milliseconds(_wait)) == std::cv_status::no_timeout)
            {
                // Background thread has terminated, so we can break of this loop
                break;
            }

            // Were we cancelled ?
            if (isCancellationRequested())
            {
                errMsg = "cancellation requested";
                return nullptr;
            }

            // Have we exceeded the timeout ?
            timeout -= _wait;
            if (timeout <= 0)
            {
                errMsg = "dns lookup timed out after 60 seconds";
                return nullptr;
            }
        }

        // Maybe a cancellation request got in before the bg terminated ?
        if (isCancellationRequested())
        {
            errMsg = "cancellation requested";
            return nullptr;
        }

        return _res;
    }

    void DNSLookup::run()
    {
        _res = getAddrInfo(_hostname, _port, _errMsg);
        _condition.notify_one();
        _done = true;
    }

}
