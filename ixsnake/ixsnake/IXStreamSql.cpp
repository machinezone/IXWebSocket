/*
 *  IXStreamSql.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXStreamSql.h"

namespace snake
{
    StreamSql::StreamSql(const std::string& sqlFilter)
    {
        ;
    }

    bool StreamSql::match(const nlohmann::json& pdu)
    {
        return true;
    }

} // namespace snake
