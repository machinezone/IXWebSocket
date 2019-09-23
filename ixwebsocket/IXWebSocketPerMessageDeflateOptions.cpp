/*
 *  IXWebSocketPerMessageDeflateOptions.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketPerMessageDeflateOptions.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ix
{
    /// Default values as defined in the RFC
    const uint8_t WebSocketPerMessageDeflateOptions::kDefaultServerMaxWindowBits = 15;
    static const int minServerMaxWindowBits = 8;
    static const int maxServerMaxWindowBits = 15;

    const uint8_t WebSocketPerMessageDeflateOptions::kDefaultClientMaxWindowBits = 15;
    static const int minClientMaxWindowBits = 8;
    static const int maxClientMaxWindowBits = 15;

    WebSocketPerMessageDeflateOptions::WebSocketPerMessageDeflateOptions(
        bool enabled,
        bool clientNoContextTakeover,
        bool serverNoContextTakeover,
        uint8_t clientMaxWindowBits,
        uint8_t serverMaxWindowBits)
    {
        _enabled = enabled;
        _clientNoContextTakeover = clientNoContextTakeover;
        _serverNoContextTakeover = serverNoContextTakeover;
        _clientMaxWindowBits = clientMaxWindowBits;
        _serverMaxWindowBits = serverMaxWindowBits;

        sanitizeClientMaxWindowBits();
    }

    //
    // Four extension parameters are defined for "permessage-deflate" to
    // help endpoints manage per-connection resource usage.
    //
    // - "server_no_context_takeover"
    // - "client_no_context_takeover"
    // - "server_max_window_bits"
    // - "client_max_window_bits"
    //
    // Server response could look like that:
    //
    // Sec-WebSocket-Extensions: permessage-deflate; client_no_context_takeover;
    // server_no_context_takeover
    //
    WebSocketPerMessageDeflateOptions::WebSocketPerMessageDeflateOptions(std::string extension)
    {
        extension = removeSpaces(extension);

        _enabled = false;
        _clientNoContextTakeover = false;
        _serverNoContextTakeover = false;
        _clientMaxWindowBits = kDefaultClientMaxWindowBits;
        _serverMaxWindowBits = kDefaultServerMaxWindowBits;

        // Split by ;
        std::string token;
        std::stringstream tokenStream(extension);

        while (std::getline(tokenStream, token, ';'))
        {
            if (token == "permessage-deflate")
            {
                _enabled = true;
            }

            if (token == "server_no_context_takeover")
            {
                _serverNoContextTakeover = true;
            }

            if (token == "client_no_context_takeover")
            {
                _clientNoContextTakeover = true;
            }

            if (startsWith(token, "server_max_window_bits="))
            {
                std::string val = token.substr(token.find_last_of("=") + 1);
                std::stringstream ss;
                ss << val;
                int x;
                ss >> x;

                // Sanitize values to be in the proper range [8, 15] in
                // case a server would give us bogus values
                _serverMaxWindowBits =
                    std::min(maxServerMaxWindowBits, std::max(x, minServerMaxWindowBits));
            }

            if (startsWith(token, "client_max_window_bits="))
            {
                std::string val = token.substr(token.find_last_of("=") + 1);
                std::stringstream ss;
                ss << val;
                int x;
                ss >> x;

                // Sanitize values to be in the proper range [8, 15] in
                // case a server would give us bogus values
                _clientMaxWindowBits =
                    std::min(maxClientMaxWindowBits, std::max(x, minClientMaxWindowBits));

                sanitizeClientMaxWindowBits();
            }
        }
    }

    void WebSocketPerMessageDeflateOptions::sanitizeClientMaxWindowBits()
    {
        // zlib/deflate has a bug with windowsbits == 8, so we silently upgrade it to 9
        // See https://bugs.chromium.org/p/chromium/issues/detail?id=691074
        if (_clientMaxWindowBits == 8)
        {
            _clientMaxWindowBits = 9;
        }
    }

    std::string WebSocketPerMessageDeflateOptions::generateHeader()
    {
        std::stringstream ss;
        ss << "Sec-WebSocket-Extensions: permessage-deflate";

        if (_clientNoContextTakeover) ss << "; client_no_context_takeover";
        if (_serverNoContextTakeover) ss << "; server_no_context_takeover";

        ss << "; server_max_window_bits=" << _serverMaxWindowBits;
        ss << "; client_max_window_bits=" << _clientMaxWindowBits;

        ss << "\r\n";

        return ss.str();
    }

    bool WebSocketPerMessageDeflateOptions::enabled() const
    {
        return _enabled;
    }

    bool WebSocketPerMessageDeflateOptions::getClientNoContextTakeover() const
    {
        return _clientNoContextTakeover;
    }

    bool WebSocketPerMessageDeflateOptions::getServerNoContextTakeover() const
    {
        return _serverNoContextTakeover;
    }

    uint8_t WebSocketPerMessageDeflateOptions::getClientMaxWindowBits() const
    {
        return _clientMaxWindowBits;
    }

    uint8_t WebSocketPerMessageDeflateOptions::getServerMaxWindowBits() const
    {
        return _serverMaxWindowBits;
    }

    bool WebSocketPerMessageDeflateOptions::startsWith(const std::string& str,
                                                       const std::string& start)
    {
        return str.compare(0, start.length(), start) == 0;
    }

    std::string WebSocketPerMessageDeflateOptions::removeSpaces(const std::string& str)
    {
        std::string out(str);
        out.erase(
            std::remove_if(out.begin(), out.end(), [](unsigned char x) { return std::isspace(x); }),
            out.end());

        return out;
    }
} // namespace ix
