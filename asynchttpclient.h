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
#include <boost/atomic.hpp>
#include <boost/thread/condition.hpp>

#include "httpclientlog.h"
#include "responseinfo.h"
#include "urlparser.h"
#include "httputil.h"
#include "scopedcounter.h"



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
        , m_throw_in_cb(throw_in_cb)
        , m_sock(m_io_service)
        , m_deadline_timer(io_serv)
        , m_cb_called(false)
        , m_counter_busy(0)
        , m_method(METHOD_UNKNOWN)
    {
    }

    //wait for stop
    ~CAsyncHttpClient()
    {
        boost::system::error_code ec;
        m_deadline_timer.cancel(ec);
        m_sock.close(ec);

        boost::lock_guard<boost::mutex> lock_busy(m_mutex_busy);
        while (m_counter_busy)
        {
            HTTP_CLIENT_INFO << "wait not-busy notify";
            m_cond_busy.wait(m_mutex_busy);
            HTTP_CLIENT_INFO << "got not-busy notify";
        }
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
        m_deadline_timer.expires_from_now(boost::posix_time::seconds(m_timeout));
        m_deadline_timer.async_wait(boost::bind(&CAsyncHttpClient::timeout_cb, this,
            boost::asio::placeholders::error));
        boost::asio::spawn(m_io_service, boost::bind(&CAsyncHttpClient::yield_func, this, _1));
    }

    void yield_func(boost::asio::yield_context yield)
    {
        scoped_counter_cond<int, boost::mutex, boost::condition> counter_cond(m_counter_busy, m_mutex_busy, m_cond_busy);

        std::string error_msg;
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
                error_msg = "can not resolve addr which has host=";
                error_msg += query_host + " and service="
                    + query_serivce + ", error:" + ec.message();
                HTTP_CLIENT_ERROR << error_msg;
                break;
            }

            boost::asio::async_connect(m_sock, ep_iter, yield[ec]);
            if (ec)
            {
                error_msg = "can not connect to addr which has host=";
                error_msg += query_host + " and service=" + query_serivce + ", error:" + ec.message();
                HTTP_CLIENT_ERROR << error_msg;
                break;
            }

            boost::asio::async_write(m_sock, boost::asio::buffer(m_request_string), yield[ec]);
            if (ec)
            {
                error_msg = "can not send data to addr which has host=";
                error_msg += query_host + " and service=" + query_serivce + ", error:" + ec.message();
                HTTP_CLIENT_ERROR << error_msg;
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
                    error_msg = "can not recv response header, error:" + ec.message();
                    HTTP_CLIENT_ERROR << error_msg;
                    break;
                }

                std::stringstream ss;
                ss << &response_buf;
                std::string headers_contained = ss.str();
                size_t headers_pos = headers_contained.find("\r\n\r\n");
                assert(headers_pos != std::string::npos);
                std::string headers_exactly = headers_contained.substr(0, headers_pos + 4);
                content_when_header = headers_contained.substr(headers_pos + 4);

                HTTP_CLIENT_INFO << "response headers:\r\n" << headers_exactly;
                if (!parse_response_headers(headers_exactly, m_response))
                {
                    error_msg = "can not parse response header, invalid header:\r\n"
                        + headers_exactly;
                    HTTP_CLIENT_ERROR << error_msg;
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

                std::string all_chunk_content;
                if (!content_when_header.empty())
                {
                    HTTP_CLIENT_INFO << "response chunk:\r\n" << content_when_header;
                    all_chunk_content += content_when_header;

                    std::string content;
                    if (reach_chunk_end(all_chunk_content, m_response.content))
                    {
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
                            error_msg = ec.message();
                        }
                        break;
                    }

                    std::stringstream cur_ss;
                    cur_ss << &response_buf;
                    HTTP_CLIENT_INFO << "response chunk:\r\n" << cur_ss.str();
                    all_chunk_content += cur_ss.str();

                    std::string content;
                    if (reach_chunk_end(all_chunk_content, m_response.content))
                    {
                        break;
                    }
                }
            }
            else if (0 == m_response.headers.count("transfer-encoding") && m_response.headers.count("content-length"))
            {
                HTTP_CLIENT_INFO << "content with content-length";

                m_response.content += content_when_header;
                size_t content_length = boost::lexical_cast<size_t>(m_response.headers["content-length"]);
                if (content_when_header.size() < content_length)
                {
                    boost::asio::streambuf response_buf;
                    boost::asio::async_read(m_sock, response_buf,
                        boost::asio::transfer_at_least(content_length - content_when_header.size()),
                        yield[ec]);
                    if (ec)
                    {
                        error_msg = ec.message();
                        break;
                    }

                    std::stringstream cur_ss;
                    cur_ss << &response_buf;
                    m_response.content += cur_ss.str();
                }
            }
            else
            {
                HTTP_CLIENT_INFO << "recv content till closed";

                m_response.content += content_when_header;
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
                            error_msg = ec.message();
                        }
                        break;
                    }
                    std::stringstream cur_ss;
                    cur_ss << &response_buf;
                    m_response.content += cur_ss.str();
                }
            }

            HTTP_CLIENT_INFO << "response content:\r\n" << m_response.content;

        } while (false);

        DoCallback(error_msg);
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

        case METHOD_HEAD:
            ss << "HEAD ";
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
    // name:     CAsyncHttpClient::reach_chunk_end
    // param:    std::string & all_chunk
    // param:    std::string & content          if ending, contains all content parsed from all_chunk, otherwise not-defined
    // return:   bool
    // ps:
    //************************************
    static bool reach_chunk_end(const std::string& all_chunk, std::string& content)
    {
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


    void timeout_cb(const boost::system::error_code &ec)
    {
        scoped_counter_cond<int, boost::mutex, boost::condition> counter_cond(m_counter_busy, m_mutex_busy, m_cond_busy);

        if (ec)
        {
            if (ec != boost::asio::error::operation_aborted)
            {
                std::string error_msg = "timeout callback encountered an error:" + ec.message();
                HTTP_CLIENT_ERROR << error_msg;
                DoCallback(error_msg);
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
            DoCallback("");
        }
    }


    void DoCallback(const std::string& error_msg)
    {
        //确保不会执行回调多次
        bool called_expected = false;
        if (m_cb_called.compare_exchange_strong(called_expected, true))
        {
            assert(!called_expected);
            boost::system::error_code ec;
            m_deadline_timer.cancel(ec);
            m_sock.close(ec);
            m_response.error_msg = error_msg;

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
    const bool m_throw_in_cb;

    boost::asio::ip::tcp::socket m_sock;

    boost::asio::deadline_timer m_deadline_timer;
    boost::atomic_bool m_cb_called;

    boost::condition m_cond_busy;
    boost::mutex m_mutex_busy;
    int m_counter_busy;

    HTTP_METHOD m_method;
    HttpClientCallback m_cb;
    UrlParser m_urlparser;

    std::string m_request_string;

    ResponseInfo m_response;
};



