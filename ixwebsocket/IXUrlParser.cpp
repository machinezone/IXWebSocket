/*
 *  IXUrlParser.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXUrlParser.h"
#include "LUrlParser.h"

#include <iostream>

namespace ix
{
    bool UrlParser::parse(const std::string& url,
                          std::string& protocol,
                          std::string& host,
                          std::string& path,
                          std::string& query,
                          int& port)
    {
        LUrlParser::clParseURL res = LUrlParser::clParseURL::ParseURL(url);

        if (!res.IsValid())
        {
            return false;
        }

        protocol = res.m_Scheme;
        host     = res.m_Host;
        path     = res.m_Path;
        query    = res.m_Query;

        if (!res.GetPort(&port))
        {
            if (protocol == "ws" || protocol == "http")
            {
                port = 80;
            }
            else if (protocol == "wss" || protocol == "https")
            {
                port = 443;
            }
            else
            {
                // Invalid protocol. Should be caught by regex check
                // but this missing branch trigger cpplint linter.
                return false;
            }
        }

        if (path.empty())
        {
            path = "/";
        }
        else if (path[0] != '/')
        {
            path = '/' + path;
        }

        if (!query.empty())
        {
            path += "?";
            path += query;
        }

        return true;
    }

    void UrlParser::printUrl(const std::string& url)
    {
        std::string protocol, host, path, query;
        int port {0};

        if (!parse(url, protocol, host, path, query, port))
        {
            return;
        }

        std::cout << "[" << url << "]" << std::endl;
        std::cout << protocol << std::endl;
        std::cout << host << std::endl;
        std::cout << port << std::endl;
        std::cout << path << std::endl;
        std::cout << query << std::endl;
        std::cout << "-------------------------------" << std::endl;
    }
}
