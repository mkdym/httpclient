#pragma once
#include <string>
#include <map>



class CAsyncHttpClient;
class HttpRequest
{
    friend class CAsyncHttpClient;

public:
    void SetUrl(const std::string& url)
    {
        m_url = url;
    }

    void SetQueryParam(const std::string& query_param)
    {
        m_query_param = query_param;
    }

    void AddHeader(const std::string& key, const std::string& value)
    {
        m_headers[key] = value;
    }

    void AddBody(const std::string& s)
    {
        m_body += s;
    }

    void AddBody(const void *p, const size_t len)
    {
        m_body += std::string(reinterpret_cast<const char *>(p), len);
    }

private:
    std::string m_url;
    std::string m_query_param;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
};


