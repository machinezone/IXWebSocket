#
# This makefile is just used to easily work with docker (linux build)
#
all: brew

install: brew

# Use -DCMAKE_INSTALL_PREFIX= to install into another location
# on osx it is good practice to make /usr/local user writable
# sudo chown -R `whoami`/staff /usr/local
brew:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 .. ; make -j 4 install)

ws:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 .. ; make -j 4)

ws_install:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 .. ; make -j 4 install)

ws_openssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_OPEN_SSL=1 .. ; make -j 4)

ws_mbedtls:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_MBED_TLS=1 .. ; make -j 4)

ws_no_ssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_WS=1 .. ; make -j 4)

uninstall:
	xargs rm -fv < build/install_manifest.txt

tag:
	git tag v"`cat DOCKER_VERSION`"

xcode:
	cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 -GXcode && open ixwebsocket.xcodeproj

xcode_openssl:
	cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 -DUSE_OPEN_SSL=1 -GXcode && open ixwebsocket.xcodeproj

.PHONY: docker

NAME   := bsergean/ws
TAG    := $(shell cat DOCKER_VERSION)
IMG    := ${NAME}:${TAG}
LATEST := ${NAME}:latest
BUILD  := ${NAME}:build

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
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_WS=1 -DUSE_TEST=1 .. ; make -j 4)
	(cd test ; python2.7 run.py -r)

test_openssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_OPEN_SSL=1 -DUSE_TEST=1 .. ; make -j 4)
	(cd test ; python2.7 run.py -r)

test_mbedtls:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TLS=1 -DUSE_MBED_TLS=1 -DUSE_TEST=1 .. ; make -j 4)
	(cd test ; python2.7 run.py -r)

test_no_ssl:
	mkdir -p build && (cd build ; cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_TEST=1 .. ; make -j 4)
	(cd test ; python2.7 run.py -r)

ws_test: ws
	(cd ws ; env DEBUG=1 PATH=../ws/build:$$PATH bash test_ws.sh)

autobahn_report:
	cp -rvf ~/sandbox/reports/clients/* ../bsergean.github.io/IXWebSocket/autobahn/

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

.PHONY: test
.PHONY: build
.PHONY: ws
