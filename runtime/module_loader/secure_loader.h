/* AstroCS 模块 C ABI v1 — Secure Loader 合同头（ABI-003）
 *
 * 文件: runtime/module_loader/secure_loader.h
 * 依赖: astrocs/abi/module_api_v1.h（族链: module → host → artifact → status）;
 *       本头独立可编译（C11 与 C++17, -fno-exceptions 亦可）。
 *
 * 权威（ABI-003; 规格 = 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-003 节 +
 * 12_DLL_ABI_AND_LOADER_STANDARD.md §6 + AstroCS_ENGINEERING_CONSTRAINTS.md §F3）:
 *   - Windows: 受控绝对路径 + SetDefaultDllDirectories/AddDllDirectory/
 *     LoadLibraryExW(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
 *     LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32),
 *     参数为 canonical absolute path; 禁止当前目录/PATH/SearchPath/用户数据目录发现。
 *   - Linux: 仅从 product manifest 的绝对 canonical path dlopen(RLD_NOW|RLD_LOCAL);
 *     禁止 LD_LIBRARY_PATH 作为发现语义。
 *   - 两平台统一加载前后校验: 路径(canonical/白名单 root)、sha256、module ID、
 *     ABI version、build ID; 失败写诊断但绝不执行 fallback 静态算法。
 *
 * 实现分层（本文件族）:
 *   host(registry, ABI-004 接入) 从 packaging/astrocs.product.json 解析出 unit
 *   记录(unit_id/kind/rel_path→abs/sha256/module_id/abi_version)填入
 *   acs_load_manifest_unit_v1 —— loader 不做 JSON 解析(12 §3: module 不自行开路径),
 *   只强制 unit 的语义(绝对+canonical+hash+ID 一致性), 未登记/相对/非 canonical
 *   一律拒绝。
 *
 * 日志纪律(12 §6 / 验收"日志不泄凭据"): loader 写 logger 的消息只含原因与
 * detail_code, 不含路径、不含 sha256 值、不含文件内容; 详细诊断(含路径)经
 * acs_error_info_v1 返回调用方, 由调用方决定去向。错误消息字符串为编译期常量
 * 字面量, 所有权=loader 静态, 永久有效, 调用方不得 free。
 *
 * 状态码映射(v1 冻结, 数值来自 status_codes.h; detail_code 是权威细分):
 *   ACS_ERR_PARAM       参数/路径策略拒绝(相对路径/非 canonical/escape 白名单根/
 *                       manifest 期望 sha256 与实际不符/kind 不支持/输入空)
 *   ACS_ERR_IO          文件不存在/不可读/读失败(realpath 失败=不存在)
 *   ACS_ERR_ABI_MISMATCH ELF 格式/架构/类别不符、缺必需导出符号、query 握手
 *                       host_abi 失配、descriptor head/abi_version/module_id 失配、
 *                       build ID 与 manifest 不符
 *   ACS_ERR_NOMEM       host allocator 失败
 *   ACS_ERR_UNSUPPORTED 当前平台不提供该实现(_WIN32 下由 WIN-* 落地前)
 *   ACS_ERR_INTERNAL    内部不变量违例(不可恢复, 视为 bug)
 * 所有拒绝均非 0 且 err->detail_code 填本头枚举; err->domain=ACS_ERR_DOMAIN_CONFIG。
 */
#ifndef ASTROCS_ABI_SECURE_LOADER_H
#define ASTROCS_ABI_SECURE_LOADER_H

#include "astrocs/abi/module_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* loader 合同版本(独立于 ABI v1; 头部/语义扩展走尾部追加) */
#define ACS_SECURE_LOADER_VERSION_V1 1u

