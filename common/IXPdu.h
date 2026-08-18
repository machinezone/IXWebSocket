/*
 *  IXPdu.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 *
 *  A minimal, dependency free serialization format, used by the ws utility to
 *  exchange structured messages (the chat, send, receive and transfer modes).
 *
 *  A pdu is a set of named fields, each one written on its own line as:
 *
 *      <name> <hex encoded value>
 *
 *  Names are plain ascii identifiers, values are hex encoded so that binary
 *  payloads (a file content) can be carried without any escaping. This is not
 *  compact (values take twice their size on the wire) but it is trivial to
 *  produce, to parse and to eyeball while debugging.
 *
 *  This format is not compatible with the msgpack based one used by
 *  IXWebSocket 12.0.1 and earlier.
 */

#pragma once

#include <map>
#include <string>
#include <vector>

namespace ix
{
    typedef std::map<std::string, std::string> Pdu;

    inline std::string hexEncode(const std::string& value)
    {
        static const char* digits = "0123456789abcdef";

        std::string out;
        out.reserve(value.size() * 2);

        for (auto&& c : value)
        {
            auto byte = static_cast<unsigned char>(c);
            out += digits[byte >> 4];
            out += digits[byte & 0x0f];
        }

        return out;
    }

    inline std::string hexEncode(const std::vector<uint8_t>& value)
    {
        return hexEncode(std::string(value.begin(), value.end()));
    }

    inline bool hexDecode(const std::string& value, std::string& out)
    {
        if (value.size() % 2 != 0) return false;

        out.clear();
        out.reserve(value.size() / 2);

        int nibbles[2];
        for (size_t i = 0; i < value.size(); i += 2)
        {
            for (size_t j = 0; j < 2; ++j)
            {
                char c = value[i + j];

                if (c >= '0' && c <= '9')
                {
                    nibbles[j] = c - '0';
                }
                else if (c >= 'a' && c <= 'f')
                {
                    nibbles[j] = c - 'a' + 10;
                }
                else if (c >= 'A' && c <= 'F')
                {
                    nibbles[j] = c - 'A' + 10;
                }
                else
                {
                    return false;
                }
            }

            out += static_cast<char>((nibbles[0] << 4) | nibbles[1]);
        }

        return true;
    }

    inline std::string serializePdu(const Pdu& pdu)
    {
        std::string out;

        for (auto&& it : pdu)
        {
            out += it.first;
            out += ' ';
            out += hexEncode(it.second);
            out += '\n';
        }

        return out;
    }

    inline bool parsePdu(const std::string& str, Pdu& pdu)
    {
        pdu.clear();

        std::string::size_type start = 0;
        while (start < str.size())
        {
            auto end = str.find('\n', start);
            if (end == std::string::npos) end = str.size();

            auto line = str.substr(start, end - start);
            start = end + 1;

            if (line.empty()) continue;

            auto sep = line.find(' ');
            if (sep == std::string::npos) return false;

            std::string value;
            if (!hexDecode(line.substr(sep + 1), value)) return false;

            pdu[line.substr(0, sep)] = value;
        }

        return true;
    }

    // Convenience accessor, returns an empty string for a missing field.
    inline std::string getPduField(const Pdu& pdu, const std::string& name)
    {
        auto it = pdu.find(name);
        return (it == pdu.end()) ? std::string() : it->second;
    }
} // namespace ix
