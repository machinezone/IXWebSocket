/*
 *  IXSentryClientTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <iostream>
#include <string.h>

using namespace ix;

namespace ix
{
    TEST_CASE("sentry", "[sentry]")
    {
        SECTION("Attempt to perform nil")
        {
#if 0
            SentryClient sentryClient;
            std::string stack = "";
            auto frames = sentryClient.parseLuaStackTrace(stack);

            REQUIRE(frames.size() > 0);
#endif
        }
    }

} // namespace ix
