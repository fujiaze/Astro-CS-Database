/* aio_publish.cpp - HiPS 原子发布原语 v1 唯一实现 (AIO-002)
 *
 * 合同: lib/hips/include/astrocs/hips/publish.h (v1 数值冻结)。
 * 流水线: staging → 校验(计数) → fsync 树 → 原子 promote(rename) — 对齐
 * IO_003_ATOMIC_OUTPUT_PUBLISH.md §4; 失败/取消 → stage_discard → 目标根
 * 无 partial (DISP-HIPS-001/004 模块事务面收口; 生产 writer 零改动)。
 *
 * 平台: Linux/macOS 全功能 (fsync/O_DIRECTORY/dirfd); Windows 编译保持
 * (_commit 落盘 / FindFirstFile 递归 / MoveFileEx 原子替换; 目录句柄 fsync
 * Windows 语义缺失 → 尽力模式, 目录元数据随 promote 的 rename 收敛, 登记
 * 于 lib/hips/README.md §9)。验证平台 = Linux (CI 同 SHA)。
 *
 * 科学纪律: scientific_change=false; 本 TU 无任何科学公式。异常屏障:
 * 实现 TU 内 malloc/new 失败走状态码 (无跨边界异常); 递归深度上限 64
 * (HiPS 树深 ≤ Norder15 + 2 目录段, 上限不可达, 防御病态输入)。
 */
#define _POSIX_C_SOURCE 200809L

#include "astrocs/hips/publish.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <io.h>
#define PUBLISH_MKDIR(p) _mkdir(p)
#define PUBLISH_FSYNC_FD(fd) _commit(fd)
#define PUBLISH_OPEN_RDONLY(p) _open(p, _O_RDONLY | _O_BINARY)
#define PUBLISH_CLOSE_FD(fd) _close(fd)
#define PUBLISH_UNLINK(p) _unlink(p)
#define PUBLISH_RMDIR(p) _rmdir(p)
#define PUBLISH_PATH_MAX 1024
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#define PUBLISH_MKDIR(p) mkdir((p), 0755)
#define PUBLISH_FSYNC_FD(fd) fsync(fd)
#define PUBLISH_OPEN_RDONLY(p) open((p), O_RDONLY)
#define PUBLISH_CLOSE_FD(fd) close(fd)
#define PUBLISH_UNLINK(p) unlink(p)
#define PUBLISH_RMDIR(p) rmdir(p)
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#define PUBLISH_DIR_FSYNC 1
#define PUBLISH_PATH_MAX 1024
#endif

/* ═══════════════════ 故障注入 (口径见 publish.h) ═══════════════════ */

static int publish_fault_active(const char* name) {
    const char* env = getenv("ASTROCS_HIPS_PUBLISH_FAULT");
    if (!env || !env[0]) return 0;
    if (strcmp(env, name) != 0) return 0;
    fprintf(stderr, "FAULT-INJECT: %s\n", name);
    return 1;
}

/* kill 中断测试锚点 (publish.h 声明; 唯一公开注入点) */
int hips_publish_fault_slow_write_v1(void) {
    return publish_fault_active("p1_stage_slow_write");
}

/* ═══════════════════ 路径 helper ═══════════════════ */

static int publish_str_ok(const char* s) { return s && s[0] != '\0'; }

/* 目录段与文件名段切分: out_dir 尾段为 basename (空/"."/".." → "hips") */
static void publish_split_dir(const char* path, char* parent, uint64_t parent_cap,
                              char* base, uint64_t base_cap) {
    size_t n = strlen(path);
    /* 去尾分隔符 (保留根 "/") */
    while (n > 1 && (path[n - 1] == '/' || path[n - 1] == '\\')) n--;
    const char* last = path + n;
    while (last > path && last[-1] != '/' && last[-1] != '\\') last--;
    size_t plen = (size_t)(last - path);
    size_t blen = n - plen;
    if (plen >= parent_cap) plen = parent_cap ? parent_cap - 1 : 0;
    if (blen >= base_cap) blen = base_cap ? base_cap - 1 : 0;
    memcpy(parent, path, plen);
    parent[plen] = '\0';
    if (plen == 0) { parent[0] = '.'; parent[1] = '\0'; }
    memcpy(base, last, blen);
    base[blen] = '\0';
    if (base[0] == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        snprintf(base, base_cap, "hips");
    }
}

