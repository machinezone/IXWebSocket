/*
 *  IXWebSocketPerMessageDeflateCodecTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone. All rights reserved.
 *
 *  make build_test && build/test/ixwebsocket_unittest per-message-deflate-codec
 */

#include "IXTest.h"
#include <catch_amalgamated.hpp>
#include <iostream>
#include <ixwebsocket/IXWebSocketPerMessageDeflate.h>
#include <ixwebsocket/IXWebSocketPerMessageDeflateCodec.h>
#include <ixwebsocket/IXWebSocketPerMessageDeflateOptions.h>
#include <string.h>

using namespace ix;

namespace ix
{
    std::string compressAndDecompress(const std::string& a)
    {
        std::string b, c;

        WebSocketPerMessageDeflateCompressor compressor;
        compressor.init(11, true);
        compressor.compress(a, b);

        WebSocketPerMessageDeflateDecompressor decompressor;
        decompressor.init(11, true);
        decompressor.decompress(b, c);

        return c;
    }

    std::string compressAndDecompressVector(const std::string& a)
    {
        std::string b, c;

        std::vector<uint8_t> vec(a.begin(), a.end());

        WebSocketPerMessageDeflateCompressor compressor;
        compressor.init(11, true);
        compressor.compress(vec, b);

        WebSocketPerMessageDeflateDecompressor decompressor;
        decompressor.init(11, true);
        decompressor.decompress(b, c);

        return c;
    }

    TEST_CASE("per-message-deflate-codec", "[zlib]")
    {
        SECTION("string api")
        {
            REQUIRE(compressAndDecompress("") == "");
            REQUIRE(compressAndDecompress("foo") == "foo");
            REQUIRE(compressAndDecompress("bar") == "bar");
            REQUIRE(compressAndDecompress("asdcaseqw`21897dehqwed") == "asdcaseqw`21897dehqwed");
            REQUIRE(compressAndDecompress("/usr/local/include/ixwebsocket/IXSocketAppleSSL.h") ==
                    "/usr/local/include/ixwebsocket/IXSocketAppleSSL.h");
        }

        SECTION("vector api")
        {
            REQUIRE(compressAndDecompressVector("") == "");
            REQUIRE(compressAndDecompressVector("foo") == "foo");
            REQUIRE(compressAndDecompressVector("bar") == "bar");
            REQUIRE(compressAndDecompressVector("asdcaseqw`21897dehqwed") ==
                    "asdcaseqw`21897dehqwed");
            REQUIRE(
                compressAndDecompressVector("/usr/local/include/ixwebsocket/IXSocketAppleSSL.h") ==
                "/usr/local/include/ixwebsocket/IXSocketAppleSSL.h");
        }
    }

} // namespace ix

TEST_CASE("Compression parameters follow the sending endpoint", "[zlib]")
{
    const bool serverNoContext = GENERATE(false, true);
    const bool clientNoContext = GENERATE(false, true);
    ix::WebSocketPerMessageDeflateOptions options(true, clientNoContext, serverNoContext, 15, 9);
    ix::WebSocketPerMessageDeflate client, server;
    REQUIRE(client.init(options));
    REQUIRE(server.init(options, true));

    // Repeats beyond the smaller window expose swapped inflation/compression parameters.
    uint32_t random = 1;
    std::string block;
    for (size_t i = 0; i < 4096; ++i)
    {
        random = random * 1664525 + 1013904223;
        block.push_back(static_cast<char>(random >> 24));
    }
    std::string payload;
    for (unsigned i = 0; i < 32; ++i)
        payload += block;
    for (unsigned i = 0; i < 3; ++i)
    {
        std::string compressed, decoded;
        REQUIRE(client.compress(payload, compressed));
        REQUIRE(server.decompress(compressed, decoded));
        REQUIRE(decoded == payload);
        REQUIRE(server.compress(payload, compressed));
        REQUIRE(client.decompress(compressed, decoded));
        REQUIRE(decoded == payload);
        if (serverNoContext)
        {
            ix::WebSocketPerMessageDeflateDecompressor fresh;
            REQUIRE(fresh.init(9, false));
            REQUIRE(fresh.decompress(compressed, decoded));
            REQUIRE(decoded == payload);
        }
    }
}
