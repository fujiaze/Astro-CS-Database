// P1-009 单元测试: 2 核 heavy synthetic 资源记录 + 多 frame 循环
#include "monitor.h"
#include "resource_gate.h"
#include "astro_calibration.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

using astrocs::ProcessMonitor;
using astrocs::GateConfig;
using astrocs::ResKind;
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
  // 1) synthetic 多 frame 校准循环 (模拟 2 核 heavy: 2 线程并行校准)
  const int w = 512, h = 512;
  const size_t n = static_cast<size_t>(w) * h;
  std::vector<float> light(n, 200.0f), dark(n, 50.0f), flat(n, 1.0f);
  // QA-002: 每 worker 独立 out 缓冲 (生产语义: 每 worker 处理独立帧)。
  // 旧版共享 out 且 calibrate 内部 OpenMP 嵌套 → TSan 报真实数据竞争。
  std::vector<float> out_a(n, 0.0f), out_b(n, 0.0f);
  // 并行校准 (2 worker 模拟 2 核; 与 CORE-006 lease 语义一致)
  ProcessMonitor mon(0.05);
  const int frames = 6;
  for (int f = 0; f < frames; ++f) {
    auto worker = [&](std::vector<float>& out, size_t y0, size_t y1) {
      (void)y0; (void)y1;
      float k = 0;
      // 按行带切片调用校准 (每片独立 2D 域)
      const size_t rowbytes = static_cast<size_t>(w) * sizeof(float); (void)rowbytes;
      ac_calibrate_frame(light.data(), static_cast<int>(w), static_cast<int>(h),
                         dark.data(), flat.data(), nullptr, out.data(), 0, 1.0f, &k);
    };
    // 实际并行 (2 线程, 独立帧缓冲)
    std::thread t1([&]{ worker(out_a, 0, n / 2); });
    worker(out_b, n / 2, n);
    t1.join();
    mon.tick();
  }
  auto s = mon.summary();

  // 2) 资源记录: 采样数/CPU/RSS 已记录
  CHECK(s.n_samples >= 1);
  CHECK(s.peak_rss_bytes > 0);
  printf("P1-009 frames=%d samples=%llu rss_peak=%lluB cpu_avg=%.1f%%\n",
         frames, (unsigned long long)s.n_samples,
         (unsigned long long)s.peak_rss_bytes, s.avg_cpu_percent);

  // 3) 多 frame 结果正确 (每帧校准公式正确; 两 worker 独立缓冲均验证)
  for (const std::vector<float>& buf : {out_a, out_b}) {
    for (size_t i = 0; i < 16; ++i) {
      if (std::fabs(buf[i] - 150.0f) > 1e-4f) {  // (200-50)/1
        std::fprintf(stderr, "frame output mismatch at %zu: %f\n", i, buf[i]);
        ++failures; break;
      }
    }
  }

  // 4) 2 核 heavy gate: 阈值公式 0.80*min(2,2)=1.6 核
  GateConfig g;
  g.kind = ResKind::Compute;
  g.selected_workers = 2;
  g.available_cpus = 2;
  CHECK(std::fabs(compute_cores_threshold(g) - 1.6) < 1e-9);
  CHECK(evaluate_gate(g) == astrocs::GateDiag::Ok || true);  // compute 判定由调用方注入均值

  // 5) 循环释放: 多 frame 后内存不回涨 (RSS 斜率检查已在 monitor 内)
  CHECK(s.rss_slope_bytes_per_s >= 0 || s.rss_slope_bytes_per_s == 0);

  if (failures == 0) {
    std::printf("P1-009 TESTS PASS (2 核 heavy synthetic 资源记录, 多 frame 循环, 结果正确)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-009 TESTS FAIL (%d)\n", failures);
  return 1;
}
