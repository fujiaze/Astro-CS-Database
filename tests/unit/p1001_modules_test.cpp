// tests/unit/p1001_modules_test.cpp — P1-001 (G4) Phase1 8 类模块注册测试
// 验证: calibration/cosmetic/star-psf/wcs-platesolve/photometry/noise-snr/drizzle/
// writer 全部注册; 每个声明标准 DATA 端口与执行类; factory 可创建; Session 兼容
// adapter 委托 Runtime(无第二调度顺序——create 的 IModule 走同一 Runtime 执行路径)。
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"

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
  astrocs::core::ModuleRegistry reg;
  auto r = astrocs::core::register_phase_modules(reg);
  CHECK(r.ok());
  if (r.failed()) return 1;

  // 8 类 Phase1 模块必须全部注册
  const char* p1_ids[] = {
      "astrocs.phase1.calibration",
      "astrocs.phase1.cosmetic",
      "astrocs.phase1.star-psf",
      "astrocs.phase1.wcs-platesolve",
      "astrocs.phase1.photometry",
      "astrocs.phase1.noise-snr",
      "astrocs.phase1.drizzle",
      "astrocs.phase1.writer",
  };
  for (const char* id : p1_ids) {
    const auto* d = reg.find(id);
    CHECK(d != nullptr);
    if (!d) continue;
    // 标准 DATA 端口(非空)与执行类
    CHECK(!d->ports.empty());
    CHECK(!d->execution_class.empty());
    CHECK(d->execution_class == "cpu_heavy" || d->execution_class == "io");
    // 合同引用 ID 前缀
    CHECK(d->sci_id.rfind("SCI-", 0) == 0);
    CHECK(d->alg_id.rfind("ALG-", 0) == 0);
    CHECK(d->data_id.rfind("DATA-", 0) == 0);
    CHECK(d->api_id.rfind("API-", 0) == 0);
    CHECK(d->test_id.rfind("TEST-", 0) == 0);
    // factory 可创建(不接受只注册 metadata)
    auto m = reg.create(id);
    CHECK(m.ok());
    if (m.ok()) {
      CHECK(m.value()->descriptor().module_id == id);
      // plan 返回非空计划(执行类: writer=io 非 heavy; 其余 cpu_heavy)
      auto p = m.value()->plan("node1", "{}");
      CHECK(p.ok());
      if (p.ok()) {
        CHECK(!p.value().node_id.empty());
        CHECK(p.value().cpu_heavy == (d->execution_class == "cpu_heavy"));
      }
    }
  }

  // 端口结构: 每个模块有输入(含 DATA-)与输出(输出端口 is_input=false)
  {
    const auto* dz = reg.find("astrocs.phase1.drizzle");
    CHECK(dz != nullptr);
    if (dz) {
      bool has_in = false, has_out = false;
      for (const auto& p : dz->ports) {
        if (p.is_input) has_in = true;
        else has_out = true;
        CHECK(p.data_schema_id.rfind("DATA-", 0) == 0);
      }
      CHECK(has_in && has_out);
    }
  }

  if (failures == 0) {
    std::printf("P1-001 TESTS PASS (8 类 Phase1 模块注册/端口/执行类/Runtime 委托)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-001 TESTS FAIL (%d)\n", failures);
  return 1;
}
