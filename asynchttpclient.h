#pragma once
#include <string>
#include <map>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "httpclientlog.h"
#include "responseinfo.h"
#include "urlparser.h"
#include "httputil.h"



//callback function signature
typedef boost::function<void(const ResponseInfo& r)> HttpClientCallback;



enum HTTP_METHOD
{
    METHOD_UNKNOWN,
    METHOD_POST,
    METHOD_GET,
    METHOD_PUT,
    METHOD_DELETE,
    METHOD_HEAD,
};


//key point: async http client class
//class is designed to make one request in one instance's lifetime
//you may or not get error when making request again
//always HTTP/1.1
class CAsyncHttpClient
{
public:
    //************************************
    // brief:    constructor
    // name:     CAsyncHttpClient::CAsyncHttpClient
    // param:    boost::asio::io_service & io_service
    // param:    const unsigned short timeout           timeout setting, seconds
    // param:    const bool throw_in_cb                 if true, throw when exception in cb, otherwise, no-throw
    // return:   
    // ps:      
    //************************************
    CAsyncHttpClient(boost::asio::io_service& io_serv,
        const unsigned short timeout,
        const bool throw_in_cb = false)
        : m_io_service(io_serv)
        , m_timeout(timeout)
        , m_deadline_timer(io_serv)
        , m_cb_called(false)
        , m_throw_in_cb(throw_in_cb)
        , m_method(METHOD_UNKNOWN)
        , m_timeout_cb_called(false)
        , m_sock(m_io_service)
    {
    }

    //************************************
    // brief:    destructor
    // name:
    // return:
    // ps:       if have not called cb and no error, call cb with error "abandoned"
    //************************************
    ~CAsyncHttpClient()
    {
        if (m_response.error_msg.empty())
        {
            m_response.error_msg = "abandoned";
        }
        DoCallback();
    }


    //************************************
    // brief:    make request
    // name:     CAsyncHttpClient::makePost
    // param:    HttpClientCallback cb                                  callback function
    // param:    const std::string & url                                request url
    // param:    const std::map<std::string, std::string> & headers     headers info of key-value style
    // param:    const std::string & query_param                        query param. usually appended to url after "?"
    // param:    std::string & body                                     post data
    // return:   void
    // ps:       no return value, if error, it will throw
    //************************************
    void makePost(HttpClientCallback cb, const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& query_param,
        const std::string& body)
    {
        makeRequest(cb, METHOD_POST, url, headers, query_param, body);
    }

    //see above
    void makeGet(HttpClientCallback cb, const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& query_param)
    {
        makeRequest(cb, METHOD_GET, url, headers, query_param, "");
    }

    //see above
    void makePut(HttpClientCallback cb, const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& query_param,
        const std::string& body)
    {
        makeRequest(cb, METHOD_PUT, url, headers, query_param, body);
    }

    //see above
    void makeDelete(HttpClientCallback cb, const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& query_param,
        const std::string& body)
    {
        makeRequest(cb, METHOD_DELETE, url, headers, query_param, body);
    }


private:
    void makeRequest(HttpClientCallback cb,
        const HTTP_METHOD m, const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& query_param,
        const std::string& body)
    {
        m_cb = cb;
        m_method = m;
        m_urlparser.Parse(url);

        //construct complete query_param
        std::string query_param_all;
        if (!m_urlparser.query_param.empty() && !query_param.empty())
        {
            query_param_all = m_urlparser.query_param + "&" + query_param;
        }
        else
        {
            query_param_all = m_urlparser.query_param + query_param;
        }

        m_request_string = build_request_string(m_urlparser.host_all,
            m_urlparser.path, query_param_all, m_method,
            headers, body);
        HTTP_CLIENT_INFO << "request_string:\r\n" << m_request_string;

        //start timeout
        m_timeout_cb_called = false;
        m_deadline_timer.expires_from_now(boost::posix_time::seconds(m_timeout));
        m_deadline_timer.async_wait(boost::bind(&CAsyncHttpClient::timeout_cb, this,
            boost::asio::placeholders::error));

        m_cb_called = false;
        boost::asio::spawn(m_io_service, boost::bind(&CAsyncHttpClient::yield_func, this, _1));
    }

