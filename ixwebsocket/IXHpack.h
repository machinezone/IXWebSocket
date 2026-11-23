/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#pragma once
#include <cstddef>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace ix
{
    // RFC 7541. Each decoder belongs to one HTTP/2 connection.
    class Hpack
    {
    public:
        using Headers = std::vector<std::pair<std::string, std::string>>;
        // Literal, never-indexed request fields avoid retaining credentials.
        static std::string encode(const Headers& headers);
        bool decode(const std::string& block, Headers& headers);

    private:
        Headers::value_type indexed(size_t index) const;
        void evict();
        std::deque<Headers::value_type> _dynamic;
        size_t _capacity = 4096;
        size_t _size = 0;
    };
} // namespace ix
