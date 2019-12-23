/*
 *  IXSocketAppleSSL.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2018 Machine Zone, Inc. All rights reserved.
 *
 *  Adapted from Satori SDK Apple SSL code.
 */
#include "IXSocketAppleSSL.h"

#include "IXSocketConnect.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define socketerrno errno

#include <Security/SecureTransport.h>

namespace
{
    OSStatus read_from_socket(SSLConnectionRef connection, void* data, size_t* len)
    {
        int fd = (int) (long) connection;
        if (fd < 0) return errSSLInternal;

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
            switch (errno)
            {
                case ENOENT: return errSSLClosedGraceful;

                case EAGAIN: return errSSLWouldBlock;

                case ECONNRESET: return errSSLClosedAbort;

                default: return errSecIO;
            }
        }
    }

    OSStatus write_to_socket(SSLConnectionRef connection, const void* data, size_t* len)
    {
        int fd = (int) (long) connection;
        if (fd < 0) return errSSLInternal;

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
                success = CFStringGetCString(message, localBuffer, 128, kCFStringEncodingUTF8);
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

#undef CURL_BUILD_IOS
    OSStatus CopyIdentityFromPKCS12File(
        const char *cPath,
        const char *cPassword,
        SecIdentityRef *out_cert_and_key)
    {
        OSStatus status = errSecItemNotFound;
        CFURLRef pkcs_url = CFURLCreateFromFileSystemRepresentation(
            NULL, (const UInt8 *)cPath, strlen(cPath), false);
        CFStringRef password = cPassword ? CFStringCreateWithCString(NULL,
            cPassword, kCFStringEncodingUTF8) : NULL;
        CFDataRef pkcs_data = NULL;

        /* We can import P12 files on iOS or OS X 10.7 or later: */
        /* These constants are documented as having first appeared in 10.6 but they
           raise linker errors when used on that cat for some reason. */
        if (CFURLCreateDataAndPropertiesFromResource(
            NULL, pkcs_url, &pkcs_data, NULL, NULL, &status)) {
            CFArrayRef items = NULL;

            /* On iOS SecPKCS12Import will never add the client certificate to the
             * Keychain.
             *
             * It gives us back a SecIdentityRef that we can use directly. */
#if CURL_BUILD_IOS
            const void *cKeys[] = {kSecImportExportPassphrase};
            const void *cValues[] = {password};
            CFDictionaryRef options = CFDictionaryCreate(NULL, cKeys, cValues,
                    password ? 1L : 0L, NULL, NULL);

            if (options != NULL) {
                status = SecPKCS12Import(pkcs_data, options, &items);
                CFRelease(options);
            }

            /* On macOS SecPKCS12Import will always add the client certificate to
             * the Keychain.
             *
             * As this doesn't match iOS, and apps may not want to see their client
             * certificate saved in the the user's keychain, we use SecItemImport
             * with a NULL keychain to avoid importing it.
             *
             * This returns a SecCertificateRef from which we can construct a
             * SecIdentityRef.
             */
#else
            SecItemImportExportKeyParameters keyParams;
            SecExternalFormat inputFormat = kSecFormatPKCS12;
            SecExternalItemType inputType = kSecItemTypeCertificate;

            memset(&keyParams, 0x00, sizeof(keyParams));
            keyParams.version    = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
            keyParams.passphrase = password;

            status = SecItemImport(pkcs_data, NULL, &inputFormat, &inputType,
                                   0, &keyParams, NULL, &items);
#endif

            /* Extract the SecIdentityRef */
            if (status == errSecSuccess && items && CFArrayGetCount(items))
            {
                CFIndex i, count;
                count = CFArrayGetCount(items);

                for (i = 0; i < count; i++)
                {
                    CFTypeRef item = (CFTypeRef) CFArrayGetValueAtIndex(items, i);
                    CFTypeID  itemID = CFGetTypeID(item);

                    if (itemID == CFDictionaryGetTypeID())
                    {
                        CFTypeRef identity = (CFTypeRef) CFDictionaryGetValue(
                                (CFDictionaryRef) item,
                                kSecImportItemIdentity);
                        CFRetain(identity);
                        *out_cert_and_key = (SecIdentityRef) identity;
                        break;
                    }
                    else if (itemID == SecCertificateGetTypeID())
                    {
                        status = SecIdentityCreateWithCertificate(NULL,
                                (SecCertificateRef) item,
                                out_cert_and_key);
                        break;
                    }
                }
            }

            if (items) CFRelease(items);
            CFRelease(pkcs_data);
        }

        if (password) CFRelease(password);
        CFRelease(pkcs_url);
        return status;
    }


} // anonymous namespace

