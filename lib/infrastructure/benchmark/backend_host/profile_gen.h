// lib/infrastructure/benchmark/backend_host/profile_gen.h — cpu_profile.json 生成与复读 (BENCH-004/005 + CPU-003)
// API: generate_profile_json(旧 schema, 保留供旧测试/后端测试)
// CPU-003: generate_profile_v2 / verify_profile_v2(新 v2 schema)
#ifndef ACSD_PROFILE_GEN_H
#define ACSD_PROFILE_GEN_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace acsd::backend_host {

/* ── V5 API(旧 schema; BENCH-004/005 测试与旧 CLI 兼容) ── */
/* 生成对 cpu_profile.schema.json 有效的 profile JSON 文本。
 * mode: "quick"|"full"; build_id: X.Y.Z-alpha.N+g<hash12>; commit: 40hex;
 * backend_sha: 主 backend 文件 hash(内置 baseline=可执行文件 hash)。 */
std::string generate_profile_json(const std::string& mode, const std::string& build_id,
                                  const std::string& commit, const std::string& backend_sha);

/* ── V6.1 CPU-003 API(v2 schema; schemas/cpu_profile.schema.json) ── */

// 单 kernel 单候选的原始测量(全量保存, 供复读与)
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

/* ── R-53: oracle 失败的性质证据(唯一判据源; 门必须能区分环境性与代码性 oracle:fail) ──
 * 动机: profile 只落 correctness_test="oracle:fail" 时, 读侧无法区分
 *   ① 环境性 = 没有任何候选能进入内核执行(provider 缺失/self_test/加载/ISA 预检剔除)
 *      ⇒ 本机/本安装树确实测不了; 与
 *   ② 代码性 = 候选进入了内核执行且数值不符(判据/内核/provider 缺陷)
 *      ⇒ 恒红门/真缺陷, 必须判红 —— 两档恒 oracle:fail 的真根因即 ②。
 * 判据(见 oracle_fail_class): executed==0 ∧ culled>0 ⇒ 环境性;
 *   executed>0 ∧ (首次不符 ∨ 非数值执行失败) ⇒ 代码性; 其余(含空证据) ⇒ 不可判定。
 * **不可判定必须判红**: 证据缺失不得按环境性放行(否则就是新的开口子)。 */
struct OracleFirstMismatch {
    uint32_t index = 0;              // 逐元素比较中**首次**不符的索引(bench_kernel 报的第一个)
    double got = 0, ref = 0;         // kernel 实测值 / 独立参考值
    std::string size_class;          // "small"|"medium"|"large"(所属规模档)
    std::string provider;            // 该候选 provider
    uint32_t workers = 0;
    uint64_t block = 0;
};

struct OracleFailEvidence {
    // 证据块是否存在(与"空证据"区分): 复读侧按 JSON 有无 oracle_fail 置位;
    // 生成侧仅当 class 可判定时才置位 ⇒ "有失败却无证据"必然被判红。
    bool present = false;
    uint32_t executed_candidates = 0;   // 真调用过内核的候选数(provider×workers×block 计)
    uint32_t culled_candidates = 0;     // 执行前被剔除的候选槽位数(同上单位; 含不可用 provider 整槽)
    // 其中因"provider 可用但未注册该 kernel"被剔除的槽位数: 这是 provider/kernel 实现面
    // 缺陷, 不是环境能力问题 ⇒ 单列, 只它一个原因时判代码性(不得按环境性放行)。
    uint32_t missing_kernel_candidates = 0;
    std::vector<std::string> culled_detail;  // 被剔除原因(provider: reason), 环境性的正面证据
    double tolerance = 0;               // 判据容差(冻结值 kOracleRelTol=2e-4)
    bool has_first_mismatch = false;
    OracleFirstMismatch first_mismatch;
    std::string detail;                 // 执行了但非数值失败时的原始原因(如 "kernel rc=-1")
};

enum class OracleFailClass { kUndetermined = 0, kEnvironmental, kCode };

OracleFailClass oracle_fail_class(const OracleFailEvidence& ev);
const char* oracle_fail_class_name(OracleFailClass c);            // undetermined|environmental|code
const char* oracle_fail_kind_name(const OracleFailEvidence& ev);  // 之上再分 no_candidate_executed|numeric_mismatch|kernel_error

