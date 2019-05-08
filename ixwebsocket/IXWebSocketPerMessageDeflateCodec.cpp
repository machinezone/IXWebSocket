/*
 *  IXWebSocketPerMessageDeflateCodec.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXWebSocketPerMessageDeflateCodec.h"
#include "IXWebSocketPerMessageDeflateOptions.h"

#include <cassert>
#include <string.h>

namespace
{
    // The passed in size (4) is important, without it the string litteral
    // is treated as a char* and the null termination (\x00) makes it
    // look like an empty string.
    const std::string kEmptyUncompressedBlock = std::string("\x00\x00\xff\xff", 4);

    const int kBufferSize = 1 << 14;
}

namespace ix
{
    //
    // Compressor
    //
    WebSocketPerMessageDeflateCompressor::WebSocketPerMessageDeflateCompressor()
      : _compressBufferSize(kBufferSize)
    {
        memset(&_deflateState, 0, sizeof(_deflateState));

        _deflateState.zalloc = Z_NULL;
        _deflateState.zfree = Z_NULL;
        _deflateState.opaque = Z_NULL;
    }

    WebSocketPerMessageDeflateCompressor::~WebSocketPerMessageDeflateCompressor()
    {
        deflateEnd(&_deflateState);
    }

    bool WebSocketPerMessageDeflateCompressor::init(uint8_t deflateBits,
                                                    bool clientNoContextTakeOver)
    {
        int ret = deflateInit2(
            &_deflateState,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -1*deflateBits,
            4, // memory level 1-9
            Z_DEFAULT_STRATEGY
        );

        if (ret != Z_OK) return false;

        _compressBuffer = std::make_unique<unsigned char[]>(_compressBufferSize);

        _flush = (clientNoContextTakeOver)
                 ? Z_FULL_FLUSH
                 : Z_SYNC_FLUSH;

        return true;
    }

    bool WebSocketPerMessageDeflateCompressor::endsWith(const std::string& value,
                                                        const std::string& ending)
    {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    bool WebSocketPerMessageDeflateCompressor::compress(const std::string& in,
                                                        std::string& out)
    {
        //
        // 7.2.1.  Compression
        //
        //    An endpoint uses the following algorithm to compress a message.
        //
        //    1.  Compress all the octets of the payload of the message using
        //        DEFLATE.
        //
        //    2.  If the resulting data does not end with an empty DEFLATE block
        //        with no compression (the "BTYPE" bits are set to 00), append an
        //        empty DEFLATE block with no compression to the tail end.
        //
        //    3.  Remove 4 octets (that are 0x00 0x00 0xff 0xff) from the tail end.
        //        After this step, the last octet of the compressed data contains
        //        (possibly part of) the DEFLATE header bits with the "BTYPE" bits
        //        set to 00.
        //
        size_t output;

        if (in.empty())
        {
            uint8_t buf[6] = {0x02, 0x00, 0x00, 0x00, 0xff, 0xff};
            out.append((char *)(buf), 6);
            return true;
        }

        _deflateState.avail_in = (uInt) in.size();
        _deflateState.next_in = (Bytef*) in.data();

        do
        {
            // Output to local buffer
            _deflateState.avail_out = (uInt) _compressBufferSize;
            _deflateState.next_out = _compressBuffer.get();

            deflate(&_deflateState, _flush);

            output = _compressBufferSize - _deflateState.avail_out;

            out.append((char *)(_compressBuffer.get()),output);
        } while (_deflateState.avail_out == 0);

        if (endsWith(out, kEmptyUncompressedBlock))
        {
            out.resize(out.size() - 4);
        }

        return true;
    }

    //
    // Decompressor
    //
    WebSocketPerMessageDeflateDecompressor::WebSocketPerMessageDeflateDecompressor()
      : _compressBufferSize(kBufferSize)
    {
        memset(&_inflateState, 0, sizeof(_inflateState));

        _inflateState.zalloc = Z_NULL;
        _inflateState.zfree = Z_NULL;
        _inflateState.opaque = Z_NULL;
        _inflateState.avail_in = 0;
        _inflateState.next_in = Z_NULL;
    }

    WebSocketPerMessageDeflateDecompressor::~WebSocketPerMessageDeflateDecompressor()
    {
        inflateEnd(&_inflateState);
    }

    bool WebSocketPerMessageDeflateDecompressor::init(uint8_t inflateBits,
                                                      bool clientNoContextTakeOver)
    {
        int ret = inflateInit2(
            &_inflateState,
            -1*inflateBits
        );

        if (ret != Z_OK) return false;

        _compressBuffer = std::make_unique<unsigned char[]>(_compressBufferSize);

        _flush = (clientNoContextTakeOver)
                 ? Z_FULL_FLUSH
                 : Z_SYNC_FLUSH;

        return true;
    }

    bool WebSocketPerMessageDeflateDecompressor::decompress(const std::string& in,
                                                            std::string& out)
    {
        //
        // 7.2.2.  Decompression
        //
        //    An endpoint uses the following algorithm to decompress a message.
        //
        //    1.  Append 4 octets of 0x00 0x00 0xff 0xff to the tail end of the
        //        payload of the message.
        //
        //    2.  Decompress the resulting data using DEFLATE.
        //
        std::string inFixed(in);
        inFixed += kEmptyUncompressedBlock;

        _inflateState.avail_in = (uInt) inFixed.size();
        _inflateState.next_in = (unsigned char *)(const_cast<char *>(inFixed.data()));

        do
        {
            _inflateState.avail_out = (uInt) _compressBufferSize;
            _inflateState.next_out = _compressBuffer.get();

            int ret = inflate(&_inflateState, Z_SYNC_FLUSH);

            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            {
                return false; // zlib error
            }

            out.append(
                reinterpret_cast<char *>(_compressBuffer.get()),
                _compressBufferSize - _inflateState.avail_out
            );
        } while (_inflateState.avail_out == 0);

        return true;
    }
}

