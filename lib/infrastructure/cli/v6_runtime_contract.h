// lib/infrastructure/cli/v6_runtime_contract.h — RUNTIME-CI-001 (Wave 9) 运行面单一事实源
//
// 本头文件是 V6 运行面（CLI 进程）的**唯一**契约实现点，覆盖：
//   * 宪章 §10.4 统一线程预算：一个进程只有一个资源调度器与线程预算源；
//     模块不得硬编码 workers / ISA / block，不得建立不受 Runtime 管理的私有长期线程池；
//     模块按 work unit 申请线程租约，Runtime 防止嵌套并行和超额订阅。
//   * 宪章 §3.1/§3.2 CLI 薄入口与三 Phase 相互隔离：不得把三个 Phase 隐式串接。
//   * 冻结节点 FZ-WEIGHT-SINGLE-PATH：权重只有一个口径 —— Phase1 产稀疏 SNR 控制点
//     → Phase2 重建稠密 SNR 面 → 取逆方差（最优功率）定权 → 叠加。没有可选择项 ⇒
//     phase2 的 --mode <token> 一律 fail-closed 拒绝（不存在任何合法 token）；
//     FZ-MODE-RETIRED psfsw_robust 不是现行对象（ASTROCS_DESIGN.md §3.1；
//     docs/design/UNIFIED_MODEL.md:58），拒绝消息带迁移提示；
//     FZ-FIELD-WEIGHTMODE legacy 整数 weight_mode 与 auto / support_x_snr2 / equal /
//     pixel_ivar **全部**拒绝（§9.73 裁决 A44 后无任何合法取值，含原 1|2 → baseline
//     的非生产放行面）。
//   * FZ-P3-MODES {surface_brightness, point_source_flux, visualization}。
//   * 宪章 §10.5 / §17.6 重计算利用率门禁的**字段面**，以及 SO-05 签字前的
//     fail-closed 记录/裁决分离策略（CONTROLLER_LOG C-007；04_OPEN_ITEMS_AND_SIGNOFF SO-05）。
//
// 纪律：
//   * 纯头文件、无科学公式、无第三方依赖（只用 std）；不进入公共 ABI。
//   * 数值阈值不在此发明：门限常量权威在 lib/infrastructure/cli/resource_gate.h（§18.2 冻结 85%/60%），
//     本头文件只承载**字段完备性**与 SO-05 处置策略。
//   * 本文件不提供任何"自动判决升级为硬失败"的入口：SO-05 未签字前资源判据恒为
//     record_only + 显式 PENDING_OWNER_SIGNOFF 标记（见 evaluate_heavy_run）。
//   * 历史复现开关 --strict-resource-gate 仍在 lib/infrastructure/cli/commands.cpp（旧测试断言 rc=10 用），
//     属测试面而非本契约的生产面；本契约的生产策略恒为 record_only。
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace astrocs {
namespace v6runtime {

// =====================================================================
// §3.2 三 Phase 相互隔离（独立产品命令/独立配置/独立进程/独立 Runtime）
// =====================================================================
enum class Phase { kP1 = 1, kP2 = 2, kP3 = 3 };

inline const char* phase_name(Phase p) {
    switch (p) {
    case Phase::kP1: return "phase1";
    case Phase::kP2: return "phase2";
    case Phase::kP3: return "phase3";
    }
    return "phase?";
}

// 隐式串接禁止：任何 CLI 命令 token 序列若同时需要 Phase2 与 Phase3 的运行语义，
// 或出现聚合式 run/graph/pipeline 入口，即视为违反 §3.2。本函数是纯判定，
// 供 CLI 与独立 Oracle 共同引用（单一事实源）。
inline bool is_implicit_phase_chain(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return false;
    // 顶层聚合入口（run/graph/pipeline 作为命令首词）属 §3.2 禁止的隐式串接。
    // 注意: "phase2 run" / "phase3 plan" 中的 run/plan 是**子命令**而非顶层入口，
    // 只有命令首词才判聚合。
    const std::string& head = tokens.front();
    if (head == "run" || head == "graph" || head == "pipeline" || head == "all") return true;
    bool p1 = false, p2 = false, p3 = false;
    for (const auto& t : tokens) {
        if (t == "phase1" || t.rfind("phase1 ", 0) == 0) p1 = true;
        if (t == "phase2" || t.rfind("phase2 ", 0) == 0) p2 = true;
        if (t == "phase3" || t.rfind("phase3 ", 0) == 0) p3 = true;
    }
    const int n = (p1 ? 1 : 0) + (p2 ? 1 : 0) + (p3 ? 1 : 0);
    return n > 1;
}

// =====================================================================
// CLI 模式路由（单一事实源；CLI 与 Oracle 均引用本处判定）
// =====================================================================
enum class RouteKind { kProduction = 0, kBaseline = 1, kReject = 2 };

inline const char* route_kind_name(RouteKind k) {
    switch (k) {
    case RouteKind::kProduction: return "production";
    case RouteKind::kBaseline:   return "baseline";
    case RouteKind::kReject:     return "reject";
    }
    return "reject";
}

struct ModeRoute {
    RouteKind kind = RouteKind::kReject;
    std::string token;    // canonical token（production/baseline 时为规范名）
    std::string surface;  // "phase2_weight" | "phase3_export" | "phase2_legacy_int"
    std::string reason;   // reject 时给出冻结节点 id + 说明
    int rc = 2;           // reject 时 CLI 退出码（astrocs::ARGS = 2）
};

// Phase2 权重口径选择面（FZ-WEIGHT-SINGLE-PATH）。
//   合法 token：**无** —— 权重只有一个口径，没有可选择项。
//   全部 token 一律 fail-closed 拒绝（rc=2），**无例外**：
//     psfsw_robust(RETIRED) | psf_snr_power(DEFERRED) | auto | support_x_snr2 |
//     equal | pixel_ivar | "0" | "1" | "2" | 任何其它串。
// 说明（§9.73 裁决 A44 后订正）：字符串 "1"/"2" 是 legacy 整数的字符串形态，
// 一律直接拒绝，避免"数字字符串冒充权重口径"。
// 原实现把 equal / pixel_ivar 放行为 kBaseline（rc=0 + 告警），其唯一理由是
// 「它们是 route_legacy_weight_mode_int 的映射目标登记」；A44 删除该整数路由后，
// 该理由消失 ⇒ 二者与其余 token 同归 fail-closed（本注释首句本来就是这条口径，
// 旧实现与自身冻结说明不一致）。
// 依据：ASTROCS_DESIGN.md §3.1（权重只能来自纯净信号/噪声之比的逆方差，跨帧绝对
// 标定，不基于参考帧；全程只有 SNR）+ docs/science/PSF_SIGNAL_WEIGHT.md §4（单一
// 权重口径，无模式选择）。psfsw_robust_weight 不是现行对象
// （docs/design/UNIFIED_MODEL.md:58）⇒ 显式拒绝 + 迁移提示，不得静默接受。
inline ModeRoute route_phase2_weight_token(const std::string& raw) {
    ModeRoute r;
    r.surface = "phase2_weight";
    r.token = raw;
    if (raw == "psfsw_robust") {
        r.reason = "FZ-MODE-RETIRED: 'psfsw_robust' rejected - psfsw_robust_weight is "
                   "not a current object (ASTROCS_DESIGN.md 3.1; UNIFIED_MODEL.md:58); "
                   "migration: Phase2 reconstructs the dense SNR field and derives "
                   "inverse-variance weights w = SNR^2/F_ref^2";
        return r;
    }
    if (raw == "psf_snr_power") {
        r.reason = "FZ-MODE-DEFERRED: psf_snr_power is DEFERRED and must not enter the "
                   "production route (C-004.1)";
        return r;
    }
    if (raw == "auto" || raw == "support_x_snr2") {
        r.reason = "FZ-FIELD-WEIGHTMODE: legacy/forbidden weight token";
        return r;
    }
    if (raw == "0" || raw == "1" || raw == "2") {
        r.reason = "FZ-FIELD-WEIGHTMODE: legacy integer weight_mode must be selected via "
                   "the integer field, not a mode token";
        return r;
    }
    if (raw == "equal" || raw == "pixel_ivar") {
        // §9.73 裁决 A44：原实现把这两个 token 放行为 kBaseline（rc=0），理由是
        // 「legacy 整数映射的目标登记」。整数路由已删除 ⇒ 该理由消失；且它们是
        // **输入路径**（CLI --mode）上的口径 token ⇒ 与其余 token 同归 fail-closed
        // （ASTROCS_DESIGN.md §3.1:175「没有可选择项」；PSF_SIGNAL_WEIGHT.md §4:72
        // 「不存在口径选择键、口径枚举、口径配置项或口径产物」）。
        r.reason = "FZ-WEIGHT-SINGLE-PATH: '" + raw + "' rejected - there is no "
                   "selectable weight mode; Phase2 reconstructs the dense SNR field "
                   "and derives inverse-variance weights w = SNR^2/F_ref^2 = 1/sigma_F^2";
        return r;
    }
    r.reason = "FZ-WEIGHT-SINGLE-PATH: '" + raw + "' rejected - there is no selectable "
               "weight mode; Phase2 reconstructs the dense SNR field and derives "
               "inverse-variance weights w = SNR^2/F_ref^2 = 1/sigma_F^2";
    return r;
}

// Phase3 输出模式（FZ-P3-MODES）。
inline ModeRoute route_phase3_mode(const std::string& raw) {
    ModeRoute r;
    r.surface = "phase3_export";
    r.token = raw;
    if (raw == "surface_brightness" || raw == "point_source_flux" || raw == "visualization") {
        r.kind = RouteKind::kProduction;
        r.rc = 0;
        return r;
    }
    if (raw == "psf_snr_power" || raw == "auto" || raw == "support_x_snr2") {
        r.reason = "FZ-P3-MODES: not a Phase3 output mode (deferred/legacy token)";
        return r;
    }
    r.reason = "FZ-P3-MODES: unknown export mode (allowed: surface_brightness | "
               "point_source_flux | visualization)";
    return r;
}

// legacy 整数 weight_mode 字段：**纯拒绝面**（§9.73 裁决 A44）。
// 原实现把 1|2 映射为 kBaseline（rc=0，非生产但放行）、仅 0 拒绝；A44 删除 legacy
// 整数权重模式域后，该字段不存在任何合法取值 ⇒ 全值域 fail-closed。
// 保留函数名与具名拒绝面（本仓既有惯例：退役对象的拒绝面必须存活、收紧不是删除；
// 见 eng/ci/check_no_weight_mode_code.py C2「退役对象的拒绝面必须存活」与
// is_retired_weight_mode_token）。最危险值 0（support x SNR^2，无量纲、非信号/噪声
// 之比）保留**更具体**的拒绝理由，便于用户按提示改对。
// 依据：ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、没有可选择项」；
// docs/science/PSF_SIGNAL_WEIGHT.md §4:72「不存在口径选择键、口径枚举、口径配置项
// 或口径产物」；docs/contracts/DATA_SEMANTICS.md §31.3（legacy weight_mode=0 → REJECT）。
inline ModeRoute route_legacy_weight_mode_int(int v) {
    ModeRoute r;
    r.surface = "phase2_legacy_int";
    r.token = std::to_string(v);
    // 全值域 fail-closed：最危险值 0（support x SNR^2）保留更具体的拒绝理由。
    // 该分支文本是 V6 变异驱动（eng/tools/v6/v6_runtime_mutation_driver.py
    // "legacy-weight-mode-0-allowed"）的注入锚点，**逐字保持**。
    if (v == 0) {
        r.reason = "FZ-FIELD-WEIGHTMODE: legacy weight_mode 0 (support x SNR^2) is not a "
                   "science variance surface and is rejected (fail-closed)";
    } else {
        r.reason = "FZ-FIELD-WEIGHTMODE: unsupported legacy weight_mode integer";
    }
    return r;
}

inline bool is_deferred_weight_token(const std::string& t) {
    return t == "psf_snr_power";
}

// =====================================================================
// §10.4 统一线程预算：一个进程只有一个资源调度器与线程预算源
// =====================================================================
inline constexpr const char* kOneBudgetSourceRule =
    "one_process_one_resource_scheduler_and_thread_budget_source";

// 进程级预算登记处。第二方注册 = 违反 §10.4（返回 false，不静默覆盖）。
// 线程租约：Σ 已租出 <= allocated；模块申请超出租约时被拒（防嵌套并行/超额订阅）。
class ProcessBudgetRegistry {
public:
    static ProcessBudgetRegistry& instance() {
        static ProcessBudgetRegistry r;   // 进程唯一实例（§10.4 单一来源）
        return r;
    }

    bool register_source(const std::string& owner, std::uint32_t cores) {
        std::lock_guard<std::mutex> lk(mu_);
        if (has_source_) return false;              // 第二来源：拒绝
        if (cores == 0) return false;               // 零预算非法（不得硬编码/退化）
        has_source_ = true;
        owner_ = owner;
        cores_ = cores;
        leased_total_ = 0;
        return true;
    }
    bool has_source() const {
        std::lock_guard<std::mutex> lk(mu_);
        return has_source_;
    }
    std::string owner() const {
        std::lock_guard<std::mutex> lk(mu_);
        return owner_;
    }
    std::uint32_t allocated_cores() const {
        std::lock_guard<std::mutex> lk(mu_);
        return cores_;
    }
    std::uint32_t leased_total() const {
        std::lock_guard<std::mutex> lk(mu_);
        return leased_total_;
    }
    int nested_parallel_denied() const {
        std::lock_guard<std::mutex> lk(mu_);
        return denied_;
    }
    // 模块按 work unit 申请线程租约。want==0 视为要求全部剩余；超出租约 → 拒绝（err 非空）。
    std::uint32_t request_lease(const std::string& /*requester*/, std::uint32_t want,
                               std::string* err) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!has_source_) {
            if (err) *err = "no budget source registered (violates §10.4)";
            return 0;
        }
        const std::uint32_t avail = cores_ > leased_total_ ? cores_ - leased_total_ : 0;
        const std::uint32_t ask = (want == 0) ? avail : want;
        if (ask == 0) {
            if (err) *err = "empty lease request";
            return 0;
        }
        if (ask > avail) {
            ++denied_;                              // 超额订阅/嵌套并行：拒绝
            if (err) *err = "lease exceeds available budget (nested parallel / oversubscribe)";
            return 0;
        }
        leased_total_ += ask;
        return ask;
    }
    void release_lease(std::uint32_t n) {
        std::lock_guard<std::mutex> lk(mu_);
        leased_total_ = (n >= leased_total_) ? 0 : leased_total_ - n;
    }
    void reset_for_test() {
        std::lock_guard<std::mutex> lk(mu_);
        has_source_ = false;
        owner_.clear();
        cores_ = 0;
        leased_total_ = 0;
        denied_ = 0;
    }

private:
    ProcessBudgetRegistry() = default;
    mutable std::mutex mu_;
    bool has_source_ = false;
    std::string owner_;
    std::uint32_t cores_ = 0;
    std::uint32_t leased_total_ = 0;
    int denied_ = 0;
};

