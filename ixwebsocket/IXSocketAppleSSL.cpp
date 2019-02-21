/*
 *  IXSocketAppleSSL.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 *
 *  Adapted from Satori SDK Apple SSL code.
 */
#include "IXSocketAppleSSL.h"
#include "IXSocketConnect.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#include <iostream>

#include <errno.h>
#define socketerrno errno

#include <Security/SecureTransport.h>

namespace {

OSStatus read_from_socket(SSLConnectionRef connection, void *data, size_t *len)
{
    int fd = (int) (long) connection;
    if (fd < 0)
        return errSSLInternal;

    assert(data != nullptr);
    assert(len != nullptr);

    size_t requested_sz = *len;

    ssize_t status = read(fd, data, requested_sz);

    if (status > 0)
    {
        *len = (size_t) status;
        if (requested_sz > *len)
            return errSSLWouldBlock;
        else
            return noErr;
    }
    else if (0 == status)
    {
        *len = 0;
        return errSSLClosedGraceful;
    }
    else
    {
        *len = 0;
        switch (errno) {
            case ENOENT:
                return errSSLClosedGraceful;

            case EAGAIN:
                return errSSLWouldBlock;

            case ECONNRESET:
                return errSSLClosedAbort;

            default:
                return errSecIO;
        }
    }
}

OSStatus write_to_socket(SSLConnectionRef connection, const void *data, size_t *len)
{
    int fd = (int) (long) connection;
    if (fd < 0)
        return errSSLInternal;

    assert(data != nullptr);
    assert(len != nullptr);

    size_t to_write_sz = *len;
    ssize_t status = write(fd, data, to_write_sz);

    if (status > 0)
    {
        *len = (size_t) status;
        if (to_write_sz > *len)
            return errSSLWouldBlock;
        else
            return noErr;
    }
    else if (0 == status)
    {
        *len = 0;
        return errSSLClosedGraceful;
    }
    else
    {
        *len = 0;
        if (EAGAIN == errno)
        {
            return errSSLWouldBlock;
        }
        else
        {
            return errSecIO;
        }
    }
}

std::string getSSLErrorDescription(OSStatus status)
{
    std::string errMsg("Unknown SSL error.");

    CFErrorRef error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL);
    if (error)
    {
        CFStringRef message = CFErrorCopyDescription(error);
        if (message)
        {
            char localBuffer[128];
            Boolean success;
            success = CFStringGetCString(message, localBuffer, 128,
                                         CFStringGetSystemEncoding());
            if (success)
            {
                errMsg = localBuffer;
            }
            CFRelease(message);
        }
        CFRelease(error);
    }

    return errMsg;
}

} // anonymous namespace

namespace ix
{
    SocketAppleSSL::SocketAppleSSL(int fd) : Socket(fd),
        _sslContext(nullptr)
    {
        ;
    }

    SocketAppleSSL::~SocketAppleSSL()
    {
        SocketAppleSSL::close();
    }

    // No wait support
    bool SocketAppleSSL::connect(const std::string& host,
                                 int port,
                                 std::string& errMsg,
                                 const CancellationRequest& isCancellationRequested)
    {
        OSStatus status;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            _sockfd = SocketConnect::connect(host, port, errMsg, isCancellationRequested);
            if (_sockfd == -1) return false;

            _sslContext = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);

            SSLSetIOFuncs(_sslContext, read_from_socket, write_to_socket);
            SSLSetConnection(_sslContext, (SSLConnectionRef) (long) _sockfd);
            SSLSetProtocolVersionMin(_sslContext, kTLSProtocol12);
            SSLSetPeerDomainName(_sslContext, host.c_str(), host.size());

            do {
                status = SSLHandshake(_sslContext);
            } while (errSSLWouldBlock == status ||
                     errSSLServerAuthCompleted == status);
        }

        if (noErr != status)
        {
            errMsg = getSSLErrorDescription(status);
            close();
            return false;
        }

        return true;
    }

    void SocketAppleSSL::close()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_sslContext == nullptr) return;

        SSLClose(_sslContext);
        CFRelease(_sslContext);
        _sslContext = nullptr;

        Socket::close();
    }

    ssize_t SocketAppleSSL::send(char* buf, size_t nbyte)
    {
        ssize_t ret = 0;
        OSStatus status;
        do {
            size_t processed = 0;
            std::lock_guard<std::mutex> lock(_mutex);
            status = SSLWrite(_sslContext, buf, nbyte, &processed);
            ret += processed;
            buf += processed;
            nbyte -= processed;
        } while (nbyte > 0 && errSSLWouldBlock == status);

        if (ret == 0 && errSSLClosedAbort != status)
            ret = -1;
        return ret;
    }

    ssize_t SocketAppleSSL::send(const std::string& buffer)
    {
        return send((char*)&buffer[0], buffer.size());
    }

    // No wait support
    ssize_t SocketAppleSSL::recv(void* buf, size_t nbyte)
    {
        OSStatus status = errSSLWouldBlock;
        while (errSSLWouldBlock == status)
        {
            size_t processed = 0;
            std::lock_guard<std::mutex> lock(_mutex);
            status = SSLRead(_sslContext, buf, nbyte, &processed);

            if (processed > 0)
                return (ssize_t) processed;

            // The connection was reset, inform the caller that this
            // Socket should close
            if (status == errSSLClosedGraceful ||
                status == errSSLClosedNoNotify ||
                status == errSSLClosedAbort)
            {
                errno = ECONNRESET;
                return -1;
            }

            if (status == errSSLWouldBlock)
            {
                errno = EWOULDBLOCK;
                return -1;
            }
        }
        return -1;
    }

}
