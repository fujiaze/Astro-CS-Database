// P2-002 单元测试: 生产禁 workers=1 + heavy 门禁 + cpu_workers 传递确定性
#include "resource_gate.h"
#include "astro/phase2/stage2_common.h"

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

  // 4) 确定性前提: gate 阈值公式确定 (同输入同结果)
  {
    GateConfig g;
    g.kind = ResKind::Compute;
    g.selected_workers = 4;
    g.available_cpus = 2;
    const double t1 = compute_cores_threshold(g);
    const double t2 = compute_cores_threshold(g);
    CHECK(t1 == t2);                 // 确定性
    CHECK(t1 == 0.80 * 2.0);         // 0.80*min(4,2)
  }

  if (failures == 0) {
    std::printf("P2-002 TESTS PASS (heavy 禁 workers=1 门禁, cpu_workers 传递/值域, 确定性阈值)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
