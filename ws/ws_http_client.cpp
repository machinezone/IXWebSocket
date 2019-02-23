/*
 *  http_client.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXHttpClient.h>

namespace ix
{
    void ws_http_client_main(const std::string& url)
    {
        HttpClient httpClient;
        bool verbose = true;
        auto out = httpClient.get(url, verbose);
        auto errorCode = std::get<0>(out);
        auto headers = std::get<1>(out);
        auto payload = std::get<2>(out);
        auto errorMsg = std::get<3>(out);

        for (auto it : headers)
        {
            std::cout << it.first << ": " << it.second << std::endl;
        }

        std::cout << "error code: " << errorCode << std::endl;
        if (!errorMsg.empty())
        {
            std::cout << "error message: " << errorMsg << std::endl;
        }

        std::cout << "payload: " << payload << std::endl;
    }
}
