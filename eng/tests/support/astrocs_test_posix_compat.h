/* ============================================================================
 * astrocs_test_posix_compat.h — 测试面「平台专属调用」唯一判定点
 *
 * 背景 (FINAL-07 WIN-PORT 批次二):
 *   正式平台为 Windows x64 与 Linux amd64。测试面存在若干**只在类 UNIX 存在**的
 *   接口（环境变量设置 / 管道打开 / 动态加载与符号解析 / 目录遍历与文件属性），
 *   此前在 4 个族里各写各的：有的文件做了 #ifdef _WIN32 分支（如 adapter 用例的
 *   dlopen 头面），同族另一处却直接裸调用（如 integration 用例的 dlopen 调用面）；
 *   p1hips / p1star 两处又各自私建局部 shim。三套口径并存 ⇒ 下一个人必然在第四处漏。
 *
 *   本头把上述判定**收敛到一处**：Windows 侧给出 CRT / Win32 的**语义等价物**，
 *   类 UNIX 侧只做与改动前完全相同的系统头包含 ⇒ **Linux 行为零变化**（零 delta）。
 *   所有测试 TU 只写一份调用码，平台差异不再出现在调用点。
 *
 * 用法（必须遵守）:
 *   1. 本头**自足**：它自己包含所需的全部 CRT / Win32 / POSIX 头，调用点不必再写
 *      <unistd.h> / <dirent.h> / <dlfcn.h> / <sys/wait.h> 之类；
 *   2. 放在**所有其它头之后**（宏定义要在系统头解析完之后生效）；
 *   3. **只**使用本头提供的名字；未提供的名字（见文末「无等价物清单」）说明该能力
 *      在目标平台确实不可表达 ⇒ 调用点必须按
 *      「显式平台限定 + 明确状态」处理（ASTROCS_TEST_HAS_* 门 + 打印 NOT-EXECUTED），
 *      **不得**用「整段跳过」掩盖，**不得**伪造等价物造成恒真。
 *
 * 权威依据:
 *   docs/ASTROCS_DESIGN.md §1（正式平台）、ENGINEERING_SPEC.md（平台面与测试纪律）、
 *   既有同类先例 lib/infrastructure/aio/tests/CMakeLists.txt 的 FINAL-07 WIN-PORT
 *   「if(UNIX) + else() message(...) 显式跳过、不静默失效」口径。
 * ==========================================================================*/
#ifndef ASTROCS_TEST_POSIX_COMPAT_H
#define ASTROCS_TEST_POSIX_COMPAT_H

/* ── 能力标记：1 = 本平台可用；0 = 无等价语义，调用点必须显式限定 ─────────── */
#ifdef _WIN32
#define ASTROCS_TEST_HAS_FORK 0          /* 无 fork：跨进程度传递进程内状态不可表达 */
#define ASTROCS_TEST_HAS_SIGNAL_KILL 0   /* 无 SIGKILL + waitpid(WIFSIGNALED) */
#define ASTROCS_TEST_HAS_RTLD_NEXT 0     /* 无 LD_PRELOAD 链式查找语义 */
#define ASTROCS_TEST_HAS_GETLOADAVG 0    /* 无 load average（GetSystemTimes 是利用率，不同量纲） */
#else
#define ASTROCS_TEST_HAS_FORK 1
#define ASTROCS_TEST_HAS_SIGNAL_KILL 1
#define ASTROCS_TEST_HAS_RTLD_NEXT 1
#define ASTROCS_TEST_HAS_GETLOADAVG 1
#endif

#ifdef _WIN32
/* ==========================================================================
 * Windows 侧：CRT / Win32 等价物
 * ==========================================================================*/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>      /* _mkdir / _rmdir          */
#include <io.h>          /* _unlink / _findfirst64   */
#include <process.h>     /* _getpid / _putenv_s      */
#include <stdio.h>       /* _popen / _pclose         */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>    /* _stat / struct stat      */
#include <sys/types.h>
#include <time.h>

/* ── 1. 环境变量（setenv / unsetenv / putenv → CRT）───────────────────────
 * 语义对齐: POSIX setenv(name, value, overwrite) 成功返回 0、失败非 0；
 *           _putenv_s 成功返回 0、失败返回 errno ⇒ 「!= 0 即失败」的判定不变。
 * overwrite 参数在 Windows 侧恒为「覆盖」（测试调用点一律用 1）⇒ 语义一致。 */
static __inline int astrocs_test_setenv(const char* name, const char* value) {
    return _putenv_s(name, (value != NULL) ? value : "");
}
static __inline int astrocs_test_unsetenv(const char* name) {
    return _putenv_s(name, "");          /* 置空 = 从环境中移除 */
}
#define setenv(name, value, overwrite) astrocs_test_setenv((name), (value))
#define unsetenv(name) astrocs_test_unsetenv(name)
#define putenv(string) _putenv(string)

