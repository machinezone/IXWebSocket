#!/bin/sh

case $MODE in
    echo_server)
        ./echo_server.py
        ;;
    ssl)
        python /usr/bin/echo_server_ssl.py
        ;;
esac
