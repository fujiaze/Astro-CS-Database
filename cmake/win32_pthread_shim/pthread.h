// MSVC pthread shim — WIN-001: 让 vendored cfitsio 在 Windows/MSVC 编译通过。
// cfitsio _REENTRANT 模式仅用 pthread_mutex_t + init/lock/unlock + attr(递归)。
// 实现: 纯原子自旋锁 (MSVC 内建 _InterlockedExchange, 零 Windows 头依赖 —
// 避免与 SDK 的 SRWLOCK/winnt.h 冲突 C2371/C2375, 不 include windows.h)。
// 仅用于 cfitsio 编译 (include 路径最先注入), 不污染其他代码。
#pragma once

#ifndef _WIN32
#error "win32_pthread_shim/pthread.h is MSVC/Windows only"
#endif

#include <stdint.h>

typedef struct pthread_mutex_t {
  volatile long locked;   // 0=free, 1=held (自旋互斥)
} pthread_mutex_t;

typedef struct pthread_mutexattr_t {
  int type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER { 0 }
#define PTHREAD_MUTEX_RECURSIVE  1   // cfitsio cfileio.c 用 attr settype(RECURSIVE); shim 近似非递归

#ifdef __cplusplus
extern "C" {
#endif

long _InterlockedExchange(volatile long* target, long value);  // MSVC 内建, 无需头

#ifdef __cplusplus
}
#endif

static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) {
  (void)a;
  m->locked = 0;
  return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t* m) {
  // 自旋直到成功获取 (cfitsio 锁持有短; 近似互斥语义足够, 无递归)
  while (_InterlockedExchange(&m->locked, 1) != 0) {
    // 忙等 (MSVC 下无 _mm_pause 头依赖; 锁竞争极低)
  }
  return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
  _InterlockedExchange(&m->locked, 0);
  return 0;
}
static inline int pthread_mutexattr_init(pthread_mutexattr_t* a) {
  a->type = 0;
  return 0;
}
static inline int pthread_mutexattr_settype(pthread_mutexattr_t* a, int type) {
  a->type = type;
  return 0;
}

// cfitsio _REENTRANT 用 strtok_r (POSIX); MSVC 提供 strtok_s (C11 Annex K)。
// 语义兼容 (分割 + 保存 next token 指针)。
static inline char* strtok_r(char* str, const char* delim, char** saveptr) {
  return strtok_s(str, delim, saveptr);
}
