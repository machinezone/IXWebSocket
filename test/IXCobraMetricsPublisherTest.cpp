/*
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <iostream>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <ixcrypto/IXUuid.h>
#include <ixsnake/IXRedisServer.h>
#include <ixsnake/IXSnakeServer.h>
#include <set>

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

    std::atomic<bool> gStop;
    std::atomic<bool> gSubscriberConnectedAndSubscribed;
    std::atomic<size_t> gUniqueMessageIdsCount;
    std::atomic<int> gMessageCount;

    std::set<std::string> gIds;
    std::mutex gProtectIds; // std::set is no thread-safe, so protect access with this mutex.

    //
    // Background thread subscribe to the channel and validates what was sent
    //
    void startSubscriber(const ix::CobraConfig& config, const std::string& channel)
    {
        gSubscriberConnectedAndSubscribed = false;
        gUniqueMessageIdsCount = 0;
        gMessageCount = 0;

        ix::CobraConnection conn;
        conn.configure(config);
        conn.connect();

        conn.setEventCallback([&conn, &channel](const CobraEventPtr& event) {
            if (event->type == ix::CobraEventType::Open)
            {
                TLogger() << "Subscriber connected:";
                for (auto&& it : event->headers)
                {
                    log("Headers " + it.first + " " + it.second);
                }
            }
            else if (event->type == ix::CobraEventType::Closed)
            {
                TLogger() << "Subscriber closed:" << event->errMsg;
            }
            else if (event->type == ix::CobraEventType::Error)
            {
                TLogger() << "Subscriber error:" << event->errMsg;
            }
            else if (event->type == ix::CobraEventType::Authenticated)
            {
                log("Subscriber authenticated");
                std::string filter;
                std::string position("$");

                conn.subscribe(channel,
                               filter,
                               position,
                               [](const Json::Value& msg, const std::string& /*position*/) {
                                   log(msg.toStyledString());

                                   std::string id = msg["id"].asString();
                                   {
                                       std::lock_guard<std::mutex> guard(gProtectIds);
                                       gIds.insert(id);
                                   }

                                   gMessageCount++;
                               });
            }
            else if (event->type == ix::CobraEventType::Subscribed)
            {
                TLogger() << "Subscriber: subscribed to channel " << event->subscriptionId;
                if (event->subscriptionId == channel)
                {
                    gSubscriberConnectedAndSubscribed = true;
                }
                else
                {
                    TLogger() << "Subscriber: unexpected channel " << event->subscriptionId;
                }
            }
            else if (event->type == ix::CobraEventType::UnSubscribed)
            {
                TLogger() << "Subscriber: ununexpected from channel " << event->subscriptionId;
                if (event->subscriptionId != channel)
                {
                    TLogger() << "Subscriber: unexpected channel " << event->subscriptionId;
                }
            }
            else if (event->type == ix::CobraEventType::Published)
            {
                TLogger() << "Subscriber: published message acked: " << event->msgId;
            }
        });

        while (!gStop)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
        }

        conn.unsubscribe(channel);
        conn.disconnect();

        gUniqueMessageIdsCount = gIds.size();
    }

    // publish 100 messages, during roughly 100ms
    // this is used to test thread safety of CobraMetricsPublisher::push
    void runAdditionalPublisher(ix::CobraMetricsPublisher* cobraMetricsPublisher)
    {
        Json::Value data;
        data["foo"] = "bar";

        for (int i = 0; i < 100; ++i)
        {
            cobraMetricsPublisher->push("sms_metric_F_id", data);
            ix::msleep(1);
        }
    }

} // namespace

