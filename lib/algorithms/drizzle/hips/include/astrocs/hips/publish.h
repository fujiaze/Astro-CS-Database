/* publish.h - HiPS 原子发布原语 v1 合同头 (AIO-002)
 *
 * 任务: AIO-002 (ASTROCS-CONSTITUTION-ALIGNMENT-V1) "实现HiPS原子发布与临时
 * 目录清理"。对齐 IO-003 发布流水线 (docs/interfaces/io/IO_003_ATOMIC_OUTPUT_
 * PUBLISH.md §4: 临时写 → 关闭/fsync → 校验 → 原子 rename → 完成标记) 的
 * C ABI 化最小面; 收口 DISP-HIPS-001 (abort 不清理已写文件) / DISP-HIPS-004
 * (无原子发布, aio_hips_writer.cpp:185-186 remove+create 直写) 的模块事务面;
 * 临时目录 RAII 自愈对齐 V7 历史登记 (make_tmp_dir 无清理, 24G tmpfs 填满
 * 19.94G 致 cfitsio status=107 假败)。
 *
 * 归属与导出面: 本 ABI 是 lib/hips 模块事务内核 (astrocs.p1.hips_writer DLL
 * 内部支撑, 与 module_entry.cpp 同编; 对外仍唯一导出 astrocs_module_query_v1,
 * 本头符号经 version-script/DEF 保持隐藏)。测试经 tests/unit/p1_hips/
 * adapter_entry_impl.cpp 直链 TU 使用。
 *
 * 发布状态机 (execute 事务化, module_entry.cpp 调用序):
 *   1. stage_create   兄弟 staging 目录 <parent>/.<name>.hips_staging.tmp
 *                     (先自愈清同名残留 → kill/崩溃残留 RAII 收口)
 *   2. (writer 全量写 stage; legacy aio_hips_* 九导出直写语义不变)
 *   3. tree_fsync     递归 fsync stage 树 (文件+目录, 后序) + 文件计数校验
 *   4. promote        rename(stage → out_dir) 整树原子出现
 *   任一步失败/取消 → stage_discard 递归删除 → out_dir 根无 partial。
 *   out_dir 根要么保持旧态要么出现完整新树 (全有或全无)。
 *
 * 科学纪律: scientific_change=false —— 生产 writer (lib/astro_image_io/
 * aio_hips_writer.cpp) 零改动; tile/归一/MOC/层次公式 (ALG-HIPS-001..005)
 * 不在本头域; 本头只承载发布事务原语。
 *
 * 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常/RTTI 跨边界; 无第三方
 * 类型。并发: 全部函数 reentrant (无共享全局状态); internal_parallel=none
 * (单流遍历, 确定性)。跨边界不抛异常 (实现层 C 屏障, 失败返回稳定状态码)。
 */
#ifndef ASTROCS_HIPS_PUBLISH_H
#define ASTROCS_HIPS_PUBLISH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASTROCS_HIPS_PUBLISH_ABI_VERSION 1u

/* staging 目录固定词根 (basename 前缀 '.' + 后缀 '.hips_staging.tmp';
 * 段词法不含 '/', 不与 HiPS 子产品名 signal/support/variance/ivar/snr
 * 冲突)。 */
#define ASTROCS_HIPS_STAGE_BASENAME ".hips_staging.tmp"

/* ───────── 状态码 (v1 数值冻结) ─────────
 * 0..15 与 70 全域一致于 include/astrocs/io/aio_abi_v1.h aio_status
 * (AIO-001 数值冻结; _Static_assert 编译期对齐证明)。 */
typedef enum aio_publish_status_v1 {
    AIO_PUBLISH_OK = 0,
    AIO_PUBLISH_ERR_PARAM = 1,        /* 参数非法 (NULL/空/路径溢出) */
    AIO_PUBLISH_ERR_ABI_MISMATCH = 2, /* (保留, 数值对齐) */
    AIO_PUBLISH_ERR_NOMEM = 3,
    AIO_PUBLISH_ERR_IO = 4,           /* 建目录/打开/读失败 */
    AIO_PUBLISH_ERR_UNSUPPORTED = 5,
    AIO_PUBLISH_ERR_CANCELLED = 6,    /* (保留; 取消由调用方处置) */
    AIO_PUBLISH_ERR_STATE = 7,        /* 状态非法 (stage 缺失/目标非空) */
    AIO_PUBLISH_ERR_TRUNCATED = 8,    /* (保留, 数值对齐) */
    AIO_PUBLISH_ERR_BAD_HEADER = 9,
    AIO_PUBLISH_ERR_MISMATCH = 10,
    AIO_PUBLISH_ERR_CHECKSUM = 11,
    AIO_PUBLISH_ERR_NANINF = 12,
    AIO_PUBLISH_ERR_DISKFULL = 13,    /* fsync 落盘失败 errno==ENOSPC/EDQUOT */
    AIO_PUBLISH_ERR_DECL_INVALID = 14,
    AIO_PUBLISH_ERR_HASH_MISMATCH = 15,
    AIO_PUBLISH_ERR_INTERNAL = 70,    /* 未分类内部错误 (同 acs_status 语义) */
    AIO_PUBLISH_STATUS_COUNT = 71     /* 哨兵: 该值及以外均非法 */
} aio_publish_status_v1;

