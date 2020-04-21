/*
 *  IXCobraToSentryBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToSentryBot.h"

#include "IXCobraBot.h"
#include "IXQueueManager.h"
#include <chrono>
#include <ixcobra/IXCobraConnection.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>

namespace ix
{
    int64_t cobra_to_sentry_bot(const CobraConfig& config,
                                const std::string& channel,
                                const std::string& filter,
                                const std::string& position,
                                SentryClient& sentryClient,
                                bool verbose,
                                size_t maxQueueSize,
                                bool enableHeartbeat,
                                int runtime)
    {
        CobraBot bot;
        bot.setOnBotMessageCallback([&sentryClient](const Json::Value& msg,
                                                    const std::string& /*position*/,
                                                    const bool verbose,
                                                    std::atomic<bool>& throttled,
                                                    std::atomic<bool> &
                                                    /*fatalCobraError*/) -> bool {
            auto ret = sentryClient.send(msg, verbose);
            HttpResponsePtr response = ret.first;

            if (!response)
            {
                spdlog::warn("Null HTTP Response");
                return false;
            }

            if (verbose)
            {
                for (auto it : response->headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }

                spdlog::info("Upload size: {}", response->uploadSize);
                spdlog::info("Download size: {}", response->downloadSize);

                spdlog::info("Status: {}", response->statusCode);
                if (response->errorCode != HttpErrorCode::Ok)
                {
                    spdlog::info("error message: {}", response->errorMsg);
                }

                if (response->headers["Content-Type"] != "application/octet-stream")
                {
                    spdlog::info("payload: {}", response->payload);
                }
            }

            bool success = response->statusCode == 200;

            if (!success)
            {
                spdlog::error("Error sending data to sentry: {}", response->statusCode);
                spdlog::error("Body: {}", ret.second);
                spdlog::error("Response: {}", response->payload);

                // Error 429 Too Many Requests
                if (response->statusCode == 429)
                {
                    auto retryAfter = response->headers["Retry-After"];
                    std::stringstream ss;
                    ss << retryAfter;
                    int seconds;
                    ss >> seconds;

                    if (!ss.eof() || ss.fail())
                    {
                        seconds = 30;
                        spdlog::warn("Error parsing Retry-After header. "
                                     "Using {} for the sleep duration",
                                     seconds);
                    }

                    spdlog::warn("Error 429 - Too Many Requests. ws will sleep "
                                 "and retry after {} seconds",
                                 retryAfter);

                    throttled = true;
                    auto duration = std::chrono::seconds(seconds);
                    std::this_thread::sleep_for(duration);
                    throttled = false;
                }
            }

            return success;
        });

        bool useQueue = true;

        return bot.run(config,
                       channel,
                       filter,
                       position,
                       verbose,
                       maxQueueSize,
                       useQueue,
                       enableHeartbeat,
                       runtime);
    }
} // namespace ix
