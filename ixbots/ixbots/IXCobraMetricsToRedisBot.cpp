/*
 *  IXCobraMetricsToRedisBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraMetricsToRedisBot.h"

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
    bool processPerfMetricsEventSlowFrames(const Json::Value& msg,
                                           RedisClient& redisClient,
                                           const std::string& deviceId)
    {
        auto frameRateHistogramCounts = msg["data"]["FrameRateHistogramCounts"];

        int slowFrames = 0;
        slowFrames += frameRateHistogramCounts[4].asInt();
        slowFrames += frameRateHistogramCounts[5].asInt();
        slowFrames += frameRateHistogramCounts[6].asInt();
        slowFrames += frameRateHistogramCounts[7].asInt();

        //
        // XADD without a device id
        //
        std::stringstream ss;
        ss << msg["id"].asString() << "_slow_frames" << "."
           << msg["device"]["game"].asString() << "."
           << msg["device"]["os_name"].asString() << "."
           << removeSpaces(msg["data"]["Tag"].asString());

        int maxLen;
        maxLen = 100000;
        std::string id = ss.str();
        std::string errMsg;
        if (redisClient.xadd(id, std::to_string(slowFrames), maxLen, errMsg).empty())
        {
            CoreLogger::info(std::string("redis XADD error: ") + errMsg);
        }

        //
        // XADD with a device id
        //
        ss.str(""); // reset the stringstream
        ss << msg["id"].asString() << "_slow_frames_by_device" << "."
           << deviceId << "."
           << msg["device"]["game"].asString() << "."
           << msg["device"]["os_name"].asString() << "."
           << removeSpaces(msg["data"]["Tag"].asString());

        id = ss.str();
        maxLen = 1000;
        if (redisClient.xadd(id, std::to_string(slowFrames), maxLen, errMsg).empty())
        {
            CoreLogger::info(std::string("redis XADD error: ") + errMsg);
        }

        //
        // Add device to the device zset, and increment the score
        // so that we know which devices are used more than others
        // ZINCRBY myzset 1 one
        //
        ss.str(""); // reset the stringstream
        ss << msg["id"].asString() << "_slow_frames_devices" << "."
           << msg["device"]["game"].asString();

        id = ss.str();
        std::vector<std::string> args = {
            "ZINCRBY", id, "1", deviceId
        };
        auto response = redisClient.send(args, errMsg);
        if (response.first == RespType::Error)
        {
            CoreLogger::info(std::string("redis ZINCRBY error: ") + errMsg);
        }

        return true;
    }

    int64_t cobra_metrics_to_redis_bot(const ix::CobraBotConfig& config,
                                       RedisClient& redisClient,
                                       bool verbose)
    {
        CobraBot bot;

        bot.setOnBotMessageCallback(
            [&redisClient, &verbose, &bot]
             (const Json::Value& msg,
              const std::string& /*position*/,
              std::atomic<bool>& /*throttled*/,
              std::atomic<bool>& /*fatalCobraError*/,
              std::atomic<uint64_t>& sentCount) -> void {
            if (msg["device"].isNull())
            {
                CoreLogger::info("no device entry, skipping event");
                return;
            }

            if (msg["id"].isNull())
            {
                CoreLogger::info("no id entry, skipping event");
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
                auto deviceId = bot.getDeviceIdentifier(msg);
                success = processPerfMetricsEventSlowFrames(msg, redisClient, deviceId);
            }

            if (success) sentCount++;
        });

        return bot.run(config);
    }
} // namespace ix
