FROM debian:stretch

# RUN yum install -y gcc-c++ make cmake openssl-devel gdb
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update 
RUN apt-get -y install g++ 
RUN apt-get -y install libssl-dev
RUN apt-get -y install gdb
RUN apt-get -y install screen
RUN apt-get -y install procps
RUN apt-get -y install lsof

COPY . .

WORKDIR examples/ws_connect
RUN ["sh", "build_linux.sh"]
