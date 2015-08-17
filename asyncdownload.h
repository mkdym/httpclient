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
        ResponseCallback response_cb)
    {
        m_filename = filename;
        m_async_client.make_request(req, response_cb, default_headers_cb,
            boost::bind(&CAsyncHttpDownload::content_cb, this, _1, _2));
    }

private:
    bool content_cb(std::string& cur_content, std::string& error_msg)
    {
        error_msg.clear();
        if (!m_file.is_open() && !prepare_file(m_filename, error_msg))
        {
            return false;
        }
        else
        {
            try
            {
                m_file << cur_content;
            }
            catch (std::exception& e)
            {
                error_msg = "write to file fail, error: ";
                error_msg += e.what();
            }

            if (error_msg.empty())
            {
                cur_content.clear();
                return true;
            }
            else
            {
                HTTP_CLIENT_ERROR << error_msg;
                return false;
            }
        }
    }

    bool prepare_file(const std::string& filename, std::string& error_msg)
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
                    error_msg = "create dir[";
                    error_msg += p.parent_path().string() + "] error: " + ec.message();
                    HTTP_CLIENT_ERROR << error_msg;
                    break;
                }
            }

            m_file.open(p, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
            unsigned long error_code = HTTP_OS_DEFINE::get_last_error();
            if (!m_file.is_open())
            {
                error_msg = "open file[";
                error_msg += p.string() + "] fail, error code: "
                    + boost::lexical_cast<std::string>(error_code);
                HTTP_CLIENT_ERROR << error_msg;
                break;
            }
            HTTP_CLIENT_DEBUG << "open file[" << p << "] for downloading success";
            success = true;

        } while (false);
        return success;
    }

private:
    const bool m_throw_in_cb;
    CAsyncHttpClient m_async_client;
    std::string m_filename;
    boost::filesystem::fstream m_file;
};


