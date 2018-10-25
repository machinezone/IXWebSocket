#!/bin/sh

(cd build ; make)
./build/ping_pong ws://localhost:5678
