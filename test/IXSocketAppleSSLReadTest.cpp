/*
 *  IXSocketAppleSSLReadTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXSocketAppleSSL.h>

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
        static void install(SocketAppleSSL& socket)
        {
            socket._sslContext =
                SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);
            REQUIRE(socket._sslContext != nullptr);
            REQUIRE(SSLSetIOFuncs(socket._sslContext, read, write) == noErr);
        }
    };
} // namespace ix

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
