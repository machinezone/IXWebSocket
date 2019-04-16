/*
 *  IXWebSocketHttpHeaders.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketHttpHeaders.h"
#include "IXSocket.h"
#include <algorithm>
#include <locale>

namespace ix
{
    bool CaseInsensitiveLess::NocaseCompare::operator()(const unsigned char & c1, const unsigned char & c2) const
    {
#ifdef _WIN32
        return std::tolower(c1, std::locale()) < std::tolower(c2, std::locale());
#else
        return std::tolower(c1) < std::tolower(c2);
#endif
    }

    bool CaseInsensitiveLess::operator()(const std::string & s1, const std::string & s2) const
    {
        return std::lexicographical_compare
                (s1.begin(), s1.end(),   // source range
                 s2.begin(), s2.end(),   // dest range
                 NocaseCompare());       // comparison
    }

    std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders(
        std::shared_ptr<Socket> socket,
        const CancellationRequest& isCancellationRequested)
    {
        WebSocketHttpHeaders headers;

        char line[1024];
        int i;

        while (true)
        {
            int colon = 0;

            for (i = 0;
                 i < 2 || (i < 1023 && line[i-2] != '\r' && line[i-1] != '\n');
                 ++i)
            {
                if (!socket->readByte(line+i, isCancellationRequested))
                {
                    return std::make_pair(false, headers);
                }

                if (line[i] == ':' && colon == 0)
                {
                    colon = i;
                }
            }
            if (line[0] == '\r' && line[1] == '\n')
            {
                break;
            }

            // line is a single header entry. split by ':', and add it to our
            // header map. ignore lines with no colon.
            if (colon > 0)
            {
                line[i] = '\0';
                std::string lineStr(line);
                // colon is ':', colon+1 is ' ', colon+2 is the start of the value.
                // i is end of string (\0), i-colon is length of string minus key;
                // subtract 1 for '\0', 1 for '\n', 1 for '\r',
                // 1 for the ' ' after the ':', and total is -4
                std::string name(lineStr.substr(0, colon));
                std::string value(lineStr.substr(colon + 2, i - colon - 4));

                headers[name] = value;
            }
        }

        return std::make_pair(true, headers);
    }
}
