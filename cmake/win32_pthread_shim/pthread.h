// MSVC pthread shim — WIN-001: 让 vendored cfitsio 在 Windows/MSVC 编译通过。
// cfitsio _REENTRANT 模式仅用 pthread_mutex_t + init/lock/unlock + attr。
// 映射到 SRWLOCK (轻量读写锁语义兼容 mutex); attr 为 no-op。
// 仅用于 cfitsio 编译 (include 路径最先注入), 不污染其他代码。
#pragma once

#include <windows.h>

typedef struct pthread_mutex_t {
  SRWLOCK lock;
  int     initialized;
} pthread_mutex_t;

typedef struct pthread_mutexattr_t {
  int type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER \
  { SRWLOCK_INIT, 1 }

static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) {
  (void)a;
  InitializeSRWLock(&m->lock);
  m->initialized = 1;
  return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t* m) {
  AcquireSRWLockExclusive(&m->lock);
  return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
  ReleaseSRWLockExclusive(&m->lock);
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
