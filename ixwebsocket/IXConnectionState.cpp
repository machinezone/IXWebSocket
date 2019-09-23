/*
 *  IXConnectionState.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXConnectionState.h"

namespace ix
{
    std::atomic<uint64_t> ConnectionState::_globalId(0);

    ConnectionState::ConnectionState()
        : _terminated(false)
    {
        computeId();
    }

    void ConnectionState::computeId()
    {
        _id = std::to_string(_globalId++);
    }

    const std::string& ConnectionState::getId() const
    {
        return _id;
    }

    std::shared_ptr<ConnectionState> ConnectionState::createConnectionState()
    {
        return std::make_shared<ConnectionState>();
    }

    bool ConnectionState::isTerminated() const
    {
        return _terminated;
    }

    void ConnectionState::setTerminated()
    {
        _terminated = true;
    }
} // namespace ix
