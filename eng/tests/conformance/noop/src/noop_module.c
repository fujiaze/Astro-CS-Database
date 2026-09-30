/* ACSD noop conformance module — eng/tests/conformance/noop/src/noop_module.c
 *
 * 状态: BLD-003 SKELETON (宿主骨架 G2 的可加载性探针)。本文件只负责:
 *   - 提供可被安全 loader (ABI-003) 加载的独立 SHARED target;
 *   - 唯一导出 acsd_module_query_v1 (12 §1 / ARC-001 §1.1: module DLL
 *     不得导出其他符号; ABI-006 全查 exports);
 *   - query 握手: host_abi 失配 → ACS_ERR_ABI_MISMATCH (不降级猜测);
 *     out_api = 模块静态表 (所有权=module 静态, 有效至模块卸载);
 *   - describe 返回静态 descriptor (module_id=acsd.conformance.noop)。
 *
 * 明确 NOT 实现 (不伪装完成; 语义由 ABI-005 noop/echo conformance module 填充):
 *   - validate_config / plan / create / execute / inspect 返回
 *     ACS_ERR_UNSUPPORTED, 标注 SKELETON pending ABI-005;
 *   - 本文件无任何 host callback 正/负语义、无 artifact/executor/cancel/metrics
 *     探针 — 那些属 ABI-005 (eng/tests/abi) 与 WIN-* (Windows 侧)。
 *
 * 编译合同: 纯 C11; 无 STL/异常/RTTI; -fno-exceptions 亦可编译;
 * 边界函数无 C++ 异常 (无 throw), 故无需 try/catch 包裹。
 * 导出可见性: ACSD_ABI_SHARED 由 target 编译期定义; 本文件在构建 DLL 本体
 * 时需 ACSD_ABI_EXPORTS (由 CMake target 编译定义) → dllexport/visibility。
 */
#include "acsd/abi/module_api_v1.h"

#include <string.h>

/* 本 target 编译时由 CMake 定义 ACSD_ABI_EXPORTS; 若缺失(如直接 gcc 试编译),
 * 补定义以保证 ACSD_EXPORT 在共享构建时展开为可见导出。 */
#if defined(ACSD_ABI_SHARED) && !defined(ACSD_ABI_EXPORTS)
#define ACSD_ABI_EXPORTS 1
#endif

/* ───────── 静态字符串 (descriptor 输出; 所有权=module 静态, 调用方不得 free) ───────── */

static const char kModuleId[] = "acsd.conformance.noop";
static const char kVersion[] = "0.1.0-alpha.1";
static const char kBuildId[] = "BLD-003-skeleton";
static const char kSciId[] = "SCI-NONE";        /* conformance: 无科学合同 */
static const char kAlgId[] = "ALG-NONE";        /* conformance: 无算法合同 */
static const char kApiId[] = "API-ABI-001";     /* module C ABI v1 (ABI-001) */

static acsd_str_v1 acsd_str_from(const char* s) {
    acsd_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acsd_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = (uint64_t)strlen(s);
    return v;
}

/* ───────── 生命周期 vtable 实现 (BLD-003 SKELETON) ───────── */

static acsd_status noop_describe(const acsd_module_api_v1* self,
                                acsd_str_v1 module_id,
                                acsd_module_descriptor_v1* out_desc) {
    (void)self;
    if (!out_desc) return ACS_ERR_PARAM;
    memset(out_desc, 0, sizeof(*out_desc));
    out_desc->head.struct_size = (uint32_t)sizeof(acsd_module_descriptor_v1);
    out_desc->head.abi_version = ACS_ABI_VERSION_V1;
    out_desc->module_id  = acsd_str_from(kModuleId);
    out_desc->version    = acsd_str_from(kVersion);
    out_desc->build_id   = acsd_str_from(kBuildId);
    out_desc->sci_id     = acsd_str_from(kSciId);
    out_desc->alg_id     = acsd_str_from(kAlgId);
    out_desc->api_id     = acsd_str_from(kApiId);
    out_desc->phase             = 0;   /* 平台/conformance (module.yaml registry 定义) */
    out_desc->config_schema_ver = 1;
    out_desc->execution_class   = 2;   /* metadata (与 module_api_v1.h enum 注释一致) */
    out_desc->parallel_ok       = 0;
    out_desc->flags             = 0;
    /* module_id 参数非空且不等于本模块 → 失配 */
    if (module_id.data != NULL && module_id.size > 0) {
        if (module_id.size != strlen(kModuleId) ||
            memcmp(module_id.data, kModuleId, module_id.size) != 0) {
            return ACS_ERR_PARAM;
        }
    }
    return ACS_OK;
}

