all:
	honcho start # install honcho (pip install honcho) or procman (the original ruby script)

.PHONY: docker

NAME   := bsergean/echo_server
TAG    := $(shell cat DOCKER_VERSION)
IMG    := ${NAME}:${TAG}
LATEST := ${NAME}:latest
BUILD  := ${NAME}:build

docker_test:
	docker build -t ${BUILD} .

docker:
	git clean -dfx
	docker build -t ${IMG} .
	docker tag ${IMG} ${BUILD}

docker_push:
	docker tag ${IMG} ${LATEST}
	docker push ${LATEST}
	docker push ${IMG}

echo_server:
	docker run -p 8766:8766 -e BIND_HOST=0.0.0.0 -e MODE=echo_server -it --rm bsergean/echo_server:build

echo_server_ssl:
	docker run -p 8766:8766 -e BIND_HOST=0.0.0.0 -e MODE=echo_server_ssl -it --rm bsergean/echo_server:build

broadcast_server:
	docker run -p 8766:8766 -e BIND_HOST=0.0.0.0 -e MODE=broadcast_server -it --rm bsergean/echo_server:build

broadcast_server_ssl:
	docker run -p 8766:8766 -e BIND_HOST=0.0.0.0 -e MODE=broadcast_server_ssl -it --rm bsergean/echo_server:build
