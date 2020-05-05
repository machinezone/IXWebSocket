/*
 *  IXCobraToStdoutBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include <ixcobra/IXCobraConfig.h>
#include <stddef.h>
#include <string>

namespace ix
{
    int64_t cobra_to_stdout_bot(const ix::CobraConfig& config,
                                const std::string& channel,
                                const std::string& filter,
                                const std::string& position,
                                bool fluentd,
                                bool quiet,
                                bool enableHeartbeat,
                                int runtime);
} // namespace ix
