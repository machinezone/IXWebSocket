/*
 * Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <iostream>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketPerMessageDeflateCodec.h>
#include <memory>
#include <string>

int main()
{
    ix::WebSocket websocket;
    // Heap allocations expose a wrong consumer layout under AddressSanitizer.
    std::unique_ptr<ix::WebSocketPerMessageDeflateCompressor> compressor(
        new ix::WebSocketPerMessageDeflateCompressor);
    std::unique_ptr<ix::WebSocketPerMessageDeflateDecompressor> decompressor(
        new ix::WebSocketPerMessageDeflateDecompressor);
    const bool compress = compressor->init(15, true);
    const bool decompress = decompressor->init(15, true);
    if (compress != bool(IXWEBSOCKET_CODEC_USE_ZLIB) || decompress != compress)
    {
        return 1;
    }
    if (compress)
    {
        const std::string input("installed consumer\0binary data", 30);
        std::string encoded, decoded;
        if (!compressor->compress(input, encoded) || !decompressor->decompress(encoded, decoded) ||
            decoded != input)
        {
            return 2;
        }
    }
    std::cout << sizeof(*compressor) << ' ' << sizeof(*decompressor) << '\n';
}
