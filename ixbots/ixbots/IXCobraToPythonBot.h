/*
 *  IXCobraMetricsToStatsdBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include <ixbots/IXStatsdClient.h>
#include "IXCobraBotConfig.h"
#include <stddef.h>
#include <string>

namespace ix
{
    int64_t cobra_to_python_bot(const ix::CobraBotConfig& config,
                                StatsdClient& statsdClient,
                                const std::string& moduleName);
} // namespace ix
