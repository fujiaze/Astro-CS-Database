// eng/tests/unit/mon002_gate_test.cpp — MON-002 (G3) 资源门禁单元测试
// 覆盖: compute 门禁(worker p50/CPU p50>=90/mean>=85)/短任务不得掩盖单线程
//       (MON-002 复验: wall<5s 固定豁免只对 Io kind, Compute 短任务按已采样本判)/
//       available>=2 不退 1/io 证据/memory 带宽证据/mixed 必须拆份/first-10s 快速失败/
//       横切诊断(memory_growth/progress_stall/io_wait_high)/伪造采样数据必须 FAIL。
// 规格依据: 04_TASK_SPECIFICATIONS §MON-002(v6_1_rework)。
#include "exit_codes.h"
#include "resource_gate.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// B7/F-14 回归锁辅助: 读取落盘报告文本 / 提取 JSON u64 字段(轻量, 无 nlohmann)。
static std::string read_report_text(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "r");
    if (!f) return std::string();
    std::string s;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), f)) s += buf;
    std::fclose(f);
    return s;
}
static bool report_json_u64(const std::string& js, const char* key, unsigned long long* out) {
    const std::string pat = std::string("\"") + key + "\":";
    const auto pos = js.find(pat);
    if (pos == std::string::npos) return false;
    *out = std::strtoull(js.c_str() + pos + pat.size(), nullptr, 10);
    return true;
}

