#!/bin/sh

test -d build || {
    mkdir -p build
    cd build
    cmake ..
}
(cd build ; make)
./build/ping_pong ws://localhost:5678
