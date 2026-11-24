/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <random>

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        return 2;
    }
    if (!ix::initNetSystem())
    {
        return 1;
    }

    struct Cleanup
    {
        ~Cleanup()
        {
            ix::uninitNetSystem();
        }
    } cleanup;

    const bool compression = std::string(argv[3]) == "on";
    const bool serverClose = std::string(argv[4]) == "server";
    std::string large(256 * 1024, '\0');
    std::mt19937 random(0);
    for (auto& byte : large)
    {
        byte = static_cast<char>(random());
    }
    const bool cancel = std::string(argv[4]) == "cancel";
    const bool duplex = std::string(argv[4]) == "duplex";
    std::vector<std::pair<std::string, bool>> messages {{"", false},
                                                        {"Bonjour — 中文 👋", false},
                                                        {"", true},
                                                        {std::string("\0\xff\x80"
                                                                     "binary",
                                                                     9),
                                                         true},
                                                        {large, true}};
    if (duplex)
    {
        uint32_t state = 0x12345678;
        std::string prefill(256 * 1024, '\0');
        for (auto& byte : prefill)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            byte = static_cast<char>(state);
        }
        messages.insert(messages.begin(), {prefill, true});
    }
    std::promise<bool> finished;
    auto result = finished.get_future();
    std::atomic<bool> done {false};
    size_t received = 0;
    bool pong = false, closing = false;
    ix::WebSocket client;
    client.setUrl(argv[1]);
    client.setHttp2(true);
    client.disableAutomaticReconnection();
    client.setHandshakeTimeout(5);
    ix::SocketTLSOptions options;
    options.caFile = argv[2];
    client.setTLSOptions(options);
    if (!compression)
    {
        client.disablePerMessageDeflate();
    }
    auto finish = [&](bool success)
    {
        if (!done.exchange(true))
        {
            finished.set_value(success);
        }
    };
    if (duplex)
    {
        client.setExtraHeaders({{"Origin", "https://client.example"},
                                {"X-Proxy-Test", "h2-integration"},
                                {"X-H2-Duplex", "1"}});
    }
    else
    {
        client.setExtraHeaders(
            {{"Origin", "https://client.example"}, {"X-Proxy-Test", "h2-integration"}});
    }
    client.setOnMessageCallback(
        [&](const ix::WebSocketMessagePtr& message)
        {
            if (message->type == ix::WebSocketMessageType::Open)
            {
                if (cancel)
                {
                    finish(client.sendBinary(large).success);
                    return;
                }
                for (size_t i = duplex ? 1 : 0; i < messages.size(); ++i)
                {
                    const auto& payload = messages[i];
                    if (!client.send(payload.first, payload.second).success)
                    {
                        finish(false);
                        return;
                    }
                }
                client.ping("native-ping");
            }
            else if (message->type == ix::WebSocketMessageType::Message)
            {
                if (received == messages.size() || message->str != messages[received].first ||
                    message->binary != messages[received].second)
                {
                    std::cerr << "Message mismatch at " << received << ": got "
                              << message->str.size() << " bytes binary=" << message->binary << '\n';
                    finish(false);
                    return;
                }
                ++received;
            }
            else if (message->type == ix::WebSocketMessageType::Pong)
            {
                if (message->str != "native-ping")
                {
                    finish(false);
                    return;
                }
                pong = true;
            }
            else if (message->type == ix::WebSocketMessageType::Error)
            {
                std::cerr << message->errorInfo.reason << '\n';
                finish(false);
            }
            else if (message->type == ix::WebSocketMessageType::Close)
            {
                finish(received == messages.size() && pong &&
                       message->closeInfo.code == (serverClose ? 4000 : 1000) &&
                       message->closeInfo.reason ==
                           (serverClose ? "server close test" : "native close"));
            }
            if (!closing && received == messages.size() && pong)
            {
                closing = true;
                if (serverClose)
                {
                    client.sendText("close-from-server");
                }
                else
                {
                    client.close(1000, "native close");
                }
            }
        });
    client.start();
    const bool ready = result.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    bool success = ready && result.get();
    const auto stopping = std::chrono::steady_clock::now();
    client.stop();
    if (cancel)
    {
        const auto elapsed = std::chrono::steady_clock::now() - stopping;
        success = success && elapsed < std::chrono::seconds(2);
    }
    if (!success)
    {
        std::cerr << "Native HTTP/2 client failed; received " << received
                  << " messages (completed=" << ready << ")\n";
    }
    else
    {
        std::cout << "PASS native HTTP/2 client compression=" << compression << " close=" << argv[4]
                  << '\n';
    }
    return success ? 0 : 1;
}
