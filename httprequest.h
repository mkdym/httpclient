#pragma once
#include <string>
#include <map>

#include "urlparser.h"




enum HTTP_METHOD
{
    METHOD_UNKNOWN,
    METHOD_POST,
    METHOD_GET,
    METHOD_PUT,
    METHOD_DELETE,
    METHOD_HEAD,
};



class CAsyncHttpClient;
class HttpRequest
{
    friend class CAsyncHttpClient;

public:
    HttpRequest()
        : m_method(METHOD_GET)
        , m_http_ver("HTTP/1.1")
    {
    }

    void set_url(const std::string& url)
    {
        m_urlparser.parse(url);
    }

    void set_method(const HTTP_METHOD& m)
    {
        m_method = m;
    }

    void set_http_ver(const std::string& v)
    {
        m_http_ver = v;
    }

    void set_query_param(const std::string& query_param)
    {
        m_query_param = query_param;
    }

    void add_header(const std::string& key, const std::string& value)
    {
        m_headers[key] = value;
    }

    void add_body(const std::string& s)
    {
        m_body += s;
    }

    void add_body(const void *p, const size_t len)
    {
        m_body += std::string(reinterpret_cast<const char *>(p), len);
    }

    const std::string& gethostname() const
    {
        return m_urlparser.host_part;
    }

    const std::string& getservicename() const
    {
        return m_urlparser.service;
    }

    std::string build_as_string() const
    {
        std::stringstream ss;

        switch (m_method)
        {
        case METHOD_POST:
            ss << "POST ";
            break;

        case METHOD_GET:
            ss << "GET ";
            break;

        case METHOD_PUT:
            ss << "PUT ";
            break;

        case METHOD_DELETE:
            ss << "DELETE ";
            break;

        case METHOD_HEAD:
            ss << "HEAD ";
            break;

        default:
            break;
        }

        //construct complete query_param
        std::string query_param_all;
        if (!m_urlparser.query_param.empty() && !m_query_param.empty())
        {
            query_param_all = m_urlparser.query_param + "&" + m_query_param;
        }
        else
        {
            query_param_all = m_urlparser.query_param + m_query_param;
        }

        ss << m_urlparser.path;
        if (!query_param_all.empty())
        {
            ss << "?" << query_param_all;
        }
        ss << " " << m_http_ver << "\r\n";

        for (std::map<std::string, std::string>::const_iterator iterKey = m_headers.begin();
            iterKey != m_headers.end();
            ++iterKey)
        {
            ss << iterKey->first << ": " << iterKey->second << "\r\n";
        }
        if (0 == m_headers.count("Host"))
        {
            ss << "Host: " << m_urlparser.host_all << "\r\n";
        }
        if (0 == m_headers.count("Content-Length") && !m_body.empty())
        {
            ss << "Content-Length: " << m_body.size() << "\r\n";
        }

        ss << "\r\n";
        ss << m_body;

        return ss.str();
    }

private:
    HTTP_METHOD m_method;
    std::string m_http_ver;
    std::string m_query_param;
    std::map<std::string, std::string> m_headers;
    std::string m_body;

    UrlParser m_urlparser;
};


