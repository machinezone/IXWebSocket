/*
 *  IXQueueManager.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <stddef.h>
#include <atomic>
#include <json/json.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>

namespace ix
{
    class QueueManager
    {
    public:
        QueueManager(size_t maxQueueSize, std::atomic<bool>& stop)
            : _maxQueueSize(maxQueueSize)
            , _stop(stop)
        {
        }

        Json::Value pop();
        void add(Json::Value msg);

    private:
        std::map<std::string, std::queue<Json::Value>> _queues;
        std::mutex _mutex;
        std::condition_variable _condition;
        size_t _maxQueueSize;
        std::atomic<bool>& _stop;
    };
}
