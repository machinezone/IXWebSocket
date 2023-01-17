#ifndef ProxySoctekSettings_h
#define ProxySoctekSettings_h

#include <iostream>

enum class ProxyConnectionType {

    connection_none = 0x00,
    connection_web = 0x20,
    connection_sock4 = 0x04,
    connection_sock5 = 0x05

};

struct ProxySetup {

public:


    void setProxyPass(std::string &proxypass);
    void setProxyUser(std::string &proxyuser);
    void setProxyHost(std::string &proxyhost);
    void setProxyType(ProxyConnectionType &proxytype) ;
    void setProxyPort(int port_num);
    int get_proxy_type() const;
    int get_proxy_port() const;
    std::string get_proxy_pass() const;
    std::string get_proxy_user() const;
    std::string get_proxy_host() const;


private:

    std::string proxyPass;
    std::string proxyUser;
    std::string proxyHost;
    ProxyConnectionType ProxyConnType = ProxyConnectionType::connection_none;
    int proxyPort = 8080;

};

#endif