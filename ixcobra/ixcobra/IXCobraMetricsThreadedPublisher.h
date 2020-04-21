/*
 *  IXCobraMetricsThreadedPublisher.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017 Machine Zone. All rights reserved.
 */

#pragma once

#include "IXCobraConnection.h"
#include <atomic>
#include <condition_variable>
#include <json/json.h>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace ix
{
    struct SocketTLSOptions;

    class CobraMetricsThreadedPublisher
    {
    public:
        CobraMetricsThreadedPublisher();
        ~CobraMetricsThreadedPublisher();

        /// Configuration / set keys, etc...
        void configure(const CobraConfig& config, const std::string& channel);

        /// Start the worker thread, used for background publishing
        void start();

        /// Push a msg to our queue of messages to be published to cobra on the background
        //  thread. Main user right now is the Cobra Metrics System
        CobraConnection::MsgId push(const Json::Value& msg);

        /// Set cobra connection publish mode
        void setPublishMode(CobraConnectionPublishMode publishMode);

        /// Flush the publish queue
        bool flushQueue();

        /// Lifecycle management. Free resources when backgrounding
        void suspend();
        void resume();

        /// Tells whether the socket connection is opened
        bool isConnected() const;

        /// Returns true only if we're authenticated
        bool isAuthenticated() const;

    private:
        enum class MessageKind
        {
            Message = 0,
            Suspend = 1,
            Resume = 2
        };

        /// Push a message to be processed by the background thread
        void pushMessage(MessageKind messageKind);

        /// Get a wait time which is increasing exponentially based on the number of retries
        uint64_t getWaitTimeExp(int retry_count);

        /// Debugging routine to print the connection parameters to the console
        void printInfo();

        /// Publish a message to satory
        /// Will retry multiple times (3) if a problem occurs.
        ///
        /// Right now, only called on the publish worker thread.
        void safePublish(const Json::Value& msg);

        /// The worker thread "daemon" method. That method never returns unless _stop is set to true
        void run();

        /// Our connection to cobra.
        CobraConnection _cobra_connection;

        /// The channel we are publishing to
        std::string _channel;

        /// Internal data structures used to publish to cobra
        /// Pending messages are stored into a queue, which is protected by a mutex
        /// We used a condition variable to prevent the worker thread from busy polling
        /// So we notify the condition variable when an incoming message arrives to signal
        /// that it should wake up and take care of publishing it to cobra
        /// To shutdown the worker thread one has to set the _stop boolean to true.
        /// This is done in the destructor
        std::queue<MessageKind> _queue;
        mutable std::mutex _queue_mutex;
        std::condition_variable _condition;
        std::atomic<bool> _stop;
        std::thread _thread;
    };

} // namespace ix
