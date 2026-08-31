// lib/backend_host/profile_gen.h — cpu_profile.json 生成与复读 (BENCH-004/005 + CPU-003)
// V5 API: generate_profile_json(旧 schema, 保留供旧测试/后端测试)
// V6.1 CPU-003: generate_profile_v2 / verify_profile_v2(新 v2 schema)
#ifndef ASTROCS_PROFILE_GEN_H
#define ASTROCS_PROFILE_GEN_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace astrocs::backend_host {

/* ── V5 API(旧 schema; BENCH-004/005 测试与旧 CLI 兼容) ── */
/* 生成对 cpu_profile.schema.json 有效的 profile JSON 文本。
 * mode: "quick"|"full"; build_id: X.Y.Z-alpha.N+g<hash12>; commit: 40hex;
 * backend_sha: 主 backend 文件 hash(内置 baseline=可执行文件 hash)。 */
std::string generate_profile_json(const std::string& mode, const std::string& build_id,
                                  const std::string& commit, const std::string& backend_sha);

/* ── V6.1 CPU-003 API(v2 schema; schemas/cpu_profile.schema.json) ── */

// 单 kernel 单候选的原始测量(全量保存, 供复读与审计)
struct RawCandidate {
    std::string kernel_id;        // 如 "calibration-pixel-transform"
    std::string size_class;       // "small"|"medium"|"large"
    std::string provider;         // "baseline"|"avx2"|"avx512"
    uint32_t workers = 0;
    uint64_t block = 0;
    double median_ns = 0, mad_ns = 0, p05_ns = 0, p95_ns = 0;
    bool oracle_pass = false;     // 正确性筛选(独立 scalar Oracle)
    std::string fallback_reason;  // 空=通过; 非空=被剔除原因
};

// 单 kernel 的最终选择(profile kernels 对象项)
struct KernelProfile {
    std::string kernel_id;
    std::string workload_class;   // 由 kernel 语义决定(如 "drizzle-accumulate"→"memory")
    std::string provider;         // 最终选择
    uint32_t workers = 1;
    uint64_t block = 1;
    std::string correctness_test;  // "oracle:pass" | "oracle:fail" | "selftest:fail"
    std::string self_test_sha256;  // provider self_test 可验证 hash(64hex; 空=未运行)
    double median_ns = 0, mad_ns = 0;
    std::string fallback_reason;   // 空=正常; 非空=逐 kernel 回退原因
};

struct ProfileBundle {
    std::string json;                  // 完整 v2 profile JSON 文本
    std::vector<RawCandidate> raw;     // 全部原始候选
    std::map<std::string, KernelProfile> kernels;  // kernel_id → 选择
    std::string raw_samples_sha256;    // 原始候选序列化 hash
    std::string profile_id;            // "sha256:<hex>"
};

/* 生成 v2 profile。mode: "quick"(1 代表 kernel medium) | "full"(12 kernel × 3 规模)。
 * build_id: "0.10.0-alpha.2+g<hash12>"; commit: 40hex; cli_sha256: 运行二进制实测。
 * backends_dir: 含 backends.manifest.json 与 provider DSO 的目录(空=仅内置 baseline)。
 * reentrant=yes; threadsafe=no(串行测量)。 */
ProfileBundle generate_profile_v2(const std::string& mode, const std::string& build_id,
                                  const std::string& commit,
                                  const std::string& cli_sha256,
                                  const std::string& backends_dir);

/* 独立复读: 解析并校验 v2 profile 文本。返回 "" 表示合法, 否则返回错误描述。
 * 校验: schema/必填字段/版本/commit/指纹/workers/block/median 合理性。
 * expected_commit 非空时须匹配 build.source_commit。 */
std::string verify_profile_v2(const std::string& json_text,
                              const std::string& expected_commit);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_PROFILE_GEN_H
