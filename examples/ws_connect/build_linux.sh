#!/bin/sh
#
# Author: Benjamin Sergeant
# Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
#

# 'manual' way of building. You can also use cmake.

g++ --std=c++11 \
    -DIXWEBSOCKET_USE_TLS \
    -g \
    ../../ixwebsocket/IXEventFd.cpp	\
    ../../ixwebsocket/IXSocket.cpp	\
    ../../ixwebsocket/IXWebSocketTransport.cpp \
    ../../ixwebsocket/IXWebSocket.cpp \
    ../../ixwebsocket/IXSocketOpenSSL.cpp \
    -I ../.. \
    ws_connect.cpp \
    -o ws_connect \
    -lcrypto -lssl -lpthread
