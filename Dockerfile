# Build time
FROM debian:buster as build

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update 
RUN apt-get -y install wget 
RUN mkdir -p /tmp/cmake
WORKDIR /tmp/cmake
RUN wget https://github.com/Kitware/CMake/releases/download/v3.14.0/cmake-3.14.0-Linux-x86_64.tar.gz 
RUN tar zxf cmake-3.14.0-Linux-x86_64.tar.gz

RUN apt-get -y install g++ 
RUN apt-get -y install libssl-dev
RUN apt-get -y install libz-dev
RUN apt-get -y install make

COPY . .

ARG CMAKE_BIN_PATH=/tmp/cmake/cmake-3.14.0-Linux-x86_64/bin
ENV PATH="${CMAKE_BIN_PATH}:${PATH}"

RUN ["make"]

# Runtime
FROM debian:buster as runtime

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update 
# Runtime 
RUN apt-get install -y libssl1.1 

# Debugging
RUN apt-get install -y strace
RUN apt-get install -y gdb
RUN apt-get install -y procps
RUN apt-get install -y htop

RUN adduser --disabled-password --gecos '' app
COPY --chown=app:app --from=build /usr/local/bin/ws /usr/local/bin/ws
RUN chmod +x /usr/local/bin/ws
RUN ldd /usr/local/bin/ws

# Now run in usermode
USER app
WORKDIR /home/app

ENTRYPOINT ["ws"]
CMD ["--help"]
