/*
 *  cobra_publisher.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <atomic>
#include <ixwebsocket/IXWebSocket.h>
#include "IXCobraConnection.h"
#include "jsoncpp/json/json.h"

void msleep(int ms)
{
    std::chrono::duration<double, std::milli> duration(ms);
    std::this_thread::sleep_for(duration);
}

int main(int argc, char* argv[])
{
    if (argc != 7)
    {
        std::cerr << "Usage error: need 6 arguments." << std::endl;
    }

    std::string endpoint = argv[1];
    std::string appkey = argv[2];
    std::string channel = argv[3];
    std::string rolename = argv[4];
    std::string rolesecret = argv[5];
    std::string path = argv[6];

    std::atomic<size_t> incomingBytes(0);
    std::atomic<size_t> outgoingBytes(0);
    ix::CobraConnection::setTrafficTrackerCallback(
        [&incomingBytes, &outgoingBytes](size_t size, bool incoming)
        {
            if (incoming)
            {
                incomingBytes += size;
            }
            else
            {
                outgoingBytes += size;
            }
        }
    );

    bool done = false;
    ix::CobraConnection cobraConnection;
    ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(
        true, false, false, 15, 15);
    cobraConnection.configure(appkey, endpoint, rolename, rolesecret,
                               webSocketPerMessageDeflateOptions);
    cobraConnection.connect();
    cobraConnection.setEventCallback(
        [&cobraConnection, channel, path, &done]
        (ix::CobraConnectionEventType eventType,
         const std::string& errMsg,
         const ix::WebSocketHttpHeaders& headers)
        {
            if (eventType == ix::CobraConnection_EventType_Open)
            {
                std::cout << "Handshake Headers:" << std::endl;
                for (auto it : headers)
                {
                    std::cout << it.first << ": " << it.second << std::endl;
                }
            }
            else if (eventType == ix::CobraConnection_EventType_Authenticated)
            {
                std::cout << "Authenticated" << std::endl;

                std::string line;
                std::ifstream f(path);
                if (!f.is_open())
                {
                    std::cerr << "Error while opening file: " << path << std::endl;
                }

                int n = 0;
                while (getline(f, line))
                {
                    Json::Value value;
                    Json::Reader reader;
                    reader.parse(line, value);

                    cobraConnection.publish(channel, value);
                    n++;
                }
                std::cerr << "#published messages: " << n << std::endl;

                if (f.bad())
                {
                    std::cerr << "Error while opening file: " << path << std::endl;
                }

                done = true;
            }
            else if (eventType == ix::CobraConnection_EventType_Error)
            {
                std::cerr << "Cobra Error received: " << errMsg << std::endl;
                done = true;
            }
            else if (eventType == ix::CobraConnection_EventType_Closed)
            {
                std::cerr << "Cobra connection closed" << std::endl;
            }
        }
    );

    while (!done)
    {
        msleep(1);
    }

    std::cout << "Incoming bytes: " << incomingBytes << std::endl;
    std::cout << "Outgoing bytes: " << outgoingBytes << std::endl;

    return 0;
}
