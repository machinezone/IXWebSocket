/*
 *  IXSocketMbedTLS.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 *
 *  Some code taken from
 *  https://github.com/rottor12/WsClientLib/blob/master/lib/src/WsClientLib.cpp
 */
#include "IXSocketMbedTLS.h"

namespace ix
{
    SocketMbedTLS::SocketMbedTLS()
    {
        mbedtls_ssl_init(&_ssl);
        mbedtls_ssl_config_init(&_conf);
        mbedtls_ctr_drbg_init(&_ctr_drbg);
        mbedtls_entropy_init(&_entropy);
    }

    SocketMbedTLS::~SocketMbedTLS()
    {
        mbedtls_ssl_free(&_ssl);
        mbedtls_ssl_config_free(&_conf);
        mbedtls_ctr_drbg_free(&_ctr_drbg);
        mbedtls_entropy_free(&_entropy);
    }

    bool SocketMbedTLS::connect(const std::string& host,
                                int port,
                                std::string& errMsg,
                                const CancellationRequest& isCancellationRequested)
    {
        return Socket::connect(host, port, errMsg, nullptr);
    }

    void SocketMbedTLS::close()
    {
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
