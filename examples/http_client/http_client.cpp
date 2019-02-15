/*
 *  http_client.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXHttpClient.h>

using namespace ix;

void run(const std::string& url)
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


int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ws_connect <url>" << std::endl;
        return 1;
    }
    std::string url = argv[1];

    Socket::init();
    run(url);
    return 0;
}
