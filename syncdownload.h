#pragma once
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "osdefine.h"
#include "asynchttpclient.h"



class CSyncHttpDownload
{
public:
    CSyncHttpDownload(const unsigned short timeout, const bool throw_in_cb = false)
        : m_sync_client(timeout, throw_in_cb)
    {
    }

    ~CSyncHttpDownload()
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
    const ResponseInfo& download(const std::string& url, const std::string& filename)
    {
        bool has_error = false;
        do 
        {
            boost::filesystem::path p(filename);
            if (p.has_parent_path())
            {
                boost::system::error_code ec;
                boost::filesystem::create_directories(p.parent_path(), ec);
                if (ec)
                {
                    m_error_response.error_msg = "create dir[";
                    m_error_response.error_msg += p.parent_path().string() + "] error: " + ec.message();
                    HTTP_CLIENT_ERROR << m_error_response.error_msg;
                    break;
                }
            }

            m_file.open(p, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
            unsigned long error_code = HTTP_OS_DEFINE::get_last_error();
            if (!m_file.is_open())
            {
                m_error_response.error_msg = "open file[";
                m_error_response.error_msg += p.string() + "] fail, error code: "
                    + boost::lexical_cast<std::string>(error_code);
                HTTP_CLIENT_ERROR << m_error_response.error_msg;
                break;
            }
            HTTP_CLIENT_INFO << "open file[" << p << "] for downloading success";

        } while (false);
        if (has_error)
        {
            return m_error_response;
        }
        else
        {
            m_req.set_url(url);
            return m_sync_client.make_request(m_req, default_headers_cb,
                boost::bind(&CSyncHttpDownload::content_cb, this, _1));
        }
    }

private:
    bool content_cb(std::string& cur_content)
    {
        m_file << cur_content;
        cur_content.clear();
        return true;
    }

private:
    CSyncHttpClient m_sync_client;
    RequestInfo m_req;
    boost::filesystem::fstream m_file;
    ResponseInfo m_error_response;
};


