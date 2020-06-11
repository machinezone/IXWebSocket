/*
 *  IXCobraToStdoutTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <chrono>
#include <iostream>
#include <ixbots/IXCobraToStdoutBot.h>
#include <ixcobra/IXCobraConnection.h>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <ixcrypto/IXUuid.h>
#include <ixsentry/IXSentryClient.h>
#include <ixredis/IXRedisServer.h>
#include <ixsnake/IXSnakeServer.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXUserAgent.h>

using namespace ix;

namespace
{
    void runPublisher(const ix::CobraConfig& config, const std::string& channel)
    {
        ix::CobraMetricsPublisher cobraMetricsPublisher;
        cobraMetricsPublisher.configure(config, channel);
        cobraMetricsPublisher.setSession(uuid4());
        cobraMetricsPublisher.enable(true);

        Json::Value msg;
        msg["fps"] = 60;

        cobraMetricsPublisher.setGenericAttributes("game", "ody");

        // Wait a bit
        ix::msleep(500);

        // publish some messages
        cobraMetricsPublisher.push("sms_metric_A_id", msg); // (msg #1)
        cobraMetricsPublisher.push("sms_metric_B_id", msg); // (msg #2)
        ix::msleep(500);

        cobraMetricsPublisher.push("sms_metric_A_id", msg); // (msg #3)
        cobraMetricsPublisher.push("sms_metric_D_id", msg); // (msg #4)
        ix::msleep(500);

        cobraMetricsPublisher.push("sms_metric_A_id", msg); // (msg #4)
        cobraMetricsPublisher.push("sms_metric_F_id", msg); // (msg #5)
        ix::msleep(500);
    }
} // namespace

TEST_CASE("Cobra_to_stdout_bot", "[cobra_bots]")
{
    SECTION("Exchange and count sent/received messages.")
    {
        int port = getFreePort();
        snake::AppConfig appConfig = makeSnakeServerConfig(port, true);

        // Start a redis server
        ix::RedisServer redisServer(appConfig.redisPort);
        auto res = redisServer.listen();
        REQUIRE(res.first);
        redisServer.start();

        // Start a snake server
        snake::SnakeServer snakeServer(appConfig);
        snakeServer.run();

        // Run the bot for a small amount of time
        std::string channel = ix::generateSessionId();
        std::string appkey("FC2F10139A2BAc53BB72D9db967b024f");
        std::string role = "_sub";
        std::string secret = "66B1dA3ED5fA074EB5AE84Dd8CE3b5ba";
        std::string endpoint = makeCobraEndpoint(port, true);

        ix::CobraConfig config;
        config.endpoint = endpoint;
        config.appkey = appkey;
        config.rolename = role;
        config.rolesecret = secret;
        config.socketTLSOptions = makeClientTLSOptions();

        std::thread publisherThread(runPublisher, config, channel);

        ix::CobraBotConfig cobraBotConfig;
        cobraBotConfig.cobraConfig = config;
        cobraBotConfig.channel = channel;
        cobraBotConfig.runtime = 3; // Only run the bot for 3 seconds
        cobraBotConfig.enableHeartbeat = false;
        bool quiet = false;

        // We could try to capture the output ... not sure how.
        bool fluentd = true;

        int64_t sentCount = ix::cobra_to_stdout_bot(cobraBotConfig, fluentd, quiet);
        //
        // We want at least 2 messages to be sent
        //
        REQUIRE(sentCount >= 2);

        // Give us 1s for all messages to be received
        ix::msleep(1000);

        spdlog::info("Stopping snake server...");
        snakeServer.stop();

        spdlog::info("Stopping redis server...");
        redisServer.stop();

        publisherThread.join();
    }
}
