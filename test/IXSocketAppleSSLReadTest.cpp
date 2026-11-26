/*
 *  IXSocketAppleSSLReadTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <cerrno>
#include <fcntl.h>
#include <ixwebsocket/IXSocketAppleSSL.h>
#include <sys/socket.h>

namespace ix
{
    struct SocketAppleSSLReadTestAccess
    {
        static OSStatus read(SSLConnectionRef, void*, size_t* size)
        {
            *size = 0;
            return errSecIO;
        }

        static OSStatus write(SSLConnectionRef, const void*, size_t* size)
        {
            *size = 0;
            return errSecIO;
        }

        static OSStatus readBlocked(SSLConnectionRef, void*, size_t* size)
        {
            *size = 0;
            return errSSLWouldBlock;
        }

        static OSStatus writeBlocked(SSLConnectionRef, const void*, size_t* size)
        {
            *size = 0;
            return errSSLWouldBlock;
        }

        static OSStatus handshake(SocketAppleSSL& socket,
                                  std::string& error,
                                  const CancellationRequest& cancelled)
        {
            return socket.tlsHandShake(error, cancelled);
        }

        static void install(SocketAppleSSL& socket, bool blocked = false, bool reading = true)
        {
            socket._sslContext =
                SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);
            REQUIRE(socket._sslContext != nullptr);
            socket._handshakeWantsRead = reading;
            REQUIRE(SSLSetIOFuncs(socket._sslContext,
                                  blocked ? readBlocked : read,
                                  blocked ? writeBlocked : write) == noErr);
        }
    };
} // namespace ix

TEST_CASE("SecureTransport closes the native socket without a TLS context", "[socket_apple][close]")
{
    const bool contextCreated = GENERATE(false, true);
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(fd != -1);
    ix::SocketAppleSSL socket(ix::SocketTLSOptions {}, fd);
    if (contextCreated)
    {
        ix::SocketAppleSSLReadTestAccess::install(socket);
    }

    socket.close();
    errno = 0;
    CHECK(fcntl(fd, F_GETFD) == -1);
    CHECK(errno == EBADF);
    socket.close();
}

TEST_CASE("SecureTransport terminal errors clear a previous wait status", "[socket_apple][read]")
{
    const bool reading = GENERATE(false, true);
    ix::SocketAppleSSL socket(ix::SocketTLSOptions {});
    ix::SocketAppleSSLReadTestAccess::install(socket);
    ix::Socket::setErrno(EWOULDBLOCK);
    char byte = 0;
    CHECK((reading ? socket.recv(&byte, 1) : socket.send(&byte, 1)) == -1);
    CHECK_FALSE(ix::Socket::isWaitNeeded());
    CHECK(ix::Socket::getErrno() != 0);
}

TEST_CASE("SecureTransport handshakes stop on readiness errors", "[socket_apple][handshake_wait]")
{
    const bool reading = GENERATE(false, true);
    ix::SocketAppleSSL socket(ix::SocketTLSOptions {});
    ix::SocketAppleSSLReadTestAccess::install(socket, true, reading);
    unsigned checks = 0;
    std::string error;
    CHECK(ix::SocketAppleSSLReadTestAccess::handshake(
              socket, error, [&] { return ++checks > 3; }) == errSSLInternal);
    CHECK(checks == 1);
    CHECK(error == "Socket error during TLS handshake");
}
