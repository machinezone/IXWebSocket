/*
 *  IXHttp.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttp.h"

#include <sstream>
#include <vector>

namespace ix
{
    std::string Http::trim(const std::string& str)
    {
        std::string out(str);
        out.erase(std::remove(out.begin(), out.end(), ' '), out.end());
        out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
        out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
        return out;
    }

    std::tuple<std::string, std::string, std::string> Http::parseRequestLine(const std::string& line)
    {
        // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
        std::string token;
        std::stringstream tokenStream(line);
        std::vector<std::string> tokens;

        // Split by ' '
        while (std::getline(tokenStream, token, ' '))
        {
            tokens.push_back(token);
        }

        std::string method;
        if (tokens.size() >= 1)
        {
            method = trim(tokens[0]);
        }

        std::string requestUri;
        if (tokens.size() >= 2)
        {
            requestUri = trim(tokens[1]);
        }

        std::string httpVersion;
        if (tokens.size() >= 3)
        {
            httpVersion = trim(tokens[2]);
        }

        return std::make_tuple(method, requestUri, httpVersion);
    }
}
