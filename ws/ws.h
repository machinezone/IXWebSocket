/*
 *  ws.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <ixcobra/IXCobraConfig.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <string>

namespace ix
{
    int ws_http_client_main(const std::string& url,
                            const std::string& headers,
                            const std::string& data,
                            bool headersOnly,
                            int connectTimeout,
                            int transferTimeout,
                            bool followRedirects,
                            int maxRedirects,
                            bool verbose,
                            bool save,
                            const std::string& output,
                            bool compress,
                            const ix::SocketTLSOptions& tlsOptions);

    int ws_ping_pong_main(const std::string& url, const ix::SocketTLSOptions& tlsOptions);

    int ws_echo_server_main(int port,
                            bool greetings,
                            const std::string& hostname,
                            const ix::SocketTLSOptions& tlsOptions,
                            bool ipv6,
                            bool disablePerMessageDeflate,
                            bool disablePong);

    int ws_broadcast_server_main(int port,
                                 const std::string& hostname,
                                 const ix::SocketTLSOptions& tlsOptions);
    int ws_transfer_main(int port,
                         const std::string& hostname,
                         const ix::SocketTLSOptions& tlsOptions);

    int ws_chat_main(const std::string& url, const std::string& user);

    int ws_connect_main(const std::string& url,
                        const std::string& headers,
                        bool disableAutomaticReconnection,
                        bool disablePerMessageDeflate,
                        bool binaryMode,
                        uint32_t maxWaitBetweenReconnectionRetries,
                        const ix::SocketTLSOptions& tlsOptions,
                        const std::string& subprotocol,
                        int pingIntervalSecs);

    int ws_receive_main(const std::string& url,
                        bool enablePerMessageDeflate,
                        int delayMs,
                        const ix::SocketTLSOptions& tlsOptions);

    int ws_send_main(const std::string& url,
                     const std::string& path,
                     bool disablePerMessageDeflate,
                     const ix::SocketTLSOptions& tlsOptions);

    int ws_redis_cli_main(const std::string& hostname,
                          int port,
                          const std::string& password);

    int ws_redis_publish_main(const std::string& hostname,
                              int port,
                              const std::string& password,
                              const std::string& channel,
                              const std::string& message,
                              int count);

    int ws_redis_subscribe_main(const std::string& hostname,
                                int port,
                                const std::string& password,
                                const std::string& channel,
                                bool verbose);

    int ws_cobra_publish_main(const ix::CobraConfig& appkey,
                              const std::string& channel,
                              const std::string& path);

    int ws_cobra_metrics_publish_main(const ix::CobraConfig& config,
                                      const std::string& channel,
                                      const std::string& path,
                                      bool stress);

    int ws_cobra_metrics_to_redis(const ix::CobraConfig& config,
                                  const std::string& channel,
                                  const std::string& filter,
                                  const std::string& position,
                                  const std::string& host,
                                  int port);

    int ws_snake_main(int port,
                      const std::string& hostname,
                      const std::string& redisHosts,
                      int redisPort,
                      const std::string& redisPassword,
                      bool verbose,
                      const std::string& appsConfigPath,
                      const ix::SocketTLSOptions& tlsOptions,
                      bool disablePong);

    int ws_httpd_main(int port,
                      const std::string& hostname,
                      bool redirect,
                      const std::string& redirectUrl,
                      const ix::SocketTLSOptions& tlsOptions);

    int ws_autobahn_main(const std::string& url, bool quiet);

    int ws_redis_server_main(int port, const std::string& hostname);

    int ws_proxy_server_main(int port,
                             const std::string& hostname,
                             const ix::SocketTLSOptions& tlsOptions,
                             const std::string& remoteHost,
                             bool verbose);

    int ws_sentry_minidump_upload(const std::string& metadataPath,
                                  const std::string& minidump,
                                  const std::string& project,
                                  const std::string& key,
                                  bool verbose);

    int ws_dns_lookup(const std::string& hostname);
} // namespace ix
