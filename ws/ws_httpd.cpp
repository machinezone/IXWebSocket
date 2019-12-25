/*
 *  ws_httpd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <fstream>
#include <ixwebsocket/IXHttpServer.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>

namespace ix
{
    int ws_httpd_main(int port,
                      const std::string& hostname,
                      bool redirect,
                      const std::string& redirectUrl,
                      const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::HttpServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        if (redirect)
        {
            server.makeRedirectServer(redirectUrl);
        }

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::error(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix
