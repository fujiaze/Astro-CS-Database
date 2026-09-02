/* AstroCS 模块 C ABI v1 — Module API：模块/提供商生命周期接口（ABI-001）
 *
 * 文件: include/astrocs/abi/module_api_v1.h
 * 依赖: host_api_v1.h（单向）; 本文件独立可编译（经 host_api_v1.h 连带
 * artifact_api_v1.h + status_codes.h）。
 *
 * 角色: 每个 module DLL 的唯一导出入口（12 §1 / ARC-001 §1.1）:
 *   ASTROCS_EXPORT acs_status ASTROCS_CALL
 *   astrocs_module_query_v1(uint32_t host_abi,
 *                           const acs_host_api_v1* host,
 *                           const acs_module_api_v1** out_api);
 * module 不得导出其他符号（ARC-001 §1.1; ABI-006 全查 exports）。
 *
 * 生命周期（12 §2 / ARC-001 §2 冻结调用时序）:
 *   query → describe → validate_config → plan → create
 *         → execute* → inspect → request_cancel(可选) → destroy
 * - query: host 与 module ABI 握手; host_abi 失配 → ACS_ERR_ABI_MISMATCH,
 *   不降级猜测。out_api 所有权=module 静态表, 有效至模块卸载;
 *   host 对象在 query 返回后不得再被访问（destroy 后同, 12 §2）。
 * - create 返回 opaque 实例句柄（module 私有状态, 不泄裸所有权）;
 *   同句柄不得并发调用; destroy 后句柄失效。
 * - execute 可重复调用条件/线程安全/输出 sink/cancel 语义由 ABI-002
 *   状态机冻结（本头只冻结类型与入口形状; 每函数注释含并发模板）。
 * - 全部 C++ 异常在 DLL 导出函数内捕获转 acs_status（ACS_ERR_EXCEPTION）。
 */
#ifndef ASTROCS_ABI_MODULE_API_V1_H
#define ASTROCS_ABI_MODULE_API_V1_H

#include "astrocs/abi/host_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* opaque module 实例句柄: 只经 create/destroy 对; 禁止猜测布局/跨 DLL 释放 */
typedef struct acs_module_instance_v1_s acs_module_instance_v1;

/* 模块静态描述（describe 输出; 与 module.yaml/manifest 三方一致校验,
 * 12 §5; 字符串所有权=module 静态存储, 有效至模块卸载, 调用方不得 free） */
typedef struct acs_module_descriptor_v1 {
    acs_head    head;
    acs_str_v1  module_id;        /* 如 "astrocs.p1.calibration" */
    acs_str_v1  version;          /* 语义版本 "1.x" */
    acs_str_v1  build_id;         /* 编译指纹 */
    acs_str_v1  sci_id;           /* SCI-xxx */
    acs_str_v1  alg_id;           /* ALG-xxx */
    acs_str_v1  api_id;           /* API-xxx */
    uint32_t    phase;            /* 1|2|3|catalog=0? 由 module.yaml 注册表定义 */
    uint32_t    config_schema_ver;/* data schema version（与 ABI version 分离, 12 §4） */
    int32_t     execution_class;  /* 0=cpu_heavy 1=io 2=metadata（module.yaml enum） */
    int32_t     parallel_ok;      /* 1=可内部并行(经 host executor) */
    uint64_t    flags;            /* 保留; v1=0 */
} acs_module_descriptor_v1;

