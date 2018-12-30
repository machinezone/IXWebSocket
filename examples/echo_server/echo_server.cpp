/*
 *  echo_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocketServer.h>

int main(int argc, char** argv)
{
    int port = 8080;
    if (argc == 2)
    {
        std::stringstream ss;
        ss << argv[1];
        ss >> port;
    }

    ix::WebSocketServer server(port);
    auto res = server.run();
    if (!res.first)
    {
        std::cerr << res.second << std::endl;
    }

    return 0;
}
