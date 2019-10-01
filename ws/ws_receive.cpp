/*
 *  ws_receiver.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <ixcrypto/IXBase64.h>
#include <ixcrypto/IXHash.h>
#include <ixcrypto/IXUuid.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocket.h>
#include <msgpack11/msgpack11.hpp>
#include <mutex>
#include <sstream>
#include <vector>

using msgpack11::MsgPack;

namespace ix
{
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
        std::cout << msg << std::endl;
    }

    void WebSocketReceiver::waitForConnection()
    {
        std::cout << "ws_receive: Connecting..." << std::endl;

        std::unique_lock<std::mutex> lock(_conditionVariableMutex);
        _condition.wait(lock);
    }

    void WebSocketReceiver::waitForMessage()
    {
        std::cout << "ws_receive: Waiting for message..." << std::endl;

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
        std::cerr << "ws_receive: Received message: " << str.size() << std::endl;

        std::string errMsg;
        MsgPack data = MsgPack::parse(str, errMsg);
        if (!errMsg.empty())
        {
            handleError("ws_receive: Invalid MsgPack", std::string());
            return;
        }

        std::cout << "id: " << data["id"].string_value() << std::endl;

        std::vector<uint8_t> content = data["content"].binary_items();
        std::cout << "ws_receive: Content size: " << content.size() << std::endl;

        // Validate checksum
        uint64_t cksum = ix::djb2Hash(content);
        auto cksumRef = data["djb2_hash"].string_value();

        std::cout << "ws_receive: Computed hash: " << cksum << std::endl;
        std::cout << "ws_receive: Reference hash: " << cksumRef << std::endl;

        if (std::to_string(cksum) != cksumRef)
        {
            handleError("Hash mismatch.", std::string());
            return;
        }

        std::string filename = data["filename"].string_value();
        filename = extractFilename(filename);

        std::string filenameTmp = filename + ".tmp";

        std::cout << "ws_receive: Writing to disk: " << filenameTmp << std::endl;
        std::ofstream out(filenameTmp);
        out.write((char*) &content.front(), content.size());
        out.close();

        std::cout << "ws_receive: Renaming " << filenameTmp << " to " << filename << std::endl;
        rename(filenameTmp.c_str(), filename.c_str());

        std::map<MsgPack, MsgPack> pdu;
        pdu["ack"] = true;
        pdu["id"] = data["id"];
        pdu["filename"] = data["filename"];

        std::cout << "Sending ack to sender" << std::endl;
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
                std::cout << "Uri: " << msg->openInfo.uri << std::endl;
                std::cout << "Handshake Headers:" << std::endl;
                for (auto it : msg->openInfo.headers)
                {
                    std::cout << it.first << ": " << it.second << std::endl;
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

        std::cout << "ws_receive: Done !" << std::endl;
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
} // namespace ix
