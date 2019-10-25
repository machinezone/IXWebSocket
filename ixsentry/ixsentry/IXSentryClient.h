/*
 *  IXSentryClient.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#pragma once

#include <algorithm>
#include <ixwebsocket/IXHttpClient.h>
#include <jsoncpp/json/json.h>
#include <regex>

namespace ix
{
    class SentryClient
    {
    public:
        SentryClient(const std::string& dsn);
        ~SentryClient() = default;

        std::pair<HttpResponsePtr, std::string> send(const Json::Value& msg, bool verbose);

    private:
        int64_t getTimestamp();
        std::string computeAuthHeader();
        std::string getIso8601();
        std::string computePayload(const Json::Value& msg);

        Json::Value parseLuaStackTrace(const std::string& stack);

        std::string _dsn;
        bool _validDsn;
        std::string _url;

        // Used for authentication with a header
        std::string _publicKey;
        std::string _secretKey;

        Json::FastWriter _jsonWriter;

        std::regex _luaFrameRegex;

        HttpClient _httpClient;
    };

} // namespace ix
