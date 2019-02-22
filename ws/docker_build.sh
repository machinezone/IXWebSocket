#!/bin/sh
#
# Author: Benjamin Sergeant
# Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
#

# 'manual' way of building. I cannot get CMake to work to build in a container.

g++ --std=c++14 \
    -DIXWEBSOCKET_USE_TLS \
    -g \
    ../ixwebsocket/IXEventFd.cpp \
    ../ixwebsocket/IXSocket.cpp \
    ../ixwebsocket/IXSocketServer.cpp \
    ../ixwebsocket/IXSocketConnect.cpp \
    ../ixwebsocket/IXDNSLookup.cpp \
    ../ixwebsocket/IXCancellationRequest.cpp \
    ../ixwebsocket/IXWebSocket.cpp \
    ../ixwebsocket/IXWebSocketServer.cpp \
    ../ixwebsocket/IXWebSocketTransport.cpp \
    ../ixwebsocket/IXWebSocketHandshake.cpp \
    ../ixwebsocket/IXWebSocketPerMessageDeflate.cpp \
    ../ixwebsocket/IXWebSocketPerMessageDeflateCodec.cpp \
    ../ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp \
    ../ixwebsocket/IXSocketOpenSSL.cpp \
    ../ixwebsocket/linux/IXSetThreadName_linux.cpp \
    ../third_party/jsoncpp/jsoncpp.cpp \
    ixcrypto/IXBase64.cpp \
    ixcrypto/IXHash.cpp \
    ixcrypto/IXUuid.cpp \
    ws_transfer.cpp \
    ws_send.cpp \
    ws_receive.cpp \
    ws.cpp \
    -I . \
    -I .. \
    -I ../third_party \
    -o ws \
    -lcrypto -lssl -lz -lpthread
