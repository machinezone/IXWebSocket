#
# This makefile is used for convenience, and wrap simple cmake commands
# You don't need to use it as an end user, it is more for developer.
#
# * work with docker (linux build)
# * execute the unittest
#
# The default target will install ws, the command line tool coming with
# IXWebSocket into /usr/local/bin
#
#
all: brew

install: brew

# Use -DCMAKE_INSTALL_PREFIX= to install into another location
# on osx it is good practice to make /usr/local user writable
# sudo chown -R `whoami`/staff /usr/local
#
# Release, Debug, MinSizeRel, RelWithDebInfo are the build types
#
brew:
	mkdir -p build && (cd build ; cmake -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 .. ; ninja install)

# Docker default target. We've add problem with OpenSSL and TLS 1.3 (on the
# server side ?) and I can't work-around it easily, so we're using mbedtls on
# Linux for the SSL backend, which works great.
ws_mbedtls_install:
	mkdir -p build && (cd build ; cmake -GNinja -DCMAKE_BUILD_TYPE=MinSizeRel -DUSE_TLS=1 -DUSE_WS=1 -DUSE_MBED_TLS=1 .. ; ninja install)

ws:
	mkdir -p build && (cd build ; cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 .. ; ninja install)

ws_install:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DUSE_TLS=1 -DUSE_WS=1 .. ; make -j 4 install)

ws_openssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_OPEN_SSL=1 .. ; make -j 4)

ws_mbedtls:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_MBED_TLS=1 .. ; make -j 4)

ws_no_ssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_WS=1 .. ; make -j 4)

uninstall:
	xargs rm -fv < build/install_manifest.txt

tag:
	git tag v"`sh tools/extract_version.sh`"

xcode:
	cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 -GXcode && open ixwebsocket.xcodeproj

xcode_openssl:
	cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 -DUSE_OPEN_SSL=1 -GXcode && open ixwebsocket.xcodeproj

.PHONY: docker

NAME   := ${DOCKER_REPO}/ws
TAG    := $(shell sh tools/extract_version.sh)
IMG    := ${NAME}:${TAG}
LATEST := ${NAME}:latest
BUILD  := ${NAME}:build

print_version:
	@echo 'IXWebSocket version =>' ${TAG}

set_version:
	sh tools/update_version.sh ${VERSION}

docker_test:
	docker build -f docker/Dockerfile.debian -t bsergean/ixwebsocket_test:build .

docker:
	git clean -dfx
	docker build -t ${IMG} .
	docker tag ${IMG} ${BUILD}

docker_push:
	docker tag ${IMG} ${LATEST}
	docker push ${LATEST}
	docker push ${IMG}

deploy: docker docker_push

run:
	docker run --cap-add sys_ptrace --entrypoint=sh -it bsergean/ws:build

# this is helpful to remove trailing whitespaces
trail:
	sh third_party/remote_trailing_whitespaces.sh

format:
	clang-format -i `find test ixwebsocket ws -name '*.cpp' -o -name '*.h'`

# That target is used to start a node server, but isn't required as we have
# a builtin C++ server started in the unittest now
test_server:
	(cd test && npm i ws && node broadcast-server.js)

# env TEST=Websocket_server make test
# env TEST=Websocket_chat make test
# env TEST=heartbeat make test
test:
	mkdir -p build && (cd build ; cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 .. ; ninja install)
	(cd test ; python2.7 run.py -r)

test_make:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 .. ; make -j 4)
	(cd test ; python2.7 run.py -r)

test_tsan:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_TEST=1 .. && xcodebuild -project ixwebsocket.xcodeproj -target ixwebsocket_unittest -enableThreadSanitizer YES)
	(cd build/test ; ln -sf Debug/ixwebsocket_unittest)
	(cd test ; python2.7 run.py -r)

test_ubsan:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_TEST=1 .. && xcodebuild -project ixwebsocket.xcodeproj -target ixwebsocket_unittest -enableUndefinedBehaviorSanitizer YES)
	(cd build/test ; ln -sf Debug/ixwebsocket_unittest)
	(cd test ; python2.7 run.py -r)

test_asan: build_test_asan
	(cd test ; python2.7 run.py -r)

build_test_asan:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_TEST=1 .. && xcodebuild -project ixwebsocket.xcodeproj -target ixwebsocket_unittest -enableAddressSanitizer YES)
	(cd build/test ; ln -sf Debug/ixwebsocket_unittest)

test_tsan_openssl:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_TEST=1 -DUSE_OPEN_SSL=1 .. && xcodebuild -project ixwebsocket.xcodeproj -target ixwebsocket_unittest -enableThreadSanitizer YES)
	(cd build/test ; ln -sf Debug/ixwebsocket_unittest)
	(cd test ; python2.7 run.py -r)

test_ubsan_openssl:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_TEST=1 -DUSE_OPEN_SSL=1 .. && xcodebuild -project ixwebsocket.xcodeproj -target ixwebsocket_unittest -enableUndefinedBehaviorSanitizer YES)
	(cd build/test ; ln -sf Debug/ixwebsocket_unittest)
	(cd test ; python2.7 run.py -r)

