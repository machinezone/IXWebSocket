## General

ws is a command line tool that should exercise most of the IXWebSocket code, and provide example code.

```
ws is a websocket tool
Usage: ws [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
  send                        Send a file
  receive                     Receive a file
  transfer                    Broadcasting server
  connect                     Connect to a remote server
  chat                        Group chat
  echo_server                 Echo server
  broadcast_server            Broadcasting server
  ping                        Ping pong
  curl                        HTTP Client
  redis_publish               Redis publisher
  redis_subscribe             Redis subscriber
  cobra_subscribe             Cobra subscriber
  cobra_publish               Cobra publisher
  cobra_to_statsd             Cobra to statsd
  cobra_to_sentry             Cobra to sentry
  snake                       Snake server
  httpd                       HTTP server
```

## File transfer

```
# Start transfer server, which is just a broadcast server at this point
ws transfer # running on port 8080.

# Start receiver first
ws receive ws://localhost:8080

# Then send a file. File will be received and written to disk by the receiver process
ws send ws://localhost:8080 /file/to/path
```

## HTTP Client

```
$ ws curl --help
HTTP Client
Usage: ws curl [OPTIONS] url

Positionals:
  url TEXT REQUIRED           Connection url

Options:
  -h,--help                   Print this help message and exit
  -d TEXT                     Form data
  -F TEXT                     Form data
  -H TEXT                     Header
  --output TEXT               Output file
  -I                          Send a HEAD request
  -L                          Follow redirects
  --max-redirects INT         Max Redirects
  -v                          Verbose
  -O                          Save output to disk
  --compress                  Enable gzip compression
  --connect-timeout INT       Connection timeout
  --transfer-timeout INT      Transfer timeout
```

## Cobra Client

[cobra](https://github.com/machinezone/cobra) is a real time messenging server. ws has sub-command to interacti with cobra.