/* 逐段 mkdir -p (EEXIST 忽略; 对齐 writer make_dirs 语义) */
static int publish_mkdir_parents(const char* path) {
    char tmp[PUBLISH_PATH_MAX];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(tmp)) return AIO_PUBLISH_ERR_PARAM;
    memcpy(tmp, path, n + 1);
    for (size_t i = 1; i < n; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            tmp[i] = '\0';
            if (tmp[0] != '\0') PUBLISH_MKDIR(tmp);   /* EEXIST 忽略 */
            tmp[i] = '/';
        }
    }
    PUBLISH_MKDIR(tmp);                               /* EEXIST 忽略 */
    return AIO_PUBLISH_OK;
}

static int publish_stat_exists(const char* p, int* is_dir) {
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    if (is_dir) *is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    return 1;
}

static uint64_t publish_dir_is_nonempty(const char* p, int* ok) {
    *ok = 1;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pat[PUBLISH_PATH_MAX];
    snprintf(pat, sizeof(pat), "%s\\*", p);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { *ok = 0; return 0; }
    uint64_t n = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
#else
    DIR* d = opendir(p);
    if (!d) { *ok = 0; return 0; }
    struct dirent* e;
    uint64_t n = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        n++;
    }
    closedir(d);
    return n;
#endif
}

/* ═══════════════════ 递归删除 (RAII 收口) ═══════════════════ */

static int publish_rmrf(const char* path, int depth) {
    if (depth > 64) return AIO_PUBLISH_ERR_IO;
    int is_dir = 0;
    if (!publish_stat_exists(path, &is_dir)) return AIO_PUBLISH_ERR_IO;
    if (!is_dir) {
        if (PUBLISH_UNLINK(path) != 0 && errno != ENOENT)
            return AIO_PUBLISH_ERR_IO;
        return AIO_PUBLISH_OK;
    }
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pat[PUBLISH_PATH_MAX];
    char child[PUBLISH_PATH_MAX];
    snprintf(pat, sizeof(pat), "%s\\*", path);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;
            snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
            int rc = publish_rmrf(child, depth + 1);
            if (rc != AIO_PUBLISH_OK) { FindClose(h); return rc; }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR* d = opendir(path);
    if (!d) return AIO_PUBLISH_ERR_IO;
    struct dirent* e;
    char child[PUBLISH_PATH_MAX];
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        int rc = publish_rmrf(child, depth + 1);
        if (rc != AIO_PUBLISH_OK) { closedir(d); return rc; }
    }
    closedir(d);
#endif
    if (PUBLISH_RMDIR(path) != 0 && errno != ENOENT)
        return AIO_PUBLISH_ERR_IO;
    return AIO_PUBLISH_OK;
}

/* ═══════════════════ 递归 fsync + 计数 (校验面) ═══════════════════ */

static int publish_fsync_file(const char* path) {
    int fd = PUBLISH_OPEN_RDONLY(path);
    if (fd < 0) return AIO_PUBLISH_ERR_IO;
    int rc = AIO_PUBLISH_OK;
    if (PUBLISH_FSYNC_FD(fd) != 0) {
        rc = (errno == ENOSPC || errno == EDQUOT) ? AIO_PUBLISH_ERR_DISKFULL
                                                  : AIO_PUBLISH_ERR_IO;
    }
    if (PUBLISH_CLOSE_FD(fd) != 0 && rc == AIO_PUBLISH_OK)
        rc = AIO_PUBLISH_ERR_IO;
    return rc;
}

