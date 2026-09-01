// CPU-008 单元测试: 资源监测器采样 + 开销 + gate 阈值
#include "monitor.h"
#include "resource_gate.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

using astrocs::ProcessMonitor;
using astrocs::GateConfig;
using astrocs::ResKind;
using astrocs::GateDiag;
using astrocs::evaluate_gate;
using astrocs::compute_cores_threshold;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) 采样字段: tick 后样本含 cpu/rss/read/write 增量
  {
    ProcessMonitor mon(0.05);
    // 制造一点 CPU 活动
    volatile double acc = 0;
    for (int i = 0; i < 1000000; ++i) acc += i * 0.0001;
    mon.tick();
    auto s = mon.summary();
    CHECK(s.n_samples >= 1);
    // RSS 非负
    CHECK(s.peak_rss_bytes > 0);
    // wall 非零
    CHECK(s.wall_seconds >= 0.0);
    printf("CPU-008 sample: rss_peak=%lluB read=%llu write=%llu\n",
           (unsigned long long)s.peak_rss_bytes,
           (unsigned long long)s.total_read_bytes,
           (unsigned long long)s.total_write_bytes);
  }

  // 2) run_for 采样间隔: 0.3s @ 0.1s interval → ~3 样本
  {
    ProcessMonitor mon(0.1);
    mon.run_for(0.35);
    auto s = mon.summary();
    CHECK(s.n_samples >= 2);
    printf("CPU-008 run_for samples=%llu\n", (unsigned long long)s.n_samples);
  }

  // 3) 开销: 采样开销占 wall < 2% (规格: 目标<2% wall time)
  {
    ProcessMonitor mon(0.05);
    mon.run_for(0.5);
    auto s = mon.summary();
    double overhead_frac = s.sample_overhead_ms > 0 && s.wall_seconds > 0
        ? (s.sample_overhead_ms / 1000.0) / s.wall_seconds : 0.0;
    printf("CPU-008 overhead_frac=%.4f (wall=%.3fs)\n", overhead_frac, s.wall_seconds);
    CHECK(overhead_frac < 0.02);  // <2% 目标
  }

  // 4) gate 阈值: compute 且 wall>=5s 时 avg_equivalent_cores 下限 = 0.80*min(workers,cpus)
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.selected_workers = 2;
    g.available_cpus = 2;
    double thr = compute_cores_threshold(g);
    CHECK(std::fabs(thr - 1.6) < 1e-9);  // 0.80*2
    // 1 核: 0.8
    g.selected_workers = 1; g.available_cpus = 1;
    CHECK(std::fabs(compute_cores_threshold(g) - 0.8) < 1e-9);
    // I/O: 无 CPU 阈值
    g.kind = ResKind::Io;
    CHECK(compute_cores_threshold(g) == 0.0);
  }

  // 5) mixed 未拆份 → FAIL (07 §3)
  {
    GateConfig g;
    g.kind = ResKind::Mixed;
    g.mixed_has_compute_subrange = false;
    g.mixed_has_io_subrange = false;
    CHECK(evaluate_gate(g) == GateDiag::MixedUnsplit);
    // 拆份后通过
    g.mixed_has_compute_subrange = true;
    g.mixed_has_io_subrange = true;
    CHECK(evaluate_gate(g) == GateDiag::Ok);
  }

  if (failures == 0) {
    std::printf("CPU-008 TESTS PASS (采样字段/间隔/开销<2%%/阈值/mixed 拆份)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-008 TESTS FAIL (%d)\n", failures);
  return 1;
}
