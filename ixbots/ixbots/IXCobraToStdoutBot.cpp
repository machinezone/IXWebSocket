/*
 *  IXCobraToStdoutBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToStdoutBot.h"

#include "IXCobraBot.h"
#include <chrono>
#include <iostream>
#include <sstream>

namespace ix
{
    using StreamWriterPtr = std::unique_ptr<Json::StreamWriter>;

    StreamWriterPtr makeStreamWriter()
    {
        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None";
        builder["indentation"] = ""; // will make the JSON object compact
        std::unique_ptr<Json::StreamWriter> jsonWriter(builder.newStreamWriter());
        return jsonWriter;
    }

    std::string timeSinceEpoch()
    {
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
        std::chrono::system_clock::duration dtn = tp.time_since_epoch();

        std::stringstream ss;
        ss << dtn.count() * std::chrono::system_clock::period::num /
                  std::chrono::system_clock::period::den;
        return ss.str();
    }

    void writeToStdout(bool fluentd,
                       const StreamWriterPtr& jsonWriter,
                       const Json::Value& msg,
                       const std::string& position)
    {
        Json::Value enveloppe;
        if (fluentd)
        {
            enveloppe["producer"] = "cobra";
            enveloppe["consumer"] = "fluentd";

            Json::Value nestedMessage(msg);
            nestedMessage["position"] = position;
            nestedMessage["created_at"] = timeSinceEpoch();
            enveloppe["message"] = nestedMessage;

            jsonWriter->write(enveloppe, &std::cout);
            std::cout << std::endl; // add lf and flush
        }
        else
        {
            enveloppe = msg;
            std::cout << position << " ";
            jsonWriter->write(enveloppe, &std::cout);
            std::cout << std::endl;
        }
    }

    int64_t cobra_to_stdout_bot(const ix::CobraBotConfig& config,
                                bool fluentd,
                                bool quiet)
    {
        CobraBot bot;
        auto jsonWriter = makeStreamWriter();

        bot.setOnBotMessageCallback(
            [&fluentd, &quiet, &jsonWriter](const Json::Value& msg,
                                            const std::string& position,
                                            std::atomic<bool>& /*throttled*/,
                                            std::atomic<bool>& /*fatalCobraError*/,
                                            std::atomic<uint64_t>& sentCount) -> void {
                if (!quiet)
                {
                    writeToStdout(fluentd, jsonWriter, msg, position);
                }
                sentCount++;
            });

        return bot.run(config);
    }
} // namespace ix
