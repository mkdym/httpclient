#pragma once
#include <boost/lexical_cast.hpp>
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
        : m_throw_in_cb(throw_in_cb)
        , m_async_client(io_serv, timeout, throw_in_cb)
    {
    }

    ~CAsyncHttpDownload()
    {
    }

public:
    //ps: content of responseinfo will be empty
    //may call response_cb in function
    void download(const RequestInfo& req,
        const std::string& filename,
        HttpClientCallback response_cb)
    {
        ResponseInfo error_response;
        do 
        {
            boost::filesystem::path p(filename);
            if (p.has_parent_path())
            {
                boost::system::error_code ec;
                boost::filesystem::create_directories(p.parent_path(), ec);
                if (ec)
                {
                    error_response.error_msg = "create dir[";
                    error_response.error_msg += p.parent_path().string() + "] error: " + ec.message();
                    HTTP_CLIENT_ERROR << error_response.error_msg;
                    break;
                }
            }

            m_file.open(p, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
            unsigned long error_code = HTTP_OS_DEFINE::get_last_error();
            if (!m_file.is_open())
            {
                error_response.error_msg = "open file[";
                error_response.error_msg += p.string() + "] fail, error code: "
                    + boost::lexical_cast<std::string>(error_code);
                HTTP_CLIENT_ERROR << error_response.error_msg;
                break;
            }
            HTTP_CLIENT_INFO << "open file[" << p << "] for downloading success";

        } while (false);
        if (!error_response.error_msg.empty())
        {
            try
            {
                response_cb(error_response);
            }
            catch (...)
            {
                HTTP_CLIENT_ERROR << "exception happened in response callback function";
                if (m_throw_in_cb)
                {
                    HTTP_CLIENT_INFO << "throw";
                    throw;
                }
            }
        }
        else
        {
            m_async_client.make_request(req, response_cb, default_headers_cb,
                boost::bind(&CAsyncHttpDownload::content_cb, this, _1));
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
    const bool m_throw_in_cb;
    CAsyncHttpClient m_async_client;
    boost::filesystem::fstream m_file;
};