static acsd_status noop_validate_config(const acsd_module_api_v1* self,
                                       acsd_str_v1 config_json,
                                       acsd_error_info_v1* err) {
    (void)self; (void)config_json;
    if (err) { err->status = ACS_ERR_UNSUPPORTED; err->domain = ACS_ERR_DOMAIN_CONFIG; }
    return ACS_ERR_UNSUPPORTED;   /* SKELETON: ABI-005 填充 */
}

static acsd_status noop_plan(const acsd_module_api_v1* self,
                            acsd_str_v1 node_id,
                            acsd_str_v1 config_json,
                            acsd_strbuf_v1* out_plan_json,
                            acsd_error_info_v1* err) {
    (void)self; (void)node_id; (void)config_json; (void)out_plan_json;
    if (err) { err->status = ACS_ERR_UNSUPPORTED; err->domain = ACS_ERR_DOMAIN_CONFIG; }
    return ACS_ERR_UNSUPPORTED;   /* SKELETON: ABI-005 填充 */
}

static acsd_status noop_create(const acsd_module_api_v1* self,
                              acsd_str_v1 config_json,
                              const acsd_host_api_v1* host,
                              acsd_module_instance_v1** out,
                              acsd_error_info_v1* err) {
    (void)self; (void)config_json; (void)host;
    if (err) { err->status = ACS_ERR_UNSUPPORTED; err->domain = ACS_ERR_DOMAIN_CONFIG; }
    if (out) *out = NULL;
    return ACS_ERR_UNSUPPORTED;   /* SKELETON: ABI-005 填充 */
}

static acsd_status noop_execute(acsd_module_instance_v1* inst,
                               acsd_str_v1 input_manifest_json,
                               acsd_str_v1 config_json,
                               acsd_strbuf_v1* out_manifest_json,
                               acsd_error_info_v1* err) {
    (void)inst; (void)input_manifest_json; (void)config_json; (void)out_manifest_json;
    if (err) { err->status = ACS_ERR_UNSUPPORTED; err->domain = ACS_ERR_DOMAIN_CONFIG; }
    return ACS_ERR_UNSUPPORTED;   /* SKELETON: ABI-005 填充 */
}

static acsd_status noop_inspect(const acsd_module_instance_v1* inst,
                               acsd_strbuf_v1* out_json,
                               acsd_error_info_v1* err) {
    (void)inst; (void)out_json;
    if (err) { err->status = ACS_ERR_UNSUPPORTED; err->domain = ACS_ERR_DOMAIN_INTERNAL; }
    return ACS_ERR_UNSUPPORTED;   /* SKELETON: ABI-005 填充 */
}

static acsd_status noop_request_cancel(acsd_module_instance_v1* inst) {
    (void)inst;
    return ACS_OK;   /* 无实例语义; 置位为空操作 (ABI-002: request_cancel 可为空操作) */
}

static void noop_destroy(acsd_module_instance_v1* inst) {
    (void)inst;   /* SKELETON: 无实例状态; inst=NULL 或任何值均为空操作 */
}

/* 模块静态 vtable: 所有权=module 静态存储 (12 §1); host 只读 */
static const acsd_module_api_v1 g_noop_api = {
    { (uint32_t)sizeof(acsd_module_api_v1), ACS_ABI_VERSION_V1 },
    noop_describe,
    noop_validate_config,
    noop_plan,
    noop_create,
    noop_execute,
    noop_inspect,
    noop_request_cancel,
    noop_destroy
};

/* ───────── 唯一导出入口 (12 §1) ─────────
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测 (ABI-002)。
 * host 必填 allocator (host_api_v1.h: allocator 必填, NULL → query 拒绝)。
 */
ACSD_EXPORT acsd_status ACSD_CALL
acsd_module_query_v1(uint32_t host_abi,
                        const acsd_host_api_v1* host,
                        const acsd_module_api_v1** out_api) {
    if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (!host || !host->allocator) return ACS_ERR_ABI_MISMATCH;
    if (!out_api) return ACS_ERR_PARAM;
    *out_api = &g_noop_api;
    return ACS_OK;
}
