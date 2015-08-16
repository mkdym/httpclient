#pragma once
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "osdefine.h"
#include "asynchttpclient.h"



class CAsyncHttpDownload
{
public:
    CAsyncHttpDownload(boost::asio::io_service& io_serv,
        const unsigned short timeout,
        const bool throw_in_cb = false)
        : m_async_client(io_serv, timeout, throw_in_cb)
    {
        m_req.set_method(METHOD_GET);
    }

    ~CAsyncHttpDownload()
    {
    }

public:
    void set_method(const HTTP_METHOD& m)
    {
        m_req.set_method(m);
    }

    void set_http_ver(const std::string& v)
    {
        m_req.set_http_ver(v);
    }

    void add_header(const std::string& key, const std::string& value)
    {
        m_req.add_header(key, value);
    }

    //ps: content of responseinfo will be empty
    bool download(const std::string& url,
        const std::string& filename,
        HttpClientCallback response_cb)
    {
        bool success = false;
        do 
        {
            boost::filesystem::path p(filename);
            if (p.has_parent_path())
            {
                boost::system::error_code ec;
                boost::filesystem::create_directories(p.parent_path(), ec);
                if (ec)
                {
                    HTTP_CLIENT_ERROR << "create dir[" << p.parent_path()
                        << "] error: " << ec.message();
                    break;
                }
            }

            m_file.open(p, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
            unsigned long error_code = HTTP_OS_DEFINE::get_last_error();
            if (!m_file.is_open())
            {
                HTTP_CLIENT_ERROR << "open file[" << p << "] fail, error code: " << error_code;
                break;
            }
            HTTP_CLIENT_INFO << "open file[" << p << "] for downloading success";

            m_req.set_url(url);
            m_async_client.make_request(m_req, response_cb, default_headers_cb,
                boost::bind(&CAsyncHttpDownload::content_cb, this, _1));

            success = true;

        } while (false);
        return success;
    }

private:
    void content_cb(std::string& cur_content)
    {
        m_file << cur_content;
        cur_content.clear();
    }

private:
    CAsyncHttpClient m_async_client;
    RequestInfo m_req;
    boost::filesystem::fstream m_file;
};


