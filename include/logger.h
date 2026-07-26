#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_SUCCESS,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_OFF
} LogLevel;

void logger_init(LogLevel level, const char *log_file_path);
void logger_set_level(LogLevel level);
void logger_close(void);

void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_success(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* LOGGER_H */