// =====================================================================
// §10.5 重计算必采字段面（字段完备性判定；阈值权威在 resource_gate.h）
// =====================================================================
struct HeavyRunMetrics {
    std::string run_id;
    std::string phase;
    std::string mode;
    // 进程 CPU
    double process_cpu_seconds = 0.0;
    double avg_equivalent_cores = 0.0;
    double cpu_p50_percent = -1.0;
    double cpu_mean_percent = -1.0;
    // 每线程 CPU
    std::uint32_t threads = 0;
    std::uint32_t active_compute_threads = 0;
    double per_thread_cpu_max_seconds = 0.0;
    double per_thread_cpu_sum_seconds = 0.0;
    // 内存
    std::uint64_t rss_bytes = 0;
    std::uint64_t pss_bytes = 0;
    double rss_growth_mb_per_s = 0.0;
    // 读写 + I/O wait
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
    double io_wait_percent = 0.0;
    // 调度
    std::uint64_t work_units = 0;
    std::uint64_t queue_depth = 0;
    double worker_balance = 0.0;   // active / (active + runnable)，0..1
    // 时钟与预算
    double wall_seconds = 0.0;
    double active_window_seconds = 0.0;
    std::uint32_t allocated_cores = 0;
};

// 规格要求的字段名集合（§10.5 逐项；供 CLI 事件/Oracle/CI 共同引用，禁各自发明）。
inline std::vector<std::string> required_metric_keys() {
    return {
        "process_cpu_seconds", "per_thread_cpu_max_seconds", "per_thread_cpu_sum_seconds",
        "rss_bytes", "pss_bytes", "rss_growth_mb_per_s", "read_bytes", "write_bytes",
        "io_wait_percent", "work_units", "queue_depth", "worker_balance",
        "wall_seconds", "active_window_seconds",
    };
}

