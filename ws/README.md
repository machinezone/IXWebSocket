# General

ws is a command line tool that should exercise most of the IXWebSocket code, and provide example code.

## file transfer

```
# Start transfer server, which is just a broadcast server at this point
ws transfer # running on port 8080.

# Start receiver first
ws receive ws://localhost:8080 

# Then send a file. File will be received and written to disk by the receiver process
ws send ws://localhost:8080 /file/to/path
```

## curl

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
