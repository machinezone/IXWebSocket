#!/bin/sh
#
# Author: Benjamin Sergeant
# Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
#

# 'manual' way of building. You can also use cmake.

g++ --std=c++11 \
    ../../ixwebsocket/IXSocket.cpp	\
    ../../ixwebsocket/IXWebSocketTransport.cpp \
    ../../ixwebsocket/IXWebSocket.cpp \
    -I ../.. \
    cmd_websocket_chat.cpp \
    -o cmd_websocket_chat
