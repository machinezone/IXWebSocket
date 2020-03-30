/*
 *  IXCobraToStatsdBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <ixcobra/IXCobraConfig.h>
#include <ixbots/IXStatsdClient.h>
#include <string>
#include <stddef.h>

namespace ix
{
    int cobra_to_statsd_bot(const ix::CobraConfig& config,
                            const std::string& channel,
                            const std::string& filter,
                            const std::string& position,
                            StatsdClient& statsdClient,
                            const std::string& fields,
                            const std::string& gauge,
                            bool verbose,
                            size_t maxQueueSize,
                            bool enableHeartbeat,
                            int runtime);
} // namespace ix
