/*
 *  IXCobraBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <functional>
#include <ixcobra/IXCobraConfig.h>
#include <json/json.h>
#include <stddef.h>

namespace ix
{
    using OnBotMessageCallback = std::function<bool(const Json::Value&,
                                                    const std::string&,
                                                    const bool verbose,
                                                    std::atomic<bool>&,
                                                    std::atomic<bool>&)>;

    class CobraBot
    {
    public:
        CobraBot() = default;

        int64_t run(const CobraConfig& config,
                    const std::string& channel,
                    const std::string& filter,
                    const std::string& position,
                    bool verbose,
                    size_t maxQueueSize,
                    bool useQueue,
                    bool enableHeartbeat,
                    int runtime);

        void setOnBotMessageCallback(const OnBotMessageCallback& callback);

    private:
        OnBotMessageCallback _onBotMessageCallback;
    };
} // namespace ix
