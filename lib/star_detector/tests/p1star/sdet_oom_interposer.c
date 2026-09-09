/* P1-STAR-TEST OOM interposer (tests/p1star 域内复制, 参照批次 R 样板
 * run/local/bughunt_batchR/oom_inject.c): 第 N 次 malloc/calloc/realloc/strdup
 * 返回 NULL, 支持 OOM_MIN/OOM_MAX 字节过滤与 OOM_TOTAL 末值上报。
 * 用法: OOM_FAIL_AT=N OOM_COUNTDOWN=1 OOM_MIN=a OOM_MAX=b OOM_TOTAL=path program */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long g_fail_at = -1;
static long g_countdown = 1;
static long g_seq = 0;
static long g_min = 0, g_max = 0;
static int g_active = 0;
static int g_armed = 0;  /* 显式 arm: 未 arm 时纯计数不失败 (进程启动期分配不参与) */
static void init_once(void);
/* 加载探针: negative 组 dlsym(RTLD_DEFAULT) 验证 interposer 真实预载
 * (LD_PRELOAD 环境串存在 ≠ 加载成功; glibc ld.so 对含空格路径按空格分词) */
int sdet_oom_interposer_magic = 0x53544152; /* "STAR" */

/* 显式 arm (导出): 从调用点重置计数并锚定注入点 — 检测段注入需要排除
 * 进程启动期 (libstdc++/stdio/locale) 分配, 否则小 N 全落在启动期 */
void sdet_oom_arm(long n) {
    init_once();
    g_fail_at = n;
    g_seq = 0;
    g_armed = 1;
}
static void *(*real_malloc)(size_t) = 0;
static void *(*real_calloc)(size_t, size_t) = 0;
static void *(*real_realloc)(void *, size_t) = 0;
static char *(*real_strdup)(const char *) = 0;

static void init_once(void) {
    if (g_active) return;
    g_active = 1;
    const char *at = getenv("OOM_FAIL_AT");
    const char *cd = getenv("OOM_COUNTDOWN");
    const char *mn = getenv("OOM_MIN");
    const char *mx = getenv("OOM_MAX");
    if (at) { g_fail_at = atol(at); g_countdown = cd ? atol(cd) : 1; }
    if (mn) g_min = atol(mn);
    if (mx) g_max = atol(mx);
    real_malloc  = dlsym(RTLD_NEXT, "malloc");
    real_calloc  = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    real_strdup  = dlsym(RTLD_NEXT, "strdup");
}

static int should_fail(size_t n) {
    init_once();
    if (!g_armed) { g_seq++; return 0; }
    if (g_max > 0 && ((long)n < g_min || (long)n > g_max)) return 0;
    g_seq++;
    {
        /* trace 用 open/write 直写 (fopen 内部 malloc 会无限递归) */
        const char *tr = getenv("OOM_TRACE");
        if (tr) {
            int fd = open(tr, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                char line[64];
                int len = snprintf(line, sizeof(line), "seq=%ld size=%zu\n", g_seq, n);
                if (len > 0) write(fd, line, (size_t)len);
                close(fd);
            }
        }
    }
    if (g_fail_at < 0) return 0;  /* 统计模式: 只计数不失败 */
    if (g_seq >= g_fail_at && g_seq < g_fail_at + g_countdown) return 1;
    return 0;
}

__attribute__((destructor)) static void oom_report(void) {
    const char *tp = getenv("OOM_TOTAL");
    if (tp) {
        FILE *f = fopen(tp, "w");
        if (f) { fprintf(f, "%ld\n", g_seq); fclose(f); }
    }
    const char *sp = getenv("OOM_SEQ");
    if (sp) {
        FILE *f = fopen(sp, "w");
        if (f) { fprintf(f, "seq=%ld\n", g_seq); fclose(f); }
    }
}

void *malloc(size_t n) { init_once(); if (should_fail(n)) return 0; return real_malloc(n); }
void *calloc(size_t a, size_t b) { init_once(); if (should_fail(a * b)) return 0; return real_calloc(a, b); }
void *realloc(void *p, size_t n) { init_once(); if (should_fail(n)) return 0; return real_realloc(p, n); }
char *strdup(const char *s) {
    init_once();
    if (should_fail(strlen(s) + 1)) return 0;
    return real_strdup(s);
}
