/*
 *  IXWebSocketPerMessageDeflateCodec.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include "zlib.h"
#include <memory>
#include <string>
#include <vector>

namespace ix
{
    class WebSocketPerMessageDeflateCompressor
    {
    public:
        WebSocketPerMessageDeflateCompressor();
        ~WebSocketPerMessageDeflateCompressor();

        bool init(uint8_t deflateBits, bool clientNoContextTakeOver);
        bool compress(const std::string& in, std::string& out);
        bool compress(const std::string& in, std::vector<uint8_t>& out);
        bool compress(const std::vector<uint8_t>& in, std::string& out);
        bool compress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

    private:
        template<typename T, typename S>
        bool compressData(const T& in, S& out);
        template<typename T>
        bool endsWithEmptyUnCompressedBlock(const T& value);

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