namespace ix
{
    SocketAppleSSL::SocketAppleSSL(const SocketTLSOptions& tlsOptions, int fd)
        : Socket(fd)
        , _sslContext(nullptr)
        , _tlsOptions(tlsOptions)
    {
        ;
    }

    SocketAppleSSL::~SocketAppleSSL()
    {
        SocketAppleSSL::close();
    }

    bool SocketAppleSSL::accept(std::string& errMsg)
    {
        errMsg = "TLS not supported yet in server mode with apple ssl backend";
        return false;
    }

    bool SocketAppleSSL::handleTLSOptions(std::string& errMsg)
    {
        SecIdentityRef cert_and_key = NULL;

        const char* ssl_cert = _tlsOptions.certFile.c_str();

        OSStatus err = CopyIdentityFromPKCS12File(ssl_cert, "foobar", &cert_and_key);

        if (err == noErr && cert_and_key)
        {
            SecCertificateRef cert = NULL;
            CFTypeRef certs_c[1];
            CFArrayRef certs;

            err = SecIdentityCopyCertificate(cert_and_key, &cert);

            certs_c[0] = cert_and_key;
            certs = CFArrayCreate(NULL, (const void **)certs_c, 1L,
                    &kCFTypeArrayCallBacks);
            err = SSLSetCertificate(_sslContext, certs);
            if (err != noErr)
            {
                errMsg = "SSLSetCertificate failed";
                return false;
            }
        }
        else
        {
            switch(err) {
                case errSecAuthFailed: case -25264: /* errSecPkcs12VerifyFailure */
                    errMsg = "SSL: Incorrect password for the certificate \"%s\" "
                            "and its private key."; // , ssl_cert);
                    break;
                case -26275: /* errSecDecode */ case -25257: /* errSecUnknownFormat */
                    errMsg = "SSL: Couldn't make sense of the data in the "
                            "certificate \"%s\" and its private key.";
                            ; // ssl_cert);
                    break;
                case -25260: /* errSecPassphraseRequired */
                    errMsg = "SSL The certificate \"%s\" requires a password.";
                            // ssl_cert);
                    break;
                case errSecItemNotFound:
                    errMsg = "SSL: Can't find the certificate \"%s\" and its private "
                            "key in the Keychain."; // , ssl_cert);
                    break;
                default:
                    errMsg = "SSL: Can't load the certificate \"%s\" and its private "
                            "key: OSStatus %d" ; // , ssl_cert, err);
                    break;
            }
            return false;
        }

        return true;
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
            SSLSetConnection(_sslContext, (SSLConnectionRef)(long) _sockfd);
            SSLSetProtocolVersionMin(_sslContext, kTLSProtocol12);
            SSLSetPeerDomainName(_sslContext, host.c_str(), host.size());

            if (!handleTLSOptions(errMsg)) return false; // FIXME not calling close()

            if (_tlsOptions.isPeerVerifyDisabled())
            {
                Boolean option(1);
                SSLSetSessionOption(_sslContext, kSSLSessionOptionBreakOnServerAuth, option);

                do
                {
                    status = SSLHandshake(_sslContext);
                } while (errSSLWouldBlock == status || errSSLServerAuthCompleted == status);

                if (status == errSSLServerAuthCompleted)
                {
                    // proceed with the handshake
                    do
                    {
                        status = SSLHandshake(_sslContext);
                    } while (errSSLWouldBlock == status || errSSLServerAuthCompleted == status);
                }
            }
            else
            {
                do
                {
                    status = SSLHandshake(_sslContext);
                } while (errSSLWouldBlock == status || errSSLServerAuthCompleted == status);
            }
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
        do
        {
            size_t processed = 0;
            std::lock_guard<std::mutex> lock(_mutex);
            status = SSLWrite(_sslContext, buf, nbyte, &processed);
            ret += processed;
            buf += processed;
            nbyte -= processed;
        } while (nbyte > 0 && errSSLWouldBlock == status);

        if (ret == 0 && errSSLClosedAbort != status) ret = -1;
        return ret;
    }

    ssize_t SocketAppleSSL::send(const std::string& buffer)
    {
        return send((char*) &buffer[0], buffer.size());
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

            if (processed > 0) return (ssize_t) processed;

            // The connection was reset, inform the caller that this
            // Socket should close
            if (status == errSSLClosedGraceful || status == errSSLClosedNoNotify ||
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

} // namespace ix