/* ── 2. 管道打开（popen / pclose → CRT）────────────────────────────────── */
#define popen(command, mode) _popen((command), (mode))
#define pclose(stream) _pclose(stream)
/* sleep(秒) → Win32 Sleep(毫秒)。与 POSIX 同为「至少睡够请求时长」语义：
 * 毫秒向上取整不会缩短等待，依赖该等待做时序错开的用例不弱化。 */
#define sleep(seconds) Sleep((DWORD)((seconds) * 1000))

/* ── 3. 动态加载与符号解析（dlopen 族 → LoadLibrary / GetProcAddress）────
 * RTLD_* 在 Windows 无对应开关（加载即立即解析、无全局/局部可见性分级），
 * 一律取 0：调用点的 RTLD_NOW|RTLD_LOCAL 仍编译通过，且语义上「立即解析 +
 * 本句柄可见」与默认行为一致。
 * RTLD_DEFAULT ⇒ 主模块句柄（GetModuleHandleA(NULL)），与 glibc 的
 * 「全局作用域查找」在测试桩场景下等价。 */
#define RTLD_LAZY 0
#define RTLD_NOW 0
#define RTLD_LOCAL 0
#define RTLD_GLOBAL 0
#define RTLD_DEFAULT ((void*)0)
/* RTLD_NEXT 故意不定义：LD_PRELOAD 链式查找在 Windows 无等价物（见文末清单）；
 * 需要它的 TU 属 interposer 面，应由构建层显式限定到 UNIX。 */

static __inline const char* astrocs_test_dlerror(void) {
    static char buf[256];
    const DWORD e = GetLastError();
    if (e == 0) return "no error";
    (void)snprintf(buf, sizeof(buf),
                   "LoadLibrary/GetProcAddress failed (GetLastError=%lu)",
                   (unsigned long)e);
    return buf;
}
static __inline void* astrocs_test_dlopen(const char* path) {
    SetLastError(0);
    return (void*)LoadLibraryA(path);
}
static __inline void* astrocs_test_dlsym(void* handle, const char* name) {
    HMODULE mod = (handle != NULL && handle != (void*)0)
                      ? (HMODULE)handle
                      : GetModuleHandleA(NULL);
    return (void*)GetProcAddress(mod, name);
}
static __inline int astrocs_test_dlclose(void* handle) {
    return (handle != NULL) ? (FreeLibrary((HMODULE)handle) ? 0 : -1) : 0;
}
#define dlopen(path, flags) astrocs_test_dlopen(path)
#define dlsym(handle, name) astrocs_test_dlsym((handle), (name))
#define dlclose(handle) astrocs_test_dlclose(handle)
#define dlerror() astrocs_test_dlerror()

/* ── 3b. 「真实分配器」解析（_WIN32）─────────────────────────────────────
 * 用途: 同址 include 生产源码的用例会用分配宏（#define malloc ...）把本 TU 的分配
 *   重定向到测试桩；用例自身仍需拿到**真实**分配器来收发内存。
 *   POSIX 侧靠 dlsym(RTLD_NEXT, "malloc")（LD_PRELOAD 链式查找下一处定义）。
 *   Windows 无 LD_PRELOAD 链，但 CRT 提供同语义的 base 分配器 _malloc_base 族 ——
 *   它们**不**被分配宏重定向（宏只改 malloc/calloc/... 这些名字），且本来就是供
 *   「覆写 malloc 的库」调用的底层实现 ⇒ 语义等价，非伪等价。 */
#include <malloc.h>
static __inline void* astrocs_test_real_allocator(const char* name) {
    if (name == NULL) return NULL;
    if (strcmp(name, "malloc")  == 0) return (void*)(void* (*)(size_t))_malloc_base;
    if (strcmp(name, "calloc")  == 0) return (void*)(void* (*)(size_t, size_t))_calloc_base;
    if (strcmp(name, "realloc") == 0) return (void*)(void* (*)(void*, size_t))_realloc_base;
    if (strcmp(name, "free")    == 0) return (void*)(void (*)(void*))_free_base;
    return NULL;
}

/* ── 4. 目录遍历与文件属性（dirent 族 / stat 族 → CRT _findfirst64）─────
 * dirent 语义映射:
 *   opendir  → _findfirst64("<path>" + 通配星号)；**空目录必须成功**（POSIX
 *              opendir 对空目录返回有效句柄）⇒ 句柄失败但属性为目录时返回空目录句柄；
 *   readdir  → 首次返回首个匹配项，其后 _findnext64；"." / ".." 由 CRT 通配
 *              **不返回**（POSIX 返回）⇒ 调用点的显式过滤仍然安全（冗余但不错）；
 *   closedir → _findclose + 释放。
 * 文件属性: lstat → _stat（Windows 无符号链接/普通文件的 lstat 区分，测试只用于
 *           「是否目录」判定，语义足够）；unlink → _unlink；rmdir → _rmdir；
 *           mkdir(path, mode) → _mkdir(path)（mode 在 Windows 无效，POSIX 侧本就
 *           受 umask 影响，调用点用 0777 只表达「可写」意图）。 */
