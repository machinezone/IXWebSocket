/* Copyright (c) 2026 Machine Zone, Inc. All rights reserved. */
#include "IXSocketHttp2.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    uint32_t number(const std::string& data, size_t offset = 0)
    {
        uint32_t value = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            value = (value << 8) | static_cast<unsigned char>(data[offset + i]);
        }
        return value;
    }

    std::string number(uint32_t value)
    {
        std::string data(4, '\0');
        for (int i = 3; i >= 0; --i)
        {
            data[i] = char(value);
            value >>= 8;
        }
        return data;
    }

    bool validName(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }
        for (unsigned char c : name)
        {
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                (c && std::strchr("!#$%&'*+-.^_`|~", c)))
            {
                continue;
            }
            return false;
        }
        return true;
    }

    bool validValue(const std::string& value)
    {
        for (unsigned char c : value)
        {
            if (!c || c == '\r' || c == '\n')
            {
                return false;
            }
        }
        return value.empty() || (value.front() != ' ' && value.front() != '\t' &&
                                 value.back() != ' ' && value.back() != '\t');
    }
} // namespace

namespace ix
{
    SocketHttp2::SocketHttp2(std::unique_ptr<Socket> socket)
        : _inner(std::move(socket))
    {
    }

    SocketHttp2::~SocketHttp2()
    {
        close();
    }

    bool SocketHttp2::fail(const std::string& message, uint32_t code)
    {
        if (_error.empty())
        {
            _error = "HTTP/2: " + message;
            if (_output.size() < 64)
            {
                Output output;
                output.frame =
                    std::string("\0\0\10\7\0\0\0\0\0", 9) + number(uint32_t(0)) + number(code);
                _output.push_back(std::move(output));
            }
        }
        return false;
    }

    bool SocketHttp2::queue(
        unsigned type, unsigned flags, uint32_t stream, const std::string& payload, size_t accepted)
    {
        // Bound pending control traffic even if a malicious peer floods PING/SETTINGS.
        if (_output.size() >= 64)
        {
            return fail("outbound control queue limit exceeded");
        }
        Output output;
        output.frame.resize(9);
        output.frame[0] = char(payload.size() >> 16);
        output.frame[1] = char(payload.size() >> 8);
        output.frame[2] = char(payload.size());
        output.frame[3] = char(type);
        output.frame[4] = char(flags);
        output.frame.replace(5, 4, number(stream));
        output.frame += payload;
        output.accepted = accepted;
        _output.push_back(std::move(output));
        return true;
    }

    bool SocketHttp2::flush()
    {
        while (!_output.empty())
        {
            auto& output = _output.front();
            const auto count =
                _inner->send(&output.frame[0] + output.offset, output.frame.size() - output.offset);
            if (count < 0 && Socket::isWaitNeeded())
            {
                return true;
            }
            if (count <= 0)
            {
                return fail("cannot write frame");
            }
            output.offset += static_cast<size_t>(count);
            if (output.offset == output.frame.size())
            {
                if (output.accepted)
                {
                    _accepted = output.accepted;
                    _dataPending = false;
                    _inner->wakeUpFromPoll(SelectInterrupt::kSendRequest);
                }
                _output.pop_front();
            }
        }
        return true;
    }

