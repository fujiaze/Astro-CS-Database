// RT-008 单元测试: CLI Runtime client（preset→IR→Runtime 唯一路径）
#include "runtime_client.h"

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

static void test_ir_builder_3phase() {
  std::string err;
  std::string cfg = R"({"output_dir":"/tmp","inputs":{"lights":["a.fits"]},"phase3":{"output_fits_path":"/tmp/o.fits"}})";
  std::string ir = astrocs::cli::build_pipeline_ir({1, 2, 3}, cfg, &err);
  CHECK(!ir.empty());
  CHECK(err.empty());
  // 三个 phase node + outputs; P2-006 (G5): phase2 为 7 节点链(coverage..write)
  CHECK(ir.find("\"cal\"") != std::string::npos);
  CHECK(ir.find("\"coverage\"") != std::string::npos);
  CHECK(ir.find("\"write\"") != std::string::npos);
  CHECK(ir.find("\"hips\"") != std::string::npos);
  CHECK(ir.find("\"res\"") == std::string::npos);  // 旧单节点 res 已移除
}

static void test_ir_builder_single_phase() {
  std::string err;
  std::string cfg = R"({"output_dir":"/tmp","inputs":{"lights":["a.fits"]}})";
  // 单 phase 命令是同一 IR 子图，不是第二条路径
  std::string ir1 = astrocs::cli::build_pipeline_ir({1}, cfg, &err);
  CHECK(!ir1.empty());
  CHECK(ir1.find("\"cal\"") != std::string::npos);
  CHECK(ir1.find("\"res\"") == std::string::npos);  // 无 phase2
  std::string ir3 = astrocs::cli::build_pipeline_ir({3}, cfg, &err);
  CHECK(ir3.empty());  // 缺 phase3 对象 → 失败（与 run config 合同一致）
  std::string cfg3 = R"({"output_dir":"/tmp","phase3":{"output_fits_path":"/tmp/o.fits"}})";
  std::string ir3b = astrocs::cli::build_pipeline_ir({3}, cfg3, &err);
  CHECK(!ir3b.empty());
  CHECK(ir3b.find("\"hips\"") != std::string::npos);
  CHECK(ir3b.find("\"cal\"") == std::string::npos);
}

static void test_ir_builder_missing_config() {
  std::string err;
  std::string bad = R"({"output_dir":"/tmp"})";  // 缺 inputs.lights
  std::string ir = astrocs::cli::build_pipeline_ir({1}, bad, &err);
  CHECK(ir.empty());
  CHECK(!err.empty());
}

static void test_register_modules() {
  astrocs::core::ModuleRegistry reg;
  auto r = astrocs::cli::register_cli_modules(reg);
  CHECK(r.ok());
  CHECK(reg.size() == 22);  // P1-001: 8 类 Phase1 + Phase2/3 + P2-006: 7 节点链
}

static void test_run_pipeline_validation_error() {
  // 非法 IR（模块缺失输入）→ 静态验证失败 → exit 4
  std::string fr;
  std::string bad = R"({"output_dir":"/tmp"})";  // 缺 inputs → IR 构建失败 → exit 2
  int rc = astrocs::cli::run_pipeline({1}, bad, 2, &fr);
  CHECK(rc == 2);
}

int main() {
  test_ir_builder_3phase();
  test_ir_builder_single_phase();
  test_ir_builder_missing_config();
  test_register_modules();
  test_run_pipeline_validation_error();
  if (failures == 0) {
    std::printf("RT-008_CLIENT_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "RT-008_CLIENT_FAIL failures=%d\n", failures);
  return 1;
}