/* ═══════════════ loader 细分错误码(权威 = err->detail_code) ═══════════════ */
enum {
    ACS_LOADER_EC_NONE = 0,
    ACS_LOADER_EC_INVALID_INPUT = 1,     /* 空 options/unit/allocator/out */
    ACS_LOADER_EC_PATH_NOT_ABS = 2,      /* abs_path 非绝对 */
    ACS_LOADER_EC_PATH_NOT_CANONICAL = 3,/* realpath 后与入参不一致(含目录内 symlink) */
    ACS_LOADER_EC_PATH_ESCAPE = 4,       /* canonical 结果在白名单 allowed_root 外 */
    ACS_LOADER_EC_HASH_MISMATCH = 5,     /* manifest 期望 sha256 与实际文件不符 */
    ACS_LOADER_EC_FILE_MISSING = 6,      /* 文件不存在 */
    ACS_LOADER_EC_FILE_READ = 7,         /* 打开/读取失败 */
    ACS_LOADER_EC_ELF_MAGIC = 8,         /* 非 ELF */
    ACS_LOADER_EC_ELF_CLASS = 9,         /* 非 ELFCLASS64 */
    ACS_LOADER_EC_ELF_MACHINE = 10,      /* e_machine != EM_X86_64(AMD64) */
    ACS_LOADER_EC_SYMBOL_MISSING = 11,   /* 缺必需导出符号(module/provider 入口) */
    ACS_LOADER_EC_HANDSHAKE_ABI = 12,    /* query host_abi 失配 */
    ACS_LOADER_EC_HANDSHAKE_PARAM = 13,  /* query 返回 PARAM/空 out_api */
    ACS_LOADER_EC_DESCRIPTOR_MISMATCH = 14, /* descriptor head/abi/version 失配 */
    ACS_LOADER_EC_MODULE_ID_MISMATCH = 15,  /* descriptor.module_id != manifest */
    ACS_LOADER_EC_BUILD_ID_MISMATCH = 16,   /* descriptor.build_id != manifest 期望 */
    ACS_LOADER_EC_KIND_UNSUPPORTED = 17,    /* unit.kind 非 module/provider */
    ACS_LOADER_EC_OS_LOAD = 18,          /* dlopen/LoadLibraryExW 失败 */
    ACS_LOADER_EC_INTERNAL = 70
};

/* ═══════════════ manifest unit(host 从 product manifest 解析; 借入) ═══════════════
 * 所有权: 全部 span 借入, 调用方持有, 有效至 load 调用返回。
 * kind: "module" → 必需 astrocs_module_query_v1 + describe 校验;
 *       "provider" → 必需 astrocs_provider_query_v1 + 握手校验;
 *       其它 → ACS_LOADER_EC_KIND_UNSUPPORTED。 */
typedef struct acs_load_manifest_unit_v1 {
    acs_head    head;               /* sizeof/ACS_ABI_VERSION_V1 */
    acs_str_v1  unit_id;            /* 如 "MOD-NOOP"; 诊断用 */
    acs_str_v1  kind;               /* "module"|"provider" */
    acs_str_v1  abs_path_utf8;      /* UTF-8 绝对路径(必须绝对; 加载前 canonical 化) */
    acs_str_v1  module_id;          /* 期望 module_id; 空 = 不校验(非 module kind) */
    acs_str_v1  expected_sha256;    /* 64 hex 小写; 空 = 跳过 hash(SKELETON manifest
                                       兼容; 生产 product manifest 必须登记) */
    acs_str_v1  expected_build_id;  /* 期望 build_id; 空 = 不强制 */
    uint32_t    abi_version;        /* 期望 ABI(ACS_ABI_VERSION_V1); 0 = 不校验 */
    uint32_t    reserved;           /* v1 = 0 */
} acs_load_manifest_unit_v1;

/* ═══════════════ loader options(借入) ═══════════════ */
typedef struct acs_loader_options_v1 {
    acs_head    head;               /* sizeof/ACS_ABI_VERSION_V1 */
    acs_load_manifest_unit_v1 unit; /* 必填 */
    acs_str_v1  allowed_root_utf8;  /* 可空: canonical 结果必须位于该根下(防 symlink
                                       escape); 空 = 不做 root 检查(仍强制 canonical) */
    const acs_allocator_v1* allocator; /* 必填: 句柄/输出字符串内存归属 */
    const acs_logger_v1*    logger;    /* 可空; 消息只含原因+detail(不泄路径/内容) */
} acs_loader_options_v1;

/* ═══════════════ loaded 信息(describe 输出; 调用方提供存储) ═══════════════
 * resolved_path_utf8 / loaded_sha256: loader 经 options.allocator 分配,
 * 有效至 handle release(loader 释放), 调用方不得 free、不得在 release 后访问。
 * module_id / module_version / build_id / api_id: 借 module 静态存储(describe),
 * 有效至 handle release(模块卸载), 调用方不得 free。
 * module_api / provider_api: 借模块静态 vtable, 同上生命周期; 所有权=module。 */
