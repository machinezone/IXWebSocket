#!/bin/sh

rm -rf /tmp/ws_test
mkdir -p /tmp/ws_test

# Start a transport server
cd /tmp/ws_test
ws transfer --port 8090 --pidfile /tmp/ws_test/pidfile &

# Wait until the transfer server is up 
while true
do
    nc -zv 127.0.0.1 8090 && {
        echo "Transfer server up and running"
        break
    }
    echo "sleep ..."
    sleep 0.1
done

# Start a receiver
mkdir -p /tmp/ws_test/receive
cd /tmp/ws_test/receive
ws receive ws://127.0.0.1:8090 &

mkdir /tmp/ws_test/send
cd /tmp/ws_test/send
# mkfile 10m 10M_file
dd if=/dev/urandom of=10M_file count=10000 bs=1024

# Start the sender job
ws send ws://127.0.0.1:8090 10M_file

# Wait until the file has been written to disk
while true
do
    if test -f /tmp/ws_test/receive/10M_file ; then
        echo "Received file does exists, exiting loop"
        break
    fi
    echo "sleep ..."
    sleep 0.1
done

cksum /tmp/ws_test/send/10M_file
cksum /tmp/ws_test/receive/10M_file

# Give some time to ws receive to terminate
sleep 2

# Cleanup
kill `cat /tmp/ws_test/pidfile`
