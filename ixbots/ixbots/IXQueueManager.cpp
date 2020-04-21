/*
 *  IXQueueManager.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXQueueManager.h"

#include <algorithm>
#include <vector>

namespace ix
{
    std::pair<Json::Value, std::string> QueueManager::pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_queues.empty())
        {
            Json::Value val;
            return std::make_pair(val, std::string());
        }

        std::vector<std::string> games;
        for (auto it : _queues)
        {
            games.push_back(it.first);
        }

        std::random_shuffle(games.begin(), games.end());
        std::string game = games[0];

        auto duration = std::chrono::seconds(1);
        _condition.wait_for(lock, duration);

        if (_queues[game].empty())
        {
            Json::Value val;
            return std::make_pair(val, std::string());
        }

        auto msg = _queues[game].front();
        _queues[game].pop();
        return msg;
    }

    void QueueManager::add(const Json::Value& msg, const std::string& position)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        std::string game;
        if (msg.isMember("device") && msg["device"].isMember("game"))
        {
            game = msg["device"]["game"].asString();
        }

        if (game.empty()) return;

        // if the sending is not fast enough there is no point
        // in queuing too many events.
        if (_queues[game].size() < _maxQueueSize)
        {
            _queues[game].push(std::make_pair(msg, position));
            _condition.notify_one();
        }
    }
} // namespace ix