    bool SocketHttp2::headers()
    {
        Hpack::Headers fields;
        if (!_hpack.decode(_headerBlock, fields))
        {
            return fail("invalid HPACK header block", 9);
        }
        _headerBlock.clear();
        bool ordinary = false;
        int status = 0;
        WebSocketHttpHeaders headers;
        for (const auto& field : fields)
        {
            if (!validValue(field.second))
            {
                return fail("invalid response header value");
            }
            if (field.first == ":status" && !_opened && !ordinary && !status)
            {
                if (field.second.size() != 3)
                {
                    return fail("invalid status");
                }
                for (char digit : field.second)
                {
                    if (digit < '0' || digit > '9')
                    {
                        return fail("invalid status");
                    }
                    status = status * 10 + digit - '0';
                }
                if (status < 100 || status > 599 || status == 101)
                {
                    return fail("invalid HTTP/2 status");
                }
                continue;
            }
            ordinary = true;
            if (!validName(field.first) || field.first == "connection" ||
                field.first == "upgrade" || field.first == "keep-alive" ||
                field.first == "proxy-connection" || field.first == "transfer-encoding" ||
                field.first == "te")
            {
                return fail("invalid response header name");
            }
            if (headers.count(field.first))
            {
                headers[field.first] += ", " + field.second;
            }
            else
            {
                headers[field.first] = field.second;
            }
        }
        if (_opened)
        {
            if (!_headerEndStream)
            {
                return fail("trailers must end the stream");
            }
        }
        else
        {
            if (!status)
            {
                return fail("missing status");
            }
            if (status < 200)
            {
                return !_headerEndStream || fail("informational response ended stream");
            }
            _headers = std::move(headers);
            _status = status;
            _opened = status >= 200 && status < 300;
            if (_opened && (_headerEndStream || _headers.count("content-length")))
            {
                return fail("invalid successful CONNECT response");
            }
        }
        _ended = _headerEndStream;
        return true;
    }

    void SocketHttp2::credit(size_t count)
    {
        if (!count || _ended)
        {
            return;
        }
        _receiveConnection += count;
        _receiveStream += count;
        queue(8, 0, 0, number(static_cast<uint32_t>(count)));
        queue(8, 0, 1, number(static_cast<uint32_t>(count)));
    }

