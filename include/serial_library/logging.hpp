#pragma once

#include "serial_library/serial_library_base.hpp"

//
// log level setting
//

#define SERLIB_CURRENT_LOG_LEVEL SerlibLogLevel::SERLIB_DEBUG

//
// logging macros. These are overridable by defining the macro before including the file
//

#ifndef SERLIB_LOG_GENERIC
#define SERLIB_LOG_GENERIC(level, ...) serlibLoggingFunc(SerlibLogLevel::level, "[serlib] [" #level "] " __VA_ARGS__)
#endif

#ifndef SERLIB_LOG_DEBUG
#define SERLIB_LOG_DEBUG(...) SERLIB_LOG_GENERIC(SERLIB_DEBUG, __VA_ARGS__)
#endif

#ifndef SERLIB_LOG_INFO
#define SERLIB_LOG_INFO(...)  SERLIB_LOG_GENERIC(SERLIB_INFO, __VA_ARGS__)
#endif

#ifndef SERLIB_LOG_ERROR
#define SERLIB_LOG_ERROR(...) SERLIB_LOG_GENERIC(SERLIB_ERROR, __VA_ARGS__)
#endif

enum SerlibLogLevel
{
    SERLIB_DEBUG,
    SERLIB_INFO,
    SERLIB_ERROR
};


//
// actual code to handle logging
//

#if defined(USE_LINUX)

#include <cstdarg>
#include <stdio.h>

static void serlibLoggingFunc(SerlibLogLevel level, const char *fmt, ...)
{
    if(SERLIB_CURRENT_LOG_LEVEL <= level)
    {
        va_list v;
        va_start(v, fmt);
        vfprintf((level == SerlibLogLevel::SERLIB_ERROR ? stderr : stdout), fmt, v);
        printf("\n");
        va_end(v);
    }
}

#endif
