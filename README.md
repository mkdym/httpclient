# httpclient

A C++ HTTP client library mainly based on boost.asio, boost.coroutine.

### Features

* headers only
* designed to make asynchronous http request
* provide synchronous http, asynchronous/synchronous download request, all based on asynchronous http request
* support HTTP proxy
* can be integrated with other log libraries

### Examples

see test.cpp

### Todo

* support SOCKS4/SOCKS5 proxy
* support https
* improve url parser
* ...


### References

* evhttpclient: https://github.com/jspotter/evhttpclient
* url_encode/url_decode: http://blog.csdn.net/gemo/article/details/8468311
* boost.asio example: http://www.boost.org/doc/libs/1_58_0/doc/html/boost_asio/example/cpp03/http/client/async_client.cpp

### Remarks

I only made a little test on Windows and Linux(CentOS6.3x86_64).  
Please tell me when error or suggestion, thanks.  
e-mail: abju123@sina.com

