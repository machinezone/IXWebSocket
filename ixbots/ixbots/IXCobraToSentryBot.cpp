/*
 *  IXCobraToSentryBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToSentryBot.h"

#include "IXCobraBot.h"
#include "IXQueueManager.h"
#include <ixcobra/IXCobraConnection.h>
#include <ixcore/utils/IXCoreLogger.h>

#include <chrono>
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
                CoreLogger::warn("Null HTTP Response");
                return false;
            }

            if (verbose)
            {
                for (auto it : response->headers)
                {
                    CoreLogger::info(it.first + ": " + it.second);
                }

                CoreLogger::info("Upload size: " + std::to_string(response->uploadSize));
                CoreLogger::info("Download size: " + std::to_string(response->downloadSize));

                CoreLogger::info("Status: " + std::to_string(response->statusCode));
                if (response->errorCode != HttpErrorCode::Ok)
                {
                    CoreLogger::info("error message: " + response->errorMsg);
                }

                if (response->headers["Content-Type"] != "application/octet-stream")
                {
                    CoreLogger::info("payload: " + response->payload);
                }
            }

            bool success = response->statusCode == 200;

            if (!success)
            {
                CoreLogger::error("Error sending data to sentry: " + std::to_string(response->statusCode));
                CoreLogger::error("Body: " + ret.second);
                CoreLogger::error("Response: " + response->payload);

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
                        CoreLogger::warn("Error parsing Retry-After header. "
                                         "Using " + retryAfter + " for the sleep duration");
                    }

                    CoreLogger::warn("Error 429 - Too Many Requests. ws will sleep "
                                     "and retry after " + retryAfter + " seconds");

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
