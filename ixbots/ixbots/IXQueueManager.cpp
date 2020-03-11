/*
 *  IXQueueManager.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXQueueManager.h"
#include <vector>
#include <algorithm>

namespace ix
{
    Json::Value QueueManager::pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_queues.empty())
        {
            Json::Value val;
            return val;
        }

        std::vector<std::string> games;
        for (auto it : _queues)
        {
            games.push_back(it.first);
        }

        std::random_shuffle(games.begin(), games.end());
        std::string game = games[0];

        _condition.wait(lock, [this] { return !_stop; });

        if (_queues[game].empty())
        {
            Json::Value val;
            return val;
        }

        auto msg = _queues[game].front();
        _queues[game].pop();
        return msg;
    }

    void QueueManager::add(Json::Value msg)
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
            _queues[game].push(msg);
            _condition.notify_one();
        }
    }
}
