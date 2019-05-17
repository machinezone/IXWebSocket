#
# This makefile is just used to easily work with docker (linux build)
#
all: brew

install: brew

# Use -DCMAKE_INSTALL_PREFIX= to install into another location
# on osx it is good practice to make /usr/local user writable
# sudo chown -R `whoami`/staff /usr/local
brew:
	mkdir -p build && (cd build ; cmake -DUSE_TLS=1 -DUSE_WS=1 .. ; make -j install)

ws:
	mkdir -p build && (cd build ; cmake -DUSE_TLS=1 -DUSE_WS=1 .. ; make -j)

uninstall:
	xargs rm -fv < build/install_manifest.txt

.PHONY: docker

NAME   := bsergean/ws
TAG    := $(shell cat DOCKER_VERSION)
IMG    := ${NAME}:${TAG}
LATEST := ${NAME}:latest
BUILD  := ${NAME}:build

docker:
	docker build -t ${IMG} .
	docker tag ${IMG} ${BUILD}

docker_push:
	docker tag ${IMG} ${LATEST}
	docker push ${LATEST}

run:
	docker run --cap-add sys_ptrace --entrypoint=bash -it bsergean/ws:build

# this is helpful to remove trailing whitespaces
trail:
	sh third_party/remote_trailing_whitespaces.sh

# That target is used to start a node server, but isn't required as we have 
# a builtin C++ server started in the unittest now
test_server:
	(cd test && npm i ws && node broadcast-server.js)

# env TEST=Websocket_server make test
# env TEST=Websocket_chat make test
# env TEST=heartbeat make test
test:
	python2.7 test/run.py

ws_test: ws
	(cd ws ; env DEBUG=1 PATH=../ws/build:$$PATH bash test_ws.sh)

# For the fork that is configured with appveyor
rebase_upstream:
	git fetch upstream
	git checkout master
	git reset --hard upstream/master
	git push origin master --force

install_cmake_for_linux:
	mkdir -p /tmp/cmake
	(cd /tmp/cmake ; curl -L -O https://github.com/Kitware/CMake/releases/download/v3.14.0/cmake-3.14.0-Linux-x86_64.tar.gz ; tar zxf cmake-3.14.0-Linux-x86_64.tar.gz)

.PHONY: test
.PHONY: build
.PHONY: ws