struct astrocs_test_dirent {
    char d_name[260];                     /* MAX_PATH：与 Windows 文件名上限一致 */
};
typedef struct astrocs_test_dir {
    intptr_t        handle;               /* _findfirst64 句柄；-1 = 空目录/已结束 */
    int             first;                /* 尚未返回首个匹配项 */
    struct _finddata64i32_t fd;
    struct astrocs_test_dirent ent;
} astrocs_test_dir_t;

static __inline astrocs_test_dir_t* astrocs_test_opendir(const char* path) {
    astrocs_test_dir_t* d;
    char pattern[1024];
    const DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
        return NULL;                      /* 不存在或不是目录 ⇒ 与 POSIX 一致返回 NULL */
    d = (astrocs_test_dir_t*)malloc(sizeof(astrocs_test_dir_t));
    if (d == NULL) return NULL;
    (void)snprintf(pattern, sizeof(pattern), "%s/*", path);
    d->handle = _findfirst64(pattern, &d->fd);
    d->first = 1;
    return d;                             /* 空目录：handle == -1，readdir 立即返回 NULL */
}
static __inline struct astrocs_test_dirent* astrocs_test_readdir(astrocs_test_dir_t* d) {
    if (d == NULL || d->handle == -1) return NULL;
    if (d->first) {
        d->first = 0;
    } else if (_findnext64(d->handle, &d->fd) != 0) {
        return NULL;
    }
    (void)snprintf(d->ent.d_name, sizeof(d->ent.d_name), "%s", d->fd.name);
    return &d->ent;
}
static __inline int astrocs_test_closedir(astrocs_test_dir_t* d) {
    if (d == NULL) return -1;
    if (d->handle != -1) (void)_findclose(d->handle);
    free(d);
    return 0;
}
#define DIR astrocs_test_dir_t
#define dirent astrocs_test_dirent
#define opendir(path) astrocs_test_opendir(path)
#define readdir(d) astrocs_test_readdir(d)
#define closedir(d) astrocs_test_closedir(d)

#define lstat(path, buf) _stat((path), (buf))
#define stat(path, buf) _stat((path), (buf))
#define unlink(path) _unlink(path)
#define rmdir(path) _rmdir(path)
#define mkdir(path, mode) _mkdir(path)
#define strdup(text) _strdup(text)
#define getpid() _getpid()
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif

/* nanosleep: Win32 只有毫秒级 Sleep；向上取整保证「至少睡够请求时长」，
 * 不缩短等待 ⇒ 依赖该等待做时序错开的调用点语义不弱化。 */
static __inline int astrocs_test_nanosleep(const struct timespec* req,
                                           struct timespec* rem) {
    DWORD ms;
    if (rem != NULL) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    if (req == NULL) return -1;
    ms = (DWORD)((long long)req->tv_sec * 1000LL +
                 ((long long)req->tv_nsec + 999999LL) / 1000000LL);
    Sleep(ms);
    return 0;
}
#define nanosleep(req, rem) astrocs_test_nanosleep((req), (rem))

#else  /* !_WIN32 */
/* ==========================================================================
 * 类 UNIX 侧: **整头为空** —— 不引入任何包含、宏、类型或函数, 只有上面的
 * ASTROCS_TEST_HAS_* 能力标记 (=1)。类 UNIX 侧本来就有这些接口, 各 TU 保留自己
 * 原有的系统头包含集合与顺序 => 包含本头前后**预处理输出逐字节一致**
 * (证据: run/FINAL-07/objdelta.py 逐 TU 零 delta 比对)。
 * 这是「平台判断只在一处」的最小形态: 非目标平台零风险。
 * ==========================================================================*/
#endif /* _WIN32 */

/* ============================================================================
 * 「无等价物清单」——本头**故意不提供**，调用点必须显式限定 + 明确状态
 * ----------------------------------------------------------------------------
 * fork()                        : Windows 无 fork；进程内 C++ 状态无法跨进程度传递。
 *                                 ⇒ ASTROCS_TEST_HAS_FORK == 0 时该 case 打印
 *                                   NOT-EXECUTED 明确状态，不计 PASS 也不计 FAIL。
 * kill(pid, SIGKILL)/waitpid    : 同上（TerminateProcess 只能作用于自建进程）。
 * RTLD_NEXT                     : LD_PRELOAD 链式查找无 Windows 等价物；interposer
 *                                 类共享库应在构建层显式限定到 UNIX。
 * getloadavg()                  : Windows 无 load average；GetSystemTimes 给的是
 *                                 CPU 利用率，量纲不同 ⇒ 拒绝伪等价。
 * ==========================================================================*/
#endif /* ASTROCS_TEST_POSIX_COMPAT_H */
