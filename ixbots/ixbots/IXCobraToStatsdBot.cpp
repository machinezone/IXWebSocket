/*
 *  IXCobraToStatsdBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToStatsdBot.h"

#include "IXCobraBot.h"
#include "IXStatsdClient.h"
#include <chrono>
#include <ixcobra/IXCobraConnection.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <sstream>
#include <vector>

namespace ix
{
    // fields are command line argument that can be specified multiple times
    std::vector<std::string> parseFields(const std::string& fields)
    {
        std::vector<std::string> tokens;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(fields);

        while (std::getline(tokenStream, token))
        {
            tokens.push_back(token);
        }

        return tokens;
    }

    //
    // Extract an attribute from a Json Value.
    // extractAttr("foo.bar", {"foo": {"bar": "baz"}}) => baz
    //
    Json::Value extractAttr(const std::string& attr, const Json::Value& jsonValue)
    {
        // Split by .
        std::string token;
        std::stringstream tokenStream(attr);

        Json::Value val(jsonValue);

        while (std::getline(tokenStream, token, '.'))
        {
            val = val[token];
        }

        return val;
    }

    int64_t cobra_to_statsd_bot(const ix::CobraBotConfig& config,
                                StatsdClient& statsdClient,
                                const std::string& fields,
                                const std::string& gauge,
                                const std::string& timer,
                                bool verbose)
    {
        auto tokens = parseFields(fields);

        CobraBot bot;
        bot.setOnBotMessageCallback(
            [&statsdClient, &tokens, &gauge, &timer, &verbose](const Json::Value& msg,
                                                     const std::string& /*position*/,
                                                     std::atomic<bool>& /*throttled*/,
                                                     std::atomic<bool>& fatalCobraError,
                                                     std::atomic<uint64_t>& sentCount) -> void {
                std::string id;
                for (auto&& attr : tokens)
                {
                    id += ".";
                    auto val = extractAttr(attr, msg);
                    id += val.asString();
                }

                if (gauge.empty() && timer.empty())
                {
                    statsdClient.count(id, 1);
                }
                else
                {
                    std::string attrName = (!gauge.empty()) ? gauge : timer;
                    auto val = extractAttr(attrName, msg);
                    size_t x;

                    if (val.isInt())
                    {
                        x = (size_t) val.asInt();
                    }
                    else if (val.isInt64())
                    {
                        x = (size_t) val.asInt64();
                    }
                    else if (val.isUInt())
                    {
                        x = (size_t) val.asUInt();
                    }
                    else if (val.isUInt64())
                    {
                        x = (size_t) val.asUInt64();
                    }
                    else if (val.isDouble())
                    {
                        x = (size_t) val.asUInt64();
                    }
                    else
                    {
                        CoreLogger::error("Gauge " + gauge + " is not a numeric type");
                        fatalCobraError = true;
                        return;
                    }

                    if (verbose)
                    {
                        CoreLogger::info(id + " - " + attrName + " -> " + std::to_string(x));
                    }

                    if (!gauge.empty())
                    {
                        statsdClient.gauge(id, x);
                    }
                    else
                    {
                        statsdClient.timing(id, x);
                    }
                }

                sentCount++;
            });

        return bot.run(config);
    }
} // namespace ix
