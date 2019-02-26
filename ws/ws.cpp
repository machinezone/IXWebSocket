/*
 *  ws.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

//
// Main drive for websocket utilities
//

#include <string>
#include <sstream>
#include <iostream>

#include <cli11/CLI11.hpp>
#include <ixwebsocket/IXSocket.h>

namespace ix
{
    int ws_http_client_main(const std::string& url,
                            const std::string& headers,
                            const std::string& data,
                            bool headersOnly,
                            bool followRedirects);

    int ws_ping_pong_main(const std::string& url);

    int ws_echo_server_main(int port);

    int ws_broadcast_server_main(int port);

    int ws_chat_main(const std::string& url,
                     const std::string& user);

    int ws_connect_main(const std::string& url);

    int ws_receive_main(const std::string& url,
                        bool enablePerMessageDeflate);

    int ws_transfer_main(int port);

    int ws_send_main(const std::string& url,
                     const std::string& path);
}

int main(int argc, char** argv)
{
    CLI::App app{"ws is a websocket tool"};
    app.require_subcommand();

    std::string url("ws://127.0.0.1:8080");
    std::string path;
    std::string user;
    std::string data;
    std::string headers;
    bool headersOnly = false;
    bool followRedirects = false;
    int port = 8080;

    CLI::App* sendApp = app.add_subcommand("send", "Send a file");
    sendApp->add_option("url", url, "Connection url")->required();
    sendApp->add_option("path", path, "Path to the file to send")
        ->required()->check(CLI::ExistingPath);

    CLI::App* receiveApp = app.add_subcommand("receive", "Receive a file");
    receiveApp->add_option("url", url, "Connection url")->required();

    CLI::App* transferApp = app.add_subcommand("transfer", "Broadcasting server");
    transferApp->add_option("--port", port, "Connection url");

    CLI::App* connectApp = app.add_subcommand("connect", "Connect to a remote server");
    connectApp->add_option("url", url, "Connection url")->required();

    CLI::App* chatApp = app.add_subcommand("chat", "Group chat");
    chatApp->add_option("url", url, "Connection url")->required();
    chatApp->add_option("user", user, "User name")->required();

    CLI::App* echoServerApp = app.add_subcommand("echo_server", "Echo server");
    echoServerApp->add_option("--port", port, "Connection url");

    CLI::App* broadcastServerApp = app.add_subcommand("broadcast_server", "Broadcasting server");
    broadcastServerApp->add_option("--port", port, "Connection url");

    CLI::App* pingPongApp = app.add_subcommand("ping", "Ping pong");
    pingPongApp->add_option("url", url, "Connection url")->required();

    CLI::App* httpClientApp = app.add_subcommand("http_client", "HTTP Client");
    httpClientApp->add_option("url", url, "Connection url")->required();
    httpClientApp->add_option("-d", data, "Form data")->join();
    httpClientApp->add_option("-F", data, "Form data")->join();
    httpClientApp->add_option("-H", headers, "Header")->join();
    httpClientApp->add_flag("-I", headersOnly, "Header");
    httpClientApp->add_flag("-L", followRedirects, "Header");

    CLI11_PARSE(app, argc, argv);

    ix::Socket::init();

    if (app.got_subcommand("transfer"))
    {
        return ix::ws_transfer_main(port);
    }
    else if (app.got_subcommand("send"))
    {
        return ix::ws_send_main(url, path);
    }
    else if (app.got_subcommand("receive"))
    {
        bool enablePerMessageDeflate = false;
        return ix::ws_receive_main(url, enablePerMessageDeflate);
    }
    else if (app.got_subcommand("connect"))
    {
        return ix::ws_connect_main(url);
    }
    else if (app.got_subcommand("chat"))
    {
        return ix::ws_chat_main(url, user);
    }
    else if (app.got_subcommand("echo_server"))
    {
        return ix::ws_echo_server_main(port);
    }
    else if (app.got_subcommand("broadcast_server"))
    {
        return ix::ws_broadcast_server_main(port);
    }
    else if (app.got_subcommand("ping"))
    {
        return ix::ws_ping_pong_main(url);
    }
    else if (app.got_subcommand("http_client"))
    {
        std::cout << "data: " << data << std::endl;
        return ix::ws_http_client_main(url, headers, data, headersOnly, followRedirects);
    }

    return 1;
}
