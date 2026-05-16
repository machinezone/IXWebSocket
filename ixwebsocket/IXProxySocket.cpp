#include "IXProxySocket.h"

#include "IXSocketConnect.h"
#include <sstream>

#ifdef IXWEBSOCKET_USE_TLS
#ifdef IXWEBSOCKET_USE_OPEN_SSL
#include "IXSocketOpenSSL.h"

    template <>
    bool ix::ProxySocket<ix::SocketOpenSSL>::connect(const std::string& host,
                                                     int port,
                                                     std::string& errMsg,
                                                     const CancellationRequest& isCancellationRequested)
    {
        bool handshakeSuccessful = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (!_openSSLInitializationSuccessful)
            {
                errMsg = "OPENSSL_init_ssl failure";
                return false;
            }

            _sockfd = SocketConnect::connect(_proxyOptions.host, _proxyOptions.port, errMsg, isCancellationRequested);
            if (_sockfd == -1) return false;

            if (_proxyOptions.socksVersion == SocksVersion::SOCKS4 || _proxyOptions.socksVersion == SocksVersion::SOCKS4a)
            {
                if (!socks4Connect(host, port, errMsg, isCancellationRequested))
                {
                    Socket::close();
                    return false;
                }
            } else
            {
                if (!socks5Connect(host, port, errMsg, isCancellationRequested))
            {
                Socket::close();
                return false;
            }
        }

        _ssl_context = openSSLCreateContext(errMsg);
        if (_ssl_context == nullptr)
        {
            return false;
        }

        if (!handleTLSOptions(errMsg))
        {
            return false;
        }

        _ssl_connection = SSL_new(_ssl_context);
        if (_ssl_connection == nullptr)
        {
            errMsg = "OpenSSL failed to connect";
            SSL_CTX_free(_ssl_context);
            _ssl_context = nullptr;
            return false;
        }
        SSL_set_fd(_ssl_connection, _sockfd);

        // SNI support
        SSL_set_tlsext_host_name(_ssl_connection, host.c_str());

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
        // Support for server name verification
        // (The docs say that this should work from 1.0.2, and is the default from
        // 1.1.0, but it does not. To be on the safe side, the manual test
        // below is enabled for all versions prior to 1.1.0.)
        if (!_tlsOptions.disable_hostname_validation)
        {
            X509_VERIFY_PARAM* param = SSL_get0_param(_ssl_connection);
            X509_VERIFY_PARAM_set1_host(param, host.c_str(), host.size());
        }
#endif
        handshakeSuccessful = openSSLClientHandshake(host, errMsg, isCancellationRequested);
    }

    if (!handshakeSuccessful)
    {
        close();
        return false;
    }

    return true;
}

template bool ix::ProxySocket<ix::SocketOpenSSL>::socks5Connect(const std::string& host,

                            int port,
                            std::string& errMsg,
                            const CancellationRequest& isCancellationRequested);
 

#elif defined(IXWEBSOCKET_USE_MBED_TLS)
#include "IXSocketMbedTLS.h"

template <>
bool ix::ProxySocket<ix::SocketMbedTLS>::connect(const std::string& host,
                                                 int port,
                                                 std::string& errMsg,
                                                 const CancellationRequest& isCancellationRequested)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _sockfd = SocketConnect::connect(_proxyOptions.host, _proxyOptions.port, errMsg, isCancellationRequested);
        if (_sockfd == -1) return false;
        if (_proxyOptions.socksVersion == SocksVersion::SOCKS4 || _proxyOptions.socksVersion == SocksVersion::SOCKS4a)
        {
            if (!socks4Connect(host, port, errMsg, isCancellationRequested))
            {
                Socket::close();
                return false;
            }
        } else
        {
            if (!socks5Connect(host, port, errMsg, isCancellationRequested))
            {
                Socket::close();
                return false;
            }
        }
    }

    bool isClient = true;
    bool initialized = init(host, isClient, errMsg);
    if (!initialized)
    {
        close();
        return false;
    }

    mbedtls_ssl_set_bio(&_ssl, &_sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);

    int res;
    do
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            res = mbedtls_ssl_handshake(&_ssl);
        }

        if (isCancellationRequested())
        {
            errMsg = "Cancellation requested";
            close();
            return false;
        }
    } while (res == MBEDTLS_ERR_SSL_WANT_READ || res == MBEDTLS_ERR_SSL_WANT_WRITE);

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

template bool ix::ProxySocket<ix::SocketMbedTLS>::socks5Connect(const std::string& host,

                            int port,
                            std::string& errMsg,
                            const CancellationRequest& isCancellationRequested);
 

#elif defined(IXWEBSOCKET_USE_SECURE_TRANSPORT)
#include "IXSocketAppleSSL.h"

template <>
bool ix::ProxySocket<ix::SocketAppleSSL>::connect(const std::string& host,
                                                 int port,
                                                 std::string& errMsg,
                                                 const CancellationRequest& isCancellationRequested)
{
    OSStatus status;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _sockfd = SocketConnect::connect(_proxyOptions.host, _proxyOptions.port, errMsg, isCancellationRequested);
       
        if (_sockfd == -1) return false;
        if (_proxyOptions.socksVersion == SocksVersion::SOCKS4 || _proxyOptions.socksVersion == SocksVersion::SOCKS4a)
        {
            if (!socks4Connect(host, port, errMsg, isCancellationRequested))
            {
                Socket::close();
                return false;
            }
        } else
        {
            if (!socks5Connect(host, port, errMsg, isCancellationRequested))
            {
                Socket::close();
                return false;
            }
        }

        _sslContext = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);

        SSLSetIOFuncs(
            _sslContext, SocketAppleSSL::readFromSocket, SocketAppleSSL::writeToSocket);
        SSLSetConnection(_sslContext, (SSLConnectionRef)(long) _sockfd);
        SSLSetProtocolVersionMin(_sslContext, kTLSProtocol12);

        if (!_tlsOptions.disable_hostname_validation)
            SSLSetPeerDomainName(_sslContext, host.c_str(), host.size());

        if (_tlsOptions.isPeerVerifyDisabled())
        {
            Boolean option(1);
            SSLSetSessionOption(_sslContext, kSSLSessionOptionBreakOnServerAuth, option);

            status = tlsHandShake(errMsg, isCancellationRequested);

            if (status == errSSLServerAuthCompleted)
            {
                // proceed with the handshake
                status = tlsHandShake(errMsg, isCancellationRequested);
            }
        }
        else
        {
            status = tlsHandShake(errMsg, isCancellationRequested);
        }
    }

    if (status != noErr)
    {
        errMsg = getSSLErrorDescription(status);
        close();
        return false;
    }

    return true;
}

