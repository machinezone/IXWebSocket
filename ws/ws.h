/*
 *  ws.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */
#pragma once

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
                            bool compress);

    int ws_ping_pong_main(const std::string& url);

    int ws_echo_server_main(int port, const std::string& hostname);
    int ws_broadcast_server_main(int port, const std::string& hostname);
    int ws_transfer_main(int port, const std::string& hostname);

    int ws_chat_main(const std::string& url,
                     const std::string& user);

    int ws_connect_main(const std::string& url);

    int ws_receive_main(const std::string& url,
                        bool enablePerMessageDeflate,
                        int delayMs);

    int ws_send_main(const std::string& url,
                     const std::string& path);

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

    int ws_cobra_subscribe_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                bool verbose);

    int ws_cobra_to_statsd_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                const std::string& host,
                                int port,
                                const std::string& prefix,
                                const std::string& fields,
                                bool verbose);

    int ws_cobra_to_sentry_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                const std::string& dsn,
                                bool verbose,
                                bool strict,
                                int jobs);
}
