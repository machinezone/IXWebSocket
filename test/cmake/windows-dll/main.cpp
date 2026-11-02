// Exercise public default arguments and DLL data imports in a separate consumer.
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSelectInterrupt.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

int main()
{
    if (!ix::initNetSystem()) return 1;
    {
        ix::WebSocket client;
        client.close();
        ix::WebSocketServer server;
        ix::HttpServer http;
        ix::WebSocketPerMessageDeflateOptions deflate;
        if (ix::WebSocketCloseConstants::kNormalClosureCode != 1000 ||
            ix::WebSocketCloseConstants::kNormalClosureMessage.empty() ||
            ix::SocketServer::kDefaultHost.empty() || ix::HttpClient::kGet != "GET" ||
            ix::SelectInterrupt::kSendRequest == ix::SelectInterrupt::kCloseRequest ||
            deflate.getClientMaxWindowBits() != 15 || server.getPort() != ix::SocketServer::kDefaultPort)
            return 2;
    }
    return ix::uninitNetSystem() ? 0 : 3;
}
