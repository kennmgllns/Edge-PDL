
#ifndef __LOGPRINT_H__
#define __LOGPRINT_H__

#include <assert.h>     // includes "__FILENAME__" macro
#include <stddef.h>     // size_t
#include <string.h>     // memset, etc.
#include <esp_log.h>    // log levels


#ifdef __cplusplus
extern "C" {
#endif


#define __FILELINE__        (__builtin_strchr(__FILENAME__, '.') - __FILENAME__), __FILENAME__, __LINE__

#define LOGD(fmt, ...)      logprintf(ESP_LOG_DEBUG, "%.*s[%03d] " fmt "\r\n", __FILELINE__, ## __VA_ARGS__)
#define LOGI(fmt, ...)      logprintf(ESP_LOG_INFO, "%.*s[%03d]\x1B[32m " fmt " \x1B[0m\r\n", __FILELINE__, ## __VA_ARGS__)
#define LOGW(fmt, ...)      logprintf(ESP_LOG_WARN, "%.*s[%03d]\x1B[33m " fmt " \x1B[0m\r\n", __FILELINE__, ## __VA_ARGS__)
#define LOGE(fmt, ...)      logprintf(ESP_LOG_ERROR, "%.*s[%03d]\x1B[31m " fmt " \x1B[0m\r\n", __FILELINE__, ## __VA_ARGS__)
#define LOGB                dumpbytes


/* timestamp'ed log printf */
void logprintf(esp_log_level_t level, const char *fmt, ...);
/* debug print memory contents */
void dumpbytes(const void *pv, size_t sz);


#ifdef __cplusplus
}
#endif

#endif // __LOGPRINT_H__
