// tests/unit/mon002_gate_test.cpp — MON-002 (G3) 资源门禁单元测试
// 覆盖: compute 门禁(worker p50/CPU p50/mean)/短任务豁免(<10s 不判)/available≥2 不退 1/
//       io 证据/memory 带宽证据/mixed 必须拆份/first-10s 快速失败。
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
    // 1) compute 门禁通过: available=4, workers=4, CPU p50=95 mean=90, wall=30s
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
        g.avg_equivalent_cores = 3.8; g.wall_seconds = 30.0;
        g.has_stage_annotation = true; g.cpu_percent = 95.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);
    }
    // 2) 单线程失败: available>=2 但 workers<2
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
    // 4) 短任务豁免: wall<5s → Ok
    {
        astrocs::GateConfig g;
        g.kind = astrocs::ResKind::Compute;
        g.available_cpus = 4; g.selected_workers = 1; g.max_active_threads = 1;
        g.avg_equivalent_cores = 0.3; g.wall_seconds = 2.0;
        g.has_stage_annotation = true; g.cpu_percent = 30.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);
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
    // 7) io 短串行豁免: <5s → Ok
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

    if (failures == 0) {
        std::printf("MON-002 TESTS PASS (compute 门禁/短任务豁免/io证据/memory带宽/mixed拆份/first-10s/锁退化)\n");
        return 0;
    }
    std::fprintf(stderr, "MON-002 TESTS FAIL (%d)\n", failures);
    return 1;
}