    void yield_func(boost::asio::yield_context yield)
    {
        do 
        {
            boost::system::error_code ec;

            boost::asio::ip::tcp::resolver rs(m_io_service);

            std::string query_host(m_urlparser.host_part);
            std::string query_serivce(m_urlparser.service);
            boost::asio::ip::tcp::resolver::query q(query_host, query_serivce);
            boost::asio::ip::tcp::resolver::iterator ep_iter = rs.async_resolve(q, yield[ec]);
            if (ec)
            {
                m_response.error_msg = "can not resolve addr which has host=";
                m_response.error_msg += query_host + " and service="
                    + query_serivce + ", error:" + ec.message();
                HTTP_CLIENT_ERROR << m_response.error_msg;
                break;
            }

            boost::asio::async_connect(m_sock, ep_iter, yield[ec]);
            if (ec)
            {
                m_response.error_msg = "can not connect to addr which has host=";
                m_response.error_msg += query_host + " and service=" + query_serivce + ", error:" + ec.message();
                HTTP_CLIENT_ERROR << m_response.error_msg;
                break;
            }

            boost::asio::async_write(m_sock, boost::asio::buffer(m_request_string), yield[ec]);
            if (ec)
            {
                m_response.error_msg = "can not send data to addr which has host=";
                m_response.error_msg += query_host + " and service=" + query_serivce + ", error:" + ec.message();
                HTTP_CLIENT_ERROR << m_response.error_msg;
                break;
            }

            std::string content_when_header;
            {
                /*
                see http://www.boost.org/doc/libs/1_58_0/doc/html/boost_asio/reference/async_read_until/overload1.html
                After a successful async_read_until operation,
                the streambuf may contain additional data beyond the delimiter.
                An application will typically leave that data in the streambuf
                for a subsequent async_read_until operation to examine.
                */
                boost::asio::streambuf response_buf;
                boost::asio::async_read_until(m_sock, response_buf, "\r\n\r\n", yield[ec]);
                if (ec)
                {
                    m_response.error_msg = "can not recv response header, error:" + ec.message();
                    HTTP_CLIENT_ERROR << m_response.error_msg;
                    break;
                }

                std::stringstream ss;
                ss << &response_buf;
                std::string headers_contained = ss.str();
                size_t headers_pos = headers_contained.find("\r\n\r\n");
                assert(headers_pos != std::string::npos);
                std::string headers_exactly = headers_contained.substr(0, headers_pos + 4);
                content_when_header = headers_contained.substr(headers_pos + 4);

                feed_response(headers_exactly, "");
                HTTP_CLIENT_INFO << "response headers:\r\n" << m_response.raw_response;
                if (!parse_response_headers(m_response.raw_response, m_response))
                {
                    m_response.error_msg = "can not parse response header, invalid header, header:\r\n"
                        + m_response.raw_response;
                    HTTP_CLIENT_ERROR << m_response.error_msg;
                    break;
                }
            }

            /*
            see http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4
            1. check status code if 1xx, 204, and 304, check request methon if HEAD, no content
            2. check if contained Transfer-Encoding, and chunked, recv chunked content
                see https://zh.wikipedia.org/zh-cn/分块传输编码
            3. check if contained Transfer-Encoding, and no chunked, recv content until eof
            4. check if contained Content-Length, recv content with exactly length
            5. else, recv until eof
            ps: do not consider "multipart/byteranges"
            */

            if ((0 == (m_response.status_code / 200) && 1 == (m_response.status_code / 100))
                || 204 == m_response.status_code
                || 304 == m_response.status_code
                || METHOD_HEAD == m_method)
            {
                HTTP_CLIENT_INFO <<"no content";
            }
            else if (m_response.headers.count("transfer-encoding")
                && boost::algorithm::icontains(m_response.headers["transfer-encoding"], "chunked"))
            {
                HTTP_CLIENT_INFO << "chunked content";

                std::string chunk_content;
                if (!content_when_header.empty())
                {
                    std::string content;
                    if (reach_chunk_end(content_when_header, chunk_content, content))
                    {
                        feed_response(chunk_content, content);
                        break;
                    }
                }

                while (true)
                {
                    boost::asio::streambuf response_buf;
                    boost::asio::async_read(m_sock, response_buf,
                        boost::asio::transfer_at_least(1),
                        yield[ec]);
                    if (ec)
                    {
                        if (ec != boost::asio::error::eof)
                        {
                            m_response.error_msg = ec.message();
                        }
                        break;
                    }

                    std::string content;
                    if (reach_chunk_end(response_buf, chunk_content, content))
                    {
                        feed_response(chunk_content, content);
                        break;
                    }
                }
            }
            else if (0 == m_response.headers.count("transfer-encoding") && m_response.headers.count("content-length"))
            {
                HTTP_CLIENT_INFO << "content with content-length";

                feed_response(content_when_header, content_when_header);
                size_t content_length = boost::lexical_cast<size_t>(m_response.headers["content-length"]);
                if (content_when_header.size() < content_length)
                {
                    boost::asio::streambuf response_buf;
                    boost::asio::async_read(m_sock, response_buf,
                        boost::asio::transfer_at_least(content_length - content_when_header.size()),
                        yield[ec]);
                    if (ec)
                    {
                        m_response.error_msg = ec.message();
                        break;
                    }
                    feed_response(response_buf, true);
                }
            }
            else
            {
                HTTP_CLIENT_INFO << "recv content till closed";

                feed_response(content_when_header, content_when_header);
                while (true)
                {
                    boost::asio::streambuf response_buf;
                    boost::asio::async_read(m_sock, response_buf,
                        boost::asio::transfer_at_least(1),
                        yield[ec]);
                    if (ec)
                    {
                        if (ec != boost::asio::error::eof)
                        {
                            m_response.error_msg = ec.message();
                        }
                        break;
                    }
                    feed_response(response_buf, true);
                }
            }

            HTTP_CLIENT_INFO << "response content:\r\n" << m_response.content;

        } while (false);

        {
            boost::lock_guard<boost::mutex> cb_lock(m_cb_mutex);
            DoCallback();
        }
    }


