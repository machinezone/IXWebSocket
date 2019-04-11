/*
 *  IXSentryClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXSentryClient.h"

#include <chrono>
#include <iostream>

#include <ixwebsocket/IXWebSocketHttpHeaders.h>


namespace ix
{
    SentryClient::SentryClient(const std::string& dsn) :
        _dsn(dsn),
        _validDsn(false),
        _luaFrameRegex("\t([^/]+):([0-9]+): in function '([^/]+)'")
    {
        const std::regex dsnRegex("(http[s]?)://([^:]+):([^@]+)@([^/]+)/([0-9]+)");
        std::smatch group;

        if (std::regex_match(dsn, group, dsnRegex) and group.size() == 6)
        {
            _validDsn = true;

            const auto scheme = group.str(1);
            const auto host = group.str(4);
            const auto project_id = group.str(5);
            _url = scheme + "://" + host + "/api/" + project_id + "/store/";

            _publicKey = group.str(2);
            _secretKey = group.str(3);
        }
    }

    int64_t SentryClient::getTimestamp()
    {
        const auto tp = std::chrono::system_clock::now();
        const auto dur = tp.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    }

    std::string SentryClient::getIso8601()
    {
        std::time_t now;
        std::time(&now);
        char buf[sizeof "2011-10-08T07:07:09Z"];
        std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
        return buf;
    }

    std::string SentryClient::computeAuthHeader()
    {
        std::string securityHeader("Sentry sentry_version=5");
        securityHeader += ",sentry_client=ws/1.0.0";
        securityHeader += ",sentry_timestamp=" + std::to_string(SentryClient::getTimestamp());
        securityHeader += ",sentry_key=" + _publicKey;
        securityHeader += ",sentry_secret=" + _secretKey;

        return securityHeader;
    }

    Json::Value SentryClient::parseLuaStackTrace(const std::string& stack)
    {
        Json::Value frames;

        // Split by lines
        std::string line;
        std::stringstream tokenStream(stack);

        std::stringstream ss;
        std::smatch group;

        while (std::getline(tokenStream, line))
        {
            //	MapScene.lua:2169: in function 'singleCB'
            if (std::regex_match(line, group, _luaFrameRegex))
            {
                const auto fileName = group.str(1);
                const auto linenoStr = group.str(2);
                const auto function = group.str(3);

                ss << linenoStr;
                uint64_t lineno;
                ss >> lineno;

                Json::Value frame;
                frame["lineno"] = lineno;
                frame["filename"] = fileName;
                frame["function"] = function;

                frames.append(frame);
            }
        }

        return frames;
    }

    std::string SentryClient::computePayload(const Json::Value& msg)
    {
        Json::Value payload;
        payload["platform"] = "python";
        payload["sdk"]["name"] = "ws";
        payload["sdk"]["version"] = "1.0.0";
        payload["timestamp"] = SentryClient::getIso8601();

        Json::Value exception;
        exception["value"] = msg["data"]["message"];

        std::string stackTraceFieldName = 
            (msg["id"].asString() == "game_noisytypes_id") ? "traceback" : "stack";
                             
        exception["stacktrace"]["frames"] = 
            parseLuaStackTrace(msg["data"][stackTraceFieldName].asString());

        payload["exception"].append(exception);

        Json::Value extra;
        extra["cobra_event"] = msg;

        exception["extra"] = extra;

        return _jsonWriter.write(payload);
    }

    bool SentryClient::send(const Json::Value& msg,
                            bool verbose)
    {
        HttpRequestArgs args;
        args.extraHeaders["X-Sentry-Auth"] = SentryClient::computeAuthHeader();
        args.connectTimeout = 60;
        args.transferTimeout = 5 * 60;
        args.followRedirects = true;
        args.verbose = verbose;
        args.logger = [](const std::string& msg)
        {
            std::cout << msg;
        };

        std::string body = computePayload(msg);
        HttpResponse out = _httpClient.post(_url, body, args);

        auto statusCode = std::get<0>(out);
        auto errorCode = std::get<1>(out);
        auto responseHeaders = std::get<2>(out);
        auto payload = std::get<3>(out);
        auto errorMsg = std::get<4>(out);
        auto uploadSize = std::get<5>(out);
        auto downloadSize = std::get<6>(out);

        if (verbose)
        {
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

            if (responseHeaders["Content-Type"] != "application/octet-stream")
            {
                std::cerr << "payload: " << payload << std::endl;
            }
        }

        return statusCode == 200;
    }
} // namespace ix
