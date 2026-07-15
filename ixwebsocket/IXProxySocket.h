#pragma once

#include "IXProxyOptions.h"
#include "IXSocket.h"
#include "IXSocketTLSOptions.h"
#include <array>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <vector>

namespace ix
{
    template <typename T>
    class ProxySocket final : public T
    {
    public:
        static_assert(std::is_base_of<Socket, T>::value, "T must derive from socket");
        template <typename U = T, typename = std::enable_if<std::is_same<U, Socket>::value>>
        ProxySocket(const ProxyOptions& proxyOptions, int fd = -1) :
            T(fd),
            _proxyOptions(proxyOptions)
        {}

        template <typename U = T, typename = std::enable_if<!std::is_same<U, Socket>::value>>
        ProxySocket(const ProxyOptions& proxyOptions,
                    const SocketTLSOptions& tlsOptions,
                    int fd = -1) :
            T(tlsOptions, fd),
            _proxyOptions(proxyOptions)
        {}

        virtual bool connect(const std::string& host,
                             int port,
                             std::string& errMsg,
                             const CancellationRequest& isCancellationRequested) final;
    private:
        ProxyOptions _proxyOptions;

        bool socks5Connect(const std::string& host,
                           int port,
                           std::string& errMsg,
                           const CancellationRequest& isCancellationRequested);        

        bool socks4Connect(const std::string& host,
                           int port,
                           std::string& errMsg,
                           const CancellationRequest& isCancellationRequested) {return false;}
        
        bool rawWriteBytes(const std::string& str,
                           const CancellationRequest& isCancellationRequested)
        {
            int offset = 0;
            int len = (int) str.size();
    
            while (true)
            {
                if (isCancellationRequested && isCancellationRequested()) return false;

                ssize_t ret = Socket::send((char*) &str[offset], len);
    
                // We wrote some bytes, as needed, all good.
                if (ret > 0)
                {
                    if (ret == len)
                    {
                        return true;
                    }
                    else
                    {
                        offset += ret;
                        len -= ret;
                        continue;
                    }
                }
                // There is possibly something to be writen, try again
                else if (ret < 0 && Socket::isWaitNeeded())
                {
                    continue;
                }   
                // There was an error during the write, abort
                else
                {
                    return false;
                }
            }
        }

        std::pair<bool, std::string> rawReadBytes(
            size_t length,
            const OnProgressCallback& onProgressCallback,
            const OnChunkCallback& onChunkCallback,
            const CancellationRequest& isCancellationRequested)
        {
            std::array<uint8_t, 1 << 14> readBuffer;
            std::vector<uint8_t> output;
            size_t bytesRead = 0;

            while (bytesRead != length)
            {
                if (isCancellationRequested && isCancellationRequested())
                {
                    const std::string errorMsg("Cancellation Requested");
                    return std::make_pair(false, errorMsg);
                }
    
                size_t size = std::min(readBuffer.size(), length - bytesRead);
                ssize_t ret = Socket::recv((char*) &readBuffer[0], size);
    
                if (ret > 0)
                {
                    if (onChunkCallback)
                    {
                        std::string chunk(readBuffer.begin(), readBuffer.begin() + ret);
                        onChunkCallback(chunk);
                    }
                    else
                    {
                        output.insert(output.end(), readBuffer.begin(), readBuffer.begin() + ret);
                    }       
                    bytesRead += ret;
                }
                else if (ret <= 0 && !Socket::isWaitNeeded())
                {
                    const std::string errorMsg("Recv Error");
                    return std::make_pair(false, errorMsg);
                }

                if (onProgressCallback) onProgressCallback((int) bytesRead, (int) length);

                // Wait with a 1ms timeout until the socket is ready to read.
                // This way we are not busy looping
                if (Socket::isReadyToRead(1) == PollResultType::Error)
                {
                    const std::string errorMsg("Poll Error");
                    return std::make_pair(false, errorMsg);
                }
            }

            return std::make_pair(true, std::string(output.begin(), output.end()));
        }
    };
} // namespace ix
