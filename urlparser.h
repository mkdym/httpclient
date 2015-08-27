#pragma once
#include <string>
#include <boost/algorithm/string.hpp>

#include "httpclientlog.h"



//helper calss
//parse url
struct UrlParser
{
    //host name. contains port number if has
    std::string host_all;
    //path, defalut "/"
    std::string path;
    //host name. does not contain port number. for getaddrinfo
    std::string host_part;
    //query param. usually after path in url, and separated by "?" with path
    std::string query_param;
    //port number. 0 if not specified
    unsigned short port;
    //protocol, always before "://" in url, http or https,...
    std::string proto;


    UrlParser()
        : port(0)
    {
    }


    void parse(const std::string& url)
    {
        size_t proto_pos = url.find("://");
        if (std::string::npos != proto_pos)
        {
            proto = url.substr(0, proto_pos);
            boost::algorithm::to_lower(proto);
        }

        proto_pos = (proto_pos == std::string::npos ? 0 : proto_pos + 3);
        size_t host_pos = url.find_first_of("/?", proto_pos);
        if (std::string::npos == host_pos)
        {
            host_all = url.substr(proto_pos);
            path = "/";
        }
        else
        {
            host_all = url.substr(proto_pos, host_pos - proto_pos);
            path = url.substr(host_pos);
        }

        size_t param_pos = path.find('?');
        if (std::string::npos != param_pos)
        {
            query_param = path.substr(param_pos + 1);
            path = path.substr(0, param_pos);
        }
        if (path.empty())
        {
            path = "/";
        }

        size_t port_pos = host_all.find(':');
        if (std::string::npos != port_pos)
        {
            host_part = host_all.substr(0, port_pos);
            std::string port_str = host_all.substr(port_pos + 1);
            port = static_cast<unsigned short>(strtoul(port_str.c_str(), NULL, 10));
            if (0 == port)
            {
                HTTP_CLIENT_ERROR << "port str[" << port_str << "] can not be converted to number, set port number 0";
            }
        }
        else
        {
            host_part = host_all;
        }

        HTTP_CLIENT_DEBUG << "url[" << url << "] parse result:\r\n"
            << "host_all=" << host_all
            << ", path=" << path
            << ", host_part=" << host_part
            << ", query_param=" << query_param
            << ", port=" << port
            << ", proto=" << proto;
    }
};




