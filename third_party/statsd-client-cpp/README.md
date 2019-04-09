# a client sdk for StatsD, written in C++

## API
See [header file](src/statsd_client.h) for more api detail.

** Notice: this client is not thread-safe **

## Demo
### test\_client
This simple demo shows how the use this client.

### system\_monitor
This is a daemon for monitoring a Linux system.
It'll wake up every minute and monitor the following:

* load
* cpu
* free memory
* free swap (disabled)
* received bytes
* transmitted bytes
* procs
* uptime

The stats sent to statsd will be in "host.MACAddress" namespace.

Usage:

    system_monitor statsd-host interface-to-monitor

e.g.

    `system_monitor 172.16.42.1 eth0`

