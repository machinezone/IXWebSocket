/*
 *  http_client.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <fstream>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace ix
{
    std::string extractFilename(const std::string& path)
    {
        std::string::size_type idx;

        idx = path.rfind('/');
        if (idx != std::string::npos)
        {
            std::string filename = path.substr(idx + 1);
            return filename;
        }
        else
        {
            return path;
        }
    }

    WebSocketHttpHeaders parseHeaders(const std::string& data)
    {
        WebSocketHttpHeaders headers;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(data);

        while (std::getline(tokenStream, token))
        {
            std::size_t pos = token.rfind(':');

            // Bail out if last '.' is found
            if (pos == std::string::npos) continue;

            auto key = token.substr(0, pos);
            auto val = token.substr(pos + 1);

            spdlog::info("{}: {}", key, val);
            headers[key] = val;
        }

        return headers;
    }

    //
    // Useful endpoint to test HTTP post
    // https://postman-echo.com/post
    //
    HttpParameters parsePostParameters(const std::string& data)
    {
        HttpParameters httpParameters;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(data);

        while (std::getline(tokenStream, token))
        {
            std::size_t pos = token.rfind('=');

            // Bail out if last '.' is found
            if (pos == std::string::npos) continue;

            auto key = token.substr(0, pos);
            auto val = token.substr(pos + 1);

            spdlog::info("{}: {}", key, val);
            httpParameters[key] = val;
        }

        return httpParameters;
    }

    int ws_http_client_main(const std::string& url,
                            const std::string& headersData,
                            const std::string& data,
                            bool headersOnly,
                            int connectTimeout,
                            int transferTimeout,
                            bool followRedirects,
                            int maxRedirects,
                            bool verbose,
                            bool save,
                            const std::string& output,
                            bool compress,
                            const ix::SocketTLSOptions& tlsOptions)
    {
        HttpClient httpClient;
        httpClient.setTLSOptions(tlsOptions);

        auto args = httpClient.createRequest();
        args->extraHeaders = parseHeaders(headersData);
        args->connectTimeout = connectTimeout;
        args->transferTimeout = transferTimeout;
        args->followRedirects = followRedirects;
        args->maxRedirects = maxRedirects;
        args->verbose = verbose;
        args->compress = compress;
        args->logger = [](const std::string& msg) { spdlog::info(msg); };
        args->onProgressCallback = [](int current, int total) -> bool {
            spdlog::info("Downloaded {} bytes out of {}", current, total);
            return true;
        };

        HttpParameters httpParameters = parsePostParameters(data);

        HttpResponsePtr response;
        if (headersOnly)
        {
            response = httpClient.head(url, args);
        }
        else if (data.empty())
        {
            response = httpClient.get(url, args);
        }
        else
        {
            response = httpClient.post(url, httpParameters, args);
        }

        spdlog::info("");

        for (auto it : response->headers)
        {
            spdlog::info("{}: {}", it.first, it.second);
        }

        spdlog::info("Upload size: {}", response->uploadSize);
        spdlog::info("Download size: {}", response->downloadSize);

        spdlog::info("Status: {}", response->statusCode);
        if (response->errorCode != HttpErrorCode::Ok)
        {
            spdlog::info("error message: ", response->errorMsg);
        }

        if (!headersOnly && response->errorCode == HttpErrorCode::Ok)
        {
            if (save || !output.empty())
            {
                // FIMXE we should decode the url first
                std::string filename = extractFilename(url);
                if (!output.empty())
                {
                    filename = output;
                }

                spdlog::info("Writing to disk: {}", filename);
                std::ofstream out(filename);
                out.write((char*) &response->payload.front(), response->payload.size());
                out.close();
            }
            else
            {
                if (response->headers["Content-Type"] != "application/octet-stream")
                {
                    spdlog::info("payload: {}", response->payload);
                }
                else
                {
                    spdlog::info("Binary output can mess up your terminal.");
                    spdlog::info("Use the -O flag to save the file to disk.");
                    spdlog::info("You can also use the --output option to specify a filename.");
                }
            }
        }

        return 0;
    }
} // namespace ix
