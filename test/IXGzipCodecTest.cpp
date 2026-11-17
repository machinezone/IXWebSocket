/*
 *  IXGzipCodecTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXGzipCodec.h>

TEST_CASE("Gzip decoding requires a complete stream", "[gzip]")
{
    const std::string body = GENERATE(std::string(), std::string("hello"), std::string(32768, 'x'));
    const std::string encoded = ix::gzipCompress(body);
    REQUIRE_FALSE(encoded.empty());
    for (size_t length = 0; length < encoded.size(); ++length)
    {
        CAPTURE(length, encoded.size());
        std::string decoded;
        CHECK_FALSE(ix::gzipDecompress(encoded.substr(0, length), decoded));
    }
    std::string decoded;
    REQUIRE(ix::gzipDecompress(encoded, decoded));
    CHECK(decoded == body);
}

TEST_CASE("Gzip decoding checks integrity and preserves append behavior", "[gzip]")
{
    const std::string body("hello\0world", 11);
    auto encoded = ix::gzipCompress(body);
    std::string decoded = "prefix";
    REQUIRE(ix::gzipDecompress(encoded, decoded));
    CHECK(decoded == "prefix" + body);
    encoded[encoded.size() - 8] ^= 1;
    decoded.clear();
    CHECK_FALSE(ix::gzipDecompress(encoded, decoded));
}