// 字段完备性（缺失即不完整；不在此判定阈值）。missing 承接逐项缺失名。
inline bool heavy_metrics_complete(const HeavyRunMetrics& m, std::string* missing) {
    std::string miss;
    auto need = [&](bool cond, const char* name) {
        if (!cond) { if (!miss.empty()) miss += ","; miss += name; }
    };
    need(m.allocated_cores >= 1, "allocated_cores");
    need(m.threads >= 1, "threads");
    need(m.active_compute_threads >= 1, "active_compute_threads");
    need(m.per_thread_cpu_sum_seconds > 0.0, "per_thread_cpu_sum_seconds");
    need(m.per_thread_cpu_max_seconds > 0.0, "per_thread_cpu_max_seconds");
    need(m.wall_seconds > 0.0, "wall_seconds");
    need(m.active_window_seconds >= 0.0, "active_window_seconds");
    need(!m.phase.empty(), "phase");
    need(!m.mode.empty(), "mode");
    if (missing) *missing = miss;
    return miss.empty();
}

// =====================================================================
// SO-05：签字前的记录/裁决分离（fail-closed）
// =====================================================================
inline constexpr const char* kSo05Id = "SO-05";
inline constexpr const char* kSo05Status = "PENDING_OWNER_SIGNOFF";
inline constexpr const char* kRecordOnlyPolicy = "record_only";
inline constexpr const char* kNoAutoAdjudication =
    "auto_adjudication_withheld_pending_owner_signoff";

