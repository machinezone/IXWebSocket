/*
 *  IXHttpClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttpClient.h"

#include "IXGzipCodec.h"
#include "IXSocketFactory.h"
#include "IXUrlParser.h"
#include "IXUserAgent.h"
#include "IXWebSocketHttpHeaders.h"
#include <assert.h>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
    ix::HttpRequestArgsPtr snapshotRequest(const ix::HttpRequestArgs& args)
    {
        auto copy = std::make_shared<ix::HttpRequestArgs>();
        copy->url = args.url;
        copy->verb = args.verb;
        copy->extraHeaders = args.extraHeaders;
        copy->body = args.body;
        copy->bodySize = args.bodySize;
        copy->onUploadCallback = args.onUploadCallback;
        copy->multipartBoundary = args.multipartBoundary;
        copy->connectTimeout = args.connectTimeout;
        copy->transferTimeout = args.transferTimeout;
        copy->followRedirects = args.followRedirects;
        copy->maxRedirects = args.maxRedirects;
        copy->verbose = args.verbose;
        copy->compress = args.compress;
        copy->compressRequest = args.compressRequest;
        copy->logger = args.logger;
        copy->onProgressCallback = args.onProgressCallback;
        copy->onChunkCallback = args.onChunkCallback;
        copy->cancel = args.cancel.load();
        return copy;
    }
    bool parseSize(const std::string& text, unsigned base, size_t& value)
    {
        if (text.empty())
        {
            return false;
        }
        value = 0;
        for (unsigned char c : text)
        {
            unsigned digit;
            if (c >= '0' && c <= '9')
            {
                digit = c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = c - 'a' + 10;
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = c - 'A' + 10;
            }
            else
            {
                return false;
            }
            if (digit >= base || value > ((std::numeric_limits<size_t>::max)() - digit) / base)
            {
                return false;
            }
            value = value * base + digit;
        }
        return true;
    }
} // namespace

namespace ix
{
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods
    const std::string HttpClient::kPost = "POST";
    const std::string HttpClient::kGet = "GET";
    const std::string HttpClient::kHead = "HEAD";
    const std::string HttpClient::kDelete = "DELETE";
    const std::string HttpClient::kPut = "PUT";
    const std::string HttpClient::kPatch = "PATCH";

    HttpClient::HttpClient(bool async)
        : _async(async)
        , _stop(false)
        , _forceBody(false)
    {
        if (!_async)
        {
            return;
        }

        _thread = std::thread(&HttpClient::run, this);
    }

    HttpClient::~HttpClient()
    {
        if (!_thread.joinable())
        {
            return;
        }

        _stop = true;
        _condition.notify_one();
        _thread.join();
    }

    std::unique_ptr<Socket> HttpClient::takeConnection(const std::string& url,
                                                       SocketTLSOptions& tlsOptions,
                                                       ProxyOptions& proxyOptions,
                                                       std::string& error)
    {
        std::unique_lock<std::recursive_mutex> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            error = "HTTP client is busy";
            return nullptr;
        }
        if (!_socket)
        {
            error = "HTTP client has no reusable connection";
            return nullptr;
        }
        if (!UrlParser::isSameOrigin(_connectionUrl, url))
        {
            error = "HTTP connection and WebSocket destination must have the same host, port and "
                    "TLS mode";
            return nullptr;
        }
        char byte;
        if (_socket->recv(&byte, 1) >= 0 || !Socket::isWaitNeeded())
        {
            _socket.reset();
            error = "HTTP connection is closed or has unexpected pending data";
            return nullptr;
        }
        tlsOptions = _tlsOptions;
        proxyOptions = _proxyOptions;
        return std::move(_socket);
    }

    void HttpClient::setTLSOptions(const SocketTLSOptions& tlsOptions)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _socket.reset();
        ++_connectionConfigVersion;
        _tlsOptions = tlsOptions;
    }

    void HttpClient::setProxyOptions(const ProxyOptions& proxyOptions)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _socket.reset();
        ++_connectionConfigVersion;
        _proxyOptions = proxyOptions;
    }

    void HttpClient::setForceBody(bool value)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _forceBody = value;
    }

    HttpRequestArgsPtr HttpClient::createRequest(const std::string& url, const std::string& verb)
    {
        auto request = std::make_shared<HttpRequestArgs>();
        request->url = url;
        request->verb = verb;
        return request;
    }

    bool HttpClient::performRequest(HttpRequestArgsPtr args,
                                    const OnResponseCallback& onResponseCallback)
    {
        assert(_async && "HttpClient needs its async parameter set to true "
                         "in order to call performRequest");
        if (!_async || !args)
        {
            return false;
        }

        auto snapshot = snapshotRequest(*args);

        {
            std::lock_guard<std::mutex> lock(_queueMutex);

            // Retain the original arguments only for their shared cancellation flag.
            _queue.push(
                [this, args, snapshot, onResponseCallback]
                {
                    auto response = requestImpl(
                        snapshot->url, snapshot->verb, snapshot->body, snapshot, args->cancel, 0);
                    onResponseCallback(response);
                });
        }

        // wake up one thread
        _condition.notify_one();

        return true;
    }

    void HttpClient::run()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(_queueMutex);

                while (!_stop && _queue.empty())
                {
                    _condition.wait(lock);
                }

                if (_stop)
                {
                    return;
                }

                task = std::move(_queue.front());
                _queue.pop();
            }

            if (_stop)
            {
                return;
            }

            task();

            if (_stop)
            {
                return;
            }
        }
    }

    HttpResponsePtr HttpClient::request(const std::string& url,
                                        const std::string& verb,
                                        const std::string& body,
                                        HttpRequestArgsPtr args,
                                        int redirects)
    {
        return requestImpl(url, verb, body, args, args->cancel, redirects);
    }

    HttpResponsePtr HttpClient::requestImpl(const std::string& url,
                                            const std::string& verb,
                                            const std::string& body,
                                            HttpRequestArgsPtr args,
                                            std::atomic<bool>& cancel,
                                            int redirects,
                                            bool allowRetry)
    {
        // We only have one socket connection, so we cannot
        // make multiple requests concurrently.
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        uint64_t uploadSize = 0;
        uint64_t downloadSize = 0;
        int code = 0;
        WebSocketHttpHeaders headers;
        std::string payload;
        std::string description;

        const bool streaming = static_cast<bool>(args->onUploadCallback);
        const bool hasBody = verb == kPost || verb == kPut || verb == kPatch || _forceBody;
        const auto transfer = args->extraHeaders.find("Transfer-Encoding");
        std::string coding = transfer == args->extraHeaders.end() ? "" : transfer->second;
        const auto begin = coding.find_first_not_of(" \t");
        coding = begin == std::string::npos
                     ? ""
                     : coding.substr(begin, coding.find_last_not_of(" \t") - begin + 1);
        const bool chunked = transfer != args->extraHeaders.end() &&
                             !CaseInsensitiveLess::cmp(coding, "chunked") &&
                             !CaseInsensitiveLess::cmp("chunked", coding);
        if ((transfer != args->extraHeaders.end() &&
             (!chunked || !hasBody || args->extraHeaders.count("Content-Length") ||
              args->compressRequest)) ||
            (streaming && (!hasBody || !body.empty() || args->compressRequest ||
                           args->extraHeaders.count("Content-Length"))))
        {
            return std::make_shared<HttpResponse>(
                0,
                "",
                HttpErrorCode::Invalid,
                WebSocketHttpHeaders(),
                "",
                "Uploads require a body-capable method and unambiguous framing; "
                "streamed bodies require an empty string body and no compression; "
                "Transfer-Encoding supports only chunked without Content-Length or compression");
        }

        std::string protocol, host, path, query;
        int port;
        bool isProtocolDefaultPort;

        if (!UrlParser::parse(url, protocol, host, path, query, port, isProtocolDefaultPort))
        {
            std::stringstream ss;
            ss << "Cannot parse url: " << url;
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  HttpErrorCode::UrlMalformed,
                                                  headers,
                                                  payload,
                                                  ss.str(),
                                                  uploadSize,
                                                  downloadSize);
        }

        if (cancel || _stop)
        {
            return std::make_shared<HttpResponse>(
                0, "", HttpErrorCode::Cancelled, headers, "", "Request cancelled");
        }
        // Move the idle connection out: every error/exception then closes the active
        // socket automatically. Only a fully consumed response returns it to the cache.
        auto socket = std::move(_socket);
        bool reused = socket && UrlParser::isSameOrigin(_connectionUrl, url);
        if (reused)
        {
            // Read through the TLS abstraction, including buffered close notifications.
            // Unsolicited data is also unsafe to interpret as the next response.
            char byte;
            reused = socket->recv(&byte, 1) < 0 && Socket::isWaitNeeded();
        }
        const uint64_t configVersion = _connectionConfigVersion;
        std::string errorMsg;
        if (!reused)
        {
            socket.reset();
            socket = createSocket(protocol == "https", -1, errorMsg, _tlsOptions, _proxyOptions);
        }

        if (!socket)
        {
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  HttpErrorCode::CannotCreateSocket,
                                                  headers,
                                                  payload,
                                                  errorMsg,
                                                  uploadSize,
                                                  downloadSize);
        }

        // Build request string
        std::stringstream ss;
        ss << verb << " " << path << " HTTP/1.1\r\n";
        ss << "Host: " << host;
        if (!isProtocolDefaultPort)
        {
            ss << ":" << port;
        }
        ss << "\r\n";

#ifdef IXWEBSOCKET_USE_ZLIB
        if (args->compress && !args->onChunkCallback)
        {
            ss << "Accept-Encoding: gzip"
               << "\r\n";
        }
#endif

        // Append extra headers
        for (auto&& it : args->extraHeaders)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }

        // Set a default Accept header if none is present
        if (args->extraHeaders.find("Accept") == args->extraHeaders.end())
        {
            ss << "Accept: */*"
               << "\r\n";
        }

        // Set a default User agent if none is present
        if (args->extraHeaders.find("User-Agent") == args->extraHeaders.end())
        {
            ss << "User-Agent: " << userAgent() << "\r\n";
        }

        // Set an origin header if missing
        if (args->extraHeaders.find("Origin") == args->extraHeaders.end())
        {
            ss << "Origin: " << protocol << "://" << host << ":" << port << "\r\n";
        }

        if (hasBody)
        {
            // Set request compression header
#ifdef IXWEBSOCKET_USE_ZLIB
            if (args->compressRequest)
            {
                ss << "Content-Encoding: gzip"
                   << "\r\n";
            }
#endif

            if (!chunked)
            {
                ss << "Content-Length: " << (streaming ? args->bodySize : body.size()) << "\r\n";
            }

            // Set default Content-Type if unspecified
            if (args->extraHeaders.find("Content-Type") == args->extraHeaders.end())
            {
                if (args->multipartBoundary.empty())
                {
                    ss << "Content-Type: "
                       << (streaming ? "application/octet-stream"
                                     : "application/x-www-form-urlencoded")
                       << "\r\n";
                }
                else
                {
                    ss << "Content-Type: multipart/form-data; boundary=" << args->multipartBoundary
                       << "\r\n";
                }
            }
            ss << "\r\n";
            if (!streaming && !chunked)
            {
                ss << body;
            }
        }
        else
        {
            ss << "\r\n";
        }

        std::string req(ss.str());
        std::string errMsg;
        if (streaming && !chunked &&
            args->bodySize > (std::numeric_limits<uint64_t>::max)() - req.size())
        {
            return std::make_shared<HttpResponse>(
                0, "", HttpErrorCode::Invalid, WebSocketHttpHeaders(), "", "Upload size overflow");
        }

        // Make a cancellation object dealing with connection timeout
        auto cancelled = makeCancellationRequestWithTimeout(args->connectTimeout, cancel);

        auto isCancellationRequested = [&]() { return cancelled() || _stop; };

        bool success = reused || socket->connect(host, port, errMsg, isCancellationRequested);
        if (!success)
        {
            auto errorCode = cancel ? HttpErrorCode::Cancelled : HttpErrorCode::CannotConnect;
            std::ostringstream oss;
            oss << "Cannot connect to url: " << url << " / error : " << errMsg;
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  errorCode,
                                                  headers,
                                                  payload,
                                                  oss.str(),
                                                  uploadSize,
                                                  downloadSize);
        }

        // Make a new cancellation object dealing with transfer timeout
        cancelled = makeCancellationRequestWithTimeout(args->transferTimeout, cancel);

        if (args->verbose)
        {
            std::ostringstream oss;
            oss << "Sending " << verb << " request "
                << "to " << host << ":" << port << std::endl
                << "request size: " << req.size() << " bytes" << std::endl
                << "=============" << std::endl
                << req << "=============" << std::endl
                << std::endl;

            log(oss.str(), args);
        }

        // A stale idle connection may fail after the probe. Retry only bodyless safe
        // methods, once, before receiving any response bytes. Never replay an upload.
        const bool canRetry =
            reused && allowRetry && !hasBody && !streaming && (verb == kGet || verb == kHead);
        const auto retry = [&]()
        {
            socket.reset();
            return requestImpl(url, verb, body, args, cancel, redirects, false);
        };
        if (!socket->writeBytes(req, isCancellationRequested))
        {
            if (canRetry && !isCancellationRequested())
            {
                return retry();
            }
            auto errorCode = cancel ? HttpErrorCode::Cancelled : HttpErrorCode::SendError;
            std::string msg("Cannot send request");
            return std::make_shared<HttpResponse>(
                code, description, errorCode, headers, payload, msg, uploadSize, downloadSize);
        }

        uploadSize = req.size();
        if (streaming || chunked)
        {
            auto uploadFailed = [&](const std::string& message)
            {
                socket->close();
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      cancel ? HttpErrorCode::Cancelled
                                                             : HttpErrorCode::SendError,
                                                      headers,
                                                      payload,
                                                      message,
                                                      uploadSize,
                                                      downloadSize);
            };
            // Keep each write small even on platforms where the socket API takes an int.
            std::string chunk;
            const size_t capacity = 64 * 1024;
            uint64_t offset = 0;
            const uint64_t size = streaming ? args->bodySize : body.size();
            while (chunked || offset < size)
            {
                if (isCancellationRequested())
                {
                    return uploadFailed("Upload cancelled or timed out");
                }
                const size_t wanted =
                    chunked && streaming
                        ? capacity
                        : static_cast<size_t>((std::min) (uint64_t(capacity), size - offset));
                chunk.resize(wanted);
                size_t count;
                try
                {
                    if (streaming)
                    {
                        count = args->onUploadCallback(offset, &chunk[0], wanted);
                    }
                    else
                    {
                        count = wanted;
                        if (count)
                        {
                            std::memcpy(&chunk[0], body.data() + offset, count);
                        }
                    }
                }
                catch (...)
                {
                    return uploadFailed("Upload callback threw an exception");
                }
                if (count > wanted || (!count && !chunked))
                {
                    return uploadFailed("Upload callback returned an invalid byte count");
                }
                if (!count)
                {
                    // EOF for a chunked source.
                    break;
                }
                if (count > (std::numeric_limits<uint64_t>::max)() - offset)
                {
                    return uploadFailed("Upload size overflow");
                }
                chunk.resize(count);
                if (chunked)
                {
                    std::ostringstream header;
                    header << std::hex << count << "\r\n";
                    chunk.insert(0, header.str());
                    chunk += "\r\n";
                }
                if (chunk.size() > (std::numeric_limits<uint64_t>::max)() - uploadSize)
                {
                    return uploadFailed("Upload size overflow");
                }
                if (!socket->writeBytes(chunk, isCancellationRequested))
                {
                    return uploadFailed("Cannot send upload body");
                }
                offset += count;
                uploadSize += chunk.size();
            }
            if (chunked)
            {
                if (uploadSize > (std::numeric_limits<uint64_t>::max)() - 5 ||
                    !socket->writeBytes("0\r\n\r\n", isCancellationRequested))
                {
                    return uploadFailed("Cannot finish chunked upload");
                }
                uploadSize += 5;
            }
        }

        std::pair<bool, std::string> lineResult;
        for (unsigned informational = 0;; ++informational)
        {
            lineResult = socket->readLine(isCancellationRequested);
            if (!lineResult.first)
            {
                if (canRetry && informational == 0 && lineResult.second.empty() &&
                    !isCancellationRequested())
                {
                    return retry();
                }
                const auto error =
                    cancel ? HttpErrorCode::Cancelled : HttpErrorCode::CannotReadStatusLine;
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      error,
                                                      headers,
                                                      payload,
                                                      "Cannot retrieve status line",
                                                      uploadSize,
                                                      downloadSize);
            }
            if (args->verbose)
            {
                log("Status line " + lineResult.second, args);
            }
            if (sscanf(lineResult.second.c_str(), "HTTP/1.1 %d", &code) != 1 || code < 100 ||
                code > 599)
            {
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      HttpErrorCode::MissingStatus,
                                                      headers,
                                                      payload,
                                                      "Cannot parse response code from status line",
                                                      uploadSize,
                                                      downloadSize);
            }
            auto result = parseHttpHeaders(socket, isCancellationRequested);
            headers = std::move(result.second);
            if (!result.first)
            {
                const auto error =
                    cancel ? HttpErrorCode::Cancelled : HttpErrorCode::HeaderParsingError;
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      error,
                                                      headers,
                                                      payload,
                                                      "Cannot parse http headers",
                                                      uploadSize,
                                                      downloadSize);
            }
            if (code >= 200)
            {
                break;
            }
            if (code == 101 || informational == 15)
            {
                return std::make_shared<HttpResponse>(
                    code,
                    description,
                    HttpErrorCode::HeaderParsingError,
                    headers,
                    payload,
                    "Unexpected protocol switch or too many informational responses",
                    uploadSize,
                    downloadSize);
            }
        }

        const bool followRedirect =
            args->followRedirects &&
            (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
        const auto locationHeader = headers.find("Location");
        const bool drainRedirect = followRedirect && locationHeader != headers.end() &&
                                   UrlParser::isSameOrigin(url, locationHeader->second);
        size_t contentLength = 0;
        const auto lengthHeader = headers.find("Content-Length");
        const bool validLength =
            lengthHeader == headers.end() || parseSize(lengthHeader->second, 10, contentLength);
        const bool bodyless = verb == kHead || code == 204 || code == 304;
        const bool framed =
            bodyless || headers.count("Transfer-Encoding") || headers.count("Content-Length");
        // Discard intermediate redirect bodies without buffering or exposing them to callbacks.
        const OnChunkCallback onChunk =
            followRedirect ? OnChunkCallback([](const std::string&) { }) : args->onChunkCallback;
        const OnProgressCallback onProgress =
            followRedirect ? OnProgressCallback() : args->onProgressCallback;

        auto failure = [&](HttpErrorCode error, const std::string& message)
        {
            if (cancel)
            {
                error = HttpErrorCode::Cancelled;
            }
            return std::make_shared<HttpResponse>(
                code, description, error, headers, payload, message, uploadSize, downloadSize);
        };

        // Transfer-Encoding determines framing in preference to Content-Length.
        if (bodyless || (followRedirect && (!framed || !drainRedirect)))
        {
            // Redirects without an explicit length can still be followed on a fresh socket.
        }
        else if (headers.find("Transfer-Encoding") != headers.end())
        {
            const auto& coding = headers.at("Transfer-Encoding");
            if (CaseInsensitiveLess::cmp(coding, "chunked") ||
                CaseInsensitiveLess::cmp("chunked", coding))
            {
                return failure(HttpErrorCode::HeaderParsingError, "Unsupported Transfer-Encoding");
            }
            while (true)
            {
                lineResult = socket->readLine(isCancellationRequested);
                if (!lineResult.first)
                {
                    return failure(HttpErrorCode::ChunkReadError, "Cannot read chunk size");
                }
                size_t chunkSize = 0;
                const auto sizeText =
                    lineResult.second.substr(0, lineResult.second.find_first_of(";\r\n"));
                if (!parseSize(sizeText, 16, chunkSize))
                {
                    return failure(HttpErrorCode::ChunkReadError, "Invalid chunk size");
                }
                if (chunkSize == 0)
                {
                    // Trailers are a separate header block; do not overwrite framing headers.
                    if (!parseHttpHeaders(socket, isCancellationRequested).first)
                    {
                        return failure(HttpErrorCode::ChunkReadError, "Invalid chunk trailers");
                    }
                    break;
                }
                auto chunk =
                    socket->readBytes(chunkSize, onProgress, onChunk, isCancellationRequested);
                if (!chunk.first)
                {
                    return failure(HttpErrorCode::ChunkReadError, "Cannot read chunk");
                }
                lineResult = socket->readLine(isCancellationRequested);
                if (!lineResult.first || (lineResult.second != "\r\n" && lineResult.second != "\n"))
                {
                    return failure(HttpErrorCode::ChunkReadError, "Invalid chunk terminator");
                }
                downloadSize += chunkSize;
                if (!onChunk)
                {
                    payload += chunk.second;
                }
            }
        }
        else if (headers.find("Content-Length") != headers.end())
        {
            if (!validLength)
            {
                return failure(HttpErrorCode::HeaderParsingError, "Invalid Content-Length");
            }
            auto chunk =
                socket->readBytes(contentLength, onProgress, onChunk, isCancellationRequested);
            if (!chunk.first)
            {
                return failure(HttpErrorCode::ChunkReadError, "Cannot read body");
            }
            downloadSize = contentLength;
            if (!onChunk)
            {
                payload = std::move(chunk.second);
            }
        }
        else
        {
            return failure(HttpErrorCode::CannotReadBody, "Cannot read http body");
        }

        // If the content was compressed with gzip, decode it
        if (!bodyless && !followRedirect && headers["Content-Encoding"] == "gzip")
        {
#ifdef IXWEBSOCKET_USE_ZLIB
            std::string decompressedPayload;
            if (!gzipDecompress(payload, decompressedPayload))
            {
                std::string msg("Error decompressing payload");
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      HttpErrorCode::Gzip,
                                                      headers,
                                                      payload,
                                                      msg,
                                                      uploadSize,
                                                      downloadSize);
            }
            payload = decompressedPayload;
#else
            std::string msg("ixwebsocket was not compiled with gzip support on");
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  HttpErrorCode::Gzip,
                                                  headers,
                                                  payload,
                                                  msg,
                                                  uploadSize,
                                                  downloadSize);
