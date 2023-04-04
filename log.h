#pragma once

typedef enum LogLevel {
    NONE=0,
    INFO,
    WARN,
    ERROR,
    FATAL,
} LogLevel;

#define MSGLOG(level, fmt, args...)     msgLog((level), __FUNCTION__, (fmt), ##args)

#ifdef __cplusplus
extern "C" {
#endif

void msgLog(LogLevel level, const char *function, const char *fmt, ...)  __attribute__ ((__format__ (__printf__, 3, 4)));

#ifdef __cplusplus
}
#endif
