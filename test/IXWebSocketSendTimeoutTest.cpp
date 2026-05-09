#include "IXTest.h"
#include "catch.hpp"
#include "ixwebsocket/IXWebSocketMessageType.h"
#include <ixwebsocket/IXUrlParser.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <memory>

using namespace ix;

static std::atomic<bool> client_connected {false};
static std::atomic<bool> client_closed {false};

TEST_CASE("SendTimeout")
{
    SECTION("Test send timeout kicking in")
    {
        // Create a server with a one second send timeout
        int port = getFreePort();
        std::unique_ptr<ix::WebSocketServer> server = std::unique_ptr<ix::WebSocketServer>(
            new ix::WebSocketServer(port,
                                    "127.0.0.1",
                                    SocketServer::kDefaultTcpBacklog,
                                    SocketServer::kDefaultMaxConnections,
                                    WebSocketServer::kDefaultHandShakeTimeoutSecs,
                                    AF_INET,
                                    /*pingIntervalSeconds=*/5,
                                    /*sendTimeoutSeconds=*/1));

        auto res = server->listen();
        REQUIRE(res.first);

        server->setOnConnectionCallback(
            [](std::weak_ptr<WebSocket> wws, std::shared_ptr<ConnectionState> cs) -> void
            {
                TLogger() << "Client connected!";
                auto ws = wws.lock();
                client_connected = true;

                // When the client sends a message, send it 50k messages back
                // to quickly fill up the socket buffer and run into a send
                // timeout.
                ws->setOnMessageCallback(
                    [ws](const WebSocketMessagePtr& wsmptr)
                    {
                        if (wsmptr->type == WebSocketMessageType::Message)
                        {
                            auto i = 0;
                            while (++i < 50000)
                            {
                                auto r = ws->sendText("SPAM SPAM SPAM SPAM SPAM SPAM!");
                                if (!r.success)
                                {
                                    ws->close();
                                    break;
                                }
                            }
                        }
                        else if (wsmptr->type == WebSocketMessageType::Close)
                        {
                            TLogger()
                                << "SERVER: Client connection closed:" << wsmptr->closeInfo.reason;
                            client_closed = true;
                        }
                    });
            });

        std::string url = "ws://127.0.0.1:" + std::to_string(port) + "/";
        ix::WebSocket client;
        client.setUrl(url);

        client.setOnMessageCallback(
            [&client](const ix::WebSocketMessagePtr& msg)
            {
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    TLogger() << "CLIENT: Open";
                    client.sendText("Hello");
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    TLogger() << "CLIENT: Close";
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    auto r = client.sendText("Hello, again!");

                    // Block the client thread after sending a message
                    // to make the socket buffers run full.
                    if (r.success) msleep(1000);
                }
            });

        server->start();
        client.start();

        // Wait for client to connect and be closed again.
        while (!client_connected || !client_closed)
        {
            msleep(10);
        }

        server->stop();
    }
}
