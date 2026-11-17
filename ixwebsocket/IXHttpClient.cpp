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
        if (text.empty()) return false;
        value = 0;
        for (unsigned char c : text)
        {
            unsigned digit;
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (c >= 'a' && c <= 'f')
                digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                digit = c - 'A' + 10;
            else
                return false;
            if (digit >= base || value > ((std::numeric_limits<size_t>::max)() - digit) / base)
                return false;
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
        if (!_async) return;

        _thread = std::thread(&HttpClient::run, this);
    }

    HttpClient::~HttpClient()
    {
        if (!_thread.joinable()) return;

        _stop = true;
        _condition.notify_one();
        _thread.join();
    }

    void HttpClient::setTLSOptions(const SocketTLSOptions& tlsOptions)
    {
        _tlsOptions = tlsOptions;
    }

    void HttpClient::setProxyOptions(const ProxyOptions& proxyOptions)
    {
        _proxyOptions = proxyOptions;
    }

    void HttpClient::setForceBody(bool value)
    {
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
        if (!_async || !args) return false;

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

                if (_stop) return;

                task = std::move(_queue.front());
                _queue.pop();
            }

            if (_stop) return;

            task();

            if (_stop) return;
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
                                            int redirects)
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

        bool tls = protocol == "https";
        std::string errorMsg;
        _socket = createSocket(tls, -1, errorMsg, _tlsOptions, _proxyOptions);

        if (!_socket)
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

        if (verb == kPost || verb == kPut || verb == kPatch || _forceBody)
        {
            // Set request compression header
#ifdef IXWEBSOCKET_USE_ZLIB
            if (args->compressRequest)
            {
                ss << "Content-Encoding: gzip"
                   << "\r\n";
            }
#endif

            ss << "Content-Length: " << body.size() << "\r\n";

            // Set default Content-Type if unspecified
            if (args->extraHeaders.find("Content-Type") == args->extraHeaders.end())
            {
                if (args->multipartBoundary.empty())
                {
                    ss << "Content-Type: application/x-www-form-urlencoded"
                       << "\r\n";
                }
                else
                {
                    ss << "Content-Type: multipart/form-data; boundary=" << args->multipartBoundary
                       << "\r\n";
                }
            }
            ss << "\r\n";
            ss << body;
        }
        else
        {
            ss << "\r\n";
        }

        std::string req(ss.str());
        std::string errMsg;

        // Make a cancellation object dealing with connection timeout
        auto cancelled = makeCancellationRequestWithTimeout(args->connectTimeout, cancel);

        auto isCancellationRequested = [&]() {
            return cancelled() || _stop;
        };

        bool success = _socket->connect(host, port, errMsg, isCancellationRequested);
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

        if (!_socket->writeBytes(req, isCancellationRequested))
        {
            auto errorCode = cancel ? HttpErrorCode::Cancelled : HttpErrorCode::SendError;
            std::string msg("Cannot send request");
            return std::make_shared<HttpResponse>(
                code, description, errorCode, headers, payload, msg, uploadSize, downloadSize);
        }

        uploadSize = req.size();

        auto lineResult = _socket->readLine(isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            auto errorCode =
                cancel ? HttpErrorCode::Cancelled : HttpErrorCode::CannotReadStatusLine;
            std::string msg("Cannot retrieve status line");
            return std::make_shared<HttpResponse>(
                code, description, errorCode, headers, payload, msg, uploadSize, downloadSize);
        }

        if (args->verbose)
        {
            std::ostringstream oss;
            oss << "Status line " << line;
            log(oss.str(), args);
        }

        if (sscanf(line.c_str(), "HTTP/1.1 %d", &code) != 1)
        {
            std::string msg("Cannot parse response code from status line");
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  HttpErrorCode::MissingStatus,
                                                  headers,
                                                  payload,
                                                  msg,
                                                  uploadSize,
                                                  downloadSize);
        }

        auto result = parseHttpHeaders(_socket, isCancellationRequested);
        auto headersValid = result.first;
        headers = result.second;

        if (!headersValid)
        {
            auto errorCode = cancel ? HttpErrorCode::Cancelled : HttpErrorCode::HeaderParsingError;
            std::string msg("Cannot parse http headers");
            return std::make_shared<HttpResponse>(
                code, description, errorCode, headers, payload, msg, uploadSize, downloadSize);
        }

        // These responses never contain a message body, even with representation metadata.
        if (code == 204 || code == 304)
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  HttpErrorCode::Ok,
                                                  headers,
                                                  payload,
                                                  std::string(),
                                                  uploadSize,
                                                  0);

        // Only statuses that define an automatic redirect are followed.
        if ((code == 301 || code == 302 || code == 303 || code == 307 || code == 308) &&
            args->followRedirects)
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

            // Recurse
            std::string location = headers["Location"];
            if (code == 303 && verb != kHead)
            {
                auto redirected = snapshotRequest(*args);
                redirected->extraHeaders.erase("Content-Length");
                redirected->extraHeaders.erase("Content-Type");
                redirected->extraHeaders.erase("Content-Encoding");
                redirected->multipartBoundary.clear();
                redirected->compressRequest = false;
                return requestImpl(
                    location, kGet, std::string(), redirected, cancel, redirects + 1);
            }
            return requestImpl(location, verb, body, args, cancel, redirects + 1);
        }

        if (verb == "HEAD")
        {
            return std::make_shared<HttpResponse>(code,
                                                  description,
                                                  HttpErrorCode::Ok,
                                                  headers,
                                                  payload,
                                                  std::string(),
                                                  uploadSize,
                                                  downloadSize);
        }

        auto failure = [&](HttpErrorCode error, const std::string& message)
        {
            if (cancel) error = HttpErrorCode::Cancelled;
            return std::make_shared<HttpResponse>(
                code, description, error, headers, payload, message, uploadSize, downloadSize);
        };

        // Transfer-Encoding determines framing in preference to Content-Length.
        if (headers.find("Transfer-Encoding") != headers.end())
        {
            const auto& coding = headers.at("Transfer-Encoding");
            if (CaseInsensitiveLess::cmp(coding, "chunked") ||
                CaseInsensitiveLess::cmp("chunked", coding))
                return failure(HttpErrorCode::HeaderParsingError, "Unsupported Transfer-Encoding");
            while (true)
            {
                lineResult = _socket->readLine(isCancellationRequested);
                if (!lineResult.first)
                    return failure(HttpErrorCode::ChunkReadError, "Cannot read chunk size");
                size_t chunkSize = 0;
                const auto sizeText =
                    lineResult.second.substr(0, lineResult.second.find_first_of(";\r\n"));
                if (!parseSize(sizeText, 16, chunkSize))
                    return failure(HttpErrorCode::ChunkReadError, "Invalid chunk size");
                if (chunkSize == 0)
                {
                    // Trailers are a separate header block; do not overwrite framing headers.
                    if (!parseHttpHeaders(_socket, isCancellationRequested).first)
                        return failure(HttpErrorCode::ChunkReadError, "Invalid chunk trailers");
                    break;
                }
                auto chunk = _socket->readBytes(chunkSize,
                                                args->onProgressCallback,
                                                args->onChunkCallback,
                                                isCancellationRequested);
                if (!chunk.first)
                    return failure(HttpErrorCode::ChunkReadError, "Cannot read chunk");
                lineResult = _socket->readLine(isCancellationRequested);
                if (!lineResult.first || (lineResult.second != "\r\n" && lineResult.second != "\n"))
                    return failure(HttpErrorCode::ChunkReadError, "Invalid chunk terminator");
                downloadSize += chunkSize;
                if (!args->onChunkCallback) payload += chunk.second;
            }
        }
        else if (headers.find("Content-Length") != headers.end())
        {
            size_t contentLength = 0;
            if (!parseSize(headers.at("Content-Length"), 10, contentLength))
                return failure(HttpErrorCode::HeaderParsingError, "Invalid Content-Length");
            auto chunk = _socket->readBytes(contentLength,
                                            args->onProgressCallback,
                                            args->onChunkCallback,
                                            isCancellationRequested);
            if (!chunk.first) return failure(HttpErrorCode::ChunkReadError, "Cannot read body");
            downloadSize = contentLength;
            if (!args->onChunkCallback) payload = std::move(chunk.second);
        }
        else
        {
            return failure(HttpErrorCode::CannotReadBody, "Cannot read http body");
        }

        // If the content was compressed with gzip, decode it
        if (headers["Content-Encoding"] == "gzip")
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
