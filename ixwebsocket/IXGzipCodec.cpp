/*
 *  IXGzipCodec.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXGzipCodec.h"
#include "IXBench.h"

#include <array>
#include <string.h>

#ifdef IXWEBSOCKET_USE_ZLIB
#include <zlib.h>
#endif

#ifdef IXWEBSOCKET_USE_DEFLATE
#include <libdeflate.h>
#endif

namespace ix
{
#ifdef IXWEBSOCKET_USE_ZLIB
    std::string gzipCompress(const std::string& str)
    {
#ifdef IXWEBSOCKET_USE_DEFLATE
        int compressionLevel = 6;
        struct libdeflate_compressor *compressor;

        {
            Bench bench("creating compressor");
            compressor =
                libdeflate_alloc_compressor(compressionLevel);
        }

        const void *uncompressed_data = str.data();
        size_t uncompressed_size = str.size();
        void *compressed_data;
        size_t actual_compressed_size;
        size_t max_compressed_size;

        max_compressed_size = libdeflate_gzip_compress_bound(compressor,
                                                             uncompressed_size);
        {
            Bench bench("alloc data");
            compressed_data = malloc(max_compressed_size);
        }

        if (compressed_data == NULL)
        {
            return std::string();
        }

        {
            Bench bench("compressing data");
            actual_compressed_size = libdeflate_gzip_compress(
                compressor,
                uncompressed_data,
                uncompressed_size,
                compressed_data,
                max_compressed_size);
        }

        if (actual_compressed_size == 0)
        {
            free(compressed_data);
            return std::string();
        }

        libdeflate_free_compressor(compressor);

        std::string out;
        {
            Bench bench("append data");
            out.append(reinterpret_cast<char*>(compressed_data), actual_compressed_size);
        }

        {
            Bench bench("free data");
            free(compressed_data);
        }

        return out;
#else
        z_stream zs; // z_stream is zlib's control structure
        memset(&zs, 0, sizeof(zs));

        // deflateInit2 configure the file format: request gzip instead of deflate
        const int windowBits = 15;
        const int GZIP_ENCODING = 16;

        deflateInit2(&zs,
                     Z_DEFAULT_COMPRESSION,
                     Z_DEFLATED,
                     windowBits | GZIP_ENCODING,
                     8,
                     Z_DEFAULT_STRATEGY);

        zs.next_in = (Bytef*) str.data();
        zs.avail_in = (uInt) str.size(); // set the z_stream's input

        int ret;
        char outbuffer[32768];
        std::string outstring;

        // retrieve the compressed bytes blockwise
        do
        {
            zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
            zs.avail_out = sizeof(outbuffer);

            ret = deflate(&zs, Z_FINISH);

            if (outstring.size() < zs.total_out)
            {
                // append the block to the output string
                outstring.append(outbuffer, zs.total_out - outstring.size());
            }
        } while (ret == Z_OK);

        deflateEnd(&zs);

        return outstring;
#endif
    }

    bool gzipDecompress(const std::string& in, std::string& out)
    {
        z_stream inflateState;
        memset(&inflateState, 0, sizeof(inflateState));

        inflateState.zalloc = Z_NULL;
        inflateState.zfree = Z_NULL;
        inflateState.opaque = Z_NULL;
        inflateState.avail_in = 0;
        inflateState.next_in = Z_NULL;

        if (inflateInit2(&inflateState, 16 + MAX_WBITS) != Z_OK)
        {
            return false;
        }

        inflateState.avail_in = (uInt) in.size();
        inflateState.next_in = (unsigned char*) (const_cast<char*>(in.data()));

        const int kBufferSize = 1 << 14;
        std::array<unsigned char, kBufferSize> compressBuffer;

        do
        {
            inflateState.avail_out = (uInt) kBufferSize;
            inflateState.next_out = &compressBuffer.front();

            int ret = inflate(&inflateState, Z_SYNC_FLUSH);

            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            {
                inflateEnd(&inflateState);
                return false;
            }

            out.append(reinterpret_cast<char*>(&compressBuffer.front()),
                       kBufferSize - inflateState.avail_out);
        } while (inflateState.avail_out == 0);

        inflateEnd(&inflateState);
        return true;
    }
#endif
} // namespace ix
