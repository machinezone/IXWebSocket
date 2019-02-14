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
    httpClient.get(url);
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
