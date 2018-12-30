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

test_server:
	(cd test && npm i ws && node broadcast-server.js)
test:
	(cd test && cmake . && make && ./ixwebsocket_unittest)

.PHONY: test
