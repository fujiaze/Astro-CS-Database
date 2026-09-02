/* AstroCS Artifact C ABI v1 (DATA-001 冻结) — include/astrocs/contracts/artifact_abi_v1.h
 *
 * 角色: 跨 DLL/进程边界的 Artifact 句柄/manifest 查询 C ABI (13_DATA_PIPELINE_AND_ARTIFACT_STANDARD §1:
 * "Pipeline edge 传递 ArtifactHandle，不是路径字符串")。
 *
 * 冻结合同 (v1 不可变; 扩展须升版本):
 *   1) 句柄 = opaque; 生命周期仅经 parse/destroy 对; 内部实现不可见。
 *   2) 句柄不暴露任何文件系统路径字符串: storage 位置仅以 storage_uri (URI 形态, 含 run
 *      相对 scheme) + content_digest 呈现; 本头无 path/目录字段; 解析 storage_uri 为真实
 *      文件系统路径只允许发生在 Store 实现内部 (DATA-003 接线)。
 *   3) 字符串字段返回值所有权 = 句柄, 有效至 destroy; 调用方不得 free。
 *   4) 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常/RTTI 跨边界。
 *   5) manifest 结构合同: contracts/data/artifact_manifest.schema.json;
 *      type 注册表: contracts/data/artifact_types.registry.json (未知 type 由 schema 层拒绝)。
 *
 * 错误码语义复用 astrocs/common_abi_v1.h 的 acs_status。
 */
#ifndef ASTROCS_CONTRACTS_ARTIFACT_ABI_V1_H
#define ASTROCS_CONTRACTS_ARTIFACT_ABI_V1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_ARTIFACT_ABI_VERSION_V1 1u

/* manifest 状态枚举 (与 artifact_manifest.schema.json status enum 一一对应) */
typedef enum acs_artifact_status {
    ACS_ART_STATUS_COMPLETE = 0,
    ACS_ART_STATUS_INCOMPLETE = 1,
    ACS_ART_STATUS_FAILED = 2,
    ACS_ART_STATUS_CANCELLED = 3,
    ACS_ART_STATUS_PENDING = 4,
    ACS_ART_STATUS_COUNT = 5 /* 哨兵; 非法值一律 == 该枚举域之外 → 拒绝 */
} acs_artifact_status;

/* opaque artifact handle: 只经 acs_artifact_manifest_parse_v1/destroy_v1 管理 */
typedef struct acs_artifact_handle_v1_s acs_artifact_handle_v1;

/* ── 生命周期 ──
 * 解析 manifest JSON (UTF-8) 并做结构校验; 成功创建句柄。
 * 结构校验覆盖: 必填字段齐全(缺字段拒绝)、status/枚举合法、digest hex=64、
 * size 非负、input_digests 内 artifact_id 唯一(重复输入拒绝)。
 * 顶层重复 key (含重复 producer 对象) 由严格 JSON 解析层
 * (runtime/artifact_store/artifact_manifest_validator.py) 拒绝; C 层顺序取首个。
 * type_id↔注册表对照属 schema 层 (python validator / DATA-003 Store), 不在此 C 层硬编码。
 * 失败返回非 0 且 *out 置 NULL; err/err_cap 可为 NULL。
 * 并发: reentrant=yes (无全局状态); 线程安全: 同一句柄不同时调用 (句柄非共享); 所有权:
 * json 调用方所有, 解析期间只读; 句柄所有权转移给调用方, 必须 destroy。
 */
int acs_artifact_manifest_parse_v1(const char* json_utf8, size_t json_bytes,
                                   acs_artifact_handle_v1** out,
                                   char* err, size_t err_cap);

/* 释放句柄及其内部所有字符串。h 为 NULL 时为空操作。 */
void acs_artifact_handle_destroy_v1(acs_artifact_handle_v1* h);

/* ── 查询器 (13 标准 §1: handle 必须可向 Store 查询) ──
 * 全部查询函数: 不失败(句柄已校验); 返回值所有权=句柄, 有效至 destroy。
 * 字符串字段类型均为 const char* NUL 结尾; 无任何字段返回文件系统路径。
 */
const char* acs_artifact_query_manifest_schema_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_artifact_id_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_type_id_v1(const acs_artifact_handle_v1* h);
uint32_t    acs_artifact_query_schema_version_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_storage_uri_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_content_digest_hex_v1(const acs_artifact_handle_v1* h);
uint64_t    acs_artifact_query_size_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_producer_module_id_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_producer_build_id_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_run_id_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_phase_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_node_id_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_config_digest_hex_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_created_utc_v1(const acs_artifact_handle_v1* h);
acs_artifact_status acs_artifact_query_status_v1(const acs_artifact_handle_v1* h);
size_t      acs_artifact_query_input_count_v1(const acs_artifact_handle_v1* h);
const char* acs_artifact_query_input_artifact_id_v1(const acs_artifact_handle_v1* h, size_t i);
const char* acs_artifact_query_input_digest_v1(const acs_artifact_handle_v1* h, size_t i);

/* 状态枚举 → 冻结字符串名 (与 schema enum 一致); 非法值返回 NULL。 */
const char* acs_artifact_status_name_v1(acs_artifact_status s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CONTRACTS_ARTIFACT_ABI_V1_H */
