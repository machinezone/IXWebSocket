#!/bin/sh
#
# Author: Benjamin Sergeant
# Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
#

# 'manual' way of building. You can also use cmake.

clang++ --std=c++11 --stdlib=libc++ \
    ../../ixwebsocket/IXSocket.cpp	\
    ../../ixwebsocket/IXWebSocketTransport.cpp \
    ../../ixwebsocket/IXSocketAppleSSL.cpp	\
    ../../ixwebsocket/IXWebSocket.cpp \
    cmd_websocket_chat.cpp \
    -o cmd_websocket_chat \
    -framework Security \
    -framework Foundation
