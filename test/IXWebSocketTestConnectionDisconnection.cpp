/*
 *  IXWebSocketTestConnectionDisconnection.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <set>
#include <ixwebsocket/IXWebSocket.h>
#include "IXTest.h"

#include "catch.hpp"

using namespace ix;

namespace
{
    const std::string WEBSOCKET_DOT_ORG_URL("wss://echo.websocket.org");
    const std::string GOOGLE_URL("wss://google.com");
    const std::string UNKNOWN_URL("wss://asdcasdcaasdcasdcasdcasdcasdcasdcasassdd.com");
}

namespace
{
    class IXWebSocketTestConnectionDisconnection
    {
        public:
            IXWebSocketTestConnectionDisconnection();
            void start(const std::string& url);
            void stop();

        private:
            ix::WebSocket _webSocket;
    };

    IXWebSocketTestConnectionDisconnection::IXWebSocketTestConnectionDisconnection()
    {
        ;
    }

    void IXWebSocketTestConnectionDisconnection::stop()
    {
        _webSocket.stop();
    }

    void IXWebSocketTestConnectionDisconnection::start(const std::string& url)
    {
        _webSocket.setUrl(url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + url);

        _webSocket.setOnMessageCallback(
            [](ix::WebSocketMessageType messageType,
               const std::string& str,
               size_t wireSize,
               const ix::WebSocketErrorInfo& error,
               const ix::WebSocketOpenInfo& openInfo,
               const ix::WebSocketCloseInfo& closeInfo)
            {
                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    log("cmd_websocket_satori_chat: connected !");
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    log("cmd_websocket_satori_chat: disconnected !");
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    ss << "cmd_websocket_satori_chat: Error! ";
                    ss << error.reason;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    log("cmd_websocket_satori_chat: received message.!");
                }
                else if (messageType == ix::WebSocket_MessageType_Ping)
                {
                    log("cmd_websocket_satori_chat: received ping message.!");
                }
                else if (messageType == ix::WebSocket_MessageType_Pong)
                {
                    log("cmd_websocket_satori_chat: received pong message.!");
                }
                else if (messageType == ix::WebSocket_MessageType_Fragment)
                {
                    log("cmd_websocket_satori_chat: received fragment.!");
                }
                else
                {
                    log("Invalid ix::WebSocketMessageType");
                }
            });

        // Start the connection
        _webSocket.start();
    }
}

//
// We try to connect to different servers, and make sure there are no crashes.
// FIXME: We could do more checks (make sure that we were not able to connect to unknown servers, etc...)
//
TEST_CASE("websocket_connections", "[websocket]")
{
    SECTION("Try to connect to invalid servers.")
    {
        IXWebSocketTestConnectionDisconnection chatA;

        chatA.start(GOOGLE_URL);
        ix::msleep(1000);
        chatA.stop();

        chatA.start(UNKNOWN_URL);
        ix::msleep(1000);
        chatA.stop();
    }

    SECTION("Try to connect and disconnect with different timing, not enough time to succesfully connect")
    {
        IXWebSocketTestConnectionDisconnection chatA;
        for (int i = 0; i < 50; ++i)
        {
            log(std::string("Run: ") + std::to_string(i));
            chatA.start(WEBSOCKET_DOT_ORG_URL);
            ix::msleep(i);
            chatA.stop();
        }
    }

    /*SECTION("Try to connect and disconnect with different timing, from not enough time to successfull connect")
    {
        IXWebSocketTestConnectionDisconnection chatA;
        for (int i = 0; i < 20; ++i)
        {
            log(std::string("Run: ") + std::to_string(i));
            chatA.start(WEBSOCKET_DOT_ORG_URL);
            ix::msleep(i*50);
            chatA.stop();
        }
    }*/
}