    bool SocketHttp2::frame(unsigned type,
                            unsigned flags,
                            uint32_t stream,
                            const std::string& payload)
    {
        if (_firstFrame && (type != 4 || (flags & 1)))
        {
            return fail("missing initial SETTINGS");
        }
        _firstFrame = false;
        if (_headerActive && (type != 9 || stream != 1))
        {
            return fail("interleaved header block");
        }
        if (type == 4)
        {
            if (stream || (flags & 1 ? !payload.empty() : payload.size() % 6 != 0))
            {
                return fail("invalid SETTINGS", stream ? 1 : 6);
            }
            if (flags & 1)
            {
                if (!_awaitingAck)
                {
                    return fail("unexpected SETTINGS acknowledgement");
                }
                _awaitingAck = false;
                return true;
            }
            for (size_t i = 0; i < payload.size(); i += 6)
            {
                const unsigned id = (static_cast<unsigned char>(payload[i]) << 8) |
                                    static_cast<unsigned char>(payload[i + 1]);
                const uint32_t value = number(payload, i + 2);
                switch (id)
                {
                    case 2: return fail("server sent ENABLE_PUSH");
                    case 3: _maxConcurrent = value; break;
                    case 4:
                        if (value > 0x7fffffff)
                        {
                            return fail("invalid initial window", 3);
                        }
                        _sendStream += int64_t(value) - _initialWindow;
                        if (_sendStream > 0x7fffffff)
                        {
                            return fail("stream window overflow", 3);
                        }
                        _initialWindow = value;
                        break;
                    case 5:
                        if (value < 16384 || value > 0xffffff)
                        {
                            return fail("invalid frame size setting");
                        }
                        break;
                    case 6: _peerHeaderLimit = value; break;
                    case 8:
                        if (value > 1 || (_extendedConnect && !value))
                        {
                            return fail("invalid extended CONNECT setting");
                        }
                        _extendedConnect = value != 0;
                        break;
                    default: break;
                }
            }
            _settingsReceived = true;
            return queue(4, 1, 0, "");
        }
        if (type == 6)
        {
            if (stream || payload.size() != 8)
            {
                return fail("invalid PING", stream ? 1 : 6);
            }
            return (flags & 1) || queue(6, 1, 0, payload);
        }
        if (type == 7)
        {
            if (stream || payload.size() < 8)
            {
                return fail("invalid GOAWAY", stream ? 1 : 6);
            }
            if (!_requestSent || (number(payload) & 0x7fffffff) < 1 || number(payload, 4))
            {
                return fail("peer refused or terminated the connection");
            }
            return true; // Stream 1 may finish while the connection drains.
        }
        if (type == 8)
        {
            if (payload.size() != 4 || (stream && (stream != 1 || !_requestSent)))
            {
                return fail("invalid WINDOW_UPDATE", payload.size() != 4 ? 6 : 1);
            }
            const uint32_t count = number(payload) & 0x7fffffff;
            auto& window = stream ? _sendStream : _sendConnection;
            if (!count || window + count > 0x7fffffff)
            {
                return fail("invalid flow-control credit", count ? 3 : 1);
            }
            window += count;
            _inner->wakeUpFromPoll(SelectInterrupt::kSendRequest);
            return true;
        }
        if (type == 2)
        {
            if (!stream || payload.size() != 5 || (number(payload) & 0x7fffffff) == stream)
            {
                return fail("invalid PRIORITY", payload.size() != 5 ? 6 : 1);
            }
            return true;
        }
        if (type == 5)
        {
            return fail("server push is disabled");
        }
        if (type == 3)
        {
            if (stream != 1 || !_requestSent || payload.size() != 4)
            {
                return fail("invalid RST_STREAM", payload.size() != 4 ? 6 : 1);
            }
            _ended = _reset = true;
            return number(payload) == 0 || fail("peer reset the stream");
        }
        if (type != 0 && type != 1 && type != 9)
        {
            return true;
        } // Extension frame.
        if (stream != 1 || !_requestSent)
        {
            return fail("frame on an idle or unexpected stream");
        }
        if (_ended)
        {
            return fail("frame after END_STREAM");
        }
        size_t begin = 0, end = payload.size();
        if (type != 9 && (flags & 8))
        {
            if (payload.empty() || static_cast<unsigned char>(payload[0]) >= payload.size())
            {
                return fail("invalid padding");
            }
            begin = 1;
            end -= static_cast<unsigned char>(payload[0]);
        }
        if (type == 0)
        {
            if (!_opened)
            {
                // An unsuccessful CONNECT may carry an ordinary HTTP error body.
                // This dedicated connection will be discarded by the caller.
                if (_status >= 300)
                {
                    _ended = (flags & 1) != 0;
                    return true;
                }
                return fail("DATA before successful CONNECT");
            }
            _receiveConnection -= payload.size();
            _receiveStream -= payload.size();
            if (_receiveConnection < 0 || _receiveStream < 0)
            {
                return fail("receive window exceeded", 3);
            }
            if (_dataOffset)
            {
                _data.erase(0, _dataOffset);
                _dataOffset = 0;
            }
            _data.append(payload, begin, end - begin);
            credit(payload.size() - (end - begin));
            if (flags & 1)
            {
                _ended = true;
            }
            return true;
        }
        if (type == 1)
        {
            if (_headerActive)
            {
                return fail("nested HEADERS");
            }
            if (flags & 32)
            {
                if (end - begin < 5 || (number(payload, begin) & 0x7fffffff) == stream)
                {
                    return fail("invalid HEADERS priority", end - begin < 5 ? 6 : 1);
                }
                begin += 5;
            }
            _headerEndStream = (flags & 1) != 0;
            _headerActive = true;
        }
        else if (!_headerActive)
        {
            return fail("unexpected CONTINUATION");
        }
        if (end - begin > 65536 - _headerBlock.size())
        {
            return fail("header block too large", 11);
        }
        _headerBlock.append(payload, begin, end - begin);
        if (flags & 4)
        {
            _headerActive = false;
            return headers();
        }
        return true;
    }

