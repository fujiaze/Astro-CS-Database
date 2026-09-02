/* AstroCS 模块 C ABI v1 — 数据平面（Artifact/JSON/host data services）（ABI-001）
 *
 * 文件: include/astrocs/abi/artifact_api_v1.h
 *
 * 依赖: 仅 include/astrocs/abi/status_codes.h（POD 基础层）。
 *
 * 角色: module DLL 读写"typed artifact / manifest / 任意 JSON/字节"全部经
 * host data services（acs_artifact_service_v1）。FORBID-002/FORBID-006:
 * module 不得直接打开文件、猜测路径或依赖 io.dll 私有接口 —— 一切 I/O 经
 * runtime host 注入的服务表。io.dll 是唯一直接 I/O 层; 第三方 I/O 封装在
 * io.dll 内不泄漏进模块 ABI（ARC-001 FORBID-002, 03_TARGET §4）。
 *
 * 冻结规则（同 status_codes.h + 本文件专属）:
 *   - opaque handle 规则: acs_artifact_handle_v1 / acs_manifest_handle_v1 为
 *     不透明指针, 生命周期只经各自的 open/close 对; 禁止把 handle 值当整数、
 *     跨 DLL 释放、或猜测内部布局（FORBID-004）。同 handle 不得并发调用。
 *   - UTF-8 span 规则: 全部文本/JSON 以 (const char* + uint64_t bytes) 显式
 *     长度; UTF-8; 返回字符串所有权=句柄, 有效至 close; 调用方不得 free。
 *   - 内存所有权: 只有两通道 —— (a) 跨边界内存全经 host allocator
 *     (acs_allocator_v1, 分配方=host 提供, 释放方=同一 allocator);
 *     (b) 输出对象由 host 分配、host 用同一 allocator 释放;
 *     句柄内部存储由 host 自持, module 只经查询 API 读, 禁止反向释放。
 *   - UTF-16 路径: Windows 内部文件系统路径 UTF-16; module 不直接触碰
 *     storage_uri → 真实路径的解析（只发生在 io.dll 内部）。
 *
 * 并发模板（12 §2 / API-001 §3）: 函数级注释必含
 *   reentrant: yes|no;  threadsafe: yes|no;  internal_parallel: none|...;
 *   aliasing: ...;  所有权: ...。本文件服务表自身在 module 加载期只读
 *   （threadsafe=yes, 无内部并行）。
 */
#ifndef ASTROCS_ABI_ARTIFACT_API_V1_H
#define ASTROCS_ABI_ARTIFACT_API_V1_H

#include "astrocs/abi/status_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* opaque artifact handle: 只经 acs_artifact_service_v1 的
 * artifact_open/close; 同 handle 不得并发访问; 句柄非可移植值。 */
typedef struct acs_artifact_handle_v1_s acs_artifact_handle_v1;

/* opaque JSON/manifest 句柄: 只经 manifest_parse/destroy */
typedef struct acs_manifest_handle_v1_s acs_manifest_handle_v1;

/* host data service 前向声明: 函数指针表内 self 参数需在 typedef 完成前可见 (C11) */
typedef struct acs_artifact_service_v1 acs_artifact_service_v1;

/* ═══════════════════════════ host data services ═══════════════════════════
 * 由 runtime（或 io.dll 作为 host 服务宿主）填充并注入; module 加载期读一次,
 * 生命周期内表内容不变。每个函数失败返回非 0 状态并（可空）写 err。
 * 并发: 服务表字段只读（threadsafe=yes）; 各句柄方法并发规则见函数注释。 */
typedef struct acs_artifact_service_v1 {
    acs_head head;

    /* 从 storage_uri 打开 artifact（manifest 声明 storage_uri; 解析只发生在
     * host/io 内部, module 不接触文件系统路径）。句柄所有权转移给调用方,
     * 必须 artifact_close。失败: *out=NULL。
     * reentrant=yes; threadsafe=no(同句柄); aliasing=n/a; 所有权: 句柄→调用方。 */
    acs_status (*artifact_open)(const acs_artifact_service_v1* self,
                                acs_str_v1 storage_uri,
                                acs_str_v1 expected_digest_hex,   /* 可空(空串) */
                                acs_artifact_handle_v1** out,
                                acs_error_info_v1* err);
    /* 关闭句柄并释放其内部存储。h=NULL 为空操作。调用后不得再访问 host 对象。 */
    void (*artifact_close)(acs_artifact_handle_v1* h);

    /* 查询句柄声明字段; 返回字符串所有权=句柄, 有效至 close; 不失败。 */
    acs_str_v1 (*artifact_query_uri)(const acs_artifact_handle_v1* h);
    acs_str_v1 (*artifact_query_type_id)(const acs_artifact_handle_v1* h);
    acs_str_v1 (*artifact_query_content_digest_hex)(const acs_artifact_handle_v1* h);
    uint64_t   (*artifact_query_size)(const acs_artifact_handle_v1* h);

    /* 读取整个 artifact 内容到 host 分配的 buffer。
     * 所有权: out_data 由 host allocator 分配; 调用方用同一 allocator free。
     * reentrant=yes; threadsafe=no(同句柄); internal_parallel=none。 */
    acs_status (*artifact_read_all)(const acs_artifact_handle_v1* h,
                                    const acs_allocator_v1* alloc,
                                    acs_span_u8* out_data,
                                    acs_error_info_v1* err);

    /* JSON 严格解析（顶层重复 key/非法 UTF-8 等拒绝）→ opaque 句柄。
     * 输入 json 借入, 解析期内只读; 句柄所有权→调用方, 必须 manifest_destroy。
     * reentrant=yes; threadsafe=yes(不同句柄); internal_parallel=none。 */
    acs_status (*manifest_parse)(const acs_artifact_service_v1* self,
                                 acs_str_v1 json_utf8,
                                 acs_manifest_handle_v1** out,
                                 acs_error_info_v1* err);
    void (*manifest_destroy)(acs_manifest_handle_v1* h);

    /* 查询 JSON 顶层字段（UTF-8）。命中且为字符串 → 返回 true + 值;
     * 缺失/类型不符 → false。值所有权=句柄。 */
    int  (*manifest_get_str)(const acs_manifest_handle_v1* h,
                             acs_str_v1 key,
                             acs_str_v1* out_value);
    int  (*manifest_get_u64)(const acs_manifest_handle_v1* h,
                             acs_str_v1 key,
                             uint64_t* out_value);
} acs_artifact_service_v1;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_ABI_ARTIFACT_API_V1_H */
