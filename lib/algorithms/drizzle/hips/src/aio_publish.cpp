/* aio_publish.cpp - HiPS 原子发布原语 v1 唯一实现 (AIO-002)
 *
 * 合同: lib/algorithms/drizzle/hips/include/astrocs/hips/publish.h (v1 数值冻结)。
 * 流水线: staging → 校验(计数) → fsync 树 → 原子 promote(rename) — 对齐
 * IO_003_ATOMIC_OUTPUT_PUBLISH.md §4; 失败/取消 → stage_discard → 目标根
 * 无 partial (DISP-HIPS-001/004 模块事务面收口; 生产 writer 零改动)。
 *
 * I/O 归属 (ASTROCS_DESIGN §9 + GAP_AUDIT §9.73 裁决 U5):
 *   「aio 是文件级唯一 I/O 边界」+ 机器判据「全仓文件打开 / 流式读写 /
 *   文件系统写操作, 除 aio 内部外应为 0」。
 *   ⇒ 本 TU **不再**自行调用任何文件系统原语 (mkdir/stat/opendir/readdir/
 *   closedir/unlink/rmdir/open/fsync/close/rename/MoveFileEx/FindFirstFile);
 *   全部机制经 lib/infrastructure/aio/src/aio_atomic_file.h
 *   (namespace aio_atomic) —— 本 TU 只保留**策略**(publish v1 状态码映射、
 *   路径词法、故障注入)。原实现内联的 POSIX/Win32 调用已整体搬入 aio。
 *
 * 平台: Linux/macOS 全功能 (fsync/O_DIRECTORY/dirfd); Windows 编译保持
 * (_commit 落盘 / 目录遍历 / MoveFileEx 原子替换; 目录句柄 fsync
 * Windows 语义缺失 → 尽力模式, 目录元数据随 promote 的 rename 收敛, 登记
 * 于 lib/algorithms/drizzle/hips/README.md §9)。验证平台 = Linux (CI 同 SHA)。
 *
 * 科学纪律: scientific_change=false; 本 TU 无任何科学公式。异常屏障:
 * 实现 TU 内 malloc/new 失败走状态码 (无跨边界异常); 递归深度上限 64
 * (HiPS 树深 ≤ Norder15 + 2 目录段, 上限不可达, 防御病态输入)。
 */
#include "astrocs/hips/publish.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "aio_atomic_file.h"

/* staging 路径缓冲上限 (publish.h v1 冻结推荐值 1024) */
#define PUBLISH_PATH_MAX 1024

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

/* ═══════════════════ 路径 helper (纯词法, 无 I/O) ═══════════════════ */

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

/* 推导 staging 路径: <parent(out_dir)>/.<base>.hips_staging.tmp。
 * 返回 0 = 成功; 非 0 = publish v1 状态码 (路径溢出 = PARAM)。 */
static int publish_stage_path(const char* out_dir_utf8, char* out, size_t cap,
                              size_t* out_len) {
    char parent[PUBLISH_PATH_MAX];
    char base[PUBLISH_PATH_MAX];
    publish_split_dir(out_dir_utf8, parent, sizeof(parent), base, sizeof(base));
    const int n = snprintf(out, cap, "%s/.%s%s", parent, base,
                           ASTROCS_HIPS_STAGE_BASENAME);
    if (n <= 0 || (size_t)n >= cap) return AIO_PUBLISH_ERR_PARAM;
    if (out_len) *out_len = (size_t)n;
    return AIO_PUBLISH_OK;
}

/* ═══════════════════ 发布树机制 (全部经 aio_atomic) ═══════════════════ */

/* 递归 fsync + 计数 (后序: 先文件后子目录再自身)。返回 publish v1 状态码。 */
static int publish_fsync_tree(const std::string& path, int depth,
                              uint64_t* n_files, uint64_t* total_bytes) {
    if (depth > 64) return AIO_PUBLISH_ERR_IO;
    int rc = AIO_PUBLISH_OK;
    int abort_rc = 0;
    const int walk = aio_atomic::for_each_child(
        path,
        [&](const std::string& child, int kind) -> int {
            if (kind == 1) {                       /* 目录: 后序递归 */
                const int crc = publish_fsync_tree(child, depth + 1, n_files,
                                                   total_bytes);
                if (crc != AIO_PUBLISH_OK && rc == AIO_PUBLISH_OK) rc = crc;
            } else if (kind == 0) {                /* 常规文件: 计数 + fsync */
                if (n_files) (*n_files)++;
                /* 大小统计: 由 aio 机制原语回填 (不自行 stat) */
                uint64_t sz = 0;
                int is_d = 0;
                if (aio_atomic::path_size(child, &sz, &is_d) && !is_d) {
                    if (total_bytes) *total_bytes += sz;
                }
                const int frc = aio_atomic::fsync_path(child, 0);
                if (frc != 0 && rc == AIO_PUBLISH_OK) {
                    rc = (frc == ENOSPC || frc == EDQUOT) ? AIO_PUBLISH_ERR_DISKFULL
                                                          : AIO_PUBLISH_ERR_IO;
                }
            }
            /* kind == 2 (符号链接/fifo/设备): 计数不计 fsync, 发布树不含
             * (writer 只产常规文件; 与原 publish v1 语义逐位一致 —— 不 open
             * 非常规项, 避免 fifo 上 open 阻塞) */
            return 0;   /* 逐项继续; 错误经 rc 汇总 */
        },
        &abort_rc);
    if (walk != 0) return AIO_PUBLISH_ERR_IO;
    if (abort_rc != 0 && rc == AIO_PUBLISH_OK) rc = abort_rc;
    /* 目录自身 fsync (后序: 子项已刷) */
    const int drc = aio_atomic::fsync_path(path, 1);
    if (drc != 0 && rc == AIO_PUBLISH_OK) {
        rc = (drc == ENOSPC || drc == EDQUOT) ? AIO_PUBLISH_ERR_DISKFULL
                                              : AIO_PUBLISH_ERR_IO;
    }
    return rc;
}