#endif
        }

        const bool reusable =
            framed && validLength &&
            !hasHttpHeaderToken(args->extraHeaders, "Connection", "close") &&
            !hasHttpHeaderToken(headers, "Connection", "close") &&
            (!args->extraHeaders.count("Transfer-Encoding") || chunked) &&
            !(hasBody && args->extraHeaders.count("Content-Length")) &&
            !(headers.count("Transfer-Encoding") && headers.count("Content-Length")) &&
            !(code == 204 &&
              (headers.count("Content-Length") || headers.count("Transfer-Encoding"))) &&
            !(verb == "CONNECT" && code >= 200 && code < 300);
        if (reusable && (!followRedirect || drainRedirect) && !isCancellationRequested() &&
            configVersion == _connectionConfigVersion)
        {
            _socket = std::move(socket);
            _connectionUrl = url;
        }
        // Close non-reusable redirect connections before opening the next one.
        socket.reset();

        if (followRedirect)
        {
            if (headers.find("Location") == headers.end())
            {
                std::string msg("Missing location header for redirect");
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      HttpErrorCode::MissingLocation,
                                                      headers,
                                                      payload,
                                                      msg,
                                                      uploadSize,
                                                      downloadSize);
            }

            if (redirects >= args->maxRedirects)
            {
                std::ostringstream oss;
                oss << "Too many redirects: " << redirects;
                return std::make_shared<HttpResponse>(code,
                                                      description,
                                                      HttpErrorCode::TooManyRedirects,
                                                      headers,
                                                      payload,
                                                      oss.str(),
                                                      uploadSize,
                                                      downloadSize);
            }

            const std::string& location = headers.at("Location");
            auto redirected = snapshotRequest(*args);
            // Credentials are scoped to the same scheme, host and port.
            if (!UrlParser::isSameOrigin(url, location))
            {
                redirected->extraHeaders.erase("Authorization");
                redirected->extraHeaders.erase("Cookie");
            }
            if (code == 303 && verb != kHead)
            {
                redirected->extraHeaders.erase("Content-Length");
                redirected->extraHeaders.erase("Transfer-Encoding");
                redirected->extraHeaders.erase("Content-Type");
                redirected->extraHeaders.erase("Content-Encoding");
                redirected->multipartBoundary.clear();
                redirected->compressRequest = false;
                redirected->onUploadCallback = nullptr;
                redirected->bodySize = 0;
                return requestImpl(
                    location, kGet, std::string(), redirected, cancel, redirects + 1);
            }
            return requestImpl(location, verb, body, redirected, cancel, redirects + 1);
        }

        return std::make_shared<HttpResponse>(code,
                                              description,
                                              HttpErrorCode::Ok,
                                              headers,
                                              payload,
                                              std::string(),
                                              uploadSize,
                                              downloadSize);
    }

    HttpResponsePtr HttpClient::get(const std::string& url, HttpRequestArgsPtr args)
    {
        return request(url, kGet, std::string(), args);
    }

    HttpResponsePtr HttpClient::head(const std::string& url, HttpRequestArgsPtr args)
    {
        return request(url, kHead, std::string(), args);
    }

    HttpResponsePtr HttpClient::Delete(const std::string& url, HttpRequestArgsPtr args)
    {
        return request(url, kDelete, std::string(), args);
    }

    HttpResponsePtr HttpClient::request(const std::string& url,
                                        const std::string& verb,
                                        const HttpParameters& httpParameters,
                                        const HttpFormDataParameters& httpFormDataParameters,
                                        HttpRequestArgsPtr args)
    {
        std::string body;

        if (httpFormDataParameters.empty())
        {
            body = serializeHttpParameters(httpParameters);
        }
        else
        {
            std::string multipartBoundary = generateMultipartBoundary();
            args->multipartBoundary = multipartBoundary;
            body = serializeHttpFormDataParameters(
                multipartBoundary, httpFormDataParameters, httpParameters);
        }

#ifdef IXWEBSOCKET_USE_ZLIB
        if (args->compressRequest)
        {
            body = gzipCompress(body);
        }
#endif

        return request(url, verb, body, args);
    }

    HttpResponsePtr HttpClient::post(const std::string& url,
                                     const HttpParameters& httpParameters,
                                     const HttpFormDataParameters& httpFormDataParameters,
                                     HttpRequestArgsPtr args)
    {
        return request(url, kPost, httpParameters, httpFormDataParameters, args);
    }

    HttpResponsePtr HttpClient::post(const std::string& url,
                                     const std::string& body,
                                     HttpRequestArgsPtr args)
    {
        return request(url, kPost, body, args);
    }

    HttpResponsePtr HttpClient::put(const std::string& url,
                                    const HttpParameters& httpParameters,
                                    const HttpFormDataParameters& httpFormDataParameters,
                                    HttpRequestArgsPtr args)
    {
        return request(url, kPut, httpParameters, httpFormDataParameters, args);
    }

    HttpResponsePtr HttpClient::put(const std::string& url,
                                    const std::string& body,
                                    const HttpRequestArgsPtr args)
    {
        return request(url, kPut, body, args);
    }

    HttpResponsePtr HttpClient::patch(const std::string& url,
                                      const HttpParameters& httpParameters,
                                      const HttpFormDataParameters& httpFormDataParameters,
                                      HttpRequestArgsPtr args)
    {
        return request(url, kPatch, httpParameters, httpFormDataParameters, args);
    }

    HttpResponsePtr HttpClient::patch(const std::string& url,
                                      const std::string& body,
                                      const HttpRequestArgsPtr args)
    {
        return request(url, kPatch, body, args);
    }

    std::string HttpClient::urlEncode(const std::string& value)
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
        {
            std::string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped << c;
                continue;
            }

            // Any other characters are percent-encoded
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }

        return escaped.str();
    }

    std::string HttpClient::serializeHttpParameters(const HttpParameters& httpParameters)
    {
        std::stringstream ss;
        size_t count = httpParameters.size();
        size_t i = 0;

        for (auto&& it : httpParameters)
        {
            ss << urlEncode(it.first) << "=" << urlEncode(it.second);

            if (i++ < (count - 1))
            {
                ss << "&";
            }
        }
        return ss.str();
    }

    std::string HttpClient::serializeHttpFormDataParameters(
        const std::string& multipartBoundary,
        const HttpFormDataParameters& httpFormDataParameters,
        const HttpParameters& httpParameters)
    {
        //
        // --AaB03x
        // Content-Disposition: form-data; name="submit-name"

        // Larry
        // --AaB03x
        // Content-Disposition: form-data; name="foo.txt"; filename="file1.txt"
        // Content-Type: text/plain

        // ... contents of file1.txt ...
        // --AaB03x--
        //
        std::stringstream ss;

        for (auto&& it : httpFormDataParameters)
        {
            ss << "--" << multipartBoundary << "\r\n"
               << "Content-Disposition:"
               << " form-data; name=\"" << it.first << "\";"
               << " filename=\"" << it.first << "\""
               << "\r\n"
               << "Content-Type: application/octet-stream"
               << "\r\n"
               << "\r\n"
               << it.second << "\r\n";
        }

        for (auto&& it : httpParameters)
        {
            ss << "--" << multipartBoundary << "\r\n"
               << "Content-Disposition:"
               << " form-data; name=\"" << it.first << "\";"
               << "\r\n"
               << "\r\n"
               << it.second << "\r\n";
        }

        ss << "--" << multipartBoundary << "--\r\n";

        return ss.str();
    }

    void HttpClient::log(const std::string& msg, HttpRequestArgsPtr args)
    {
        if (args->logger)
        {
            args->logger(msg);
        }
    }

    std::string HttpClient::generateMultipartBoundary()
    {
        std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

        static thread_local std::mt19937 generator(std::random_device {}());

        std::shuffle(str.begin(), str.end(), generator);

        return str;
    }
} // namespace ix
