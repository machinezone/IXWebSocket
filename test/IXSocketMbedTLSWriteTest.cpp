/*
 * IXSocketMbedTLSWriteTest.cpp
 *
 * Exercise the real TLS adapter with deterministic, in-memory backpressure.
 */

#ifdef IXWEBSOCKET_USE_MBED_TLS

#include <catch_amalgamated.hpp>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocket.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <mbedtls/version.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#if MBEDTLS_VERSION_MAJOR < 4
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif
#include <algorithm>
#include <deque>
#include <mutex>
#include <vector>

#include <ixwebsocket/IXSocketMbedTLS.h>

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) && defined(MBEDTLS_KEY_EXCHANGE_PSK_ENABLED) && \
    defined(MBEDTLS_PSA_CRYPTO_C)
#include <psa/crypto.h>

namespace ix
{
    struct SocketMbedTLSWriteTestAccess
    {
        static void install(SocketMbedTLS& socket, mbedtls_ssl_context& session)
        {
            socket.initMBedTLS();
            std::swap(socket._ssl, session);
        }

        static int recordSize(const SocketMbedTLS& socket)
        {
            return mbedtls_ssl_get_max_out_record_payload(&socket._ssl);
        }
    };
}

namespace
{
    struct MemoryPeer
    {
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config config;
        MemoryPeer* other = nullptr;
        std::deque<unsigned char> incoming;
        int writesBeforeBlocking = -1;
        int blockedResult = MBEDTLS_ERR_SSL_WANT_WRITE;
        bool failWrites = false;

        MemoryPeer()
        {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&config);
        }

        ~MemoryPeer()
        {
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&config);
        }

        MemoryPeer(const MemoryPeer&) = delete;
        MemoryPeer& operator=(const MemoryPeer&) = delete;

        static int random(void*, unsigned char* output, size_t size)
        {
            return psa_generate_random(output, size) == PSA_SUCCESS ? 0 : -1;
        }

        static int send(void* context, const unsigned char* data, size_t size)
        {
            auto& peer = *static_cast<MemoryPeer*>(context);
            if (peer.failWrites) return MBEDTLS_ERR_NET_SEND_FAILED;
            if (peer.writesBeforeBlocking == 0) return peer.blockedResult;
            if (peer.writesBeforeBlocking > 0) --peer.writesBeforeBlocking;
            peer.other->incoming.insert(peer.other->incoming.end(), data, data + size);
            return static_cast<int>(size);
        }

        static int receive(void* context, unsigned char* data, size_t size)
        {
            auto& peer = *static_cast<MemoryPeer*>(context);
            if (peer.incoming.empty()) return MBEDTLS_ERR_SSL_WANT_READ;
            const size_t count = (std::min)(size, peer.incoming.size());
            for (size_t i = 0; i < count; ++i)
            {
                data[i] = peer.incoming.front();
                peer.incoming.pop_front();
            }
            return static_cast<int>(count);
        }

        void configure(bool server)
        {
            const int* supported = mbedtls_ssl_list_ciphersuites();
            while (*supported && *supported != MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256)
                ++supported;
            if (!*supported) SKIP("TLS 1.2 PSK AES-GCM ciphersuite is not enabled");
            REQUIRE(psa_crypto_init() == PSA_SUCCESS);
            REQUIRE(mbedtls_ssl_config_defaults(&config,
                        server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0);
#if MBEDTLS_VERSION_NUMBER >= 0x03020000
            mbedtls_ssl_conf_min_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
            mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
#else
            mbedtls_ssl_conf_min_version(&config, 3, 3);
            mbedtls_ssl_conf_max_version(&config, 3, 3);
#endif
#if MBEDTLS_VERSION_MAJOR < 4
            mbedtls_ssl_conf_rng(&config, random, nullptr);
#endif
            static const int suites[] = {MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256, 0};
            mbedtls_ssl_conf_ciphersuites(&config, suites);
            const unsigned char key[] = {1, 2, 3, 4, 5, 6, 7, 8,
                                         9, 10, 11, 12, 13, 14, 15, 16};
            const unsigned char identity[] = "memory-write-test";
            REQUIRE(mbedtls_ssl_conf_psk(&config, key, sizeof(key), identity,
                                        sizeof(identity) - 1) == 0);
            REQUIRE(mbedtls_ssl_setup(&ssl, &config) == 0);
            mbedtls_ssl_set_bio(&ssl, this, send, receive, nullptr);
        }
    };

    struct MemoryConnection
    {
        ix::SocketMbedTLS& socket;
        MemoryPeer client;
        MemoryPeer server;

        explicit MemoryConnection(ix::SocketMbedTLS& socket)
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
            ix::SocketMbedTLSWriteTestAccess::install(socket, client.ssl);
        }

        ~MemoryConnection()
        {
            socket.close();
        }

        static bool handshake(mbedtls_ssl_context& ssl)
        {
            const int result = mbedtls_ssl_handshake(&ssl);
            REQUIRE((result == 0 || result == MBEDTLS_ERR_SSL_WANT_READ ||
                     result == MBEDTLS_ERR_SSL_WANT_WRITE));
            return result == 0;
        }

        std::vector<unsigned char> received()
        {
            std::vector<unsigned char> output;
            unsigned char buffer[16384];
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                const int count = mbedtls_ssl_read(&server.ssl, buffer, sizeof(buffer));
                if (count == MBEDTLS_ERR_SSL_WANT_READ) return output;
                REQUIRE(count > 0);
                output.insert(output.end(), buffer, buffer + count);
            }
            FAIL("TLS read budget exhausted");
            return output;
        }
    };

    std::vector<unsigned char> message(unsigned char seed, size_t size = 32776)
    {
        std::vector<unsigned char> data(size);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<unsigned char>(seed + i * 31 + i / 251);
        return data;
    }

    std::vector<unsigned char> pendingMessage(const ix::SocketMbedTLS& socket, unsigned char seed)
    {
        const int size = ix::SocketMbedTLSWriteTestAccess::recordSize(socket);
        REQUIRE(size > 8);
        return message(seed, static_cast<size_t>(size) * 2 + 8);
    }

    void drain(ix::SocketMbedTLS& socket, std::vector<unsigned char>& queue)
    {
        for (int attempt = 0; attempt < 100 && !queue.empty(); ++attempt)
        {
            const auto count = socket.send(reinterpret_cast<char*>(queue.data()), queue.size());
            REQUIRE(count > 0);
            REQUIRE(static_cast<size_t>(count) <= queue.size());
            queue.erase(queue.begin(), queue.begin() + count);
        }
        REQUIRE(queue.empty());
    }

    void blockTail(ix::SocketMbedTLS& socket, MemoryConnection& connection,
                   std::vector<unsigned char>& queue, int blockedResult)
    {
        connection.client.writesBeforeBlocking = 2;
        connection.client.blockedResult = blockedResult;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            const auto count = socket.send(reinterpret_cast<char*>(queue.data()), queue.size());
            if (count < 0)
            {
                REQUIRE(ix::Socket::isWaitNeeded());
                REQUIRE(queue.size() == 8);
                REQUIRE(socket.send(reinterpret_cast<char*>(queue.data()), queue.size()) == -1);
                return;
            }
            REQUIRE(count > 0);
            REQUIRE(static_cast<size_t>(count) <= queue.size());
            queue.erase(queue.begin(), queue.begin() + count);
        }
        FAIL("Did not reach the pending tail write");
    }

    void growsWhilePending(int blockedResult)
    {
        ix::SocketMbedTLS socket(ix::SocketTLSOptions{});
        MemoryConnection connection(socket);
        auto expected = pendingMessage(socket, 17);
        auto queue = expected;
        blockTail(socket, connection, queue, blockedResult);
        const auto next = message(93);
        expected.insert(expected.end(), next.begin(), next.end());
        {
            // A new allocation also releases the caller's original buffer.
            auto replacement = queue;
            replacement.insert(replacement.end(), next.begin(), next.end());
            queue.swap(replacement);
        }
        REQUIRE(socket.send(reinterpret_cast<char*>(queue.data()), queue.size()) == -1);
        connection.client.writesBeforeBlocking = -1;
        drain(socket, queue);
        const auto actual = connection.received();
        REQUIRE(actual.size() == expected.size());
        REQUIRE(actual == expected);
        socket.close();
    }
}

