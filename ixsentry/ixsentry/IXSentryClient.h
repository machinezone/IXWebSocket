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
#include <memory>

namespace ix
{
    class SentryClient
    {
    public:
        SentryClient(const std::string& dsn);
        ~SentryClient() = default;

        std::pair<HttpResponsePtr, std::string> send(const Json::Value& msg, bool verbose);

        Json::Value parseLuaStackTrace(const std::string& stack);

        void uploadMinidump(
            const std::string& sentryMetadata,
            const std::string& minidumpBytes,
            const std::string& project,
            const std::string& key,
            bool verbose,
            const OnResponseCallback& onResponseCallback);

    private:
        int64_t getTimestamp();
        std::string computeAuthHeader();
        std::string getIso8601();
        std::string computePayload(const Json::Value& msg);

        std::string computeUrl(const std::string& project, const std::string& key);

        void displayReponse(HttpResponsePtr response);

        std::string _dsn;
        bool _validDsn;
        std::string _url;

        // Used for authentication with a header
        std::string _publicKey;
        std::string _secretKey;

        Json::FastWriter _jsonWriter;

        std::regex _luaFrameRegex;

        std::shared_ptr<HttpClient> _httpClient;
    };

} // namespace ix