    bool SocketHttp2::read()
    {
        char buffer[16384];
        const auto count = _inner->recv(buffer, sizeof(buffer));
        if (count < 0 && Socket::isWaitNeeded())
        {
            return true;
        }
        if (count < 0)
        {
            return fail("cannot read frame");
        }
        if (!count)
        {
            _ended = true;
            return _input.empty() || fail("truncated frame");
        }
        _input.append(buffer, static_cast<size_t>(count));
        size_t offset = 0;
        while (_input.size() - offset >= 9)
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(_input.data() + offset);
            const size_t length = (size_t(bytes[0]) << 16) | (size_t(bytes[1]) << 8) | bytes[2];
            if (length > 16384)
            {
                return fail("frame exceeds advertised maximum", 6);
            }
            if (_input.size() - offset < length + 9)
            {
                break;
            }
            const uint32_t stream = number(_input, offset + 5) & 0x7fffffff;
            if (!frame(bytes[3], bytes[4], stream, _input.substr(offset + 9, length)))
            {
                return false;
            }
            offset += length + 9;
        }
        _input.erase(0, offset);
        return flush();
    }

    WebSocketInitResult SocketHttp2::handshake(const Hpack::Headers& fields,
                                               const CancellationRequest& cancelled)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        if (!_inner->writeBytes(preface, cancelled))
        {
            return WebSocketInitResult(false, 0, "Cannot send HTTP/2 preface");
        }
        // Disable server push, and bound the decoded response header list.
        queue(4, 0, 0, std::string("\0\2\0\0\0\0\0\6\0\1\0\0", 12));
        while (_error.empty() && !_status && !_ended)
        {
            if (cancelled && cancelled())
            {
                fail("handshake cancelled or timed out");
                break;
            }
            if (_settingsReceived && !_requestSent)
            {
                if (!_extendedConnect)
                {
                    fail("peer does not enable extended CONNECT", 0);
                    break;
                }
                if (_maxConcurrent)
                {
                    size_t headerSize = 0;
                    for (const auto& field : fields)
                    {
                        headerSize += 32 + field.first.size() + field.second.size();
                    }
                    if (headerSize > _peerHeaderLimit || headerSize > 65536)
                    {
                        fail("request headers exceed peer limit");
                        break;
                    }
                    const std::string block = Hpack::encode(fields);
                    for (size_t offset = 0; offset < block.size(); offset += 16384)
                    {
                        const size_t length = (std::min) (size_t(16384), block.size() - offset);
                        queue(offset ? 9 : 1,
                              offset + length == block.size() ? 4 : 0,
                              1,
                              block.substr(offset, length));
                    }
                    _requestSent = true;
                }
            }
            if (!flush())
            {
                break;
            }
            if (!_output.empty())
            {
                if (_inner->isReadyToWrite(10) == PollResultType::Error)
                {
                    fail("socket write error");
                }
                continue;
            }
            const auto ready = _inner->isReadyToRead(10);
            if (ready == PollResultType::Error || ready == PollResultType::CloseRequest)
            {
                fail("socket closed during handshake");
                break;
            }
            if (ready == PollResultType::ReadyForRead && !read())
            {
                break;
            }
        }
        const bool success = _opened && _error.empty();
        return WebSocketInitResult(success,
                                   _status,
                                   success ? ""
                                           : (_error.empty() ? "HTTP/2 CONNECT rejected (status " +
                                                                   std::to_string(_status) + ")"
                                                             : _error),
                                   _headers);
    }

    std::ptrdiff_t SocketHttp2::send(char* buffer, size_t length)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_closed || !_error.empty() || _reset)
        {
            Socket::setErrno(EINVAL);
            return -1;
        }
        if (!_accepted && (_sendConnection <= 0 || _sendStream <= 0))
        {
            if (!read() || _reset)
            {
                Socket::setErrno(EINVAL);
                return -1;
            }
        }
        if (!_accepted && !_dataPending && length)
        {
            const int64_t count = (std::min) ({int64_t(length > 16384 ? 16384 : length),
                                               _sendConnection,
                                               _sendStream});
            if (count > 0)
            {
                _sendConnection -= count;
                _sendStream -= count;
                _dataPending = true;
                queue(0,
                      0,
                      1,
                      std::string(buffer, static_cast<size_t>(count)),
                      static_cast<size_t>(count));
            }
        }
        if (!flush())
        {
            Socket::setErrno(EINVAL);
            return -1;
        }
        if (_accepted)
        {
            const size_t count = _accepted;
            _accepted = 0;
            return static_cast<std::ptrdiff_t>(count);
        }
        Socket::setErrno(EWOULDBLOCK);
        return -1;
    }

    std::ptrdiff_t SocketHttp2::recv(void* buffer, size_t length)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_dataOffset == _data.size() && !_closed && _error.empty() && !_ended)
        {
            read();
        }
        const size_t count = (std::min) (length, _data.size() - _dataOffset);
        if (count)
        {
            std::memcpy(buffer, _data.data() + _dataOffset, count);
            _dataOffset += count;
            credit(count);
            flush();
            if (_dataOffset == _data.size())
            {
                _data.clear();
                _dataOffset = 0;
                if (_ended)
                {
                    _deferEnd = true;
                }
            }
            return static_cast<std::ptrdiff_t>(count);
        }
        if (_deferEnd)
        {
            // Let the WebSocket parser dispatch the last bytes and send its
            // Close reply before exposing the stream's read-half closure.
            _deferEnd = false;
            Socket::setErrno(EWOULDBLOCK);
            return -1;
        }
        if (_closed || _ended)
        {
            return 0;
        }
        Socket::setErrno(_error.empty() ? EWOULDBLOCK : EINVAL);
        return -1;
    }

    PollResultType SocketHttp2::isReadyToRead(int timeoutMs)
    {
        bool writing, buffered;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_closed || _ended || !_error.empty())
            {
                return PollResultType::ReadyForRead;
            }
            if (!flush())
            {
                return PollResultType::Error;
            }
            if (_accepted)
            {
                return PollResultType::SendRequest;
            }
            buffered = _dataOffset != _data.size();
            writing = !_output.empty();
        }
        if (buffered)
        {
            // Do not starve write/cancel interrupts under continuous inbound DATA.
            const auto ready = _inner->isReadyToRead(0);
            if (ready == PollResultType::SendRequest || ready == PollResultType::CloseRequest)
            {
                return ready;
            }
            return PollResultType::ReadyForRead;
        }
        if (writing)
        {
            const auto ready = _inner->isReadyToWrite(10);
            return ready == PollResultType::ReadyForWrite ? PollResultType::SendRequest : ready;
        }
        return _inner->isReadyToRead(timeoutMs);
    }

    PollResultType SocketHttp2::isReadyToWrite(int timeoutMs)
    {
        bool blocked, buffered, writing;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            buffered = _dataOffset != _data.size();
            if (buffered && (_ended || !_error.empty()))
            {
                return PollResultType::ReadyForRead;
            }
            if (_closed || _reset || !_error.empty())
            {
                return PollResultType::Error;
            }
            // Control frames must make progress even while DATA is blocked by
            // flow control. Async TLS backends may need several send attempts.
            if (!flush())
            {
                return PollResultType::Error;
            }
            if (_accepted)
            {
                return PollResultType::ReadyForWrite;
            }
            writing = !_output.empty();
            blocked = _sendConnection <= 0 || _sendStream <= 0;
            if (blocked && _ended)
            {
                return PollResultType::ReadyForRead;
            }
        }
        if (!blocked || writing)
        {
            const auto ready = _inner->isReadyToWrite(timeoutMs);
            return buffered && ready == PollResultType::Timeout ? PollResultType::ReadyForRead
                                                                : ready;
        }
        if (buffered)
        {
            return PollResultType::ReadyForRead;
        }
        const auto ready = _inner->isReadyToRead(timeoutMs);
        if (ready == PollResultType::ReadyForRead)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!read() || _reset)
            {
                return PollResultType::Error;
            }
            if (_dataOffset != _data.size())
            {
                return PollResultType::ReadyForRead;
            }
            return _accepted || (_sendConnection > 0 && _sendStream > 0)
                       ? PollResultType::ReadyForWrite
                       : PollResultType::Timeout;
        }
        return ready;
    }

    bool SocketHttp2::wakeUpFromPoll(uint64_t code)
    {
        return _inner->wakeUpFromPoll(code);
    }

    bool SocketHttp2::isWakeUpFromPollSupported()
    {
        return _inner->isWakeUpFromPollSupported();
    }

    void SocketHttp2::close()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_closed)
        {
            _closed = true;
            if (_opened && _error.empty() && !_reset)
            {
                queue(0, 1, 1, "");
            }
            flush();
            _inner->close();
        }
    }
} // namespace ix