test_tsan_openssl_release:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Release -DUSE_TLS=1 -DUSE_TEST=1 -DUSE_OPEN_SSL=1 .. && xcodebuild -project ixwebsocket.xcodeproj -configuration Release -target ixwebsocket_unittest -enableThreadSanitizer YES)
	(cd build/test ; ln -sf Release/ixwebsocket_unittest)
	(cd test ; python2.7 run.py -r)

test_tsan_mbedtls:
	mkdir -p build && (cd build && cmake -GXcode -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_TEST=1 -DUSE_MBED_TLS=1 .. && xcodebuild -project ixwebsocket.xcodeproj -target ixwebsocket_unittest -enableThreadSanitizer YES)
	(cd build/test ; ln -sf Debug/ixwebsocket_unittest)
	(cd test ; python2.7 run.py -r)

build_test_openssl:
	mkdir -p build && (cd build ; cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_OPEN_SSL=1 -DUSE_TEST=1 .. ; ninja install)

test_openssl: build_test_openssl
	(cd test ; python2.7 run.py -r)

build_test_mbedtls:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_MBED_TLS=1 -DUSE_TEST=1 .. ; make -j 4)

test_mbedtls: build_test_mbedtls
	(cd test ; python2.7 run.py -r)

test_no_ssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TEST=1 .. ; make -j 4)
	(cd test ; python2.7 run.py -r)

ws_test: ws
	(cd ws ; env DEBUG=1 PATH=../ws/build:$$PATH bash test_ws.sh)

autobahn_report:
	cp -rvf ~/sandbox/reports/clients/* ../bsergean.github.io/IXWebSocket/autobahn/

httpd:
	clang++ --std=c++14 --stdlib=libc++ httpd.cpp \
		ixwebsocket/IXSelectInterruptFactory.cpp \
		ixwebsocket/IXCancellationRequest.cpp \
		ixwebsocket/IXSocketTLSOptions.cpp \
		ixwebsocket/IXUserAgent.cpp \
		ixwebsocket/IXDNSLookup.cpp \
		ixwebsocket/IXBench.cpp \
		ixwebsocket/IXWebSocketHttpHeaders.cpp \
		ixwebsocket/IXSelectInterruptPipe.cpp \
		ixwebsocket/IXHttp.cpp \
		ixwebsocket/IXSocketConnect.cpp \
		ixwebsocket/IXSocket.cpp \
		ixwebsocket/IXSocketServer.cpp \
		ixwebsocket/IXNetSystem.cpp \
		ixwebsocket/IXHttpServer.cpp \
		ixwebsocket/IXSocketFactory.cpp \
		ixwebsocket/IXConnectionState.cpp \
		ixwebsocket/IXUrlParser.cpp \
		ixwebsocket/IXSelectInterrupt.cpp \
		ixwebsocket/apple/IXSetThreadName_apple.cpp \
		-lz

httpd_linux:
	g++ --std=c++11 -o ixhttpd httpd.cpp -Iixwebsocket \
		ixwebsocket/IXSelectInterruptFactory.cpp \
		ixwebsocket/IXCancellationRequest.cpp \
		ixwebsocket/IXSocketTLSOptions.cpp \
		ixwebsocket/IXUserAgent.cpp \
		ixwebsocket/IXDNSLookup.cpp \
		ixwebsocket/IXBench.cpp \
		ixwebsocket/IXWebSocketHttpHeaders.cpp \
		ixwebsocket/IXSelectInterruptPipe.cpp \
		ixwebsocket/IXHttp.cpp \
		ixwebsocket/IXSocketConnect.cpp \
		ixwebsocket/IXSocket.cpp \
		ixwebsocket/IXSocketServer.cpp \
		ixwebsocket/IXNetSystem.cpp \
		ixwebsocket/IXHttpServer.cpp \
		ixwebsocket/IXSocketFactory.cpp \
		ixwebsocket/IXConnectionState.cpp \
		ixwebsocket/IXUrlParser.cpp \
		ixwebsocket/IXSelectInterrupt.cpp \
		ixwebsocket/linux/IXSetThreadName_linux.cpp \
		-lz -lpthread
	cp -f ixhttpd /usr/local/bin

# For the fork that is configured with appveyor
rebase_upstream:
	git fetch upstream
	git checkout master
	git reset --hard upstream/master
	git push origin master --force

install_cmake_for_linux:
	mkdir -p /tmp/cmake
	(cd /tmp/cmake ; curl -L -O https://github.com/Kitware/CMake/releases/download/v3.14.0/cmake-3.14.0-Linux-x86_64.tar.gz ; tar zxf cmake-3.14.0-Linux-x86_64.tar.gz)

#  python -m venv venv
#  source venv/bin/activate
#  pip install mkdocs
doc:
	mkdocs gh-deploy

change:
	vim ixwebsocket/IXWebSocketVersion.h docs/CHANGELOG.md

.PHONY: test
.PHONY: build
.PHONY: ws
