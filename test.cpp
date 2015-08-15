#define HAS_HTTP_CLIENT_LOG
#include <boost/thread.hpp>
#include "asynchttpclient.h"
#include "synchttpclient.h"


boost::asio::io_service g_io_service;


void r_cb(boost::shared_ptr<CAsyncHttpClient>& pClient, const ResponseInfo& r)
{
    if (r.timeout)
    {
        std::cout << "timeout" << std::endl;
    }
    else if (!r.error_msg.empty())
    {
        std::cout << r.error_msg.c_str() << std::endl;
    }
    else if (-1 == r.status_code)
    {
        std::cout << "unknown error" << std::endl;
    }
    else
    {
        std::cout << "response size=" << r.content.size()
            << ", content:\r\n" << r.content.c_str() << std::endl;
    }
}


void f()
{
    while (1)
    {
        RequestInfo req;
        req.set_url("http://www.baidu.com/123");
        req.set_method(METHOD_GET);
        boost::shared_ptr<CAsyncHttpClient> pClient = boost::make_shared<CAsyncHttpClient>(boost::ref(g_io_service), 5);
        //pClient->makeGet(boost::bind(r_cb, pClient, _1), "www.google.com", headers, "");
        pClient->make_request(boost::bind(r_cb, pClient, _1), req);
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
}


void test_sync()
{
    RequestInfo req;
    req.set_url("http://www.baidu.com/123");
    req.set_method(METHOD_GET);
    CSyncHttpClient client(5);
    const ResponseInfo& r = client.make_request(req);
}

void test_async()
{
    boost::thread t(f);
    {
        boost::asio::io_service::work work(g_io_service);
        g_io_service.run();
    }
}


int main()
{
    test_async();
    return 0;
}



