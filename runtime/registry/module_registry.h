/* AstroCS 动态模块 Registry 合同头（ABI-004）
 *
 * 文件: runtime/registry/module_registry.h
 * 依赖: astrocs/abi/host_api_v1.h（span/错误类型; 族链 status→artifact→host）;
 *       runtime/module_loader/secure_loader.h（ABI-003: 安全加载契约）;
 *       本头独立可编译（C11 与 C++17, -fno-exceptions 亦可）。
 *
 * 权威（ABI-004; 规格 = 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-004 节 +
 * 12_DLL_ABI_AND_LOADER_STANDARD.md §5/§6 + 11_MODULE_SOURCE_TEST_STANDARD.md §4）:
 *   - 运行时 registry 只来自 product manifest + DLL query(经 ABI-003 安全 loader);
 *     static factory 不再是正式路由(12 §1: 模块只经批准的 C ABI 查询入口注册);
 *   - 检测: 重复 ID(unit_id/module_id)、版本冲突(module.yaml↔DLL descriptor)、
 *     未登记 DLL(安装树模块目录存在文件但 manifest 未登记)、descriptor 不一致
 *     (三方 module_id/ABI/hash 不符);
 *   - 三方机器比对: module.yaml(构建期源) / DLL embedded descriptor(query 后
 *     describe) / 发布 product manifest —— module_id、ABI version、version、
 *     DLL 名、hash 一致; 任何不一致 registry verify 失败, 绝不静默 fallback。
 *
 * 实现分层: 本头族(registry) = product manifest 的最小 JSON 提取(仅 units[] 形状)
 * + module.yaml 顶层 key 提取(仅标量字段) + ABI-003 loader 编排(query/describe/
 * sha256) + 检测/报告。registry 不自行加载任意路径: 全部经 manifest 声明 + loader
 * canonical/hash/白名单校验(ABI-003 合同); manifest 相对路径相对 manifest 所在
 * 目录解析后必须绝对 canonical 且(可选)在 allowed_root 内。
 *
 * 错误/日志纪律(与 loader 一致): 错误消息为编译期静态字面量, 不含路径/sha/内容;
 * registry 不写日志文件; 详细诊断经 acs_error_info_v1 返回调用方。
 * 状态码映射: 结构/参数错误 ACS_ERR_PARAM; JSON/schema/检测失败详见
 * ACS_REG_EC_* detail_code + ACS_ERR_* 主码(见下); 底层 loader 拒绝透传其 err。
 */
#ifndef ASTROCS_REGISTRY_MODULE_REGISTRY_H
#define ASTROCS_REGISTRY_MODULE_REGISTRY_H

#include "astrocs/abi/host_api_v1.h"
#include "runtime/module_loader/secure_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_REGISTRY_VERSION_V1 1u

/* ═══════════════ registry 细分错误/发现码(权威 = detail_code/finding.kind) ═══════════════ */
enum {
    ACS_REG_EC_NONE = 0,
    ACS_REG_EC_INVALID_INPUT = 1,      /* 空 options/allocator/out/非法 head */
    ACS_REG_EC_MANIFEST_READ = 2,      /* manifest 打不开/读失败 */
    ACS_REG_EC_MANIFEST_JSON = 3,      /* manifest JSON 语法/结构(缺 units 等) */
    ACS_REG_EC_MANIFEST_SCHEMA = 4,    /* schema 违例(kind 非法/缺必填/sha 非 64hex) */
    ACS_REG_EC_PATH_NOT_ABS = 5,       /* manifest 路径非绝对 */
    ACS_REG_EC_PATH_NOT_CANONICAL = 6, /* canonical 不一致/越界 */
    ACS_REG_EC_DUP_UNIT_ID = 7,        /* 重复 unit_id(检测项) */
    ACS_REG_EC_DUP_MODULE_ID = 8,      /* 重复 module_id(检测项; 版本冲突前置) */
    ACS_REG_EC_VERSION_CONFLICT = 9,   /* module.yaml version ↔ DLL descriptor 冲突 */
    ACS_REG_EC_UNREGISTERED_DLL = 10,  /* 模块目录存在 DLL/.so 但 manifest 未登记 */
    ACS_REG_EC_MODULE_ID_MISMATCH = 11,/* manifest module_id ↔ DLL describe 不符 */
    ACS_REG_EC_HASH_MISMATCH = 12,     /* manifest 登记 sha256 ↔ 实际文件不符 */
    ACS_REG_EC_LOADER = 13,            /* 底层 loader 拒绝(entry 未加载) */
    ACS_REG_EC_YAML_INCONSISTENT = 14, /* module.yaml module_id/abi 与 manifest 不符 */
    ACS_REG_EC_INTERNAL = 70
};

