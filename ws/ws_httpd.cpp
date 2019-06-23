/*
 *  ws_httpd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <ixwebsocket/IXHttpServer.h>
#include <spdlog/spdlog.h>

namespace ix
{
    int ws_httpd_main(int port, const std::string& hostname)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::HttpServer server(port, hostname);

        auto res = server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
}
