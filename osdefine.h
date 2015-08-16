#pragma once


#if defined(_WIN32) || defined(WIN32)
#define _HTTP_WINDOWS
#endif


#if defined(_HTTP_WINDOWS)
#include <Windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif



namespace HTTP_OS_DEFINE
{
    unsigned long get_last_error()
    {
#if defined(_HTTP_WINDOWS)
        return GetLastError();
#else
        return errno;
#endif
    }

    unsigned long get_pid()
    {
#if defined(_HTTP_WINDOWS)
        return GetCurrentProcessId();
#else
        return getpid();
#endif
    }

    unsigned long get_tid()
    {
#if defined(_HTTP_WINDOWS)
        return GetCurrentThreadId();
#else
        return 0;//I don't known how to get it
#endif
    }

    void output_debug_string(const std::string& s)
    {
#if defined(_HTTP_WINDOWS)
        OutputDebugStringA(s.c_str());
        OutputDebugStringA("\r\n");
#else
        //
#endif
    }
}

