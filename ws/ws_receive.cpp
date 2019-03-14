/*
 *  ws_receiver.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>
#include <ixcrypto/IXUuid.h>
#include <ixcrypto/IXBase64.h>
#include <ixcrypto/IXHash.h>
#include <msgpack11/msgpack11.hpp>

using msgpack11::MsgPack;

namespace ix
{
    class WebSocketReceiver
    {
        public:
            WebSocketReceiver(const std::string& _url,
                              bool enablePerMessageDeflate);

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

            std::mutex _conditionVariableMutex;
            std::condition_variable _condition;

            std::string extractFilename(const std::string& path);
            void handleError(const std::string& errMsg, const std::string& id);
            void log(const std::string& msg);
    };

    WebSocketReceiver::WebSocketReceiver(const std::string& url,
                                         bool enablePerMessageDeflate) :
        _url(url),
        _enablePerMessageDeflate(enablePerMessageDeflate)
    {
        ;
    }

    void WebSocketReceiver::stop()
    {
        _webSocket.stop();
    }

    void WebSocketReceiver::log(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    void WebSocketReceiver::waitForConnection()
    {
        std::cout << "Connecting..." << std::endl;

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    void WebSocketReceiver::waitForMessage()
    {
        std::cout << "Waiting for message..." << std::endl;

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
            std::string filename = path.substr(idx+1);
            return filename;
        }
        else
        {
            return path;
        }
    }

    void WebSocketReceiver::handleError(const std::string& errMsg,
                                        const std::string& id)
    {
        std::map<MsgPack, MsgPack> pdu;
        pdu["kind"] = "error";
        pdu["id"] = id;
        pdu["message"] = errMsg;

        MsgPack msg(pdu);
        _webSocket.send(msg.dump());
    }

    void WebSocketReceiver::handleMessage(const std::string& str)
    {
        std::cerr << "Received message: " << str.size() << std::endl;

        std::string errMsg;
        MsgPack data = MsgPack::parse(str, errMsg);
        if (!errMsg.empty())
        {
            handleError("Invalid MsgPack", std::string());
            return;
        }

        std::cout << "id: " << data["id"].string_value() << std::endl;

        std::vector<uint8_t> content = data["content"].binary_items();
        std::cout << "Content size: " << content.size() << std::endl;

        // Validate checksum
        uint64_t cksum = ix::djb2Hash(content);
        auto cksumRef = data["djb2_hash"].string_value();

        std::cout << "Computed hash: " << cksum << std::endl;
        std::cout << "Reference hash: " << cksumRef << std::endl;

        if (std::to_string(cksum) != cksumRef)
        {
            handleError("Hash mismatch.", std::string());
            return;
        }

        std::string filename = data["filename"].string_value();
        filename = extractFilename(filename);

        std::string filenameTmp = filename + ".tmp";

        std::cout << "Writing to disk: " << filenameTmp << std::endl;
        std::ofstream out(filenameTmp);
        out.write((char*)&content.front(), content.size());
        out.close();

        std::cout << "Renaming " << filenameTmp << " to " << filename << std::endl;
        rename(filenameTmp.c_str(), filename.c_str());

        std::map<MsgPack, MsgPack> pdu;
        pdu["ack"] = true;
        pdu["id"] = data["id"];
        pdu["filename"] = data["filename"];

        MsgPack msg(pdu);
        _webSocket.send(msg.dump());
    }

    void WebSocketReceiver::start()
    {
        _webSocket.setUrl(_url);

        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            _enablePerMessageDeflate, false, false, 15, 15);
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

                    log("ws_receive: connected");
                    std::cout << "Uri: " << openInfo.uri << std::endl;
                    std::cout << "Handshake Headers:" << std::endl;
                    for (auto it : openInfo.headers)
                    {
                        std::cout << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (messageType == ix::WebSocket_MessageType_Close)
                {
                    ss << "ws_receive: connection closed:";
                    ss << " code " << closeInfo.code;
                    ss << " reason " << closeInfo.reason << std::endl;
                    log(ss.str());
                }
                else if (messageType == ix::WebSocket_MessageType_Message)
                {
                    ss << "ws_receive: transfered " << wireSize << " bytes";
                    log(ss.str());
                    handleMessage(str);
                    _condition.notify_one();
                }
                else if (messageType == ix::WebSocket_MessageType_Fragment)
                {
                    ss << "ws_receive: received fragment";
                    log(ss.str());
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

    void wsReceive(const std::string& url,
                   bool enablePerMessageDeflate)
    {
        WebSocketReceiver webSocketReceiver(url, enablePerMessageDeflate);
        webSocketReceiver.start();

        webSocketReceiver.waitForConnection();

        webSocketReceiver.waitForMessage();

        std::chrono::duration<double, std::milli> duration(1000);
        std::this_thread::sleep_for(duration);

        std::cout << "Done !" << std::endl;
        webSocketReceiver.stop();
    }

    int ws_receive_main(const std::string& url,
                        bool enablePerMessageDeflate)
    {
        Socket::init();
        wsReceive(url, enablePerMessageDeflate);
        return 0;
    }
}
