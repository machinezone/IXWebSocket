/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 * Static/Huffman tables are protocol constants from RFC 7541, Appendices A/B.
 * No external HPACK implementation is used at build time or runtime.
 */
/* RFC 7541 table data:
 * Copyright (c) 2015 IETF Trust and the persons identified as the document authors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "IXHpack.h"

#include <array>
#include <cstdint>
#include <limits>

namespace
{
    const std::pair<const char*, const char*> staticTable[] = {
        {":authority", ""},
        {":method", "GET"},
        {":method", "POST"},
        {":path", "/"},
        {":path", "/index.html"},
        {":scheme", "http"},
        {":scheme", "https"},
        {":status", "200"},
        {":status", "204"},
        {":status", "206"},
        {":status", "304"},
        {":status", "400"},
        {":status", "404"},
        {":status", "500"},
        {"accept-charset", ""},
        {"accept-encoding", "gzip, deflate"},
        {"accept-language", ""},
        {"accept-ranges", ""},
        {"accept", ""},
        {"access-control-allow-origin", ""},
        {"age", ""},
        {"allow", ""},
        {"authorization", ""},
        {"cache-control", ""},
        {"content-disposition", ""},
        {"content-encoding", ""},
        {"content-language", ""},
        {"content-length", ""},
        {"content-location", ""},
        {"content-range", ""},
        {"content-type", ""},
        {"cookie", ""},
        {"date", ""},
        {"etag", ""},
        {"expect", ""},
        {"expires", ""},
        {"from", ""},
        {"host", ""},
        {"if-match", ""},
        {"if-modified-since", ""},
        {"if-none-match", ""},
        {"if-range", ""},
        {"if-unmodified-since", ""},
        {"last-modified", ""},
        {"link", ""},
        {"location", ""},
        {"max-forwards", ""},
        {"proxy-authenticate", ""},
        {"proxy-authorization", ""},
        {"range", ""},
        {"referer", ""},
        {"refresh", ""},
        {"retry-after", ""},
        {"server", ""},
        {"set-cookie", ""},
        {"strict-transport-security", ""},
        {"transfer-encoding", ""},
        {"user-agent", ""},
        {"vary", ""},
        {"via", ""},
        {"www-authenticate", ""},
    };
    const uint32_t codes[] = {
        0x1ff8,     0x7fffd8,  0xfffffe2,  0xfffffe3, 0xfffffe4, 0xfffffe5,  0xfffffe6,  0xfffffe7,
        0xfffffe8,  0xffffea,  0x3ffffffc, 0xfffffe9, 0xfffffea, 0x3ffffffd, 0xfffffeb,  0xfffffec,
        0xfffffed,  0xfffffee, 0xfffffef,  0xffffff0, 0xffffff1, 0xffffff2,  0x3ffffffe, 0xffffff3,
        0xffffff4,  0xffffff5, 0xffffff6,  0xffffff7, 0xffffff8, 0xffffff9,  0xffffffa,  0xffffffb,
        0x14,       0x3f8,     0x3f9,      0xffa,     0x1ff9,    0x15,       0xf8,       0x7fa,
        0x3fa,      0x3fb,     0xf9,       0x7fb,     0xfa,      0x16,       0x17,       0x18,
        0x0,        0x1,       0x2,        0x19,      0x1a,      0x1b,       0x1c,       0x1d,
        0x1e,       0x1f,      0x5c,       0xfb,      0x7ffc,    0x20,       0xffb,      0x3fc,
        0x1ffa,     0x21,      0x5d,       0x5e,      0x5f,      0x60,       0x61,       0x62,
        0x63,       0x64,      0x65,       0x66,      0x67,      0x68,       0x69,       0x6a,
        0x6b,       0x6c,      0x6d,       0x6e,      0x6f,      0x70,       0x71,       0x72,
        0xfc,       0x73,      0xfd,       0x1ffb,    0x7fff0,   0x1ffc,     0x3ffc,     0x22,
        0x7ffd,     0x3,       0x23,       0x4,       0x24,      0x5,        0x25,       0x26,
        0x27,       0x6,       0x74,       0x75,      0x28,      0x29,       0x2a,       0x7,
        0x2b,       0x76,      0x2c,       0x8,       0x9,       0x2d,       0x77,       0x78,
        0x79,       0x7a,      0x7b,       0x7ffe,    0x7fc,     0x3ffd,     0x1ffd,     0xffffffc,
        0xfffe6,    0x3fffd2,  0xfffe7,    0xfffe8,   0x3fffd3,  0x3fffd4,   0x3fffd5,   0x7fffd9,
        0x3fffd6,   0x7fffda,  0x7fffdb,   0x7fffdc,  0x7fffdd,  0x7fffde,   0xffffeb,   0x7fffdf,
        0xffffec,   0xffffed,  0x3fffd7,   0x7fffe0,  0xffffee,  0x7fffe1,   0x7fffe2,   0x7fffe3,
        0x7fffe4,   0x1fffdc,  0x3fffd8,   0x7fffe5,  0x3fffd9,  0x7fffe6,   0x7fffe7,   0xffffef,
        0x3fffda,   0x1fffdd,  0xfffe9,    0x3fffdb,  0x3fffdc,  0x7fffe8,   0x7fffe9,   0x1fffde,
        0x7fffea,   0x3fffdd,  0x3fffde,   0xfffff0,  0x1fffdf,  0x3fffdf,   0x7fffeb,   0x7fffec,
        0x1fffe0,   0x1fffe1,  0x3fffe0,   0x1fffe2,  0x7fffed,  0x3fffe1,   0x7fffee,   0x7fffef,
        0xfffea,    0x3fffe2,  0x3fffe3,   0x3fffe4,  0x7ffff0,  0x3fffe5,   0x3fffe6,   0x7ffff1,
        0x3ffffe0,  0x3ffffe1, 0xfffeb,    0x7fff1,   0x3fffe7,  0x7ffff2,   0x3fffe8,   0x1ffffec,
        0x3ffffe2,  0x3ffffe3, 0x3ffffe4,  0x7ffffde, 0x7ffffdf, 0x3ffffe5,  0xfffff1,   0x1ffffed,
        0x7fff2,    0x1fffe3,  0x3ffffe6,  0x7ffffe0, 0x7ffffe1, 0x3ffffe7,  0x7ffffe2,  0xfffff2,
        0x1fffe4,   0x1fffe5,  0x3ffffe8,  0x3ffffe9, 0xffffffd, 0x7ffffe3,  0x7ffffe4,  0x7ffffe5,
        0xfffec,    0xfffff3,  0xfffed,    0x1fffe6,  0x3fffe9,  0x1fffe7,   0x1fffe8,   0x7ffff3,
        0x3fffea,   0x3fffeb,  0x1ffffee,  0x1ffffef, 0xfffff4,  0xfffff5,   0x3ffffea,  0x7ffff4,
        0x3ffffeb,  0x7ffffe6, 0x3ffffec,  0x3ffffed, 0x7ffffe7, 0x7ffffe8,  0x7ffffe9,  0x7ffffea,
        0x7ffffeb,  0xffffffe, 0x7ffffec,  0x7ffffed, 0x7ffffee, 0x7ffffef,  0x7fffff0,  0x3ffffee,
        0x3fffffff,
    };
    const unsigned char codeLengths[] = {
        13, 23, 28, 28, 28, 28, 28, 28, 28, 24, 30, 28, 28, 30, 28, 28, 28, 28, 28, 28, 28, 28,
        30, 28, 28, 28, 28, 28, 28, 28, 28, 28, 6,  10, 10, 12, 13, 6,  8,  11, 10, 10, 8,  11,
        8,  6,  6,  6,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  7,  8,  15, 6,  12, 10, 13, 6,
        7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
        8,  7,  8,  13, 19, 13, 14, 6,  15, 5,  6,  5,  6,  5,  6,  6,  6,  5,  7,  7,  6,  6,
        6,  5,  6,  7,  6,  5,  5,  6,  7,  7,  7,  7,  7,  15, 11, 14, 13, 28, 20, 22, 20, 20,
        22, 22, 22, 23, 22, 23, 23, 23, 23, 23, 24, 23, 24, 24, 22, 23, 24, 23, 23, 23, 23, 21,
        22, 23, 22, 23, 23, 24, 22, 21, 20, 22, 22, 23, 23, 21, 23, 22, 22, 24, 21, 22, 23, 23,
        21, 21, 22, 21, 23, 22, 23, 23, 20, 22, 22, 22, 23, 22, 22, 23, 26, 26, 20, 19, 22, 23,
        22, 25, 26, 26, 26, 27, 27, 26, 24, 25, 19, 21, 26, 27, 27, 26, 27, 24, 21, 21, 26, 26,
        28, 27, 27, 27, 20, 24, 20, 21, 22, 21, 21, 23, 22, 22, 25, 25, 24, 24, 26, 23, 26, 27,
        26, 26, 27, 27, 27, 27, 27, 28, 27, 27, 27, 27, 27, 26, 30,
    };
    struct HuffmanTree
    {
        struct Node
        {
            int child[2] = {-1, -1};
            int symbol = -1;
        };
        std::array<Node, 513> nodes;
        HuffmanTree()
        {
            int count = 1;
            for (int symbol = 0; symbol < 257; ++symbol)
            {
                int node = 0;
                for (int bit = codeLengths[symbol] - 1; bit >= 0; --bit)
                {
                    const int edge = (codes[symbol] >> bit) & 1;
                    if (nodes[node].child[edge] == -1)
                    {
                        nodes[node].child[edge] = count++;
                    }
                    node = nodes[node].child[edge];
                }
                nodes[node].symbol = symbol;
            }
        }
    };
    bool integer(const std::string& data, size_t& offset, unsigned prefix, size_t& value)
    {
        if (offset == data.size())
        {
            return false;
        }
        const unsigned mask = (1u << prefix) - 1;
        value = static_cast<unsigned char>(data[offset++]) & mask;
        if (value != mask)
        {
            return true;
        }
        unsigned shift = 0;
        while (offset < data.size() && shift < sizeof(size_t) * 8)
        {
            const unsigned byte = static_cast<unsigned char>(data[offset++]);
            if ((byte & 127) > ((std::numeric_limits<size_t>::max)() - value) >> shift)
            {
                return false;
            }
            value += size_t(byte & 127) << shift;
            if (!(byte & 128))
            {
                return true;
            }
            shift += 7;
        }
        return false;
    }
    void integer(std::string& data, size_t value, unsigned prefix, unsigned flags)
    {
        const unsigned mask = (1u << prefix) - 1;
        if (value < mask)
        {
            data += char(flags | value);
            return;
        }
        data += char(flags | mask);
        value -= mask;
        while (value >= 128)
        {
            data += char((value & 127) | 128);
            value >>= 7;
        }
        data += char(value);
    }
    bool string(const std::string& data, size_t& offset, std::string& value)
    {
        if (offset == data.size())
        {
            return false;
        }
        const bool huffman = (static_cast<unsigned char>(data[offset]) & 128) != 0;
        size_t length;
        if (!integer(data, offset, 7, length) || length > data.size() - offset)
        {
            return false;
        }
        value.clear();
        if (!huffman)
        {
            value.assign(data, offset, length);
            offset += length;
            return true;
        }
        static const HuffmanTree tree;
        int node = 0;
        unsigned trailing = 0;
        bool ones = true;
        const size_t end = offset + length;
        while (offset < end)
        {
            const unsigned byte = static_cast<unsigned char>(data[offset++]);
            for (int bit = 7; bit >= 0; --bit)
            {
                const unsigned edge = (byte >> bit) & 1;
                node = tree.nodes[node].child[edge];
                if (node < 0)
                {
                    return false;
                }
                ++trailing;
                ones = ones && edge;
                const int symbol = tree.nodes[node].symbol;
                if (symbol >= 0)
                {
                    if (symbol == 256 || value.size() == 65536)
                    {
                        return false;
                    }
                    value += char(symbol);
                    node = 0;
                    trailing = 0;
                    ones = true;
                }
            }
        }
        return node == 0 || (trailing <= 7 && ones);
    }
} // namespace

namespace ix
{
    std::string Hpack::encode(const Headers& headers)
    {
        std::string block;
        for (const auto& field : headers)
        {
            block += char(0x10); // Never indexed, new name.
            integer(block, field.first.size(), 7, 0);
            block += field.first;
            integer(block, field.second.size(), 7, 0);
            block += field.second;
        }
        return block;
    }
    Hpack::Headers::value_type Hpack::indexed(size_t index) const
    {
        if (index && index <= 61)
        {
            return staticTable[index - 1];
        }
        if (index > 61 && index - 62 < _dynamic.size())
        {
            return _dynamic[index - 62];
        }
        return {};
    }
    void Hpack::evict()
    {
        while (_size > _capacity)
        {
            const auto& field = _dynamic.back();
            _size -= 32 + field.first.size() + field.second.size();
            _dynamic.pop_back();
        }
    }
    bool Hpack::decode(const std::string& block, Headers& headers)
    {
        headers.clear();
        if (block.size() > 65536)
        {
            return false;
        }
        size_t offset = 0, total = 0;
        bool fields = false;
        while (offset < block.size())
        {
            const unsigned byte = static_cast<unsigned char>(block[offset]);
            size_t index;
            if ((byte & 0xe0) == 0x20)
            {
                if (fields || !integer(block, offset, 5, index) || index > 4096)
                {
                    return false;
                }
                _capacity = index;
                evict();
                continue;
            }
            fields = true;
            const bool full = (byte & 128) != 0;
            const bool insert = !full && (byte & 64);
            if (!integer(block, offset, full ? 7 : (insert ? 6 : 4), index))
            {
                return false;
            }
            auto field = indexed(index);
            if (full)
            {
                if (field.first.empty())
                {
                    return false;
                }
            }
            else
            {
                if (index && field.first.empty())
                {
                    return false;
                }
                if (!index && !string(block, offset, field.first))
                {
                    return false;
                }
                if (!string(block, offset, field.second))
                {
                    return false;
                }
            }
            total += 32 + field.first.size() + field.second.size();
            if (field.first.empty() || total > 65536)
            {
                return false;
            }
            if (insert)
            {
                _size += 32 + field.first.size() + field.second.size();
                _dynamic.push_front(field);
                evict();
            }
            headers.push_back(std::move(field));
        }
        return true;
    }
} // namespace ix
