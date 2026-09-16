#pragma once

enum {
    LOG_INFO  = 0,
    LOG_DEBUG = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
};

void dpsf_log(int level, const char* module, const char* fmt, ...);
