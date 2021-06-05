/*
 *  IXHttpClient.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include "IXHttpClient.h"

#include "IXSocketFactory.h"
#include "IXUrlParser.h"
#include "IXUserAgent.h"
#include "IXWebSocketHttpHeaders.h"
#include <assert.h>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>
#include <zlib.h>

namespace ix
{
    const std::string HttpClient::kPost = "POST";
    const std::string HttpClient::kGet = "GET";
    const std::string HttpClient::kHead = "HEAD";
    const std::string HttpClient::kDel = "DEL";
    const std::string HttpClient::kPut = "PUT";

    HttpClient::HttpClient(bool async)
        : _async(async)
        , _stop(false)
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

    HttpRequestArgsPtr HttpClient::createRequest(const std::string& url, const std::string& verb)
    {
        auto request = std::make_shared<HttpRequestArgs>();
        request->url = url;
        request->verb = verb;
        return request;
    }

	HttpRequestArgsPtr HttpClient::createStreamRequest(std::istream* stream, size_t bufferSize, const std::string& url,
                                                       const std::string& verb)
    {
        auto request = std::make_shared<HttpStreamRequestArgs>(stream);
        request->url = url;
        request->verb = verb;
        request->bufferSize = bufferSize;
        return request;
    }

    bool HttpClient::performRequest(HttpRequestArgsPtr args,
                                    const OnResponseCallback& onResponseCallback)
    {
        assert(_async && "HttpClient needs its async parameter set to true "
                         "in order to call performRequest");
        if (!_async) return false;

        // Enqueue the task
        {
            // acquire lock
            std::unique_lock<std::mutex> lock(_queueMutex);

            // add the task
            _queue.push(std::make_pair(args, onResponseCallback));
        } // release lock

        // wake up one thread
        _condition.notify_one();

        return true;
    }

    void HttpClient::run()
    {
        while (true)
        {
            HttpRequestArgsPtr args;
            OnResponseCallback onResponseCallback;

            {
                std::unique_lock<std::mutex> lock(_queueMutex);

                while (!_stop && _queue.empty())
                {
                    _condition.wait(lock);
                }

                if (_stop) return;

                auto p = _queue.front();
                _queue.pop();

                args = p.first;
                onResponseCallback = p.second;
            }

            if (_stop) return;


            HttpResponsePtr response;
            if (auto streamRequest = dynamic_cast<HttpStreamRequestArgs*>(args.get()))
            {
                response = request(
                    args->url, args->verb, streamRequest->stream, args, streamRequest->bufferSize);
            }
            else
            {
                response = request(args->url, args->verb, args->body, args);
            }
            
            onResponseCallback(response);

            if (_stop) return;
        }
    }

	HttpResponsePtr HttpClient::pre_request(RequestData& data,
                                 const std::string& url,
                                 const std::string& verb,
                                 HttpRequestArgsPtr args,
                                 int redirects)
    {
        // We only have one socket connection, so we cannot
        // make multiple requests concurrently.
        std::lock_guard<std::mutex> lock(_mutex);

        if (!UrlParser::parse(url, data.protocol, data.host, data.path, data.query, data.port))
        {
            std::stringstream ss;
            ss << "Cannot parse url: " << url;
            return std::make_shared<HttpResponse>(data.code,
                                                    data.description,
                                                    HttpErrorCode::UrlMalformed,
                                                    data.headers,
                                                    data.payload,
                                                    ss.str(),
                                                    data.uploadSize,
                                                    data.downloadSize);
        }

        bool tls = data.protocol == "https";
        _socket = createSocket(tls, -1, data.errorMsg, _tlsOptions);

        if (!_socket)
        {
            return std::make_shared<HttpResponse>(data.code,
                                                    data.description,
                                                    HttpErrorCode::CannotCreateSocket,
                                                    data.headers,
                                                    data.payload,
                                                    data.errorMsg,
                                                    data.uploadSize,
                                                    data.downloadSize);
        }

        // Build request string
        data.ss << verb << " " << data.path << " HTTP/1.1\r\n";

        if (data.port == 80 || (tls && data.port == 433))
        {
            data.ss << "Host: " << data.host << "\r\n";
        } else {
            data.ss << "Host: " << data.host << ":" << data.port << "\r\n";
        }

        if (args->compress)
        {
            data.ss << "Accept-Encoding: gzip"
                << "\r\n";
        }

        // Append extra headers
        for (auto&& it : args->extraHeaders)
        {
            data.ss << it.first << ": " << it.second << "\r\n";
        }

        // Set a default Accept header if none is present
        if (data.headers.find("Accept") == data.headers.end())
        {
            data.ss << "Accept: */*"
                << "\r\n";
        }

        // Set a default User agent if none is present
        if (data.headers.find("User-Agent") == data.headers.end())
        {
            data.ss << "User-Agent: " << userAgent() << "\r\n";
        }  

		return nullptr;
	}

	HttpResponsePtr HttpClient::post_request(RequestData& data,
                                             const std::string& url,
                                             const std::string& verb,
                                             HttpRequestArgsPtr args,
                                             int redirects)
    {
        data.uploadSize = data.req.size();

        auto lineResult = _socket->readLine(_isCancellationRequested);
        auto lineValid = lineResult.first;
        auto line = lineResult.second;

        if (!lineValid)
        {
            std::string errorMsg("Cannot retrieve status line");
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::CannotReadStatusLine,
                                                  data.headers,
                                                  data.payload,
                                                  errorMsg,
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        if (args->verbose)
        {
            std::stringstream ss;
            ss << "Status line " << line;
            log(ss.str(), args);
        }

        if (sscanf(line.c_str(), "HTTP/1.1 %d", &data.code) != 1)
        {
            std::string errorMsg("Cannot parse response code from status line");
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::MissingStatus,
                                                  data.headers,
                                                  data.payload,
                                                  errorMsg,
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        auto result = parseHttpHeaders(_socket, _isCancellationRequested);
        auto headersValid = result.first;
        data.headers = result.second;

        if (!headersValid)
        {
            std::string errorMsg("Cannot parse http headers");
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::HeaderParsingError,
                                                  data.headers,
                                                  data.payload,
                                                  errorMsg,
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        // Redirect ?
        if ((data.code >= 301 && data.code <= 308) && args->followRedirects)
        {
            if (data.headers.find("Location") == data.headers.end())
            {
                std::string errorMsg("Missing location header for redirect");
                return std::make_shared<HttpResponse>(data.code,
                                                      data.description,
                                                      HttpErrorCode::MissingLocation,
                                                      data.headers,
                                                      data.payload,
                                                      errorMsg,
                                                      data.uploadSize,
                                                      data.downloadSize);
            }

            if (data.redirects >= args->maxRedirects)
            {
                std::stringstream ss;
                ss << "Too many redirects: " << data.redirects;
                return std::make_shared<HttpResponse>(data.code,
                                                      data.description,
                                                      HttpErrorCode::TooManyRedirects,
                                                      data.headers,
                                                      data.payload,
                                                      ss.str(),
                                                      data.uploadSize,
                                                      data.downloadSize);
            }

            // Recurse
            std::string location = data.headers["Location"];
            return data.redirect();
        }

        if (verb == "HEAD")
        {
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::Ok,
                                                  data.headers,
                                                  data.payload,
                                                  std::string(),
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        // Parse response:
        if (data.headers.find("Content-Length") != data.headers.end())
        {
            ssize_t contentLength = -1;
            data.ss.str("");
            data.ss << data.headers["Content-Length"];
            data.ss >> contentLength;

            data.payload.reserve(contentLength);

            auto chunkResult = _socket->readBytes(
                contentLength, args->onProgressCallback, _isCancellationRequested);
            if (!chunkResult.first)
            {
                data.errorMsg = "Cannot read chunk";
                return std::make_shared<HttpResponse>(data.code,
                                                      data.description,
                                                      HttpErrorCode::ChunkReadError,
                                                      data.headers,
                                                      data.payload,
                                                      data.errorMsg,
                                                      data.uploadSize,
                                                      data.downloadSize);
            }
            data.payload += chunkResult.second;
        }
        else if (data.headers.find("Transfer-Encoding") != data.headers.end() &&
                 data.headers["Transfer-Encoding"] == "chunked")
        {
            std::stringstream ss;

            while (true)
            {
                lineResult = _socket->readLine(_isCancellationRequested);
                line = lineResult.second;

                if (!lineResult.first)
                {
                    return std::make_shared<HttpResponse>(data.code,
                                                          data.description,
                                                          HttpErrorCode::ChunkReadError,
                                                          data.headers,
                                                          data.payload,
                                                          data.errorMsg,
                                                          data.uploadSize,
                                                          data.downloadSize);
                }

                uint64_t chunkSize;
                ss.str("");
                ss << std::hex << line;
                ss >> chunkSize;

                if (args->verbose)
                {
                    std::stringstream oss;
                    oss << "Reading " << chunkSize << " bytes" << std::endl;
                    log(oss.str(), args);
                }

                data.payload.reserve(data.payload.size() + (size_t) chunkSize);

                // Read a chunk
                auto chunkResult = _socket->readBytes(
                    (size_t) chunkSize, args->onProgressCallback, _isCancellationRequested);
                if (!chunkResult.first)
                {
                    data.errorMsg = "Cannot read chunk";
                    return std::make_shared<HttpResponse>(data.code,
                                                          data.description,
                                                          HttpErrorCode::ChunkReadError,
                                                          data.headers,
                                                          data.payload,
                                                          data.errorMsg,
                                                          data.uploadSize,
                                                          data.downloadSize);
                }
                data.payload += chunkResult.second;

                // Read the line that terminates the chunk (\r\n)
                lineResult = _socket->readLine(_isCancellationRequested);

                if (!lineResult.first)
                {
                    return std::make_shared<HttpResponse>(data.code,
                                                          data.description,
                                                          HttpErrorCode::ChunkReadError,
                                                          data.headers,
                                                          data.payload,
                                                          data.errorMsg,
                                                          data.uploadSize,
                                                          data.downloadSize);
                }

                if (chunkSize == 0) break;
            }
        }
        else if (data.code == 204)
        {
            ; // 204 is NoContent response code
        }
        else
        {
            std::string errorMsg("Cannot read http body");
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::CannotReadBody,
                                                  data.headers,
                                                  data.payload,
                                                  errorMsg,
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        data.downloadSize = data.payload.size();

        // If the content was compressed with gzip, decode it
        if (data.headers["Content-Encoding"] == "gzip")
        {
            std::string decompressedPayload;
            if (!gzipInflate(data.payload, decompressedPayload))
            {
                std::string errorMsg("Error decompressing payload");
                return std::make_shared<HttpResponse>(data.code,
                                                      data.description,
                                                      HttpErrorCode::Gzip,
                                                      data.headers,
                                                      data.payload,
                                                      errorMsg,
                                                      data.uploadSize,
                                                      data.downloadSize);
            }
            data.payload = decompressedPayload;
        }

        return std::make_shared<HttpResponse>(data.code,
                                              data.description,
                                              HttpErrorCode::Ok,
                                              data.headers,
                                              data.payload,
                                              std::string(),
                                              data.uploadSize,
                                              data.downloadSize);
    }

	HttpResponsePtr HttpClient::request(const std::string& url,
                                        const std::string& verb,
                                        const std::string& body,
                                        HttpRequestArgsPtr args,
                                        int redirects)
    {
        RequestData data;
        data.redirect = [&] { return request(url, verb, body, args, redirects + 1); };
        auto ret = pre_request(data, url, verb, args, redirects);
        if (ret) return ret;

        if (verb == kPost || verb == kPut)
        {
            data.ss << "Content-Length: " << body.size() << "\r\n";

            // Set default Content-Type if unspecified
            if (args->extraHeaders.find("Content-Type") == args->extraHeaders.end())
            {
                if (args->multipartBoundary.empty())
                {
                    data.ss << "Content-Type: application/x-www-form-urlencoded"
                            << "\r\n";
                }
                else
                {
                    data.ss << "Content-Type: multipart/form-data; boundary="
                            << args->multipartBoundary << "\r\n";
                }
            }
            data.ss << "\r\n";
            data.ss << body;
        }
        else
        {
            data.ss << "\r\n";
        }

        data.req = data.ss.str();
        std::string errMsg;
        std::atomic<bool> requestInitCancellation(false);

        // Make a cancellation object dealing with connection timeout
        _isCancellationRequested =
            makeCancellationRequestWithTimeout(args->connectTimeout, requestInitCancellation);

        bool success = _socket->connect(data.host, data.port, errMsg, _isCancellationRequested);
        if (!success)
        {
            std::stringstream ss;
            ss << "Cannot connect to url: " << url << " / error : " << errMsg;
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::CannotConnect,
                                                  data.headers,
                                                  data.payload,
                                                  ss.str(),
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        // Make a new cancellation object dealing with transfer timeout
        _isCancellationRequested =
            makeCancellationRequestWithTimeout(args->transferTimeout, requestInitCancellation);

        if (args->verbose)
        {
            std::stringstream ss;
            ss << "Sending " << verb << " request "
               << "to " << data.host << ":" << data.port << std::endl
               << "request size: " << data.req.size() << " bytes" << std::endl
               << "=============" << std::endl
               << data.req << "=============" << std::endl
               << std::endl;

            log(ss.str(), args);
        }

        if (!_socket->writeBytes(data.req, _isCancellationRequested))
        {
            std::string errorMsg("Cannot send request");
            return std::make_shared<HttpResponse>(data.code,
                                                  data.description,
                                                  HttpErrorCode::SendError,
                                                  data.headers,
                                                  data.payload,
                                                  errorMsg,
                                                  data.uploadSize,
                                                  data.downloadSize);
        }

        return post_request(data, url, verb, args, redirects);
    }

	HttpResponsePtr HttpClient::request(const std::string& url,
                            const std::string& verb,
                            std::istream* body,
                            HttpRequestArgsPtr args,
							size_t bufferSize,
							int redirects)
    {
        RequestData data;
        data.redirect = [&] { return request(url, verb, body, args, bufferSize, redirects + 1); };
        auto ret = pre_request(data, url, verb, args, redirects);
        if (ret) return ret;

		std::streampos bodySize = 0;
        if (verb == kPost || verb == kPut)
        {
            body->seekg(0, std::ios::end);
            bodySize = body->tellg();
            body->seekg(0, std::ios::beg);
            data.ss << "Content-Length: " << bodySize << "\r\n";

            // Set default Content-Type if unspecified
            if (args->extraHeaders.find("Content-Type") == args->extraHeaders.end())
            {
                if (args->multipartBoundary.empty())
                {
                    data.ss << "Content-Type: application/x-www-form-urlencoded"
                        << "\r\n";
                }
                else
                {
                    data.ss << "Content-Type: multipart/form-data; boundary="
                        << args->multipartBoundary << "\r\n";
                }
            }
            data.ss << "\r\n";
            //ss << body;
        }
        else
        {
            data.ss << "\r\n";
        }

		data.req = data.ss.str();
        std::string errMsg;
        std::atomic<bool> requestInitCancellation(false);

        // Make a cancellation object dealing with connection timeout
        _isCancellationRequested =
            makeCancellationRequestWithTimeout(args->connectTimeout, requestInitCancellation);

        bool success = _socket->connect(data.host, data.port, errMsg, _isCancellationRequested);
        if (!success)
        {
            std::stringstream ss;
            ss << "Cannot connect to url: " << url << " / error : " << errMsg;
            return std::make_shared<HttpResponse>(data.code,
                                                    data.description,
                                                    HttpErrorCode::CannotConnect,
                                                    data.headers,
                                                    data.payload,
                                                    ss.str(),
                                                    data.uploadSize,
                                                    data.downloadSize);
        }

        // Make a new cancellation object dealing with transfer timeout
        _isCancellationRequested =
            makeCancellationRequestWithTimeout(args->transferTimeout, requestInitCancellation);

		if (args->verbose)
        {
            std::stringstream ss;
            ss << "Sending " << verb << " request "
                << "to " << data.host << ":" << data.port << std::endl
                << "request size: " << data.req.size() + bodySize << " bytes" << std::endl
                << "=============" << std::endl
                << data.req << "=============" << std::endl
                << std::endl;

            log(ss.str(), args);
        }

		// Write header first
        if (!_socket->writeBytes(data.req, _isCancellationRequested))
        {
            std::string errorMsg("Cannot send request");
            return std::make_shared<HttpResponse>(data.code,
                                                    data.description,
                                                    HttpErrorCode::SendError,
                                                    data.headers,
                                                    data.payload,
                                                    errorMsg,
                                                    data.uploadSize,
                                                    data.downloadSize);
        }
		// If we have a body, write that next
        if (body)
        {
            std::string buffer;
            buffer.resize(bufferSize);
            body->read(reinterpret_cast<char*>(&buffer[0]), bufferSize);
            while (body->gcount())
            {
                auto size = buffer.size();
                if (!_socket->writeBytes(buffer, _isCancellationRequested))
                {
                    std::string errorMsg("Cannot send request");
                    return std::make_shared<HttpResponse>(data.code,
                                                            data.description,
                                                            HttpErrorCode::SendError,
                                                            data.headers,
                                                            data.payload,
                                                            errorMsg,
                                                            data.uploadSize,
                                                            data.downloadSize);
                }
                if (args->onProgressCallback)
                    args->onProgressCallback(body->eof() ? bodySize : body->tellg(), bodySize);

                body->read(reinterpret_cast<char*>(&buffer[0]), bufferSize);
            }
        }
        // Don't report response progress
        args->onProgressCallback = nullptr;
        return post_request(data, url, verb, args, redirects);
	}

     void HttpClient::cancel()
     {
         // Make a cancellation object dealing with connection timeout
         _requestInitCancellation = true;
         _isCancellationRequested =
             makeCancellationRequestWithTimeout(0, _requestInitCancellation); 
     }

    HttpResponsePtr HttpClient::get(const std::string& url, HttpRequestArgsPtr args)
    {
        return request(url, kGet, std::string(), args);
    }

    HttpResponsePtr HttpClient::head(const std::string& url, HttpRequestArgsPtr args)
    {
        return request(url, kHead, std::string(), args);
    }

    HttpResponsePtr HttpClient::del(const std::string& url, HttpRequestArgsPtr args)
    {
        return request(url, kDel, std::string(), args);
    }

    HttpResponsePtr HttpClient::post(const std::string& url,
                                     const HttpParameters& httpParameters,
                                     HttpRequestArgsPtr args)
    {
        return request(url, kPost, serializeHttpParameters(httpParameters), args);
    }

    HttpResponsePtr HttpClient::post(const std::string& url,
                                     const std::string& body,
                                     HttpRequestArgsPtr args)
    {
        return request(url, kPost, body, args);
    }

    HttpResponsePtr HttpClient::put(const std::string& url,
                                    const HttpParameters& httpParameters,
                                    HttpRequestArgsPtr args)
    {
        return request(url, kPut, serializeHttpParameters(httpParameters), args);
    }

    HttpResponsePtr HttpClient::put(const std::string& url,
                                    const std::string& body,
                                    const HttpRequestArgsPtr args)
    {
        return request(url, kPut, body, args);
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
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
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

    bool HttpClient::gzipInflate(const std::string& in, std::string& out)
    {
        z_stream inflateState;
        std::memset(&inflateState, 0, sizeof(inflateState));

        inflateState.zalloc = Z_NULL;
        inflateState.zfree = Z_NULL;
        inflateState.opaque = Z_NULL;
        inflateState.avail_in = 0;
        inflateState.next_in = Z_NULL;

        if (inflateInit2(&inflateState, 16 + MAX_WBITS) != Z_OK)
        {
            return false;
        }

        inflateState.avail_in = (uInt) in.size();
        inflateState.next_in = (unsigned char*) (const_cast<char*>(in.data()));

        const int kBufferSize = 1 << 14;

        std::unique_ptr<unsigned char[]> compressBuffer =
            std::make_unique<unsigned char[]>(kBufferSize);

        do
        {
            inflateState.avail_out = (uInt) kBufferSize;
            inflateState.next_out = compressBuffer.get();

            int ret = inflate(&inflateState, Z_SYNC_FLUSH);

            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            {
                inflateEnd(&inflateState);
                return false;
            }

            out.append(reinterpret_cast<char*>(compressBuffer.get()),
                       kBufferSize - inflateState.avail_out);
        } while (inflateState.avail_out == 0);

        inflateEnd(&inflateState);
        return true;
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

        static std::random_device rd;
        static std::mt19937 generator(rd());

        std::shuffle(str.begin(), str.end(), generator);

        return str;
    }
} // namespace ix
