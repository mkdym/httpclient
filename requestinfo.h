#pragma once
#include <string>
#include <map>
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "httputil.h"
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
class RequestInfo
{
    friend class CAsyncHttpClient;

public:
    //default url_encode the path part in url
    RequestInfo()
        : m_method(METHOD_GET)
        , m_http_ver("HTTP/1.1")
        , m_should_encode_path(true)
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

    //enable/disable path url_encode in build_as_string
    void enbale_path_encode(const bool enabled)
    {
        m_should_encode_path = enabled;
    }

private:
    std::string gethostname() const
    {
        return m_urlparser.host_part;
    }

    //for getaddrinfo
    std::string getservicename() const
    {
        if (m_urlparser.port)//use port
        {
            return boost::lexical_cast<std::string>(m_urlparser.port);
        }
        else if (!m_urlparser.proto.empty())//use proto
        {
            return m_urlparser.proto;
        }
        else//if no port, no proto, use http
        {
            return "http";
        }
    }

    std::string build_as_string() const
    {
        std::stringstream ss;

        switch (m_method)
        {
        case METHOD_POST:
            ss << "POST";
            break;

        case METHOD_GET:
            ss << "GET";
            break;

        case METHOD_PUT:
            ss << "PUT";
            break;

        case METHOD_DELETE:
            ss << "DELETE";
            break;

        case METHOD_HEAD:
            ss << "HEAD";
            break;

        default:
            break;
        }
        ss << " ";

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

        //in the first line, use the complete url for dealing with proxy
        //http://www.jmarshall.com/easy/http/#proxies
        if (!m_urlparser.proto.empty())
        {
            ss << m_urlparser.proto << "://";
        }
        ss << m_urlparser.host_all;

        if (m_should_encode_path)
        {
            std::string encoded_path;
            std::vector<std::string> paths;
            boost::algorithm::split(paths, m_urlparser.path, boost::algorithm::is_any_of("/"));
            for (std::vector<std::string>::const_iterator iter_path = paths.begin();
                iter_path != paths.end();
                ++iter_path)
            {
                encoded_path += HttpUtil::url_encode(*iter_path) + "/";
            }
            boost::algorithm::trim_if(encoded_path, boost::algorithm::is_any_of("/"));
            ss << "/" << encoded_path;
        }
        else
        {
            ss << m_urlparser.path;
        }

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
    bool m_should_encode_path;

    UrlParser m_urlparser;
};


