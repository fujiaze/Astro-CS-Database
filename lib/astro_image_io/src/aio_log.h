#pragma once

enum {
    AIO_LOG_INFO  = 0,
    AIO_LOG_DEBUG = 1,
    AIO_LOG_WARN  = 2,
    AIO_LOG_ERROR = 3
};

void aio_log(int level, const char* module, const char* fmt, ...);
