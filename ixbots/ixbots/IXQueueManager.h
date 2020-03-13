/*
 *  IXQueueManager.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <stddef.h>
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
        QueueManager(size_t maxQueueSize)
            : _maxQueueSize(maxQueueSize)
        {
        }

        Json::Value pop();
        void add(Json::Value msg);

    private:
        std::map<std::string, std::queue<Json::Value>> _queues;
        std::mutex _mutex;
        std::condition_variable _condition;
        size_t _maxQueueSize;
    };
}
