/*
 *  IXWebSocketPerMessageDeflateCodec.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "zlib.h"
#include <memory>
#include <string>

namespace ix
{
    class WebSocketPerMessageDeflateCompressor
    {
    public:
        WebSocketPerMessageDeflateCompressor();
        ~WebSocketPerMessageDeflateCompressor();

        bool init(uint8_t deflateBits, bool clientNoContextTakeOver);
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

        bool init(uint8_t inflateBits, bool clientNoContextTakeOver);
        bool decompress(const std::string& in, std::string& out);

    private:
        int _flush;
        size_t _compressBufferSize;
        std::unique_ptr<unsigned char[]> _compressBuffer;
        z_stream _inflateState;
    };

} // namespace ix