/* ═══════════════ 单条登记记录(registry 输出; 逐字段单位/所有权见注释) ═══════════════
 * 字符串所有权: registry 句柄(allocator 分配), 有效至句柄 close; 调用方不得 free,
 * 不得在 close 后访问。字段内容 = 三方比对后的实际值。 */
typedef struct acs_registry_entry_v1 {
    acs_head    head;
    acs_str_v1  unit_id;            /* manifest unit_id(如 MOD-NOOP) */
    acs_str_v1  kind;               /* module/provider */
    acs_str_v1  rel_path;           /* manifest rel_path */
    acs_str_v1  abs_path;           /* canonical 绝对路径(rel_path 相对 manifest 目录) */
    acs_str_v1  module_id;          /* manifest 登记 module_id(module kind) */
    acs_str_v1  module_id_dll;      /* DLL describe.module_id(未加载=空) */
    acs_str_v1  module_id_yaml;     /* module.yaml module_id(源缺失=空) */
    acs_str_v1  version_dll;        /* DLL describe.version */
    acs_str_v1  version_yaml;       /* module.yaml module_version */
    acs_str_v1  build_id_dll;       /* DLL describe.build_id */
    acs_str_v1  sha256_registered;  /* manifest 登记 sha256(空=null/SKELETON) */
    acs_str_v1  sha256_actual;      /* 现场计算实际 sha256(文件存在时) */
    acs_str_v1  status;             /* manifest status(SKELETON/IMPLEMENTED) */
    uint32_t    abi_version;        /* manifest abi_version */
    uint32_t    loaded;             /* 1 = loader query+describe 成功(正式路由) */
    int32_t     finding_mask;       /* 命中检测位组合(0=clean); 位定义见下 */
    uint32_t    detail_code;        /* 最近错误/发现细分(0=clean) */
} acs_registry_entry_v1;

/* finding_mask 位(entry 级) */
enum {
    ACS_REG_F_LOAD_FAILED = 1 << 0,     /* loader 拒绝该 entry(缺 symbol/ABI/hash…) */
    ACS_REG_F_MODULE_ID_MISMATCH = 1 << 1,
    ACS_REG_F_HASH_MISMATCH = 1 << 2,
    ACS_REG_F_VERSION_CONFLICT = 1 << 3,
    ACS_REG_F_YAML_INCONSISTENT = 1 << 4,
    ACS_REG_F_UNREGISTERED = 1 << 5     /* 仅出现在未登记 DLL finding 中 */
};

/* ═══════════════ registry 级 finding(检测发现; registry 句柄所有) ═══════════════ */
typedef struct acs_registry_finding_v1 {
    acs_head    head;
    uint32_t    kind;              /* ACS_REG_EC_* (7/8/9/10/11/12/14) */
    int32_t     entry_index;       /* 关联 entry; -1 = registry 级 */
    acs_str_v1  detail;            /* 静态字面量(无路径/内容) + 定位提示 */
    uint32_t    aux_index;         /* 第二个关联 entry(-1 无); 重复检测用 */
} acs_registry_finding_v1;

