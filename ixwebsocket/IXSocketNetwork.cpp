/*
 * IXSocketNetwork.cpp
 * Native macOS TLS with bounded asynchronous I/O and poll notifications.
 */
#ifdef IXWEBSOCKET_USE_NETWORK_FRAMEWORK
#include "IXSocketNetwork.h"

#include <Network/Network.h>
#include <Security/Security.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <vector>

namespace ix
{
    namespace
    {
        // Level-triggered notification: one byte remains until all buffered input
        // is consumed. The descriptor carries notifications, never TLS payloads.
        struct NetworkSignal
        {
            int readFd = -1;
            int writeFd = -1;
            bool signalled = false; // guarded by the owning state's mutex
            NetworkSignal()
            {
                int fds[2];
                if (::pipe(fds) != 0) return;
                readFd = fds[0];
                writeFd = fds[1];
                for (int fd : fds)
                {
                    int flags = fcntl(fd, F_GETFL);
                    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1 ||
                        !Socket::setCloseOnExec(fd))
                    {
                        ::close(readFd);
                        ::close(writeFd);
                        readFd = writeFd = -1;
                        return;
                    }
                }
            }
            ~NetworkSignal()
            {
                if (readFd != -1) ::close(readFd);
                if (writeFd != -1) ::close(writeFd);
            }
            void notify()
            {
                if (!signalled && writeFd != -1)
                {
                    char byte = 1;
                    ssize_t written;
                    do
                    {
                        written = ::write(writeFd, &byte, 1);
                    } while (written < 0 && errno == EINTR);
                    signalled = written == 1;
                }
            }
            void clear()
            {
                char byte;
                while (::read(readFd, &byte, 1) > 0)
                {
                }
                signalled = false;
            }
        };

        std::string networkError(nw_error_t error)
        {
            if (!error) return "Network.framework connection failed";
            CFErrorRef cfError = nw_error_copy_cf_error(error);
            CFStringRef description = cfError ? CFErrorCopyDescription(cfError) : nullptr;
            char buffer[1024];
            std::string result =
                "Network.framework error " + std::to_string(nw_error_get_error_code(error));
            if (description &&
                CFStringGetCString(description, buffer, sizeof(buffer), kCFStringEncodingUTF8))
                result += ": " + std::string(buffer);
            if (description) CFRelease(description);
            if (cfError) CFRelease(cfError);
            return result;
        }

        std::string readFile(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        }

