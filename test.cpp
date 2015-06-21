#include <boost/thread.hpp>
#include "asynchttpclient.h"


boost::asio::io_service io_service;


void r_cb(boost::shared_ptr<CAsyncHttpClient> pClient, const ResponseInfo& r, void *client_data, void *request_data)
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
        std::cout << "response:\r\n" << r.content.c_str() << std::endl;
    }
}


void *MY_NULL_PTR = NULL;


void f()
{
    std::map<std::string, std::string> headers;
    std::string body;
    while (1)
    {
        boost::shared_ptr<CAsyncHttpClient> pClient = boost::make_shared<CAsyncHttpClient>(boost::ref(io_service), 5, MY_NULL_PTR);
        //pClient->makeGet(boost::bind(r_cb, pClient, _1, _2, _3), "www.baidu.com", headers, "", NULL);
        pClient->makePost(boost::bind(r_cb, pClient, _1, _2, _3), "http://www.baidu.com/123", headers, "", body, NULL);
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
}


int main()
{
    boost::thread t(f);
    while (true)
    {
        io_service.run();
        io_service.reset();
    }
    return 0;
}