/* ───────── 原语 v1 (4 函数; 路径均为 UTF-8, 与 IO-001 同语义) ───────── */

/* 1) 建 staging 目录: <parent(out_dir)>/.<basename>.hips_staging.tmp。
 *    先确定性自愈: 同名残留 staging 递归删除 (RAII 收口 — kill/崩溃后残留
 *    不累积); 再 mkdir parent 逐段 (不存在才建, EEXIST 忽略) 与 staging
 *    本身 (独占)。stage_path_buf 由调用方分配, 成功写入完整 staging 路径
 *    (容量不足 → PARAM; 推荐 1024)。
 *    out_dir NULL/空 → PARAM; 段路径过深 → IO。
 *    reentrant; internal_parallel=none。 */
int aio_publish_stage_create_v1(const char* out_dir_utf8,
                                char* stage_path_buf, uint64_t stage_path_cap);

/* 2) 递归删除 staging 目录 (文件先 unlink, 子目录后序 rmdir; 不跟随
 *    符号链接)。staging 不存在 → OK (幂等; RAII 语义: 删除总是收敛)。
 *    out_dir_utf8 为发布目标根 (staging 路径由其推导, 与 create 一致)。
 *    reentrant; internal_parallel=none。 */
int aio_publish_stage_discard_v1(const char* out_dir_utf8);

/* 3) 递归 fsync staging 树 (后序: 先文件后子目录再自身; 文件统计进
 *    n_files_out/total_bytes_out, 可 NULL)。任一 fsync 失败 → IO;
 *    errno==ENOSPC/EDQUOT → DISKFULL (ENOSPC 收敛点: 写缓冲延迟落盘在
 *    fsync 暴露)。树不存在 → IO。
 *    reentrant; internal_parallel=none。 */
int aio_publish_tree_fsync_v1(const char* stage_path_utf8,
                              uint64_t* n_files_out, uint64_t* total_bytes_out);

/* 4) 原子 promote: rename(stage → out_dir) 整树原子出现 (同文件系统,
 *    内核原子)。out_dir 已存在且非空目录/为普通文件 → STATE (唯一目标:
 *    拒绝覆盖非空); out_dir 不存在或为空目录 → OK (空目录被替换)。
 *    stage 不存在 → STATE。跨设备 EXDEV → IO。
 *    reentrant; internal_parallel=none。 */
int aio_publish_promote_v1(const char* out_dir_utf8,
                           const char* stage_path_utf8);

/* ───────── 故障注入 (测试面; 生产零行为差异) ─────────
 * ASTROCS_HIPS_PUBLISH_FAULT=<name> 使对应函数确定性翻转 (返回错误) 并输出
 * "FAULT-INJECT" 行 (对齐 AIO-001 fault_injection 口径: 注入必败, 不存在
 * 恒 PASS 占位)。注册名:
 *   p1_stage_create_fail  stage_create → IO
 *   p1_fsync_fail         tree_fsync  → IO
 *   p1_promote_fail       promote     → STATE
 *   p1_discard_noop       stage_discard 假清 (返回 OK 不删; 用于验证
 *                         discard 常规路径真实删除 —— 注入后残留仍在)
 *   p1_stage_slow_write   (module_entry.cpp 事务面) tile 循环前延时 600ms
 *                         (kill 中断测试锚点; 注入时事务驻留 staging 期)
 * 未知名忽略 (不注入)。 */

/* kill 中断测试锚点 (module_entry.cpp 调用; 注入 p1_stage_slow_write 时
 * 延时 600ms, 返回 1)。非注入环境恒 0 延时 —— 生产零行为差异。 */
int hips_publish_fault_slow_write_v1(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ───────── 编译期对齐证明 (AIO-001 aio_status 数值域一致) ───────── */
#if defined(__cplusplus)
static_assert(AIO_PUBLISH_OK == 0 && AIO_PUBLISH_ERR_PARAM == 1 &&
                  AIO_PUBLISH_ERR_IO == 4 && AIO_PUBLISH_ERR_STATE == 7 &&
                  AIO_PUBLISH_ERR_TRUNCATED == 8,
              "AIO-002: aio_publish_status 0..8 与 aio_status/acs_status 一致");
static_assert(AIO_PUBLISH_ERR_DISKFULL == 13 &&
                  AIO_PUBLISH_ERR_INTERNAL == 70 &&
                  AIO_PUBLISH_STATUS_COUNT == 71,
              "AIO-002: aio_publish_status 13/70/71 数值冻结");
#else
_Static_assert(AIO_PUBLISH_OK == 0 && AIO_PUBLISH_ERR_PARAM == 1 &&
                   AIO_PUBLISH_ERR_IO == 4 && AIO_PUBLISH_ERR_STATE == 7 &&
                   AIO_PUBLISH_ERR_TRUNCATED == 8,
               "AIO-002: aio_publish_status 0..8 与 aio_status/acs_status 一致");
_Static_assert(AIO_PUBLISH_ERR_DISKFULL == 13 &&
                   AIO_PUBLISH_ERR_INTERNAL == 70 &&
                   AIO_PUBLISH_STATUS_COUNT == 71,
               "AIO-002: aio_publish_status 13/70/71 数值冻结");
#endif

#endif /* ASTROCS_HIPS_PUBLISH_H */
