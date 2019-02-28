/*
 *  IXHttpClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttpClient.h"
#include "IXUrlParser.h"
#include "IXWebSocketHttpHeaders.h"
#include "IXSocketFactory.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>

namespace ix
{
    HttpClient::HttpClient()
    {

    }

    HttpClient::~HttpClient()
    {

    }

    HttpResponse HttpClient::request(
        const std::string& verb,
        const std::string& body,
        HttpRequestArgs args)
    {
        int code = 0;
        WebSocketHttpHeaders headers;
        std::string payload;

        std::string protocol, host, path, query;
        int port;

        if (!parseUrl(args.url, protocol, host, path, query, port))
        {
            std::stringstream ss;
            ss << "Cannot parse url: " << args.url;
            return std::make_tuple(code, headers, payload, ss.str());
        }

        if (protocol != "http" && protocol != "https")
        {
            std::stringstream ss;
            ss << "Invalid protocol: " << protocol
               << " for url " << args.url
               << " . Supported protocols are http and https";

            return std::make_tuple(code, headers, payload, ss.str());
        }

        bool tls = protocol == "https";
        std::string errorMsg;
        _socket = createSocket(tls, errorMsg);

        if (!_socket)
        {
            return std::make_tuple(code, headers, payload, errorMsg);
        }

        // Build request string
        std::stringstream ss;
        ss << verb << " " << path << " HTTP/1.1\r\n";
        ss << "Host: " << host << "\r\n";
        ss << "User-Agent: ixwebsocket/1.0.0" << "\r\n";
        ss << "Accept: */*" << "\r\n";

        // Append extra headers
        for (auto&& it : args.extraHeaders)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }

        if (verb == "POST")
        {
            ss << "Content-Length: " << body.size() << "\r\n";

            // Set default Content-Type if unspecified
            if (args.extraHeaders.find("Content-Type") == args.extraHeaders.end())
            {
                ss << "Content-Type: application/x-www-form-urlencoded" << "\r\n";
            }
            ss << "\r\n";
            ss << body;
        }
        else
        {
            ss << "\r\n";
        }

        std::string req(ss.str());

        std::string errMsg;
        std::atomic<bool> requestInitCancellation(false);
        auto isCancellationRequested =
            makeCancellationRequestWithTimeout(args.timeoutSecs, requestInitCancellation);

        bool success = _socket->connect(host, port, errMsg, isCancellationRequested);
        if (!success)
        {
            std::stringstream ss;
            ss << "Cannot connect to url: " << args.url;
            return std::make_tuple(code, headers, payload, ss.str());
        }

        if (args.verbose)
        {
            std::cerr << "Sending " << verb << " request "
                      << "to " << host << ":" << port << std::endl
                      << "request size: " << req.size() << " bytes" << std::endl
                      << "=============" << std::endl
                      << req
                      << "=============" << std::endl
                      << std::endl;
        }

        if (!_socket->writeBytes(req, isCancellationRequested))
        {
            std::string errorMsg("Cannot send request");
            return std::make_tuple(code, headers, payload, errorMsg);
        }

        auto lineResult = _socket->readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            std::string errorMsg("Cannot retrieve status line");
            return std::make_tuple(code, headers, payload, errorMsg);
        }

        if (sscanf(line.c_str(), "HTTP/1.1 %d", &code) != 1)
        {
            std::string errorMsg("Cannot parse response code from status line");
            return std::make_tuple(code, headers, payload, errorMsg);
        }

        auto result = parseHttpHeaders(_socket, isCancellationRequested);
        auto headersValid = result.first;
        headers = result.second;

        if (!headersValid)
        {
            code = 0; // 0 ?
            std::string errorMsg("Cannot parse http headers");
            return std::make_tuple(code, headers, payload, errorMsg);
        }

        // Redirect ?
        // FIXME wrong conditional
        if ((code == 301 || code == 308) && args.followRedirects)
        {
            if (headers.find("location") == headers.end())
            {
                code = 0; // 0 ?
                std::string errorMsg("Missing location header for redirect");
                return std::make_tuple(code, headers, payload, errorMsg);
            }

            // Recurse
            std::string location = headers["location"];
            return request(verb, body, args);
        }

        if (verb == "HEAD")
        {
            return std::make_tuple(code, headers, payload, std::string());
        }

        // Parse response:
        // http://bryce-thomas.blogspot.com/2012/01/technical-parsing-http-to-extract.html
        if (headers.find("content-length") != headers.end())
        {
            ssize_t contentLength = -1;
            ss.str("");
            ss << headers["content-length"];
            ss >> contentLength;

            payload.reserve(contentLength);

            // FIXME: very inefficient way to read bytes, but it works...
            for (int i = 0; i < contentLength; ++i)
            {
                char c;
                if (!_socket->readByte(&c, isCancellationRequested))
                {
                    ss.str("");
                    ss << "Cannot read byte";
                    return std::make_tuple(-1, headers, payload, ss.str());
                }

                payload += c;
            }
        }
        else if (headers.find("transfer-encoding") != headers.end() &&
                 headers["transfer-encoding"] == "chunked")
        {
            std::stringstream ss;

            while (true)
            {
                lineResult = _socket->readLine(isCancellationRequested);
                line = lineResult.second;

                if (!lineResult.first)
                {
                    code = 0; // 0 ?
                    std::string errorMsg("Cannot read http body");
                    return std::make_tuple(code, headers, payload, errorMsg);
                }

                uint64_t chunkSize;
                ss.str("");
                ss << std::hex << line;
                ss >> chunkSize;

                if (args.verbose)
                {
                    std::cerr << "Reading " << chunkSize << " bytes"
                              << std::endl;
                }

                payload.reserve(payload.size() + chunkSize);

                // Read another line

                for (uint64_t i = 0; i < chunkSize; ++i)
                {
                    char c;
                    if (!_socket->readByte(&c, isCancellationRequested))
                    {
                        ss.str("");
                        ss << "Cannot read byte";
                        return std::make_tuple(-1, headers, payload, ss.str());
                    }

                    payload += c;
                }

                lineResult = _socket->readLine(isCancellationRequested);

                if (!lineResult.first)
                {
                    code = 0; // 0 ?
                    std::string errorMsg("Cannot read http body");
                    return std::make_tuple(code, headers, payload, errorMsg);
                }

                if (chunkSize == 0) break;
            }
        }
        else if (code == 204)
        {
            ; // 204 is NoContent response code
        }
        else
        {
            code = 0; // 0 ?
            std::string errorMsg("Cannot read http body");
            return std::make_tuple(code, headers, payload, errorMsg);
        }

        return std::make_tuple(code, headers, payload, "");
    }

    HttpResponse HttpClient::get(HttpRequestArgs args)
    {
        return request("GET", std::string(), args);
    }

    HttpResponse HttpClient::head(HttpRequestArgs args)
    {
        return request("HEAD", std::string(), args);
    }

    HttpResponse HttpClient::post(const HttpParameters& httpParameters,
                                  HttpRequestArgs args)
    {
        return request("POST", serializeHttpParameters(httpParameters), args);
    }

    HttpResponse HttpClient::post(const std::string& body,
                                  HttpRequestArgs args)
    {
        return request("POST", body, args);
    }

    std::string HttpClient::urlEncode(const std::string& value)
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (std::string::const_iterator i = value.begin(), n = value.end();
             i != n; ++i)
        {
            std::string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped << c;
                continue;
            }

            // Any other characters are percent-encoded
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }

        return escaped.str();
    }

    std::string HttpClient::serializeHttpParameters(const HttpParameters& httpParameters)
    {
        std::stringstream ss;
        size_t count = httpParameters.size();
        size_t i = 0;

        for (auto&& it : httpParameters)
        {
            ss << urlEncode(it.first)
               << "="
               << urlEncode(it.second);

            if (i++ < (count-1))
            {
               ss << "&";
            }
        }
        return ss.str();
    }
}
