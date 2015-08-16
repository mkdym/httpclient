#pragma once
#include <iostream>
#include <sstream>
#include <string>


#if defined(_WIN32) || defined(WIN32)
#define _HTTP_WINDOWS
#endif



#if defined(HAS_HTTP_CLIENT_LOG) && defined(_HTTP_WINDOWS)
#include <Windows.h>
#endif

#include <boost/algorithm/string.hpp>



enum HTTP_LOG_LEVEL
{
    HTTP_LOG_LEVEL_INFO,
    HTTP_LOG_LEVEL_WARN,
    HTTP_LOG_LEVEL_ERROR,
};



//log for debug
//if need, define macro HAS_HTTP_CLIENT_LOG
#define HTTP_CLIENT_INFO     HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_INFO)
#define HTTP_CLIENT_WARN     HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_WARN)
#define HTTP_CLIENT_ERROR    HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_ERROR)


#define HTTP_CLIENT_LOG(_level) CAsyncHttpClientLog<_level>(__FILE__, __LINE__).stream()


template<HTTP_LOG_LEVEL _level>
class CAsyncHttpClientLog
{
public:
    CAsyncHttpClientLog(const char* file, const int line)
        : m_file_name(file)
        , m_line(line)
    {
        size_t pos = m_file_name.find_last_of("/\\");
        if (std::string::npos != pos)
        {
            m_file_name = m_file_name.substr(pos);
            boost::algorithm::trim_if(m_file_name, boost::algorithm::is_any_of("/\\"));
        }
    }

    ~CAsyncHttpClientLog()
    {
#if defined(HAS_HTTP_CLIENT_LOG)
        std::string level;
        switch (_level)
        {
        case HTTP_LOG_LEVEL_INFO:
            level = "INFO ";
            break;

        case HTTP_LOG_LEVEL_WARN:
            level = "WARN ";
            break;

        case HTTP_LOG_LEVEL_ERROR:
            level = "ERROR";
            break;

        default:
            level = "WHAT?";
            break;
        }

        std::string log_time;
        time_t rawtime = time(NULL);
        tm *timeinfo = localtime(&rawtime);
        if (timeinfo)
        {
            const int time_buffer_size = 100;
            char time_buffer[time_buffer_size] = {0};
            strftime(time_buffer, time_buffer_size, "%Y-%m-%d-%H:%M:%S", timeinfo);
            log_time = time_buffer;
        }
        else
        {
            log_time = "unknown time???";
        }

        std::ostringstream oss_msg;
        oss_msg << log_time << " " << level.c_str()
#if defined(_HTTP_WINDOWS)
            << " [" << GetCurrentProcessId() << ":" << GetCurrentThreadId() << "]"
#else
            << " [" << getpid() << ":" << pthread_self() << "]"
#endif
            << " [" << m_file_name.c_str() << ":" << m_line << "]"
            << " " << m_oss.str().c_str();

#if defined(_HTTP_WINDOWS)
        OutputDebugStringA(oss_msg.str().c_str());
        OutputDebugStringA("\r\n");
#endif
        std::cout << oss_msg.str().c_str() << std::endl;

#endif
    }

    std::ostringstream& stream()
    {
        return m_oss;
    }

private:
    std::ostringstream m_oss;
    std::string m_file_name;
    int m_line;
};





