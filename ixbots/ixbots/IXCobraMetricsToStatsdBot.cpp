/*
 *  IXCobraMetricsToStatsdBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraMetricsToStatsdBot.h"

#include "IXCobraBot.h"
#include "IXStatsdClient.h"
#include <chrono>
#include <ixcobra/IXCobraConnection.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>


namespace
{
    std::string removeSpaces(const std::string& str)
    {
        std::string out(str);
        out.erase(
            std::remove_if(out.begin(), out.end(), [](unsigned char x) { return std::isspace(x); }),
            out.end());

        return out;
    }
}

namespace ix
{
    bool processPerfMetricsEvent(const Json::Value& msg,
                                 StatsdClient& statsdClient)
    {
        auto frameRateHistogram = msg["data"]["FrameRateHistogram"];
        auto frameTimeTotal = msg["data"]["FrameTimeTotal"].asFloat();

        auto gt_60 = 100 * frameRateHistogram[0].asFloat() / frameTimeTotal;
        auto lt_60 = 100 * frameRateHistogram[1].asFloat() / frameTimeTotal;
        auto lt_30 = 100 * frameRateHistogram[2].asFloat() / frameTimeTotal;
        auto lt_20 = 100 * frameRateHistogram[3].asFloat() / frameTimeTotal;
        auto lt_15 = 100 * frameRateHistogram[4].asFloat() / frameTimeTotal;
        auto lt_12 = 100 * frameRateHistogram[5].asFloat() / frameTimeTotal;
        auto lt_10 = 100 * frameRateHistogram[6].asFloat() / frameTimeTotal;
        auto lt_08 = 100 * frameRateHistogram[7].asFloat() / frameTimeTotal;

        std::stringstream ss;
        ss <<  msg["id"].asString() << "."
           <<  msg["device"]["game"].asString() << "."
           <<  msg["device"]["os_name"].asString() << "."
           <<  removeSpaces(msg["data"]["Tag"].asString());

        std::string id = ss.str();

        statsdClient.gauge(id + ".gt_60", gt_60);
        statsdClient.gauge(id + ".lt_60", lt_60);
        statsdClient.gauge(id + ".lt_30", lt_30);
        statsdClient.gauge(id + ".lt_20", lt_20);
        statsdClient.gauge(id + ".lt_15", lt_15);
        statsdClient.gauge(id + ".lt_12", lt_12);
        statsdClient.gauge(id + ".lt_10", lt_10);
        statsdClient.gauge(id + ".lt_08", lt_08);

        return true;
    }

    int64_t cobra_metrics_to_statsd_bot(const ix::CobraBotConfig& config,
                                        StatsdClient& statsdClient,
                                        bool verbose)
    {
        CobraBot bot;
        bot.setOnBotMessageCallback(
            [&statsdClient, &verbose](const Json::Value& msg,
                                      const std::string& /*position*/,
                                      std::atomic<bool>& /*throttled*/,
                                      std::atomic<bool>& /*fatalCobraError*/,
                                      std::atomic<uint64_t>& sentCount) -> void {
            if (msg["device"].isNull() || msg["id"].isNull())
            {
                CoreLogger::info("no device or id entry, skipping event");
                return;
            }

            //
            // Display full message with
            if (verbose)
            {
                CoreLogger::info(msg.toStyledString());
            }

            bool success = false;
            if (msg["id"].asString() == "engine_performance_metrics_id")
            {
                success = processPerfMetricsEvent(msg, statsdClient);
            }

            if (success) sentCount++;
        });

        return bot.run(config);
    }
}