int main() {
    // 1) compute 门禁通过: available=4, workers=4, CPU p50=95(>=90) mean=90(>=85), wall=30s
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.8; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 90.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::Ok);
    }
    // 2) 单线程失败: available>=2 但 workers<2(未采样回退 selected/max_active 判定)
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 1; g.max_active_threads = 1;
        g.avg_equivalent_cores = 1.0; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 100.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::SingleThreaded);
    }
    // 3) 低等效核失败: avg 0.5 < 0.8*4=3.2
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 0.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 15.0; g.iowait_percent = 1.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::LowAvgCores);
    }
    // 4) [MON-002 复验修正] 短任务(wall<5s)单线程 → FAIL。
    //    旧实现 wall<5s 对 Compute 一刀切豁免, 掩盖单线程(规格明令禁止:
    //    "heavy 任务即使短于 5 秒也不能因固定豁免而掩盖单线程"; 5s/5% 豁免仅限 Io)。
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 1; g.max_active_threads = 1;
        g.avg_equivalent_cores = 0.3; g.wall_seconds = 2.0;
        g.has_stage_annotation = true; g.cpu_percent = 30.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::SingleThreaded);
    }
    // 4b) 短任务多核正常 → **NotApplicable**(短任务只跳过统计判据, 不跳过单线程
    //     判定; GATE-FIX-RES: 跳过统计判据的显式分类是 NotApplicable, 不再静默
    //     Ok —— "未判"与"判过且通过"必须可区分)
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 0.3; g.wall_seconds = 2.0;
        g.has_stage_annotation = true; g.cpu_percent = 30.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::NotApplicable);
    }
    // 4c) 采样 worker p50 权威于租约: 伪造 selected=4 但采样 p50=1.0 → FAIL(负向:
    //     伪造 worker 采样数据喂 evaluate_gate 必须失败)
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.8; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        g.workers_p50 = 1.0; g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 90.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::SingleThreaded);
    }
    // 5) 无标注 >5s → UnannotatedPriority
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 2; g.selected_workers = 2; g.max_active_threads = 2;
        g.avg_equivalent_cores = 2.0; g.wall_seconds = 30.0;
        g.has_stage_annotation = false;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::UnannotatedPriority);
    }
    // 6) io 证据: 缺证据 → IoMissingEvidence
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Io;
        g.wall_seconds = 30.0; g.has_stage_annotation = true;
        g.io_bytes = 0; g.io_ops = 0; g.io_await_ms = 0.0; g.io_is_short_serial = false;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::IoMissingEvidence);
    }
    // 7) io 短串行豁免(5s/5% 豁免仅对 Io kind) → Ok
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Io;
        g.io_is_short_serial = true;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::Ok);
    }
    // 8) memory 带宽未测 → FAIL
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Memory;
        g.achieved_memory_bandwidth_frac = -1.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::MemoryBandwidthLow);
    }
    // 9) mixed 未拆份 → MixedUnsplit
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Mixed;
        g.mixed_has_compute_subrange = false; g.mixed_has_io_subrange = false;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::MixedUnsplit);
    }
    // 10) first-10s 快速失败: 低 CPU+非 IO+非内存饱和 → true
    {
        acsd::GateConfig g;
        g.first10s_low_cpu = true; g.first10s_non_io = true; g.first10s_mem_not_saturated = true;
        CHECK(acsd::fast_fail_first10s(g));
        g.first10s_non_io = false;
        CHECK(!acsd::fast_fail_first10s(g));
    }
    // 11) 全局锁退化: N-worker 比 1-worker 慢 → FAIL
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 90.0;
        g.one_worker_ns = 100.0; g.n_worker_ns = 200.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::GlobalLockDegradation);
    }
    // 12) [MON-002 阈值] CPU p50 < 90%(active window>=10s 语义由调用方保证) → CpuP50Low
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 60.0;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 60.0; g.cpu_mean_percent = 92.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::CpuP50Low);
    }
    // 13) [MON-002 阈值] CPU mean < 85% → CpuMeanLow
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 80.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::CpuMeanLow);
    }
    // 14) [横切诊断] 内存持续增长: rss_slope 64MB/s > 32MB/s → MemoryGrowth;
    //     负斜率(收缩)不得误判
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.rss_slope_measured = true; g.rss_slope_mb_per_s = 64.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::MemoryGrowth);
        g.rss_slope_mb_per_s = -10.0;
        CHECK(acsd::evaluate_gate(g) != acsd::GateDiag::MemoryGrowth);
    }
    // 15) [横切诊断] 无进度 → ProgressStall
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.progress_stalled = true;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::ProgressStall);
    }
    // 16) [横切诊断] 异常 IO 等待 iowait 80% > 50% → IoWaitHigh
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.iowait_percent = 80.0;
        CHECK(acsd::evaluate_gate(g) == acsd::GateDiag::IoWaitHigh);
    }
    // 17) diagnosis 含实测值 vs 阈值(MON-002: 失败输出 diagnosis 且内容具体)
    {
        acsd::GateConfig g;
        g.kind = acsd::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 60.0;
        const std::string m1 = acsd::diag_message(acsd::GateDiag::CpuP50Low, g);
        CHECK(m1.find("60") != std::string::npos && m1.find("90") != std::string::npos);
        g.workers_p50 = 1.0;
        const std::string m2 = acsd::diag_message(acsd::GateDiag::SingleThreaded, g);
        CHECK(m2.find("worker p50") != std::string::npos && m2.find("1.0") != std::string::npos);
        const acsd::GateConfig g3;
        const std::string m3 = acsd::diag_message(acsd::GateDiag::MemoryGrowth, g3);
        CHECK(m3.find("32.0") != std::string::npos);
    }
    // 18) 退出码常量: gate 失败必须映射统一 RESOURCE(10)(禁止仅 emit 不改退出状态)
    {
        CHECK(acsd::RESOURCE == 10);
    }

    // ========================================================================
    // B7 / F-14 回归锁(OWNER-07 执行侧裁决: reclaim 被测量改为分配器实占优先,
    // 不放宽任何冻结阈值; 探针不可用回退 RSS 老口径)。
    // ========================================================================
    // 锁 ①: 注入泄漏(持续分配不释放 ⇒ 收尾 outstanding 高、斜率无界) → 必 FAIL。
    {
        // 峰值 = 收尾 outstanding(全未归还), RSS 未回落: 回落 0 < 0.5 且收尾
        // 实占 2.5GiB > 32MiB → 防漏硬条件命中 UnexplainedResidual。
        acsd::AllocReclaimInputs in;
        in.peak_rss_bytes = 3072ull << 20;
        in.last_rss_bytes = 3072ull << 20;
        in.peak_alloc_outstanding_bytes = 2560ull << 20;
        in.last_alloc_outstanding_bytes = 2560ull << 20;
        in.allocator_probe_available = true;
        const acsd::AllocReclaimDecision leak = acsd::decide_allocation_reclaim(in);
        CHECK(leak.verdict == acsd::AllocReclaimVerdict::UnexplainedResidual);
        CHECK(leak.reclaim_frac_alloc < acsd::kAllocMinReclaimFrac);
        CHECK(leak.alloc_measure_used);
        acsd::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = 306.0;
        g.alloc_growth_mb_per_s = acsd::kMon001NotSampled;   // 隔离 reclaim 判据
        g.alloc_reclaim_verdict = leak.verdict;
        CHECK(acsd::evaluate_mon002(g) == acsd::GateDiag::AllocReclaimMissing);
        // 斜率面: 持续分配 ⇒ RSS 曲线 Theil-Sen 无界(40MiB/s ≥ 32MiB/s) → FAIL。
        acsd::AllocationRecorder rec;
        for (int i = 0; i < 30; ++i) {
            acsd::ProcSample s;
            s.rss_bytes = (200ull << 20) + static_cast<uint64_t>(i) * (20ull << 20);
            s.vms_bytes = s.rss_bytes + (64ull << 20);
            rec.observe(i * 0.5, s);
        }
        rec.finalize();
        CHECK(rec.report().growth_verdict == acsd::AllocGrowthVerdict::Unbounded);
        g.alloc_samples_measured = static_cast<double>(rec.report().n_samples);
        g.alloc_growth_mb_per_s = rec.report().rss_growth_mb_per_s;
        g.alloc_reclaim_verdict = rec.report().reclaim_verdict;
        CHECK(acsd::evaluate_mon002(g) == acsd::GateDiag::AllocGrowthUnbounded);
    }
    // 锁 ②: 仅分配器缓存型(大量 alloc/free 后退出; RSS 残留但 outstanding 已
    //       归还) → 必 PASS 且 allocator_cache_residual_bytes 非零。
    {
        acsd::AllocReclaimInputs in;
        in.peak_rss_bytes = 4096ull << 20;
        in.last_rss_bytes = 3072ull << 20;                 // RSS 残留 1GiB(arena 不还页)
        in.peak_alloc_outstanding_bytes = 2048ull << 20;
        in.last_alloc_outstanding_bytes = 200ull << 20;    // live outstanding 已归还 90.2%
        in.allocator_probe_available = true;
        const acsd::AllocReclaimDecision d = acsd::decide_allocation_reclaim(in);
        CHECK(d.verdict == acsd::AllocReclaimVerdict::Reclaimed);
        CHECK(d.alloc_measure_used);
        CHECK(d.reclaim_frac_alloc >= acsd::kAllocMinReclaimFrac);
        const uint64_t expect_cache = (3072ull << 20) - (200ull << 20);
        CHECK(d.allocator_cache_residual_bytes == expect_cache);
        CHECK(d.allocator_cache_residual_bytes != 0);
        acsd::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = 306.0;
        g.alloc_growth_mb_per_s = acsd::kMon001NotSampled;
        g.alloc_reclaim_verdict = d.verdict;
        CHECK(acsd::evaluate_mon002(g) == acsd::GateDiag::Ok);
        // 新字段必须落进 memory report JSON(不得静默通过, schema v2)。
        acsd::AllocReport rep;
        rep.n_samples = 306; rep.n_curve = 306; rep.wall_seconds = 152.5;
        rep.peak_rss_bytes = in.peak_rss_bytes; rep.last_rss_bytes = in.last_rss_bytes;
        rep.peak_alloc_outstanding_bytes = in.peak_alloc_outstanding_bytes;
        rep.last_alloc_outstanding_bytes = in.last_alloc_outstanding_bytes;
        rep.allocator_probe_available = true;
        rep.reclaim_frac = d.reclaim_frac;
        rep.reclaim_frac_alloc = d.reclaim_frac_alloc;
        rep.retained_bytes = d.retained_bytes;
        rep.allocator_cache_residual_bytes = d.allocator_cache_residual_bytes;
        rep.reclaim_measure_alloc = d.alloc_measure_used;
        rep.reclaim_verdict = d.verdict;
        const std::string dir = "run/rqs-fix/B7-memgate/tmp_mon002_gate";
        std::filesystem::create_directories(dir);
        CHECK(acsd::write_alloc_report_json(dir, rep));
        const std::string js = read_report_text(dir + "/alloc_report.json");
        CHECK(js.find("acsd.memory-report/v2") != std::string::npos);
        CHECK(js.find("\"reclaim_measure\":\"allocator\"") != std::string::npos);
        CHECK(js.find("\"reclaim_verdict\":\"reclaimed\"") != std::string::npos);
        unsigned long long cache_field = 0;
        CHECK(report_json_u64(js, "allocator_cache_residual_bytes", &cache_field));
        CHECK(cache_field == expect_cache);
        CHECK(cache_field != 0);
    }
    // 锁 ③: 结构对照 —— 探针不可用时走 RSS 老口径, 旧判定逐条不变。
    {
        acsd::AllocReclaimInputs in;
        // (3a) 不可用 + 回落 0.08 且残留 40MiB > 32MiB → 旧 UnexplainedResidual
        in.peak_rss_bytes = 500ull << 20;
        in.last_rss_bytes = 460ull << 20;
        in.allocator_probe_available = false;
        const acsd::AllocReclaimDecision a = acsd::decide_allocation_reclaim(in);
        CHECK(a.verdict == acsd::AllocReclaimVerdict::UnexplainedResidual);
        CHECK(a.reclaim_frac_alloc < 0.0);          // -1 哨兵, 不冒充
        CHECK(!a.alloc_measure_used);
        CHECK(a.allocator_cache_residual_bytes == 0);
        // (3b) 不可用 + 回落 0.7 >= 0.5 → Reclaimed(旧口径)
        in.last_rss_bytes = 150ull << 20;
        CHECK(acsd::decide_allocation_reclaim(in).verdict ==
              acsd::AllocReclaimVerdict::Reclaimed);
        // (3c) 不可用 + 绝对差 20MiB <= 32MiB 容差 → Reclaimed(旧容差语义)
        in.peak_rss_bytes = 200ull << 20; in.last_rss_bytes = 180ull << 20;
        CHECK(acsd::decide_allocation_reclaim(in).verdict ==
              acsd::AllocReclaimVerdict::Reclaimed);
        // (3d) 探针可用但峰值实占 4MiB < 残差容差(不足以解释进程内存)→ 回退
        //      RSS, 与 (3a) 同判(该回退不放松任何阈值)。
        in.peak_rss_bytes = 500ull << 20; in.last_rss_bytes = 460ull << 20;
        in.allocator_probe_available = true;
        in.peak_alloc_outstanding_bytes = 4ull << 20;
        in.last_alloc_outstanding_bytes = 4ull << 20;
        const acsd::AllocReclaimDecision df = acsd::decide_allocation_reclaim(in);
        CHECK(df.verdict == acsd::AllocReclaimVerdict::UnexplainedResidual);
        CHECK(!df.alloc_measure_used);
        // (3e) 时间窗逃逸 PeakInWindow 保留(RSS 老口径)
        in.allocator_probe_available = false;
        in.peak_alloc_outstanding_bytes = 0; in.last_alloc_outstanding_bytes = 0;
        in.rss_peak_in_window = true;
        CHECK(acsd::decide_allocation_reclaim(in).verdict ==
              acsd::AllocReclaimVerdict::PeakInWindow);
        // (3f) 时间窗逃逸在分配器口径同样保留(未命中防漏硬条件时)。
        in.allocator_probe_available = true;
        in.peak_alloc_outstanding_bytes = 1ull << 30;
        in.last_alloc_outstanding_bytes = 16ull << 20;    // 收尾 <= 32MiB, 不命中硬条件
        in.alloc_peak_in_window = true;
        CHECK(acsd::decide_allocation_reclaim(in).verdict ==
              acsd::AllocReclaimVerdict::PeakInWindow);
    }
    // 锁 ④: 真实 T4 全链 phase1 run 实测值回放 → Reclaimed(旧口径误判为
    //       unexplained_residual)。
    {
        // 证据(run/release-rescue/real-chain-fix/cli/F78_t4_green/alloc_report.json):
        //   peak_rss_bytes=4668153856   last_rss_bytes=2733805568
        //   peak_alloc_outstanding_bytes=2637375312  last_alloc_outstanding_bytes=204048272
        //   allocator_probe_available=true  wall_seconds=152.501  n_samples=306
        //   growth_verdict=stable  rss_slope_bytes_per_s=1340.555(≈0.001 MiB/s)
        //   旧口径: reclaim_frac(RSS)=0.414 < 0.5 且 retained=1934348288 > 32MiB
        //           → unexplained_residual(误判);
        //   分配器口径: (2637375312-204048272)/2637375312 = 0.9226 ≥ 0.5 → Reclaimed。
        acsd::AllocReclaimInputs in;
        in.peak_rss_bytes = 4668153856ull;
        in.last_rss_bytes = 2733805568ull;
        in.peak_alloc_outstanding_bytes = 2637375312ull;
        in.last_alloc_outstanding_bytes = 204048272ull;
        in.allocator_probe_available = true;
        const acsd::AllocReclaimDecision d = acsd::decide_allocation_reclaim(in);
        CHECK(d.verdict == acsd::AllocReclaimVerdict::Reclaimed);
        CHECK(d.alloc_measure_used);
        CHECK(d.reclaim_frac_alloc > 0.92 && d.reclaim_frac_alloc < 0.93);
        CHECK(d.reclaim_frac >= 0.41 && d.reclaim_frac < 0.42);  // RSS 旧口径原样保留
        CHECK(d.reclaim_frac < acsd::kAllocMinReclaimFrac);  // 证明旧口径必判负
        CHECK(d.retained_bytes == 1934348288ull);
        CHECK(d.retained_bytes > acsd::kAllocReclaimResidualTolBytes);
        const uint64_t expect_cache = 2733805568ull - 204048272ull;   // 2529757296
        CHECK(d.allocator_cache_residual_bytes == expect_cache);
        CHECK(d.allocator_cache_residual_bytes != 0);
        acsd::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = 306.0;
        g.alloc_growth_mb_per_s = 0.001;   // stable
        g.alloc_reclaim_verdict = d.verdict;
        CHECK(acsd::evaluate_mon002(g) == acsd::GateDiag::Ok);
        acsd::AllocReport rep;
        rep.n_samples = 306; rep.n_curve = 306; rep.wall_seconds = 152.501;
        rep.peak_rss_bytes = in.peak_rss_bytes; rep.last_rss_bytes = in.last_rss_bytes;
        rep.peak_alloc_outstanding_bytes = in.peak_alloc_outstanding_bytes;
        rep.last_alloc_outstanding_bytes = in.last_alloc_outstanding_bytes;
        rep.allocator_probe_available = true;
        rep.reclaim_frac = d.reclaim_frac;
        rep.reclaim_frac_alloc = d.reclaim_frac_alloc;
        rep.retained_bytes = d.retained_bytes;
        rep.allocator_cache_residual_bytes = d.allocator_cache_residual_bytes;
        rep.reclaim_measure_alloc = d.alloc_measure_used;
        rep.reclaim_verdict = d.verdict;
        const std::string dir = "run/rqs-fix/B7-memgate/tmp_mon002_gate_t4";
        std::filesystem::create_directories(dir);
        CHECK(acsd::write_alloc_report_json(dir, rep));
        const std::string js = read_report_text(dir + "/alloc_report.json");
        CHECK(js.find("\"reclaim_verdict\":\"reclaimed\"") != std::string::npos);
        CHECK(js.find("\"reclaim_measure\":\"allocator\"") != std::string::npos);
        unsigned long long cache_field = 0;
        CHECK(report_json_u64(js, "allocator_cache_residual_bytes", &cache_field));
        CHECK(cache_field == expect_cache);
    }

    if (failures == 0) {
        std::printf("MON-002 TESTS PASS (compute 门禁/短任务不掩盖单线程/worker p50/CPU p50>=90/mean>=85/"
                    "io证据/memory带宽/mixed拆份/first-10s/锁退化/memory_growth/progress_stall/io_wait_high/"
                    "B7-F14 回归锁 4 组: 注入leak必FAIL/仅分配器缓存必PASS且字段非零/探针不可用回退RSS/真实T4 Reclaimed)\n");
        return 0;
    }
    std::fprintf(stderr, "MON-002 TESTS FAIL (%d)\n", failures);
    return 1;
}
