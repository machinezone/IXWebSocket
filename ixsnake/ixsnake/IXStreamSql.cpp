/*
 *  IXStreamSql.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 *
 *  Super simple hacked up version of a stream sql expression,
 *  that only supports non nested field evaluation
 */

#include "IXStreamSql.h"
#include <sstream>
#include <iostream>

namespace snake
{
    StreamSql::StreamSql(const std::string& sqlFilter)
        : _valid(false)
    {
        std::string token;
        std::stringstream tokenStream(sqlFilter);
        std::vector<std::string> tokens;

        // Split by ' '
        while (std::getline(tokenStream, token, ' '))
        {
            tokens.push_back(token);
        }

        _valid = tokens.size() == 8;
        if (!_valid) return;

        _field = tokens[5];
        _operator = tokens[6];
        _value = tokens[7];

        // remove single quotes
        _value = _value.substr(1, _value.size() - 2);

        if (_operator == "LIKE")
        {
            _value = _value.substr(1, _value.size() - 2);
        }
    }

    bool StreamSql::valid() const
    {
        return _valid;
    }

    bool StreamSql::match(const nlohmann::json& msg)
    {
        if (!_valid) return false;

        if (msg.find(_field) == msg.end())
        {
            return false;
        }

        std::string value = msg[_field];
        return value == _value;
    }

} // namespace snake
