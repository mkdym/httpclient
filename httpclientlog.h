#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp>

#if defined(HAS_HTTP_CLIENT_LOG)
#include "osdefine.h"
#endif



enum HTTP_LOG_LEVEL
{
    HTTP_LOG_LEVEL_DEBUG,
    HTTP_LOG_LEVEL_INFO,
    HTTP_LOG_LEVEL_WARN,
    HTTP_LOG_LEVEL_ERROR,
};



//log for debug
//if need, define macro HAS_HTTP_CLIENT_LOG
//you can modify the macros to satisfy your log library
//generally, do not use info level, use debug level
//as a release library, it should output log as little as possible
//and as we know, debug log is always discarded when release
#define HTTP_CLIENT_DEBUG       HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_DEBUG)
#define HTTP_CLIENT_INFO        HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_INFO)
#define HTTP_CLIENT_WARN        HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_WARN)
#define HTTP_CLIENT_ERROR       HTTP_CLIENT_LOG(HTTP_LOG_LEVEL_ERROR)


#define HTTP_CLIENT_LOG(_level) CAsyncHttpClientLog<_level>(__FILE__, __LINE__).stream()


template<HTTP_LOG_LEVEL _level>
class CAsyncHttpClientLog
{
public:
    CAsyncHttpClientLog(const char* file, const int line)
        : m_file_name(file)
        , m_line(line)
    {
#if defined(HAS_HTTP_CLIENT_LOG)
        size_t pos = m_file_name.find_last_of("/\\");
        if (std::string::npos != pos)
        {
            m_file_name = m_file_name.substr(pos);
            boost::algorithm::trim_if(m_file_name, boost::algorithm::is_any_of("/\\"));
        }
#endif
    }

    ~CAsyncHttpClientLog()
    {
#if defined(HAS_HTTP_CLIENT_LOG)
        std::string level;
        switch (_level)
        {
        case HTTP_LOG_LEVEL_DEBUG:
            level = "DEBUG";
            break;

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
        tm timeinfo = {0};
        if (0 == HTTP_OS_DEFINE::localtime_t(time(NULL), timeinfo))
        {
            const int time_buffer_size = 100;
            char time_buffer[time_buffer_size] = {0};
            strftime(time_buffer, time_buffer_size, "%Y-%m-%d-%H:%M:%S", &timeinfo);
            log_time = time_buffer;
        }
        else
        {
            log_time = "unknown time???";
        }

        std::ostringstream oss_msg;
        oss_msg << log_time << " " << level.c_str()
            << " [" << HTTP_OS_DEFINE::get_pid() << ":" << HTTP_OS_DEFINE::get_tid() << "]"
            << " [" << m_file_name.c_str() << ":" << m_line << "]"
            << " " << m_oss.str().c_str();

        HTTP_OS_DEFINE::output_debug_string(oss_msg.str());
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





