/*
 *  ws_broadcast_server.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <ixwebsocket/IXWebSocketServer.h>

namespace ix
{
    int ws_echo_server_main(int port, const std::string& hostname)
    {
        std::cout << "Listening on " << hostname << ":" << port << std::endl;

        ix::WebSocketServer server(port, hostname);

        server.setOnConnectionCallback(
            [](std::shared_ptr<ix::WebSocket> webSocket)
            {
                webSocket->setOnMessageCallback(
                    [webSocket](ix::WebSocketMessageType messageType,
                       const std::string& str,
                       size_t wireSize,
                       const ix::WebSocketErrorInfo& error,
                       const ix::WebSocketOpenInfo& openInfo,
                       const ix::WebSocketCloseInfo& closeInfo)
                    {
                        if (messageType == ix::WebSocket_MessageType_Open)
                        {
                            std::cerr << "New connection" << std::endl;
                            std::cerr << "Uri: " << openInfo.uri << std::endl;
                            std::cerr << "Headers:" << std::endl;
                            for (auto it : openInfo.headers)
                            {
                                std::cerr << it.first << ": " << it.second << std::endl;
                            }
                        }
                        else if (messageType == ix::WebSocket_MessageType_Close)
                        {
                            std::cerr << "Closed connection"
                                      << " code " << closeInfo.code
                                      << " reason " << closeInfo.reason << std::endl;
                        }
                        else if (messageType == ix::WebSocket_MessageType_Error)
                        {
                            std::stringstream ss;
                            ss << "Connection error: " << error.reason      << std::endl;
                            ss << "#retries: "         << error.retries     << std::endl;
                            ss << "Wait time(ms): "    << error.wait_time   << std::endl;
                            ss << "HTTP Status: "      << error.http_status << std::endl;
                            std::cerr << ss.str();
                        }
                        else if (messageType == ix::WebSocket_MessageType_Message)
                        {
                            std::cerr << "Received "
                                      << wireSize << " bytes"
                                      << std::endl;
                            webSocket->send(str);
                        }
                    }
                );
            }
        );

        auto res = server.listen();
        if (!res.first)
        {
            std::cerr << res.second << std::endl;
            return 1;
        }

        server.start();
        server.wait();

        return 0;
    }
}
