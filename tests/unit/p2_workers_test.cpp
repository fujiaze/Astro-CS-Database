// P2-002 单元测试: 生产禁 workers=1 + heavy 门禁 + cpu_workers 传递确定性
#include "resource_gate.h"
#include "astro/phase2/stage2_common.h"

#include <cmath>
#include <cstdio>
#include <string>

using astrocs::GateConfig;
using astrocs::ResKind;
using astrocs::GateDiag;
using astrocs::evaluate_gate;
using astrocs::compute_cores_threshold;
using astrocs::gate_diag_name;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) 生产 heavy 禁选 1: available>=2 且 selected_workers<2 → SingleThreaded 拒
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.available_cpus = 2;
    g.selected_workers = 1;   // heavy 配置试图选 1
    g.wall_seconds = 10.0;    // heavy: 超过 5s 短任务豁免阈值
    g.has_stage_annotation = true;  // 有标注, 避免 UnannotatedPriority
    CHECK(evaluate_gate(g) == GateDiag::SingleThreaded);
    // 2 worker 通过
    g.selected_workers = 2;
    g.max_active_threads = 2;
    CHECK(evaluate_gate(g) != GateDiag::SingleThreaded);
  }

  // 2) cpu_workers 传递: config 解析保留显式值; 0=auto 合法
  {
    // 直接用 P2Stage2Config 结构: cpu_workers 字段存在且默认合法
    P2Stage2Config cfg{};
    // 默认 exec 配置: cpu_workers 必须为 0(auto) 或 >=2 (禁止 1 硬编码)
    CHECK(cfg.exec.cpu_workers == 0 || cfg.exec.cpu_workers >= 2);
  }

  // 3) cpu_workers 值域: 0..1024 校验; 键为 execution (非 exec), 需 output.hips
  {
    P2Stage2Config cfg{};
    std::string err;
    nlohmann::json base = {
        {"inputs", {{"hips", {"/tmp/in1.hips", "/tmp/in2.hips"}}}},
        {"output", {{"hips", "/tmp/out.hips"}}}};
    // cpu_workers=-1 拒绝
    nlohmann::json j = base;
    j["execution"] = {{"cpu_workers", -1}};
    bool ok = p2_stage2_parse_config(j, &cfg, &err);
    if (ok) std::fprintf(stderr, "w-1 accepted?!\n");
    CHECK(!ok);  // 负值拒绝
    // cpu_workers=2 接受
    j = base;
    j["execution"] = {{"cpu_workers", 2}};
    ok = p2_stage2_parse_config(j, &cfg, &err);
    if (!ok) std::fprintf(stderr, "w2 err: %s\n", err.c_str());
    CHECK(ok);   // 2 接受
    CHECK(cfg.exec.cpu_workers == 2);
    // 显式 1: 允许解析 (测试参考用), 但生产 heavy gate 在运行时拒 (见 #1)
    j = base;
    j["execution"] = {{"cpu_workers", 1}};
    ok = p2_stage2_parse_config(j, &cfg, &err);
    CHECK(ok);
    CHECK(cfg.exec.cpu_workers == 1);
  }

  // 4) 确定性前提 + M5a-G-001 冻结值回归锁(§18.2: 85%/60%; 旧 D.6 的
  //    80%/75%/50% 已废止, 不得回归)。
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.selected_workers = 4;
    g.available_cpus = 2;
    const double t1 = compute_cores_threshold(g);
    const double t2 = compute_cores_threshold(g);
    CHECK(t1 == t2);                 // 确定性
    CHECK(t1 == 0.85 * 2.0);         // 0.85*min(4,2) = 1.7
    CHECK(astrocs::kCpuMeanMinPercent == 85.0);
    CHECK(astrocs::kMon001UtilSampleMinPercent == 85.0);
    CHECK(astrocs::kMon001QueueUtilMinPercent == 60.0);
  }

  // 4b) M5a-G-001 回归锁 case_avg_085_boundary: 已分配容量 4 → 核下限 3.4;
  //     3.39 必须 FAIL(low_avg_cores), 3.4 恰好通过(边界含等号)。
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
    g.wall_seconds = 30.0; g.has_stage_annotation = true; g.workers_p50 = 4.0;
    g.cpu_percent = 90.0; g.iowait_percent = 1.0; g.mem_bandwidth_percent = 90.0;
    g.cpu_p50_percent = 95.0; g.cpu_mean_percent = 90.0;  // 已归一容量百分比
    CHECK(std::fabs(compute_cores_threshold(g) - 3.4) < 1e-9);
    g.avg_equivalent_cores = 3.39;
    CHECK(evaluate_gate(g) == GateDiag::LowAvgCores);
    g.avg_equivalent_cores = 3.4;
    CHECK(evaluate_gate(g) == GateDiag::Ok);
  }

  // 4c) M5a-G-002 回归锁 case_alloc4_observed_0p9_core_fails: 已分配 4 核、
  //     实测 0.9 等效核(= 采集端 percent_of_one_core 90.0)必须 FAIL; 修复前
  //     90.0 被直接当成"90%"绕过 85% 均值门(分母退化 1 核)。
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
    g.granted_workers = 4;                 // 已分配容量 = 4 核(观测权威)
    g.wall_seconds = 30.0; g.has_stage_annotation = true; g.workers_p50 = 4.0;
    const double mean_pct = astrocs::cpu_percent_of_allocated_capacity(g, 90.0);
    CHECK(std::fabs(mean_pct - 22.5) < 1e-9);                 // 90/4 = 22.5% 容量
    CHECK(std::fabs(astrocs::utilization_value(g, 90.0) - 0.225) < 1e-9);
    CHECK(astrocs::allocated_capacity_cores(g) == 4);
    g.avg_equivalent_cores = 0.9;                             // 远低于 0.85*4
    g.cpu_mean_percent = mean_pct;
    g.cpu_p50_percent = astrocs::cpu_percent_of_allocated_capacity(g, 380.0);
    CHECK(evaluate_gate(g) != GateDiag::Ok);                  // 必须 FAIL
  }

  // 4d) M5a-G-002 均值门归一回归锁: avg 核数达标也不得掩盖"容量占比不足"。
  //     分配 4 核、实测 2.0 核(=50% 容量)、avg_equivalent_cores=3.6 →
  //     §18.2 均值门仍必须 FAIL(cpu_mean_low); 3.6 核(=90% 容量)则通过。
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.available_cpus = 4; g.selected_workers = 4; g.max_active_threads = 4;
    g.granted_workers = 4;
    g.wall_seconds = 30.0; g.has_stage_annotation = true; g.workers_p50 = 4.0;
    g.cpu_percent = 90.0; g.iowait_percent = 1.0; g.mem_bandwidth_percent = 90.0;
    g.avg_equivalent_cores = 3.6;
    g.cpu_p50_percent = astrocs::cpu_percent_of_allocated_capacity(g, 380.0);  // 95%
    g.cpu_mean_percent = astrocs::cpu_percent_of_allocated_capacity(g, 200.0); // 50%
    CHECK(evaluate_gate(g) == GateDiag::CpuMeanLow);
    g.cpu_mean_percent = astrocs::cpu_percent_of_allocated_capacity(g, 360.0); // 90%
    CHECK(evaluate_gate(g) == GateDiag::Ok);
  }

  if (failures == 0) {
    std::printf("P2-002 TESTS PASS (heavy 禁 workers=1 门禁, cpu_workers 传递/值域, 确定性阈值)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
