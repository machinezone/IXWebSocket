/*
 *  ws.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

//
// Main driver for websocket utilities
//
#include "ws.h"

#include <cli11/CLI11.hpp>
#include <fstream>
#include <iostream>
#include <ixcore/utils/IXCoreLogger.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <signal.h>
#endif


int main(int argc, char** argv)
{
    ix::initNetSystem();

    ix::IXCoreLogger::LogFunc logFunc = [](const char* msg) { spdlog::info(msg); };
    ix::IXCoreLogger::setLogFunction(logFunc);

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    // Display command.
    if (getenv("DEBUG"))
    {
        std::cout << "Command: ";
        for (int i = 0; i < argc; ++i)
        {
            std::cout << argv[i] << " ";
        }
        std::cout << std::endl;
    }

    CLI::App app {"ws is a websocket tool"};

    std::string url("ws://127.0.0.1:8008");
    std::string path;
    std::string user;
    std::string data;
    std::string headers;
    std::string output;
    std::string hostname("127.0.0.1");
    std::string pidfile;
    std::string channel;
    std::string filter;
    std::string message;
    std::string password;
    std::string appkey;
    std::string endpoint;
    std::string rolename;
    std::string rolesecret;
    std::string prefix("ws.test.v0");
    std::string fields;
    std::string dsn;
    std::string redisHosts("127.0.0.1");
    std::string redisPassword;
    std::string appsConfigPath("appsConfig.json");
    std::string subprotocol;
    std::string remoteHost;
    std::string minidump;
    std::string metadata;
    std::string project;
    std::string key;
    ix::SocketTLSOptions tlsOptions;
    std::string ciphers;
    std::string redirectUrl;
    bool headersOnly = false;
    bool followRedirects = false;
    bool verbose = false;
    bool save = false;
    bool quiet = false;
    bool compress = false;
    bool strict = false;
    bool stress = false;
    bool disableAutomaticReconnection = false;
    bool disablePerMessageDeflate = false;
    bool greetings = false;
    bool binaryMode = false;
    bool redirect = false;
    bool version = false;
    bool verifyNone = false;
    int port = 8008;
    int redisPort = 6379;
    int statsdPort = 8125;
    int connectTimeOut = 60;
    int transferTimeout = 1800;
    int maxRedirects = 5;
    int delayMs = -1;
    int count = 1;
    int jobs = 4;
    uint32_t maxWaitBetweenReconnectionRetries;

    auto addTLSOptions = [&tlsOptions, &verifyNone](CLI::App* app) {
        app->add_option(
               "--cert-file", tlsOptions.certFile, "Path to the (PEM format) TLS cert file")
            ->check(CLI::ExistingPath);
        app->add_option("--key-file", tlsOptions.keyFile, "Path to the (PEM format) TLS key file")
            ->check(CLI::ExistingPath);
        app->add_option("--ca-file", tlsOptions.caFile, "Path to the (PEM format) ca roots file")
            ->check(CLI::ExistingPath);
        app->add_option("--ciphers",
                        tlsOptions.ciphers,
                        "A (comma/space/colon) separated list of ciphers to use for TLS");
        app->add_flag("--tls", tlsOptions.tls, "Enable TLS (server only)");
        app->add_flag("--verify_none", verifyNone, "Disable peer cert verification");
    };

    app.add_flag("--version", version, "Connection url");

    CLI::App* sendApp = app.add_subcommand("send", "Send a file");
    sendApp->add_option("url", url, "Connection url")->required();
    sendApp->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    sendApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(sendApp);

    CLI::App* receiveApp = app.add_subcommand("receive", "Receive a file");
    receiveApp->add_option("url", url, "Connection url")->required();
    receiveApp->add_option("--delay",
                           delayMs,
                           "Delay (ms) to wait after receiving a fragment"
                           " to artificially slow down the receiver");
    receiveApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(receiveApp);

    CLI::App* transferApp = app.add_subcommand("transfer", "Broadcasting server");
    transferApp->add_option("--port", port, "Connection url");
    transferApp->add_option("--host", hostname, "Hostname");
    transferApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(transferApp);

    CLI::App* connectApp = app.add_subcommand("connect", "Connect to a remote server");
    connectApp->add_option("url", url, "Connection url")->required();
    connectApp->add_option("-H", headers, "Header")->join();
    connectApp->add_flag("-d", disableAutomaticReconnection, "Disable Automatic Reconnection");
    connectApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    connectApp->add_flag("-b", binaryMode, "Send in binary mode");
    connectApp->add_option("--max_wait",
                           maxWaitBetweenReconnectionRetries,
                           "Max Wait Time between reconnection retries");
    connectApp->add_option("--subprotocol", subprotocol, "Subprotocol");
    addTLSOptions(connectApp);

    CLI::App* chatApp = app.add_subcommand("chat", "Group chat");
    chatApp->add_option("url", url, "Connection url")->required();
    chatApp->add_option("user", user, "User name")->required();

    CLI::App* echoServerApp = app.add_subcommand("echo_server", "Echo server");
    echoServerApp->add_option("--port", port, "Port");
    echoServerApp->add_option("--host", hostname, "Hostname");
    echoServerApp->add_flag("-g", greetings, "Verbose");
    addTLSOptions(echoServerApp);

    CLI::App* broadcastServerApp = app.add_subcommand("broadcast_server", "Broadcasting server");
    broadcastServerApp->add_option("--port", port, "Port");
    broadcastServerApp->add_option("--host", hostname, "Hostname");
    addTLSOptions(broadcastServerApp);

    CLI::App* pingPongApp = app.add_subcommand("ping", "Ping pong");
    pingPongApp->add_option("url", url, "Connection url")->required();
    addTLSOptions(pingPongApp);

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
    addTLSOptions(httpClientApp);

    CLI::App* redisPublishApp = app.add_subcommand("redis_publish", "Redis publisher");
    redisPublishApp->add_option("--port", redisPort, "Port");
    redisPublishApp->add_option("--host", hostname, "Hostname");
    redisPublishApp->add_option("--password", password, "Password");
    redisPublishApp->add_option("channel", channel, "Channel")->required();
    redisPublishApp->add_option("message", message, "Message")->required();
    redisPublishApp->add_option("-c", count, "Count");

    CLI::App* redisSubscribeApp = app.add_subcommand("redis_subscribe", "Redis subscriber");
    redisSubscribeApp->add_option("--port", redisPort, "Port");
    redisSubscribeApp->add_option("--host", hostname, "Hostname");
    redisSubscribeApp->add_option("--password", password, "Password");
    redisSubscribeApp->add_option("channel", channel, "Channel")->required();
    redisSubscribeApp->add_flag("-v", verbose, "Verbose");
    redisSubscribeApp->add_option("--pidfile", pidfile, "Pid file");

    CLI::App* cobraSubscribeApp = app.add_subcommand("cobra_subscribe", "Cobra subscriber");
    cobraSubscribeApp->add_option("--appkey", appkey, "Appkey")->required();
    cobraSubscribeApp->add_option("--endpoint", endpoint, "Endpoint")->required();
    cobraSubscribeApp->add_option("--rolename", rolename, "Role name")->required();
    cobraSubscribeApp->add_option("--rolesecret", rolesecret, "Role secret")->required();
    cobraSubscribeApp->add_option("channel", channel, "Channel")->required();
    cobraSubscribeApp->add_option("--pidfile", pidfile, "Pid file");
    cobraSubscribeApp->add_option("--filter", filter, "Stream SQL Filter");
    cobraSubscribeApp->add_flag("-q", quiet, "Quiet / only display stats");

    CLI::App* cobraPublish = app.add_subcommand("cobra_publish", "Cobra publisher");
    cobraPublish->add_option("--appkey", appkey, "Appkey")->required();
    cobraPublish->add_option("--endpoint", endpoint, "Endpoint")->required();
    cobraPublish->add_option("--rolename", rolename, "Role name")->required();
    cobraPublish->add_option("--rolesecret", rolesecret, "Role secret")->required();
    cobraPublish->add_option("channel", channel, "Channel")->required();
    cobraPublish->add_option("--pidfile", pidfile, "Pid file");
    cobraPublish->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);

    CLI::App* cobraMetricsPublish =
        app.add_subcommand("cobra_metrics_publish", "Cobra metrics publisher");
    cobraMetricsPublish->add_option("--appkey", appkey, "Appkey");
    cobraMetricsPublish->add_option("--endpoint", endpoint, "Endpoint");
    cobraMetricsPublish->add_option("--rolename", rolename, "Role name");
    cobraMetricsPublish->add_option("--rolesecret", rolesecret, "Role secret");
    cobraMetricsPublish->add_option("channel", channel, "Channel")->required();
    cobraMetricsPublish->add_option("--pidfile", pidfile, "Pid file");
    cobraMetricsPublish->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    cobraMetricsPublish->add_flag("--stress", stress, "Stress mode");

    CLI::App* cobra2statsd = app.add_subcommand("cobra_to_statsd", "Cobra metrics to statsd");
    cobra2statsd->add_option("--appkey", appkey, "Appkey");
    cobra2statsd->add_option("--endpoint", endpoint, "Endpoint");
    cobra2statsd->add_option("--rolename", rolename, "Role name");
    cobra2statsd->add_option("--rolesecret", rolesecret, "Role secret");
    cobra2statsd->add_option("--host", hostname, "Statsd host");
    cobra2statsd->add_option("--port", statsdPort, "Statsd port");
    cobra2statsd->add_option("--prefix", prefix, "Statsd prefix");
    cobra2statsd->add_option("--fields", fields, "Extract fields for naming the event")->join();
    cobra2statsd->add_option("channel", channel, "Channel")->required();
    cobra2statsd->add_flag("-v", verbose, "Verbose");
    cobra2statsd->add_option("--pidfile", pidfile, "Pid file");
    cobra2statsd->add_option("--filter", filter, "Stream SQL Filter");

    CLI::App* cobra2sentry = app.add_subcommand("cobra_to_sentry", "Cobra metrics to sentry");
    cobra2sentry->add_option("--appkey", appkey, "Appkey")->required();
    cobra2sentry->add_option("--endpoint", endpoint, "Endpoint")->required();
    cobra2sentry->add_option("--rolename", rolename, "Role name")->required();
    cobra2sentry->add_option("--rolesecret", rolesecret, "Role secret")->required();
    cobra2sentry->add_option("--dsn", dsn, "Sentry DSN");
    cobra2sentry->add_option("--jobs", jobs, "Number of thread sending events to Sentry");
    cobra2sentry->add_option("channel", channel, "Channel")->required();
    cobra2sentry->add_flag("-v", verbose, "Verbose");
    cobra2sentry->add_flag("-s", strict, "Strict mode. Error out when sending to sentry fails");
    cobra2sentry->add_option("--pidfile", pidfile, "Pid file");
    cobra2sentry->add_option("--filter", filter, "Stream SQL Filter");

    CLI::App* cobra2redisApp =
        app.add_subcommand("cobra_metrics_to_redis", "Cobra metrics to redis");
    cobra2redisApp->add_option("--appkey", appkey, "Appkey")->required();
    cobra2redisApp->add_option("--endpoint", endpoint, "Endpoint")->required();
    cobra2redisApp->add_option("--rolename", rolename, "Role name")->required();
    cobra2redisApp->add_option("--rolesecret", rolesecret, "Role secret")->required();
    cobra2redisApp->add_option("channel", channel, "Channel")->required();
    cobra2redisApp->add_option("--pidfile", pidfile, "Pid file");
    cobra2redisApp->add_option("--filter", filter, "Stream SQL Filter");
    cobra2redisApp->add_option("--hostname", hostname, "Redis hostname");
    cobra2redisApp->add_option("--port", redisPort, "Redis port");
    cobra2redisApp->add_flag("-q", quiet, "Quiet / only display stats");

    CLI::App* runApp = app.add_subcommand("snake", "Snake server");
    runApp->add_option("--port", port, "Connection url");
    runApp->add_option("--host", hostname, "Hostname");
    runApp->add_option("--pidfile", pidfile, "Pid file");
    runApp->add_option("--redis_hosts", redisHosts, "Redis hosts");
    runApp->add_option("--redis_port", redisPort, "Redis hosts");
    runApp->add_option("--redis_password", redisPassword, "Redis password");
    runApp->add_option("--apps_config_path", appsConfigPath, "Path to auth data")
        ->check(CLI::ExistingPath);
    runApp->add_flag("-v", verbose, "Verbose");

    CLI::App* httpServerApp = app.add_subcommand("httpd", "HTTP server");
    httpServerApp->add_option("--port", port, "Port");
    httpServerApp->add_option("--host", hostname, "Hostname");
    httpServerApp->add_flag("-L", redirect, "Redirect all request to redirect_url");
    httpServerApp->add_option("--redirect_url", redirectUrl, "Url to redirect to");
    addTLSOptions(httpServerApp);

    CLI::App* autobahnApp = app.add_subcommand("autobahn", "Test client Autobahn compliance");
    autobahnApp->add_option("--url", url, "url");
    autobahnApp->add_flag("-q", quiet, "Quiet");

    CLI::App* redisServerApp = app.add_subcommand("redis_server", "Redis server");
    redisServerApp->add_option("--port", port, "Port");
    redisServerApp->add_option("--host", hostname, "Hostname");

    CLI::App* proxyServerApp = app.add_subcommand("proxy_server", "Proxy server");
    proxyServerApp->add_option("--port", port, "Port");
    proxyServerApp->add_option("--host", hostname, "Hostname");
    proxyServerApp->add_option("--remote_host", remoteHost, "Remote Hostname");
    proxyServerApp->add_flag("-v", verbose, "Verbose");

    CLI::App* minidumpApp = app.add_subcommand("upload_minidump", "Upload a minidump to sentry");
    minidumpApp->add_option("--minidump", minidump, "Minidump path")->check(CLI::ExistingPath);
    minidumpApp->add_option("--metadata", metadata, "Hostname")->check(CLI::ExistingPath);
    minidumpApp->add_option("--project", project, "Sentry Project")->required();
    minidumpApp->add_option("--key", key, "Sentry Key")->required();
    minidumpApp->add_flag("-v", verbose, "Verbose");

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

    if (verifyNone)
    {
        tlsOptions.caFile = "NONE";
    }

    int ret = 1;
    if (app.got_subcommand("transfer"))
    {
        ret = ix::ws_transfer_main(port, hostname, tlsOptions);
    }
    else if (app.got_subcommand("send"))
    {
        ret = ix::ws_send_main(url, path, tlsOptions);
    }
    else if (app.got_subcommand("receive"))
    {
        bool enablePerMessageDeflate = false;
        ret = ix::ws_receive_main(url, enablePerMessageDeflate, delayMs, tlsOptions);
    }
    else if (app.got_subcommand("connect"))
    {
        ret = ix::ws_connect_main(url,
                                  headers,
                                  disableAutomaticReconnection,
                                  disablePerMessageDeflate,
                                  binaryMode,
                                  maxWaitBetweenReconnectionRetries,
                                  tlsOptions,
                                  subprotocol);
    }
    else if (app.got_subcommand("chat"))
    {
        ret = ix::ws_chat_main(url, user);
    }
    else if (app.got_subcommand("echo_server"))
    {
        ret = ix::ws_echo_server_main(port, greetings, hostname, tlsOptions);
    }
    else if (app.got_subcommand("broadcast_server"))
    {
        ret = ix::ws_broadcast_server_main(port, hostname, tlsOptions);
    }
    else if (app.got_subcommand("ping"))
    {
        ret = ix::ws_ping_pong_main(url, tlsOptions);
    }
    else if (app.got_subcommand("curl"))
    {
        ret = ix::ws_http_client_main(url,
                                      headers,
                                      data,
                                      headersOnly,
                                      connectTimeOut,
                                      transferTimeout,
                                      followRedirects,
                                      maxRedirects,
                                      verbose,
                                      save,
                                      output,
                                      compress,
                                      tlsOptions);
    }
    else if (app.got_subcommand("redis_publish"))
    {
        ret = ix::ws_redis_publish_main(hostname, redisPort, password, channel, message, count);
    }
    else if (app.got_subcommand("redis_subscribe"))
    {
        ret = ix::ws_redis_subscribe_main(hostname, redisPort, password, channel, verbose);
    }
    else if (app.got_subcommand("cobra_subscribe"))
    {
        ret = ix::ws_cobra_subscribe_main(
            appkey, endpoint, rolename, rolesecret, channel, filter, quiet);
    }
    else if (app.got_subcommand("cobra_publish"))
    {
        ret = ix::ws_cobra_publish_main(appkey, endpoint, rolename, rolesecret, channel, path);
    }
    else if (app.got_subcommand("cobra_metrics_publish"))
    {
        ret = ix::ws_cobra_metrics_publish_main(
            appkey, endpoint, rolename, rolesecret, channel, path, stress);
    }
    else if (app.got_subcommand("cobra_to_statsd"))
    {
        ret = ix::ws_cobra_to_statsd_main(appkey,
                                          endpoint,
                                          rolename,
                                          rolesecret,
                                          channel,
                                          filter,
                                          hostname,
                                          statsdPort,
                                          prefix,
                                          fields,
                                          verbose);
    }
    else if (app.got_subcommand("cobra_to_sentry"))
    {
        ret = ix::ws_cobra_to_sentry_main(
            appkey, endpoint, rolename, rolesecret, channel, filter, dsn, verbose, strict, jobs);
    }
    else if (app.got_subcommand("cobra_metrics_to_redis"))
    {
        ret = ix::ws_cobra_metrics_to_redis(
            appkey, endpoint, rolename, rolesecret, channel, filter, hostname, redisPort);
    }
    else if (app.got_subcommand("snake"))
    {
        ret = ix::ws_snake_main(
            port, hostname, redisHosts, redisPort, redisPassword, verbose, appsConfigPath);
    }
    else if (app.got_subcommand("httpd"))
    {
        ret = ix::ws_httpd_main(port, hostname, redirect, redirectUrl, tlsOptions);
    }
    else if (app.got_subcommand("autobahn"))
    {
        ret = ix::ws_autobahn_main(url, quiet);
    }
    else if (app.got_subcommand("redis_server"))
    {
        ret = ix::ws_redis_server_main(port, hostname);
    }
    else if (app.got_subcommand("proxy_server"))
    {
        ret = ix::ws_proxy_server_main(port, hostname, tlsOptions, remoteHost, verbose);
    }
    else if (app.got_subcommand("upload_minidump"))
    {
        ret = ix::ws_sentry_minidump_upload(metadata, minidump, project, key, verbose);
    }
    else if (version)
    {
        std::cout << "ws " << ix::userAgent() << std::endl;
    }
    else
    {
        std::cerr << "A subcommand or --version is required" << std::endl;
    }

    ix::uninitNetSystem();
    return ret;
}
