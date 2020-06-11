/*
 *  IXCobraBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <atomic>
#include <functional>
#include "IXCobraBotConfig.h"
#include <json/json.h>
#include <stddef.h>

namespace ix
{
    using OnBotMessageCallback = std::function<void(const Json::Value&,
                                                    const std::string&,
                                                    std::atomic<bool>&,
                                                    std::atomic<bool>&,
                                                    std::atomic<uint64_t>&)>;

    class CobraBot
    {
    public:
        CobraBot() = default;

        int64_t run(const CobraBotConfig& botConfig);
        void setOnBotMessageCallback(const OnBotMessageCallback& callback);

        std::string getDeviceIdentifier(const Json::Value& msg);

    private:
        OnBotMessageCallback _onBotMessageCallback;
    };
} // namespace ix
