/*
 *  http_client.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>

namespace ix
{
    std::string extractFilename(const std::string& path)
    {
        std::string::size_type idx;

        idx = path.rfind('/');
        if (idx != std::string::npos)
        {
            std::string filename = path.substr(idx+1);
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
            auto val = token.substr(pos+2);

            std::cerr << key << ": " << val << std::endl;
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
            auto val = token.substr(pos+1);

            std::cerr << key << ": " << val << std::endl;
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
                            bool compress)
    {
        HttpRequestArgs args;
        args.extraHeaders = parseHeaders(headersData);
        args.connectTimeout = connectTimeout;
        args.transferTimeout = transferTimeout;
        args.followRedirects = followRedirects;
        args.maxRedirects = maxRedirects;
        args.verbose = verbose;
        args.compress = compress;
        args.logger = [](const std::string& msg)
        {
            std::cout << msg;
        };
        args.onProgressCallback = [](int current, int total) -> bool
        {
            std::cerr << "\r" << "Downloaded "
                      << current << " bytes out of " << total;
            return true;
        };

        HttpParameters httpParameters = parsePostParameters(data);

        HttpClient httpClient;
        HttpResponse out;
        if (headersOnly)
        {
            out = httpClient.head(url, args);
        }
        else if (data.empty())
        {
            out = httpClient.get(url, args);
        }
        else
        {
            out = httpClient.post(url, httpParameters, args);
        }

        std::cerr << std::endl;

        auto statusCode = std::get<0>(out);
        auto errorCode = std::get<1>(out);
        auto responseHeaders = std::get<2>(out);
        auto payload = std::get<3>(out);
        auto errorMsg = std::get<4>(out);
        auto uploadSize = std::get<5>(out);
        auto downloadSize = std::get<6>(out);

        for (auto it : responseHeaders)
        {
            std::cerr << it.first << ": " << it.second << std::endl;
        }

        std::cerr << "Upload size: " << uploadSize << std::endl;
        std::cerr << "Download size: " << downloadSize << std::endl;

        std::cerr << "Status: " << statusCode << std::endl;
        if (errorCode != HttpErrorCode_Ok)
        {
            std::cerr << "error message: " << errorMsg << std::endl;
        }

        if (!headersOnly && errorCode == HttpErrorCode_Ok)
        {
            if (save || !output.empty())
            {
                // FIMXE we should decode the url first
                std::string filename = extractFilename(url);
                if (!output.empty())
                {
                    filename = output;
                }

                std::cout << "Writing to disk: " << filename << std::endl;
                std::ofstream out(filename);
                out.write((char*)&payload.front(), payload.size());
                out.close();
            }
            else
            {
                if (responseHeaders["Content-Type"] != "application/octet-stream")
                {
                    std::cout << "payload: " << payload << std::endl;
                }
                else
                {
                    std::cerr << "Binary output can mess up your terminal." << std::endl;
                    std::cerr << "Use the -O flag to save the file to disk." << std::endl;
                    std::cerr << "You can also use the --output option to specify a filename." << std::endl;
                }
            }
        }

        return 0;
    }
}
