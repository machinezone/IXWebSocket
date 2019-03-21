/*
 *  IXConnectionState.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXConnectionState.h"

#include <sstream>

namespace ix
{
    std::atomic<uint64_t> ConnectionState::_globalId(0);

    ConnectionState::ConnectionState()
    {
        computeId();
    }

    void ConnectionState::computeId()
    {
        std::stringstream ss;
        ss << _globalId++;
        _id = ss.str();
    }

    const std::string& ConnectionState::getId() const
    {
        return _id;
    }

    std::shared_ptr<ConnectionState> ConnectionState::createConnectionState()
    {
        return std::make_shared<ConnectionState>();
    }
}

