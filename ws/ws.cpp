/*
 *  ws.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

//
// Main driver for websocket utilities
//
#include "ws.h"

//
// Main drive for websocket utilities
//

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>

#include <cli11/CLI11.hpp>
#include <ixwebsocket/IXSocket.h>

int main(int argc, char** argv)
{
    CLI::App app{"ws is a websocket tool"};
    app.require_subcommand();

    std::string url("ws://127.0.0.1:8080");
    std::string path;
    std::string user;
    std::string data;
    std::string headers;
    std::string output;
    std::string hostname("127.0.0.1");
    std::string pidfile;
    std::string channel;
    std::string message;
    bool headersOnly = false;
    bool followRedirects = false;
    bool verbose = false;
    bool save = false;
    bool compress = false;
    int port = 8080;
    int redisPort = 6379;
    int connectTimeOut = 60;
    int transferTimeout = 1800;
    int maxRedirects = 5;
    int delayMs = -1;

    CLI::App* sendApp = app.add_subcommand("send", "Send a file");
    sendApp->add_option("url", url, "Connection url")->required();
    sendApp->add_option("path", path, "Path to the file to send")
        ->required()->check(CLI::ExistingPath);
    sendApp->add_option("--pidfile", pidfile, "Pid file");

    CLI::App* receiveApp = app.add_subcommand("receive", "Receive a file");
    receiveApp->add_option("url", url, "Connection url")->required();
    receiveApp->add_option("--delay", delayMs, "Delay (ms) to wait after receiving a fragment"
                                               " to artificially slow down the receiver");
    receiveApp->add_option("--pidfile", pidfile, "Pid file");

    CLI::App* transferApp = app.add_subcommand("transfer", "Broadcasting server");
    transferApp->add_option("--port", port, "Connection url");
    transferApp->add_option("--host", hostname, "Hostname");
    transferApp->add_option("--pidfile", pidfile, "Pid file");

    CLI::App* connectApp = app.add_subcommand("connect", "Connect to a remote server");
    connectApp->add_option("url", url, "Connection url")->required();

    CLI::App* chatApp = app.add_subcommand("chat", "Group chat");
    chatApp->add_option("url", url, "Connection url")->required();
    chatApp->add_option("user", user, "User name")->required();

    CLI::App* echoServerApp = app.add_subcommand("echo_server", "Echo server");
    echoServerApp->add_option("--port", port, "Port");
    echoServerApp->add_option("--host", hostname, "Hostname");

    CLI::App* broadcastServerApp = app.add_subcommand("broadcast_server", "Broadcasting server");
    broadcastServerApp->add_option("--port", port, "Port");
    broadcastServerApp->add_option("--host", hostname, "Hostname");

    CLI::App* pingPongApp = app.add_subcommand("ping", "Ping pong");
    pingPongApp->add_option("url", url, "Connection url")->required();

    CLI::App* httpClientApp = app.add_subcommand("curl", "HTTP Client");
    httpClientApp->add_option("url", url, "Connection url")->required();
    httpClientApp->add_option("-d", data, "Form data")->join();
    httpClientApp->add_option("-F", data, "Form data")->join();
    httpClientApp->add_option("-H", headers, "Header")->join();
    httpClientApp->add_option("--output", output, "Output file");
    httpClientApp->add_flag("-I", headersOnly, "Send a HEAD request");
    httpClientApp->add_flag("-L", followRedirects, "Follow redirects");
    httpClientApp->add_option("--max-redirects", maxRedirects, "Max Redirects");
    httpClientApp->add_flag("-v", verbose, "Verbose");
    httpClientApp->add_flag("-O", save, "Save output to disk");
    httpClientApp->add_flag("--compress", compress, "Enable gzip compression");
    httpClientApp->add_option("--connect-timeout", connectTimeOut, "Connection timeout");
    httpClientApp->add_option("--transfer-timeout", transferTimeout, "Transfer timeout");

    CLI::App* redisPublishApp = app.add_subcommand("redis_publish", "Redis publisher");
    redisPublishApp->add_option("--port", redisPort, "Port");
    redisPublishApp->add_option("--host", hostname, "Hostname");
    redisPublishApp->add_option("channel", channel, "Channel")->required();
    redisPublishApp->add_option("message", message, "Message")->required();

    CLI::App* redisSubscribeApp = app.add_subcommand("redis_subscribe", "Redis subscriber");
    redisSubscribeApp->add_option("--port", redisPort, "Port");
    redisSubscribeApp->add_option("--host", hostname, "Hostname");
    redisSubscribeApp->add_option("channel", channel, "Channel")->required();
    redisSubscribeApp->add_flag("-v", verbose, "Verbose");

    CLI11_PARSE(app, argc, argv);

    // pid file handling
    if (!pidfile.empty())
    {
        unlink(pidfile.c_str());

        std::ofstream f;
        f.open(pidfile);
        f << getpid();
        f.close();
    }

    if (app.got_subcommand("transfer"))
    {
        return ix::ws_transfer_main(port, hostname);
    }
    else if (app.got_subcommand("send"))
    {
        return ix::ws_send_main(url, path);
    }
    else if (app.got_subcommand("receive"))
    {
        bool enablePerMessageDeflate = false;
        return ix::ws_receive_main(url, enablePerMessageDeflate, delayMs);
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
        return ix::ws_echo_server_main(port, hostname);
    }
    else if (app.got_subcommand("broadcast_server"))
    {
        return ix::ws_broadcast_server_main(port, hostname);
    }
    else if (app.got_subcommand("ping"))
    {
        return ix::ws_ping_pong_main(url);
    }
    else if (app.got_subcommand("curl"))
    {
        return ix::ws_http_client_main(url, headers, data, headersOnly,
                                       connectTimeOut, transferTimeout,
                                       followRedirects, maxRedirects, verbose,
                                       save, output, compress);
    }
    else if (app.got_subcommand("redis_publish"))
    {
        return ix::ws_redis_publish_main(hostname, redisPort, channel, message);
    }
    else if (app.got_subcommand("redis_subscribe"))
    {
        return ix::ws_redis_subscribe_main(hostname, redisPort, channel, verbose);
    }

    return 1;
}
