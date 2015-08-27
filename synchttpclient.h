#pragma once
#include <boost/thread/condition.hpp>
#include "asynchttpclient.h"



class CSyncHttpClient
{
public:
    CSyncHttpClient(const unsigned short timeout, const bool throw_in_cb = false)
        : m_io_serv()
        , m_async_client(m_io_serv, timeout, throw_in_cb)
    {
    }

    ~CSyncHttpClient()
    {
        m_io_serv.stop();
        if (m_run_thread.joinable())
        {
            m_run_thread.join();
        }
    }

public:
    void set_proxy(const ProxyInfo& proxy)
    {
        m_async_client.set_proxy(proxy);
    }

    const ResponseInfo& make_request(const RequestInfo& req,
        HeadersCallback headers_cb = default_headers_cb,
        ContentCallback content_cb = default_content_cb)
    {
        if (!create_io_run_thread())
        {
            return m_response;
        }

        m_async_client.make_request(req, boost::bind(&CSyncHttpClient::cb, this, _1), headers_cb, content_cb);
        return wait();
    }

private:
    void cb(const ResponseInfo& r)
    {
        m_response = r;
        m_cb_cond.notify_one();
    }

    bool create_io_run_thread()
    {
        std::string error_msg;
        try
        {
            m_run_thread = boost::thread(boost::bind(&CSyncHttpClient::io_run, this));
        }
        catch (boost::thread_resource_error& err)
        {
            error_msg = "can not create sync http io run thread, error: ";
            error_msg += err.what();
        }
        if (!error_msg.empty())
        {
            m_response.error_msg = error_msg;
            HTTP_CLIENT_ERROR << error_msg;
            return false;
        }
        else
        {
            return true;
        }
    }

    void io_run()
    {
        boost::asio::io_service::work work(m_io_serv);
        m_io_serv.run();
    }

    const ResponseInfo& wait()
    {
        HTTP_CLIENT_DEBUG << "waiting http result";
        boost::lock_guard<boost::mutex> cb_lock(m_cb_mutex);
        m_cb_cond.wait(m_cb_mutex);
        HTTP_CLIENT_DEBUG << "got http result";
        return m_response;
    }

private:
    boost::asio::io_service m_io_serv;
    boost::thread m_run_thread;
    CAsyncHttpClient m_async_client;
    boost::mutex m_cb_mutex;
    boost::condition m_cb_cond;
    ResponseInfo m_response;
};






