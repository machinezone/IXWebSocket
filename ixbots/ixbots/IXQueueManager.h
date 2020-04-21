/*
 *  IXQueueManager.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <condition_variable>
#include <json/json.h>
#include <map>
#include <mutex>
#include <queue>
#include <stddef.h>

namespace ix
{
    class QueueManager
    {
    public:
        QueueManager(size_t maxQueueSize)
            : _maxQueueSize(maxQueueSize)
        {
        }

        std::pair<Json::Value, std::string> pop();
        void add(const Json::Value& msg, const std::string& position);

    private:
        std::map<std::string, std::queue<std::pair<Json::Value, std::string>>> _queues;
        std::mutex _mutex;
        std::condition_variable _condition;
        size_t _maxQueueSize;
    };
} // namespace ix
