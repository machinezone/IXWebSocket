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
                      bool redirect,
                      const std::string& redirectUrl)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::HttpServer server(port, hostname);

        if (redirect)
        {
            //
            // See https://developer.mozilla.org/en-US/docs/Web/HTTP/Redirections
            //
            server.setOnConnectionCallback(
                [redirectUrl](
                    HttpRequestPtr request,
                    std::shared_ptr<ConnectionState> /*connectionState*/) -> HttpResponsePtr {

                    WebSocketHttpHeaders headers;

                    // Log request
                    std::stringstream ss;
                    ss << request->method << " " << request->headers["User-Agent"] << " "
                       << request->uri;
                    spdlog::info(ss.str());

                    if (request->method == "POST")
                    {
                        return std::make_shared<HttpResponse>(
                            200, "OK", HttpErrorCode::Ok, headers, std::string());
                    }

                    headers["Location"] = redirectUrl;

                    return std::make_shared<HttpResponse>(
                        301, "OK", HttpErrorCode::Ok, headers, std::string());
                });
        }

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
