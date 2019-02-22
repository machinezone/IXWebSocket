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
    class WebSocketSender
    {
        public:
            WebSocketSender(const std::string& _url,
                            bool enablePerMessageDeflate);

            void subscribe(const std::string& channel);
            void start();
            void stop();

            void waitForConnection();
            void waitForAck();

            void sendMessage(const std::string& filename, bool throttle);

        private:
            std::string _url;
            std::string _id;
            ix::WebSocket _webSocket;
            bool _enablePerMessageDeflate;

            std::mutex _conditionVariableMutex;
            std::condition_variable _condition;

            void log(const std::string& msg);
    };

    WebSocketSender::WebSocketSender(const std::string& url,
                                     bool enablePerMessageDeflate) :
        _url(url),
        _enablePerMessageDeflate(enablePerMessageDeflate)
    {
        ;
    }

    void WebSocketSender::stop()
    {
        _webSocket.stop();
    }

    void WebSocketSender::log(const std::string& msg)
    {
        std::cout << msg << std::endl;
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

    std::string load(const std::string& path)
    {
        // std::vector<uint8_t> memblock;
        std::string str;

        std::ifstream file(path);
        if (!file.is_open()) return std::string();

        file.seekg(0, file.end);
        std::streamoff size = file.tellg();
        file.seekg(0, file.beg);

        str.resize(size);
        file.read((char*)&str.front(), static_cast<std::streamsize>(size));

        return str;
    }

    void WebSocketSender::start()
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

                    ss << "ws_send: received message of size " << wireSize;
                    log(ss.str());

                    std::string errMsg;
                    MsgPack data = MsgPack::parse(str, errMsg);
                    if (!errMsg.empty())
                    {
                        std::cerr << "Invalid MsgPack response" << std::endl;
                        return;
                    }

                    std::string id = data["id"].string_value();
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

    class Bench
    {
        public:
            Bench(const std::string& description) :
                _description(description),
                _start(std::chrono::system_clock::now()),
                _reported(false)
            {
                ;
            }

            ~Bench()
            {
                if (!_reported)
                {
                    report();
                }
            }

            void report()
            {
                auto now = std::chrono::system_clock::now();
                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start);

                _ms = milliseconds.count();
                std::cout << _description << " completed in "
                          << _ms << "ms" << std::endl;

                _reported = true;
            }

            uint64_t getDuration() const
            {
                return _ms;
            }

        private:
            std::string _description;
            std::chrono::time_point<std::chrono::system_clock> _start;
            uint64_t _ms;
            bool _reported;
    };

    void WebSocketSender::sendMessage(const std::string& filename,
                                      bool throttle)
    {
        std::string content;
        {
            Bench bench("load file from disk");
            content = load(filename);
        }

        _id = uuid4();

        std::string b64Content;
        {
            Bench bench("base 64 encode file");
            b64Content = base64_encode(content, content.size());
        }

        std::map<MsgPack, MsgPack> pdu;
        pdu["kind"] = "send";
        pdu["id"] = _id;
        pdu["content"] = b64Content;
        auto hash = djb2Hash(b64Content);
        pdu["djb2_hash"] = std::to_string(hash);
        pdu["filename"] = filename;

        MsgPack msg(pdu);

        Bench bench("Sending file through websocket");
        _webSocket.send(msg.dump(),
                        [throttle](int current, int total) -> bool
        {
            std::cout << "Step " << current << " out of " << total << std::endl;

            if (throttle)
            {
                std::chrono::duration<double, std::milli> duration(10);
                std::this_thread::sleep_for(duration);
            }

            return true;
        });

        bench.report();
        auto duration = bench.getDuration();
        auto transferRate = 1000 * b64Content.size() / duration;
        transferRate /= (1024 * 1024);
        std::cout << "Send transfer rate: " << transferRate << "MB/s" << std::endl;
    }

    void wsSend(const std::string& url,
                const std::string& path,
                bool enablePerMessageDeflate,
                bool throttle)
    {
        WebSocketSender webSocketSender(url, enablePerMessageDeflate);
        webSocketSender.start();

        webSocketSender.waitForConnection();

        std::cout << "Sending..." << std::endl;
        webSocketSender.sendMessage(path, throttle);

        webSocketSender.waitForAck();

        std::cout << "Done !" << std::endl;
        webSocketSender.stop();
    }

    int ws_send_main(const std::string& url,
                     const std::string& path)
    {
        bool throttle = false;
        bool enablePerMessageDeflate = false;

        Socket::init();
        wsSend(url, path, enablePerMessageDeflate, throttle);
        return 0;
    }
}
