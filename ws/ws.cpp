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

namespace ix
{
    int ws_receive_main(const std::string& url,
                        bool enablePerMessageDeflate);

    extern int ws_transfer_main(int port);

    extern int ws_send_main(const std::string& url,
                            const std::string& path);
}

int main(int argc, char** argv)
{
    CLI::App app{"ws is a websocket tool"};
    app.require_subcommand();

    std::string url;
    std::string path;
    int port = 8080;

    CLI::App* sendApp = app.add_subcommand("send", "Send a file");
    sendApp->add_option("url", url, "Connection url")->required();
    sendApp->add_option("path", path, "Path to the file to send")->required();

    CLI::App* receiveApp = app.add_subcommand("receive", "Receive a file");
    receiveApp->add_option("url", url, "Connection url")->required();

    CLI::App* transferApp = app.add_subcommand("transfer", "Broadcasting server");
    transferApp->add_option("--port", port, "Connection url");

    CLI11_PARSE(app, argc, argv);

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
    else
    {
        assert(false);
    }

    return 1;
}
