/*
 *  IXCobraMetricsThreadedPublisher.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

#include "IXCobraMetricsThreadedPublisher.h"
#include <ixwebsocket/IXSetThreadName.h>
#include <ixcore/utils/IXCoreLogger.h>

#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <iostream>


namespace ix
{
    CobraMetricsThreadedPublisher::CobraMetricsThreadedPublisher() :
        _stop(false)
    {
        _cobra_connection.setEventCallback(
            []
            (ix::CobraConnectionEventType eventType,
             const std::string& errMsg,
             const ix::WebSocketHttpHeaders& headers,
             const std::string& subscriptionId,
             CobraConnection::MsgId msgId)
            {
                std::stringstream ss;

                if (eventType == ix::CobraConnection_EventType_Open)
                {
                    ss << "Handshake headers" << std::endl;

                    for (auto it : headers)
                    {
                        ss << it.first << ": " << it.second << std::endl;
                    }
                }
                else if (eventType == ix::CobraConnection_EventType_Authenticated)
                {
                    ss << "Authenticated";
                }
                else if (eventType == ix::CobraConnection_EventType_Error)
                {
                    ss << "Error: " << errMsg;
                }
                else if (eventType == ix::CobraConnection_EventType_Closed)
                {
                    ss << "Connection closed: " << errMsg;
                }
                else if (eventType == ix::CobraConnection_EventType_Subscribed)
                {
                    ss << "Subscribed through subscription id: " << subscriptionId;
                }
                else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
                {
                    ss << "Unsubscribed through subscription id: " << subscriptionId;
                }
                else if (eventType == ix::CobraConnection_EventType_Published)
                {
                    ss << "Published message " << msgId << " acked";
                }

                ix::IXCoreLogger::Log(ss.str().c_str());
        });
    }

    CobraMetricsThreadedPublisher::~CobraMetricsThreadedPublisher()
    {
        // The background thread won't be joinable if it was never
        // started by calling CobraMetricsThreadedPublisher::start
        if (!_thread.joinable()) return;

        _stop = true;
        _condition.notify_one();
        _thread.join();
    }

    void CobraMetricsThreadedPublisher::start()
    {
        if (_thread.joinable()) return; // we've already been started

        _thread = std::thread(&CobraMetricsThreadedPublisher::run, this);
    }

    void CobraMetricsThreadedPublisher::configure(const std::string& appkey,
                                                  const std::string& endpoint,
                                                  const std::string& channel,
                                                  const std::string& rolename,
                                                  const std::string& rolesecret,
                                                  bool enablePerMessageDeflate)
    {
        _channel = channel;

        ix::WebSocketPerMessageDeflateOptions webSocketPerMessageDeflateOptions(enablePerMessageDeflate);
        _cobra_connection.configure(appkey, endpoint,
                                    rolename, rolesecret,
                                    webSocketPerMessageDeflateOptions);
    }

    void CobraMetricsThreadedPublisher::pushMessage(MessageKind messageKind)
    {
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            _queue.push(messageKind);
        }

        // wake up one thread
        _condition.notify_one();
    }

    void CobraMetricsThreadedPublisher::setPublishMode(CobraConnectionPublishMode publishMode)
    {
        _cobra_connection.setPublishMode(publishMode);
    }

    bool CobraMetricsThreadedPublisher::flushQueue()
    {
        return _cobra_connection.flushQueue();
    }

    void CobraMetricsThreadedPublisher::run()
    {
        setThreadName("CobraMetricsPublisher");

        _cobra_connection.connect();

        while (true)
        {
            Json::Value msg;
            MessageKind messageKind;

            {
                std::unique_lock<std::mutex> lock(_queue_mutex);

                while (!_stop && _queue.empty())
                {
                    _condition.wait(lock);
                }
                if (_stop)
                {
                    _cobra_connection.disconnect();
                    return;
                }

                messageKind = _queue.front();
                _queue.pop();
            }

            switch (messageKind)
            {
                case MessageKind::Suspend:
                {
                    _cobra_connection.suspend();
                    continue;
                }; break;

                case MessageKind::Resume:
                {
                    _cobra_connection.resume();
                    continue;
                }; break;

                case MessageKind::Message:
                {
                    if (_cobra_connection.getPublishMode() == CobraConnection_PublishMode_Immediate)
                    {
                        _cobra_connection.publishNext();
                    }
                }; break;
            }
        }
    }

    CobraConnection::MsgId CobraMetricsThreadedPublisher::push(const Json::Value& msg)
    {
        static const std::string messageIdKey("id");

        //
        // Publish to multiple channels. This let the consumer side
        // easily subscribe to all message of a certain type, without having
        // to do manipulations on the messages on the server side.
        //
        Json::Value channels;

        channels.append(_channel);
        if (msg.isMember(messageIdKey))
        {
            channels.append(msg[messageIdKey]);
        }
        auto res = _cobra_connection.prePublish(channels, msg, true);
        auto msgId = res.first;

        pushMessage(MessageKind::Message);

        return msgId;
    }

    void CobraMetricsThreadedPublisher::suspend()
    {
        pushMessage(MessageKind::Suspend);
    }

    void CobraMetricsThreadedPublisher::resume()
    {
        pushMessage(MessageKind::Resume);
    }

    bool CobraMetricsThreadedPublisher::isConnected() const
    {
        return _cobra_connection.isConnected();
    }

    bool CobraMetricsThreadedPublisher::isAuthenticated() const
    {
        return _cobra_connection.isAuthenticated();
    }

} // namespace ix
