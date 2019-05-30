/*
 *  IXConnectionState.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <memory>
#include <stdint.h>
#include <string>

namespace ix
{
    class ConnectionState
    {
    public:
        ConnectionState();
        virtual ~ConnectionState() = default;

        virtual void computeId();
        virtual const std::string& getId() const;

        void setTerminated();
        bool isTerminated() const;

        static std::shared_ptr<ConnectionState> createConnectionState();

    protected:
        std::atomic<bool> _terminated;
        std::string _id;

        static std::atomic<uint64_t> _globalId;
    };
} // namespace ix
