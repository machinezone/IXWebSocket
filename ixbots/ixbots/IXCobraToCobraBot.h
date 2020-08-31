/*
 *  IXCobraToCobraBot.h
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
    int64_t cobra_to_cobra_bot(const ix::CobraBotConfig& config,
                               const std::string& republishChannel,
                               const std::string& publisherRolename,
                               const std::string& publisherRolesecret);
} // namespace ix
