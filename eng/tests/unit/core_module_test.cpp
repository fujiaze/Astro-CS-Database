// CORE-003 单元测试: ModuleRegistry duplicate/ABI/合同校验
#include "astrocs/core/module.h"

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

static ModuleDescriptor make_module() {
  ModuleDescriptor m;
  m.module_id = "astrocs.phase1.calibration";
  m.version = "1.0";
  m.abi = "c++17";
  m.execution_class = "cpu_heavy";
  m.parallel_ok = true;
  m.sci_id = "SCI-CAL-001";
  m.test_id = "TEST-CAL-001";
  PortDescriptor in;
  in.name = "raw"; in.data_schema_id = "DATA-IMG-RAW-001"; in.is_input = true;
  PortDescriptor out;
  out.name = "image"; out.data_schema_id = "DATA-IMG-CAL-001"; out.is_input = false;
  m.ports = {in, out};
  return m;
}

static void test_register_ok() {
  ModuleRegistry reg;
  auto r = reg.register_module(make_module());
  CHECK(r.ok());
  CHECK(reg.size() == 1);
}

static void test_duplicate_rejected() {
  ModuleRegistry reg;
  CHECK(reg.register_module(make_module()).ok());
  auto r2 = reg.register_module(make_module());
  CHECK(r2.failed());
  CHECK(r2.error().message().find("duplicate") != std::string::npos);
  CHECK(reg.size() == 1);
}

static void test_invalid_module() {
  ModuleRegistry reg;
  ModuleDescriptor m = make_module();
  m.module_id = "not-namespaced";
  auto r = reg.register_module(m);
  CHECK(r.failed());
  CHECK(r.error().domain() == ErrorDomain::DATA);
  ModuleDescriptor m2 = make_module();
  m2.abi = "rust";
  CHECK(reg.register_module(m2).failed());
  ModuleDescriptor m3 = make_module();
  m3.ports.pop_back();  // 只有 input
  CHECK(reg.register_module(m3).failed());
}

static void test_find_and_export() {
  ModuleRegistry reg;
  reg.register_module(make_module());
  const ModuleDescriptor* f = reg.find("astrocs.phase1.calibration");
  CHECK(f != nullptr);
  CHECK(f->sci_id == "SCI-CAL-001");
  CHECK(reg.find("nope") == nullptr);
  std::string idx;
  CHECK(reg.export_index_json(&idx));
  CHECK(idx.find("astrocs.module-index/v1") != std::string::npos);
  CHECK(idx.find("astrocs.phase1.calibration") != std::string::npos);
}

int main() {
  test_register_ok();
  test_duplicate_rejected();
  test_invalid_module();
  test_find_and_export();
  if (failures == 0) {
    std::printf("CORE-003 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-003 TESTS FAIL (%d)\n", failures);
  return 1;
}
