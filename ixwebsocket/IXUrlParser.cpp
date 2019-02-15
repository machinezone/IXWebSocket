/*
 *  IXUrlParser.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXUrlParser.h"

#include <regex>
#include <iostream>
#include <sstream>


namespace ix 
{
    bool parseUrl(const std::string& url,
                  std::string& protocol,
                  std::string& host,
                  std::string& path,
                  std::string& query,
                  int& port)
    {
        std::regex ex("(ws|wss|http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
        std::cmatch what;
        if (!regex_match(url.c_str(), what, ex))
        {
            return false;
        }

        std::string portStr;

        protocol = std::string(what[1].first, what[1].second);
        host     = std::string(what[2].first, what[2].second);
        portStr  = std::string(what[3].first, what[3].second);
        path     = std::string(what[4].first, what[4].second);
        query    = std::string(what[5].first, what[5].second);

        if (portStr.empty())
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
        else
        {
            std::stringstream ss;
            ss << portStr;
            ss >> port;
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

    void printUrl(const std::string& url)
    {
        std::string protocol, host, path, query;
        int port {0};

        if (!parseUrl(url, protocol, host, path, query, port))
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