static int publish_fsync_tree(const char* path, int depth,
                              uint64_t* n_files, uint64_t* total_bytes) {
    if (depth > 64) return AIO_PUBLISH_ERR_IO;
    int rc = AIO_PUBLISH_OK;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pat[PUBLISH_PATH_MAX];
    char child[PUBLISH_PATH_MAX];
    snprintf(pat, sizeof(pat), "%s\\*", path);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return AIO_PUBLISH_ERR_IO;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            int crc = publish_fsync_tree(child, depth + 1, n_files, total_bytes);
            if (crc != AIO_PUBLISH_OK && rc == AIO_PUBLISH_OK) rc = crc;
        } else {
            if (n_files) (*n_files)++;
            if (total_bytes)
                *total_bytes += ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            int frc = publish_fsync_file(child);
            if (frc != AIO_PUBLISH_OK && rc == AIO_PUBLISH_OK) rc = frc;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(path);
    if (!d) return AIO_PUBLISH_ERR_IO;
    struct dirent* e;
    char child[PUBLISH_PATH_MAX];
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(child, &st) != 0) {
            if (rc == AIO_PUBLISH_OK) rc = AIO_PUBLISH_ERR_IO;
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            int crc = publish_fsync_tree(child, depth + 1, n_files, total_bytes);
            if (crc != AIO_PUBLISH_OK && rc == AIO_PUBLISH_OK) rc = crc;
        } else if (S_ISREG(st.st_mode)) {
            if (n_files) (*n_files)++;
            if (total_bytes) *total_bytes += (uint64_t)st.st_size;
            int frc = publish_fsync_file(child);
            if (frc != AIO_PUBLISH_OK && rc == AIO_PUBLISH_OK) rc = frc;
        }
        /* 非常规项 (符号链接/fifo): 计数不计 fsync, 发布树不含 (writer 只产
         * 常规文件; 病态输入由 promote 前 STATE/IO 兜底) */
    }
    closedir(d);
    /* 目录自身 fsync (后序: 子项已刷) */
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        if (fsync(fd) != 0 && rc == AIO_PUBLISH_OK) {
            rc = (errno == ENOSPC || errno == EDQUOT) ? AIO_PUBLISH_ERR_DISKFULL
                                                      : AIO_PUBLISH_ERR_IO;
        }
        close(fd);
    } else if (rc == AIO_PUBLISH_OK) {
        rc = AIO_PUBLISH_ERR_IO;
    }
#endif
    return rc;
}

/* ═══════════════════ v1 原语实现 ═══════════════════ */

int aio_publish_stage_create_v1(const char* out_dir_utf8,
                                char* stage_path_buf, uint64_t stage_path_cap) {
    if (!publish_str_ok(out_dir_utf8) || !stage_path_buf || stage_path_cap == 0)
        return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_stage_create_fail"))
        return AIO_PUBLISH_ERR_IO;

    char parent[PUBLISH_PATH_MAX];
    char base[PUBLISH_PATH_MAX];
    publish_split_dir(out_dir_utf8, parent, sizeof(parent), base, sizeof(base));

    char stage[PUBLISH_PATH_MAX];
    int n = snprintf(stage, sizeof(stage), "%s/.%s%s", parent, base,
                     ASTROCS_HIPS_STAGE_BASENAME);
    if (n <= 0 || (size_t)n >= sizeof(stage) || (uint64_t)n >= stage_path_cap)
        return AIO_PUBLISH_ERR_PARAM;

    /* RAII 自愈: 同名残留 (kill/崩溃) 先确定性删除 */
    int exists = 0;
    if (publish_stat_exists(stage, &exists)) {
        int rc = publish_rmrf(stage, 0);
        if (rc != AIO_PUBLISH_OK) return rc;
    }

    int rc = publish_mkdir_parents(parent);
    if (rc != AIO_PUBLISH_OK) return rc;
    if (PUBLISH_MKDIR(stage) != 0) {
        if (errno == EEXIST) return AIO_PUBLISH_ERR_STATE;  /* 病态并发占用 */
        return AIO_PUBLISH_ERR_IO;
    }
    memcpy(stage_path_buf, stage, (size_t)n + 1);
    return AIO_PUBLISH_OK;
}

