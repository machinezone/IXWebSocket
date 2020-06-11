/*
 *  IXCobraMetricsToRedisBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include <ixredis/IXRedisClient.h>
#include "IXCobraBotConfig.h"
#include <stddef.h>
#include <string>

namespace ix
{
    int64_t cobra_metrics_to_redis_bot(const ix::CobraBotConfig& config,
                                       RedisClient& redisClient,
                                       bool verbose);
} // namespace ix

