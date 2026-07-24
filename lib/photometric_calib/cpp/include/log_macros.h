#ifndef LOG_MACROS_H
#define LOG_MACROS_H

#include <cstdio>

// LOG_DEBUG: 默认编译时不启用 (宏展开为空), 可通过定义 PC_ENABLE_DEBUG 启用
// 用于循环内高频日志, 避免日志 I/O 主导耗时
#ifdef PC_ENABLE_DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

// LOG_INFO / LOG_ERROR: 始终输出到 stderr
#define LOG_INFO(fmt, ...) fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#endif // LOG_MACROS_H