typedef struct acs_loaded_module_v1 {
    acs_head    head;               /* 调用方填 sizeof/ABI_VERSION; loader 校验 */
    acs_str_v1  resolved_path_utf8; /* realpath 后实际加载的 canonical 路径 */
    acs_str_v1  loaded_sha256;      /* 实际加载文件 sha256(64 hex 小写) */
    const acs_module_api_v1*   module_api;   /* kind=module 握手成功; 否则 NULL */
    const acs_provider_api_v1* provider_api; /* kind=provider 握手成功; 否则 NULL */
    acs_str_v1  module_id;          /* 与 manifest 一致校验通过 */
    acs_str_v1  module_version;     /* descriptor.version */
    acs_str_v1  build_id;           /* descriptor.build_id(与 manifest 期望一致) */
    acs_str_v1  api_id;             /* descriptor.api_id("API-ABI-001" 类) */
    uint32_t    abi_version;        /* 校验确认的 ABI version */
    uint32_t    detail_code;        /* 本 handle 最近一次操作 detail(0=成功) */
} acs_loaded_module_v1;

/* ═══════════════ opaque handle ═══════════════ */
typedef struct acs_loader_handle_s acs_loader_handle;

/* ───────── 安全加载(唯一加载入口) ─────────
 * 流程(两平台一致语义; Linux 真实实现, Windows 契约见文件头注释):
 *   1. 校验 opt/unit/allocator(head 版本/非空);
 *   2. 路径: 必须绝对; realpath canonical; 与 allowed_root 前缀校验(escape 拒);
 *      Windows: 同样先 canonical + root 校验再 LoadLibraryExW;
 *   3. 读文件: sha256(FIPS 180-4 内部实现)与 unit.expected_sha256 比对(失配拒);
 *      读前先验 ELF/PE 头(仅 Linux ELF64-x86-64 接受);
 *   4. dlopen(RLD_NOW|RLD_LOCAL)(Windows: LoadLibraryExW 安全 flags);
 *   5. 加载后: 解析必需入口符号; module → host_abi 握手(ACS_ABI_VERSION_V1)+
 *      describe → 校验 descriptor head/abi_version/module_id/build_id/version。
 *   任何一步失败: handle=NULL, 非 0 状态 + err(detail_code/domain/message),
 *   不 fallback、不重试其它路径。
 * reentrant=yes; threadsafe=yes(不同 handle); 无内部并行。
 * err 可空(失败时无 detail); out 必填非空, 失败置 *out=NULL。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_secure_loader_load_v1(const acs_loader_options_v1* opt,
                          acs_error_info_v1* err,
                          acs_loader_handle** out);

/* ───────── 读取已加载信息 ─────────
 * out->head 由调用方填 sizeof/ABI_VERSION; loader 校验失配 → ACS_ERR_ABI_MISMATCH。
 * 成功填充后调用方可读 module_api(经其调 lifecycle); handle release 后全部
 * 指针/span 失效。reentrant=yes; threadsafe=yes(只读已冻结状态)。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_secure_loader_describe_v1(acs_loader_handle* h,
                              acs_loaded_module_v1* out);

/* ───────── 释放(唯一卸载入口) ─────────
 * 关闭句柄: dlclose(Windows FreeLibrary) + 释放 loader 分配字符串。
 * 调用方必须先 destroy 全部经 module_api 创建的实例(12 §2), 卸载后表指针失效。
 * h=NULL 为空操作。reentrant=yes; threadsafe=no(同 handle)。 */
ASTROCS_EXPORT void ASTROCS_CALL
acs_secure_loader_release_v1(acs_loader_handle* h);

/* ───────── 平台自检(不加载任何文件) ─────────
 * 校验: 本构建平台受支持、内部 sha256 已知向量(FIPS 180-4: 空串/"abc")正确、
 * 布局静态断言成立。失败返回非 0(ACS_ERR_SELFTEST)。供测试/doctor。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_secure_loader_self_test_v1(void);

/* ───────── 布局静态断言(C11/C++17 双形态) ───────── */
ACS_STATIC_ASSERT(sizeof(acs_load_manifest_unit_v1) > sizeof(acs_head),
                  "manifest unit must carry payload");
ACS_STATIC_ASSERT(offsetof(acs_load_manifest_unit_v1, head) == 0u, "head first");
ACS_STATIC_ASSERT(offsetof(acs_loader_options_v1, head) == 0u, "head first");
ACS_STATIC_ASSERT(offsetof(acs_loaded_module_v1, head) == 0u, "head first");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_ABI_SECURE_LOADER_H */
