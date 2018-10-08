# Building

1. cmake -G .
2. make

## Disable TLS

chat$ cmake -DUSE_TLS=OFF .
-- Configuring done
-- Generating done
-- Build files have been written to: /Users/bsergeant/src/foss/ixwebsocket/examples/chat
chat$ make
Scanning dependencies of target ixwebsocket
[ 16%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXSocket.cpp.o
[ 33%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXWebSocket.cpp.o
[ 50%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXWebSocketTransport.cpp.o
[ 66%] Linking CXX static library libixwebsocket.a
[ 66%] Built target ixwebsocket
[ 83%] Linking CXX executable cmd_websocket_chat
[100%] Built target cmd_websocket_chat

## Enable TLS (default)

```
chat$ cmake -DUSE_TLS=ON .
-- Configuring done
-- Generating done
-- Build files have been written to: /Users/bsergeant/src/foss/ixwebsocket/examples/chat
(venv) chat$ make
Scanning dependencies of target ixwebsocket
[ 14%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXSocket.cpp.o
[ 28%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXWebSocket.cpp.o
[ 42%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXWebSocketTransport.cpp.o
[ 57%] Building CXX object ixwebsocket/CMakeFiles/ixwebsocket.dir/ixwebsocket/IXSocketAppleSSL.cpp.o
[ 71%] Linking CXX static library libixwebsocket.a
[ 71%] Built target ixwebsocket
[ 85%] Linking CXX executable cmd_websocket_chat
[100%] Built target cmd_websocket_chat
```
