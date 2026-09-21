// eng/tests/system/v6_runtime/v6_runtime_contract_test.cpp
// RUNTIME-CI-001 (Wave 9): 运行面契约正向/负向单元测试。
// 被测单一事实源 = lib/infrastructure/cli/v6_runtime_contract.h（统一预算 / 模式路由 / SO-05 策略 /
// 每线程字段面 / §3.2 Phase 隔离）。本测试只做结构性判定，不重跑科学实现。
//
// 用法: v6_runtime_contract_test <units|modes|budget|so05|isolation|metrics|negative>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "v6_runtime_contract.h"

using namespace astrocs::v6runtime;

static int g_fail = 0;
static int g_checks = 0;

static void CHECK(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_fail;
        std::fprintf(stderr, "  [FAIL] %s\n", what.c_str());
    } else {
        std::fprintf(stderr, "  [ok] %s\n", what.c_str());
    }
}

static void test_modes() {
    // FZ-MODE-PRODUCTION: 三模式 = production
    for (const char* t : {"point_information", "surface_gls", "psfsw_robust"}) {
        const ModeRoute r = route_phase2_mode(t);
        CHECK(r.kind == RouteKind::kProduction && r.rc == 0,
              std::string("phase2 production mode ") + t);
    }
    // FZ-MODE-DEFERRED: psf_snr_power 必拒
    const ModeRoute d = route_phase2_mode("psf_snr_power");
    CHECK(d.kind == RouteKind::kReject && d.rc == 2, "psf_snr_power rejected (DEFERRED)");
    CHECK(d.reason.find("FZ-MODE-DEFERRED") != std::string::npos,
          "psf_snr_power reject reason cites FZ-MODE-DEFERRED");
    // FZ-FIELD-WEIGHTMODE: auto / support_x_snr2 / 数字串 必拒
    for (const char* t : {"auto", "support_x_snr2", "0", "1", "2", "bogus", ""}) {
        CHECK(route_phase2_mode(t).kind == RouteKind::kReject,
              std::string("phase2 token rejected: ") + (t[0] ? t : "<empty>"));
    }
    // baseline 非生产
    for (const char* t : {"equal", "pixel_ivar"}) {
        CHECK(route_phase2_mode(t).kind == RouteKind::kBaseline,
              std::string("phase2 baseline: ") + t);
    }
    // legacy 整数: 0 必拒, 1|2 -> baseline
    CHECK(route_legacy_weight_mode_int(0).kind == RouteKind::kReject,
          "legacy weight_mode 0 rejected");
    CHECK(route_legacy_weight_mode_int(1).kind == RouteKind::kBaseline,
          "legacy weight_mode 1 -> baseline equal");
    CHECK(route_legacy_weight_mode_int(2).kind == RouteKind::kBaseline,
          "legacy weight_mode 2 -> baseline pixel_ivar");
    CHECK(route_legacy_weight_mode_int(7).kind == RouteKind::kReject,
          "legacy weight_mode unknown rejected");
    // FZ-P3-MODES
    for (const char* t : {"surface_brightness", "point_source_flux", "visualization"}) {
        CHECK(route_phase3_mode(t).kind == RouteKind::kProduction,
              std::string("phase3 production mode ") + t);
    }
    for (const char* t : {"psf_snr_power", "auto", "support_x_snr2", "nope"}) {
        CHECK(route_phase3_mode(t).kind == RouteKind::kReject,
              std::string("phase3 token rejected: ") + t);
    }
}

static void test_budget() {
    ProcessBudgetRegistry& R = ProcessBudgetRegistry::instance();
    R.reset_for_test();
    CHECK(R.register_source("runtime", 8), "first budget source accepted");
    CHECK(!R.register_source("rogue_private_pool", 4),
          "second budget source rejected (one process, one source)");
    CHECK(R.owner() == "runtime" && R.allocated_cores() == 8,
          "owner/allocated cores retained");
    std::string err;
    const uint32_t a = R.request_lease("module_a", 3, &err);
    CHECK(a == 3, "lease 3 granted");
    const uint32_t b = R.request_lease("module_b", 5, &err);
    CHECK(b == 5, "lease 5 granted (total 8)");
    const uint32_t c = R.request_lease("module_c", 1, &err);
    CHECK(c == 0 && !err.empty(), "oversubscription lease rejected");
    CHECK(R.nested_parallel_denied() == 1, "nested/oversubscribe denied counter = 1");
    R.release_lease(5);
    const uint32_t d = R.request_lease("module_d", 5, &err);
    CHECK(d == 5, "lease after release granted");
    R.reset_for_test();
    CHECK(!R.has_source(), "reset clears registry");
    CHECK(R.request_lease("m", 1, &err) == 0, "lease without source rejected");
}

