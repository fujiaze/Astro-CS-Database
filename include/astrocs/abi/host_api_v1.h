/* AstroCS 模块 C ABI v1 — Host API：runtime 注入的全部 host services（ABI-001）
 *
 * 文件: include/astrocs/abi/host_api_v1.h
 * 依赖: status_codes.h / artifact_api_v1.h（单向; 自身独立可编译）。
 *
 * 角色: 本文件定义 host（astrocs_runtime.dll / astrocs_io.dll, ARC-001 §1.3
 * 入口 astrocs_host_api_v1）对 module/provider 暴露的全部服务表。module 在
 * query 期经 astrocs_module_query_v1 收到 host 指针（12 §1）; provider 同理。
 *
 * 服务清单（12 §3 + 03_TARGET §4 + ARC-001 host service 列表）:
 *   allocator / logger / metrics / cancel / progress /
 *   executor+thread budget / artifact+manifest(data services) /
 *   config query / registry query(只读)。
 * module 不得: 自行打开任意路径、私建无界线程池、写 stdout、吞异常、
 * 释放 host 内存（12 §3, FORBID-002/003/007/010）。
 *
 * 跨 DLL 调用面规则（status_codes.h 冻结规则全集适用）:
 *   - 全部 struct 前两字段 acs_head; 表版本失配 → ACS_ERR_ABI_MISMATCH。
 *   - 全部文本 UTF-8 长度显式; 返回字符串所有权=所属句柄/服务。
 *   - host 回调/字段在 module 加载期读一次; module 不得改写表内容。
 *   - 异常边界: module 侧所有经 DLL 导出的 C 函数内部必须捕获全部 C++
 *     异常并转 acs_status; host 侧回调内 module 代码不得让异常穿过 host
 *     (实现侧 try/catch 全包裹)。
 */
#ifndef ASTROCS_ABI_HOST_API_V1_H
#define ASTROCS_ABI_HOST_API_V1_H

#include "astrocs/abi/artifact_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── metrics 计数器标识（v1 冻结; 模块在计划期声明要上报的计数） ── */
enum {
    ACS_METRIC_CPU_NS = 0,        /* 累计 CPU 耗时 ns */
    ACS_METRIC_WALL_NS = 1,       /* 累计墙钟 ns */
    ACS_METRIC_BYTES_READ = 2,    /* 经 host artifact 读字节 */
    ACS_METRIC_BYTES_WRITTEN = 3, /* 经 host artifact 写字节 */
    ACS_METRIC_COUNT = 4          /* 哨兵 */
};

/* ── host logger: 线程安全由 host 保证; module 内部并行可并发调用 ──
 * level 用 ACS_LOG_*; component 借入。 */
typedef struct acs_logger_v1 {
    acs_head head;
    void (*log)(void* user_data, int level,
                acs_str_v1 component, acs_str_v1 msg);
    void* user_data;
} acs_logger_v1;

/* ── cancel: 单向置位(host→module); module 只读轮询, 在冻结安全点检查 ── */
typedef struct acs_cancel_v1 {
    acs_head head;
    int (*is_cancelled)(void* user_data);  /* 0/1, 原子读 */
    void* user_data;
} acs_cancel_v1;

/* ── executor + ThreadBudget: 全 Phase 唯一共享 executor（约束 D.3/D.4）──
 * module 不得私建线程池; 重计算只使用 host 授予的租借（FORBID-003）。
 * acquire 原子租借 n 个 worker: 0=成功, 非 0=预算不足(ACS_ERR_BUDGET);
 * release 归还; 全部归还后才可返回 execute 完成。 */
typedef struct acs_executor_v1 {
    acs_head head;
    uint32_t available_cpus;   /* affinity∩cgroup∩Job Object */
    uint32_t max_workers;      /* 本次调用允许的 worker 上限 */
    int (*acquire)(void* user_data, uint32_t n);
    void (*release)(void* user_data, uint32_t n);
    void* user_data;
} acs_executor_v1;

/* ── host services 总表: 加载时注入, module 只读; 空字段(NULL 服务) = 不支持,
 * module 应先查再调。本表被 module/provider 的 query 入口接收。 ── */
typedef struct acs_host_api_v1 acs_host_api_v1;   /* 前向声明: self 参数 C11 可见性 */
typedef struct acs_host_api_v1 {
    acs_head head;                       /* struct_size/abi_version(ACS_ABI_VERSION_V1) */
    const acs_allocator_v1*    allocator;   /* 必填; NULL → query 拒绝 */
    const acs_logger_v1*       logger;      /* 可空 */
    const acs_cancel_v1*       cancel;      /* 可空; execute 期间应轮询 */
    const acs_executor_v1*     executor;    /* 可空(仅 io/metadata 类 module) */
    const acs_artifact_service_v1* artifacts; /* 可空; 无 I/O 的 module 可为 NULL */

    /* 配置/registry 只读查询（FORBID-007: module 不得写 registry/manifest）:
     * config_query 返回 module 配置 JSON(所有权=host 内部静态, 调用期有效);
     * registry_lookup_module 按 module_id 查 descriptor JSON(只读)。 */
    acs_status (*config_query)(const acs_host_api_v1* self,
                               acs_str_v1 module_id,
                               acs_str_v1 key,
                               acs_strbuf_v1* out,   /* 调用方 buffer */
                               acs_error_info_v1* err);
    acs_status (*registry_lookup_module)(const acs_host_api_v1* self,
                                         acs_str_v1 module_id,
                                         acs_str_v1* out_json_utf8, /* 所有权=host */
                                         acs_error_info_v1* err);
} acs_host_api_v1;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_ABI_HOST_API_V1_H */
