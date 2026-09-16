#pragma once

enum {
    SDET_LOG_INFO  = 0,
    SDET_LOG_DEBUG = 1,
    SDET_LOG_WARN  = 2,
    SDET_LOG_ERROR = 3
};

void sdet_log(int level, const char* module, const char* fmt, ...);
