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
#include <map>
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
        ss << msg["id"].asString() << "."
           << msg["device"]["game"].asString() << "."
           << msg["device"]["os_name"].asString() << "."
           << removeSpaces(msg["data"]["Tag"].asString());

        std::string id = ss.str();

        statsdClient.gauge(id + ".lt_30", gt_60 + lt_60 + lt_30);
        statsdClient.gauge(id + ".lt_20", lt_20);
        statsdClient.gauge(id + ".lt_15", lt_15);
        statsdClient.gauge(id + ".lt_12", lt_12);
        statsdClient.gauge(id + ".lt_10", lt_10);
        statsdClient.gauge(id + ".lt_08", lt_08);

        return true;
    }

    std::string getDeviceIdentifier(const Json::Value& msg)
    {
        std::string deviceId("na");

        auto osName = msg["device"]["os_name"];
        if (osName == "Android")
        {
            deviceId = msg["device"]["model"].asString();
        }
        else if (osName == "iOS")
        {
            deviceId = msg["device"]["hardware_model"].asString();
        }

        return deviceId;
    }

    bool processPerfMetricsEventSlowFrames(const Json::Value& msg,
                                           StatsdClient& statsdClient,
                                           std::map<std::string, int>& deviceIdCounters,
                                           std::atomic<uint64_t>& sentCount)
    {
        auto frameRateHistogramCounts = msg["data"]["FrameRateHistogramCounts"];

        int slowFrames = 0;
        slowFrames += frameRateHistogramCounts[4].asInt();
        slowFrames += frameRateHistogramCounts[5].asInt();
        slowFrames += frameRateHistogramCounts[6].asInt();
        slowFrames += frameRateHistogramCounts[7].asInt();

        std::stringstream ss;
        ss << msg["id"].asString() << "_slow_frames" << "."
           << msg["device"]["game"].asString() << "."
           << msg["device"]["os_name"].asString() << "."
           << removeSpaces(msg["data"]["Tag"].asString());

        std::string id = ss.str();
        statsdClient.gauge(id, slowFrames);

        // extract device model names for common devices
        auto deviceId = getDeviceIdentifier(msg);
        deviceIdCounters[deviceId]++;

        if (deviceId == "N841AP" || deviceId == "SM-N960U")
        {
            ss.str(""); // reset the stringstream
            ss << msg["id"].asString() << "_slow_frames_by_device" << "."
               << deviceId << "."
               << msg["device"]["game"].asString() << "."
               << msg["device"]["os_name"].asString() << "."
               << removeSpaces(msg["data"]["Tag"].asString());

            std::string id = ss.str();
            statsdClient.gauge(id, slowFrames);
        }

        // periodically display all device ids
        if (sentCount % 1000 == 0)
        {
            ss.str(""); // reset the stringstream
            ss << "## " << deviceIdCounters.size() << " unique device ids ##" << std::endl;
            for (auto&& it : deviceIdCounters)
            {
                ss << it.first << " => " << it.second << std::endl;
            }
            CoreLogger::info(ss.str());
        }

        return true;
    }

    int64_t cobra_metrics_to_statsd_bot(const ix::CobraBotConfig& config,
                                        StatsdClient& statsdClient,
                                        bool verbose)
    {
        CobraBot bot;
        std::map<std::string, int> deviceIdCounters;

        bot.setOnBotMessageCallback(
            [&statsdClient, &verbose, &deviceIdCounters]
                                     (const Json::Value& msg,
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
                success |= processPerfMetricsEventSlowFrames(msg, statsdClient, deviceIdCounters, sentCount);
            }

            if (success) sentCount++;
        });

        return bot.run(config);
    }
}
