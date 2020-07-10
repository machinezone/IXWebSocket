/*
 *  IXStreamSql.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>
#include "nlohmann/json.hpp"

namespace snake
{
    class StreamSql
    {
    public:
        StreamSql(const std::string& sqlFilter);
        ~StreamSql() = default;

        bool match(const nlohmann::json& pdu);

    private:
    };
}
