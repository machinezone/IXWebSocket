/*
 *  IXConnectionState.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>

namespace ix
{
    using OnSetTerminatedCallback = std::function<void()>;

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

    private:
        void setOnSetTerminatedCallback(const OnSetTerminatedCallback& callback);

    protected:
        std::atomic<bool> _terminated;
        std::string _id;
        OnSetTerminatedCallback _onSetTerminatedCallback;

        static std::atomic<uint64_t> _globalId;

        friend class SocketServer;
    };
} // namespace ix
