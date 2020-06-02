/*
 *  IXHttpServer.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttpServer.h"

#include "IXNetSystem.h"
#include "IXSocketConnect.h"
#include "IXUserAgent.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <zlib.h>
#include <cstring>

namespace
{
    std::pair<bool, std::vector<uint8_t>> load(const std::string& path)
    {
        std::vector<uint8_t> memblock;

        std::ifstream file(path);
        if (!file.is_open()) return std::make_pair(false, memblock);

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        memblock.resize((size_t) size);
        file.read((char*) &memblock.front(), static_cast<std::streamsize>(size));

        return std::make_pair(true, memblock);
    }

    std::pair<bool, std::string> readAsString(const std::string& path)
    {
        auto res = load(path);
        auto vec = res.second;
        return std::make_pair(res.first, std::string(vec.begin(), vec.end()));
    }

    std::string gzipCompress(const std::string& str)
    {
        z_stream zs; // z_stream is zlib's control structure
        memset(&zs, 0, sizeof(zs));

        // deflateInit2 configure the file format: request gzip instead of deflate
        const int windowBits = 15;
        const int GZIP_ENCODING = 16;

        deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     windowBits | GZIP_ENCODING, 8,
                     Z_DEFAULT_STRATEGY);

        zs.next_in = (Bytef*) str.data();
        zs.avail_in = (uInt) str.size(); // set the z_stream's input

        int ret;
        char outbuffer[32768];
        std::string outstring;

        // retrieve the compressed bytes blockwise
        do
        {
            zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
            zs.avail_out = sizeof(outbuffer);

            ret = deflate(&zs, Z_FINISH);

            if(outstring.size() < zs.total_out)
            {
                // append the block to the output string
                outstring.append(outbuffer,
                                 zs.total_out - outstring.size());
            }
        } while(ret == Z_OK);

        deflateEnd(&zs);

        return outstring;
    }
} // namespace

namespace ix
{
    HttpServer::HttpServer(
        int port, const std::string& host, int backlog, size_t maxConnections, int addressFamily)
        : SocketServer(port, host, backlog, maxConnections, addressFamily)
        , _connectedClientsCount(0)
    {
        setDefaultConnectionCallback();
    }

    HttpServer::~HttpServer()
    {
        stop();
    }

    void HttpServer::stop()
    {
        stopAcceptingConnections();

        // FIXME: cancelling / closing active clients ...

        SocketServer::stop();
    }

    void HttpServer::setOnConnectionCallback(const OnConnectionCallback& callback)
    {
        _onConnectionCallback = callback;
    }

    void HttpServer::handleConnection(std::unique_ptr<Socket> socket,
                                      std::shared_ptr<ConnectionState> connectionState)
    {
        _connectedClientsCount++;

        auto ret = Http::parseRequest(socket);
        // FIXME: handle errors in parseRequest

        if (std::get<0>(ret))
        {
            auto response = _onConnectionCallback(std::get<2>(ret), connectionState);
            if (!Http::sendResponse(response, socket))
            {
                logError("Cannot send response");
            }
        }
        connectionState->setTerminated();

        _connectedClientsCount--;
    }

    size_t HttpServer::getConnectedClientsCount()
    {
        return _connectedClientsCount;
    }

    void HttpServer::setDefaultConnectionCallback()
    {
        setOnConnectionCallback(
            [this](HttpRequestPtr request,
                   std::shared_ptr<ConnectionState> /*connectionState*/) -> HttpResponsePtr {
                std::string uri(request->uri);
                if (uri.empty() || uri == "/")
                {
                    uri = "/index.html";
                }

                WebSocketHttpHeaders headers;
                headers["Server"] = userAgent();

                std::string path("." + uri);
                auto res = readAsString(path);
                bool found = res.first;
                if (!found)
                {
                    return std::make_shared<HttpResponse>(
                        404, "Not Found", HttpErrorCode::Ok, WebSocketHttpHeaders(), std::string());
                }

                std::string content = res.second;

                std::string acceptEncoding = request->headers["Accept-encoding"];
                if (acceptEncoding == "*" || acceptEncoding.find("gzip") != std::string::npos)
                {
                    content = gzipCompress(content);
                    headers["Content-Encoding"] = "gzip";
                }

                // Log request
                std::stringstream ss;
                ss << request->method << " " << request->headers["User-Agent"] << " "
                   << request->uri << " " << content.size();
                logInfo(ss.str());

                // FIXME: check extensions to set the content type
                // headers["Content-Type"] = "application/octet-stream";
                headers["Accept-Ranges"] = "none";

                for (auto&& it : request->headers)
                {
                    headers[it.first] = it.second;
                }

                return std::make_shared<HttpResponse>(
                    200, "OK", HttpErrorCode::Ok, headers, content);
            });
    }

    void HttpServer::makeRedirectServer(const std::string& redirectUrl)
    {
        //
        // See https://developer.mozilla.org/en-US/docs/Web/HTTP/Redirections
        //
        setOnConnectionCallback(
            [this,
             redirectUrl](HttpRequestPtr request,
                          std::shared_ptr<ConnectionState> /*connectionState*/) -> HttpResponsePtr {
                WebSocketHttpHeaders headers;
                headers["Server"] = userAgent();

                // Log request
                std::stringstream ss;
                ss << request->method << " " << request->headers["User-Agent"] << " "
                   << request->uri;
                logInfo(ss.str());

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
} // namespace ix
