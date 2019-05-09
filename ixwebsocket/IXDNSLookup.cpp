/*
 *  IXDNSLookup.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXDNSLookup.h"
#include "IXNetSystem.h"

#include <string.h>
#include <chrono>

namespace ix
{
    const int64_t DNSLookup::kDefaultWait = 10; // ms

    std::atomic<uint64_t> DNSLookup::_nextId(0);
    std::set<uint64_t> DNSLookup::_activeJobs;
    std::mutex DNSLookup::_activeJobsMutex;

    DNSLookup::DNSLookup(const std::string& hostname, int port, int64_t wait) :
        _port(port),
        _wait(wait),
        _res(nullptr),
        _done(false),
        _id(_nextId++)
    {
        setHostname(hostname);
    }

    DNSLookup::~DNSLookup()
    {
        // Remove this job from the active jobs list
        std::lock_guard<std::mutex> lock(_activeJobsMutex);
        _activeJobs.erase(_id);
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
        if (isCancellationRequested && isCancellationRequested())
        {
            errMsg = "cancellation requested";
            return nullptr;
        }

        return getAddrInfo(getHostname(), _port, errMsg);
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

        // Record job in the active Job set
        {
            std::lock_guard<std::mutex> lock(_activeJobsMutex);
            _activeJobs.insert(_id);
        }

        //
        // Good resource on thread forced termination
        // https://www.bo-yang.net/2017/11/19/cpp-kill-detached-thread
        //
        _thread = std::thread(&DNSLookup::run, this, _id, getHostname(), _port);
        _thread.detach();

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);

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
            if (isCancellationRequested && isCancellationRequested())
            {
                errMsg = "cancellation requested";
                return nullptr;
            }
        }

        // Maybe a cancellation request got in before the bg terminated ?
        if (isCancellationRequested && isCancellationRequested())
        {
            errMsg = "cancellation requested";
            return nullptr;
        }

        errMsg = getErrMsg();
        return getRes();
    }

    void DNSLookup::run(uint64_t id, const std::string& hostname, int port) // thread runner
    {
        // We don't want to read or write into members variables of an object that could be
        // gone, so we use temporary variables (res) or we pass in by copy everything that
        // getAddrInfo needs to work.
        std::string errMsg;
        struct addrinfo* res = getAddrInfo(hostname, port, errMsg);

        // if this isn't an active job, and the control thread is gone
        // there is nothing to do, and we don't want to touch the defunct
        // object data structure such as _errMsg or _condition
        std::lock_guard<std::mutex> lock(_activeJobsMutex);
        if (_activeJobs.count(id) == 0)
        {
            return;
        }

        // Copy result into the member variables
        setRes(res);
        setErrMsg(errMsg);

        _condition.notify_one();
        _done = true;
    }

    void DNSLookup::setHostname(const std::string& hostname)
    {
        std::lock_guard<std::mutex> lock(_hostnameMutex);
        _hostname = hostname;
    }

    const std::string& DNSLookup::getHostname()
    {
        std::lock_guard<std::mutex> lock(_hostnameMutex);
        return _hostname;
    }

    void DNSLookup::setErrMsg(const std::string& errMsg)
    {
        std::lock_guard<std::mutex> lock(_errMsgMutex);
        _errMsg = errMsg;
    }

    const std::string& DNSLookup::getErrMsg()
    {
        std::lock_guard<std::mutex> lock(_errMsgMutex);
        return _errMsg;
    }

    void DNSLookup::setRes(struct addrinfo* addr)
    {
        std::lock_guard<std::mutex> lock(_resMutex);
        _res = addr;
    }

    struct addrinfo* DNSLookup::getRes()
    {
        std::lock_guard<std::mutex> lock(_resMutex);
        return _res;
    }
}
