/*
 *  IXSocket.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 */

#include "IXSocket.h"
#include "IXSocketConnect.h"
#include "IXNetSystem.h"
#include "IXSelectInterrupt.h"
#include "IXSelectInterruptFactory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include <algorithm>
#include <iostream>

namespace ix
{
    const int Socket::kDefaultPollNoTimeout = -1; // No poll timeout by default
    const int Socket::kDefaultPollTimeout = kDefaultPollNoTimeout;
    const uint64_t Socket::kSendRequest = 1;
    const uint64_t Socket::kCloseRequest = 2;
    constexpr size_t Socket::kChunkSize;

    Socket::Socket(int fd) :
        _sockfd(fd),
        _selectInterrupt(createSelectInterrupt())
    {
        ;
    }

    Socket::~Socket()
    {
        close();
    }

    void Socket::poll(const OnPollCallback& onPollCallback, int timeoutSecs)
    {
        if (_sockfd == -1)
        {
            if (onPollCallback) onPollCallback(PollResultType_Error);
            return;
        }

        PollResultType pollResult = isReadyToRead(1000 * timeoutSecs);

        if (onPollCallback) onPollCallback(pollResult);
    }

    PollResultType Socket::select(bool readyToRead, int timeoutMs)
    {
        fd_set rfds;
        fd_set wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        fd_set* fds = (readyToRead) ? &rfds : & wfds;
        FD_SET(_sockfd, fds);

        // File descriptor used to interrupt select when needed
        int interruptFd = _selectInterrupt->getFd();
        if (interruptFd != -1)
        {
            FD_SET(interruptFd, fds);
        }

        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs < 1000) ? 0 : 1000 * (timeoutMs % 1000);

        // Compute the highest fd.
        int sockfd = _sockfd;
        int nfds = (std::max)(sockfd, interruptFd);

        int ret = ::select(nfds + 1, &rfds, &wfds, nullptr,
                           (timeoutMs < 0) ? nullptr : &timeout);

        PollResultType pollResult = PollResultType_ReadyForRead;
        if (ret < 0)
        {
            pollResult = PollResultType_Error;
        }
        else if (ret == 0)
        {
            pollResult = PollResultType_Timeout;
        }
        else if (interruptFd != -1 && FD_ISSET(interruptFd, &rfds))
        {
            uint64_t value = _selectInterrupt->read();

            if (value == kSendRequest)
            {
                pollResult = PollResultType_SendRequest;
            }
            else if (value == kCloseRequest)
            {
                pollResult = PollResultType_CloseRequest;
            }
        }
        else if (sockfd != -1 && readyToRead && FD_ISSET(sockfd, &rfds))
        {
            pollResult = PollResultType_ReadyForRead;
        }
        else if (sockfd != -1 && !readyToRead && FD_ISSET(sockfd, &wfds))
        {
            pollResult = PollResultType_ReadyForWrite;
        }

        return pollResult;
    }

    PollResultType Socket::isReadyToRead(int timeoutMs)
    {
        bool readyToRead = true;
        return select(readyToRead, timeoutMs);
    }

    PollResultType Socket::isReadyToWrite(int timeoutMs)
    {
        bool readyToRead = false;
        return select(readyToRead, timeoutMs);
    }

    // Wake up from poll/select by writing to the pipe which is watched by select
    bool Socket::wakeUpFromPoll(uint8_t wakeUpCode)
    {
        return _selectInterrupt->notify(wakeUpCode);
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
        return send((char*)&buffer[0], buffer.size());
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
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
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
        while (true)
        {
            if (isCancellationRequested()) return false;

            char* buffer = const_cast<char*>(str.c_str());
            int len = (int) str.size();

            ssize_t ret = send(buffer, len);

            // We wrote some bytes, as needed, all good.
            if (ret > 0)
            {
                return ret == len;
            }
            // There is possibly something to be write, try again
            else if (ret < 0 && (getErrno() == EWOULDBLOCK ||
                                 getErrno() == EAGAIN))
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

    bool Socket::readByte(void* buffer,
                          const CancellationRequest& isCancellationRequested)
    {
        while (true)
        {
            if (isCancellationRequested()) return false;

            ssize_t ret;
            ret = recv(buffer, 1);

            // We read one byte, as needed, all good.
            if (ret == 1)
            {
                return true;
            }
            // There is possibly something to be read, try again
            else if (ret < 0 && (getErrno() == EWOULDBLOCK ||
                                 getErrno() == EAGAIN))
            {
                // Wait with a 1ms timeout until the socket is ready to read.
                // This way we are not busy looping
                if (isReadyToRead(1) == PollResultType_Error)
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

        for (int i = 0; i < 2 || (line[i-2] != '\r' && line[i-1] != '\n'); ++i)
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
            if (isCancellationRequested()) return std::make_pair(false, std::string());

            int size = std::min(kChunkSize, length - output.size());
            ssize_t ret = recv((char*)&_readBuffer[0], size);

            if (ret <= 0 && (getErrno() != EWOULDBLOCK &&
                             getErrno() != EAGAIN))
            {
                // Error
                return std::make_pair(false, std::string());
            }
            else if (ret > 0)
            {
                output.insert(output.end(),
                              _readBuffer.begin(),
                              _readBuffer.begin() + ret);
            }

            if (onProgressCallback) onProgressCallback((int) output.size(), (int) length);

            // Wait with a 1ms timeout until the socket is ready to read.
            // This way we are not busy looping
            if (isReadyToRead(1) == PollResultType_Error)
            {
                return std::make_pair(false, std::string());
            }
        }

        return std::make_pair(true, std::string(output.begin(),
                                                output.end()));
    }
}
