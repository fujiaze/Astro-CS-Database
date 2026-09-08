// tests/backend/p3_output_fsync_interposer.cpp — R10-C fsync 时序断言 interposer
// LD_PRELOAD 拦截 libc fopen/open/open64/fsync/rename/fwrite/fflush, 按发生
// 顺序向 stderr 输出事件行 (EVENT FOPEN <path> fd=N mode=... | EVENT FWRITE fd |
// EVENT FFLUSH fd | EVENT FSYNC fd | EVENT RENAME <path>), 供 test_p3_output.py
// 断言 flush(cfitsio 缓冲写出)→fsync(fd)→rename 顺序;
// ASTROCS_FAIL_FSYNC=1 时注入 fsync 失败 (返回 -1/EIO)。
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>

extern "C" {

static int (*real_fsync)(int) = nullptr;
static int (*real_open64)(const char*, int, ...) = nullptr;
static int (*real_rename)(const char*, const char*) = nullptr;
static int g_fail_fsync = 0;

static void emit(const char* ev, const std::string& arg) {
    // 直接系统调用写 stderr: 不得经 stdio (fprintf/fflush 会被本库拦截 → 递归)
    const std::string line = std::string("EVENT ") + ev + " " + arg + "\n";
    (void)::write(2, line.data(), line.size());
}

static void init(void) __attribute__((constructor));
static void init(void) {
    real_fsync = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
    real_open64 = (int (*)(const char*, int, ...))dlsym(RTLD_NEXT, "open64");
    real_rename = (int (*)(const char*, const char*))dlsym(RTLD_NEXT, "rename");
    g_fail_fsync = std::getenv("ASTROCS_FAIL_FSYNC") ? 1 : 0;
}

int open64(const char* path, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    const int fd = real_open64 ? real_open64(path, flags, mode) : -1;
    if (fd >= 0) {
        emit("OPEN", (path ? std::string(path) : std::string("-")) + " fd=" + std::to_string(fd));
    }
    return fd;
}

int open(const char* path, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    // 同一 libc: open 与 open64 在 x86_64 Linux 同义, 走同一实现
    const int fd = real_open64 ? real_open64(path, flags, mode) : -1;
    if (fd >= 0) {
        emit("OPEN", (path ? std::string(path) : std::string("-")) + " fd=" + std::to_string(fd));
    }
    return fd;
}

int fsync(int fd) {
    emit("FSYNC", std::to_string(fd));
    if (g_fail_fsync) {
        errno = EIO;
        return -1;
    }
    return real_fsync ? real_fsync(fd) : 0;
}

FILE* fopen(const char* path, const char* mode) {
    static FILE* (*real_fopen)(const char*, const char*) =
        (FILE* (*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
    FILE* fp = real_fopen(path, mode);
    if (fp) {
        emit("FOPEN", std::string(path ? path : "-") + " fd=" +
                          std::to_string(fileno(fp)) + " mode=" + (mode ? mode : "-"));
    }
    return fp;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* fp) {
    static size_t (*real_fwrite)(const void*, size_t, size_t, FILE*) =
        (size_t (*)(const void*, size_t, size_t, FILE*))dlsym(RTLD_NEXT, "fwrite");
    const size_t n = real_fwrite(ptr, size, nmemb, fp);
    if (fp) emit("FWRITE", std::to_string(fileno(fp)));
    return n;
}

int fflush(FILE* fp) {
    static int (*real_fflush)(FILE*) = (int (*)(FILE*))dlsym(RTLD_NEXT, "fflush");
    const int r = real_fflush(fp);
    if (fp) emit("FFLUSH", std::to_string(fileno(fp)));
    return r;
}

int rename(const char* oldp, const char* newp) {
    emit("RENAME", (oldp ? oldp : "-"));
    return real_rename ? real_rename(oldp, newp) : -1;
}

}  // extern "C"
