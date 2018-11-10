/*
 *  IXWebSocketPerMessageDeflate.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "zlib.h"
#include <string>

namespace ix 
{
    class WebSocketPerMessageDeflateOptions;

    class WebSocketPerMessageDeflateCompressor
    {
    public:
        WebSocketPerMessageDeflateCompressor();
        ~WebSocketPerMessageDeflateCompressor();
        bool init(uint8_t deflate_bits, bool client_no_context_takeover);
        bool compress(const std::string& in, std::string& out);

    private:
        static bool endsWith(const std::string& value, const std::string& ending);

        int _flush;
        size_t _compressBufferSize;
        std::unique_ptr<unsigned char[]> _compressBuffer;

        z_stream _deflateState;
    };

    class WebSocketPerMessageDeflateDecompressor
    {
    public:
        WebSocketPerMessageDeflateDecompressor();
        ~WebSocketPerMessageDeflateDecompressor();
        bool init(uint8_t inflate_bits, bool client_no_context_takeover);
        bool decompress(const std::string& in, std::string& out);

    private:
        int _flush;
        size_t _compressBufferSize;
        std::unique_ptr<unsigned char[]> _compressBuffer;

        z_stream _inflateState;
    };

    class WebSocketPerMessageDeflate
    {
    public:
        WebSocketPerMessageDeflate();
        virtual ~WebSocketPerMessageDeflate();

        bool init(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions);

        bool compress(const std::string& in, std::string& out);
        bool decompress(const std::string& in, std::string& out);

    private:
        // mode::value m_server_max_window_bits_mode;
        // mode::value m_client_max_window_bits_mode;

        std::shared_ptr<WebSocketPerMessageDeflateCompressor> _compressor;
        std::shared_ptr<WebSocketPerMessageDeflateDecompressor> _decompressor;
    };
}
