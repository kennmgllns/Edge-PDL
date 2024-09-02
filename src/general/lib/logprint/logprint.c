
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "logprint.h"


/* timestamp'ed log printf */
void logprintf(esp_log_level_t level, const char *fmt, ...)
{
    static _lock_t  lock = 0;

    va_list args;
    struct timeval tv;
    struct tm timeinfo;

    (void)level; // unused

    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &timeinfo);

    _lock_acquire(&lock);

#if 0 // with milliseconds
    printf("[%04d-%02d-%02dT%02d:%02d:%02d.%03ld] ",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, tv.tv_usec / 1000);
#else
    printf("[%04d-%02d-%02dT%02d:%02d:%02d] ",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
#endif

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    _lock_release(&lock);
}

/* debug print memory contents */
void dumpbytes(const void *pv, size_t sz)
{
    const uint8_t *pb;
    pb = (const uint8_t *)pv;
    while (sz--) {
        printf("%02x ", *pb++);
    }
    printf("\r\n");
}
