#include "IXTest.h"
#include "catch.hpp"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

using namespace ix;

TEST_CASE("IPv6")
{
    SECTION("Listening on ::1 works with AF_INET6 works")
    {
        int port = getFreePort();
        ix::WebSocketServer server(port,
                                   "::1",
                                   SocketServer::kDefaultTcpBacklog,
                                   SocketServer::kDefaultMaxConnections,
                                   WebSocketServer::kDefaultHandShakeTimeoutSecs,
                                   AF_INET6);

        auto res = server.listen();
        CHECK(res.first);
        server.start();
        server.stop();
    }
}
