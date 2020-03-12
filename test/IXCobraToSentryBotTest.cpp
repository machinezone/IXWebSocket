/*
 *  cmd_satori_chat.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <chrono>
#include <iostream>
#include <ixcobra/IXCobraConnection.h>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <ixcrypto/IXUuid.h>
#include <ixsnake/IXRedisServer.h>
#include <ixsnake/IXSnakeServer.h>
#include <ixbots/IXCobraToSentryBot.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXUserAgent.h>

using namespace ix;

namespace
{
    std::atomic<size_t> incomingBytes(0);
    std::atomic<size_t> outgoingBytes(0);

    void setupTrafficTrackerCallback()
    {
        ix::CobraConnection::setTrafficTrackerCallback([](size_t size, bool incoming) {
            if (incoming)
            {
                incomingBytes += size;
            }
            else
            {
                outgoingBytes += size;
            }
        });
    }

    void runPublisher(const ix::CobraConfig& config, const std::string& channel)
    {
        ix::CobraMetricsPublisher cobraMetricsPublisher;

        SocketTLSOptions socketTLSOptions;
        bool perMessageDeflate = true;
        cobraMetricsPublisher.configure(config.appkey,
                                        config.endpoint,
                                        channel,
                                        config.rolename,
                                        config.rolesecret,
                                        perMessageDeflate,
                                        socketTLSOptions);
        cobraMetricsPublisher.setSession(uuid4());
        cobraMetricsPublisher.enable(true); // disabled by default, needs to be enabled to be active

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
}

TEST_CASE("Cobra_to_sentry_bot", "[foo]")
{
    SECTION("Exchange and count sent/received messages.")
    {
        int port = getFreePort();
        snake::AppConfig appConfig = makeSnakeServerConfig(port);

        // Start a redis server
        ix::RedisServer redisServer(appConfig.redisPort);
        auto res = redisServer.listen();
        REQUIRE(res.first);
        redisServer.start();

        // Start a snake server
        snake::SnakeServer snakeServer(appConfig);
        snakeServer.run();

        // Start a fake sentry http server
        int sentryPort = getFreePort();
        ix::HttpServer sentryServer(sentryPort, "127.0.0.1");
        sentryServer.setOnConnectionCallback(
            [](HttpRequestPtr request,
                          std::shared_ptr<ConnectionState> /*connectionState*/) -> HttpResponsePtr {
                WebSocketHttpHeaders headers;
                headers["Server"] = userAgent();

                // Log request
                std::stringstream ss;
                ss << request->method << " " << request->headers["User-Agent"] << " "
                   << request->uri;

                if (request->method == "POST")
                {
                    return std::make_shared<HttpResponse>(
                        200, "OK", HttpErrorCode::Ok, headers, std::string());
                }
                else
                {
                    return std::make_shared<HttpResponse>(
                        405, "OK", HttpErrorCode::Invalid, headers, std::string("Invalid method"));
                }
            });

        res = sentryServer.listen();
        REQUIRE(res.first);
        sentryServer.start();

        setupTrafficTrackerCallback();

        // Run the bot for a small amount of time
        std::string channel = ix::generateSessionId();
        std::string appkey("FC2F10139A2BAc53BB72D9db967b024f");
        std::string role = "_sub";
        std::string secret = "66B1dA3ED5fA074EB5AE84Dd8CE3b5ba";
        
        std::stringstream ss;
        ss << "ws://localhost:" << port;
        std::string endpoint = ss.str();

        ix::CobraConfig config;
        config.endpoint = endpoint;
        config.appkey = appkey;
        config.rolename = role;
        config.rolesecret = secret;

        std::thread publisherThread(runPublisher, config, channel);

        std::string filter;
        bool verbose = true;
        bool strict = true;
        int jobs = 1;
        size_t maxQueueSize = 10;
        bool enableHeartbeat = false;

        // https://xxxxx:yyyyyy@sentry.io/1234567
        std::stringstream oss;
        std::string scheme("http://");

        oss << scheme << "xxxxxxx:yyyyyyy@localhost:" << sentryPort << "/1234567";
        std::string dsn = oss.str();

        // Only run the bot for 3 seconds
        int runtime = 3;

        int sentCount = cobra_to_sentry_bot(config, channel, filter, dsn,
                                            verbose, strict, jobs,
                                            maxQueueSize, enableHeartbeat, runtime);
        //
        // We want at least 2 messages to be sent
        //
        REQUIRE(sentCount >= 2);

        // Give us 1s for all messages to be received
        ix::msleep(1000);

        spdlog::info("Incoming bytes {}", incomingBytes);
        spdlog::info("Outgoing bytes {}", outgoingBytes);

        spdlog::info("Stopping snake server...");
        snakeServer.stop();

        spdlog::info("Stopping redis server...");
        redisServer.stop();

        publisherThread.join();
        sentryServer.stop();
    }
}
