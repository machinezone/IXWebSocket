#!/bin/sh

rm -rf build
mkdir -p build 
cd build 
cmake -GNinja -DCMAKE_UNITY_BUILD=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=OFF .. 
ninja