TEST_CASE("Cobra_Metrics_Publisher", "[cobra]")
{
    int port = getFreePort();
    bool preferTLS = true;
    snake::AppConfig appConfig = makeSnakeServerConfig(port, preferTLS);

    // Start a redis server
    ix::RedisServer redisServer(appConfig.redisPort);
    auto res = redisServer.listen();
    REQUIRE(res.first);
    redisServer.start();

    // Start a snake server
    snake::SnakeServer snakeServer(appConfig);
    snakeServer.run();

    setupTrafficTrackerCallback();

    std::string channel = ix::generateSessionId();
    std::string endpoint = makeCobraEndpoint(port, preferTLS);
    std::string appkey("FC2F10139A2BAc53BB72D9db967b024f");
    std::string role = "_sub";
    std::string secret = "66B1dA3ED5fA074EB5AE84Dd8CE3b5ba";

    ix::CobraConfig config;
    config.endpoint = endpoint;
    config.appkey = appkey;
    config.rolename = role;
    config.rolesecret = secret;
    config.socketTLSOptions = makeClientTLSOptions();

    gStop = false;
    std::thread subscriberThread(&startSubscriber, config, channel);

    int timeout = 10 * 1000; // 10s

    // Wait until the subscriber is ready (authenticated + subscription successful)
    while (!gSubscriberConnectedAndSubscribed)
    {
        std::chrono::duration<double, std::milli> duration(10);
        std::this_thread::sleep_for(duration);

        timeout -= 10;
        if (timeout <= 0)
        {
            snakeServer.stop();
            redisServer.stop();
            REQUIRE(false); // timeout
        }
    }

    ix::CobraMetricsPublisher cobraMetricsPublisher;
    cobraMetricsPublisher.configure(config, channel);
    cobraMetricsPublisher.setSession(uuid4());
    cobraMetricsPublisher.enable(true);

    Json::Value data;
    data["foo"] = "bar";

    // (1) Publish without restrictions
    cobraMetricsPublisher.push("sms_metric_A_id", data); // (msg #1)
    cobraMetricsPublisher.push("sms_metric_B_id", data); // (msg #2)

    // (2) Restrict what is sent using a blacklist
    // Add one entry to the blacklist
    // (will send msg #3)
    cobraMetricsPublisher.setBlacklist({
        "sms_metric_B_id" // this id will not be sent
    });
    // (msg #4)
    cobraMetricsPublisher.push("sms_metric_A_id", data);
    // ...
    cobraMetricsPublisher.push("sms_metric_B_id", data); // this won't be sent

    // Reset the blacklist
    // (msg #5)
    cobraMetricsPublisher.setBlacklist({}); // 4.

    // (3) Restrict what is sent using rate control

    // (msg #6)
    cobraMetricsPublisher.setRateControl({
        {"sms_metric_C_id", 1}, // published once per minute (60 seconds) max
    });
    // (msg #7)
    cobraMetricsPublisher.push("sms_metric_C_id", data);
    cobraMetricsPublisher.push("sms_metric_C_id", data); // this won't be sent

    ix::msleep(1400);

    // (msg #8)
    cobraMetricsPublisher.push("sms_metric_C_id", data); // now this will be sent

    ix::msleep(600); // wait a bit so that the last message is sent and can be received

    log("Testing suspend/resume now, which will disconnect the cobraMetricsPublisher.");

    // Test suspend + resume
    for (int i = 0; i < 3; ++i)
    {
        cobraMetricsPublisher.suspend();
        ix::msleep(500);
        REQUIRE(!cobraMetricsPublisher.isConnected()); // Check that we are not connected anymore

        cobraMetricsPublisher.push("sms_metric_D_id", data); // will not be sent this time

        cobraMetricsPublisher.resume();
        ix::msleep(2000);                             // give cobra 2s to connect
        REQUIRE(cobraMetricsPublisher.isConnected()); // Check that we are connected now

        cobraMetricsPublisher.push("sms_metric_E_id", data);
    }

    ix::msleep(500);

    // Test multi-threaded publish
    std::thread bgPublisher1(&runAdditionalPublisher, &cobraMetricsPublisher);
    std::thread bgPublisher2(&runAdditionalPublisher, &cobraMetricsPublisher);
    std::thread bgPublisher3(&runAdditionalPublisher, &cobraMetricsPublisher);
    std::thread bgPublisher4(&runAdditionalPublisher, &cobraMetricsPublisher);
    std::thread bgPublisher5(&runAdditionalPublisher, &cobraMetricsPublisher);

    bgPublisher1.join();
    bgPublisher2.join();
    bgPublisher3.join();
    bgPublisher4.join();
    bgPublisher5.join();

    // Now stop the thread
    gStop = true;
    subscriberThread.join();

    //
    // Validate that we received all message kinds, and the correct number of messages
    //
    CHECK(gIds.count("sms_metric_A_id") == 1);
    CHECK(gIds.count("sms_metric_B_id") == 1);
    CHECK(gIds.count("sms_metric_C_id") == 1);
    CHECK(gIds.count("sms_metric_D_id") == 1);
    CHECK(gIds.count("sms_metric_E_id") == 1);
    CHECK(gIds.count("sms_metric_F_id") == 1);
    CHECK(gIds.count("sms_set_rate_control_id") == 1);
    CHECK(gIds.count("sms_set_blacklist_id") == 1);

    spdlog::info("Incoming bytes {}", incomingBytes);
    spdlog::info("Outgoing bytes {}", outgoingBytes);

    spdlog::info("Stopping snake server...");
    snakeServer.stop();

    spdlog::info("Stopping redis server...");
    redisServer.stop();
}
