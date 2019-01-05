#
# This makefile is just used to easily work with docker (linux build)
#
all: run

.PHONY: docker
docker:
	docker build -t ws_connect:latest .

run: docker
	docker run --cap-add sys_ptrace -it ws_connect:latest bash

build:
	(cd examples/satori_publisher ; mkdir -p build ; cd build ; cmake .. ; make)
	(cd examples/chat ; mkdir -p build ; cd build ; cmake .. ; make)
	(cd examples/ping_pong ; mkdir -p build ; cd build ; cmake .. ; make)
	(cd examples/ws_connect ; mkdir -p build ; cd build ; cmake .. ; make)
	(cd examples/echo_server ; mkdir -p build ; cd build ; cmake .. ; make)
	(cd examples/broadcast_server ; mkdir -p build ; cd build ; cmake .. ; make)

# That target is used to start a node server, but isn't required as we have 
# a builtin C++ server started in the unittest now
test_server:
	(cd test && npm i ws && node broadcast-server.js)

# env TEST=Websocket_server make test
test:
	python test/run.py

.PHONY: test
.PHONY: build