        CFArrayRef importPEM(std::string pem, std::string& error)
        {
            if (pem.empty())
            {
                error = "Empty certificate or private key";
                return nullptr;
            }
            // Common CA bundles include comments and certificate descriptions.
            // SecItemImport expects just the PEM objects.
            const std::string begin = "-----BEGIN CERTIFICATE-----";
            const std::string end = "-----END CERTIFICATE-----";
            if (pem.find(begin) != std::string::npos)
            {
                std::string certificates;
                size_t offset = 0;
                while ((offset = pem.find(begin, offset)) != std::string::npos)
                {
                    size_t finish = pem.find(end, offset);
                    if (finish == std::string::npos)
                    {
                        error = "Unterminated PEM certificate";
                        return nullptr;
                    }
                    finish += end.size();
                    certificates.append(pem, offset, finish - offset);
                    certificates += '\n';
                    offset = finish;
                }
                pem = std::move(certificates);
            }
            CFDataRef data =
                CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(pem.data()), pem.size());
            if (!data)
            {
                error = "Cannot allocate certificate data";
                return nullptr;
            }
            SecExternalFormat format = kSecFormatUnknown;
            SecExternalItemType type = kSecItemTypeUnknown;
            SecItemImportExportKeyParameters params = {};
            params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
            params.passphrase = CFSTR(""); // Never prompt for a password.
            CFArrayRef items = nullptr;
            OSStatus status =
                SecItemImport(data, nullptr, &format, &type, 0, &params, nullptr, &items);
            CFRelease(data);
            if (status != errSecSuccess || !items || CFArrayGetCount(items) == 0)
            {
                if (items) CFRelease(items);
                error = "Cannot import certificate/private key: " + std::to_string(status);
                return nullptr;
            }
            return items;
        }

        // Returns owned parameters. All captured CF/dispatch objects are retained
        // explicitly because this translation unit uses C++ (not Objective-C ARC).
        nw_parameters_t makeParameters(SocketTLSOptions options,
                                       bool server,
                                       std::string host,
                                       std::string& error)
        {
            if (!options.isUsingDefaultCiphers())
            {
                error = "Network.framework backend does not support cipher strings";
                return nullptr;
            }
            if (options.certFile.empty() != options.keyFile.empty() ||
                (server && !options.hasCertAndKey()))
            {
                error =
                    "Network.framework requires both certFile and keyFile (required for servers)";
                return nullptr;
            }
            sec_identity_t identity = nullptr;
            if (options.hasCertAndKey())
            {
                CFArrayRef certs = importPEM(readFile(options.certFile), error);
                CFArrayRef keys = certs ? importPEM(readFile(options.keyFile), error) : nullptr;
                if (!certs || !keys)
                {
                    if (certs) CFRelease(certs);
                    return nullptr;
                }
                auto cert = static_cast<SecCertificateRef>(
                    const_cast<void*>(CFArrayGetValueAtIndex(certs, 0)));
                auto key =
                    static_cast<SecKeyRef>(const_cast<void*>(CFArrayGetValueAtIndex(keys, 0)));
                SecIdentityRef ref = nullptr;
                if (CFGetTypeID(cert) == SecCertificateGetTypeID() &&
                    CFGetTypeID(key) == SecKeyGetTypeID())
                    ref = SecIdentityCreate(nullptr, cert, key);
                if (ref)
                {
                    identity = sec_identity_create_with_certificates(ref, certs);
                    CFRelease(ref);
                }
                CFRelease(certs);
                CFRelease(keys);
                if (!identity)
                {
                    error = "Cannot create TLS identity; check certificate/key match";
                    return nullptr;
                }
            }
            CFArrayRef anchors = nullptr;
            if (!options.isUsingSystemDefaults() && !options.isPeerVerifyDisabled())
            {
                anchors = importPEM(options.isUsingInMemoryCAs() ? options.caFile
                                                                 : readFile(options.caFile),
                                    error);
                if (!anchors)
                {
                    if (identity) sec_release(identity);
                    return nullptr;
                }
                for (CFIndex i = 0; i < CFArrayGetCount(anchors); ++i)
                {
                    if (CFGetTypeID(CFArrayGetValueAtIndex(anchors, i)) !=
                        SecCertificateGetTypeID())
                    {
                        error = "CA bundle must contain only certificates";
                        CFRelease(anchors);
                        if (identity) sec_release(identity);
                        return nullptr;
                    }
                }
            }
            // Capture smart ownership so verification can outlive this function.
            auto anchorOwner = std::shared_ptr<const void>(anchors,
                                                           [](const void* p)
                                                           {
                                                               if (p) CFRelease(p);
                                                           });
            nw_parameters_t parameters = nw_parameters_create_secure_tcp(
                ^(nw_protocol_options_t tls) {
                    sec_protocol_options_t security = nw_tls_copy_sec_protocol_options(tls);
                    sec_protocol_options_set_min_tls_protocol_version(security,
                                                                      tls_protocol_version_TLSv12);
                    if (identity) sec_protocol_options_set_local_identity(security, identity);
                    if (!server) sec_protocol_options_set_tls_server_name(security, host.c_str());
                    sec_protocol_options_set_peer_authentication_required(
                        security, !options.isPeerVerifyDisabled());
                    sec_protocol_options_set_verify_block(
                        security,
                        ^(sec_protocol_metadata_t,
                          sec_trust_t trustObject,
                          sec_protocol_verify_complete_t complete) {
                            if (options.isPeerVerifyDisabled())
                            {
                                complete(true);
                                return;
                            }
                            SecTrustRef trust = sec_trust_copy_ref(trustObject);
                            CFStringRef hostname =
                                (!server && !options.disable_hostname_validation)
                                    ? CFStringCreateWithCString(
                                          nullptr, host.c_str(), kCFStringEncodingUTF8)
                                    : nullptr;
                            SecPolicyRef policy = SecPolicyCreateSSL(!server, hostname);
                            bool valid =
                                trust && policy &&
                                (server || options.disable_hostname_validation || hostname);
                            if (valid) valid = SecTrustSetPolicies(trust, policy) == errSecSuccess;
                            if (valid && anchorOwner)
                            {
                                valid =
                                    SecTrustSetAnchorCertificates(
                                        trust, static_cast<CFArrayRef>(anchorOwner.get())) ==
                                        errSecSuccess &&
                                    SecTrustSetAnchorCertificatesOnly(trust, true) == errSecSuccess;
                            }
                            if (valid) valid = SecTrustEvaluateWithError(trust, nullptr);
                            if (policy) CFRelease(policy);
                            if (hostname) CFRelease(hostname);
                            if (trust) CFRelease(trust);
                            complete(valid);
                        },
                        dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
                    sec_release(security);
                },
                ^(nw_protocol_options_t tcp) {
                    nw_tcp_options_set_no_delay(tcp, true);
                });
            if (identity) sec_release(identity);
            if (!parameters) error = "Cannot create Network.framework TLS parameters";
            return parameters;
        }
    } // namespace

    struct SocketNetwork::State
    {
        std::mutex mutex;
        std::condition_variable changed;
        NetworkSignal signal;
        nw_connection_t connection = nullptr;
        dispatch_queue_t queue =
            dispatch_queue_create("ixwebsocket.network", DISPATCH_QUEUE_SERIAL);
        std::vector<char> input;
        size_t offset = 0;
        bool ready = false;
        bool closed = false;
        bool eof = false;
        bool receiving = false;
        bool sending = false;
        size_t sent = 0;
        bool started = false;
        bool server = false;
        std::string error;
        ~State()
        {
            if (connection)
            {
                nw_connection_cancel(connection);
                nw_release(connection);
            }
            dispatch_release(queue);
        }
        // Called under mutex. At most 64 KiB is outstanding or buffered.
        static void receive(const std::shared_ptr<State>& state)
        {
            if (state->receiving || state->closed || state->eof || !state->error.empty() ||
                !state->input.empty())
                return;
            state->receiving = true;
            std::weak_ptr<State> weak = state;
            nw_connection_receive(
                state->connection,
                1,
                65536,
                ^(dispatch_data_t data, nw_content_context_t, bool complete, nw_error_t error) {
                    auto s = weak.lock();
                    if (!s) return;
                    std::lock_guard<std::mutex> lock(s->mutex);
                    s->receiving = false;
                    if (s->closed) return;
                    if (data)
                    {
                        const void* bytes = nullptr;
                        size_t size = 0;
                        dispatch_data_t mapped = dispatch_data_create_map(data, &bytes, &size);
                        if (mapped)
                        {
                            const char* begin = static_cast<const char*>(bytes);
                            if (size) s->input.assign(begin, begin + size);
                            dispatch_release(mapped);
                        }
                        else
                            s->error = "Cannot map received TLS data";
                    }
                    s->eof = complete;
                    if (error) s->error = networkError(error);
                    if (!s->input.empty() || s->eof || !s->error.empty())
                        s->signal.notify();
                    else
                        receive(s);
                    s->changed.notify_all();
                });
        }
    };

    SocketNetwork::SocketNetwork(const SocketTLSOptions& options)
        : Socket()
        , _state(std::make_shared<State>())
        , _options(options)
    {
        _sockfd = _state->signal.readFd;
    }

    SocketNetwork::~SocketNetwork()
    {
        close();
    }

    bool SocketNetwork::connect(const std::string& host,
                                int port,
                                std::string& error,
                                const CancellationRequest& cancelled)
    {
        if (port < 1 || port > 65535)
        {
            error = "Invalid TLS port";
            return false;
        }
        nw_parameters_t params = makeParameters(_options, false, host, error);
        if (!params) return false;
        nw_endpoint_t endpoint =
            nw_endpoint_create_host(host.c_str(), std::to_string(port).c_str());
        if (!endpoint)
        {
            nw_release(params);
            error = "Cannot create TLS endpoint";
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(_state->mutex);
            if (_state->closed || _state->connection)
            {
                error = "TLS socket already used";
                nw_release(endpoint);
                nw_release(params);
                return false;
            }
            _state->connection = nw_connection_create(endpoint, params);
        }
        nw_release(endpoint);
        nw_release(params);
        return start(error, cancelled);
    }

    bool SocketNetwork::start(std::string& error, const CancellationRequest& cancelled)
    {
        auto s = _state;
        std::unique_lock<std::mutex> lock(s->mutex);
        if (s->signal.readFd == -1 || !s->connection || s->closed || s->started ||
            !s->error.empty())
        {
            error = s->error.empty() ? "Uninitialized or already used Network.framework socket"
                                     : s->error;
            return false;
        }
        s->started = true;
        std::weak_ptr<State> weak = s;
        nw_connection_set_queue(s->connection, s->queue);
        nw_connection_set_state_changed_handler(
            s->connection, ^(nw_connection_state_t state, nw_error_t err) {
                auto current = weak.lock();
                if (!current) return;
                std::lock_guard<std::mutex> guard(current->mutex);
                if (current->closed) return;
                if (state == nw_connection_state_ready)
                {
                    current->ready = true;
                    State::receive(current);
                }
                else if (state == nw_connection_state_failed ||
                         state == nw_connection_state_waiting)
                {
                    current->error = networkError(err);
                    current->signal.notify();
                }
                else if (state == nw_connection_state_cancelled)
                {
                    current->closed = true;
                    current->signal.notify();
                }
                current->changed.notify_all();
            });
        nw_connection_start(s->connection);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!s->ready && !s->closed && s->error.empty())
        {
            if ((cancelled && cancelled()) ||
                (s->server && std::chrono::steady_clock::now() >= deadline))
            {
                s->error = "TLS connection cancelled or handshake timed out";
                break;
            }
            s->changed.wait_for(lock, std::chrono::milliseconds(10));
        }
        if (s->ready && !s->closed && s->error.empty()) return true;
        error = s->error.empty() ? "TLS connection closed during handshake" : s->error;
        lock.unlock();
        close();
        return false;
    }

    bool SocketNetwork::accept(std::string& error)
    {
        return accept(error, CancellationRequest());
    }
    bool SocketNetwork::accept(std::string& error, const CancellationRequest& cancelled)
    {
        return start(error, cancelled);
    }

    void SocketNetwork::close()
    {
        // State owns the notification descriptors; Socket's destructor must not close them.
        _sockfd = -1;
        auto s = _state;
        std::lock_guard<std::mutex> lock(s->mutex);
        if (s->closed) return;
        s->closed = true;
        if (s->connection) nw_connection_cancel(s->connection);
        s->signal.notify();
        s->changed.notify_all();
    }

    std::ptrdiff_t SocketNetwork::send(char* buffer, size_t length)
    {
        auto s = _state;
        std::lock_guard<std::mutex> lock(s->mutex);
        if (s->closed || !s->error.empty() || !s->ready)
        {
            setErrno(ECONNRESET);
            return -1;
        }
        if (!length) return 0;
        if (s->sending)
        {
            setErrno(EWOULDBLOCK);
            return -1;
        }
        if (s->sent)
        {
            size_t sent = s->sent;
            s->sent = 0;
            return static_cast<std::ptrdiff_t>(sent);
        }
        size_t count = std::min(length, size_t(65536));
        // dispatch_data_create copies the bytes when using a null destructor.
        dispatch_data_t data =
            dispatch_data_create(buffer, count, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        if (!data)
        {
            setErrno(ENOMEM);
            return -1;
        }
        s->sending = true;
        std::weak_ptr<State> weak = s;
        nw_connection_send(
            s->connection, data, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, ^(nw_error_t error) {
                auto current = weak.lock();
                if (!current) return;
                std::lock_guard<std::mutex> guard(current->mutex);
                current->sending = false;
                current->sent = count;
                if (error && !current->closed)
                {
                    current->error = networkError(error);
                    current->signal.notify();
                }
                current->changed.notify_all();
            });
        dispatch_release(data);
        setErrno(EWOULDBLOCK);
        // Report bytes only after completion, so writeBytes() cannot return and
        // close the connection while the final asynchronous send is still pending.
        return -1;
    }

    std::ptrdiff_t SocketNetwork::recv(void* buffer, size_t length)
    {
        auto s = _state;
        std::lock_guard<std::mutex> lock(s->mutex);
        if (!length) return 0;
        size_t count = std::min(length, s->input.size() - s->offset);
        if (count)
        {
            memcpy(buffer, s->input.data() + s->offset, count);
            s->offset += count;
            if (s->offset == s->input.size())
            {
                s->input.clear();
                s->offset = 0;
                if (!s->closed && !s->eof && s->error.empty()) s->signal.clear();
                State::receive(s);
            }
            return static_cast<std::ptrdiff_t>(count);
        }
        if (s->closed || !s->error.empty())
        {
            setErrno(ECONNRESET);
            return -1;
        }
        if (s->eof)
        {
            setErrno(0);
            return 0;
        }
        setErrno(EWOULDBLOCK);
        return -1;
    }

    PollResultType SocketNetwork::isReadyToWrite(int timeoutMs)
    {
        auto s = _state;
        std::unique_lock<std::mutex> lock(s->mutex);
        auto available = [&] { return !s->sending || s->closed || !s->error.empty(); };
        if (timeoutMs < 0)
            s->changed.wait(lock, available);
        else if (!s->changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), available))
            return PollResultType::Timeout;
        return s->closed || !s->error.empty() ? PollResultType::Error
                                              : PollResultType::ReadyForWrite;
    }

    struct SocketNetworkListener::State
    {
        std::mutex mutex;
        std::condition_variable changed;
        NetworkSignal signal;
        nw_listener_t listener = nullptr;
        dispatch_queue_t queue =
            dispatch_queue_create("ixwebsocket.listener", DISPATCH_QUEUE_SERIAL);
        std::deque<nw_connection_t> pending;
        bool ready = false;
        bool stopped = false;
        std::string error;
        ~State()
        {
            if (listener)
            {
                nw_listener_cancel(listener);
                nw_release(listener);
            }
            for (auto connection : pending)
            {
                nw_connection_cancel(connection);
                nw_release(connection);
            }
            dispatch_release(queue);
        }
    };

    SocketNetworkListener::SocketNetworkListener()
        : _state(std::make_shared<State>())
    {
    }
    SocketNetworkListener::~SocketNetworkListener()
    {
        stop();
    }

    bool SocketNetworkListener::listen(const std::string& host,
                                       int& port,
                                       int addressFamily,
                                       size_t maxPending,
                                       const SocketTLSOptions& options,
                                       std::string& error)
    {
        _options = options;
        struct in6_addr addr;
        if ((addressFamily != AF_INET && addressFamily != AF_INET6) ||
            ix::inet_pton(addressFamily, host.c_str(), &addr) != 1 || port < 0 || port > 65535)
        {
            error = "Network.framework listener requires a numeric bind address and valid port";
            return false;
        }
        nw_parameters_t parameters = makeParameters(options, true, "", error);
        if (!parameters) return false;
        nw_parameters_set_reuse_local_address(parameters, true);
        nw_endpoint_t endpoint =
            nw_endpoint_create_host(host.c_str(), std::to_string(port).c_str());
        nw_parameters_set_local_endpoint(parameters, endpoint);
        nw_release(endpoint);
        auto s = _state;
        std::unique_lock<std::mutex> lock(s->mutex);
        if (s->signal.readFd == -1)
        {
            nw_release(parameters);
            error = "Cannot create listener notification pipe";
            return false;
        }
        s->listener = nw_listener_create(parameters);
        nw_release(parameters);
        if (!s->listener)
        {
            error = "Cannot create Network.framework listener";
            return false;
        }
        std::weak_ptr<State> weak = s;
        nw_listener_set_queue(s->listener, s->queue);
        nw_listener_set_state_changed_handler(
            s->listener, ^(nw_listener_state_t state, nw_error_t err) {
                auto current = weak.lock();
                if (!current) return;
                std::lock_guard<std::mutex> guard(current->mutex);
                if (state == nw_listener_state_ready) current->ready = true;
                if (state == nw_listener_state_failed || state == nw_listener_state_waiting)
                {
                    current->error = networkError(err);
                    current->signal.notify();
                }
                current->changed.notify_all();
            });
        nw_listener_set_new_connection_handler(s->listener, ^(nw_connection_t connection) {
            auto current = weak.lock();
            if (!current)
            {
                nw_connection_cancel(connection);
                return;
            }
            std::lock_guard<std::mutex> guard(current->mutex);
            if (current->stopped || current->pending.size() >= maxPending)
            {
                nw_connection_cancel(connection);
                return;
            }
            nw_retain(connection);
            current->pending.push_back(connection);
            current->signal.notify();
        });
        nw_listener_start(s->listener);
        if (!s->changed.wait_for(
                lock, std::chrono::seconds(10), [&] { return s->ready || !s->error.empty(); }) ||
            !s->error.empty())
        {
            error = s->error.empty() ? "Network.framework listener startup timed out" : s->error;
            return false;
        }
        port = nw_listener_get_port(s->listener);
        return true;
    }

    int SocketNetworkListener::getFd() const
    {
        return _state->signal.readFd;
    }

    std::string SocketNetworkListener::getError() const
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        return _state->error;
    }

    std::unique_ptr<SocketNetwork> SocketNetworkListener::take(std::string& remoteIp,
                                                               int& remotePort)
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        if (_state->pending.empty()) return nullptr;
        auto connection = _state->pending.front();
        _state->pending.pop_front();
        if (_state->pending.empty()) _state->signal.clear();
        nw_endpoint_t endpoint = nw_connection_copy_endpoint(connection);
        const char* hostname = nw_endpoint_get_hostname(endpoint);
        remoteIp = hostname ? hostname : "";
        remotePort = nw_endpoint_get_port(endpoint);
        nw_release(endpoint);
        std::unique_ptr<SocketNetwork> socket(new SocketNetwork(_options));
        socket->_state->connection = connection; // transfer ownership
        socket->_state->server = true;
        return socket;
    }

    void SocketNetworkListener::stop()
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        if (_state->stopped) return;
        _state->stopped = true;
        if (_state->listener) nw_listener_cancel(_state->listener);
        for (auto connection : _state->pending)
        {
            nw_connection_cancel(connection);
            nw_release(connection);
        }
        _state->pending.clear();
        _state->signal.notify();
    }
} // namespace ix
#endif
