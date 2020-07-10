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
        StreamSql(const std::string& sqlFilter = std::string());
        ~StreamSql() = default;

        bool match(const nlohmann::json& msg);
        bool valid() const;

    private:
        std::string _field;
        std::string _operator;
        std::string _value;
        bool _valid;
    };
}
