/*
 *  IXDNSLookup.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 *
 *  Resolve a hostname+port to a struct addrinfo obtained with getaddrinfo
 *  Does this in a background thread so that it can be cancelled, since
 *  getaddrinfo is a blocking call, and we don't want to block the main thread on Mobile.
 */

#pragma once

#include "IXCancellationRequest.h"
#include <atomic>
#include <condition_variable>
#include <set>
#include <string>
#include <thread>
#include <memory>

struct addrinfo;

namespace ix
{
    class DNSLookup : public std::enable_shared_from_this<DNSLookup>
    {
    public:
        DNSLookup(const std::string& hostname, int port, int64_t wait = DNSLookup::kDefaultWait);
        ~DNSLookup() = default;

        struct addrinfo* resolve(std::string& errMsg,
                                 const CancellationRequest& isCancellationRequested,
                                 bool blocking = false);

    private:
        struct addrinfo* resolveAsync(std::string& errMsg,
                                      const CancellationRequest& isCancellationRequested);
        struct addrinfo* resolveBlocking(std::string& errMsg,
                                         const CancellationRequest& isCancellationRequested);

        static struct addrinfo* getAddrInfo(const std::string& hostname,
                                            int port,
                                            std::string& errMsg);

        void run(std::weak_ptr<DNSLookup> self, const std::string& hostname, int port); // thread runner

        void setHostname(const std::string& hostname);
        const std::string& getHostname();

        void setErrMsg(const std::string& errMsg);
        const std::string& getErrMsg();

        void setRes(struct addrinfo* addr);
        struct addrinfo* getRes();

        std::string _hostname;
        std::mutex _hostnameMutex;
        int _port;

        int64_t _wait;

        struct addrinfo* _res;
        std::mutex _resMutex;

        std::string _errMsg;
        std::mutex _errMsgMutex;

        std::atomic<bool> _done;
        std::thread _thread;
        std::condition_variable _condition;
        std::mutex _conditionVariableMutex;

        const static int64_t kDefaultWait;
    };
} // namespace ix
