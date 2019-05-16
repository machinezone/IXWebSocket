#!/bin/sh

# Handle Ctrl-C by killing all sub-processing AND exiting
trap cleanup INT

function cleanup {
    kill `cat /tmp/ws_test/pidfile.transfer`
    kill `cat /tmp/ws_test/pidfile.receive`
    kill `cat /tmp/ws_test/pidfile.send`
    exit 1
}

rm -rf /tmp/ws_test
mkdir -p /tmp/ws_test

# Start a transport server
cd /tmp/ws_test
ws transfer --port 8090 --pidfile /tmp/ws_test/pidfile.transfer &

# Wait until the transfer server is up 
while true
do
    nc -zv 127.0.0.1 8090 && {
        echo "Transfer server up and running"
        break
    }
    echo "sleep ... wait for transfer server"
    sleep 0.1
done

# Start a receiver
mkdir -p /tmp/ws_test/receive
cd /tmp/ws_test/receive
ws receive --delay 10 ws://127.0.0.1:8090 --pidfile /tmp/ws_test/pidfile.receive &

mkdir /tmp/ws_test/send
cd /tmp/ws_test/send
dd if=/dev/urandom of=20M_file count=20000 bs=1024

# Start the sender job
ws send --pidfile /tmp/ws_test/pidfile.send ws://127.0.0.1:8090 20M_file

# Wait until the file has been written to disk
while true
do
    if test -f /tmp/ws_test/receive/20M_file ; then
        echo "Received file does exists, exiting loop"
        break
    fi
    echo "sleep ... wait for output file"
    sleep 0.1
done

cksum /tmp/ws_test/send/20M_file
cksum /tmp/ws_test/receive/20M_file

# Give some time to ws receive to terminate
sleep 2

# Cleanup
kill `cat /tmp/ws_test/pidfile.transfer`
kill `cat /tmp/ws_test/pidfile.receive`
kill `cat /tmp/ws_test/pidfile.send`
exit 0
