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
#include <algorithm>
#include <locale>

namespace ix
{
    class Socket;

    struct CaseInsensitiveLess
    {
        // Case Insensitive compare_less binary function
        struct NocaseCompare
        {
            bool operator() (const unsigned char& c1, const unsigned char& c2) const
            {
                return std::tolower(c1, std::locale()) < std::tolower(c2, std::locale());
            }
        };

        bool operator() (const std::string & s1, const std::string & s2) const
        {
            return std::lexicographical_compare
                (s1.begin(), s1.end(),   // source range
                 s2.begin(), s2.end(),   // dest range
                 NocaseCompare());  // comparison
        }
    };

    using WebSocketHttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;

    std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders(
        std::shared_ptr<Socket> socket,
        const CancellationRequest& isCancellationRequested);
}
