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
                            int timeoutSecs,
                            bool followRedirects,
                            bool verbose,
                            bool save,
                            const std::string& output,
                            bool compress);

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
