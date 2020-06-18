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
#include <ixcrypto/IXUuid.h>
#include <ixredis/IXRedisServer.h>
#include <ixsnake/IXSnakeServer.h>

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

    class CobraChat
    {
    public:
        CobraChat(const std::string& user,
                  const std::string& session,
                  const ix::CobraConfig& config);

        void subscribe(const std::string& channel);
        void start();
        void stop();
        void run();
        bool isReady() const;

        void sendMessage(const std::string& text);
        size_t getReceivedMessagesCount() const;

        bool hasPendingMessages() const;
        Json::Value popMessage();

    private:
        std::string _user;
        std::string _session;
        ix::CobraConfig _cobraConfig;

        std::queue<Json::Value> _publish_queue;
        mutable std::mutex _queue_mutex;

        std::thread _thread;
        std::atomic<bool> _stop;

        ix::CobraConnection _conn;
        std::atomic<bool> _connectedAndSubscribed;

        std::queue<Json::Value> _receivedQueue;

        std::mutex _logMutex;
    };

    CobraChat::CobraChat(const std::string& user,
                         const std::string& session,
                         const ix::CobraConfig& config)
        : _user(user)
        , _session(session)
        , _cobraConfig(config)
        , _stop(false)
        , _connectedAndSubscribed(false)
    {
    }

    void CobraChat::start()
    {
        _thread = std::thread(&CobraChat::run, this);
    }

    void CobraChat::stop()
    {
        _stop = true;
        _thread.join();
    }

    bool CobraChat::isReady() const
    {
        return _connectedAndSubscribed;
    }

    size_t CobraChat::getReceivedMessagesCount() const
    {
        return _receivedQueue.size();
    }

    bool CobraChat::hasPendingMessages() const
    {
        std::unique_lock<std::mutex> lock(_queue_mutex);
        return !_publish_queue.empty();
    }

    Json::Value CobraChat::popMessage()
    {
        std::unique_lock<std::mutex> lock(_queue_mutex);
        auto msg = _publish_queue.front();
        _publish_queue.pop();
        return msg;
    }

    //
    // Callback to handle received messages, that are printed on the console
    //
    void CobraChat::subscribe(const std::string& channel)
    {
        std::string filter;
        std::string position("$");
        int batchSize = 1;

        _conn.subscribe(channel,
                        filter,
                        position,
                        batchSize,
                        [this](const Json::Value& msg, const std::string& /*position*/) {
                            spdlog::info("receive {}", msg.toStyledString());

                            if (!msg.isObject()) return;
                            if (!msg.isMember("user")) return;
                            if (!msg.isMember("text")) return;
                            if (!msg.isMember("session")) return;

                            std::string msg_user = msg["user"].asString();
                            std::string msg_text = msg["text"].asString();
                            std::string msg_session = msg["session"].asString();

                            // We are not interested in messages
                            // from a different session.
                            if (msg_session != _session) return;

                            // We are not interested in our own messages
                            if (msg_user == _user) return;

                            _receivedQueue.push(msg);

                            std::stringstream ss;
                            ss << std::endl
                               << msg_user << " > " << msg_text << std::endl
                               << _user << " > ";
                            log(ss.str());
                        });
    }

    void CobraChat::sendMessage(const std::string& text)
    {
        Json::Value msg;
        msg["user"] = _user;
        msg["session"] = _session;
        msg["text"] = text;

        std::unique_lock<std::mutex> lock(_queue_mutex);
        _publish_queue.push(msg);
    }

    //
    // Do satori communication on a background thread, where we can have
    // something like an event loop that publish, poll and receive data
    //
    void CobraChat::run()
    {
        std::string channel = _session;

        _conn.configure(_cobraConfig);
        _conn.connect();

        _conn.setEventCallback([this, channel](const CobraEventPtr& event) {
            if (event->type == ix::CobraEventType::Open)
            {
                log("Subscriber connected: " + _user);
                for (auto&& it : event->headers)
                {
                    log("Headers " + it.first + " " + it.second);
                }
            }
            else if (event->type == ix::CobraEventType::Authenticated)
            {
                log("Subscriber authenticated: " + _user);
                subscribe(channel);
            }
            else if (event->type == ix::CobraEventType::Error)
            {
                log(event->errMsg + _user);
            }
            else if (event->type == ix::CobraEventType::Closed)
            {
                log("Connection closed: " + _user);
            }
            else if (event->type == ix::CobraEventType::Subscribed)
            {
                log("Subscription ok: " + _user + " subscription_id " + event->subscriptionId);
                _connectedAndSubscribed = true;
            }
            else if (event->type == ix::CobraEventType::UnSubscribed)
            {
                log("Unsubscription ok: " + _user + " subscription_id " + event->subscriptionId);
            }
            else if (event->type == ix::CobraEventType::Published)
            {
                TLogger() << "Subscriber: published message acked: " << event->msgId;
            }
        });

        while (!_stop)
        {
            {
                while (hasPendingMessages())
                {
                    auto msg = popMessage();

                    std::string text = msg["text"].asString();

                    std::stringstream ss;
                    ss << "Sending msg [" << text << "]";
                    log(ss.str());

                    Json::Value channels;
                    channels.append(channel);
                    _conn.publish(channels, msg);
                }
            }

            ix::msleep(50);
        }

        _conn.unsubscribe(channel);

        ix::msleep(50);
        _conn.disconnect();

        _conn.setEventCallback([](const CobraEventPtr& /*event*/) {});
    }
} // namespace

TEST_CASE("Cobra_chat", "[cobra_chat]")
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

        int timeout;
        setupTrafficTrackerCallback();

        std::string session = ix::generateSessionId();
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

        CobraChat chatA("jean", session, config);
        CobraChat chatB("paul", session, config);

        chatA.start();
        chatB.start();

        // Wait for all chat instance to be ready
        timeout = 10 * 1000; // 10s
        while (true)
        {
            if (chatA.isReady() && chatB.isReady()) break;
            ix::msleep(10);

            timeout -= 10;
            if (timeout <= 0)
            {
                snakeServer.stop();
                redisServer.stop();
                REQUIRE(false); // timeout
            }
        }

        // Add a bit of extra time, for the subscription to be active
        ix::msleep(1000);

        chatA.sendMessage("from A1");
        chatA.sendMessage("from A2");
        chatA.sendMessage("from A3");

        chatB.sendMessage("from B1");
        chatB.sendMessage("from B2");

        // 1. Wait for all messages to be sent
        timeout = 10 * 1000; // 10s
        while (chatA.hasPendingMessages() || chatB.hasPendingMessages())
        {
            ix::msleep(10);

            timeout -= 10;
            if (timeout <= 0)
            {
                snakeServer.stop();
                redisServer.stop();
                REQUIRE(false); // timeout
            }
        }

        // Give us 1s for all messages to be received
        ix::msleep(1000);

        chatA.stop();
        chatB.stop();

        REQUIRE(chatA.getReceivedMessagesCount() == 2);
        REQUIRE(chatB.getReceivedMessagesCount() == 3);

        spdlog::info("Incoming bytes {}", incomingBytes);
        spdlog::info("Outgoing bytes {}", outgoingBytes);

        spdlog::info("Stopping snake server...");
        snakeServer.stop();

        spdlog::info("Stopping redis server...");
        redisServer.stop();
    }
}
