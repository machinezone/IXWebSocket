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
    if (argc < 2)
    {
        std::cerr << "Usage: ws [transfer|send|receive]" << std::endl;
        return 1;
    }

    std::string subCommand(argv[1]);
    std::cout << "ws running in " << subCommand << " mode" << std::endl;

    if (subCommand == "transfer")
    {
        int port = 8080;
        if (argc == 3)
        {
            std::stringstream ss;
            ss << argv[2];
            ss >> port;
        }
        return ix::ws_transfer_main(port);
    }

    if (subCommand == "send")
    {
        if (argc != 4)
        {
            std::cerr << "Usage: ws_send <url> <path>" << std::endl;
            return 1;
        }
        std::string url = argv[2];
        std::string path = argv[3];

        return ix::ws_send_main(url, path);
    }

    if (subCommand == "receive")
    {
        if (argc != 3)
        {
            std::cerr << "Usage: ws_receive <url>" << std::endl;
            return 1;
        }
        std::string url = argv[2];

        bool enablePerMessageDeflate = false;
        return ix::ws_receive_main(url, enablePerMessageDeflate);
    }

    std::cerr << "Unknown sub command: " << subCommand << std::endl;
    return 1;
}
