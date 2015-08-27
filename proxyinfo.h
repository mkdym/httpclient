#pragma once
#include <string>



enum PROXY_TYPE
{
    PROXY_NO,
    PROXY_HTTP,
    //PROXY_SOCKS5,
    //PROXY_SOCKS4,
};

struct ProxyInfo
{
    PROXY_TYPE type;
    std::string server;
    unsigned short port;
    //std::string user_name;
    //std::string password;
};


