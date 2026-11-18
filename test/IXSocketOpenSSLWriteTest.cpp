/*
 *  IXSocketOpenSSLWriteTest.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include "IXWebSocketSendBufferTest.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <ixwebsocket/IXSocketOpenSSL.h>
#include <ixwebsocket/IXUuid.h>
#include <set>
#include <sstream>
#include <thread>

#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(OPENSSL_NO_PSK) && !defined(OPENSSL_NO_TLS1_2)

namespace ix
{
    struct SocketOpenSSLWriteTestAccess
    {
        static bool configureKeyLogging(SocketOpenSSL& socket, SSL_CTX* context, std::string& error)
        {
            return socket.configureKeyLogging(context, error);
        }
        static bool handshake(SocketOpenSSL& socket,
                              std::string& error,
                              const CancellationRequest& cancelled)
        {
            return socket.openSSLClientHandshake("memory.invalid", error, cancelled);
        }
        static int reset(SocketOpenSSL& socket)
        {
            return SSL_clear(socket._ssl_connection);
        }
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
        bool failReads = false;
        bool eof = false;

        ~MemoryPeer()
        {
            SSL_free(ssl);
            SSL_CTX_free(context);
        }

        static int send(BIO* bio, const char* data, int size)
        {
            auto& peer = *static_cast<MemoryPeer*>(BIO_get_data(bio));
            BIO_clear_retry_flags(bio);
            if (peer.failWrites)
            {
                return -1;
            }
            if (peer.writesBeforeBlocking == 0)
            {
                if (peer.wantRead)
                {
                    BIO_set_retry_read(bio);
                }
                else
                {
                    BIO_set_retry_write(bio);
                }
                return -1;
            }
            if (peer.writesBeforeBlocking > 0)
            {
                --peer.writesBeforeBlocking;
            }
            peer.other->incoming.insert(peer.other->incoming.end(), data, data + size);
            return size;
        }

        static int receive(BIO* bio, char* data, int size)
        {
            auto& peer = *static_cast<MemoryPeer*>(BIO_get_data(bio));
            BIO_clear_retry_flags(bio);
            if (peer.failReads)
            {
                return -1;
            }
            if (peer.eof && peer.incoming.empty())
            {
                return 0;
            }
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
            if (command == BIO_CTRL_FLUSH)
            {
                return 1;
            }
            if (command == BIO_CTRL_PENDING)
            {
                return static_cast<long>(
                    static_cast<MemoryPeer*>(BIO_get_data(bio))->incoming.size());
            }
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
            if (identitySize < sizeof(name) || keySize < 16)
            {
                return 0;
            }
            std::memcpy(identity, name, sizeof(name));
            std::memset(key, 17, 16);
            return 16;
        }

        static unsigned int pskServer(SSL*, const char*, unsigned char* key, unsigned int keySize)
        {
            if (keySize < 16)
            {
                return 0;
            }
            std::memset(key, 17, 16);
            return 16;
        }

        void configure(bool server, int version = TLS1_2_VERSION)
        {
            static std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> method(makeMethod(),
                                                                                BIO_meth_free);
            context = SSL_CTX_new(TLS_method());
            REQUIRE(context != nullptr);
            REQUIRE(SSL_CTX_set_min_proto_version(context, version) == 1);
            REQUIRE(SSL_CTX_set_max_proto_version(context, version) == 1);
            if (SSL_CTX_set_cipher_list(context, "PSK-AES128-GCM-SHA256") != 1)
            {
                SKIP("TLS 1.2 PSK AES-GCM ciphersuite is not enabled");
            }
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
            {
                SSL_set_accept_state(ssl);
            }
            else
            {
                SSL_set_connect_state(ssl);
            }
        }
    };

    struct MemoryConnection
    {
        ix::SocketOpenSSL& socket;
        MemoryPeer client;
        MemoryPeer server;
        ix::WebSocketTransport transport;

        explicit MemoryConnection(ix::SocketOpenSSL& socket,
                                  ix::SocketOpenSSL* serverSocket = nullptr,
                                  int version = TLS1_2_VERSION)
            : socket(socket)
        {
            client.other = &server;
            server.other = &client;
            client.configure(false, version);
            server.configure(true, version);
            std::string error;
            REQUIRE(ix::SocketOpenSSLWriteTestAccess::configureKeyLogging(
                socket, client.context, error));
            if (serverSocket)
            {
                REQUIRE(ix::SocketOpenSSLWriteTestAccess::configureKeyLogging(
                    *serverSocket, server.context, error));
            }
            bool clientReady = false;
            bool serverReady = false;
            for (int attempt = 0; attempt < 100 && !(clientReady && serverReady); ++attempt)
            {
                if (!clientReady)
                {
                    clientReady = handshake(client.ssl);
                }
                if (!serverReady)
                {
                    serverReady = handshake(server.ssl);
                }
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
                if (SSL_get_error(server.ssl, count) == SSL_ERROR_WANT_READ)
                {
                    return output;
                }
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

TEST_CASE("OpenSSL terminal reads clear a previous wait status", "[socket_openssl][read]")
{
    const int terminal = GENERATE(0, 1, 2);
    ix::SocketOpenSSL socket(ix::SocketTLSOptions {});
    MemoryConnection connection(socket);
    char byte;
    REQUIRE(socket.recv(&byte, 1) == -1);
    REQUIRE(ix::Socket::isWaitNeeded());
    if (terminal == 0)
    {
        REQUIRE(SSL_shutdown(connection.server.ssl) == 0);
    }
    else if (terminal == 1)
    {
        connection.client.failReads = true;
    }
    else
    {
        connection.client.eof = true;
    }
    CHECK(socket.recv(&byte, 1) == (terminal == 0 ? 0 : -1));
    if (terminal != 0)
    {
        CHECK(ix::Socket::getErrno() != 0);
    }
    CHECK_FALSE(ix::Socket::isWaitNeeded());
}

TEST_CASE("OpenSSL readBytes completes from TLS buffers without polling", "[socket_openssl][read]")
{
    ix::SocketOpenSSL socket(ix::SocketTLSOptions {});
    MemoryConnection connection(socket);
    REQUIRE(SSL_write(connection.server.ssl, "abc", 3) == 3);
    auto result = socket.readBytes(3, nullptr, nullptr, nullptr);
    CHECK(result.first);
    CHECK(result.second == "abc");
}

TEST_CASE("OpenSSL client handshakes stop on readiness errors", "[socket_openssl][handshake_wait]")
{
    const bool blockedWrite = GENERATE(false, true);
    ix::SocketOpenSSL socket(ix::SocketTLSOptions {});
    MemoryConnection connection(socket);
    REQUIRE(ix::SocketOpenSSLWriteTestAccess::reset(socket) == 1);
    connection.client.incoming.clear();
    connection.server.incoming.clear();
    if (blockedWrite)
    {
        connection.client.writesBeforeBlocking = 0;
    }
    unsigned checks = 0;
    std::string error;
    CHECK_FALSE(
        ix::SocketOpenSSLWriteTestAccess::handshake(socket, error, [&] { return ++checks > 3; }));
    CHECK(checks == 1);
    CHECK(error == "Socket error during TLS handshake");
}

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
namespace
{
    struct KeyLogFile
    {
        std::string name =
            std::string(IX_OPENSSL_TEST_OUTPUT_DIR) + "/keylog-" + ix::uuid4() + ".log";
        KeyLogFile()
        {
            std::ofstream file(name, std::ios::binary);
            REQUIRE(file.is_open());
            file << "# existing content\n";
        }
        ~KeyLogFile()
        {
            std::remove(name.c_str());
        }
        std::vector<std::string> lines() const
        {
            std::ifstream file(name, std::ios::binary);
            REQUIRE(file.is_open());
            std::string line;
            REQUIRE(static_cast<bool>(std::getline(file, line)));
            CHECK(line == "# existing content");
            std::vector<std::string> result;
            while (std::getline(file, line))
            {
                result.push_back(line);
            }
            return result;
        }
    };

    std::set<std::string> keyLogRandoms(const std::vector<std::string>& lines)
    {
        std::set<std::string> randoms;
        const auto hex = [](char c)
        { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };
        for (const auto& line : lines)
        {
            std::string label, random, secret, extra;
            std::istringstream stream(line);
            REQUIRE(static_cast<bool>(stream >> label >> random >> secret));
            CHECK_FALSE(static_cast<bool>(stream >> extra));
            CHECK(random.size() == 64);
            CHECK(std::all_of(random.begin(), random.end(), hex));
            CHECK_FALSE(secret.empty());
            CHECK(std::all_of(secret.begin(), secret.end(), hex));
            randoms.insert(random);
        }
        return randoms;
    }
} // namespace

TEST_CASE("OpenSSL key logs append client and server secrets across connections",
          "[socket_openssl][keylog]")
{
    const bool sharedFile = GENERATE(false, true);
    int version = TLS1_2_VERSION;
#ifndef OPENSSL_NO_TLS1_3
    version = GENERATE(TLS1_2_VERSION, TLS1_3_VERSION);
#endif
    KeyLogFile clientFile, serverFile;
    ix::SocketTLSOptions clientOptions, serverOptions;
    clientOptions.keyLogFile = clientFile.name;
    serverOptions.keyLogFile = sharedFile ? clientFile.name : serverFile.name;
    for (unsigned i = 0; i < 3; ++i)
    {
        ix::SocketOpenSSL client(clientOptions), server(serverOptions);
        MemoryConnection connection(client, &server, version);
    }
    const auto clientLines = clientFile.lines();
    CHECK(keyLogRandoms(clientLines).size() == 3);
    if (sharedFile)
    {
        std::multiset<std::string> lines(clientLines.begin(), clientLines.end());
        for (const auto& line : clientLines)
        {
            CHECK(lines.count(line) == 2);
        }
    }
    else
    {
        const auto serverLines = serverFile.lines();
        CHECK(keyLogRandoms(serverLines).size() == 3);
        CHECK(std::multiset<std::string>(clientLines.begin(), clientLines.end()) ==
              std::multiset<std::string>(serverLines.begin(), serverLines.end()));
    }
}

TEST_CASE("Concurrent OpenSSL key logs preserve complete lines and socket errors",
          "[socket_openssl][keylog]")
{
    KeyLogFile file;
    ix::SocketTLSOptions options;
    options.keyLogFile = file.name;
    ix::SocketOpenSSL first(options), second(options);
    MemoryPeer firstPeer, secondPeer;
    firstPeer.configure(false);
    secondPeer.configure(false);
    std::string error;
    REQUIRE(ix::SocketOpenSSLWriteTestAccess::configureKeyLogging(first, firstPeer.context, error));
    REQUIRE(
        ix::SocketOpenSSLWriteTestAccess::configureKeyLogging(second, secondPeer.context, error));
    auto callback = SSL_CTX_get_keylog_callback(firstPeer.context);
    REQUIRE(callback != nullptr);
    const std::string line = "CLIENT_RANDOM " + std::string(64, '0') + " " + std::string(96, '1');
    std::atomic<bool> errorsPreserved(true);
    const auto write = [&](const SSL* ssl)
    {
        for (unsigned i = 0; i < 32; ++i)
        {
            ix::Socket::setErrno(EWOULDBLOCK);
            callback(ssl, line.c_str());
            if (ix::Socket::getErrno() != EWOULDBLOCK)
            {
                errorsPreserved = false;
            }
        }
    };
    std::thread a(write, firstPeer.ssl), b(write, secondPeer.ssl);
    a.join();
    b.join();
    CHECK(errorsPreserved.load());
    const auto lines = file.lines();
    CHECK(lines.size() == 64);
    for (const auto& actual : lines)
    {
        CHECK(actual == line);
    }
}

TEST_CASE("OpenSSL key logging is opt-in and rejects an unwritable destination",
          "[socket_openssl][keylog]")
{
    MemoryPeer peer;
    peer.configure(false);
    ix::SocketOpenSSL disabled(ix::SocketTLSOptions {});
    std::string error;
    REQUIRE(ix::SocketOpenSSLWriteTestAccess::configureKeyLogging(disabled, peer.context, error));
    CHECK(SSL_CTX_get_keylog_callback(peer.context) == nullptr);
    KeyLogFile file;
    ix::SocketTLSOptions options;
    options.keyLogFile = file.name + "/cannot-create.log";
    ix::SocketOpenSSL invalid(options);
    CHECK_FALSE(
        ix::SocketOpenSSLWriteTestAccess::configureKeyLogging(invalid, peer.context, error));
    CHECK(error == "Cannot open TLS key log file: " + options.keyLogFile);
    CHECK(SSL_CTX_get_keylog_callback(peer.context) == nullptr);
}
#endif

#else
TEST_CASE("OpenSSL memory write tests require TLS 1.2 PSK and OpenSSL 1.1 APIs", "[socket_openssl]")
{
    SKIP("OpenSSL configuration does not enable the memory TLS fixture prerequisites");
}
#endif
