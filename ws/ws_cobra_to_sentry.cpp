/*
 *  ws_cobra_to_sentry.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <ixbots/IXCobraToSentryBot.h>

namespace ix
{
    int ws_cobra_to_sentry_main(const ix::CobraConfig& config,
                                const std::string& channel,
                                const std::string& filter,
                                const std::string& dsn,
                                bool verbose,
                                bool strict,
                                int jobs,
                                size_t maxQueueSize)
    {
        bool enableHeartbeat = true;
        int runtime = -1;
        return cobra_to_sentry_bot(
            config, channel, filter, dsn, verbose, strict, jobs, maxQueueSize, enableHeartbeat, runtime);
    }
} // namespace ix
