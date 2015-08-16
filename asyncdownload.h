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
    }

    ~CAsyncHttpDownload()
    {
    }

public:
    //ps: content of responseinfo will be empty
    bool download(const RequestInfo& req,
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

            m_async_client.make_request(req, response_cb, default_headers_cb,
                boost::bind(&CAsyncHttpDownload::content_cb, this, _1));

            success = true;

        } while (false);
        return success;
    }

private:
    bool content_cb(std::string& cur_content)
    {
        m_file << cur_content;
        cur_content.clear();
        return true;
    }

private:
    CAsyncHttpClient m_async_client;
    boost::filesystem::fstream m_file;
};


