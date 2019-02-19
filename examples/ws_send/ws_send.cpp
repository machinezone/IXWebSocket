/*
 *  ws_send.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>
#include <ixcrypto/IXUuid.h>
#include <ixcrypto/IXBase64.h>
#include <ixcrypto/IXHash.h>
#include <jsoncpp/json/json.h>

using namespace ix;

namespace
{
    void log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    class WebSocketSender
    {
        public:
            WebSocketSender(const std::string& _url);

            void subscribe(const std::string& channel);
            void start();
            void stop();

            void waitForConnection();
            void waitForAck();

            void sendMessage(const std::string& filename);

        private:
            std::string _url;
            std::string _id;
            ix::WebSocket _webSocket;

            std::mutex _conditionVariableMutex;
            std::condition_variable _condition;
    };

    WebSocketSender::WebSocketSender(const std::string& url) :
        _url(url)
    {
        ;
    }

    void WebSocketSender::stop()
    {
        _webSocket.stop();
    }

    void WebSocketSender::waitForConnection()
    {
        std::cout << "Connecting..." << std::endl;

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    void WebSocketSender::waitForAck()
    {
        std::cout << "Waiting for ack..." << std::endl;

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    // FIXME: read directly to a string
    std::string load(const std::string& path)
    {   
        std::vector<uint8_t> memblock;
            
        std::ifstream file(path);
        if (!file.is_open()) return std::string();
                
        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);
                    
        memblock.resize(size);
        file.read((char*)&memblock.front(), static_cast<std::streamsize>(size));

        return std::string(memblock.begin(), memblock.end());
    }

    void WebSocketSender::start()
    {
        _webSocket.setUrl(_url);

        // Disable zlib compression / TODO: make this configurable
        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            false, false, false, 15, 15);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback(
            [this](ix::WebSocketMessageType messageType,
               const std::string& str,
               size_t wireSize,
               const ix::WebSocketErrorInfo& error,
               const ix::WebSocketOpenInfo& openInfo,
               const ix::WebSocketCloseInfo& closeInfo)
            {
                std::stringstream ss;
                if (messageType == ix::WebSocket_MessageType_Open)
                {
                    _condition.notify_one();

                    log("ws_send: connected");
                    std::cout << "Uri: " << openInfo.uri << std::endl;
                    std::cout << "Handshake Headers:" << std::endl;
                    for (auto it : openInfo.headers)
                    {
                        std::cout << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    ss << "ws_send: connection closed:";
                    ss << " code " << closeInfo.code;
                    ss << " reason " << closeInfo.reason << std::endl;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    _condition.notify_one();

                    ss << "ws_send: received message: "
                       << str;
                    log(ss.str());

                    Json::Value data;
                    Json::Reader reader;
                    if (!reader.parse(str, data))
                    {
                        std::cerr << "Invalid JSON response" << std::endl;
                        return;
                    }

                    std::string id = data["id"].asString();
                    if (_id != id)
                    {
                        std::cerr << "Invalid id" << std::endl;
                    }
                }
                else if (messageType == ix::WebSocket_MessageType_Error)
                {
                    ss << "Connection error: " << error.reason      << std::endl;
                    ss << "#retries: "         << error.retries     << std::endl;
                    ss << "Wait time(ms): "    << error.wait_time   << std::endl;
                    ss << "HTTP Status: "      << error.http_status << std::endl;
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

    void WebSocketSender::sendMessage(const std::string& filename)
    {
        std::string content = load(filename);
        _id = uuid4();

        Json::Value pdu;
        pdu["kind"] = "send";
        pdu["id"] = _id;
        pdu["content"] = base64_encode(content, content.size());
        pdu["djb2_hash"] = djb2Hash(content);
        pdu["filename"] = filename;
        _webSocket.send(pdu.toStyledString());
    }

    void wsSend(const std::string& url,
                const std::string& path)
    {
        WebSocketSender webSocketSender(url);
        webSocketSender.start();

        webSocketSender.waitForConnection();

        std::cout << "Sending..." << std::endl;
        webSocketSender.sendMessage(path);

        webSocketSender.waitForAck();

        std::cout << "Done !" << std::endl;
        webSocketSender.stop();
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: ws_send <url> <path>" << std::endl;
        return 1;
    }
    std::string url = argv[1];
    std::string path = argv[2];

    Socket::init();
    wsSend(url, path);
    return 0;
}
