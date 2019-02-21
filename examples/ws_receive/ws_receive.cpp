/*
 *  ws_receive.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <ixwebsocket/IXWebSocketServer.h>
#include <jsoncpp/json/json.h>
#include <ixcrypto/IXBase64.h>
#include <ixcrypto/IXHash.h>


namespace
{
    // We should cleanup the file name and full path further to remove .. as well
    std::string extractFilename(const std::string& path)
    {
        std::string filename("filename.conf");
        std::string::size_type idx;

        idx = path.rfind('/');
        if (idx != std::string::npos)
        {
            std::string filename = path.substr(idx+1);
            return filename;
        }
        else
        {
            return std::string();
        }
    }
}

namespace ix
{
    void errorHandler(const std::string& errMsg,
                      const std::string& id,
                      std::shared_ptr<ix::WebSocket> webSocket)
    {
        Json::Value pdu;
        pdu["kind"] = "error";
        pdu["id"] = id;
        pdu["message"] = errMsg;
        webSocket->send(pdu.toStyledString());
    }

    void messageHandler(const std::string& str,
                        std::shared_ptr<ix::WebSocket> webSocket)
    {
        std::cerr << "Received message: " << str.size() << std::endl;

        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(str, data))
        {
            errorHandler("Invalid JSON", std::string(), webSocket);
            return;
        }

        std::cout << "id: " << data["id"].asString() << std::endl;

        std::string content = ix::base64_decode(data["content"].asString());
        std::cout << "Content size: " << content.size() << std::endl;

        // Validate checksum
        uint64_t cksum = ix::djb2Hash(data["content"].asString());
        uint64_t cksumRef = data["djb2_hash"].asUInt64();

        std::cout << "Computed hash: " << cksum << std::endl;
        std::cout << "Reference hash: " << cksumRef << std::endl;

        if (cksum != cksumRef)
        {
            errorHandler("Hash mismatch.", std::string(), webSocket);
            return;
        }

        std::string filename = data["filename"].asString();
        filename = extractFilename(filename);

        std::ofstream out(filename);
        out << content;
        out.close();

        Json::Value pdu;
        pdu["ack"] = true;
        pdu["id"] = data["id"];
        pdu["filename"] = data["filename"];
        webSocket->send(pdu.toStyledString());
    }
}

int main(int argc, char** argv)
{
    int port = 8080;
    if (argc == 2)
    {
        std::stringstream ss;
        ss << argv[1];
        ss >> port;
    }

    ix::WebSocketServer server(port);

    server.setOnConnectionCallback(
        [&server](std::shared_ptr<ix::WebSocket> webSocket)
        {
            webSocket->setOnMessageCallback(
                [webSocket, &server](ix::WebSocketMessageType messageType,
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
                        std::cerr << "Closed connection" << std::endl;
                    }
                    else if (messageType == ix::WebSocket_MessageType_Message)
                    {
                        messageHandler(str, webSocket);
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
