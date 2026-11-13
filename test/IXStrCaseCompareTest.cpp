/*
 *  IXStrCaseCompareTest.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone. All rights reserved.
 */

#include <catch_amalgamated.hpp>
#include <clocale>
#include <ixwebsocket/IXStrCaseCompare.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <map>

namespace
{
    class ScopedCTypeLocale
    {
    public:
        ScopedCTypeLocale()
            : _original(std::setlocale(LC_CTYPE, nullptr))
        {
        }

        ~ScopedCTypeLocale()
        {
            std::setlocale(LC_CTYPE, _original.c_str());
        }

    private:
        std::string _original;
    };
} // namespace

using namespace ix;

namespace ix
{
    TEST_CASE("str_case_compare", "[str_case_compare]")
    {
        SECTION("1")
        {
            using HttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;

            HttpHeaders httpHeaders;

            httpHeaders["foo"] = "foo";

            REQUIRE(httpHeaders["foo"] == "foo");
            REQUIRE(httpHeaders["missing"] == "");

            // Comparison should be case insensitive
            REQUIRE(httpHeaders["Foo"] == "foo");
            REQUIRE(httpHeaders["Foo"] != "bar");
        }

        SECTION("2")
        {
            using HttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;

            HttpHeaders headers;

            headers["Upgrade"] = "webSocket";

            REQUIRE(!CaseInsensitiveLess::cmp(headers["upGRADE"], "webSocket"));
        }
    }

    TEST_CASE("Case-insensitive ordering folds only ASCII letters", "[str_case_compare]")
    {
        const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const std::string lower = "abcdefghijklmnopqrstuvwxyz";
        CHECK_FALSE(CaseInsensitiveLess::cmp(upper, lower));
        CHECK_FALSE(CaseInsensitiveLess::cmp(lower, upper));
        CHECK(CaseInsensitiveLess::cmp("", "a"));
        CHECK(CaseInsensitiveLess::cmp("prefix", "PREFIX-more"));
        CHECK_FALSE(CaseInsensitiveLess::cmp("PREFIX-more", "prefix"));
        // Adjacent punctuation must not be folded by applying a bit mask.
        CHECK(CaseInsensitiveLess::cmp("@", "`"));
        CHECK(CaseInsensitiveLess::cmp("[", "{"));
        CHECK(CaseInsensitiveLess::cmp("\\", "|"));
        CHECK(CaseInsensitiveLess::cmp("]", "}"));
        CHECK(CaseInsensitiveLess::cmp("^", "~"));
        // Bytes outside ASCII retain their unsigned ordering, including embedded NULs.
        CHECK(CaseInsensitiveLess::cmp(std::string("A\0B", 3), std::string("a\0c", 3)));
        CHECK(CaseInsensitiveLess::cmp("\x7f", "\x80"));
        CHECK(CaseInsensitiveLess::cmp("\xc4", "\xe4"));
        CHECK(CaseInsensitiveLess::cmp("\xfe", "\xff"));
    }

    TEST_CASE("Comparisons and header lookup ignore the C locale", "[str_case_compare][locale]")
    {
        ScopedCTypeLocale restore;
        REQUIRE(std::setlocale(LC_CTYPE, "C") != nullptr);
        WebSocketHttpHeaders headers = {
            {"Origin", "old"}, {"Connection", "Upgrade"}, {"X-ID", "1"}};
        bool available = false;
        for (auto name :
             {"tr_TR.ISO8859-9", "tr_TR.iso88599", "tr_TR.UTF-8", "Turkish_Turkey.1254", "tr-TR"})
        {
            if (std::setlocale(LC_CTYPE, name))
            {
                available = true;
                break;
            }
        }
        if (!available) SKIP("Turkish locale is not installed");
        CAPTURE(std::setlocale(LC_CTYPE, nullptr));

        CHECK(headers.find("ORIGIN") != headers.end());
        CHECK(headers.find("CONNECTION") != headers.end());
        CHECK(headers.find("x-id") != headers.end());
        headers["ORIGIN"] = "new";
        CHECK(headers.size() == 3);
        CHECK(headers.at("Origin") == "new");
        CHECK(CaseInsensitiveLess::cmp("\xc4", "\xe4"));
    }

} // namespace ix
