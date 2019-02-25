/*
 *  http_client.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXHttpClient.h>

namespace ix
{
    //
    // Useful endpoint to test HTTP post
    // https://postman-echo.com/post
    //
    HttpParameters parsePostParameters(const std::string& data)
    {
        HttpParameters httpParameters;

        // Split by ;
        std::string token;
        std::stringstream tokenStream(data);

        while (std::getline(tokenStream, token))
        {
            std::size_t pos = token.rfind('=');

            // Bail out if last '.' is found
            if (pos == std::string::npos) continue;

            auto key = token.substr(0, pos);
            auto val = token.substr(pos+1);

            std::cout << key << ": " << val << std::endl;
            httpParameters[key] = val;
        }

        return httpParameters;
    }

    int ws_http_client_main(const std::string& url,
                            const std::string& data)
    {
        HttpParameters httpParameters = parsePostParameters(data);

        HttpClient httpClient;
        bool verbose = true;
        HttpResponse out;
        if (data.empty())
        {
            out = httpClient.get(url, verbose);
        }
        else
        {
            out = httpClient.post(url, httpParameters, verbose);
        }
        auto errorCode = std::get<0>(out);
        auto headers = std::get<1>(out);
        auto payload = std::get<2>(out);
        auto errorMsg = std::get<3>(out);

        for (auto it : headers)
        {
            std::cout << it.first << ": " << it.second << std::endl;
        }

        std::cout << "error code: " << errorCode << std::endl;
        if (!errorMsg.empty())
        {
            std::cout << "error message: " << errorMsg << std::endl;
        }

        std::cout << "payload: " << payload << std::endl;

        return 0;
    }
}