TEST_CASE("MbedTLS send delivers large buffers without backpressure", "[socket_mbedtls]")
{
    ix::SocketMbedTLS socket(ix::SocketTLSOptions{});
    MemoryConnection connection(socket);
    const auto expected = message(17);
    auto queue = expected;
    drain(socket, queue);
    REQUIRE(connection.received() == expected);
    socket.close();
}

TEST_CASE("MbedTLS send preserves data when the queue grows after WANT_WRITE", "[socket_mbedtls]")
{
    growsWhilePending(MBEDTLS_ERR_SSL_WANT_WRITE);
}

TEST_CASE("MbedTLS send preserves data when the queue grows after WANT_READ", "[socket_mbedtls]")
{
    growsWhilePending(MBEDTLS_ERR_SSL_WANT_READ);
}

TEST_CASE("MbedTLS send rejects a retry shorter than its pending write", "[socket_mbedtls]")
{
    ix::SocketMbedTLS socket(ix::SocketTLSOptions{});
    MemoryConnection connection(socket);
    const auto expected = pendingMessage(socket, 17);
    auto queue = expected;
    blockTail(socket, connection, queue, MBEDTLS_ERR_SSL_WANT_WRITE);
    connection.client.writesBeforeBlocking = -1;
    REQUIRE(socket.send(reinterpret_cast<char*>(queue.data()), queue.size() - 1) == -1);
    REQUIRE_FALSE(ix::Socket::isWaitNeeded());
    drain(socket, queue);
    REQUIRE(connection.received() == expected);
    socket.close();
}

TEST_CASE("MbedTLS close discards a pending write before a new connection", "[socket_mbedtls]")
{
    ix::SocketMbedTLS socket(ix::SocketTLSOptions{});
    {
        MemoryConnection connection(socket);
        auto queue = pendingMessage(socket, 17);
        blockTail(socket, connection, queue, MBEDTLS_ERR_SSL_WANT_WRITE);
        socket.close();
    }
    MemoryConnection connection(socket);
    const auto expected = message(93);
    auto queue = expected;
    drain(socket, queue);
    REQUIRE(connection.received() == expected);
    socket.close();
}

TEST_CASE("MbedTLS fatal send errors do not retain the previous wait status", "[socket_mbedtls]")
{
    ix::SocketMbedTLS socket(ix::SocketTLSOptions{});
    MemoryConnection connection(socket);
    auto queue = pendingMessage(socket, 17);
    blockTail(socket, connection, queue, MBEDTLS_ERR_SSL_WANT_WRITE);
    connection.client.failWrites = true;
    REQUIRE(socket.send(reinterpret_cast<char*>(queue.data()), queue.size()) == -1);
    REQUIRE_FALSE(ix::Socket::isWaitNeeded());
    socket.close();
}

#else
TEST_CASE("MbedTLS memory write tests require TLS 1.2 PSK and PSA crypto", "[socket_mbedtls]")
{
    SKIP("MbedTLS configuration does not enable TLS 1.2 PSK and PSA crypto");
}
#endif
#endif
