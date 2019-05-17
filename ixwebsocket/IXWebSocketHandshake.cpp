/*
 *  IXWebSocketHandshake.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketHandshake.h"
#include "IXSocketConnect.h"
#include "IXUrlParser.h"

#include "libwshandshake.hpp"

#include <sstream>
#include <random>
#include <algorithm>


namespace ix
{
    WebSocketHandshake::WebSocketHandshake(std::atomic<bool>& requestInitCancellation,
                                           std::shared_ptr<Socket> socket,
                                           WebSocketPerMessageDeflate& perMessageDeflate,
                                           WebSocketPerMessageDeflateOptions& perMessageDeflateOptions,
                                           std::atomic<bool>& enablePerMessageDeflate) :
        _requestInitCancellation(requestInitCancellation),
        _socket(socket),
        _perMessageDeflate(perMessageDeflate),
        _perMessageDeflateOptions(perMessageDeflateOptions),
        _enablePerMessageDeflate(enablePerMessageDeflate)
    {

    }

    std::string WebSocketHandshake::trim(const std::string& str)
    {
        std::string out(str);
        out.erase(std::remove(out.begin(), out.end(), ' '), out.end());
        out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
        out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
        return out;
    }

    bool WebSocketHandshake::insensitiveStringCompare(const std::string& a, const std::string& b)
    {
        return std::equal(a.begin(), a.end(),
                          b.begin(), b.end(),
                          [](char a, char b)
                          {
                              return tolower(a) == tolower(b);
                          });
    }

    std::tuple<std::string, std::string, std::string> WebSocketHandshake::parseRequestLine(const std::string& line)
    {
        // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
        std::string token;
        std::stringstream tokenStream(line);
        std::vector<std::string> tokens;

        // Split by ' '
        while (std::getline(tokenStream, token, ' '))
        {
            tokens.push_back(token);
        }

        std::string method;
        if (tokens.size() >= 1)
        {
            method = trim(tokens[0]);
        }

        std::string requestUri;
        if (tokens.size() >= 2)
        {
            requestUri = trim(tokens[1]);
        }

        std::string httpVersion;
        if (tokens.size() >= 3)
        {
            httpVersion = trim(tokens[2]);
        }

        return std::make_tuple(method, requestUri, httpVersion);
    }

    std::string WebSocketHandshake::genRandomString(const int len)
    {
        std::string alphanum =
            "0123456789"
            "ABCDEFGH"
            "abcdefgh";

        std::random_device r;
        std::default_random_engine e1(r());
        std::uniform_int_distribution<int> dist(0, (int) alphanum.size() - 1);

        std::string s;
        s.resize(len);

        for (int i = 0; i < len; ++i)
        {
            int x = dist(e1);
            s[i] = alphanum[x];
        }

        return s;
    }

    WebSocketInitResult WebSocketHandshake::sendErrorResponse(int code, const std::string& reason)
    {
        std::stringstream ss;
        ss << "HTTP/1.1 ";
        ss << code;
        ss << " ";
        ss << reason;
        ss << "\r\n";

        // Socket write can only be cancelled through a timeout here, not manually.
        static std::atomic<bool> requestInitCancellation(false);
        auto isCancellationRequested =
            makeCancellationRequestWithTimeout(1, requestInitCancellation);

        if (!_socket->writeBytes(ss.str(), isCancellationRequested))
        {
            return WebSocketInitResult(false, 500, "Timed out while sending error response");
        }

        return WebSocketInitResult(false, code, reason);
    }

    WebSocketInitResult WebSocketHandshake::clientHandshake(const std::string& url,
                                                            const std::string& host,
                                                            const std::string& path,
                                                            int port,
                                                            int timeoutSecs)
    {
        _requestInitCancellation = false;

        auto isCancellationRequested =
            makeCancellationRequestWithTimeout(timeoutSecs, _requestInitCancellation);

        std::string errMsg;
        bool success = _socket->connect(host, port, errMsg, isCancellationRequested);
        if (!success)
        {
            std::stringstream ss;
            ss << "Unable to connect to " << host
               << " on port " << port
               << ", error: " << errMsg;
            return WebSocketInitResult(false, 0, ss.str());
        }

        //
        // Generate a random 24 bytes string which looks like it is base64 encoded
        // y3JJHMbDL1EzLkh9GBhXDw==
        // 0cb3Vd9HkbpVVumoS3Noka==
        //
        // See https://stackoverflow.com/questions/18265128/what-is-sec-websocket-key-for
        //
        std::string secWebSocketKey = genRandomString(22);
        secWebSocketKey += "==";

        std::stringstream ss;
        ss << "GET " << path << " HTTP/1.1\r\n";
        ss << "Host: "<< host << ":" << port << "\r\n";
        ss << "Upgrade: websocket\r\n";
        ss << "Connection: Upgrade\r\n";
        ss << "Sec-WebSocket-Version: 13\r\n";
        ss << "Sec-WebSocket-Key: " << secWebSocketKey << "\r\n";

        if (_enablePerMessageDeflate)
        {
            ss << _perMessageDeflateOptions.generateHeader();
        }

        ss << "\r\n";

        if (!_socket->writeBytes(ss.str(), isCancellationRequested))
        {
            return WebSocketInitResult(false, 0, std::string("Failed sending GET request to ") + url);
        }

        // Read HTTP status line
        auto lineResult = _socket->readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            return WebSocketInitResult(false, 0,
                                       std::string("Failed reading HTTP status line from ") + url);
        }

        // Validate status
        int status;

        // HTTP/1.0 is too old.
        if (sscanf(line.c_str(), "HTTP/1.0 %d", &status) == 1)
        {
            std::stringstream ss;
            ss << "Server version is HTTP/1.0. Rejecting connection to " << host
               << ", status: " << status
               << ", HTTP Status line: " << line;
            return WebSocketInitResult(false, status, ss.str());
        }

        // We want an 101 HTTP status
        if (sscanf(line.c_str(), "HTTP/1.1 %d", &status) != 1 || status != 101)
        {
            std::stringstream ss;
            ss << "Got bad status connecting to " << host
               << ", status: " << status
               << ", HTTP Status line: " << line;
            return WebSocketInitResult(false, status, ss.str());
        }

        auto result = parseHttpHeaders(_socket, isCancellationRequested);
        auto headersValid = result.first;
        auto headers = result.second;

        if (!headersValid)
        {
            return WebSocketInitResult(false, status, "Error parsing HTTP headers");
        }

        // Check the presence of the connection field
        if (headers.find("connection") == headers.end())
        {
            std::string errorMsg("Missing connection value");
            return WebSocketInitResult(false, status, errorMsg);
        }

        // Check the value of the connection field
        // Some websocket servers (Go/Gorilla?) send lowercase values for the
        // connection header, so do a case insensitive comparison
        if (!insensitiveStringCompare(headers["connection"], "Upgrade"))
        {
            std::stringstream ss;
            ss << "Invalid connection value: " << headers["connection"];
            return WebSocketInitResult(false, status, ss.str());
        }

        char output[29] = {};
        WebSocketHandshakeKeyGen::generate(secWebSocketKey, output);
        if (std::string(output) != headers["sec-websocket-accept"])
        {
            std::string errorMsg("Invalid Sec-WebSocket-Accept value");
            return WebSocketInitResult(false, status, errorMsg);
        }

        if (_enablePerMessageDeflate)
        {
            // Parse the server response. Does it support deflate ?
            std::string header = headers["sec-websocket-extensions"];
            WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(header);

            // If the server does not support that extension, disable it.
            if (!webSocketPerMessageDeflateOptions.enabled())
            {
                _enablePerMessageDeflate = false;
            }
            // Otherwise try to initialize the deflate engine (zlib)
            else if (!_perMessageDeflate.init(webSocketPerMessageDeflateOptions))
            {
                return WebSocketInitResult(
                    false, 0,"Failed to initialize per message deflate engine");
            }
        }

        return WebSocketInitResult(true, status, "", headers, path);
    }

    WebSocketInitResult WebSocketHandshake::serverHandshake(int fd, int timeoutSecs)
    {
        _requestInitCancellation = false;

        // Set the socket to non blocking mode + other tweaks
        SocketConnect::configure(fd);

        auto isCancellationRequested =
            makeCancellationRequestWithTimeout(timeoutSecs, _requestInitCancellation);

        std::string remote = std::string("remote fd ") + std::to_string(fd);

        // Read first line
        auto lineResult = _socket->readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            return sendErrorResponse(400, "Error reading HTTP request line");
        }

        // Validate request line (GET /foo HTTP/1.1\r\n)
        auto requestLine = parseRequestLine(line);
        auto method      = std::get<0>(requestLine);
        auto uri         = std::get<1>(requestLine);
        auto httpVersion = std::get<2>(requestLine);

        if (method != "GET")
        {
            return sendErrorResponse(400, "Invalid HTTP method, need GET, got " + method);
        }

        if (httpVersion != "HTTP/1.1")
        {
            return sendErrorResponse(400, "Invalid HTTP version, need HTTP/1.1, got: " + httpVersion);
        }

        // Retrieve and validate HTTP headers
        auto result = parseHttpHeaders(_socket, isCancellationRequested);
        auto headersValid = result.first;
        auto headers = result.second;

        if (!headersValid)
        {
            return sendErrorResponse(400, "Error parsing HTTP headers");
        }

        if (headers.find("sec-websocket-key") == headers.end())
        {
            return sendErrorResponse(400, "Missing Sec-WebSocket-Key value");
        }

        if (headers["upgrade"] != "websocket")
        {
            return sendErrorResponse(400, "Invalid or missing Upgrade header");
        }

        if (headers.find("sec-websocket-version") == headers.end())
        {
            return sendErrorResponse(400, "Missing Sec-WebSocket-Version value");
        }

        {
            std::stringstream ss;
            ss << headers["sec-websocket-version"];
            int version;
            ss >> version;

            if (version != 13)
            {
                return sendErrorResponse(400, "Invalid Sec-WebSocket-Version, "
                                              "need 13, got" + ss.str());
            }
        }

        char output[29] = {};
        WebSocketHandshakeKeyGen::generate(headers["sec-websocket-key"], output);

        std::stringstream ss;
        ss << "HTTP/1.1 101 Switching Protocols\r\n";
        ss << "Sec-WebSocket-Accept: " << std::string(output) << "\r\n";
        ss << "Upgrade: websocket\r\n";
        ss << "Connection: Upgrade\r\n";

        // Parse the client headers. Does it support deflate ?
        std::string header = headers["sec-websocket-extensions"];
        WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(header);

        // If the client has requested that extension, enable it.
        if (webSocketPerMessageDeflateOptions.enabled())
        {
            _enablePerMessageDeflate = true;

            if (!_perMessageDeflate.init(webSocketPerMessageDeflateOptions))
            {
                return WebSocketInitResult(
                    false, 0,"Failed to initialize per message deflate engine");
            }
            ss << webSocketPerMessageDeflateOptions.generateHeader();
        }

        ss << "\r\n";

        if (!_socket->writeBytes(ss.str(), isCancellationRequested))
        {
            return WebSocketInitResult(false, 0, std::string("Failed sending response to ") + remote);
        }

        return WebSocketInitResult(true, 200, "", headers, uri);
    }
}
