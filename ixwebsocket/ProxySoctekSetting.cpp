#include "ProxySoctekSetting.h"

#include <iostream>

void ProxySetup::setProxyPass(std::string &proxypass) {

    proxyPass = proxypass;
}


void ProxySetup::setProxyUser(std::string &proxyuser) {

    proxyUser = proxyuser;
}


void ProxySetup::setProxyHost(std::string &proxyhost) {

    proxyHost = proxyhost;
}


int ProxySetup::get_proxy_type() const
{

    return static_cast<int>(ProxyConnType);
}

std::string ProxySetup::get_proxy_user() const
{

    return proxyUser;
}

int ProxySetup::get_proxy_port() const
{

    return proxyPort;
}

std::string ProxySetup::get_proxy_pass() const
{

    return proxyPass;
}

std::string ProxySetup::get_proxy_host() const
{

    return proxyHost;
}

void ProxySetup::setProxyPort(int port_num)
{

    proxyPort =  port_num;
}

void ProxySetup::setProxyType(ProxyConnectionType& proxytype)
{

    ProxyConnType = proxytype;
}
