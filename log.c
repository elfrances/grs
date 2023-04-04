#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "log.h"

static const char *logLevelTbl[] = {
        [NONE]  "NONE",
        [INFO]  "INFO",
        [WARN]  "WARN",
        [ERROR] "ERROR",
        [FATAL] "FATAL,"
};

void msgLog(LogLevel level, const char *function, const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm tm;
    char timestamp[80];
    va_list ap;

    strftime(timestamp, sizeof (timestamp), "%Y-%m-%dT%H:%M:%S", localtime_r(&now, &tm));
    fprintf(stdout, "%s:%s:%s: ", timestamp, logLevelTbl[level], function);
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    if (level == FATAL) {
        exit(-1);
    }
}



