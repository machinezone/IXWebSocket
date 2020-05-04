/*
 *  IXCobraToSentryBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include <ixcobra/IXCobraConfig.h>
#include <ixsentry/IXSentryClient.h>
#include <string>

namespace ix
{
    int64_t cobra_to_sentry_bot(const CobraConfig& config,
                                const std::string& channel,
                                const std::string& filter,
                                const std::string& position,
                                SentryClient& sentryClient,
                                bool verbose,
                                bool enableHeartbeat,
                                int runtime);
} // namespace ix
