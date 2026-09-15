// tests/unit/p36_frame_admission_test.cpp — P36 D5 帧级并发内存准入契约单元测试
// 覆盖: K_mem 公式 / reserve 公式 / 四条 reason 分支 / 判定随输入变化 (非恒真) /
//       生产分辨率 (T4 auto 8.46 GB) 在本机 MemTotal 15.62 GiB 下 K_mem=1。
#include "frame_admission.h"

#include <cstdint>
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

using astrocs::FrameAdmissionDecision;
using astrocs::FrameAdmissionParams;
using astrocs::FramePeakSource;

int main() {
  const uint64_t gib = astrocs::kGiB;
  const uint64_t mem_total = static_cast<uint64_t>(15.62 * static_cast<double>(gib));

  // reserve = max(1 GiB, 5% MemTotal) + 0.25 GiB * active_frames
  CHECK(astrocs::frame_admission_reserve(mem_total, 1) ==
        std::max<uint64_t>(gib, mem_total / 20) + gib / 4);

  // 生产分辨率 T4 auto: 单帧 8.46 GB, K_mem = 1 ⇒ memory_insufficient
  {
    FrameAdmissionParams p;
    p.mem_total_bytes = mem_total;
    p.mem_available_bytes = 6 * gib;
    p.single_frame_peak_bytes = static_cast<uint64_t>(8.46 * static_cast<double>(gib));
    p.peak_source = FramePeakSource::MeasuredThisRun;
    p.cpu_budget = 16;
    const FrameAdmissionDecision d = astrocs::frame_admission_decide(p);
    CHECK(d.k_mem == 1);
    CHECK(d.k_final == 1);
    CHECK(!d.concurrent);
    CHECK(std::string(d.reason) == "memory_insufficient");
  }

  // nside=512 单帧 ~1.5 GB: K_mem >= 2, 但帧并发编排未接 ⇒ frame_concurrency_not_wired
  {
    FrameAdmissionParams p;
    p.mem_total_bytes = mem_total;
    p.mem_available_bytes = 12 * gib;
    p.single_frame_peak_bytes = static_cast<uint64_t>(1.5 * static_cast<double>(gib));
    p.cpu_budget = 16;
    const FrameAdmissionDecision d = astrocs::frame_admission_decide(p);
    CHECK(d.k_mem >= 2);
    CHECK(d.k_final >= 2);
    CHECK(!d.concurrent);  // 本批未接帧并发编排, 如实记录
    CHECK(std::string(d.reason) == "frame_concurrency_not_wired");
  }

  // 实时余量不足: K_mem>=2 但 MemAvailable < 2*peak+reserve
  {
    FrameAdmissionParams p;
    p.mem_total_bytes = mem_total;
    p.mem_available_bytes = 3 * gib;
    p.single_frame_peak_bytes = static_cast<uint64_t>(1.5 * static_cast<double>(gib));
    p.cpu_budget = 16;
    const FrameAdmissionDecision d = astrocs::frame_admission_decide(p);
    CHECK(d.k_mem >= 2);
    CHECK(std::string(d.reason) == "available_below_two_frames");
    CHECK(!d.concurrent);
  }

  // 核预算不足: budget=1 < 2*b_min
  {
    FrameAdmissionParams p;
    p.mem_total_bytes = mem_total;
    p.mem_available_bytes = 12 * gib;
    p.single_frame_peak_bytes = static_cast<uint64_t>(1.5 * static_cast<double>(gib));
    p.cpu_budget = 1;
    const FrameAdmissionDecision d = astrocs::frame_admission_decide(p);
    CHECK(d.k_cpu == 0 || d.k_cpu == 1);
    CHECK(std::string(d.reason) == "memory_insufficient" ||
          std::string(d.reason) == "cpu_budget_single");
    CHECK(!d.concurrent);
  }

  // 阴性对照/非恒真: 同一内存下 peak 越小 K_mem 越大 (判定确实依赖输入)
  {
    FrameAdmissionParams p;
    p.mem_total_bytes = mem_total;
    p.single_frame_peak_bytes = static_cast<uint64_t>(0.1 * static_cast<double>(gib));
    const uint32_t k_small = astrocs::frame_admission_k_mem(
        p.mem_total_bytes, p.single_frame_peak_bytes,
        astrocs::frame_admission_reserve(p.mem_total_bytes, 1));
    p.single_frame_peak_bytes = static_cast<uint64_t>(8.46 * static_cast<double>(gib));
    const uint32_t k_big = astrocs::frame_admission_k_mem(
        p.mem_total_bytes, p.single_frame_peak_bytes,
        astrocs::frame_admission_reserve(p.mem_total_bytes, 1));
    CHECK(k_small > k_big);
    CHECK(k_big == 1);
    // peak=0 (未知) 不得放大并发
    CHECK(astrocs::frame_admission_k_mem(mem_total, 0, gib) == 1);
  }

  if (failures == 0) {
    std::printf("P36_FRAME_ADMISSION_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "P36_FRAME_ADMISSION_FAIL failures=%d\n", failures);
  return 1;
}
