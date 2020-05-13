/*
 *  IXCobraToStatsdBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include <ixbots/IXStatsdClient.h>
#include "IXCobraBotConfig.h"
#include <stddef.h>
#include <string>

namespace ix
{
    int64_t cobra_to_statsd_bot(const ix::CobraBotConfig& config,
                                StatsdClient& statsdClient,
                                const std::string& fields,
                                const std::string& gauge,
                                const std::string& timer,
                                bool verbose);
} // namespace ix
