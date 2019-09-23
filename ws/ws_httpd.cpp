/*
 *  ws_httpd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <fstream>
#include <iostream>
#include <ixwebsocket/IXHttpServer.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>

namespace ix
{
    int ws_httpd_main(int port,
                      const std::string& hostname,
                      const std::string& certFile,
                      const std::string& keyFile,
                      const std::string& caFile)
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
} // namespace ix
