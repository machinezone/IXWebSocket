/*
 *  IXStreamSqlTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <iostream>
#include <ixsnake/IXStreamSql.h>
#include <string.h>

using namespace ix;

namespace ix
{
    TEST_CASE("stream_sql", "[streamsql]")
    {
        SECTION("expression A")
        {
            snake::StreamSql streamSql("select * from subscriber_republished_v1_neo where session LIKE '%{self.session_id}%'");

            nlohmann::json msg = {
                {"action", "auth/authenticate/error"},
                {"id", 1},
                {"body", {{"error", "authentication_failed"}, {"reason", "invalid secret"}}}};

            REQUIRE(streamSql.match(msg));
        }
    }

} // namespace ix
