/*
 *  IXSocketTest.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXTest.h"
#include "catch.hpp"
#include <iostream>
#include <ixwebsocket/IXWebSocketPerMessageDeflateCodec.h>
#include <string.h>

using namespace ix;

namespace ix
{
    TEST_CASE("per-message-deflate-codec", "[zlib]")
    {
        SECTION("http://google.com")
        {
            std::string a = "foobarbaz";
            std::string b, c;

            WebSocketPerMessageDeflateCompressor compressor;
            compressor.compress(a, b);

            WebSocketPerMessageDeflateDecompressor decompressor;
            decompressor.decompress(b, c);

            REQUIRE(a != b);
            REQUIRE(b != c);
            REQUIRE(a == c);
        }
    }

} // namespace ix
