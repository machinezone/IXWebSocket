/*
 *  IXSocks5.cpp
 *  Copyright (c) 2026 Machine Zone, Inc. All rights reserved.
 */
#include "IXSocks5.h"

#include "IXDNSLookup.h"
#include "IXSocket.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <vector>

namespace ix
{
    namespace
    {
        bool transfer(socket_t fd,
                      uint8_t* data,
                      size_t size,
                      bool reading,
                      std::string& error,
                      const CancellationRequest& cancelled)
        {
            size_t offset = 0;
            while (offset < size)
            {
                if (cancelled && cancelled())
                {
                    error = "SOCKS5 connection cancelled";
                    return false;
                }
                auto ready = Socket::poll(reading, 10, fd, nullptr);
                if (ready == PollResultType::Timeout) continue;
                if (ready == PollResultType::Error)
                {
                    error = "SOCKS5 socket readiness error";
                    return false;
                }
                // The socket is nonblocking. Never request bytes beyond this protocol field.
                int count = static_cast<int>((std::min) (size - offset, size_t(65536)));
                int flags = 0;
#ifndef _WIN32
#ifdef MSG_NOSIGNAL
                if (!reading) flags = MSG_NOSIGNAL;
#endif
#endif
                auto result =
                    reading
                        ? ::recv(fd, reinterpret_cast<char*>(data + offset), count, 0)
                        : ::send(fd, reinterpret_cast<const char*>(data + offset), count, flags);
                if (result > 0)
                    offset += static_cast<size_t>(result);
                else if (result == 0)
                {
                    error = "SOCKS5 proxy closed the connection";
                    return false;
                }
                else if (!Socket::isWaitNeeded() && Socket::getErrno() != EINTR)
                {
                    error = "SOCKS5 socket I/O error";
                    return false;
                }
            }
            return true;
        }

        bool numericAddress(const std::string& host, std::vector<uint8_t>& address)
        {
            std::array<uint8_t, 16> bytes;
            if (inet_pton(AF_INET, host.c_str(), bytes.data()) == 1)
            {
                address.push_back(1);
                address.insert(address.end(), bytes.begin(), bytes.begin() + 4);
                return true;
            }
            if (inet_pton(AF_INET6, host.c_str(), bytes.data()) == 1)
            {
                address.push_back(4);
                address.insert(address.end(), bytes.begin(), bytes.end());
                return true;
            }
            return false;
        }
    } // namespace

    bool Socks5::validate(const ProxyOptions& options, std::string& error)
    {
        if (options.host.empty()) return true;
        if (options.port < 1 || options.port > 65535)
            error = "Invalid SOCKS5 proxy port";
        else if (options.dnsMode != ProxyOptions::DnsMode::Local &&
                 options.dnsMode != ProxyOptions::DnsMode::Remote)
            error = "Invalid SOCKS5 DNS mode";
        else if ((!options.username.empty() || !options.password.empty()) &&
                 (options.username.empty() || options.password.empty() ||
                  options.username.size() > 255 || options.password.size() > 255))
            error = "SOCKS5 username and password must each contain 1 to 255 bytes";
        else
            return true;
        return false;
    }

    std::string Socks5::resolveHost(const std::string& host,
                                    int port,
                                    std::string& error,
                                    const CancellationRequest& cancelled)
    {
        auto lookup = std::make_shared<DNSLookup>(host, port);
        auto addresses = lookup->resolve(error, cancelled);
        if (!addresses) return {};
        for (auto address = addresses.get(); address; address = address->ai_next)
        {
            char result[INET6_ADDRSTRLEN];
            const void* bytes = nullptr;
            if (address->ai_family == AF_INET)
                bytes = &reinterpret_cast<const sockaddr_in*>(address->ai_addr)->sin_addr;
            else if (address->ai_family == AF_INET6)
                bytes = &reinterpret_cast<const sockaddr_in6*>(address->ai_addr)->sin6_addr;
            if (bytes && inet_ntop(address->ai_family, bytes, result, sizeof(result)))
                return result;
        }
        error = "SOCKS5 destination has no IPv4 or IPv6 address";
        return {};
    }

    bool Socks5::connect(socket_t fd,
                         const std::string& host,
                         int port,
                         const ProxyOptions& options,
                         std::string& error,
                         const CancellationRequest& cancelled)
    {
        if (!validate(options, error)) return false;
        if (port < 1 || port > 65535 || host.empty())
        {
            error = "Invalid SOCKS5 destination";
            return false;
        }
        std::vector<uint8_t> address;
        if (!numericAddress(host, address))
        {
            if (options.dnsMode == ProxyOptions::DnsMode::Local)
            {
                auto resolved = resolveHost(host, port, error, cancelled);
                if (resolved.empty() || !numericAddress(resolved, address)) return false;
            }
            else
            {
                if (host.size() > 255)
                {
                    error = "SOCKS5 destination hostname exceeds 255 bytes";
                    return false;
                }
                address = {3, static_cast<uint8_t>(host.size())};
                address.insert(address.end(), host.begin(), host.end());
            }
        }
        const bool credentials = !options.username.empty();
        std::vector<uint8_t> greeting = {5, static_cast<uint8_t>(credentials ? 2 : 1), 0};
        if (credentials) greeting.push_back(2);
        uint8_t response[4];
        if (!transfer(fd, greeting.data(), greeting.size(), false, error, cancelled) ||
            !transfer(fd, response, 2, true, error, cancelled))
            return false;
        if (response[0] != 5 || (response[1] != 0 && !(credentials && response[1] == 2)))
        {
            error = "SOCKS5 proxy rejected the offered authentication method";
            return false;
        }
        if (response[1] == 2)
        {
            std::vector<uint8_t> authentication = {1,
                                                   static_cast<uint8_t>(options.username.size())};
            authentication.insert(
                authentication.end(), options.username.begin(), options.username.end());
            authentication.push_back(static_cast<uint8_t>(options.password.size()));
            authentication.insert(
                authentication.end(), options.password.begin(), options.password.end());
            if (!transfer(
                    fd, authentication.data(), authentication.size(), false, error, cancelled) ||
                !transfer(fd, response, 2, true, error, cancelled))
                return false;
            if (response[0] != 1 || response[1] != 0)
            {
                error = "SOCKS5 username/password authentication failed";
                return false;
            }
        }
        std::vector<uint8_t> request = {5, 1, 0};
        request.insert(request.end(), address.begin(), address.end());
        request.push_back(static_cast<uint8_t>(port >> 8));
        request.push_back(static_cast<uint8_t>(port & 255));
        if (!transfer(fd, request.data(), request.size(), false, error, cancelled) ||
            !transfer(fd, response, 4, true, error, cancelled))
            return false;
        if (response[0] != 5 || response[2] != 0)
        {
            error = "Invalid SOCKS5 CONNECT reply";
            return false;
        }
        if (response[1] != 0)
        {
            error = "SOCKS5 CONNECT failed (reply " + std::to_string(response[1]) + ")";
            return false;
        }
        size_t size = 0;
        if (response[3] == 1)
            size = 4;
        else if (response[3] == 4)
            size = 16;
        else if (response[3] == 3)
        {
            uint8_t length;
            if (!transfer(fd, &length, 1, true, error, cancelled)) return false;
            size = length;
            if (!size)
            {
                error = "Invalid SOCKS5 reply hostname length";
                return false;
            }
        }
        else
        {
            error = "Invalid SOCKS5 reply address type";
            return false;
        }
        std::array<uint8_t, 257> bound;
        return transfer(fd, bound.data(), size + 2, true, error, cancelled);
    }
} // namespace ix