/* ═══════════════ options ═══════════════ */
typedef struct acs_registry_options_v1 {
    acs_head    head;               /* sizeof/ACS_ABI_VERSION_V1 */
    acs_str_v1  manifest_abs_path;  /* product manifest 绝对路径(必填) */
    acs_str_v1  module_yaml_root;   /* 可空: 源 module.yaml 根目录; 非空时按
                                       rel_path 逐级找 <dir>/module.yaml(三方比对) */
    acs_str_v1  allowed_root;       /* 可空: 全部 abs 解析结果必须在该根(防 escape) */
    const acs_allocator_v1* allocator; /* 必填: 句柄/输出字符串内存归属 */
    const acs_logger_v1*    logger;    /* 可空(本版未使用; 保留日志纪律位) */
    int32_t     scan_unregistered;  /* 1 = 扫描 manifest 目录模块子目录找未登记 DLL */
    uint32_t    reserved;           /* v1 = 0 */
} acs_registry_options_v1;

typedef struct acs_registry_s acs_registry;

/* ───────── 打开 registry ─────────
 * 流程:
 *   1. 校验 options/allocator/head; manifest 路径绝对;
 *   2. 读 manifest(文件) + 最小 JSON 提取 units[] → 语义/schema 检测
 *      (kind 枚举、module 缺 module_id、sha256 非 64hex 或空、重复 unit_id/
 *      重复 module_id 记为 finding 不中止);
 *   3. 逐 module/provider unit: rel_path→abs(canonical 校验) + 现场 sha256 +
 *      module.yaml(若 yaml_root 给)字段提取;
 *   4. 经 ABI-003 loader 加载 module unit(query+describe): module_id/version/
 *      build_id/hash 三方比对, 不一致记为 entry finding(加载失败也记 finding);
 *   5. 可选扫描未登记 DLL。
 * 硬失败(返回非 0, *out=NULL): 参数/路径/manifest 读或 JSON/schema 结构性错误。
 * 检测发现不中止: open 成功, findings 由 check/list 暴露。
 * reentrant=yes; threadsafe=no(同句柄构建期独占)。err 可空; out 必填。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_open_v1(const acs_registry_options_v1* opt,
                     acs_error_info_v1* err,
                     acs_registry** out);

/* ───────── 全量校验(list 的判据; 机器可读) ─────────
 * 返回 findings 总数(0=clean); *out_issue_count 同值。registry 级 finding
 * (重复 ID/未登记)与 entry 级(finding_mask)均计数。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_check_v1(acs_registry* r,
                      acs_error_info_v1* err,
                      uint32_t* out_issue_count);

/* ───────── 读取 finding / entry(list/verify 输出实际值) ───────── */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_get_finding_v1(acs_registry* r, uint32_t index,
                            acs_registry_finding_v1* out);
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_entry_count_v1(acs_registry* r, uint32_t* out_count);
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_get_entry_v1(acs_registry* r, uint32_t index,
                          acs_registry_entry_v1* out);

/* ───────── 关闭(唯一释放入口) ─────────
 * 释放句柄与全部 registry 分配字符串(entry 字段失效)。r=NULL 为空操作。
 * threadsafe=no(同句柄); close 后不得再访问。 */
ASTROCS_EXPORT void ASTROCS_CALL
acs_registry_close_v1(acs_registry* r);

/* ───────── 平台自检(无文件) ─────────
 * 校验布局静态断言 + 内部最小 JSON 提取器对已知向量行为正确。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
acs_registry_self_test_v1(void);

/* 布局静态断言 */
ACS_STATIC_ASSERT(sizeof(acs_registry_entry_v1) > sizeof(acs_head), "entry payload");
ACS_STATIC_ASSERT(sizeof(acs_registry_finding_v1) > sizeof(acs_head), "finding payload");
ACS_STATIC_ASSERT(offsetof(acs_registry_options_v1, head) == 0u, "head first");
ACS_STATIC_ASSERT(offsetof(acs_registry_entry_v1, head) == 0u, "head first");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_REGISTRY_MODULE_REGISTRY_H */
