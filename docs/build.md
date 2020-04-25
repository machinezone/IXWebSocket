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
* `-DUSE_OPEN_SSL=1` will use [openssl](https://www.openssl.org/) for the TLS support (default on Linux and Windows)
* `-DUSE_MBED_TLS=1` will use [mbedlts](https://tls.mbed.org/) for the TLS support
* `-DUSE_WS=1` will build the ws interactive command line tool
* `-DUSE_TEST=1` will build the unittest

If you are on Windows, look at the [appveyor](https://github.com/machinezone/IXWebSocket/blob/master/appveyor.yml) file (not maintained much though) or rather the [github actions](https://github.com/machinezone/IXWebSocket/blob/master/.github/workflows/ccpp.yml#L40) which have instructions for building dependencies.

It is also possible to externally include the project, so that everything is fetched over the wire when you build like so:

```
    ExternalProject_Add(
        IXWebSocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        ...
    )
```

### vcpkg

It is possible to get IXWebSocket through Microsoft [vcpkg](https://github.com/microsoft/vcpkg).

```
vcpkg install ixwebsocket
```

### Conan

[ ![Download](https://api.bintray.com/packages/conan/conan-center/ixwebsocket%3A_/images/download.svg) ](https://bintray.com/conan/conan-center/ixwebsocket%3A_/_latestVersion)

Conan is currently supported through a recipe in [Conan Center](https://github.com/conan-io/conan-center-index/tree/master/recipes/ixwebsocket) ([Bintray entry](https://bintray.com/conan/conan-center/ixwebsocket%3A_)).

Package reference

* Conan 1.21.0 and up: `ixwebsocket/7.9.2`
* Earlier versions: `ixwebsocket/7.9.2@_/_`

Note that the version listed here might not be the latest one. See Bintray or the recipe itself for the latest version. If you're migrating from the previous, custom Bintray remote, note that the package reference _has_ to be lower-case.

### Docker

There is a Dockerfile for running the unittest on Linux, and to run the `ws` tool. It is also available on the docker registry.

```
docker run docker.pkg.github.com/machinezone/ixwebsocket/ws:latest --help
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
