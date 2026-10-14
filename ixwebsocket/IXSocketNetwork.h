/*
 * IXSocketNetwork.h
 * Native macOS TLS sockets backed by Network.framework.
 */
#pragma once

#ifdef IXWEBSOCKET_USE_NETWORK_FRAMEWORK
#include "IXSocket.h"
#include "IXSocketTLSOptions.h"

namespace ix
{
    class SocketNetworkListener;

    class SocketNetwork final : public Socket
    {
    public:
        explicit SocketNetwork(const SocketTLSOptions& options);
        ~SocketNetwork() override;
        bool connect(const std::string& host,
                     int port,
                     std::string& error,
                     const CancellationRequest& cancelled) override;
        bool accept(std::string& error) override;
        bool accept(std::string& error, const CancellationRequest& cancelled);
        void close() override;
        std::ptrdiff_t send(char* buffer, size_t length) override;
        std::ptrdiff_t recv(void* buffer, size_t length) override;
        PollResultType isReadyToWrite(int timeoutMs) override;

    private:
        struct State;
        std::shared_ptr<State> _state;
        SocketTLSOptions _options;
        bool start(std::string& error, const CancellationRequest& cancelled);
        friend class SocketNetworkListener;
    };

    // A native listener supplies connections instead of accepting BSD descriptors.
    class SocketNetworkListener
    {
    public:
        SocketNetworkListener();
        ~SocketNetworkListener();
        bool listen(const std::string& host,
                    int& port,
                    int addressFamily,
                    size_t maxPending,
                    const SocketTLSOptions& options,
                    std::string& error);
        int getFd() const;
        std::string getError() const;
        std::unique_ptr<SocketNetwork> take(std::string& remoteIp, int& remotePort);
        void stop();

    private:
        struct State;
        std::shared_ptr<State> _state;
        SocketTLSOptions _options;
    };
} // namespace ix
#endif
