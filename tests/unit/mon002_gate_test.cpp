// tests/unit/mon002_gate_test.cpp — MON-002 (G3) 资源门禁单元测试
// 覆盖: compute 门禁(worker p50/CPU p50>=90/mean>=85)/短任务不得掩盖单线程
//       (MON-002 复验: wall<5s 固定豁免只对 Io kind, Compute 短任务按已采样本判)/
//       available>=2 不退 1/io 证据/memory 带宽证据/mixed 必须拆份/first-10s 快速失败/
//       横切诊断(memory_growth/progress_stall/io_wait_high)/伪造采样数据必须 FAIL。
// 规格依据: 04_TASK_SPECIFICATIONS §MON-002(v6_1_rework)。
#include "exit_codes.h"
#include "resource_gate.h"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
    // 1) compute 门禁通过: available=4, workers=4, CPU p50=95(>=90) mean=90(>=85), wall=30s
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.8; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 90.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);
    }
    // 2) 单线程失败: available>=2 但 workers<2(未采样回退 selected/max_active 判定)
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 1; g.max_active_threads = 1;
        g.avg_equivalent_cores = 1.0; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 100.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::SingleThreaded);
    }
    // 3) 低等效核失败: avg 0.5 < 0.8*4=3.2
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 0.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 15.0; g.iowait_percent = 1.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::LowAvgCores);
    }
    // 4) [MON-002 复验修正] 短任务(wall<5s)单线程 → FAIL。
    //    旧实现 wall<5s 对 Compute 一刀切豁免, 掩盖单线程(规格明令禁止:
    //    "heavy 任务即使短于 5 秒也不能因固定豁免而掩盖单线程"; 5s/5% 豁免仅限 Io)。
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 1; g.max_active_threads = 1;
        g.avg_equivalent_cores = 0.3; g.wall_seconds = 2.0;
        g.has_stage_annotation = true; g.cpu_percent = 30.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::SingleThreaded);
    }
    // 4b) 短任务多核正常 → Ok(短任务只跳过统计判据, 不跳过单线程判定)
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 0.3; g.wall_seconds = 2.0;
        g.has_stage_annotation = true; g.cpu_percent = 30.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);
    }
    // 4c) 采样 worker p50 权威于租约: 伪造 selected=4 但采样 p50=1.0 → FAIL(负向:
    //     伪造 worker 采样数据喂 evaluate_gate 必须失败)
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.8; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        g.workers_p50 = 1.0; g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 90.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::SingleThreaded);
    }
    // 5) 无标注 >5s → UnannotatedPriority
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 2; g.selected_workers = 2; g.max_active_threads = 2;
        g.avg_equivalent_cores = 2.0; g.wall_seconds = 30.0;
        g.has_stage_annotation = false;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::UnannotatedPriority);
    }
    // 6) io 证据: 缺证据 → IoMissingEvidence
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Io;
        g.wall_seconds = 30.0; g.has_stage_annotation = true;
        g.io_bytes = 0; g.io_ops = 0; g.io_await_ms = 0.0; g.io_is_short_serial = false;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::IoMissingEvidence);
    }
    // 7) io 短串行豁免(5s/5% 豁免仅对 Io kind) → Ok
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Io;
        g.io_is_short_serial = true;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);
    }
    // 8) memory 带宽未测 → FAIL
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Memory;
        g.achieved_memory_bandwidth_frac = -1.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::MemoryBandwidthLow);
    }
    // 9) mixed 未拆份 → MixedUnsplit
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Mixed;
        g.mixed_has_compute_subrange = false; g.mixed_has_io_subrange = false;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::MixedUnsplit);
    }
    // 10) first-10s 快速失败: 低 CPU+非 IO+非内存饱和 → true
    {
        astrocs::GateConfig g;
        g.first10s_low_cpu = true; g.first10s_non_io = true; g.first10s_mem_not_saturated = true;
        CHECK(astrocs::fast_fail_first10s(g));
        g.first10s_non_io = false;
        CHECK(!astrocs::fast_fail_first10s(g));
    }
    // 11) 全局锁退化: N-worker 比 1-worker 慢 → FAIL
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 90.0;
        g.one_worker_ns = 100.0; g.n_worker_ns = 200.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::GlobalLockDegradation);
    }
    // 12) [MON-002 阈值] CPU p50 < 90%(active window>=10s 语义由调用方保证) → CpuP50Low
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 60.0;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 60.0; g.cpu_mean_percent = 92.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::CpuP50Low);
    }
    // 13) [MON-002 阈值] CPU mean < 85% → CpuMeanLow
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.5; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 80.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::CpuMeanLow);
    }
    // 14) [横切诊断] 内存持续增长: rss_slope 64MB/s > 32MB/s → MemoryGrowth;
    //     负斜率(收缩)不得误判
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.rss_slope_measured = true; g.rss_slope_mb_per_s = 64.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::MemoryGrowth);
        g.rss_slope_mb_per_s = -10.0;
        CHECK(astrocs::evaluate_gate(g) != astrocs::GateDiag::MemoryGrowth);
    }
    // 15) [横切诊断] 无进度 → ProgressStall
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.progress_stalled = true;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::ProgressStall);
    }
    // 16) [横切诊断] 异常 IO 等待 iowait 80% > 50% → IoWaitHigh
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.iowait_percent = 80.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::IoWaitHigh);
    }
    // 17) diagnosis 含实测值 vs 阈值(MON-002: 失败输出 diagnosis 且内容具体)
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.workers_p50 = 4.0; g.cpu_p50_percent = 60.0;
        const std::string m1 = astrocs::diag_message(astrocs::GateDiag::CpuP50Low, g);
        CHECK(m1.find("60") != std::string::npos && m1.find("90") != std::string::npos);
        g.workers_p50 = 1.0;
        const std::string m2 = astrocs::diag_message(astrocs::GateDiag::SingleThreaded, g);
        CHECK(m2.find("worker p50") != std::string::npos && m2.find("1.0") != std::string::npos);
        const astrocs::GateConfig g3;
        const std::string m3 = astrocs::diag_message(astrocs::GateDiag::MemoryGrowth, g3);
        CHECK(m3.find("32.0") != std::string::npos);
    }
    // 18) 退出码常量: gate 失败必须映射统一 RESOURCE(10)(禁止仅 emit 不改退出状态)
    {
        CHECK(astrocs::RESOURCE == 10);
    }

    if (failures == 0) {
        std::printf("MON-002 TESTS PASS (compute 门禁/短任务不掩盖单线程/worker p50/CPU p50>=90/mean>=85/"
                    "io证据/memory带宽/mixed拆份/first-10s/锁退化/memory_growth/progress_stall/io_wait_high)\n");
        return 0;
    }
    std::fprintf(stderr, "MON-002 TESTS FAIL (%d)\n", failures);
    return 1;
}
