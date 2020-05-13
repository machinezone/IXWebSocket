/*
 *  IXCobraToSentryBot.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019-2020 Machine Zone, Inc. All rights reserved.
 */
#pragma once

#include <cstdint>
#include "IXCobraBotConfig.h"
#include <ixsentry/IXSentryClient.h>
#include <string>

namespace ix
{
    int64_t cobra_to_sentry_bot(const CobraBotConfig& config,
                                SentryClient& sentryClient,
                                bool verbose);
} // namespace ix
