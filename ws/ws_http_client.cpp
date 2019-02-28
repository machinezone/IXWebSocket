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
                            int timeoutSecs,
                            bool followRedirects,
                            bool verbose,
                            bool save,
                            const std::string& output)
    {
        HttpRequestArgs args;
        args.url = url;
        args.extraHeaders = parseHeaders(headersData);
        args.timeoutSecs = timeoutSecs;
        args.followRedirects = followRedirects;
        args.verbose = verbose;

        HttpParameters httpParameters = parsePostParameters(data);

        HttpClient httpClient;
        HttpResponse out;
        if (headersOnly)
        {
            out = httpClient.head(args);
        }
        else if (data.empty())
        {
            out = httpClient.get(args);
        }
        else
        {
            out = httpClient.post(httpParameters, args);
        }

        auto errorCode = std::get<0>(out);
        auto responseHeaders = std::get<1>(out);
        auto payload = std::get<2>(out);
        auto errorMsg = std::get<3>(out);

        for (auto it : responseHeaders)
        {
            std::cerr << it.first << ": " << it.second << std::endl;
        }

        std::cerr << "error code: " << errorCode << std::endl;
        if (!errorMsg.empty())
        {
            std::cerr << "error message: " << errorMsg << std::endl;
        }

        if (!headersOnly && errorCode == 200)
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
        }

        return 0;
    }
}
