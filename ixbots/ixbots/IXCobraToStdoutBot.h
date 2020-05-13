/*
 *  IXCobraToStdoutBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include "IXCobraBotConfig.h"
#include <stddef.h>
#include <string>

namespace ix
{
    int64_t cobra_to_stdout_bot(const ix::CobraBotConfig& config,
                                bool fluentd,
                                bool quiet);
} // namespace ix
