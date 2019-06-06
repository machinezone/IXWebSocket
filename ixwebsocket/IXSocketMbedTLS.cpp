/*
 *  IXSocketMbedTLS.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 *
 *  Some code taken from
 *  https://github.com/rottor12/WsClientLib/blob/master/lib/src/WsClientLib.cpp
 *  and mini_client.c example from mbedtls
 */

#include "IXSocketMbedTLS.h"
#include "IXSocketConnect.h"
#include "IXNetSystem.h"
#include "IXSocket.h"

#include <string.h>

namespace ix
{
    SocketMbedTLS::~SocketMbedTLS()
    {
        close();
    }

    bool SocketMbedTLS::init(const std::string& host, std::string& errMsg)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        mbedtls_ssl_init(&_ssl);
        mbedtls_ssl_config_init(&_conf);
        mbedtls_ctr_drbg_init(&_ctr_drbg);

        const char *pers = "IXSocketMbedTLS";

        mbedtls_entropy_init(&_entropy);
        if (mbedtls_ctr_drbg_seed(&_ctr_drbg,
                                  mbedtls_entropy_func,
                                  &_entropy,
                                  (const unsigned char *) pers,
                                  strlen(pers)) != 0)
        {
            errMsg = "Setting entropy seed failed";
            return false;
        }

        if (mbedtls_ssl_config_defaults(&_conf,
                                        MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT ) != 0)
        {
            errMsg = "Setting config default failed";
            return false;
        }

        mbedtls_ssl_conf_rng(&_conf, mbedtls_ctr_drbg_random, &_ctr_drbg);

        // FIXME: cert verification is disabled
        mbedtls_ssl_conf_authmode(&_conf, MBEDTLS_SSL_VERIFY_NONE);

        if (mbedtls_ssl_setup(&_ssl, &_conf) != 0)
        {
            errMsg = "SSL setup failed";
            return false;
        }

        if (mbedtls_ssl_set_hostname(&_ssl, host.c_str()) != 0)
        {
            errMsg = "SNI setup failed";
            return false;
        }

        return true;
    }

    bool SocketMbedTLS::connect(const std::string& host,
                                int port,
                                std::string& errMsg,
                                const CancellationRequest& isCancellationRequested)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _sockfd = SocketConnect::connect(host, port, errMsg, isCancellationRequested);
            if (_sockfd == -1) return false;
        }

        if (!init(host, errMsg))
        {
            close();
            return false;
        }

        mbedtls_ssl_set_bio(&_ssl, &_sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);

        int res;
        do
        {
            std::lock_guard<std::mutex> lock(_mutex);
            res = mbedtls_ssl_handshake(&_ssl);
        }
        while (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE);

        if (res != 0)
        {
            char buf[256];
            mbedtls_strerror(res, buf, sizeof(buf));

            errMsg = "error in handshake : ";
            errMsg += buf;

            close();
            return false;
        }

        return true;
    }

    void SocketMbedTLS::close()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        mbedtls_ssl_free(&_ssl);
        mbedtls_ssl_config_free(&_conf);
        mbedtls_ctr_drbg_free(&_ctr_drbg);
        mbedtls_entropy_free(&_entropy);

        Socket::close();
    }

    ssize_t SocketMbedTLS::send(char* buf, size_t nbyte)
    {
        ssize_t sent = 0;

        while (nbyte > 0)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            ssize_t res = mbedtls_ssl_write(&_ssl, (unsigned char*) buf, nbyte);

            if (res > 0) {
                nbyte -= res;
                sent += res;
            } else if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE) {
                errno = EWOULDBLOCK;
                return -1;
            } else {
                return -1;
            }
        }
        return sent;
    }

    ssize_t SocketMbedTLS::send(const std::string& buffer)
    {
        return send((char*)&buffer[0], buffer.size());
    }

    ssize_t SocketMbedTLS::recv(void* buf, size_t nbyte)
    {
        while (true)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            ssize_t res = mbedtls_ssl_read(&_ssl, (unsigned char*) buf, (int) nbyte);

            if (res > 0)
            {
                return res;
            }

            if (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                errno = EWOULDBLOCK;
            }
            return -1;
        }
    }

}
