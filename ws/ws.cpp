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
#include <ixbots/IXCobraToSentryBot.h>
#include <ixbots/IXCobraToStatsdBot.h>
#include <ixbots/IXCobraToStdoutBot.h>
#include <ixbots/IXCobraMetricsToStatsdBot.h>
#include <ixbots/IXCobraMetricsToRedisBot.h>
#include <ixredis/IXRedisClient.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <ixsentry/IXSentryClient.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <signal.h>
#else
#include <process.h>
#define getpid _getpid
#endif


int main(int argc, char** argv)
{
    ix::initNetSystem();

    ix::CoreLogger::LogFunc logFunc = [](const char* msg, ix::LogLevel level) {
        switch (level)
        {
            case ix::LogLevel::Debug:
            {
                spdlog::debug(msg);
            }
            break;

            case ix::LogLevel::Info:
            {
                spdlog::info(msg);
            }
            break;

            case ix::LogLevel::Warning:
            {
                spdlog::warn(msg);
            }
            break;

            case ix::LogLevel::Error:
            {
                spdlog::error(msg);
            }
            break;

            case ix::LogLevel::Critical:
            {
                spdlog::critical(msg);
            }
            break;
        }
    };
    ix::CoreLogger::setLogFunction(logFunc);

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    // Display command.
    if (getenv("DEBUG"))
    {
        std::stringstream ss;
        ss << "Command: ";
        for (int i = 0; i < argc; ++i)
        {
            ss << argv[i] << " ";
        }
        spdlog::info(ss.str());
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
    std::string position;
    std::string message;
    std::string password;
    std::string prefix("ws.test.v0");
    std::string fields;
    std::string gauge;
    std::string timer;
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
    std::string logfile;
    ix::SocketTLSOptions tlsOptions;
    ix::CobraConfig cobraConfig;
    ix::CobraBotConfig cobraBotConfig;
    std::string ciphers;
    std::string redirectUrl;
    bool headersOnly = false;
    bool followRedirects = false;
    bool verbose = false;
    bool save = false;
    bool quiet = false;
    bool fluentd = false;
    bool compress = false;
    bool stress = false;
    bool disableAutomaticReconnection = false;
    bool disablePerMessageDeflate = false;
    bool greetings = false;
    bool ipv6 = false;
    bool binaryMode = false;
    bool redirect = false;
    bool version = false;
    bool verifyNone = false;
    bool disablePong = false;
    int port = 8008;
    int redisPort = 6379;
    int statsdPort = 8125;
    int connectTimeOut = 60;
    int transferTimeout = 1800;
    int maxRedirects = 5;
    int delayMs = -1;
    int count = 1;
    uint32_t maxWaitBetweenReconnectionRetries;
    int pingIntervalSecs = 30;

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

    auto addCobraConfig = [&cobraConfig](CLI::App* app) {
        app->add_option("--appkey", cobraConfig.appkey, "Appkey")->required();
        app->add_option("--endpoint", cobraConfig.endpoint, "Endpoint")->required();
        app->add_option("--rolename", cobraConfig.rolename, "Role name")->required();
        app->add_option("--rolesecret", cobraConfig.rolesecret, "Role secret")->required();
    };

    auto addCobraBotConfig = [&cobraBotConfig](CLI::App* app) {
        app->add_option("--appkey", cobraBotConfig.cobraConfig.appkey, "Appkey")->required();
        app->add_option("--endpoint", cobraBotConfig.cobraConfig.endpoint, "Endpoint")->required();
        app->add_option("--rolename", cobraBotConfig.cobraConfig.rolename, "Role name")->required();
        app->add_option("--rolesecret", cobraBotConfig.cobraConfig.rolesecret, "Role secret")
            ->required();
        app->add_option("--channel", cobraBotConfig.channel, "Channel")->required();
        app->add_option("--filter", cobraBotConfig.filter, "Filter");
        app->add_option("--position", cobraBotConfig.position, "Position");
        app->add_option("--runtime", cobraBotConfig.runtime, "Runtime");
        app->add_option("--heartbeat", cobraBotConfig.enableHeartbeat, "Runtime");
        app->add_option("--heartbeat_timeout", cobraBotConfig.heartBeatTimeout, "Runtime");
        app->add_flag(
            "--limit_received_events", cobraBotConfig.limitReceivedEvents, "Max events per minute");
        app->add_option(
            "--max_events_per_minute", cobraBotConfig.maxEventsPerMinute, "Max events per minute");
        app->add_option(
            "--batch_size", cobraBotConfig.batchSize, "Subscription batch size");
    };

    app.add_flag("--version", version, "Print ws version");
    app.add_option("--logfile", logfile, "path where all logs will be redirected");

    CLI::App* sendApp = app.add_subcommand("send", "Send a file");
    sendApp->fallthrough();
    sendApp->add_option("url", url, "Connection url")->required();
    sendApp->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    sendApp->add_option("--pidfile", pidfile, "Pid file");
    sendApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    addTLSOptions(sendApp);

    CLI::App* receiveApp = app.add_subcommand("receive", "Receive a file");
    receiveApp->fallthrough();
    receiveApp->add_option("url", url, "Connection url")->required();
    receiveApp->add_option("--delay",
                           delayMs,
                           "Delay (ms) to wait after receiving a fragment"
                           " to artificially slow down the receiver");
    receiveApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(receiveApp);

    CLI::App* transferApp = app.add_subcommand("transfer", "Broadcasting server");
    transferApp->fallthrough();
    transferApp->add_option("--port", port, "Connection url");
    transferApp->add_option("--host", hostname, "Hostname");
    transferApp->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(transferApp);

    CLI::App* connectApp = app.add_subcommand("connect", "Connect to a remote server");
    connectApp->fallthrough();
    connectApp->add_option("url", url, "Connection url")->required();
    connectApp->add_option("-H", headers, "Header")->join();
    connectApp->add_flag("-d", disableAutomaticReconnection, "Disable Automatic Reconnection");
    connectApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    connectApp->add_flag("-b", binaryMode, "Send in binary mode");
    connectApp->add_option("--max_wait",
                           maxWaitBetweenReconnectionRetries,
                           "Max Wait Time between reconnection retries");
    connectApp->add_option("--ping_interval", pingIntervalSecs, "Interval between sending pings");
    connectApp->add_option("--subprotocol", subprotocol, "Subprotocol");
    addTLSOptions(connectApp);

    CLI::App* chatApp = app.add_subcommand("chat", "Group chat");
    chatApp->fallthrough();
    chatApp->add_option("url", url, "Connection url")->required();
    chatApp->add_option("user", user, "User name")->required();

    CLI::App* echoServerApp = app.add_subcommand("echo_server", "Echo server");
    echoServerApp->fallthrough();
    echoServerApp->add_option("--port", port, "Port");
    echoServerApp->add_option("--host", hostname, "Hostname");
    echoServerApp->add_flag("-g", greetings, "Verbose");
    echoServerApp->add_flag("-6", ipv6, "IpV6");
    echoServerApp->add_flag("-x", disablePerMessageDeflate, "Disable per message deflate");
    echoServerApp->add_flag("-p", disablePong, "Disable sending PONG in response to PING");
    addTLSOptions(echoServerApp);

    CLI::App* broadcastServerApp = app.add_subcommand("broadcast_server", "Broadcasting server");
    broadcastServerApp->fallthrough();
    broadcastServerApp->add_option("--port", port, "Port");
    broadcastServerApp->add_option("--host", hostname, "Hostname");
    addTLSOptions(broadcastServerApp);

    CLI::App* pingPongApp = app.add_subcommand("ping", "Ping pong");
    pingPongApp->fallthrough();
    pingPongApp->add_option("url", url, "Connection url")->required();
    addTLSOptions(pingPongApp);

    CLI::App* httpClientApp = app.add_subcommand("curl", "HTTP Client");
    httpClientApp->fallthrough();
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

    CLI::App* redisCliApp = app.add_subcommand("redis_cli", "Redis cli");
    redisCliApp->fallthrough();
    redisCliApp->add_option("--port", redisPort, "Port");
    redisCliApp->add_option("--host", hostname, "Hostname");
    redisCliApp->add_option("--password", password, "Password");

    CLI::App* redisPublishApp = app.add_subcommand("redis_publish", "Redis publisher");
    redisPublishApp->fallthrough();
    redisPublishApp->add_option("--port", redisPort, "Port");
    redisPublishApp->add_option("--host", hostname, "Hostname");
    redisPublishApp->add_option("--password", password, "Password");
    redisPublishApp->add_option("channel", channel, "Channel")->required();
    redisPublishApp->add_option("message", message, "Message")->required();
    redisPublishApp->add_option("-c", count, "Count");

    CLI::App* redisSubscribeApp = app.add_subcommand("redis_subscribe", "Redis subscriber");
    redisSubscribeApp->fallthrough();
    redisSubscribeApp->add_option("--port", redisPort, "Port");
    redisSubscribeApp->add_option("--host", hostname, "Hostname");
    redisSubscribeApp->add_option("--password", password, "Password");
    redisSubscribeApp->add_option("channel", channel, "Channel")->required();
    redisSubscribeApp->add_flag("-v", verbose, "Verbose");
    redisSubscribeApp->add_option("--pidfile", pidfile, "Pid file");

    CLI::App* cobraSubscribeApp = app.add_subcommand("cobra_subscribe", "Cobra subscriber");
    cobraSubscribeApp->fallthrough();
    cobraSubscribeApp->add_option("--pidfile", pidfile, "Pid file");
    cobraSubscribeApp->add_flag("-q", quiet, "Quiet / only display stats");
    cobraSubscribeApp->add_flag("--fluentd", fluentd, "Write fluentd prefix");
    addTLSOptions(cobraSubscribeApp);
    addCobraBotConfig(cobraSubscribeApp);

    CLI::App* cobraPublish = app.add_subcommand("cobra_publish", "Cobra publisher");
    cobraPublish->fallthrough();
    cobraPublish->add_option("--channel", channel, "Channel")->required();
    cobraPublish->add_option("--pidfile", pidfile, "Pid file");
    cobraPublish->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    addTLSOptions(cobraPublish);
    addCobraConfig(cobraPublish);

    CLI::App* cobraMetricsPublish =
        app.add_subcommand("cobra_metrics_publish", "Cobra metrics publisher");
    cobraMetricsPublish->fallthrough();
    cobraMetricsPublish->add_option("--channel", channel, "Channel")->required();
    cobraMetricsPublish->add_option("--pidfile", pidfile, "Pid file");
    cobraMetricsPublish->add_option("path", path, "Path to the file to send")
        ->required()
        ->check(CLI::ExistingPath);
    cobraMetricsPublish->add_flag("--stress", stress, "Stress mode");
    addTLSOptions(cobraMetricsPublish);
    addCobraConfig(cobraMetricsPublish);

    CLI::App* cobra2statsd = app.add_subcommand("cobra_to_statsd", "Cobra to statsd");
    cobra2statsd->fallthrough();
    cobra2statsd->add_option("--host", hostname, "Statsd host");
    cobra2statsd->add_option("--port", statsdPort, "Statsd port");
    cobra2statsd->add_option("--prefix", prefix, "Statsd prefix");
    cobra2statsd->add_option("--fields", fields, "Extract fields for naming the event")->join();
    cobra2statsd->add_option("--gauge", gauge, "Value to extract, and use as a statsd gauge")
        ->join();
    cobra2statsd->add_option("--timer", timer, "Value to extract, and use as a statsd timer")
        ->join();
    cobra2statsd->add_flag("-v", verbose, "Verbose");
    cobra2statsd->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(cobra2statsd);
    addCobraBotConfig(cobra2statsd);

    CLI::App* cobraMetrics2statsd = app.add_subcommand("cobra_metrics_to_statsd", "Cobra metrics to statsd");
    cobraMetrics2statsd->fallthrough();
    cobraMetrics2statsd->add_option("--host", hostname, "Statsd host");
    cobraMetrics2statsd->add_option("--port", statsdPort, "Statsd port");
    cobraMetrics2statsd->add_option("--prefix", prefix, "Statsd prefix");
    cobraMetrics2statsd->add_flag("-v", verbose, "Verbose");
    cobraMetrics2statsd->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(cobraMetrics2statsd);
    addCobraBotConfig(cobraMetrics2statsd);

    CLI::App* cobra2sentry = app.add_subcommand("cobra_to_sentry", "Cobra to sentry");
    cobra2sentry->fallthrough();
    cobra2sentry->add_option("--dsn", dsn, "Sentry DSN");
    cobra2sentry->add_flag("-v", verbose, "Verbose");
    cobra2sentry->add_option("--pidfile", pidfile, "Pid file");
    addTLSOptions(cobra2sentry);
    addCobraBotConfig(cobra2sentry);

    CLI::App* cobra2redisApp =
        app.add_subcommand("cobra_metrics_to_redis", "Cobra metrics to redis");
    cobra2redisApp->fallthrough();
    cobra2redisApp->add_option("--pidfile", pidfile, "Pid file");
    cobra2redisApp->add_option("--hostname", hostname, "Redis hostname");
    cobra2redisApp->add_option("--port", redisPort, "Redis port");
    cobra2redisApp->add_flag("-v", verbose, "Verbose");
    addTLSOptions(cobra2redisApp);
    addCobraBotConfig(cobra2redisApp);

    CLI::App* snakeApp = app.add_subcommand("snake", "Snake server");
    snakeApp->fallthrough();
    snakeApp->add_option("--port", port, "Connection url");
    snakeApp->add_option("--host", hostname, "Hostname");
    snakeApp->add_option("--pidfile", pidfile, "Pid file");
    snakeApp->add_option("--redis_hosts", redisHosts, "Redis hosts");
    snakeApp->add_option("--redis_port", redisPort, "Redis hosts");
    snakeApp->add_option("--redis_password", redisPassword, "Redis password");
    snakeApp->add_option("--apps_config_path", appsConfigPath, "Path to auth data")
        ->check(CLI::ExistingPath);
    snakeApp->add_flag("-v", verbose, "Verbose");
    snakeApp->add_flag("-d", disablePong, "Disable Pongs");
    addTLSOptions(snakeApp);

    CLI::App* httpServerApp = app.add_subcommand("httpd", "HTTP server");
    httpServerApp->fallthrough();
    httpServerApp->add_option("--port", port, "Port");
    httpServerApp->add_option("--host", hostname, "Hostname");
    httpServerApp->add_flag("-L", redirect, "Redirect all request to redirect_url");
    httpServerApp->add_option("--redirect_url", redirectUrl, "Url to redirect to");
    addTLSOptions(httpServerApp);

    CLI::App* autobahnApp = app.add_subcommand("autobahn", "Test client Autobahn compliance");
    autobahnApp->fallthrough();
    autobahnApp->add_option("--url", url, "url");
    autobahnApp->add_flag("-q", quiet, "Quiet");

    CLI::App* redisServerApp = app.add_subcommand("redis_server", "Redis server");
    redisServerApp->fallthrough();
    redisServerApp->add_option("--port", port, "Port");
    redisServerApp->add_option("--host", hostname, "Hostname");

    CLI::App* proxyServerApp = app.add_subcommand("proxy_server", "Proxy server");
    proxyServerApp->fallthrough();
    proxyServerApp->add_option("--port", port, "Port");
    proxyServerApp->add_option("--host", hostname, "Hostname");
    proxyServerApp->add_option("--remote_host", remoteHost, "Remote Hostname");
    proxyServerApp->add_flag("-v", verbose, "Verbose");
    addTLSOptions(proxyServerApp);

    CLI::App* minidumpApp = app.add_subcommand("upload_minidump", "Upload a minidump to sentry");
    minidumpApp->fallthrough();
    minidumpApp->add_option("--minidump", minidump, "Minidump path")
        ->required()
        ->check(CLI::ExistingPath);
    minidumpApp->add_option("--metadata", metadata, "Metadata path")
        ->required()
        ->check(CLI::ExistingPath);
    minidumpApp->add_option("--project", project, "Sentry Project")->required();
    minidumpApp->add_option("--key", key, "Sentry Key")->required();
    minidumpApp->add_flag("-v", verbose, "Verbose");

    CLI::App* dnsLookupApp = app.add_subcommand("dnslookup", "DNS lookup");
    dnsLookupApp->fallthrough();
    dnsLookupApp->add_option("host", hostname, "Hostname")->required();

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

    if (!logfile.empty())
    {
        try
        {
            auto fileLogger = spdlog::basic_logger_mt("ws", logfile);
            spdlog::set_default_logger(fileLogger);
            spdlog::flush_every(std::chrono::seconds(1));

            std::cerr << "All logs will be redirected to " << logfile << std::endl;
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            std::cerr << "Fatal error, log init failed: " << ex.what() << std::endl;
            ix::uninitNetSystem();
            return 1;
        }
    }

    // Cobra config
    cobraConfig.webSocketPerMessageDeflateOptions = ix::WebSocketPerMessageDeflateOptions(true);
    cobraConfig.socketTLSOptions = tlsOptions;

    cobraBotConfig.cobraConfig.webSocketPerMessageDeflateOptions =
        ix::WebSocketPerMessageDeflateOptions(true);
    cobraBotConfig.cobraConfig.socketTLSOptions = tlsOptions;

    int ret = 1;
    if (app.got_subcommand("transfer"))
    {
        ret = ix::ws_transfer_main(port, hostname, tlsOptions);
    }
    else if (app.got_subcommand("send"))
    {
        ret = ix::ws_send_main(url, path, disablePerMessageDeflate, tlsOptions);
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
                                  subprotocol,
                                  pingIntervalSecs);
    }
    else if (app.got_subcommand("chat"))
    {
        ret = ix::ws_chat_main(url, user);
    }
    else if (app.got_subcommand("echo_server"))
    {
        ret = ix::ws_echo_server_main(
            port, greetings, hostname, tlsOptions, ipv6, disablePerMessageDeflate, disablePong);
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
    else if (app.got_subcommand("redis_cli"))
    {
        ret = ix::ws_redis_cli_main(hostname, redisPort, password);
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
        int64_t sentCount = ix::cobra_to_stdout_bot(cobraBotConfig, fluentd, quiet);
        ret = (int) sentCount;
    }
    else if (app.got_subcommand("cobra_publish"))
    {
        ret = ix::ws_cobra_publish_main(cobraConfig, channel, path);
    }
    else if (app.got_subcommand("cobra_metrics_publish"))
    {
        ret = ix::ws_cobra_metrics_publish_main(cobraConfig, channel, path, stress);
    }
    else if (app.got_subcommand("cobra_to_statsd"))
    {
        if (!timer.empty() && !gauge.empty())
        {
            spdlog::error("--gauge and --timer options are exclusive. "
                          "you can only supply one");
            ret = 1;
        }
        else
        {
            ix::StatsdClient statsdClient(hostname, statsdPort, prefix, verbose);

            std::string errMsg;
            bool initialized = statsdClient.init(errMsg);
            if (!initialized)
            {
                spdlog::error(errMsg);
                ret = 1;
            }
            else
            {
                ret = (int) ix::cobra_to_statsd_bot(
                    cobraBotConfig, statsdClient, fields, gauge, timer, verbose);
            }
        }
    }
    else if (app.got_subcommand("cobra_metrics_to_statsd"))
    {
        ix::StatsdClient statsdClient(hostname, statsdPort, prefix, verbose);

        std::string errMsg;
        bool initialized = statsdClient.init(errMsg);
        if (!initialized)
        {
            spdlog::error(errMsg);
            ret = 1;
        }
        else
        {
            ret = (int) ix::cobra_metrics_to_statsd_bot(
                cobraBotConfig, statsdClient, verbose);
        }
    }
    else if (app.got_subcommand("cobra_to_sentry"))
    {
        ix::SentryClient sentryClient(dsn);
        sentryClient.setTLSOptions(tlsOptions);

        ret = (int) ix::cobra_to_sentry_bot(cobraBotConfig, sentryClient, verbose);
    }
    else if (app.got_subcommand("cobra_metrics_to_redis"))
    {
        ix::RedisClient redisClient;
        if (!redisClient.connect(hostname, redisPort))
        {
            spdlog::error("Cannot connect to redis host {}:{}",
                          redisHosts, redisPort);
            return 1;
        }
        else
        {
            ret = (int) ix::cobra_metrics_to_redis_bot(
                cobraBotConfig, redisClient, verbose);
        }
    }
    else if (app.got_subcommand("snake"))
    {
        ret = ix::ws_snake_main(port,
                                hostname,
                                redisHosts,
                                redisPort,
                                redisPassword,
                                verbose,
                                appsConfigPath,
                                tlsOptions,
                                disablePong);
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
    else if (app.got_subcommand("dnslookup"))
    {
        ret = ix::ws_dns_lookup(hostname);
    }
    else if (version)
    {
        spdlog::info("ws {}", ix::userAgent());
    }
    else
    {
        spdlog::error("A subcommand or --version is required");
    }

    ix::uninitNetSystem();
    return ret;
}
