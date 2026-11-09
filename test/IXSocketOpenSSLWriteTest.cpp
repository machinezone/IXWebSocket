/*
 *  IXSocketOpenSSLWriteTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include "IXWebSocketSendBufferTest.h"
#include <cstring>
#include <deque>
#include <ixwebsocket/IXSocketOpenSSL.h>

#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(OPENSSL_NO_PSK) && !defined(OPENSSL_NO_TLS1_2)

namespace ix
{
    struct SocketOpenSSLWriteTestAccess
    {
        static void install(SocketOpenSSL& socket, SSL*& session, SSL_CTX*& context)
        {
            std::swap(socket._ssl_connection, session);
            std::swap(socket._ssl_context, context);
        }
    };
} // namespace ix

namespace
{
    struct MemoryPeer
    {
        SSL_CTX* context = nullptr;
        SSL* ssl = nullptr;
        MemoryPeer* other = nullptr;
        std::deque<unsigned char> incoming;
        int writesBeforeBlocking = -1;
        bool wantRead = false;
        bool failWrites = false;

        ~MemoryPeer()
        {
            SSL_free(ssl);
            SSL_CTX_free(context);
        }

        static int send(BIO* bio, const char* data, int size)
        {
            auto& peer = *static_cast<MemoryPeer*>(BIO_get_data(bio));
            BIO_clear_retry_flags(bio);
            if (peer.failWrites) return -1;
            if (peer.writesBeforeBlocking == 0)
            {
                if (peer.wantRead)
                    BIO_set_retry_read(bio);
                else
                    BIO_set_retry_write(bio);
                return -1;
            }
            if (peer.writesBeforeBlocking > 0) --peer.writesBeforeBlocking;
            peer.other->incoming.insert(peer.other->incoming.end(), data, data + size);
            return size;
        }

        static int receive(BIO* bio, char* data, int size)
        {
            auto& peer = *static_cast<MemoryPeer*>(BIO_get_data(bio));
            BIO_clear_retry_flags(bio);
            if (peer.incoming.empty())
            {
                BIO_set_retry_read(bio);
                return -1;
            }
            const int count = static_cast<int>((std::min) (size_t(size), peer.incoming.size()));
            for (int i = 0; i < count; ++i)
            {
                data[i] = static_cast<char>(peer.incoming.front());
                peer.incoming.pop_front();
            }
            return count;
        }

        static long control(BIO* bio, int command, long, void*)
        {
            if (command == BIO_CTRL_FLUSH) return 1;
            if (command == BIO_CTRL_PENDING)
                return static_cast<long>(
                    static_cast<MemoryPeer*>(BIO_get_data(bio))->incoming.size());
            return 0;
        }

        static int create(BIO* bio)
        {
            BIO_set_init(bio, 1);
            return 1;
        }

        static int destroy(BIO*)
        {
            return 1;
        }

        static BIO_METHOD* makeMethod()
        {
            BIO_METHOD* method = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "write test memory BIO");
            REQUIRE(method != nullptr);
            REQUIRE(BIO_meth_set_write(method, send) == 1);
            REQUIRE(BIO_meth_set_read(method, receive) == 1);
            REQUIRE(BIO_meth_set_ctrl(method, control) == 1);
            REQUIRE(BIO_meth_set_create(method, create) == 1);
            REQUIRE(BIO_meth_set_destroy(method, destroy) == 1);
            return method;
        }

        static unsigned int pskClient(SSL*,
                                      const char*,
                                      char* identity,
                                      unsigned int identitySize,
                                      unsigned char* key,
                                      unsigned int keySize)
        {
            const char name[] = "write-test";
            if (identitySize < sizeof(name) || keySize < 16) return 0;
            std::memcpy(identity, name, sizeof(name));
            std::memset(key, 17, 16);
            return 16;
        }

        static unsigned int pskServer(SSL*, const char*, unsigned char* key, unsigned int keySize)
        {
            if (keySize < 16) return 0;
            std::memset(key, 17, 16);
            return 16;
        }

        void configure(bool server)
        {
            static std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> method(makeMethod(),
                                                                                BIO_meth_free);
            context = SSL_CTX_new(TLS_method());
            REQUIRE(context != nullptr);
            REQUIRE(SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) == 1);
            REQUIRE(SSL_CTX_set_max_proto_version(context, TLS1_2_VERSION) == 1);
            if (SSL_CTX_set_cipher_list(context, "PSK-AES128-GCM-SHA256") != 1)
                SKIP("TLS 1.2 PSK AES-GCM ciphersuite is not enabled");
            SSL_CTX_set_mode(context,
                             SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
            SSL_CTX_set_psk_client_callback(context, pskClient);
            SSL_CTX_set_psk_server_callback(context, pskServer);
            ssl = SSL_new(context);
            REQUIRE(ssl != nullptr);
            BIO* bio = BIO_new(method.get());
            REQUIRE(bio != nullptr);
            BIO_set_data(bio, this);
            SSL_set_bio(ssl, bio, bio);
            if (server)
                SSL_set_accept_state(ssl);
            else
                SSL_set_connect_state(ssl);
        }
    };

    struct MemoryConnection
    {
        ix::SocketOpenSSL& socket;
        MemoryPeer client;
        MemoryPeer server;
        ix::WebSocketTransport transport;

        explicit MemoryConnection(ix::SocketOpenSSL& socket)
            : socket(socket)
        {
            client.other = &server;
            server.other = &client;
            client.configure(false);
            server.configure(true);
            bool clientReady = false;
            bool serverReady = false;
            for (int attempt = 0; attempt < 100 && !(clientReady && serverReady); ++attempt)
            {
                if (!clientReady) clientReady = handshake(client.ssl);
                if (!serverReady) serverReady = handshake(server.ssl);
            }
            REQUIRE(clientReady);
            REQUIRE(serverReady);
            ix::SocketOpenSSLWriteTestAccess::install(socket, client.ssl, client.context);
            installWriteSocket(transport, socket);
        }

        ~MemoryConnection()
        {
            socket.close();
        }

        static bool handshake(SSL* ssl)
        {
            ERR_clear_error();
            const int result = SSL_do_handshake(ssl);
            const int error = SSL_get_error(ssl, result);
            REQUIRE((result == 1 || error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE));
            return result == 1;
        }

        std::vector<unsigned char> received()
        {
            std::vector<unsigned char> output;
            unsigned char buffer[16384];
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                ERR_clear_error();
                const int count = SSL_read(server.ssl, buffer, sizeof(buffer));
                if (SSL_get_error(server.ssl, count) == SSL_ERROR_WANT_READ) return output;
                REQUIRE(count > 0);
                output.insert(output.end(), buffer, buffer + count);
            }
            FAIL("TLS read budget exhausted");
            return output;
        }
    };
} // namespace

TEST_CASE("OpenSSL transport preserves retries when the queue grows", "[socket_openssl]")
{
    const bool wantRead = GENERATE(false, true);
    CAPTURE(wantRead);
    ix::SocketOpenSSL socket(ix::SocketTLSOptions {});
    MemoryConnection connection(socket);
    connection.client.writesBeforeBlocking = 2;
    connection.client.wantRead = wantRead;
    auto expected = writeTestMessage(17);
    ix::WebSocketTransportTestAccess::enqueue(connection.transport, expected);
    REQUIRE(ix::WebSocketTransportTestAccess::flush(connection.transport));
    REQUIRE(connection.transport.bufferedAmount() == 8);
    REQUIRE(ix::WebSocketTransportTestAccess::flush(connection.transport));
    const auto next = writeTestMessage(93);
    ix::WebSocketTransportTestAccess::enqueue(connection.transport, next);
    expected.insert(expected.end(), next.begin(), next.end());
    REQUIRE(ix::WebSocketTransportTestAccess::flush(connection.transport));
    REQUIRE(connection.transport.bufferedAmount() == next.size() + 8);
    connection.client.writesBeforeBlocking = -1;
    flushWriteTest(connection.transport);
    CHECK(connection.received() == expected);
}

TEST_CASE("OpenSSL fatal send errors do not retain the previous wait status", "[socket_openssl]")
{
    ix::SocketOpenSSL socket(ix::SocketTLSOptions {});
    MemoryConnection connection(socket);
    connection.client.writesBeforeBlocking = 0;
    ix::WebSocketTransportTestAccess::enqueue(connection.transport, writeTestMessage(17));
    REQUIRE(ix::WebSocketTransportTestAccess::flush(connection.transport));
    connection.client.failWrites = true;
    CHECK_FALSE(ix::WebSocketTransportTestAccess::flush(connection.transport));
    CHECK(connection.transport.bufferedAmount() == 0);
    CHECK_FALSE(ix::Socket::isWaitNeeded());
}

#else
TEST_CASE("OpenSSL memory write tests require TLS 1.2 PSK and OpenSSL 1.1 APIs", "[socket_openssl]")
{
    SKIP("OpenSSL configuration does not enable the memory TLS fixture prerequisites");
}
#endif
