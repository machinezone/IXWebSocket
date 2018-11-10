/*
 *  IXWebSocketPerMessageDeflate.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 *
 *  Per message Deflate RFC: https://tools.ietf.org/html/rfc7692
 *
 *  Chrome websocket -> https://github.com/chromium/chromium/tree/2ca8c5037021c9d2ecc00b787d58a31ed8fc8bcb/net/websockets
 */

#include "IXWebSocketPerMessageDeflate.h"
#include "IXWebSocketPerMessageDeflateOptions.h"

#include <iostream>
#include <cassert>

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
        _deflateState.zalloc = Z_NULL;
        _deflateState.zfree = Z_NULL;
        _deflateState.opaque = Z_NULL;
    }

    WebSocketPerMessageDeflateCompressor::~WebSocketPerMessageDeflateCompressor()
    {
        deflateEnd(&_deflateState);
    }

    bool WebSocketPerMessageDeflateCompressor::init(uint8_t deflateBits,
                                                    bool client_no_context_takeover)
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

        _compressBuffer.reset(new unsigned char[_compressBufferSize]);
        if (client_no_context_takeover)
        {
            _flush = Z_FULL_FLUSH;
        }
        else
        {
            _flush = Z_SYNC_FLUSH;
        }

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
                                                      bool client_no_context_takeover)
    {
        int ret = inflateInit2(
            &_inflateState,
            -1*inflateBits
        );

        if (ret != Z_OK) return false;

        _compressBuffer.reset(new unsigned char[_compressBufferSize]);
        if (client_no_context_takeover)
        {
            _flush = Z_FULL_FLUSH;
        }
        else
        {
            _flush = Z_SYNC_FLUSH;
        }

        return true;
    }

    bool WebSocketPerMessageDeflateDecompressor::decompress(const std::string& in,
                                                            std::string& out)
    {
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

    WebSocketPerMessageDeflate::WebSocketPerMessageDeflate()
    {
        _compressor.reset(new WebSocketPerMessageDeflateCompressor());
        _decompressor.reset(new WebSocketPerMessageDeflateDecompressor());
    }

    WebSocketPerMessageDeflate::~WebSocketPerMessageDeflate()
    {
        _compressor.reset();
        _decompressor.reset();
    }

    bool WebSocketPerMessageDeflate::init(const WebSocketPerMessageDeflateOptions& perMessageDeflateOptions)
    {
        bool clientNoContextTakeover = 
            perMessageDeflateOptions.getClientNoContextTakeover();

        uint8_t deflateBits = perMessageDeflateOptions.getClientMaxWindowBits();
        uint8_t inflateBits = perMessageDeflateOptions.getServerMaxWindowBits();

        return _compressor->init(deflateBits, clientNoContextTakeover) && 
               _decompressor->init(inflateBits, clientNoContextTakeover);
    }

    bool WebSocketPerMessageDeflate::compress(const std::string& in,
                                              std::string& out)
    {
        return _compressor->compress(in, out);
    }

    bool WebSocketPerMessageDeflate::decompress(const std::string& in,
                                                std::string &out)
    {
        return _decompressor->decompress(in, out);
    }

}
