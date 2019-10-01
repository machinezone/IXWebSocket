/*
 *  IXSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocket.h"

#include "IXNetSystem.h"
#include "IXSelectInterrupt.h"
#include "IXSelectInterruptFactory.h"
#include "IXSocketConnect.h"
#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef min
#undef min
#endif

namespace ix
{
    const int Socket::kDefaultPollNoTimeout = -1; // No poll timeout by default
    const int Socket::kDefaultPollTimeout = kDefaultPollNoTimeout;
    const uint64_t Socket::kSendRequest = 1;
    const uint64_t Socket::kCloseRequest = 2;
    constexpr size_t Socket::kChunkSize;

    Socket::Socket(int fd)
        : _sockfd(fd)
        , _selectInterrupt(createSelectInterrupt())
    {
        ;
    }

    Socket::~Socket()
    {
        close();
    }

    PollResultType Socket::poll(bool readyToRead,
                                int timeoutMs,
                                int sockfd,
                                std::shared_ptr<SelectInterrupt> selectInterrupt)
    {
        //
        // We used to use ::select to poll but on Android 9 we get large fds out of
        // ::connect which crash in FD_SET as they are larger than FD_SETSIZE. Switching
        // to ::poll does fix that.
        //
        // However poll isn't as portable as select and has bugs on Windows, so we
        // should write a shim to fallback to select on those platforms. See
        // https://github.com/mpv-player/mpv/pull/5203/files for such a select wrapper.
        //
        nfds_t nfds = 1;
        struct pollfd fds[2];

        fds[0].fd = sockfd;
        fds[0].events = (readyToRead) ? POLLIN : POLLOUT;
        fds[0].events |= POLLERR;

        // File descriptor used to interrupt select when needed
        int interruptFd = -1;
        if (selectInterrupt)
        {
            interruptFd = selectInterrupt->getFd();

            if (interruptFd != -1)
            {
                nfds = 2;
                fds[1].fd = interruptFd;
                fds[1].events = POLLIN;
            }
        }

        int ret = ix::poll(fds, nfds, timeoutMs);

        PollResultType pollResult = PollResultType::ReadyForRead;
        if (ret < 0)
        {
            pollResult = PollResultType::Error;
        }
        else if (ret == 0)
        {
            pollResult = PollResultType::Timeout;
        }
        else if (interruptFd != -1 && fds[1].revents & POLLIN)
        {
            uint64_t value = selectInterrupt->read();

            if (value == kSendRequest)
            {
                pollResult = PollResultType::SendRequest;
            }
            else if (value == kCloseRequest)
            {
                pollResult = PollResultType::CloseRequest;
            }
        }
        else if (sockfd != -1 && readyToRead && fds[0].revents & POLLIN)
        {
            pollResult = PollResultType::ReadyForRead;
        }
        else if (sockfd != -1 && !readyToRead && fds[0].revents & POLLOUT)
        {
            pollResult = PollResultType::ReadyForWrite;

#ifdef _WIN32
            // On connect error, in async mode, windows will write to the exceptions fds
            if (fds[0].revents & POLLERR)
            {
                pollResult = PollResultType::Error;
            }
#else
            int optval = -1;
            socklen_t optlen = sizeof(optval);

            // getsockopt() puts the errno value for connect into optval so 0
            // means no-error.
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1 || optval != 0)
            {
                pollResult = PollResultType::Error;

                // set errno to optval so that external callers can have an
                // appropriate error description when calling strerror
                errno = optval;
            }
#endif
        }

        return pollResult;
    }

    PollResultType Socket::isReadyToRead(int timeoutMs)
    {
        if (_sockfd == -1)
        {
            return PollResultType::Error;
        }

        bool readyToRead = true;
        return poll(readyToRead, timeoutMs, _sockfd, _selectInterrupt);
    }

    PollResultType Socket::isReadyToWrite(int timeoutMs)
    {
        if (_sockfd == -1)
        {
            return PollResultType::Error;
        }

        bool readyToRead = false;
        return poll(readyToRead, timeoutMs, _sockfd, _selectInterrupt);
    }

    // Wake up from poll/select by writing to the pipe which is watched by select
    bool Socket::wakeUpFromPoll(uint64_t wakeUpCode)
    {
        return _selectInterrupt->notify(wakeUpCode);
    }

    bool Socket::accept(std::string& errMsg)
    {
        if (_sockfd == -1)
        {
            errMsg = "Socket is uninitialized";
            return false;
        }
        return true;
    }

    bool Socket::connect(const std::string& host,
                         int port,
                         std::string& errMsg,
                         const CancellationRequest& isCancellationRequested)
    {
        std::lock_guard<std::mutex> lock(_socketMutex);

        if (!_selectInterrupt->clear()) return false;

        _sockfd = SocketConnect::connect(host, port, errMsg, isCancellationRequested);
        return _sockfd != -1;
    }

    void Socket::close()
    {
        std::lock_guard<std::mutex> lock(_socketMutex);

        if (_sockfd == -1) return;

        closeSocket(_sockfd);
        _sockfd = -1;
    }

    ssize_t Socket::send(char* buffer, size_t length)
    {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif

        return ::send(_sockfd, buffer, length, flags);
    }

    ssize_t Socket::send(const std::string& buffer)
    {
        return send((char*) &buffer[0], buffer.size());
    }

    ssize_t Socket::recv(void* buffer, size_t length)
    {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif

        return ::recv(_sockfd, (char*) buffer, length, flags);
    }

    int Socket::getErrno()
    {
        int err;

#ifdef _WIN32
        err = WSAGetLastError();
#else
        err = errno;
#endif

        return err;
    }

    bool Socket::isWaitNeeded()
    {
        int err = getErrno();

        if (err == EWOULDBLOCK || err == EAGAIN || err == EINPROGRESS)
        {
            return true;
        }

        return false;
    }

    void Socket::closeSocket(int fd)
    {
#ifdef _WIN32
        closesocket(fd);
#else
        ::close(fd);
#endif
    }

    bool Socket::init(std::string& errorMsg)
    {
        return _selectInterrupt->init(errorMsg);
    }

    bool Socket::writeBytes(const std::string& str,
                            const CancellationRequest& isCancellationRequested)
    {
        int offset = 0;
        int len = (int) str.size();

        while (true)
        {
            if (isCancellationRequested && isCancellationRequested()) return false;

            ssize_t ret = send((char*) &str[offset], len);

            // We wrote some bytes, as needed, all good.
            if (ret > 0)
            {
                if (ret == len)
                {
                    return true;
                }
                else
                {
                    offset += ret;
                    len -= ret;
                    continue;
                }
            }
            // There is possibly something to be writen, try again
            else if (ret < 0 && Socket::isWaitNeeded())
            {
                continue;
            }
            // There was an error during the write, abort
            else
            {
                return false;
            }
        }
    }

    bool Socket::readByte(void* buffer, const CancellationRequest& isCancellationRequested)
    {
        while (true)
        {
            if (isCancellationRequested && isCancellationRequested()) return false;

            ssize_t ret;
            ret = recv(buffer, 1);

            // We read one byte, as needed, all good.
            if (ret == 1)
            {
                return true;
            }
            // There is possibly something to be read, try again
            else if (ret < 0 && Socket::isWaitNeeded())
            {
                // Wait with a 1ms timeout until the socket is ready to read.
                // This way we are not busy looping
                if (isReadyToRead(1) == PollResultType::Error)
                {
                    return false;
                }
            }
            // There was an error during the read, abort
            else
            {
                return false;
            }
        }
    }

    std::pair<bool, std::string> Socket::readLine(
        const CancellationRequest& isCancellationRequested)
    {
        char c;
        std::string line;
        line.reserve(64);

        for (int i = 0; i < 2 || (line[i - 2] != '\r' && line[i - 1] != '\n'); ++i)
        {
            if (!readByte(&c, isCancellationRequested))
            {
                // Return what we were able to read
                return std::make_pair(false, line);
            }

            line += c;
        }

        return std::make_pair(true, line);
    }

    std::pair<bool, std::string> Socket::readBytes(
        size_t length,
        const OnProgressCallback& onProgressCallback,
        const CancellationRequest& isCancellationRequested)
    {
        if (_readBuffer.empty())
        {
            _readBuffer.resize(kChunkSize);
        }

        std::vector<uint8_t> output;
        while (output.size() != length)
        {
            if (isCancellationRequested && isCancellationRequested())
            {
                return std::make_pair(false, std::string());
            }

            size_t size = std::min(kChunkSize, length - output.size());
            ssize_t ret = recv((char*) &_readBuffer[0], size);

            if (ret > 0)
            {
                output.insert(output.end(), _readBuffer.begin(), _readBuffer.begin() + ret);
            }
            else if (ret <= 0 && !Socket::isWaitNeeded())
            {
                return std::make_pair(false, std::string());
            }

            if (onProgressCallback) onProgressCallback((int) output.size(), (int) length);

            // Wait with a 1ms timeout until the socket is ready to read.
            // This way we are not busy looping
            if (isReadyToRead(1) == PollResultType::Error)
            {
                return std::make_pair(false, std::string());
            }
        }

        return std::make_pair(true, std::string(output.begin(), output.end()));
    }
} // namespace ix
