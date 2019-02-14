/*
 *  IXHttpClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttpClient.h"

#include <iostream>

namespace ix 
{
    HttpClient::HttpClient()
    {

    }

    HttpClient::~HttpClient()
    {

    }

    HttpResponse HttpClient::get(const std::string& url)
    {
        int code = 0;
        WebSocketHttpHeaders headers;
        std::string payload;

        // FIXME: missing url parsing

        std::string host("www.cnn.com");
        int port = 80;
        std::string request("GET / HTTP/1.1\r\n\r\n");
        int expectedStatus = 200;
        int timeoutSecs = 3;

        std::string errMsg;
        static std::atomic<bool> requestInitCancellation(false);
        auto isCancellationRequested =
            makeCancellationRequestWithTimeout(timeoutSecs, requestInitCancellation);

        bool success = _socket.connect(host, port, errMsg, isCancellationRequested);
        if (!success)
        {
            int code = 0; // 0 ?
            return std::make_tuple(code, headers, payload);
        }

        std::cout << "Sending request: " << request
                  << "to " << host << ":" << port
                  << std::endl;
        if (!_socket.writeBytes(request, isCancellationRequested))
        {
            int code = 0; // 0 ?
            return std::make_tuple(code, headers, payload);
        }

        auto lineResult = _socket.readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        std::cout << "first line: " << line << std::endl;

        std::cout << "read error: " << strerror(Socket::getErrno()) << std::endl;

        int status = -1;
        sscanf(line.c_str(), "HTTP/1.1 %d", &status) == 1;

        return std::make_tuple(code, headers, payload);
    }

    HttpResponse HttpClient::post(const std::string& url)
    {
        int code = 0;
        WebSocketHttpHeaders headers;
        std::string payload;

        return std::make_tuple(code, headers, payload);
    }
}
