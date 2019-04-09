/*
 *  ws_cobra_subscribe.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include "IXCobraConnection.h"

namespace ix
{
    int ws_cobra_subscribe_main(const std::string& appkey,
                                const std::string& endpoint,
                                const std::string& rolename,
                                const std::string& rolesecret,
                                const std::string& channel,
                                bool verbose)
    {

        ix::CobraConnection conn;
        conn.configure(appkey, endpoint,
                       rolename, rolesecret,
                       ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        Json::FastWriter jsonWriter;

        conn.setEventCallback(
            [&conn, &channel, &jsonWriter]
            (ix::CobraConnectionEventType eventType,
             const std::string& errMsg,
             const ix::WebSocketHttpHeaders& headers,
             const std::string& subscriptionId)
            {
                if (eventType == ix::CobraConnection_EventType_Open)
                {
                    std::cout << "Subscriber: connected" << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_Authenticated)
                {
                    std::cout << "Subscriber authenticated" << std::endl;
                    conn.subscribe(channel,
                                   [&jsonWriter](const Json::Value& msg)
                                   {
                                       // std::cout << "Received message" << std::endl;
                                       std::cout << jsonWriter.write(msg) << std::endl;
                                   });
                }
                else if (eventType == ix::CobraConnection_EventType_Subscribed)
                {
                    std::cout << "Subscriber: subscribed to channel " << subscriptionId << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
                {
                    std::cout << "Subscriber: unsubscribed from channel " << subscriptionId << std::endl;
                }
                else if (eventType == ix::CobraConnection_EventType_Error)
                {
                    std::cout << "Subscriber: error" << errMsg << std::endl;
                }
            }
        );

        while (true)
        {
            std::chrono::duration<double, std::milli> duration(10);
            std::this_thread::sleep_for(duration);
        }

        return 0;
    }
}