int aio_publish_stage_discard_v1(const char* out_dir_utf8) {
    if (!publish_str_ok(out_dir_utf8)) return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_discard_noop")) return AIO_PUBLISH_OK;
    char parent[PUBLISH_PATH_MAX];
    char base[PUBLISH_PATH_MAX];
    publish_split_dir(out_dir_utf8, parent, sizeof(parent), base, sizeof(base));
    char stage[PUBLISH_PATH_MAX];
    int n = snprintf(stage, sizeof(stage), "%s/.%s%s", parent, base,
                     ASTROCS_HIPS_STAGE_BASENAME);
    if (n <= 0 || (size_t)n >= sizeof(stage)) return AIO_PUBLISH_ERR_PARAM;
    int exists = 0;
    if (!publish_stat_exists(stage, &exists)) return AIO_PUBLISH_OK;  /* 幂等 */
    return publish_rmrf(stage, 0);
}

int aio_publish_tree_fsync_v1(const char* stage_path_utf8,
                              uint64_t* n_files_out, uint64_t* total_bytes_out) {
    if (!publish_str_ok(stage_path_utf8)) return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_fsync_fail")) return AIO_PUBLISH_ERR_IO;
    if (n_files_out) *n_files_out = 0;
    if (total_bytes_out) *total_bytes_out = 0;
    int is_dir = 0;
    if (!publish_stat_exists(stage_path_utf8, &is_dir) || !is_dir)
        return AIO_PUBLISH_ERR_IO;
    return publish_fsync_tree(stage_path_utf8, 0, n_files_out, total_bytes_out);
}

int aio_publish_promote_v1(const char* out_dir_utf8, const char* stage_path_utf8) {
    if (!publish_str_ok(out_dir_utf8) || !publish_str_ok(stage_path_utf8))
        return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_promote_fail")) return AIO_PUBLISH_ERR_STATE;

    int stage_is_dir = 0;
    if (!publish_stat_exists(stage_path_utf8, &stage_is_dir) || !stage_is_dir)
        return AIO_PUBLISH_ERR_STATE;

    int target_is_dir = 0;
    if (publish_stat_exists(out_dir_utf8, &target_is_dir)) {
        if (!target_is_dir) return AIO_PUBLISH_ERR_STATE;   /* 目标被文件占用 */
        int ok = 0;
        uint64_t ne = publish_dir_is_nonempty(out_dir_utf8, &ok);
        if (!ok) return AIO_PUBLISH_ERR_IO;
        if (ne != 0) return AIO_PUBLISH_ERR_STATE;          /* 唯一目标: 拒非空 */
    }

#ifdef _WIN32
    if (!MoveFileExA(stage_path_utf8, out_dir_utf8, MOVEFILE_REPLACE_EXISTING))
        return (GetLastError() == ERROR_NOT_SAME_DEVICE) ? AIO_PUBLISH_ERR_IO
                                                         : AIO_PUBLISH_ERR_STATE;
#else
    if (rename(stage_path_utf8, out_dir_utf8) != 0) {
        if (errno == ENOTEMPTY || errno == EEXIST || errno == ENOENT)
            return AIO_PUBLISH_ERR_STATE;
        if (errno == EXDEV) return AIO_PUBLISH_ERR_IO;
        return AIO_PUBLISH_ERR_IO;
    }
    /* 父目录 fsync (rename 元数据落盘; 尽力 — ENOSPC 等不回滚已成功的
     * rename, 记录不失败: 树原子性已达成) */
    char parent[PUBLISH_PATH_MAX];
    char base[PUBLISH_PATH_MAX];
    publish_split_dir(out_dir_utf8, parent, sizeof(parent), base, sizeof(base));
    int fd = open(parent, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        (void)fsync(fd);
        close(fd);
    }
#endif
    return AIO_PUBLISH_OK;
}