static void test_so05() {
    const GatePolicy pol = so05_gate_policy();
    CHECK(pol.record_only, "SO-05 default policy is record_only");
    CHECK(!pol.auto_adjudication_allowed, "SO-05 auto adjudication withheld");
    CHECK(!pol.hard_fail_on_resource_verdict, "SO-05 no hard fail before signoff");
    CHECK(pol.signoff_id == kSo05Id, "signoff id SO-05");
    CHECK(pol.signoff_status == "PENDING_OWNER_SIGNOFF",
          "signoff status PENDING_OWNER_SIGNOFF");
    // 有 finding 也不得升级为硬失败（fail-closed 只记录）
    std::vector<GateFinding> f;
    GateFinding x;
    x.kind = "cpu_mean_low";
    x.detail = "cpu mean 40% < 85%";
    x.would_fail_if_signed = true;
    f.push_back(x);
    const GateVerdict v = evaluate_heavy_run(f);
    CHECK(!v.hard_fail, "SO-05 verdict never hard_fail before signoff");
    CHECK(v.status == "record_only_pending_owner_signoff", "SO-05 verdict status");
    CHECK(v.findings.size() == 1, "SO-05 findings recorded");
}

static void test_isolation() {
    CHECK(!is_implicit_phase_chain({"phase1", "run"}), "single phase1 is not a chain");
    CHECK(!is_implicit_phase_chain({"phase2", "run"}), "single phase2 is not a chain");
    CHECK(is_implicit_phase_chain({"phase1", "phase2"}), "phase1+phase2 chained -> reject");
    CHECK(is_implicit_phase_chain({"phase1 run", "phase3 run"}), "phase1+phase3 chained");
    CHECK(is_implicit_phase_chain({"run"}), "aggregate run entry -> reject");
    CHECK(is_implicit_phase_chain({"graph"}), "aggregate graph entry -> reject");
    CHECK(is_implicit_phase_chain({"pipeline"}), "aggregate pipeline entry -> reject");
}

static void test_metrics() {
    // 必采字段键集合（§10.5 逐项）
    const std::vector<std::string> keys = required_metric_keys();
    const char* must[] = {"process_cpu_seconds", "per_thread_cpu_max_seconds",
                          "per_thread_cpu_sum_seconds", "rss_bytes", "pss_bytes",
                          "rss_growth_mb_per_s", "read_bytes", "write_bytes",
                          "io_wait_percent", "work_units", "queue_depth",
                          "worker_balance", "wall_seconds", "active_window_seconds"};
    for (const char* k : must) {
        bool found = false;
        for (const auto& s : keys) if (s == k) found = true;
        CHECK(found, std::string("required metric key present: ") + k);
    }
    HeavyRunMetrics m;
    std::string missing;
    CHECK(!heavy_metrics_complete(m, &missing), "empty metrics incomplete");
    CHECK(missing.find("per_thread_cpu_sum_seconds") != std::string::npos,
          "missing list names per_thread_cpu_sum_seconds");
    m.allocated_cores = 4; m.threads = 4; m.active_compute_threads = 4;
    m.per_thread_cpu_sum_seconds = 1.0; m.per_thread_cpu_max_seconds = 0.3;
    m.wall_seconds = 1.0; m.active_window_seconds = 0.5;
    m.phase = "phase2"; m.mode = "psfsw_robust";
    CHECK(heavy_metrics_complete(m, &missing), "complete metrics accepted");
}

static void test_negative() {
    // 负向: 违规注入必须被检出（每条都断言"检出"）。
    ProcessBudgetRegistry& R = ProcessBudgetRegistry::instance();
    R.reset_for_test();
    // 注入 1: 私有第二预算源
    CHECK(R.register_source("owner", 4), "neg setup: primary source");
    const bool second_accepted = R.register_source("private_pool", 4);
    CHECK(!second_accepted, "neg1 second private budget source detected");
    // 注入 2: 超额订阅租约
    std::string err;
    (void)R.request_lease("a", 4, &err);
    CHECK(R.request_lease("b", 1, &err) == 0, "neg2 oversubscribe detected");
    R.reset_for_test();
    // 注入 3: psf_snr_power 进生产
    CHECK(route_phase2_mode("psf_snr_power").kind == RouteKind::kReject,
          "neg3 deferred mode in production detected");
    // 注入 4: legacy weight_mode 0
    CHECK(route_legacy_weight_mode_int(0).kind == RouteKind::kReject,
          "neg4 legacy weight_mode 0 detected");
    // 注入 5: 隐式 phase 串接
    CHECK(is_implicit_phase_chain({"phase1", "phase2", "phase3"}),
          "neg5 implicit phase chain detected");
    // 注入 6: SO-05 自动判决升级
    std::vector<GateFinding> f; GateFinding x; x.kind = "single_threaded";
    f.push_back(x);
    CHECK(!evaluate_heavy_run(f).hard_fail,
          "neg6 auto-adjudication escalation blocked (record-only)");
}

int main(int argc, char** argv) {
    const std::string mode = (argc > 1) ? argv[1] : "units";
    if (mode == "modes") test_modes();
    else if (mode == "budget") test_budget();
    else if (mode == "so05") test_so05();
    else if (mode == "isolation") test_isolation();
    else if (mode == "metrics") test_metrics();
    else if (mode == "negative") test_negative();
    else if (mode == "units") {
        test_modes(); test_budget(); test_so05(); test_isolation(); test_metrics();
    } else {
        std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
        return 2;
    }
    std::fprintf(stderr, "%s: checks=%d fail=%d\n", mode.c_str(), g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
