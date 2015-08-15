#pragma once
#include <string>
#include <map>



//response info struct
//you should check timeout firstly in your cb
//then check error_msg, if it's empty, no error happened
//notice: because I use stringstream to build response string,
//so content maybe contain null character, your printing of content will be incomplete.
//if you will modify the code, be cautious of this point.
struct ResponseInfo
{
    //true if timeout
    bool timeout;
    //not empty when error happened
    std::string error_msg;

    //http version string
    std::string http_version;
    //status code. when -1, it means something wrong with response's stream, maybe can not parse headers
    int status_code;
    //status code message
    std::string status_msg;

    //headers in key-value style. all keys are lowered
    std::map<std::string, std::string> headers;

    //content
    std::string content;

    ResponseInfo()
        : timeout(false)
        , status_code(-1)
    {
    }
};


