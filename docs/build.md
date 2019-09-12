## Build

### CMake

CMakefiles for the library and the examples are available. This library has few dependencies, so it is possible to just add the source files into your project. Otherwise the usual way will suffice.

```
mkdir build # make a build dir so that you can build out of tree.
cd build
cmake -DUSE_TLS=1 ..
make -j
make install # will install to /usr/local on Unix, on macOS it is a good idea to sudo chown -R `whoami`:staff /usr/local
```

Headers and a static library will be installed to the target dir.
There is a unittest which can be executed by typing `make test`.

Options for building:

* `-DUSE_TLS=1` will enable TLS support
* `-DUSE_MBED_TLS=1` will use [mbedlts](https://tls.mbed.org/) for the TLS support (default on Windows)
* `-DUSE_WS=1` will build the ws interactive command line tool

If you are on Windows, look at the [appveyor](https://github.com/machinezone/IXWebSocket/blob/master/appveyor.yml) file that has instructions for building dependencies.

### vcpkg

It is possible to get IXWebSocket through Microsoft [vcpkg](https://github.com/microsoft/vcpkg).

```
vcpkg install ixwebsocket
```

### Conan

Support for building with conan was contributed by Olivia Zoe (thanks !). The package name to reference is `IXWebSocket/5.0.0@LunarWatcher/stable`. The package is in the process to be published to the official conan package repo, but in the meantime, it can be accessed by adding a new remote

```
conan remote add remote_name_here https://api.bintray.com/conan/oliviazoe0/conan-packages
```

### Docker

There is a Dockerfile for running the unittest on Linux, and to run the `ws` tool. It is also available on the docker registry.

```
docker run bsergean/ws
```

To use docker-compose you must make a docker container first.

```
$ make docker
...
$ docker compose up &
...
$ docker exec -it ixwebsocket_ws_1 bash
app@ca2340eb9106:~$ ws --help
ws is a websocket tool
...
```
