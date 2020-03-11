/*
 *  IXCobraToStatsdBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <ixcobra/IXCobraConfig.h>
#include <string>
#include <stddef.h>

namespace ix
{
    int cobra_to_statsd_bot(const ix::CobraConfig& config,
                            const std::string& channel,
                            const std::string& filter,
                            const std::string& dsn,
                            bool verbose,
                            bool strict,
                            int jobs,
                            size_t maxQueueSize);
} // namespace ix