struct GatePolicy {
    bool record_only = true;
    bool auto_adjudication_allowed = false;
    bool hard_fail_on_resource_verdict = false;
    std::string signoff_id = kSo05Id;
    std::string signoff_status = kSo05Status;
};

inline GatePolicy so05_gate_policy() { return GatePolicy{}; }

struct GateFinding {
    std::string kind;       // 与 resource_gate.h GateDiag 名同口径（由调用方传入）
    std::string detail;
    bool would_fail_if_signed = false;
};

struct GateVerdict {
    std::string status;     // 生产面恒为 record_only_pending_owner_signoff
    std::string signoff_id = kSo05Id;
    std::string signoff_status = kSo05Status;
    bool hard_fail = false; // SO-05 未签字前恒为 false（不得擅自升级）
    std::vector<GateFinding> findings;
};

// 对一次 heavy run 给出**记录面**裁决：阈值越线只登记 finding，绝不改退出码。
// 阈值越线的判定表达式由调用方（lib/infrastructure/cli/resource_gate.h）给出，这里只做策略收口。
inline GateVerdict evaluate_heavy_run(const std::vector<GateFinding>& findings) {
    GateVerdict v;
    v.status = "record_only_pending_owner_signoff";
    v.hard_fail = false;
    v.findings = findings;
    return v;
}

// =====================================================================
// 确定性契约（FZ-RUNTIME-DETERMINISM）：同一输入在不同 worker 数/分块/调度下
// 产物逐字节一致。键用于证据留痕，判定由 eng/tests/system/v6_runtime 承担。
// =====================================================================
inline constexpr const char* kDeterminismContractId = "FZ-RUNTIME-DETERMINISM";
inline constexpr const char* kDeterminismRule =
    "byte_identical_product_across_workers_chunking_and_scheduling";

}  // namespace v6runtime
}  // namespace astrocs
