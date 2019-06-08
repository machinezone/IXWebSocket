# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased] - 2019-06-xx
### Changed
- IXSocketMbedTLS: better error handling in close and connect
- ws echo_server has a -g option to print a greeting message on connect

## [3.1.2] - 2019-06-06
### Added
- ws connect has a -x option to disable per message deflate
- Add WebSocket::disablePerMessageDeflate() option.

## [3.0.0] - 2019-06-xx
### Changed
- TLS, aka SSL works on Windows (websocket and http clients)
- ws command line tool build on Windows
- Async API for HttpClient
- HttpClient API changed to use shared_ptr for response and request
