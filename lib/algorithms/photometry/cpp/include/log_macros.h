#ifndef LOG_MACROS_H
#define LOG_MACROS_H

#include <cstdio>

// LOG_DEBUG: 默认编译时不启用 (宏展开为空), 可通过定义 PC_ENABLE_DEBUG 启用
// 用于循环内高频日志, 避免日志 I/O 主导耗时
// 设计: 宏参数为 __VA_ARGS__ (至少含 fmt), 避免 -Wpedantic 空变参警告;
// 实现分两段 fprintf, 保证 fmt 始终在 __VA_ARGS__ 内, 单独输出换行
#ifdef PC_ENABLE_DEBUG
#define LOG_DEBUG(...) do { ::fprintf(stderr, "[DEBUG] "); ::fprintf(stderr, __VA_ARGS__); ::fprintf(stderr, "\n"); } while (0)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

// LOG_INFO / LOG_ERROR: 始终输出到 stderr
#define LOG_INFO(...) do { ::fprintf(stderr, "[INFO] "); ::fprintf(stderr, __VA_ARGS__); ::fprintf(stderr, "\n"); } while (0)
#define LOG_ERROR(...) do { ::fprintf(stderr, "[ERROR] "); ::fprintf(stderr, __VA_ARGS__); ::fprintf(stderr, "\n"); } while (0)

#endif // LOG_MACROS_H
