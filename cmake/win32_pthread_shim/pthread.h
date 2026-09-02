// MSVC pthread shim — WIN-001: 让 vendored cfitsio 在 Windows/MSVC 编译通过。
// cfitsio _REENTRANT 模式仅用 pthread_mutex_t + init/lock/unlock + attr(递归)。
// 双模式: 若 <windows.h> 已由其他头引入 (SRWLOCK 已定义) 则复用;
// 否则手动声明 SRWLock API (kernel32 导出)。避免与 SDK 头冲突 (winnt.h C2059/C2371)。
// 仅用于 cfitsio 编译 (include 路径最先注入), 不污染其他代码。
#pragma once

#ifndef _WIN32
#error "win32_pthread_shim/pthread.h is MSVC/Windows only"
#endif

// 若 windows.h 链已定义 SRWLOCK (winnt.h), 不再重复定义类型/API。
#ifndef _WINNT_  // winnt.h guard

typedef struct _SRWLOCK {
  void* Ptr;
} SRWLOCK;

#define SRWLOCK_INIT { 0 }

#ifdef __cplusplus
extern "C" {
#endif

void __stdcall InitializeSRWLock(SRWLOCK* lock);
void __stdcall AcquireSRWLockExclusive(SRWLOCK* lock);
void __stdcall ReleaseSRWLockExclusive(SRWLOCK* lock);

#ifdef __cplusplus
}
#endif

#endif  // !_WINNT_

typedef SRWLOCK pthread_mutex_t;

typedef struct pthread_mutexattr_t {
  int type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_MUTEX_RECURSIVE  1   // cfitsio cfileio.c 用 attr settype(RECURSIVE)

static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) {
  (void)a;
  InitializeSRWLock(m);
  return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t* m) {
  AcquireSRWLockExclusive(m);
  return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
  ReleaseSRWLockExclusive(m);
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
