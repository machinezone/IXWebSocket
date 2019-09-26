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

    //
    // This project / appkey is configure on cobra to not do any batching.
    // This way we can start a subscriber and receive all messages as they come in.
    //
    std::string APPKEY("FC2F10139A2BAc53BB72D9db967b024f");
    std::string CHANNEL("unittest_channel");
    std::string PUBLISHER_ROLE("_pub");
    std::string PUBLISHER_SECRET("1c04DB8fFe76A4EeFE3E318C72d771db");
    std::string SUBSCRIBER_ROLE("_sub");
    std::string SUBSCRIBER_SECRET("66B1dA3ED5fA074EB5AE84Dd8CE3b5ba");

    std::atomic<bool> gStop;
    std::atomic<bool> gSubscriberConnectedAndSubscribed;
    std::atomic<size_t> gUniqueMessageIdsCount;
    std::atomic<int> gMessageCount;

    std::set<std::string> gIds;
    std::mutex gProtectIds; // std::set is no thread-safe, so protect access with this mutex.

    //
    // Background thread subscribe to the channel and validates what was sent
    //
    void startSubscriber(const std::string& endpoint)
    {
        gSubscriberConnectedAndSubscribed = false;
        gUniqueMessageIdsCount = 0;
        gMessageCount = 0;

        ix::CobraConnection conn;
        conn.configure(APPKEY,
                       endpoint,
                       SUBSCRIBER_ROLE,
                       SUBSCRIBER_SECRET,
                       ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        conn.setEventCallback([&conn](ix::CobraConnectionEventType eventType,
                                      const std::string& errMsg,
                                      const ix::WebSocketHttpHeaders& headers,
                                      const std::string& subscriptionId,
                                      CobraConnection::MsgId msgId) {
            if (eventType == ix::CobraConnection_EventType_Open)
            {
                Logger() << "Subscriber connected:";
                for (auto&& it : headers)
                {
                    log("Headers " + it.first + " " + it.second);
                }
            }
            if (eventType == ix::CobraConnection_EventType_Error)
            {
                Logger() << "Subscriber error:" << errMsg;
            }
            else if (eventType == ix::CobraConnection_EventType_Authenticated)
            {
                log("Subscriber authenticated");
                std::string filter;
                conn.subscribe(CHANNEL, filter, [](const Json::Value& msg) {
                    log(msg.toStyledString());

                    std::string id = msg["id"].asString();
                    {
                        std::lock_guard<std::mutex> guard(gProtectIds);
                        gIds.insert(id);
                    }

                    gMessageCount++;
                });
            }
            else if (eventType == ix::CobraConnection_EventType_Subscribed)
            {
                Logger() << "Subscriber: subscribed to channel " << subscriptionId;
                if (subscriptionId == CHANNEL)
                {
                    gSubscriberConnectedAndSubscribed = true;
                }
                else
                {
                    Logger() << "Subscriber: unexpected channel " << subscriptionId;
                }
            }
            else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
            {
                Logger() << "Subscriber: ununexpected from channel " << subscriptionId;
                if (subscriptionId != CHANNEL)
                {
                    Logger() << "Subscriber: unexpected channel " << subscriptionId;
                }
            }
            else if (eventType == ix::CobraConnection_EventType_Published)
            {
                Logger() << "Subscriber: published message acked: " << msgId;
            }
        });

        while (!gStop)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
        }

        conn.unsubscribe(CHANNEL);
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
    snake::AppConfig appConfig = makeSnakeServerConfig(port);

    // Start a redis server
    ix::RedisServer redisServer(appConfig.redisPort);
    auto res = redisServer.listen();
    REQUIRE(res.first);
    redisServer.start();

    // Start a snake server
    snake::SnakeServer snakeServer(appConfig);
    snakeServer.run();

    setupTrafficTrackerCallback();

    std::stringstream ss;
    ss << "ws://localhost:" << port;
    std::string endpoint = ss.str();

    // Make channel name unique
    CHANNEL += uuid4();

    gStop = false;
    std::thread bgThread(&startSubscriber, endpoint);

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

    bool perMessageDeflate = true;
    cobraMetricsPublisher.configure(
        APPKEY, endpoint, CHANNEL, PUBLISHER_ROLE, PUBLISHER_SECRET, perMessageDeflate);
    cobraMetricsPublisher.setSession(uuid4());
    cobraMetricsPublisher.enable(true); // disabled by default, needs to be enabled to be active

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
    bgThread.join();

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
