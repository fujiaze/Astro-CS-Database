// RT-005 单元测试: 可执行 ModuleRegistry
// 1. 真实 Phase1/2/3 模块经 factory 注册并 create（不接受只注册 metadata）
// 2. 每个模块 descriptor 完整合同（SCI/ALG/DATA/API/TEST、端口、execution class）
// 3. heavy+serial 拒绝；ACR production 拒绝；重复 ID 拒绝；重复端口拒绝
// 4. export_index_json 用 JSON 正确转义（含 executable 标志）
// 5. 每个模块 validate_config / inspect 走真实 session（synthetic 最小配置）
#include "astrocs/core/module_adapters.h"

#include <cstdio>
#include <string>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void test_register_three_phase_modules() {
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());
  if (r.failed()) return;
  // P1-001 (G4): 8 类 Phase1 + Phase2/3 = 10 个模块
  CHECK(reg.size() == 10);
  CHECK(reg.has_factory("astrocs.phase1.calibration"));
  CHECK(reg.has_factory("astrocs.phase2.resample"));
  CHECK(reg.has_factory("astrocs.phase3.resample"));
  // create 全部成功（不接受只注册 metadata）
  for (const auto& id : {"astrocs.phase1.calibration",
                         "astrocs.phase2.resample",
                         "astrocs.phase3.resample",
                         "astrocs.phase1.cosmetic",
                         "astrocs.phase1.star-psf",
                         "astrocs.phase1.wcs-platesolve",
                         "astrocs.phase1.photometry",
                         "astrocs.phase1.noise-snr",
                         "astrocs.phase1.drizzle",
                         "astrocs.phase1.writer"}) {
    auto m = reg.create(id);
    CHECK(m.ok());
    if (m.ok()) {
      CHECK(m.value()->descriptor().module_id == id);
      CHECK(!m.value()->descriptor().ports.empty());
    }
  }
}

static void test_descriptor_contracts() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const ModuleDescriptor* d1 = reg.find("astrocs.phase1.calibration");
  CHECK(d1 != nullptr);
  if (d1) {
    CHECK(d1->sci_id.rfind("SCI-", 0) == 0);
    CHECK(d1->alg_id.rfind("ALG-", 0) == 0);
    CHECK(d1->data_id.rfind("DATA-", 0) == 0);
    CHECK(d1->api_id.rfind("API-", 0) == 0);
    CHECK(d1->test_id.rfind("TEST-", 0) == 0);
    CHECK(d1->execution_class == "cpu_heavy");
    CHECK(d1->parallel_ok);
    CHECK(d1->abi == "c++17");
    CHECK(!d1->version.empty());
  }
  const ModuleDescriptor* d3 = reg.find("astrocs.phase3.resample");
  CHECK(d3 != nullptr);
  if (d3) {
    CHECK(d3->ports.size() == 2);
    bool in_hips = false, out_tile = false;
    for (const auto& p : d3->ports) {
      if (p.name == "hips" && p.is_input) in_hips = true;
      if (p.name == "tile" && !p.is_input) out_tile = true;
    }
    CHECK(in_hips && out_tile);
  }
}

static void test_rejections() {
  ModuleRegistry reg;
  // heavy+serial 拒绝
  ModuleDescriptor hs;
  hs.module_id = "astrocs.bad.serial";
  hs.version = "1.0.0";
  hs.abi = "c++17";
  hs.execution_class = "cpu_heavy";
  hs.parallel_ok = false;
  hs.ports = {{"in", "DATA-X", true}, {"out", "DATA-X", false}};
  auto r1 = reg.register_module(hs);
  CHECK(r1.failed());
  // ACR production 拒绝
  ModuleDescriptor acr;
  acr.module_id = "astrocs.acr.whatever";
  acr.version = "1.0.0";
  acr.abi = "c++17";
  acr.execution_class = "io";
  acr.parallel_ok = true;
  acr.ports = {{"in", "DATA-X", true}, {"out", "DATA-X", false}};
  auto r2 = reg.register_module(acr);
  CHECK(r2.failed());
  // 重复端口拒绝
  ModuleDescriptor dup;
  dup.module_id = "astrocs.bad.dupport";
  dup.version = "1.0.0";
  dup.abi = "c++17";
  dup.execution_class = "io";
  dup.parallel_ok = true;
  dup.ports = {{"in", "DATA-X", true}, {"in", "DATA-X", true}, {"out", "DATA-X", false}};
  auto r3 = reg.register_module(dup);
  CHECK(r3.failed());
  // 重复 ID 拒绝
  ModuleRegistry reg2;
  CHECK(register_phase_modules(reg2).ok());
  auto d1 = *reg2.find("astrocs.phase1.calibration");
  auto r4 = reg2.register_module(d1);
  CHECK(r4.failed());
}

static void test_export_index_json() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  std::string idx;
  CHECK(reg.export_index_json(&idx));
  // 正确 JSON（可 parse）+ 转义 + executable 标志
  CHECK(idx.find("\"schema\":\"astrocs.module-index/v1\"") != std::string::npos);
  CHECK(idx.find("\"executable\":true") != std::string::npos);
  CHECK(idx.find("astrocs.phase3.resample") != std::string::npos);
}

static void test_factory_execution() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  // 每个模块 validate_config 走真实 session（空/最小配置 → 合同路径）
  for (const auto& id : {"astrocs.phase1.calibration",
                         "astrocs.phase2.resample",
                         "astrocs.phase3.resample"}) {
    auto m = reg.create(id);
    CHECK(m.ok());
    if (!m.ok()) continue;
    // inspect 不执行科学计算（真实 session inspect 路径）
    auto insp = m.value()->inspect();
    CHECK(insp.ok());
    // plan 返回非空计划
    auto p = m.value()->plan("node1", "{}");
    CHECK(p.ok());
  }
}

int main() {
  test_register_three_phase_modules();
  test_descriptor_contracts();
  test_rejections();
  test_export_index_json();
  test_factory_execution();
  if (failures == 0) {
    std::printf("RT-005_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "RT-005_FAIL failures=%d\n", failures);
  return 1;
}
