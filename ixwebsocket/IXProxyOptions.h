#pragma once

#include <string>

enum class SocksVersion
{
    SOCKS4 = 0,
    SOCKS4a = 1,
    SOCKS5 = 2,
    SOCKS5h = 3,
};

struct ProxyOptions
{
public:
    std::string host;
    unsigned short port;
    SocksVersion socksVersion;
    std::string userId;
    std::string username;
    std::string password;
    bool auth;
};