/* 生命周期 vtable（module 实现; host 只调表内函数） */
typedef struct acs_module_api_v1 acs_module_api_v1;   /* 前向声明: self 参数 C11 可见性 */
typedef struct acs_module_api_v1 {
    acs_head head;   /* struct_size=sizeof(acs_module_api_v1); abi_version=1 */

    /* 静态描述。reentrant=yes; threadsafe=yes; 无并行; 所有权=module 静态。 */
    acs_status (*describe)(const acs_module_api_v1* self,
                           acs_str_v1 module_id,
                           acs_module_descriptor_v1* out_desc);

    /* 校验 config JSON。不触发科学 execute 或大 I/O（CLI-003 语义）。
     * config 借入; 失败写 err。reentrant=yes; threadsafe=yes(不同句柄); */
    acs_status (*validate_config)(const acs_module_api_v1* self,
                                  acs_str_v1 config_json,
                                  acs_error_info_v1* err);

    /* 生成执行计划(输出 plan JSON, 所有权=调用方 buffer)。不执行科学计算。
     * reentrant=yes; threadsafe=yes; 大 manifest 保持轻量。 */
    acs_status (*plan)(const acs_module_api_v1* self,
                       acs_str_v1 node_id,
                       acs_str_v1 config_json,
                       acs_strbuf_v1* out_plan_json,  /* 调用方 buffer */
                       acs_error_info_v1* err);

    /* create 实例。失败: *out=NULL 且 err 填原因。
     * reentrant=yes; threadsafe=yes(不同实例); 所有权: 句柄→调用方, 必须 destroy。 */
    acs_status (*create)(const acs_module_api_v1* self,
                         acs_str_v1 config_json,
                         const acs_host_api_v1* host,
                         acs_module_instance_v1** out,
                         acs_error_info_v1* err);

    /* execute*（可重复调用条件与取消语义 ABI-002 冻结）。
     * execute 使用 host executor 租借线程; 取消点由 ABI-002 定义。
     * reentrant=no(同实例); threadsafe=no(同实例); internal_parallel=经 host。 */
    acs_status (*execute)(acs_module_instance_v1* inst,
                          acs_str_v1 input_manifest_json,
                          acs_str_v1 config_json,
                          acs_strbuf_v1* out_manifest_json, /* 调用方 buffer */
                          acs_error_info_v1* err);

    /* inspect: 结构化诊断 JSON(不重执行科学计算; 线程安全: 可并发)。 */
    acs_status (*inspect)(const acs_module_instance_v1* inst,
                          acs_strbuf_v1* out_json,
                          acs_error_info_v1* err);

    /* request_cancel(可选): 单向置位; execute 在安全点响应返回
     * ACS_ERR_CANCELLED。reentrant=yes; threadsafe=yes。 */
    acs_status (*request_cancel)(acs_module_instance_v1* inst);

    /* destroy: 释放实例与内部全部资源。inst=NULL 为空操作。
     * 调用后 host 对象不得再被访问（12 §2）。 */
    void (*destroy)(acs_module_instance_v1* inst);
} acs_module_api_v1;

/* ── provider（12 §7; ARC-001 §1.2）: 已登记热点 kernel 提供者 ──
 * provider 加载后 host 先 CPUID/XGETBV 再 LoadLibrary; 加载后 self_test
 * 通过才可加入路由; 不适配 kernel 返回 unsupported 让 host 退 baseline;
 * 不在 DllMain 执行 SIMD。 */
typedef struct acs_kernel_desc_v1 {
    acs_head    head;
    acs_str_v1  kernel_id;     /* 如 "drizzle-accumulate-avx2" */
    acs_str_v1  sci_contract_id; /* ALG-xxx */
    uint32_t    precision;     /* 0=f32 1=f64 */
    uint32_t    determinism_class; /* 0=bitwise 1=fixed_order 2=threadlocal_merge */
} acs_kernel_desc_v1;

typedef struct acs_provider_api_v1 {
    acs_head head;
    acs_status (*self_test)(const acs_host_api_v1* host); /* 失败→ACS_ERR_SELFTEST */
    acs_status (*kernel_list)(const acs_host_api_v1* host,
                              uint32_t* out_count,
                              const acs_kernel_desc_v1** out_kernels); /* 静态表 */
    acs_status (*run_kernel)(uint32_t kernel_index,
                             const acs_host_api_v1* host,
                             const void* params, uint32_t params_bytes,
                             acs_span_u8 in, acs_span_u8 out); /* 所有权见各 kernel 文档 */
} acs_provider_api_v1;

/* 唯一入口函数声明（module; 由 DLL 实现导出）。
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测; out_api 非空时成功返回。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi,
                        const acs_host_api_v1* host,
                        const acs_module_api_v1** out_api);

/* provider 唯一入口声明 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_provider_query_v1(uint32_t host_abi,
                          const acs_host_api_v1* host,
                          const acs_provider_api_v1** out_api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_ABI_MODULE_API_V1_H */
