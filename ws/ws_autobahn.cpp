/*
 *  ws_autobahn.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <atomic>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>


namespace ix
{
    class AutobahnTestCase
    {
        public:
            AutobahnTestCase(const std::string& _url, bool quiet);
            void run();

        private:
            void log(const std::string& msg);

            std::string _url;
            ix::WebSocket _webSocket;

            std::atomic<bool> _done;
            bool _quiet;
    };

    AutobahnTestCase::AutobahnTestCase(const std::string& url, bool quiet) :
        _url(url),
        _done(false),
        _quiet(quiet)
    {
        _webSocket.disableAutomaticReconnection();

        // FIXME: this should be on by default
        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
            true, false, false, 15, 15);
        _webSocket.setPerMessageDeflateOptions(webSocketPerMessageDeflateOptions);
    }

    void AutobahnTestCase::log(const std::string& msg)
    {
        if (!_quiet)
        {
            std::cerr << msg;
        }
    }

    void AutobahnTestCase::run()
    {
        _webSocket.setUrl(_url);

        std::stringstream ss;
        log(std::string("Connecting to url: ") + _url);

        _webSocket.setOnMessageCallback(
            [this](const ix::WebSocketMessagePtr& msg)
            {
                std::stringstream ss;
                if (msg->type == ix::WebSocketMessageType::Open)
                {
                    log("autobahn: connected");
                    ss << "Uri: " << msg->openInfo.uri << std::endl;
                    ss << "Handshake Headers:" << std::endl;
                    for (auto it : msg->openInfo.headers)
                    {
                        ss << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (msg->type == ix::WebSocketMessageType::Close)
                {
                    ss << "autobahn: connection closed:";
                    ss << " code " << msg->closeInfo.code;
                    ss << " reason " << msg->closeInfo.reason << std::endl;

                    _done = true;
                }
                else if (msg->type == ix::WebSocketMessageType::Message)
                {
                    ss << "Received " << msg->wireSize << " bytes" << std::endl;

                    ss << "autobahn: received message: "
                       << msg->str
                       << std::endl;

                    _webSocket.send(msg->str, msg->binary);
                }
                else if (msg->type == ix::WebSocketMessageType::Error)
                {
                    ss << "Connection error: " << msg->errorInfo.reason      << std::endl;
                    ss << "#retries: "         << msg->errorInfo.retries     << std::endl;
                    ss << "Wait time(ms): "    << msg->errorInfo.wait_time   << std::endl;
                    ss << "HTTP Status: "      << msg->errorInfo.http_status << std::endl;

                    // And error can happen, in which case the test-case is marked done
                    _done = true;
                }
                else if (msg->type == ix::WebSocketMessageType::Fragment)
                {
                    ss << "Received message fragment" << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Ping)
                {
                    ss << "Received ping" << std::endl;
                }
                else if (msg->type == ix::WebSocketMessageType::Pong)
                {
                    ss << "Received pong" << std::endl;
                }
                else
                {
                    ss << "Invalid ix::WebSocketMessageType" << std::endl;
                }

                log(ss.str());
            });

        _webSocket.start();

        log("Waiting for being closed ...");
        while (!_done)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
        }

        _webSocket.stop();
    }

    //
    // make && bench ws autobahn --url 'ws://localhost:9001/runCase?case=9&agent=ixwebsocket' && ws connect -d 'ws://localhost:9001/updateReports?agent=ixwebsocket'
    //
    int ws_autobahn_main(const std::string& url, bool quiet)
    {
        int N = 1; // 519;
        N++;
        for (int i = 1 ; i < N; ++i)
        {
            int caseNumber = i;

            std::stringstream ss;
            ss << "ws://localhost:9001/runCase?case="
               << caseNumber
               << "&agent=ixwebsocket";

            std::string url(ss.str());

            AutobahnTestCase testCase(url, quiet);
            testCase.run();
        }

        return 0;
    }
}