/* 证据充分性判据(唯一出处; 返回 "" = 合规)。correctness_test 非 "oracle:pass" 时,
 * 证据不可判定 ⇒ 判红(证据缺失不得按环境性放行)。oracle:pass 携带证据是允许的
 * (某规模档失败而末档通过), 但同样必须可判定。 */
std::string oracle_fail_evidence_violation(const std::string& kernel_id,
                                           const std::string& correctness_test,
                                           const OracleFailEvidence& ev);

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
    OracleFailEvidence evidence;   // R-53: 跨规模档聚合的 oracle 失败证据(不可判定 ⇒ 判红)
};

struct ProfileBundle {
    std::string json;                  // 完整 v2 profile JSON 文本
    std::vector<RawCandidate> raw;     // 全部原始候选
    std::map<std::string, KernelProfile> kernels;  // kernel_id → 选择
    std::string raw_samples_sha256;    // 原始候选序列化 hash
    std::string profile_id;            // "sha256:<hex>"
    // 组装期不变量违反项(空=全部合规)。非空 ⇒ 该 profile 结构性不可写盘:
    // 调用方须 fail-closed(CLI 返回 acsd::CRASH), 不得落盘再等复读层拒收。
    std::vector<std::string> violations;
};

/* 组装期不变量(唯一出处, 与 verify_profile_v2 同一条判据; 返回 "" = 合规):
 * correctness_test == "oracle:pass" ⇒ median_ns > 0 且 mad_ns >= 0。
 * 依据: docs/ACSD_DESIGN.md §9「选择用稳定统计」+ eng/tests/cli/test_bench_cli.py:94-95
 * (「过 oracle 却零耗时」是结构性自相矛盾: 唯一物理含义是统计量根本没测到)。
 * 本判据可独立调用 ⇒ 负例注入无需真实测量环境。 */
std::string profile_invariant_violation(const KernelProfile& kp);

/* Oracle 内省/测试入口(与 profile 生成路径**同一实现**, 无第二份公式):
 * 返回逐元素 f64 参考。op: ACS_KOP_*; frames: 栈类帧数(非栈=1); k: op 标量;
 * w: 网格宽; N: 元素数; in0..in3: 与生成路径同序的输入。
 * 用途 = 把 oracle 的**离散语义**(网格点坐标 x=i%w, y=i/w 取整)钉进可独立复跑的
 * 正负例: 该语义与 kernel(baseline_kernels_impl.inc)逐元素同源, 任何偏离都是
 * "判据与冻结公式不同源" ⇒ 恒红门(候选结构性不可能通过), 必须在单元面判红。 */
std::vector<double> oracle_ref_v1(int op, uint32_t frames, float k, uint32_t w, uint32_t N,
                                  const std::vector<float>& in0,
                                  const std::vector<float>& in1,
                                  const std::vector<float>& in2,
                                  const std::vector<float>& in3);

/* 生成 v2 profile。mode: "quick"(1 代表 kernel medium) | "full"(12 kernel × 3 规模)。
 * build_id: "X.Y.Z[-pre]+g<hash12>"(纯 base 版本由调用方派生, 单源 VER-001); commit: 40hex; cli_sha256: 运行二进制实测。
 * backends_dir: 含 backends.manifest.json 与 provider DSO 的目录(空=仅内置 baseline)。
 * reentrant=yes; threadsafe=no(串行测量)。 */
ProfileBundle generate_profile_v2(const std::string& mode, const std::string& build_id,
                                  const std::string& commit,
                                  const std::string& cli_sha256,
                                  const std::string& backends_dir);

/* 独立复读: 解析并校验 v2 profile 文本。返回 "" 表示合法, 否则返回错误描述。
 * 校验: schema/必填字段/版本格式/commit/指纹/workers/block/median 合理性。
 * acsd_version 仅做 semver 形态校验(N3, 不钉死版本字面量); build 绑定语义由
 * expected_commit(source_commit)与 benchmark_binary_sha256 等指纹承担。
 * expected_commit 非空时须匹配 build.source_commit。 */
std::string verify_profile_v2(const std::string& json_text,
                              const std::string& expected_commit);

}  // namespace acsd::backend_host

#endif  // ACSD_PROFILE_GEN_H
