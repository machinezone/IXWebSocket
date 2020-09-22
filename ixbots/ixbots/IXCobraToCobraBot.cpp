/*
 *  IXCobraToCobraBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToCobraBot.h"

#include "IXCobraBot.h"
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <sstream>

namespace ix
{
    int64_t cobra_to_cobra_bot(const ix::CobraBotConfig& cobraBotConfig,
                               const std::string& republishChannel,
                               const std::string& publisherRolename,
                               const std::string& publisherRolesecret)
    {
        CobraBot bot;

        CobraMetricsPublisher cobraMetricsPublisher;
        CobraConfig cobraPublisherConfig = cobraBotConfig.cobraConfig;
        cobraPublisherConfig.rolename = publisherRolename;
        cobraPublisherConfig.rolesecret = publisherRolesecret;
        cobraPublisherConfig.headers["X-Cobra-Republish-Channel"] = republishChannel;

        cobraMetricsPublisher.configure(cobraPublisherConfig, republishChannel);

        bot.setOnBotMessageCallback(
            [&republishChannel, &cobraMetricsPublisher](const Json::Value& msg,
                                                        const std::string& /*position*/,
                                                        std::atomic<bool>& /*throttled*/,
                                                        std::atomic<bool>& /*fatalCobraError*/,
                                                        std::atomic<uint64_t>& sentCount) -> void {
                Json::Value msgWithNoId(msg);
                msgWithNoId.removeMember("id");
            
                cobraMetricsPublisher.push(republishChannel, msg);
                sentCount++;
            });

        return bot.run(cobraBotConfig);
    }
} // namespace ix
