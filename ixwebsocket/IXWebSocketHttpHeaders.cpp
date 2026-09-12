/*
 *  IXWebSocketHttpHeaders.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketHttpHeaders.h"

#include "IXSocket.h"
#include <algorithm>

namespace ix
{
    std::pair<bool, WebSocketHttpHeaders> parseHttpHeaders(
        std::unique_ptr<Socket>& socket, const CancellationRequest& isCancellationRequested)
    {
        WebSocketHttpHeaders headers;

        char line[1024];

        while (true)
        {
            std::size_t i{0};
            int colon = -1;
            for(i=0;i<1023;++i)
            {
                if (!socket->readByte(line + i, isCancellationRequested))
                {
                    return std::make_pair(false, headers);
                }
                if (line[i] == ':' && colon == -1)
                {
                    colon = static_cast<int>(i);
                }
                if(line[i] == '\n')
                {
                    ++i;
                    break;
                }
            }
            if((i == 2 && line[0] == '\r' && line[1] == '\n') || (i == 1 && line[0] == '\n'))
            {
                break;
            }
            // strip all \n \r 
            std::size_t end = i;
            auto rbegin = std::reverse_iterator<char*>(line + end);
            auto rend   = std::reverse_iterator<char*>(line);
            auto it = std::find_if(rbegin,rend,[](const unsigned char c){return c != '\r' && c != '\n'; });
            end = static_cast<std::size_t>(it.base() - line);
            line[end] = '\0';

            // line is a single header entry. split by ':', and add it to our
            // header map. ignore lines with no colon.
            if (colon > 0)
            {
                std::string lineStr(line);

                int start = colon + 1;
                while (start < (int) lineStr.size() && lineStr[start] == ' ')
                {
                    start++;
                }

                std::string name(lineStr.substr(0, colon));
                std::string value;
                if (start < (int) lineStr.size())
                {
                    value = lineStr.substr(start);
                    // trim trailing whitespace (\r, \n, spaces)
                    value.erase(std::find_if(value.rbegin(),
                                             value.rend(),
                                             [](unsigned char c) { return !std::isspace(c); })
                                    .base(),
                                value.end());
                }

                headers[name] = value;
            }
        }

        return std::make_pair(true, headers);
    }
} // namespace ix
