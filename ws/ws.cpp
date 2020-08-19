/*
 *  ws.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

//
// Main driver for websocket utilities
//

#include "IXBench.h"
#include "linenoise.hpp"
#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <cli11/CLI11.hpp>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <ixbots/IXCobraMetricsToRedisBot.h>
#include <ixbots/IXCobraToPythonBot.h>
#include <ixbots/IXCobraToSentryBot.h>
#include <ixbots/IXCobraToStatsdBot.h>
#include <ixbots/IXCobraToStdoutBot.h>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <ixcrypto/IXBase64.h>
#include <ixcrypto/IXHash.h>
#include <ixcrypto/IXUuid.h>
#include <ixredis/IXRedisClient.h>
#include <ixredis/IXRedisServer.h>
#include <ixsentry/IXSentryClient.h>
#include <ixsnake/IXSnakeServer.h>
#include <ixwebsocket/IXDNSLookup.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSetThreadName.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXUserAgent.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <ixwebsocket/IXWebSocketProxyServer.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <msgpack11/msgpack11.hpp>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <signal.h>
#else
#include <process.h>
#define getpid _getpid
#endif

// for convenience
using json = nlohmann::json;
using msgpack11::MsgPack;

namespace
{
    std::pair<bool, std::vector<uint8_t>> load(const std::string& path)
    {
        std::vector<uint8_t> memblock;

        std::ifstream file(path);
        if (!file.is_open()) return std::make_pair(false, memblock);

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        memblock.resize((size_t) size);
        file.read((char*) &memblock.front(), static_cast<std::streamsize>(size));

        return std::make_pair(true, memblock);
    }

    std::pair<bool, std::string> readAsString(const std::string& path)
    {
        auto res = load(path);
        auto vec = res.second;
        return std::make_pair(res.first, std::string(vec.begin(), vec.end()));
    }

    // Assume the file exists
    std::string readBytes(const std::string& path)
    {
        std::vector<uint8_t> memblock;
        std::ifstream file(path);

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        memblock.resize(size);

        file.read((char*) &memblock.front(), static_cast<std::streamsize>(size));

        std::string bytes(memblock.begin(), memblock.end());
        return bytes;
    }

    std::string truncate(const std::string& str, size_t n)
    {
        if (str.size() < n)
        {
            return str;
        }
        else
        {
            return str.substr(0, n) + "...";
        }
    }

    bool fileExists(const std::string& fileName)
    {
        std::ifstream infile(fileName);
        return infile.good();
    }
} // namespace

namespace ix
{
    //
    // Autobahn test suite
    //
    // 1. First you need to generate a config file in a config folder,
    //    which can use a white list of test to execute (with globbing),
    //    or a black list of tests to ignore
    //
    // config/fuzzingserver.json
    // {
    //     "url": "ws://127.0.0.1:9001",
    //     "outdir": "./reports/clients",
    //     "cases": ["2.*"],
    //     "exclude-cases": [
    //     ],
    //     "exclude-agent-cases": {}
    // }
    //
    //
    // 2 Run the test server (using docker)
    // docker run -it --rm -v "${PWD}/config:/config" -v "${PWD}/reports:/reports" -p 9001:9001
    // --name fuzzingserver crossbario/autobahn-testsuite
    //
    // 3. Run this command
    //    ws autobahn -q --url ws://localhost:9001
    //
    // 4. A HTML report will be generated, you can inspect it to see if you are compliant or not
    //

    class AutobahnTestCase
    {
    public:
        AutobahnTestCase(const std::string& _url, bool quiet);
        void run();

    private:
        void log(const std::string& msg);

        std::string _url;
        ix::WebSocket _webSocket;

        bool _quiet;

        std::mutex _mutex;
        std::condition_variable _condition;
    };

    AutobahnTestCase::AutobahnTestCase(const std::string& url, bool quiet)
        : _url(url)
        , _quiet(quiet)
    {
        _webSocket.disableAutomaticReconnection();

        // FIXME: this should be on by default
        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            true, false, false, 15, 15);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);
    }

    void AutobahnTestCase::log(const std::string& msg)
    {
        if (!_quiet)
        {
            spdlog::info(msg);
        }
    }

    void AutobahnTestCase::run()
    {
        _webSocket.setUrl(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                log("autobahn: connected");
                ss << "Uri: " << msg->openInfo.uri << std::endl;
                ss << "Handshake Headers:" << std::endl;
                for (auto it : msg->openInfo.headers)
                {
                    ss << it.first << ": " << it.second << std::endl;
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "autobahn: connection closed:";
                ss << " code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason << std::endl;

                _condition.notify_one();
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                ss << "Received " << msg->wireSize << " bytes" << std::endl;

                ss << "autobahn: received message: " << truncate(msg->str, 40) << std::endl;

                _webSocket.send(msg->str, msg->binary);
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;

                // And error can happen, in which case the test-case is marked done
                _condition.notify_one();
            }
            else if (msg->type == ix::WebSocketMessageType::Fragment)
            {
                ss << "Received message fragment" << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Ping)
            {
                ss << "Received ping" << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                ss << "Received pong" << std::endl;
            }
            else
            {
                ss << "Invalid ix::WebSocketMessageType" << std::endl;
            }

            log(ss.str());
        });

        _webSocket.start();

        log("Waiting for test completion ...");
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock);

        _webSocket.stop();
    }

    bool generateReport(const std::string& url)
    {
        ix::WebSocket webSocket;
        std::string reportUrl(url);
        reportUrl += "/updateReports?agent=ixwebsocket";
        webSocket.setUrl(reportUrl);
        webSocket.disableAutomaticReconnection();

        std::atomic<bool> success(true);
        std::condition_variable condition;

        webSocket.setOnMessageCallback([&condition, &success](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Close)
            {
                spdlog::info("Report generated");
                condition.notify_one();
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                spdlog::info(ss.str());

                success = false;
            }
        });

        webSocket.start();
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock);
        webSocket.stop();

        if (!success)
        {
            spdlog::error("Cannot generate report at url {}", reportUrl);
        }

        return success;
    }

    int getTestCaseCount(const std::string& url)
    {
        ix::WebSocket webSocket;
        std::string caseCountUrl(url);
        caseCountUrl += "/getCaseCount";
        webSocket.setUrl(caseCountUrl);
        webSocket.disableAutomaticReconnection();

        int count = -1;
        std::condition_variable condition;

        webSocket.setOnMessageCallback([&condition, &count](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Close)
            {
                condition.notify_one();
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::stringstream ss;
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                spdlog::info(ss.str());

                condition.notify_one();
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                // response is a string
                std::stringstream ss;
                ss << msg->str;
                ss >> count;
            }
        });

        webSocket.start();
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock);
        webSocket.stop();

        if (count == -1)
        {
            spdlog::error("Cannot retrieve test case count at url {}", caseCountUrl);
        }

        return count;
    }

    //
    // make && bench ws autobahn --url ws://localhost:9001
    //
    int ws_autobahn_main(const std::string& url, bool quiet)
    {
        int testCasesCount = getTestCaseCount(url);
        spdlog::info("Test cases count: {}", testCasesCount);

        if (testCasesCount == -1)
        {
            spdlog::error("Cannot retrieve test case count at url {}", url);
            return 1;
        }

        testCasesCount++;

        for (int i = 1; i < testCasesCount; ++i)
        {
            spdlog::info("Execute test case {}", i);

            int caseNumber = i;

            std::stringstream ss;
            ss << url << "/runCase?case=" << caseNumber << "&agent=ixwebsocket";

            std::string url(ss.str());

            AutobahnTestCase testCase(url, quiet);
            testCase.run();
        }

        return generateReport(url) ? 0 : 1;
    }

    //
    //  broadcast server
    //
    int ws_broadcast_server_main(int port,
                                 const std::string& hostname,
                                 const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        server.setOnClientMessageCallback(
            [&server](std::shared_ptr<ConnectionState> connectionState,
                      ConnectionInfo& connectionInfo,
                      WebSocket& webSocket,
                      const WebSocketMessagePtr& msg) {
                auto remoteIp = connectionInfo.remoteIp;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("New connection");
                    spdlog::info("remote ip: {}", remoteIp);
                    spdlog::info("id: {}", connectionState->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("Closed connection: code {} reason {}",
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    std::stringstream ss;
                    ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                    ss << "#retries: " << msg->errorInfo.retries << std::endl;
                    ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                    ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                    spdlog::info(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    spdlog::info("Received message fragment");
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("Received {} bytes", msg->wireSize);

                    for (auto&& client : server.getClients())
                    {
                        if (client.get() != &webSocket)
                        {
                            client->send(msg->str, msg->binary, [](int current, int total) -> bool {
                                spdlog::info("Step {} out of {}", current, total);
                                return true;
                            });

                            do
                            {
                                size_t bufferedAmount = client->bufferedAmount();
                                spdlog::info("{} bytes left to be sent", bufferedAmount);

                                std::chrono::duration<double, std::milli> duration(500);
                                std::this_thread::sleep_for(duration);
                            } while (client->bufferedAmount() != 0);
                        }
                    }
                }
            });

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::info(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }

    /*
     *  ws_chat.cpp
     *  Author: Benjamin Sergeant
     *  Copyright (c) 2017-2019 Machine Zone, Inc. All rights reserved.
     */

    //
    // Simple chat program that talks to a broadcast server
    // Broadcast server can be ran with `ws broadcast_server`
    //

    class WebSocketChat
    {
    public:
        WebSocketChat(const std::string& url, const std::string& user);

        void subscribe(const std::string& channel);
        void start();
        void stop();
        bool isReady() const;

        void sendMessage(const std::string& text);
        size_t getReceivedMessagesCount() const;

        std::string encodeMessage(const std::string& text);
        std::pair<std::string, std::string> decodeMessage(const std::string& str);

    private:
        std::string _url;
        std::string _user;
        ix::WebSocket _webSocket;
        std::queue<std::string> _receivedQueue;

        void log(const std::string& msg);
    };

    WebSocketChat::WebSocketChat(const std::string& url, const std::string& user)
        : _url(url)
        , _user(user)
    {
        ;
    }

    void WebSocketChat::log(const std::string& msg)
    {
        spdlog::info(msg);
    }

    size_t WebSocketChat::getReceivedMessagesCount() const
    {
        return _receivedQueue.size();
    }

    bool WebSocketChat::isReady() const
    {
        return _webSocket.getReadyState() == ix::ReadyState::Open;
    }

    void WebSocketChat::stop()
    {
        _webSocket.stop();
    }

    void WebSocketChat::start()
    {
        _webSocket.setUrl(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const WebSocketMessagePtr& msg) {
            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                log("ws chat: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }

                spdlog::info("ws chat: user {} connected !", _user);
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "ws chat user disconnected: " << _user;
                ss << " code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                auto result = decodeMessage(msg->str);

                // Our "chat" / "broacast" node.js server does not send us
                // the messages we send, so we don't have to filter it out.

                // store text
                _receivedQueue.push(result.second);

                ss << std::endl
                   << result.first << "(" << msg->wireSize << " bytes)"
                   << " > " << result.second << std::endl
                   << _user << " > ";
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                log(ss.str());
            }
            else
            {
                ss << "Invalid ix::WebSocketMessageType";
                log(ss.str());
            }
        });

        _webSocket.start();
    }

    std::pair<std::string, std::string> WebSocketChat::decodeMessage(const std::string& str)
    {
        auto j = json::parse(str);

        std::string msg_user = j["user"];
        std::string msg_text = j["text"];

        return std::pair<std::string, std::string>(msg_user, msg_text);
    }

    std::string WebSocketChat::encodeMessage(const std::string& text)
    {
        json j;
        j["user"] = _user;
        j["text"] = text;

        std::string output = j.dump();
        return output;
    }

    void WebSocketChat::sendMessage(const std::string& text)
    {
        _webSocket.sendText(encodeMessage(text));
    }

    int ws_chat_main(const std::string& url, const std::string& user)
    {
        spdlog::info("Type Ctrl-D to exit prompt...");
        WebSocketChat webSocketChat(url, user);
        webSocketChat.start();

        while (true)
        {
            // Read line
            std::string line;
            std::cout << user << " > " << std::flush;
            std::getline(std::cin, line);

            if (!std::cin)
            {
                break;
            }

            webSocketChat.sendMessage(line);
        }

        spdlog::info("");
        webSocketChat.stop();

        return 0;
    }

    int ws_cobra_metrics_publish_main(const ix::CobraConfig& config,
                                      const std::string& channel,
                                      const std::string& path,
                                      bool stress)
    {
        std::atomic<int> sentMessages(0);
        std::atomic<int> ackedMessages(0);
        CobraConnection::setPublishTrackerCallback(
            [&sentMessages, &ackedMessages](bool sent, bool acked) {
                if (sent) sentMessages++;
                if (acked) ackedMessages++;
            });

        CobraMetricsPublisher cobraMetricsPublisher;
        cobraMetricsPublisher.enable(true);
        cobraMetricsPublisher.configure(config, channel);

        while (!cobraMetricsPublisher.isAuthenticated())
            ;

        std::ifstream f(path);
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(str, data)) return 1;

        if (!stress)
        {
            auto msgId = cobraMetricsPublisher.push(channel, data);
            spdlog::info("Sent message: {}", msgId);
        }
        else
        {
            // Stress mode to try to trigger server and client bugs
            while (true)
            {
                for (int i = 0; i < 1000; ++i)
                {
                    cobraMetricsPublisher.push(channel, data);
                }

                cobraMetricsPublisher.suspend();
                cobraMetricsPublisher.resume();

                // FIXME: investigate why without this check we trigger a lock
                while (!cobraMetricsPublisher.isAuthenticated())
                    ;
            }
        }

        // Wait a bit for the message to get a chance to be sent
        // there isn't any ack on publish right now so it's the best we can do
        // FIXME: this comment is a lie now
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        spdlog::info("Sent messages: {} Acked messages {}", sentMessages, ackedMessages);

        return 0;
    }

    int ws_cobra_publish_main(const ix::CobraConfig& config,
                              const std::string& channel,
                              const std::string& path)
    {
        std::ifstream f(path);
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(str, data))
        {
            spdlog::info("Input file is not a JSON file");
            return 1;
        }

        ix::CobraConnection conn;
        conn.configure(config);

        // Display incoming messages
        std::atomic<bool> authenticated(false);
        std::atomic<bool> messageAcked(false);

        conn.setEventCallback(
            [&conn, &channel, &data, &authenticated, &messageAcked](const CobraEventPtr& event) {
                if (event->type == ix::CobraEventType::Open)
                {
                    spdlog::info("Publisher connected");

                    for (auto&& it : event->headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }
                }
                else if (event->type == ix::CobraEventType::Closed)
                {
                    spdlog::info("Subscriber closed: {}", event->errMsg);
                }
                else if (event->type == ix::CobraEventType::Authenticated)
                {
                    spdlog::info("Publisher authenticated");
                    authenticated = true;

                    Json::Value channels;
                    channels[0] = channel;
                    auto msgId = conn.publish(channels, data);

                    spdlog::info("Published msg {}", msgId);
                }
                else if (event->type == ix::CobraEventType::Subscribed)
                {
                    spdlog::info("Publisher: subscribed to channel {}", event->subscriptionId);
                }
                else if (event->type == ix::CobraEventType::UnSubscribed)
                {
                    spdlog::info("Publisher: unsubscribed from channel {}", event->subscriptionId);
                }
                else if (event->type == ix::CobraEventType::Error)
                {
                    spdlog::error("Publisher: error {}", event->errMsg);
                }
                else if (event->type == ix::CobraEventType::Published)
                {
                    spdlog::info("Published message id {} acked", event->msgId);
                    messageAcked = true;
                }
                else if (event->type == ix::CobraEventType::Pong)
                {
                    spdlog::info("Received websocket pong");
                }
                else if (event->type == ix::CobraEventType::HandshakeError)
                {
                    spdlog::error("Subscriber: Handshake error: {}", event->errMsg);
                }
                else if (event->type == ix::CobraEventType::AuthenticationError)
                {
                    spdlog::error("Subscriber: Authentication error: {}", event->errMsg);
                }
            });

        conn.connect();

        while (!authenticated)
            ;
        while (!messageAcked)
            ;

        return 0;
    }

    class WebSocketConnect
    {
    public:
        WebSocketConnect(const std::string& _url,
                         const std::string& headers,
                         bool disableAutomaticReconnection,
                         bool disablePerMessageDeflate,
                         bool binaryMode,
                         uint32_t maxWaitBetweenReconnectionRetries,
                         const ix::SocketTLSOptions& tlsOptions,
                         const std::string& subprotocol,
                         int pingIntervalSecs);

        void subscribe(const std::string& channel);
        void start();
        void stop();

        int getSentBytes()
        {
            return _sentBytes;
        }
        int getReceivedBytes()
        {
            return _receivedBytes;
        }

        void sendMessage(const std::string& text);

    private:
        std::string _url;
        WebSocketHttpHeaders _headers;
        ix::WebSocket _webSocket;
        bool _disablePerMessageDeflate;
        bool _binaryMode;
        std::atomic<int> _receivedBytes;
        std::atomic<int> _sentBytes;

        void log(const std::string& msg);
        WebSocketHttpHeaders parseHeaders(const std::string& data);
    };

    WebSocketConnect::WebSocketConnect(const std::string& url,
                                       const std::string& headers,
                                       bool disableAutomaticReconnection,
                                       bool disablePerMessageDeflate,
                                       bool binaryMode,
                                       uint32_t maxWaitBetweenReconnectionRetries,
                                       const ix::SocketTLSOptions& tlsOptions,
                                       const std::string& subprotocol,
                                       int pingIntervalSecs)
        : _url(url)
        , _disablePerMessageDeflate(disablePerMessageDeflate)
        , _binaryMode(binaryMode)
        , _receivedBytes(0)
        , _sentBytes(0)
    {
        if (disableAutomaticReconnection)
        {
            _webSocket.disableAutomaticReconnection();
        }
        _webSocket.setMaxWaitBetweenReconnectionRetries(maxWaitBetweenReconnectionRetries);
        _webSocket.setTLSOptions(tlsOptions);
        _webSocket.setPingInterval(pingIntervalSecs);

        _headers = parseHeaders(headers);

        if (!subprotocol.empty())
        {
            _webSocket.addSubProtocol(subprotocol);
        }

        WebSocket::setTrafficTrackerCallback([this](int size, bool incoming) {
            if (incoming)
            {
                _receivedBytes += size;
            }
            else
            {
                _sentBytes += size;
            }
        });
    }

    void WebSocketConnect::log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    WebSocketHttpHeaders WebSocketConnect::parseHeaders(const std::string& data)
    {
        WebSocketHttpHeaders headers;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(data);

        while (std::getline(tokenStream, token))
        {
            std::size_t pos = token.rfind(':');

            // Bail out if last '.' is found
            if (pos == std::string::npos) continue;

            auto key = token.substr(0, pos);
            auto val = token.substr(pos + 1);

            spdlog::info("{}: {}", key, val);
            headers[key] = val;
        }

        return headers;
    }

    void WebSocketConnect::stop()
    {
        {
            Bench bench("ws_connect: stop connection");
            _webSocket.stop();
        }
    }

    void WebSocketConnect::start()
    {
        _webSocket.setUrl(_url);
        _webSocket.setExtraHeaders(_headers);

        if (_disablePerMessageDeflate)
        {
            _webSocket.disablePerMessageDeflate();
        }
        else
        {
            ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
                true, false, false, 15, 15);
            _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);
        }

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                spdlog::info("ws_connect: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "ws_connect: connection closed:";
                ss << " code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                spdlog::info("Received {} bytes", msg->wireSize);

                ss << "ws_connect: received message: " << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Fragment)
            {
                spdlog::info("Received message fragment");
            }
            else if (msg->type == ix::WebSocketMessageType::Ping)
            {
                spdlog::info("Received ping");
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                spdlog::info("Received pong {}", msg->str);
            }
            else
            {
                ss << "Invalid ix::WebSocketMessageType";
                log(ss.str());
            }
        });

        _webSocket.start();
    }

    void WebSocketConnect::sendMessage(const std::string& text)
    {
        if (_binaryMode)
        {
            _webSocket.sendBinary(text);
        }
        else
        {
            _webSocket.sendText(text);
        }
    }

    int ws_connect_main(const std::string& url,
                        const std::string& headers,
                        bool disableAutomaticReconnection,
                        bool disablePerMessageDeflate,
                        bool binaryMode,
                        uint32_t maxWaitBetweenReconnectionRetries,
                        const ix::SocketTLSOptions& tlsOptions,
                        const std::string& subprotocol,
                        int pingIntervalSecs)
    {
        std::cout << "Type Ctrl-D to exit prompt..." << std::endl;
        WebSocketConnect webSocketChat(url,
                                       headers,
                                       disableAutomaticReconnection,
                                       disablePerMessageDeflate,
                                       binaryMode,
                                       maxWaitBetweenReconnectionRetries,
                                       tlsOptions,
                                       subprotocol,
                                       pingIntervalSecs);
        webSocketChat.start();

        while (true)
        {
            // Read line
            std::string line;
            auto quit = linenoise::Readline("> ", line);

            if (quit)
            {
                break;
            }

            if (line == "/stop")
            {
                spdlog::info("Stopping connection...");
                webSocketChat.stop();
                continue;
            }

            if (line == "/start")
            {
                spdlog::info("Starting connection...");
                webSocketChat.start();
                continue;
            }

            webSocketChat.sendMessage(line);

            // Add text to history
            linenoise::AddHistory(line.c_str());
        }

        spdlog::info("");
        webSocketChat.stop();

        spdlog::info("Received {} bytes", webSocketChat.getReceivedBytes());
        spdlog::info("Sent {} bytes", webSocketChat.getSentBytes());

        return 0;
    }

    int ws_dns_lookup(const std::string& hostname)
    {
        auto dnsLookup = std::make_shared<DNSLookup>(hostname, 80);

        std::string errMsg;
        struct addrinfo* res;

        res = dnsLookup->resolve(errMsg, [] { return false; });

        auto addr = res->ai_addr;

        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN);

        spdlog::info("host: {} ip: {}", hostname, str);

        return 0;
    }

    int ws_echo_client(const std::string& url,
                       bool disablePerMessageDeflate,
                       bool binaryMode,
                       const ix::SocketTLSOptions& tlsOptions,
                       const std::string& subprotocol,
                       int pingIntervalSecs,
                       const std::string& sendMsg,
                       bool noSend)
    {
        // Our websocket object
        ix::WebSocket webSocket;

        webSocket.setUrl(url);
        webSocket.setTLSOptions(tlsOptions);
        webSocket.setPingInterval(pingIntervalSecs);

        if (disablePerMessageDeflate)
        {
            webSocket.disablePerMessageDeflate();
        }

        if (!subprotocol.empty())
        {
            webSocket.addSubProtocol(subprotocol);
        }

        std::atomic<uint64_t> receivedCount(0);
        uint64_t receivedCountTotal(0);
        uint64_t receivedCountPerSecs(0);

        // Setup a callback to be fired (in a background thread, watch out for race conditions !)
        // when a message or an event (open, close, error) is received
        webSocket.setOnMessageCallback([&webSocket, &receivedCount, &sendMsg, noSend, binaryMode](
                                           const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                if (!noSend)
                {
                    webSocket.send(msg->str, msg->binary);
                }
                receivedCount++;
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                spdlog::info("ws_echo_client: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }

                webSocket.send(sendMsg, binaryMode);
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                spdlog::info("Received pong {}", msg->str);
            }
        });

        auto timer = [&receivedCount, &receivedCountTotal, &receivedCountPerSecs] {
            setThreadName("Timer");
            while (true)
            {
                //
                // We cannot write to sentCount and receivedCount
                // as those are used externally, so we need to introduce
                // our own counters
                //
                std::stringstream ss;
                ss << "messages received: " << receivedCountPerSecs << " per second "
                   << receivedCountTotal << " total";

                CoreLogger::info(ss.str());

                receivedCountPerSecs = receivedCount - receivedCountTotal;
                receivedCountTotal += receivedCountPerSecs;

                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t1(timer);

        // Now that our callback is setup, we can start our background thread and receive messages
        std::cout << "Connecting to " << url << "..." << std::endl;
        webSocket.start();

        // Send a message to the server (default to TEXT mode)
        webSocket.send("hello world");

        while (true)
        {
            std::string text;
            std::cout << "> " << std::flush;
            std::getline(std::cin, text);

            webSocket.send(text);
        }

        return 0;
    }

    int ws_echo_server_main(int port,
                            bool greetings,
                            const std::string& hostname,
                            const ix::SocketTLSOptions& tlsOptions,
                            bool ipv6,
                            bool disablePerMessageDeflate,
                            bool disablePong)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port,
                                   hostname,
                                   SocketServer::kDefaultTcpBacklog,
                                   SocketServer::kDefaultMaxConnections,
                                   WebSocketServer::kDefaultHandShakeTimeoutSecs,
                                   (ipv6) ? AF_INET6 : AF_INET);

        server.setTLSOptions(tlsOptions);

        if (disablePerMessageDeflate)
        {
            spdlog::info("Disable per message deflate");
            server.disablePerMessageDeflate();
        }

        if (disablePong)
        {
            spdlog::info("Disable responding to PING messages with PONG");
            server.disablePong();
        }

        server.setOnClientMessageCallback(
            [greetings](std::shared_ptr<ConnectionState> connectionState,
                        ConnectionInfo& connectionInfo,
                        WebSocket& webSocket,
                        const WebSocketMessagePtr& msg) {
                auto remoteIp = connectionInfo.remoteIp;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("New connection");
                    spdlog::info("remote ip: {}", remoteIp);
                    spdlog::info("id: {}", connectionState->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }

                    if (greetings)
                    {
                        webSocket.sendText("Welcome !");
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("Closed connection: client id {} code {} reason {}",
                                 connectionState->getId(),
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    spdlog::error("Connection error: {}", msg->errorInfo.reason);
                    spdlog::error("#retries: {}", msg->errorInfo.retries);
                    spdlog::error("Wait time(ms): {}", msg->errorInfo.wait_time);
                    spdlog::error("HTTP Status: {}", msg->errorInfo.http_status);
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("Received {} bytes", msg->wireSize);
                    webSocket.send(msg->str, msg->binary);
                }
            });

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::error(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }

    std::string extractFilename(const std::string& path)
    {
        std::string::size_type idx;

        idx = path.rfind('/');
        if (idx != std::string::npos)
        {
            std::string filename = path.substr(idx + 1);
            return filename;
        }
        else
        {
            return path;
        }
    }

    WebSocketHttpHeaders parseHeaders(const std::string& data)
    {
        WebSocketHttpHeaders headers;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(data);

        while (std::getline(tokenStream, token))
        {
            std::size_t pos = token.rfind(':');

            // Bail out if last '.' is found
            if (pos == std::string::npos) continue;

            auto key = token.substr(0, pos);
            auto val = token.substr(pos + 1);

            spdlog::info("{}: {}", key, val);
            headers[key] = val;
        }

        return headers;
    }

    //
    // Useful endpoint to test HTTP post
    // https://postman-echo.com/post
    //
    HttpParameters parsePostParameters(const std::string& data)
    {
        HttpParameters httpParameters;

        // Split by \n
        std::string token;
        std::stringstream tokenStream(data);

        while (std::getline(tokenStream, token))
        {
            std::size_t pos = token.rfind('=');

            // Bail out if last '.' is found
            if (pos == std::string::npos) continue;

            auto key = token.substr(0, pos);
            auto val = token.substr(pos + 1);

            spdlog::info("{}: {}", key, val);
            httpParameters[key] = val;
        }

        return httpParameters;
    }

    int ws_http_client_main(const std::string& url,
                            const std::string& headersData,
                            const std::string& data,
                            bool headersOnly,
                            int connectTimeout,
                            int transferTimeout,
                            bool followRedirects,
                            int maxRedirects,
                            bool verbose,
                            bool save,
                            const std::string& output,
                            bool compress,
                            const ix::SocketTLSOptions& tlsOptions)
    {
        HttpClient httpClient;
        httpClient.setTLSOptions(tlsOptions);

        auto args = httpClient.createRequest();
        args->extraHeaders = parseHeaders(headersData);
        args->connectTimeout = connectTimeout;
        args->transferTimeout = transferTimeout;
        args->followRedirects = followRedirects;
        args->maxRedirects = maxRedirects;
        args->verbose = verbose;
        args->compress = compress;
        args->logger = [](const std::string& msg) { spdlog::info(msg); };
        args->onProgressCallback = [verbose](int current, int total) -> bool {
            if (verbose)
            {
                spdlog::info("Downloaded {} bytes out of {}", current, total);
            }
            return true;
        };

        HttpParameters httpParameters = parsePostParameters(data);

        HttpResponsePtr response;
        if (headersOnly)
        {
            response = httpClient.head(url, args);
        }
        else if (data.empty())
        {
            response = httpClient.get(url, args);
        }
        else
        {
            response = httpClient.post(url, httpParameters, args);
        }

        spdlog::info("");

        for (auto it : response->headers)
        {
            spdlog::info("{}: {}", it.first, it.second);
        }

        spdlog::info("Upload size: {}", response->uploadSize);
        spdlog::info("Download size: {}", response->downloadSize);

        spdlog::info("Status: {}", response->statusCode);
        if (response->errorCode != HttpErrorCode::Ok)
        {
            spdlog::info("error message: ", response->errorMsg);
        }

        if (!headersOnly && response->errorCode == HttpErrorCode::Ok)
        {
            if (save || !output.empty())
            {
                // FIMXE we should decode the url first
                std::string filename = extractFilename(url);
                if (!output.empty())
                {
                    filename = output;
                }

                spdlog::info("Writing to disk: {}", filename);
                std::ofstream out(filename);
                out.write((char*) &response->payload.front(), response->payload.size());
                out.close();
            }
            else
            {
                if (response->headers["Content-Type"] != "application/octet-stream")
                {
                    spdlog::info("payload: {}", response->payload);
                }
                else
                {
                    spdlog::info("Binary output can mess up your terminal.");
                    spdlog::info("Use the -O flag to save the file to disk.");
                    spdlog::info("You can also use the --output option to specify a filename.");
                }
            }
        }

        return 0;
    }

    int ws_httpd_main(int port,
                      const std::string& hostname,
                      bool redirect,
                      const std::string& redirectUrl,
                      const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::HttpServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        if (redirect)
        {
            server.makeRedirectServer(redirectUrl);
        }

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::error(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }

    class WebSocketPingPong
    {
    public:
        WebSocketPingPong(const std::string& _url, const ix::SocketTLSOptions& tlsOptions);

        void subscribe(const std::string& channel);
        void start();
        void stop();

        void ping(const std::string& text);
        void send(const std::string& text);

    private:
        std::string _url;
        ix::WebSocket _webSocket;

        void log(const std::string& msg);
    };

    WebSocketPingPong::WebSocketPingPong(const std::string& url,
                                         const ix::SocketTLSOptions& tlsOptions)
        : _url(url)
    {
        _webSocket.setTLSOptions(tlsOptions);
    }

    void WebSocketPingPong::log(const std::string& msg)
    {
        spdlog::info(msg);
    }

    void WebSocketPingPong::stop()
    {
        _webSocket.stop();
    }

    void WebSocketPingPong::start()
    {
        _webSocket.setUrl(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            spdlog::info("Received {} bytes", msg->wireSize);

            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                log("ping_pong: connected");

                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "ping_pong: disconnected:"
                   << " code " << msg->closeInfo.code << " reason " << msg->closeInfo.reason
                   << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                ss << "ping_pong: received message: " << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Ping)
            {
                ss << "ping_pong: received ping message: " << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                ss << "ping_pong: received pong message: " << msg->str;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                log(ss.str());
            }
            else
            {
                ss << "Invalid ix::WebSocketMessageType";
                log(ss.str());
            }
        });

        _webSocket.start();
    }

    void WebSocketPingPong::ping(const std::string& text)
    {
        if (!_webSocket.ping(text).success)
        {
            std::cerr << "Failed to send ping message. Message too long (> 125 bytes) or endpoint "
                         "is disconnected"
                      << std::endl;
        }
    }

    void WebSocketPingPong::send(const std::string& text)
    {
        _webSocket.send(text);
    }

    int ws_ping_pong_main(const std::string& url, const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Type Ctrl-D to exit prompt...");
        WebSocketPingPong webSocketPingPong(url, tlsOptions);
        webSocketPingPong.start();

        while (true)
        {
            std::string text;
            std::cout << "> " << std::flush;
            std::getline(std::cin, text);

            if (!std::cin)
            {
                break;
            }

            if (text == "/close")
            {
                webSocketPingPong.send(text);
            }
            else
            {
                webSocketPingPong.ping(text);
            }
        }

        std::cout << std::endl;
        webSocketPingPong.stop();

        return 0;
    }

    int ws_push_server(int port,
                       bool greetings,
                       const std::string& hostname,
                       const ix::SocketTLSOptions& tlsOptions,
                       bool ipv6,
                       bool disablePerMessageDeflate,
                       bool disablePong,
                       const std::string& sendMsg)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port,
                                   hostname,
                                   SocketServer::kDefaultTcpBacklog,
                                   SocketServer::kDefaultMaxConnections,
                                   WebSocketServer::kDefaultHandShakeTimeoutSecs,
                                   (ipv6) ? AF_INET6 : AF_INET);

        server.setTLSOptions(tlsOptions);

        if (disablePerMessageDeflate)
        {
            spdlog::info("Disable per message deflate");
            server.disablePerMessageDeflate();
        }

        if (disablePong)
        {
            spdlog::info("Disable responding to PING messages with PONG");
            server.disablePong();
        }

        server.setOnClientMessageCallback(
            [greetings, &sendMsg](std::shared_ptr<ConnectionState> connectionState,
                                  ConnectionInfo& connectionInfo,
                                  WebSocket& webSocket,
                                  const WebSocketMessagePtr& msg) {
                auto remoteIp = connectionInfo.remoteIp;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("New connection");
                    spdlog::info("remote ip: {}", remoteIp);
                    spdlog::info("id: {}", connectionState->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }

                    if (greetings)
                    {
                        webSocket.sendText("Welcome !");
                    }

                    bool binary = false;
                    while (true)
                    {
                        auto sendInfo = webSocket.send(sendMsg, binary);
                        if (!sendInfo.success)
                        {
                            spdlog::info("Error sending message, closing connection");
                            webSocket.close();
                            break;
                        }
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("Closed connection: client id {} code {} reason {}",
                                 connectionState->getId(),
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    spdlog::error("Connection error: {}", msg->errorInfo.reason);
                    spdlog::error("#retries: {}", msg->errorInfo.retries);
                    spdlog::error("Wait time(ms): {}", msg->errorInfo.wait_time);
                    spdlog::error("HTTP Status: {}", msg->errorInfo.http_status);
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("Received {} bytes", msg->wireSize);
                    webSocket.send(msg->str, msg->binary);
                }
            });

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::error(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }

    class WebSocketReceiver
    {
    public:
        WebSocketReceiver(const std::string& _url,
                          bool enablePerMessageDeflate,
                          int delayMs,
                          const ix::SocketTLSOptions& tlsOptions);

        void subscribe(const std::string& channel);
        void start();
        void stop();

        void waitForConnection();
        void waitForMessage();
        void handleMessage(const std::string& str);

    private:
        std::string _url;
        std::string _id;
        ix::WebSocket _webSocket;
        bool _enablePerMessageDeflate;
        int _delayMs;
        int _receivedFragmentCounter;

        std::mutex _conditionVariableMutex;
        std::condition_variable _condition;

        std::string extractFilename(const std::string& path);
        void handleError(const std::string& errMsg, const std::string& id);
        void log(const std::string& msg);
    };

    WebSocketReceiver::WebSocketReceiver(const std::string& url,
                                         bool enablePerMessageDeflate,
                                         int delayMs,
                                         const ix::SocketTLSOptions& tlsOptions)
        : _url(url)
        , _enablePerMessageDeflate(enablePerMessageDeflate)
        , _delayMs(delayMs)
        , _receivedFragmentCounter(0)
    {
        _webSocket.disableAutomaticReconnection();
        _webSocket.setTLSOptions(tlsOptions);
    }

    void WebSocketReceiver::stop()
    {
        _webSocket.stop();
    }

    void WebSocketReceiver::log(const std::string& msg)
    {
        spdlog::info(msg);
    }

    void WebSocketReceiver::waitForConnection()
    {
        spdlog::info("{}: Connecting...", "ws_receive");

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    void WebSocketReceiver::waitForMessage()
    {
        spdlog::info("{}: Waiting for message...", "ws_receive");

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    // We should cleanup the file name and full path further to remove .. as well
    std::string WebSocketReceiver::extractFilename(const std::string& path)
    {
        std::string::size_type idx;

        idx = path.rfind('/');
        if (idx != std::string::npos)
        {
            std::string filename = path.substr(idx + 1);
            return filename;
        }
        else
        {
            return path;
        }
    }

    void WebSocketReceiver::handleError(const std::string& errMsg, const std::string& id)
    {
        std::map<MsgPack, MsgPack> pdu;
        pdu["kind"] = "error";
        pdu["id"] = id;
        pdu["message"] = errMsg;

        MsgPack msg(pdu);
        _webSocket.sendBinary(msg.dump());
    }

    void WebSocketReceiver::handleMessage(const std::string& str)
    {
        spdlog::info("ws_receive: Received message: {}", str.size());

        std::string errMsg;
        MsgPack data = MsgPack::parse(str, errMsg);
        if (!errMsg.empty())
        {
            handleError("ws_receive: Invalid MsgPack", std::string());
            return;
        }

        spdlog::info("id: {}", data["id"].string_value());

        std::vector<uint8_t> content = data["content"].binary_items();
        spdlog::info("ws_receive: Content size: {}", content.size());

        // Validate checksum
        uint64_t cksum = ix::djb2Hash(content);
        auto cksumRef = data["djb2_hash"].string_value();

        spdlog::info("ws_receive: Computed hash: {}", cksum);
        spdlog::info("ws_receive: Reference hash: {}", cksumRef);

        if (std::to_string(cksum) != cksumRef)
        {
            handleError("Hash mismatch.", std::string());
            return;
        }

        std::string filename = data["filename"].string_value();
        filename = extractFilename(filename);

        std::string filenameTmp = filename + ".tmp";

        spdlog::info("ws_receive: Writing to disk: {}", filenameTmp);
        std::ofstream out(filenameTmp);
        out.write((char*) &content.front(), content.size());
        out.close();

        spdlog::info("ws_receive: Renaming {} to {}", filenameTmp, filename);
        rename(filenameTmp.c_str(), filename.c_str());

        std::map<MsgPack, MsgPack> pdu;
        pdu["ack"] = true;
        pdu["id"] = data["id"];
        pdu["filename"] = data["filename"];

        spdlog::info("Sending ack to sender");
        MsgPack msg(pdu);
        _webSocket.sendBinary(msg.dump());
    }

    void WebSocketReceiver::start()
    {
        _webSocket.setUrl(_url);
        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            _enablePerMessageDeflate, false, false, 15, 15);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);

        std::stringstream ss;
        log(std::string("ws_receive: Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                _condition.notify_one();

                log("ws_receive: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                ss << "ws_receive: connection closed:";
                ss << " code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                ss << "ws_receive: transfered " << msg->wireSize << " bytes";
                log(ss.str());
                handleMessage(msg->str);
                _condition.notify_one();
            }
            else if (msg->type == ix::WebSocketMessageType::Fragment)
            {
                ss << "ws_receive: received fragment " << _receivedFragmentCounter++;
                log(ss.str());

                if (_delayMs > 0)
                {
                    // Introduce an arbitrary delay, to simulate a slow connection
                    std::chrono::duration<double, std::milli> duration(_delayMs);
                    std::this_thread::sleep_for(duration);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "ws_receive ";
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Ping)
            {
                log("ws_receive: received ping");
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                log("ws_receive: received pong");
            }
            else
            {
                ss << "ws_receive: Invalid ix::WebSocketMessageType";
                log(ss.str());
            }
        });

        _webSocket.start();
    }

    void wsReceive(const std::string& url,
                   bool enablePerMessageDeflate,
                   int delayMs,
                   const ix::SocketTLSOptions& tlsOptions)
    {
        WebSocketReceiver webSocketReceiver(url, enablePerMessageDeflate, delayMs, tlsOptions);
        webSocketReceiver.start();

        webSocketReceiver.waitForConnection();

        webSocketReceiver.waitForMessage();

        std::chrono::duration<double, std::milli> duration(1000);
        std::this_thread::sleep_for(duration);

        spdlog::info("ws_receive: Done !");
        webSocketReceiver.stop();
    }

    int ws_receive_main(const std::string& url,
                        bool enablePerMessageDeflate,
                        int delayMs,
                        const ix::SocketTLSOptions& tlsOptions)
    {
        wsReceive(url, enablePerMessageDeflate, delayMs, tlsOptions);
        return 0;
    }

    int ws_redis_cli_main(const std::string& hostname, int port, const std::string& password)
    {
        RedisClient redisClient;
        if (!redisClient.connect(hostname, port))
        {
            spdlog::info("Cannot connect to redis host");
            return 1;
        }

        if (!password.empty())
        {
            std::string authResponse;
            if (!redisClient.auth(password, authResponse))
            {
                std::stringstream ss;
                spdlog::info("Cannot authenticated to redis");
                return 1;
            }
            spdlog::info("Auth response: {}", authResponse);
        }

        while (true)
        {
            // Read line
            std::string line;
            std::string prompt;
            prompt += hostname;
            prompt += ":";
            prompt += std::to_string(port);
            prompt += "> ";
            auto quit = linenoise::Readline(prompt.c_str(), line);

            if (quit)
            {
                break;
            }

            std::stringstream ss(line);
            std::vector<std::string> args;
            std::string arg;

            while (ss.good())
            {
                ss >> arg;
                args.push_back(arg);
            }

            std::string errMsg;
            auto response = redisClient.send(args, errMsg);
            if (!errMsg.empty())
            {
                spdlog::error("(error) {}", errMsg);
            }
            else
            {
                if (response.first != RespType::String)
                {
                    std::cout << "(" << redisClient.getRespTypeDescription(response.first) << ")"
                              << " ";
                }

                std::cout << response.second << std::endl;
            }

            linenoise::AddHistory(line.c_str());
        }

        return 0;
    }

    int ws_redis_publish_main(const std::string& hostname,
                              int port,
                              const std::string& password,
                              const std::string& channel,
                              const std::string& message,
                              int count)
    {
        RedisClient redisClient;
        if (!redisClient.connect(hostname, port))
        {
            spdlog::info("Cannot connect to redis host");
            return 1;
        }

        if (!password.empty())
        {
            std::string authResponse;
            if (!redisClient.auth(password, authResponse))
            {
                std::stringstream ss;
                spdlog::info("Cannot authenticated to redis");
                return 1;
            }
            spdlog::info("Auth response: {}", authResponse);
        }

        std::string errMsg;
        for (int i = 0; i < count; i++)
        {
            if (!redisClient.publish(channel, message, errMsg))
            {
                spdlog::error("Error publishing to channel {} error {}", channel, errMsg);
                return 1;
            }
        }

        return 0;
    }

    int ws_redis_server_main(int port, const std::string& hostname)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::RedisServer server(port, hostname);

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::info(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }

    int ws_redis_subscribe_main(const std::string& hostname,
                                int port,
                                const std::string& password,
                                const std::string& channel,
                                bool verbose)
    {
        RedisClient redisClient;
        if (!redisClient.connect(hostname, port))
        {
            spdlog::info("Cannot connect to redis host");
            return 1;
        }

        if (!password.empty())
        {
            std::string authResponse;
            if (!redisClient.auth(password, authResponse))
            {
                std::stringstream ss;
                spdlog::info("Cannot authenticated to redis");
                return 1;
            }
            spdlog::info("Auth response: {}", authResponse);
        }

        std::atomic<int> msgPerSeconds(0);
        std::atomic<int> msgCount(0);

        auto callback = [&msgPerSeconds, &msgCount, verbose](const std::string& message) {
            if (verbose)
            {
                spdlog::info("recived: {}", message);
            }

            msgPerSeconds++;
            msgCount++;
        };

        auto responseCallback = [](const std::string& redisResponse) {
            spdlog::info("Redis subscribe response: {}", redisResponse);
        };

        auto timer = [&msgPerSeconds, &msgCount] {
            while (true)
            {
                spdlog::info("#messages {} msg/s {}", msgCount, msgPerSeconds);

                msgPerSeconds = 0;
                auto duration = std::chrono::seconds(1);
                std::this_thread::sleep_for(duration);
            }
        };

        std::thread t(timer);

        spdlog::info("Subscribing to {} ...", channel);
        if (!redisClient.subscribe(channel, responseCallback, callback))
        {
            spdlog::info("Error subscribing to channel {}", channel);
            return 1;
        }

        return 0;
    }

    class WebSocketSender
    {
    public:
        WebSocketSender(const std::string& _url,
                        bool enablePerMessageDeflate,
                        const ix::SocketTLSOptions& tlsOptions);

        void subscribe(const std::string& channel);
        void start();
        void stop();

        void waitForConnection();
        void waitForAck();

        bool sendMessage(const std::string& filename, bool throttle);

    private:
        std::string _url;
        std::string _id;
        ix::WebSocket _webSocket;
        bool _enablePerMessageDeflate;

        std::atomic<bool> _connected;

        std::mutex _conditionVariableMutex;
        std::condition_variable _condition;

        void log(const std::string& msg);
    };

    WebSocketSender::WebSocketSender(const std::string& url,
                                     bool enablePerMessageDeflate,
                                     const ix::SocketTLSOptions& tlsOptions)
        : _url(url)
        , _enablePerMessageDeflate(enablePerMessageDeflate)
        , _connected(false)
    {
        _webSocket.disableAutomaticReconnection();
        _webSocket.setTLSOptions(tlsOptions);
    }

    void WebSocketSender::stop()
    {
        _webSocket.stop();
    }

    void WebSocketSender::log(const std::string& msg)
    {
        spdlog::info(msg);
    }

    void WebSocketSender::waitForConnection()
    {
        spdlog::info("{}: Connecting...", "ws_send");

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    void WebSocketSender::waitForAck()
    {
        spdlog::info("{}: Waiting for ack...", "ws_send");

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    std::vector<uint8_t> load(const std::string& path)
    {
        std::vector<uint8_t> memblock;

        std::ifstream file(path);
        if (!file.is_open()) return memblock;

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        memblock.resize((size_t) size);
        file.read((char*) &memblock.front(), static_cast<std::streamsize>(size));

        return memblock;
    }

    void WebSocketSender::start()
    {
        _webSocket.setUrl(_url);

        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            _enablePerMessageDeflate, false, false, 15, 15);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);

        std::stringstream ss;
        log(std::string("ws_send: Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback([this](const WebSocketMessagePtr& msg) {
            std::stringstream ss;
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                _connected = true;

                _condition.notify_one();

                log("ws_send: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                _connected = false;

                ss << "ws_send: connection closed:";
                ss << " code " << msg->closeInfo.code;
                ss << " reason " << msg->closeInfo.reason << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                _condition.notify_one();

                ss << "ws_send: received message (" << msg->wireSize << " bytes)";
                log(ss.str());

                std::string errMsg;
                MsgPack data = MsgPack::parse(msg->str, errMsg);
                if (!errMsg.empty())
                {
                    spdlog::info("Invalid MsgPack response");
                    return;
                }

                std::string id = data["id"].string_value();
                if (_id != id)
                {
                    spdlog::info("Invalid id");
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                ss << "ws_send ";
                ss << "Connection error: " << msg->errorInfo.reason << std::endl;
                ss << "#retries: " << msg->errorInfo.retries << std::endl;
                ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                log(ss.str());
            }
            else if (msg->type == ix::WebSocketMessageType::Ping)
            {
                spdlog::info("ws_send: received ping");
            }
            else if (msg->type == ix::WebSocketMessageType::Pong)
            {
                spdlog::info("ws_send: received pong");
            }
            else if (msg->type == ix::WebSocketMessageType::Fragment)
            {
                spdlog::info("ws_send: received fragment");
            }
            else
            {
                ss << "ws_send: Invalid ix::WebSocketMessageType";
                log(ss.str());
            }
        });

        _webSocket.start();
    }

    bool WebSocketSender::sendMessage(const std::string& filename, bool throttle)
    {
        std::vector<uint8_t> content;
        {
            Bench bench("ws_send: load file from disk");
            content = load(filename);
        }

        _id = uuid4();

        std::map<MsgPack, MsgPack> pdu;
        pdu["kind"] = "send";
        pdu["id"] = _id;
        pdu["content"] = content;
        auto hash = djb2Hash(content);
        pdu["djb2_hash"] = std::to_string(hash);
        pdu["filename"] = filename;

        MsgPack msg(pdu);

        auto serializedMsg = msg.dump();
        spdlog::info("ws_send: sending {} bytes", serializedMsg.size());

        Bench bench("ws_send: Sending file through websocket");
        auto result =
            _webSocket.sendBinary(serializedMsg, [this, throttle](int current, int total) -> bool {
                spdlog::info("ws_send: Step {} out of {}", current + 1, total);

                if (throttle)
                {
                    std::chrono::duration<double, std::milli> duration(10);
                    std::this_thread::sleep_for(duration);
                }

                return _connected;
            });

        if (!result.success)
        {
            spdlog::error("ws_send: Error sending file.");
            return false;
        }

        if (!_connected)
        {
            spdlog::error("ws_send: Got disconnected from the server");
            return false;
        }

        spdlog::info("ws_send: sent {} bytes", serializedMsg.size());

        do
        {
            size_t bufferedAmount = _webSocket.bufferedAmount();
            spdlog::info("ws_send: {} bytes left to be sent", bufferedAmount);

            std::chrono::duration<double, std::milli> duration(500);
            std::this_thread::sleep_for(duration);
        } while (_webSocket.bufferedAmount() != 0 && _connected);

        if (_connected)
        {
            bench.report();
            auto duration = bench.getDuration();
            auto transferRate = 1000 * content.size() / duration;
            transferRate /= (1024 * 1024);
            spdlog::info("ws_send: Send transfer rate: {} MB/s", transferRate);
        }
        else
        {
            spdlog::error("ws_send: Got disconnected from the server");
        }

        return _connected;
    }

    void wsSend(const std::string& url,
                const std::string& path,
                bool enablePerMessageDeflate,
                bool throttle,
                const ix::SocketTLSOptions& tlsOptions)
    {
        WebSocketSender webSocketSender(url, enablePerMessageDeflate, tlsOptions);
        webSocketSender.start();

        webSocketSender.waitForConnection();

        spdlog::info("ws_send: Sending...");
        if (webSocketSender.sendMessage(path, throttle))
        {
            webSocketSender.waitForAck();
            spdlog::info("ws_send: Done !");
        }
        else
        {
            spdlog::error("ws_send: Error sending file.");
        }

        webSocketSender.stop();
    }

    int ws_send_main(const std::string& url,
                     const std::string& path,
                     bool disablePerMessageDeflate,
                     const ix::SocketTLSOptions& tlsOptions)
    {
        bool throttle = false;
        bool enablePerMessageDeflate = !disablePerMessageDeflate;

        wsSend(url, path, enablePerMessageDeflate, throttle, tlsOptions);
        return 0;
    }

    int ws_sentry_minidump_upload(const std::string& metadataPath,
                                  const std::string& minidump,
                                  const std::string& project,
                                  const std::string& key,
                                  bool verbose)
    {
        SentryClient sentryClient((std::string()));

        // Read minidump file from disk
        std::string minidumpBytes = readBytes(minidump);

        // Read json data
        std::string sentryMetadata = readBytes(metadataPath);

        std::atomic<bool> done(false);

        sentryClient.uploadMinidump(
            sentryMetadata,
            minidumpBytes,
            project,
            key,
            verbose,
            [verbose, &done](const HttpResponsePtr& response) {
                if (verbose)
                {
                    for (auto it : response->headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }

                    spdlog::info("Upload size: {}", response->uploadSize);
                    spdlog::info("Download size: {}", response->downloadSize);

                    spdlog::info("Status: {}", response->statusCode);
                    if (response->errorCode != HttpErrorCode::Ok)
                    {
                        spdlog::info("error message: {}", response->errorMsg);
                    }

                    if (response->headers["Content-Type"] != "application/octet-stream")
                    {
                        spdlog::info("payload: {}", response->payload);
                    }
                }

                if (response->statusCode != 200)
                {
                    spdlog::error("Error sending data to sentry: {}", response->statusCode);
                    spdlog::error("Status: {}", response->statusCode);
                    spdlog::error("Response: {}", response->payload);
                }
                else
                {
                    spdlog::info("Event sent to sentry");
                }

                done = true;
            });

        int i = 0;

        while (!done)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);

            if (i++ > 5000) break; // wait 5 seconds max
        }

        if (!done)
        {
            spdlog::error("Error: timing out trying to sent a crash to sentry");
        }

        return 0;
    }

    int ws_snake_main(int port,
                      const std::string& hostname,
                      const std::string& redisHosts,
                      int redisPort,
                      const std::string& redisPassword,
                      bool verbose,
                      const std::string& appsConfigPath,
                      const SocketTLSOptions& socketTLSOptions,
                      bool disablePong,
                      const std::string& republishChannel)
    {
        snake::AppConfig appConfig;
        appConfig.port = port;
        appConfig.hostname = hostname;
        appConfig.verbose = verbose;
        appConfig.redisPort = redisPort;
        appConfig.redisPassword = redisPassword;
        appConfig.socketTLSOptions = socketTLSOptions;
        appConfig.disablePong = disablePong;
        appConfig.republishChannel = republishChannel;

        // Parse config file
        auto res = readAsString(appsConfigPath);
        bool found = res.first;
        if (!found)
        {
            spdlog::error("Cannot read content of {}", appsConfigPath);
            return 1;
        }

        auto apps = nlohmann::json::parse(res.second);
        appConfig.apps = apps["apps"];

        std::string token;
        std::stringstream tokenStream(redisHosts);
        while (std::getline(tokenStream, token, ';'))
        {
            appConfig.redisHosts.push_back(token);
        }

        // Display config on the terminal for debugging
        dumpConfig(appConfig);

        snake::SnakeServer snakeServer(appConfig);
        snakeServer.runForever();

        return 0; // should never reach this
    }

    int ws_transfer_main(int port,
                         const std::string& hostname,
                         const ix::SocketTLSOptions& tlsOptions)
    {
        spdlog::info("Listening on {}:{}", hostname, port);

        ix::WebSocketServer server(port, hostname);
        server.setTLSOptions(tlsOptions);

        server.setOnClientMessageCallback(
            [&server](std::shared_ptr<ConnectionState> connectionState,
                      ConnectionInfo& connectionInfo,
                      WebSocket& webSocket,
                      const WebSocketMessagePtr& msg) {
                auto remoteIp = connectionInfo.remoteIp;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    spdlog::info("ws_transfer: New connection");
                    spdlog::info("remote ip: {}", remoteIp);
                    spdlog::info("id: {}", connectionState->getId());
                    spdlog::info("Uri: {}", msg->openInfo.uri);
                    spdlog::info("Headers:");
                    for (auto it : msg->openInfo.headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    spdlog::info("ws_transfer: Closed connection: client id {} code {} reason {}",
                                 connectionState->getId(),
                                 msg->closeInfo.code,
                                 msg->closeInfo.reason);
                    auto remaining = server.getClients().size() - 1;
                    spdlog::info("ws_transfer: {} remaining clients", remaining);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    std::stringstream ss;
                    ss << "ws_transfer: Connection error: " << msg->errorInfo.reason << std::endl;
                    ss << "#retries: " << msg->errorInfo.retries << std::endl;
                    ss << "Wait time(ms): " << msg->errorInfo.wait_time << std::endl;
                    ss << "HTTP Status: " << msg->errorInfo.http_status << std::endl;
                    spdlog::info(ss.str());
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    spdlog::info("ws_transfer: Received message fragment ");
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    spdlog::info("ws_transfer: Received {} bytes", msg->wireSize);
                    size_t receivers = 0;
                    for (auto&& client : server.getClients())
                    {
                        if (client.get() != &webSocket)
                        {
                            auto readyState = client->getReadyState();
                            auto id = connectionState->getId();

                            if (readyState == ReadyState::Open)
                            {
                                ++receivers;
                                client->send(
                                    msg->str, msg->binary, [&id](int current, int total) -> bool {
                                        spdlog::info("{}: [client {}]: Step {} out of {}",
                                                     "ws_transfer",
                                                     id,
                                                     current,
                                                     total);
                                        return true;
                                    });
                                do
                                {
                                    size_t bufferedAmount = client->bufferedAmount();

                                    spdlog::info("{}: [client {}]: {} bytes left to send",
                                                 "ws_transfer",
                                                 id,
                                                 bufferedAmount);

                                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                                } while (client->bufferedAmount() != 0 &&
                                         client->getReadyState() == ReadyState::Open);
                            }
                            else
                            {
                                std::string readyStateString =
                                    readyState == ReadyState::Connecting
                                        ? "Connecting"
                                        : readyState == ReadyState::Closing ? "Closing" : "Closed";
                                size_t bufferedAmount = client->bufferedAmount();

                                spdlog::info(
                                    "{}: [client {}]: has readystate {} bytes left to be sent {}",
                                    "ws_transfer",
                                    id,
                                    readyStateString,
                                    bufferedAmount);
                            }
                        }
                    }
                    if (!receivers)
                    {
                        spdlog::info("ws_transfer: no remaining receivers");
                    }
                }
            });

        auto res = server.listen();
        if (!res.first)
        {
            spdlog::info(res.second);
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
} // namespace ix

int main(int argc, char** argv)
{
    ix::initNetSystem();

    ix::CoreLogger::LogFunc logFunc = [](const char* msg, ix::LogLevel level) {
        switch (level)
        {
            case ix::LogLevel::Debug:
            {
                spdlog::debug(msg);
            }
            break;

            case ix::LogLevel::Info:
            {
                spdlog::info(msg);
            }
            break;

            case ix::LogLevel::Warning:
            {
                spdlog::warn(msg);
            }
            break;

            case ix::LogLevel::Error:
            {
                spdlog::error(msg);
            }
            break;

            case ix::LogLevel::Critical:
            {
                spdlog::critical(msg);
            }
            break;
        }
    };
    ix::CoreLogger::setLogFunction(logFunc);
    spdlog::set_level(spdlog::level::debug);

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    // Display command.
    if (getenv("DEBUG"))
    {
        std::stringstream ss;
        ss << "Command: ";
        for (int i = 0; i < argc; ++i)
        {
            ss << argv[i] << " ";
        }
        spdlog::info(ss.str());
    }

    CLI::App app {"ws is a websocket tool"};

    std::string url("ws://127.0.0.1:8008");
    std::string path;
    std::string user;
    std::string data;
    std::string headers;
    std::string output;
    std::string hostname("127.0.0.1");
    std::string pidfile;
    std::string channel;
    std::string filter;
    std::string position;
    std::string message;
    std::string password;
    std::string prefix("ws.test.v0");
    std::string fields;
    std::string gauge;
    std::string timer;
    std::string dsn;
    std::string redisHosts("127.0.0.1");
    std::string redisPassword;
    std::string appsConfigPath("appsConfig.json");
    std::string configPath;
    std::string subprotocol;
    std::string remoteHost;
    std::string minidump;
    std::string metadata;
    std::string project;
    std::string key;
    std::string logfile;
    std::string moduleName;
    std::string republishChannel;
    std::string sendMsg("hello world");
    ix::SocketTLSOptions tlsOptions;
    ix::CobraConfig cobraConfig;
    ix::CobraBotConfig cobraBotConfig;
    std::string ciphers;
    std::string redirectUrl;
    bool headersOnly = false;
    bool followRedirects = false;
    bool verbose = false;
    bool save = false;
    bool quiet = false;
    bool fluentd = false;
    bool compress = false;
    bool stress = false;
    bool disableAutomaticReconnection = false;
    bool disablePerMessageDeflate = false;
    bool greetings = false;
    bool ipv6 = false;
    bool binaryMode = false;
    bool redirect = false;
    bool version = false;
    bool verifyNone = false;
    bool disablePong = false;
    bool noSend = false;
    int port = 8008;
    int redisPort = 6379;
    int statsdPort = 8125;
    int connectTimeOut = 60;
    int transferTimeout = 1800;
    int maxRedirects = 5;
    int delayMs = -1;
    int count = 1;
    uint32_t maxWaitBetweenReconnectionRetries;
    int pingIntervalSecs = 30;

    auto addGenericOptions = [&pidfile](CLI::App* app) {
        app->add_option("--pidfile", pidfile, "Pid file");
    };

    auto addTLSOptions = [&tlsOptions, &verifyNone](CLI::App* app) {
        app->add_option(
               "--cert-file", tlsOptions.certFile, "Path to the (PEM format) TLS cert file")
            ->check(CLI::ExistingPath);
        app->add_option("--key-file", tlsOptions.keyFile, "Path to the (PEM format) TLS key file")
            ->check(CLI::ExistingPath);
        app->add_option("--ca-file", tlsOptions.caFile, "Path to the (PEM format) ca roots file")
            ->check(CLI::ExistingPath);
        app->add_option("--ciphers",
                        tlsOptions.ciphers,
                        "A (comma/space/colon) separated list of ciphers to use for TLS");
        app->add_flag("--tls", tlsOptions.tls, "Enable TLS (server only)");
        app->add_flag("--verify_none", verifyNone, "Disable peer cert verification");
    };

    auto addCobraConfig = [&cobraConfig](CLI::App* app) {
        app->add_option("--appkey", cobraConfig.appkey, "Appkey")->required();
        app->add_option("--endpoint", cobraConfig.endpoint, "Endpoint")->required();
        app->add_option("--rolename", cobraConfig.rolename, "Role name")->required();
        app->add_option("--rolesecret", cobraConfig.rolesecret, "Role secret")->required();
    };

    auto addCobraBotConfig = [&cobraBotConfig](CLI::App* app) {
        app->add_option("--appkey", cobraBotConfig.cobraConfig.appkey, "Appkey")->required();
        app->add_option("--endpoint", cobraBotConfig.cobraConfig.endpoint, "Endpoint")->required();
        app->add_option("--rolename", cobraBotConfig.cobraConfig.rolename, "Role name")->required();
        app->add_option("--rolesecret", cobraBotConfig.cobraConfig.rolesecret, "Role secret")
            ->required();
        app->add_option("--channel", cobraBotConfig.channel, "Channel")->required();
        app->add_option("--filter", cobraBotConfig.filter, "Filter");
        app->add_option("--position", cobraBotConfig.position, "Position");
        app->add_option("--runtime", cobraBotConfig.runtime, "Runtime");
        app->add_option("--heartbeat", cobraBotConfig.enableHeartbeat, "Runtime");
        app->add_option("--heartbeat_timeout", cobraBotConfig.heartBeatTimeout, "Runtime");
        app->add_flag(
            "--limit_received_events", cobraBotConfig.limitReceivedEvents, "Max events per minute");
        app->add_option(
            "--max_events_per_minute", cobraBotConfig.maxEventsPerMinute, "Max events per minute");
        app->add_option("--batch_size", cobraBotConfig.batchSize, "Subscription batch size");
    };

    app.add_flag("--version", version, "Print ws version");
    app.add_option("--logfile", logfile, "path where all logs will be redirected");

    CLI::App* sendApp = app.add_subcommand("send", "Send a file");
    sendApp->fallthrough();
    sendApp->add_option("url", url, "Connection url")->required();
    sendApp->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    sendApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    addGenericOptions(sendApp);
    addTLSOptions(sendApp);

    CLI::App* receiveApp = app.add_subcommand("receive", "Receive a file");
    receiveApp->fallthrough();
    receiveApp->add_option("url", url, "Connection url")->required();
    receiveApp->add_option("--delay",
                           delayMs,
                           "Delay (ms) to wait after receiving a fragment"
                           " to artificially slow down the receiver");
    receiveApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(receiveApp);

    CLI::App* transferApp = app.add_subcommand("transfer", "Broadcasting server");
    transferApp->fallthrough();
    transferApp->add_option("--port", port, "Connection url");
    transferApp->add_option("--host", hostname, "Hostname");
    transferApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(transferApp);

    CLI::App* connectApp = app.add_subcommand("connect", "Connect to a remote server");
    connectApp->fallthrough();
    connectApp->add_option("url", url, "Connection url")->required();
    connectApp->add_option("-H", headers, "Header")->join();
    connectApp->add_flag("-d", disableAutomaticReconnection, "Disable Automatic Reconnection");
    connectApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    connectApp->add_flag("-b", binaryMode, "Send in binary mode");
    connectApp->add_option("--max_wait",
                           maxWaitBetweenReconnectionRetries,
                           "Max Wait Time between reconnection retries");
    connectApp->add_option("--ping_interval", pingIntervalSecs, "Interval between sending pings");
    connectApp->add_option("--subprotocol", subprotocol, "Subprotocol");
    addGenericOptions(connectApp);
    addTLSOptions(connectApp);

    CLI::App* echoClientApp =
        app.add_subcommand("echo_client", "Echo messages sent by a remote server");
    echoClientApp->fallthrough();
    echoClientApp->add_option("url", url, "Connection url")->required();
    echoClientApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    echoClientApp->add_flag("-b", binaryMode, "Send in binary mode");
    echoClientApp->add_option(
        "--ping_interval", pingIntervalSecs, "Interval between sending pings");
    echoClientApp->add_option("--subprotocol", subprotocol, "Subprotocol");
    echoClientApp->add_option("--send_msg", sendMsg, "Send message");
    echoClientApp->add_flag("-m", noSend, "Do not send messages, only receive messages");
    addTLSOptions(echoClientApp);

    CLI::App* chatApp = app.add_subcommand("chat", "Group chat");
    chatApp->fallthrough();
    chatApp->add_option("url", url, "Connection url")->required();
    chatApp->add_option("user", user, "User name")->required();

    CLI::App* echoServerApp = app.add_subcommand("echo_server", "Echo server");
    echoServerApp->fallthrough();
    echoServerApp->add_option("--port", port, "Port");
    echoServerApp->add_option("--host", hostname, "Hostname");
    echoServerApp->add_flag("-q", quiet, "Quiet / only display warnings and errors");
    echoServerApp->add_flag("-g", greetings, "Greet");
    echoServerApp->add_flag("-6", ipv6, "IpV6");
    echoServerApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    echoServerApp->add_flag("-p", disablePong, "Disable sending PONG in response to PING");
    addGenericOptions(echoServerApp);
    addTLSOptions(echoServerApp);

    CLI::App* pushServerApp = app.add_subcommand("push_server", "Push server");
    pushServerApp->fallthrough();
    pushServerApp->add_option("--port", port, "Port");
    pushServerApp->add_option("--host", hostname, "Hostname");
    pushServerApp->add_flag("-q", quiet, "Quiet / only display warnings and errors");
    pushServerApp->add_flag("-g", greetings, "Greet");
    pushServerApp->add_flag("-6", ipv6, "IpV6");
    pushServerApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    pushServerApp->add_flag("-p", disablePong, "Disable sending PONG in response to PING");
    pushServerApp->add_option("--send_msg", sendMsg, "Send message");
    addTLSOptions(pushServerApp);

    CLI::App* broadcastServerApp = app.add_subcommand("broadcast_server", "Broadcasting server");
    broadcastServerApp->fallthrough();
    broadcastServerApp->add_option("--port", port, "Port");
    broadcastServerApp->add_option("--host", hostname, "Hostname");
    addTLSOptions(broadcastServerApp);

    CLI::App* pingPongApp = app.add_subcommand("ping", "Ping pong");
    pingPongApp->fallthrough();
    pingPongApp->add_option("url", url, "Connection url")->required();
    addTLSOptions(pingPongApp);

    CLI::App* httpClientApp = app.add_subcommand("curl", "HTTP Client");
    httpClientApp->fallthrough();
    httpClientApp->add_option("url", url, "Connection url")->required();
    httpClientApp->add_option("-d", data, "Form data")->join();
    httpClientApp->add_option("-F", data, "Form data")->join();
    httpClientApp->add_option("-H", headers, "Header")->join();
    httpClientApp->add_option("--output", output, "Output file");
    httpClientApp->add_flag("-I", headersOnly, "Send a HEAD request");
    httpClientApp->add_flag("-L", followRedirects, "Follow redirects");
    httpClientApp->add_option("--max-redirects", maxRedirects, "Max Redirects");
    httpClientApp->add_flag("-v", verbose, "Verbose");
    httpClientApp->add_flag("-O", save, "Save output to disk");
    httpClientApp->add_flag("--compress", compress, "Enable gzip compression");
    httpClientApp->add_option("--connect-timeout", connectTimeOut, "Connection timeout");
    httpClientApp->add_option("--transfer-timeout", transferTimeout, "Transfer timeout");
    addTLSOptions(httpClientApp);

    CLI::App* redisCliApp = app.add_subcommand("redis_cli", "Redis cli");
    redisCliApp->fallthrough();
    redisCliApp->add_option("--port", redisPort, "Port");
    redisCliApp->add_option("--host", hostname, "Hostname");
    redisCliApp->add_option("--password", password, "Password");

    CLI::App* redisPublishApp = app.add_subcommand("redis_publish", "Redis publisher");
    redisPublishApp->fallthrough();
    redisPublishApp->add_option("--port", redisPort, "Port");
    redisPublishApp->add_option("--host", hostname, "Hostname");
    redisPublishApp->add_option("--password", password, "Password");
    redisPublishApp->add_option("channel", channel, "Channel")->required();
    redisPublishApp->add_option("message", message, "Message")->required();
    redisPublishApp->add_option("-c", count, "Count");

    CLI::App* redisSubscribeApp = app.add_subcommand("redis_subscribe", "Redis subscriber");
    redisSubscribeApp->fallthrough();
    redisSubscribeApp->add_option("--port", redisPort, "Port");
    redisSubscribeApp->add_option("--host", hostname, "Hostname");
    redisSubscribeApp->add_option("--password", password, "Password");
    redisSubscribeApp->add_option("channel", channel, "Channel")->required();
    redisSubscribeApp->add_flag("-v", verbose, "Verbose");
    redisSubscribeApp->add_option("--pidfile", pidfile, "Pid file");

    CLI::App* cobraSubscribeApp = app.add_subcommand("cobra_subscribe", "Cobra subscriber");
    cobraSubscribeApp->fallthrough();
    cobraSubscribeApp->add_option("--pidfile", pidfile, "Pid file");
    cobraSubscribeApp->add_flag("-q", quiet, "Quiet / only display stats");
    cobraSubscribeApp->add_flag("--fluentd", fluentd, "Write fluentd prefix");
    addTLSOptions(cobraSubscribeApp);
    addCobraBotConfig(cobraSubscribeApp);

    CLI::App* cobraPublish = app.add_subcommand("cobra_publish", "Cobra publisher");
    cobraPublish->fallthrough();
    cobraPublish->add_option("--channel", channel, "Channel")->required();
    cobraPublish->add_option("--pidfile", pidfile, "Pid file");
    cobraPublish->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    addTLSOptions(cobraPublish);
    addCobraConfig(cobraPublish);

    CLI::App* cobraMetricsPublish =
        app.add_subcommand("cobra_metrics_publish", "Cobra metrics publisher");
    cobraMetricsPublish->fallthrough();
    cobraMetricsPublish->add_option("--channel", channel, "Channel")->required();
    cobraMetricsPublish->add_option("--pidfile", pidfile, "Pid file");
    cobraMetricsPublish->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    cobraMetricsPublish->add_flag("--stress", stress, "Stress mode");
    addTLSOptions(cobraMetricsPublish);
    addCobraConfig(cobraMetricsPublish);

    CLI::App* cobra2statsd = app.add_subcommand("cobra_to_statsd", "Cobra to statsd");
    cobra2statsd->fallthrough();
    cobra2statsd->add_option("--host", hostname, "Statsd host");
    cobra2statsd->add_option("--port", statsdPort, "Statsd port");
    cobra2statsd->add_option("--prefix", prefix, "Statsd prefix");
    cobra2statsd->add_option("--fields", fields, "Extract fields for naming the event")->join();
    cobra2statsd->add_option("--gauge", gauge, "Value to extract, and use as a statsd gauge")
        ->join();
    cobra2statsd->add_option("--timer", timer, "Value to extract, and use as a statsd timer")
        ->join();
    cobra2statsd->add_flag("-v", verbose, "Verbose");
    cobra2statsd->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(cobra2statsd);
    addCobraBotConfig(cobra2statsd);

    CLI::App* cobra2python = app.add_subcommand("cobra_to_python", "Cobra to python");
    cobra2python->fallthrough();
    cobra2python->add_option("--host", hostname, "Statsd host");
    cobra2python->add_option("--port", statsdPort, "Statsd port");
    cobra2python->add_option("--prefix", prefix, "Statsd prefix");
    cobra2python->add_option("--module", moduleName, "Python module");
    cobra2python->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(cobra2python);
    addCobraBotConfig(cobra2python);

    CLI::App* cobra2sentry = app.add_subcommand("cobra_to_sentry", "Cobra to sentry");
    cobra2sentry->fallthrough();
    cobra2sentry->add_option("--dsn", dsn, "Sentry DSN");
    cobra2sentry->add_flag("-v", verbose, "Verbose");
    cobra2sentry->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(cobra2sentry);
    addCobraBotConfig(cobra2sentry);

    CLI::App* cobra2redisApp =
        app.add_subcommand("cobra_metrics_to_redis", "Cobra metrics to redis");
    cobra2redisApp->fallthrough();
    cobra2redisApp->add_option("--pidfile", pidfile, "Pid file");
    cobra2redisApp->add_option("--hostname", hostname, "Redis hostname");
    cobra2redisApp->add_option("--port", redisPort, "Redis port");
    cobra2redisApp->add_flag("-v", verbose, "Verbose");
    addTLSOptions(cobra2redisApp);
    addCobraBotConfig(cobra2redisApp);

    CLI::App* snakeApp = app.add_subcommand("snake", "Snake server");
    snakeApp->fallthrough();
    snakeApp->add_option("--port", port, "Connection url");
    snakeApp->add_option("--host", hostname, "Hostname");
    snakeApp->add_option("--pidfile", pidfile, "Pid file");
    snakeApp->add_option("--redis_hosts", redisHosts, "Redis hosts");
    snakeApp->add_option("--redis_port", redisPort, "Redis hosts");
    snakeApp->add_option("--redis_password", redisPassword, "Redis password");
    snakeApp->add_option("--apps_config_path", appsConfigPath, "Path to auth data")
        ->check(CLI::ExistingPath);
    snakeApp->add_option("--republish_channel", republishChannel, "Republish channel");
    snakeApp->add_flag("-v", verbose, "Verbose");
    snakeApp->add_flag("-d", disablePong, "Disable Pongs");
    addTLSOptions(snakeApp);

    CLI::App* httpServerApp = app.add_subcommand("httpd", "HTTP server");
    httpServerApp->fallthrough();
    httpServerApp->add_option("--port", port, "Port");
    httpServerApp->add_option("--host", hostname, "Hostname");
    httpServerApp->add_flag("-L", redirect, "Redirect all request to redirect_url");
    httpServerApp->add_option("--redirect_url", redirectUrl, "Url to redirect to");
    addTLSOptions(httpServerApp);

    CLI::App* autobahnApp = app.add_subcommand("autobahn", "Test client Autobahn compliance");
    autobahnApp->fallthrough();
    autobahnApp->add_option("--url", url, "url");
    autobahnApp->add_flag("-q", quiet, "Quiet");

    CLI::App* redisServerApp = app.add_subcommand("redis_server", "Redis server");
    redisServerApp->fallthrough();
    redisServerApp->add_option("--port", port, "Port");
    redisServerApp->add_option("--host", hostname, "Hostname");

    CLI::App* proxyServerApp = app.add_subcommand("proxy_server", "Proxy server");
    proxyServerApp->fallthrough();
    proxyServerApp->add_option("--port", port, "Port");
    proxyServerApp->add_option("--host", hostname, "Hostname");
    proxyServerApp->add_option("--remote_host", remoteHost, "Remote Hostname");
    proxyServerApp->add_flag("-v", verbose, "Verbose");
    proxyServerApp->add_option("--config_path", configPath, "Path to config data")
        ->check(CLI::ExistingPath);
    addGenericOptions(proxyServerApp);
    addTLSOptions(proxyServerApp);

    CLI::App* minidumpApp = app.add_subcommand("upload_minidump", "Upload a minidump to sentry");
    minidumpApp->fallthrough();
    minidumpApp->add_option("--minidump", minidump, "Minidump path")
        ->required()
        ->check(CLI::ExistingPath);
    minidumpApp->add_option("--metadata", metadata, "Metadata path")
        ->required()
        ->check(CLI::ExistingPath);
    minidumpApp->add_option("--project", project, "Sentry Project")->required();
    minidumpApp->add_option("--key", key, "Sentry Key")->required();
    minidumpApp->add_flag("-v", verbose, "Verbose");

    CLI::App* dnsLookupApp = app.add_subcommand("dnslookup", "DNS lookup");
    dnsLookupApp->fallthrough();
    dnsLookupApp->add_option("host", hostname, "Hostname")->required();

    CLI11_PARSE(app, argc, argv);

    // pid file handling
    if (!pidfile.empty())
    {
        unlink(pidfile.c_str());

        std::ofstream f;
        f.open(pidfile);
        f << getpid();
        f.close();
    }

    if (verifyNone)
    {
        tlsOptions.caFile = "NONE";
    }

    if (tlsOptions.isUsingSystemDefaults())
    {
#if defined(__APPLE__)
#if defined(IXWEBSOCKET_USE_MBED_TLS) || defined(IXWEBSOCKET_USE_OPEN_SSL)
        // We could try to load some system certs as well, but this is easy enough
        tlsOptions.caFile = "/etc/ssl/cert.pem";
#endif
#elif defined(__linux__)
#if defined(IXWEBSOCKET_USE_MBED_TLS)
        std::vector<std::string> caFiles = {
            "/etc/ssl/certs/ca-bundle.crt",       // CentOS
            "/etc/ssl/certs/ca-certificates.crt", // Alpine
        };

        for (auto&& caFile : caFiles)
        {
            if (fileExists(caFile))
            {
                tlsOptions.caFile = caFile;
                break;
            }
        }
#endif
#endif
    }

    if (!logfile.empty())
    {
        try
        {
            auto fileLogger = spdlog::basic_logger_mt("ws", logfile);
            spdlog::set_default_logger(fileLogger);
            spdlog::flush_every(std::chrono::seconds(1));

            std::cerr << "All logs will be redirected to " << logfile << std::endl;
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            std::cerr << "Fatal error, log init failed: " << ex.what() << std::endl;
            ix::uninitNetSystem();
            return 1;
        }
    }

    if (quiet)
    {
        spdlog::set_level(spdlog::level::info);
    }

    // Cobra config
    cobraConfig.webSocketPerMessageDeflateOptions = ix::WebSocketPerMessageDeflateOptions(true);
    cobraConfig.socketTLSOptions = tlsOptions;

    cobraBotConfig.cobraConfig.webSocketPerMessageDeflateOptions =
        ix::WebSocketPerMessageDeflateOptions(true);
    cobraBotConfig.cobraConfig.socketTLSOptions = tlsOptions;

    int ret = 1;
    if (app.got_subcommand("connect"))
    {
        ret = ix::ws_connect_main(url,
                                  headers,
                                  disableAutomaticReconnection,
                                  disablePerMessageDeflate,
                                  binaryMode,
                                  maxWaitBetweenReconnectionRetries,
                                  tlsOptions,
                                  subprotocol,
                                  pingIntervalSecs);
    }
    else if (app.got_subcommand("echo_client"))
    {
        ret = ix::ws_echo_client(url,
                                 disablePerMessageDeflate,
                                 binaryMode,
                                 tlsOptions,
                                 subprotocol,
                                 pingIntervalSecs,
                                 sendMsg,
                                 noSend);
    }
    else if (app.got_subcommand("echo_server"))
    {
        ret = ix::ws_echo_server_main(
            port, greetings, hostname, tlsOptions, ipv6, disablePerMessageDeflate, disablePong);
    }
    else if (app.got_subcommand("push_server"))
    {
        ret = ix::ws_push_server(port,
                                 greetings,
                                 hostname,
                                 tlsOptions,
                                 ipv6,
                                 disablePerMessageDeflate,
                                 disablePong,
                                 sendMsg);
    }
    else if (app.got_subcommand("transfer"))
    {
        ret = ix::ws_transfer_main(port, hostname, tlsOptions);
    }
    else if (app.got_subcommand("send"))
    {
        ret = ix::ws_send_main(url, path, disablePerMessageDeflate, tlsOptions);
    }
    else if (app.got_subcommand("receive"))
    {
        bool enablePerMessageDeflate = false;
        ret = ix::ws_receive_main(url, enablePerMessageDeflate, delayMs, tlsOptions);
    }
    else if (app.got_subcommand("chat"))
    {
        ret = ix::ws_chat_main(url, user);
    }
    else if (app.got_subcommand("broadcast_server"))
    {
        ret = ix::ws_broadcast_server_main(port, hostname, tlsOptions);
    }
    else if (app.got_subcommand("ping"))
    {
        ret = ix::ws_ping_pong_main(url, tlsOptions);
    }
    else if (app.got_subcommand("curl"))
    {
        ret = ix::ws_http_client_main(url,
                                      headers,
                                      data,
                                      headersOnly,
                                      connectTimeOut,
                                      transferTimeout,
                                      followRedirects,
                                      maxRedirects,
                                      verbose,
                                      save,
                                      output,
                                      compress,
                                      tlsOptions);
    }
    else if (app.got_subcommand("redis_cli"))
    {
        ret = ix::ws_redis_cli_main(hostname, redisPort, password);
    }
    else if (app.got_subcommand("redis_publish"))
    {
        ret = ix::ws_redis_publish_main(hostname, redisPort, password, channel, message, count);
    }
    else if (app.got_subcommand("redis_subscribe"))
    {
        ret = ix::ws_redis_subscribe_main(hostname, redisPort, password, channel, verbose);
    }
    else if (app.got_subcommand("cobra_subscribe"))
    {
        int64_t sentCount = ix::cobra_to_stdout_bot(cobraBotConfig, fluentd, quiet);
        ret = (int) sentCount;
    }
    else if (app.got_subcommand("cobra_publish"))
    {
        ret = ix::ws_cobra_publish_main(cobraConfig, channel, path);
    }
    else if (app.got_subcommand("cobra_metrics_publish"))
    {
        ret = ix::ws_cobra_metrics_publish_main(cobraConfig, channel, path, stress);
    }
    else if (app.got_subcommand("cobra_to_statsd"))
    {
        if (!timer.empty() && !gauge.empty())
        {
            spdlog::error("--gauge and --timer options are exclusive. "
                          "you can only supply one");
            ret = 1;
        }
        else
        {
            ix::StatsdClient statsdClient(hostname, statsdPort, prefix, verbose);

            std::string errMsg;
            bool initialized = statsdClient.init(errMsg);
            if (!initialized)
            {
                spdlog::error(errMsg);
                ret = 1;
            }
            else
            {
                ret = (int) ix::cobra_to_statsd_bot(
                    cobraBotConfig, statsdClient, fields, gauge, timer, verbose);
            }
        }
    }
    else if (app.got_subcommand("cobra_to_python"))
    {
        ix::StatsdClient statsdClient(hostname, statsdPort, prefix, verbose);

        std::string errMsg;
        bool initialized = statsdClient.init(errMsg);
        if (!initialized)
        {
            spdlog::error(errMsg);
            ret = 1;
        }
        else
        {
            ret = (int) ix::cobra_to_python_bot(cobraBotConfig, statsdClient, moduleName);
        }
    }
    else if (app.got_subcommand("cobra_to_sentry"))
    {
        ix::SentryClient sentryClient(dsn);
        sentryClient.setTLSOptions(tlsOptions);

        ret = (int) ix::cobra_to_sentry_bot(cobraBotConfig, sentryClient, verbose);
    }
    else if (app.got_subcommand("cobra_metrics_to_redis"))
    {
        ix::RedisClient redisClient;
        if (!redisClient.connect(hostname, redisPort))
        {
            spdlog::error("Cannot connect to redis host {}:{}", redisHosts, redisPort);
            return 1;
        }
        else
        {
            ret = (int) ix::cobra_metrics_to_redis_bot(cobraBotConfig, redisClient, verbose);
        }
    }
    else if (app.got_subcommand("snake"))
    {
        ret = ix::ws_snake_main(port,
                                hostname,
                                redisHosts,
                                redisPort,
                                redisPassword,
                                verbose,
                                appsConfigPath,
                                tlsOptions,
                                disablePong,
                                republishChannel);
    }
    else if (app.got_subcommand("httpd"))
    {
        ret = ix::ws_httpd_main(port, hostname, redirect, redirectUrl, tlsOptions);
    }
    else if (app.got_subcommand("autobahn"))
    {
        ret = ix::ws_autobahn_main(url, quiet);
    }
    else if (app.got_subcommand("redis_server"))
    {
        ret = ix::ws_redis_server_main(port, hostname);
    }
    else if (app.got_subcommand("proxy_server"))
    {
        ix::RemoteUrlsMapping remoteUrlsMapping;

        if (!configPath.empty())
        {
            auto res = readAsString(configPath);
            bool found = res.first;
            if (!found)
            {
                spdlog::error("Cannot read config file {} from disk", configPath);
            }
            else
            {
                auto jsonData = nlohmann::json::parse(res.second);
                auto remoteUrls = jsonData["remote_urls"];

                for (auto& el : remoteUrls.items())
                {
                    remoteUrlsMapping[el.key()] = el.value();
                }
            }
        }

        ret = ix::websocket_proxy_server_main(
            port, hostname, tlsOptions, remoteHost, remoteUrlsMapping, verbose);
    }
    else if (app.got_subcommand("upload_minidump"))
    {
        ret = ix::ws_sentry_minidump_upload(metadata, minidump, project, key, verbose);
    }
    else if (app.got_subcommand("dnslookup"))
    {
        ret = ix::ws_dns_lookup(hostname);
    }
    else if (version)
    {
        std::cout << "ws " << ix::userAgent() << std::endl;
    }
    else
    {
        spdlog::error("A subcommand or --version is required");
    }

    ix::uninitNetSystem();
    return ret;
}
