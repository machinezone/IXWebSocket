#!/bin/sh

mkdir build 
cd build
cmake .. 
make 
./ixwebsocket_unittest