template bool ix::ProxySocket<ix::SocketAppleSSL>::socks5Connect(const std::string& host,

                            int port,
                            std::string& errMsg,
                            const CancellationRequest& isCancellationRequested);
 

#endif
#endif

template <>
bool ix::ProxySocket<ix::Socket>::socks4Connect(const std::string& host,
                                       int port,
                                       std::string& errMsg,
                                       const CancellationRequest& isCancellationRequested)
{
    return false;
}

template <>
bool ix::ProxySocket<ix::Socket>::connect(const std::string& host,
                                  int port,
                                  std::string& errMsg,
                                  const CancellationRequest& isCancellationRequested)
{
    {
        std::lock_guard<std::mutex> lock(_socketMutex);
        _sockfd = SocketConnect::connect(_proxyOptions.host, _proxyOptions.port, errMsg, isCancellationRequested);
    }
    if (_sockfd == -1) return false;
    if (_proxyOptions.socksVersion == SocksVersion::SOCKS4 || _proxyOptions.socksVersion == SocksVersion::SOCKS4a)
    {
        if (!socks4Connect(host, port, errMsg, isCancellationRequested))
        {
            Socket::close();
            return false;
        }
    } else
    {
        if (!socks5Connect(host, port, errMsg, isCancellationRequested))
        {
            Socket::close();
            return false;
        }
    }

    return true;
}

template <typename T>
bool ix::ProxySocket<T>::socks5Connect(const std::string& host,
                   int port,
                   std::string& errMsg,
                   const CancellationRequest& isCancellationRequested)
 
{
        std::stringstream ss;
        ss << '\x05';
        if (_proxyOptions.auth)
        {
            ss << '\x02' << '\x00' << '\x02';
        } else
        {
            ss << '\x01' << '\x00';
        }

        if (!rawWriteBytes(ss.str(), isCancellationRequested))
        {
            errMsg = "Failed to perform socks5 handshake";
            return false;
        }
        auto resp = rawReadBytes(2, nullptr, nullptr, isCancellationRequested);
        if (!resp.first)
        {
            errMsg = "Failed to perform socks5 handshake";
            return false;
        }
        ss.str("");
        if (resp.second.at(1) == '\xFF')
        {
            errMsg = "Socks server does not support this type of auth";
            return false;
        }
        if (resp.second.at(1) == '\x02')
        {
            auto size = static_cast<std::uint8_t>(_proxyOptions.username.size());
            ss << '\x01';
            ss.write(reinterpret_cast<const char*>(&size), sizeof(size));
            ss << _proxyOptions.username;
            size = static_cast<std::uint8_t>(_proxyOptions.password.size());
            ss.write(reinterpret_cast<const char*>(&size), sizeof(size));
            ss << _proxyOptions.password;
            if (!rawWriteBytes(ss.str(), isCancellationRequested))
            {
                errMsg = "Failed to perform auth";
                return false;
            }
            resp = this->rawReadBytes(2, nullptr, nullptr, isCancellationRequested);
            if (resp.second.at(1) != '\x00')
            {
                errMsg = "Invalid proxy auth";
                return false;
            }
            ss.str("");
        }
        ss << '\x05' << '\x01' << '\x00' << '\x03';

        auto rawSize = static_cast<std::uint8_t>(host.size());
        ss.write(reinterpret_cast<const char*>(&rawSize), sizeof(rawSize));
        ss << host;

        auto rawPort = static_cast<std::uint16_t>(port);
        rawPort = htons(rawPort);
            
        ss.write(reinterpret_cast<const char*>(&rawPort), sizeof(rawPort));
        if (!rawWriteBytes (ss.str(), isCancellationRequested))
        {
            errMsg = "Failed to connect to host";
            return false;
        }
        resp = this->rawReadBytes(4, nullptr, nullptr, isCancellationRequested);
        if (resp.second.at(1) != '\x00')
        {
            errMsg = "Failed to connect to host";
            return false;
        }
        switch (resp.second.at(3))
        {
            case '\x01':
            {
                rawReadBytes(6, nullptr, nullptr, isCancellationRequested);
                break;
            }
            case '\x03':
            {
                rawReadBytes(8, nullptr, nullptr, isCancellationRequested);
                break;
            }
            case '\x04':
            {
                resp = rawReadBytes(1, nullptr, nullptr, isCancellationRequested);
                rawReadBytes(resp.second.at(0)+2, nullptr, nullptr, isCancellationRequested);
                break;
            }
            default:
            {
                errMsg = "Somthing wrong happend";
                return false;
            }
        }

        return true;
}

template bool ix::ProxySocket<ix::Socket>::socks5Connect(const std::string& host,

                            int port,
                            std::string& errMsg,
                            const CancellationRequest& isCancellationRequested);
  
