/*
 *  IXWebSocketHttpHeaders.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "IXCancellationRequest.h"

#include <string>
#include <map>
#include <memory>

namespace ix
{
    class Socket;

    struct CaseInsensitiveLess
    {
        // Case Insensitive compare_less binary function
        struct NocaseCompare
        {
            bool operator() (const unsigned char& c1, const unsigned char& c2) const;
        };

        bool operator() (const std::string & s1, const std::string & s2) const;
    };

    using WebSocketHttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;

    std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders(
        std::shared_ptr<Socket> socket,
        const CancellationRequest& isCancellationRequested);
}