/* ═══════════════════ v1 原语实现 ═══════════════════ */

int aio_publish_stage_create_v1(const char* out_dir_utf8,
                                char* stage_path_buf, uint64_t stage_path_cap) {
    if (!publish_str_ok(out_dir_utf8) || !stage_path_buf || stage_path_cap == 0)
        return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_stage_create_fail"))
        return AIO_PUBLISH_ERR_IO;

    char stage[PUBLISH_PATH_MAX];
    size_t n = 0;
    int rc = publish_stage_path(out_dir_utf8, stage, sizeof(stage), &n);
    if (rc != AIO_PUBLISH_OK) return rc;
    if ((uint64_t)n >= stage_path_cap) return AIO_PUBLISH_ERR_PARAM;

    /* RAII 自愈: 同名残留 (kill/崩溃) 先确定性删除 */
    const std::string stage_s(stage);
    if (aio_atomic::path_exists(stage_s, nullptr)) {
        if (aio_atomic::remove_tree(stage_s, 0) != 0) return AIO_PUBLISH_ERR_IO;
    }

    /* 逐段 mkdir -p (aio 机制), 再独占建 staging 本身 */
    char parent[PUBLISH_PATH_MAX];
    char base[PUBLISH_PATH_MAX];
    publish_split_dir(out_dir_utf8, parent, sizeof(parent), base, sizeof(base));
    if (aio_atomic::make_dirs(std::string(parent)) != 0) return AIO_PUBLISH_ERR_IO;
    const int mrc = aio_atomic::make_dir(stage_s);
    if (mrc != 0) {
        if (mrc == EEXIST) return AIO_PUBLISH_ERR_STATE;  /* 病态并发占用 */
        return AIO_PUBLISH_ERR_IO;
    }
    memcpy(stage_path_buf, stage, n + 1);
    return AIO_PUBLISH_OK;
}

int aio_publish_stage_discard_v1(const char* out_dir_utf8) {
    if (!publish_str_ok(out_dir_utf8)) return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_discard_noop")) return AIO_PUBLISH_OK;
    char stage[PUBLISH_PATH_MAX];
    const int rc = publish_stage_path(out_dir_utf8, stage, sizeof(stage), nullptr);
    if (rc != AIO_PUBLISH_OK) return rc;
    const std::string stage_s(stage);
    if (!aio_atomic::path_exists(stage_s, nullptr))
        return AIO_PUBLISH_OK;                            /* 幂等 */
    return aio_atomic::remove_tree(stage_s, 0) == 0 ? AIO_PUBLISH_OK
                                                    : AIO_PUBLISH_ERR_IO;
}

int aio_publish_tree_fsync_v1(const char* stage_path_utf8,
                              uint64_t* n_files_out, uint64_t* total_bytes_out) {
    if (!publish_str_ok(stage_path_utf8)) return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_fsync_fail")) return AIO_PUBLISH_ERR_IO;
    if (n_files_out) *n_files_out = 0;
    if (total_bytes_out) *total_bytes_out = 0;
    int is_dir = 0;
    if (!aio_atomic::path_exists(std::string(stage_path_utf8), &is_dir) || !is_dir)
        return AIO_PUBLISH_ERR_IO;
    return publish_fsync_tree(std::string(stage_path_utf8), 0, n_files_out,
                              total_bytes_out);
}

int aio_publish_promote_v1(const char* out_dir_utf8, const char* stage_path_utf8) {
    if (!publish_str_ok(out_dir_utf8) || !publish_str_ok(stage_path_utf8))
        return AIO_PUBLISH_ERR_PARAM;
    if (publish_fault_active("p1_promote_fail")) return AIO_PUBLISH_ERR_STATE;

    const std::string stage_s(stage_path_utf8);
    const std::string out_s(out_dir_utf8);

    int stage_is_dir = 0;
    if (!aio_atomic::path_exists(stage_s, &stage_is_dir) || !stage_is_dir)
        return AIO_PUBLISH_ERR_STATE;

    int target_is_dir = 0;
    if (aio_atomic::path_exists(out_s, &target_is_dir)) {
        if (!target_is_dir) return AIO_PUBLISH_ERR_STATE;   /* 目标被文件占用 */
        int ok = 0;
        const int ne = aio_atomic::dir_is_nonempty(out_s, &ok);
        if (!ok) return AIO_PUBLISH_ERR_IO;
        if (ne != 0) return AIO_PUBLISH_ERR_STATE;          /* 唯一目标: 拒非空 */
    }

    /* 状态码映射 (与 publish v1 原实现逐位一致):
     *   POSIX  rename ENOTEMPTY/EEXIST/ENOENT → STATE; EXDEV → IO; 其他 → IO
     *   Win32  MoveFileExA ERROR_NOT_SAME_DEVICE → IO; 其他 → STATE */
    const int prc = aio_atomic::promote_dir(stage_s, out_s);
    if (prc == aio_atomic::PROMOTE_CROSS_DEVICE) return AIO_PUBLISH_ERR_IO;
    if (prc == aio_atomic::PROMOTE_STATE) return AIO_PUBLISH_ERR_STATE;
    if (prc != aio_atomic::PROMOTE_OK) return AIO_PUBLISH_ERR_IO;

    /* 父目录 fsync (rename 元数据落盘; 尽力 — ENOSPC 等不回滚已成功的
     * rename, 记录不失败: 树原子性已达成) */
    aio_atomic::fsync_parent_dir(out_s);
    return AIO_PUBLISH_OK;
}