    //always HTTP/1.1
    static std::string build_request_string(const std::string host,
        const std::string& path, const std::string& query_param,
        const HTTP_METHOD m, const std::map<std::string, std::string>& headers,
        const std::string& body)
    {
        std::stringstream ss;

        switch (m)
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

        default:
            break;
        }

        ss << path;
        if (!query_param.empty())
        {
            ss << "?" << query_param;
        }
        ss << " HTTP/1.1" << "\r\n";

        for (std::map<std::string, std::string>::const_iterator iterKey = headers.begin();
            iterKey != headers.end();
            ++iterKey)
        {
            ss << iterKey->first << ": " << iterKey->second << "\r\n";
        }
        if (0 == headers.count("Host"))
        {
            ss << "Host: " << host << "\r\n";
        }
        if (0 == headers.count("Content-Length") && !body.empty())
        {
            ss << "Content-Length: " << body.size() << "\r\n";
        }

        ss << "\r\n";
        ss << body;

        return ss.str();
    }


    static bool parse_response_headers(const std::string& s, ResponseInfo& r)
    {
        bool bReturn = false;
        do 
        {
            std::stringstream ss(s);
            ss >> r.http_version;
            ss >> r.status_code;
            std::getline(ss, r.status_msg);
            boost::algorithm::trim(r.status_msg);

            if (!ss)
            {
                HTTP_CLIENT_ERROR << "can not get status line";
                break;
            }

            while (ss)
            {
                std::string per_line;
                std::getline(ss, per_line);
                size_t pos = per_line.find(':');
                if (std::string::npos == pos)
                {
                    continue;
                }
                std::string key = per_line.substr(0, pos);
                boost::algorithm::trim(key);
                if (key.empty())
                {
                    HTTP_CLIENT_WARN << "encountered an empty key";
                    continue;
                }
                boost::algorithm::to_lower(key);
                std::string value = per_line.substr(pos + 1);
                boost::algorithm::trim(value);
                r.headers[key] = value;
            }

            bReturn = true;

        } while (false);

        return bReturn;
    }

    //************************************
    // brief:    check if reached ending chunk
    // name:     CHttpClient::reach_chunk_end
    // param:    std::string & cur_chunk        current chunk
    // param:    std::string & all_chunk        cur_chunk will be appended to all_chunk, using all_chunk to check
    // param:    std::string & content          if ending, contains all content parsed from all_chunk, otherwise not-defined
    // return:   bool
    // ps:
    //************************************
    static bool reach_chunk_end(const std::string& cur_chunk, std::string& all_chunk, std::string& content)
    {
        HTTP_CLIENT_INFO << "response chunk:\r\n" << cur_chunk;

        all_chunk += cur_chunk;
        content.clear();

        bool reach_end = false;
        size_t pos = 0;
        while (true)
        {
            //next \r\n
            size_t next_pos = all_chunk.find("\r\n", pos);
            if (std::string::npos == next_pos)
            {
                break;
            }
            std::string chunk_size_str = all_chunk.substr(pos, next_pos - pos);
            boost::algorithm::trim(chunk_size_str);
            if (chunk_size_str.empty())
            {
                pos = next_pos + 2;
                continue;
            }

            //chunk size
            unsigned long chunk_size = strtoul(chunk_size_str.c_str(), NULL, 16);
            if (0 == chunk_size)
            {
                reach_end = true;
                break;
            }

            content += all_chunk.substr(next_pos + 2, chunk_size);
            pos = next_pos + 2 + chunk_size + 2;//\r\n before and after
        }

        return reach_end;
    }

    //cur_buf can not be const, or you will get an address???
    static bool reach_chunk_end(boost::asio::streambuf& cur_buf, std::string& all_chunk, std::string& content)
    {
        std::stringstream ss;
        ss << &cur_buf;
        std::string cur_chunk = ss.str();
        return reach_chunk_end(cur_chunk, all_chunk, content);
    }


    void feed_response(const std::string& raw, const std::string& content)
    {
        m_response.raw_response += raw;
        m_response.content += content;
    }

    //cur_buf can not be const, or you will get an address???
    void feed_response(boost::asio::streambuf& buf, const bool is_content = false)
    {
        std::stringstream ss;
        ss << &buf;
        std::string s = ss.str();
        if (is_content)
        {
            feed_response(s, s);
        }
        else
        {
            feed_response(s, "");
        }
    }


    void timeout_cb(const boost::system::error_code &ec)
    {
        boost::lock_guard<boost::mutex> cb_lock(m_cb_mutex);
        m_timeout_cb_called = true;
        if (ec)
        {
            if (ec != boost::asio::error::operation_aborted)
            {
                m_response.error_msg = "timeout callback encountered an error:" + ec.message();
                HTTP_CLIENT_ERROR << m_response.error_msg;
                DoCallback();
            }
            else
            {
                HTTP_CLIENT_INFO << "timeout callback was canceled";
            }
        }
        else
        {
            HTTP_CLIENT_ERROR << "timeout";
            m_response.timeout = true;
            m_response.error_msg = "timeout";
            boost::system::error_code ec_close;
            m_sock.close(ec_close);
            DoCallback();
        }
    }

    void DoCallback()
    {
        if (!m_cb_called)
        {
            m_cb_called = true;

            if (!m_timeout_cb_called)
            {
                boost::system::error_code ec_cancel;
                m_deadline_timer.cancel(ec_cancel);
            }

            try
            {
                m_cb(m_response);
            }
            catch (...)
            {
                HTTP_CLIENT_ERROR << "exception happened in callback function";
                if (m_throw_in_cb)
                {
                    HTTP_CLIENT_INFO << "throw";
                    throw;
                }
            }
        }
    }


private:
    boost::asio::io_service& m_io_service;
    const unsigned short m_timeout;
    HttpClientCallback m_cb;
    boost::asio::deadline_timer m_deadline_timer;
    bool m_cb_called;
    bool m_throw_in_cb;
    HTTP_METHOD m_method;
    UrlParser m_urlparser;
    bool m_timeout_cb_called;
    boost::mutex m_cb_mutex;

    boost::asio::ip::tcp::socket m_sock;
    std::string m_request_string;

    ResponseInfo m_response;
};



