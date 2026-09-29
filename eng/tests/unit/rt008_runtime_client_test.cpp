// RT-008 单元测试: CLI Runtime client（preset→IR→Runtime 唯一路径）
#include "runtime_client.h"

#include "exit_codes.h"   // P-158: 退出码语义唯一源（断言用数值只在本测试内字面）

#include <cstdio>
#include <cstdlib>
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
  // preset IR 构建失败（缺 inputs）→ exit 2（尚未进入 Runtime）
  std::string fr;
  std::string bad = R"({"output_dir":"/tmp"})";  // 缺 inputs → IR 构建失败 → exit 2
  int rc = astrocs::cli::run_pipeline({1}, bad, 2, &fr);
  CHECK(rc == 2);
}

// ───────────────────────── P-158: load_pipeline 失败的退出码 ─────────────────────────
// 台账 P-158：load_pipeline 失败处**恒定 return 4**（旧 runtime_client.cpp），而上游
// 解析/静态验证错误一律 ErrorDomain::DATA（scheduler/src/pipeline.cpp:55-189 与
// runtime.cpp:133-149）⇒ DATA 域被误判成 SCIENCE_PRECONDITION(4)。
// 权威：docs/engineering/LOG_AND_ERROR_CONTRACT.md §5（DATA→2、SCIENCE_PRECONDITION→4）
//       + lib/infrastructure/cli/exit_codes.h（码值语义唯一源）。
// 判据必须非退化：DATA 域必须给 2，且不得是 4。

// 非法 IR：模块未注册 ⇒ pipeline.cpp validate 报 UNKNOWN_MODULE，域 = DATA。
static const char* p158_illegal_ir() {
  return R"({"schema":"astrocs.pipeline/v1","pipeline_id":"p158","version":"1.0.0",)"
         R"("nodes":[{"node_id":"n0","module_id":"astrocs.no.such.module","module_api":"1.x",)"
         R"("inputs":{"in":"artifact:a"},"outputs":{"out":"artifact:b"},)"
         R"("resources":{"class":"cpu_heavy","parallel":true}}],"outputs":{"o":"artifact:b"}})";
}

static void set_test_env(const char* key, const char* value) {
#if defined(_WIN32)
  _putenv_s(key, value);
#else
  setenv(key, value, 1);
#endif
}

// (a) 上游真身：Runtime::load_pipeline 对非法 IR 返回 DATA 域错误；该域映射 = 2。
static void test_p158_upstream_load_error_domain_is_data_and_maps_to_2() {
  astrocs::core::ModuleRegistry reg;
  auto rr = astrocs::cli::register_cli_modules(reg);
  CHECK(rr.ok());
  auto rt = astrocs::core::create_runtime(2u);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(p158_illegal_ir(), reg);
  CHECK(load.failed());
  CHECK(load.error().domain() == astrocs::core::ErrorDomain::DATA);
  CHECK(astrocs::cli::exit_code_for_error_domain(load.error().domain()) == 2);
  CHECK(astrocs::cli::exit_code_for_error_domain(load.error().domain()) != 4);
}

// (b) 全路径：run_pipeline 走到 load_pipeline 失败 ⇒ 退出码 2（旧实现恒 4 ⇒ 判红）。
static void test_p158_run_pipeline_load_failure_exit_code_is_2() {
  set_test_env("ASTROCS_TEST_PIPELINE_IR", p158_illegal_ir());
  std::string fr;
  const std::string cfg = R"({"output_dir":"/tmp","inputs":{"lights":["a.fits"]}})";
  const int rc = astrocs::cli::run_pipeline({1}, cfg, 2u, &fr);
  set_test_env("ASTROCS_TEST_PIPELINE_IR", "");
  CHECK(rc == 2);         // §5: DATA → 2(ARGS)
  CHECK(rc != 4);         // 不得是 SCIENCE_PRECONDITION(4)
  CHECK(!fr.empty());     // 失败原因必须上行（不静默）
}

// (c) §5 域→码全表（唯一映射实现 exit_code_for_error_domain）。
static void test_p158_domain_matrix_matches_contract_section5() {
  using D = astrocs::core::ErrorDomain;
  CHECK(astrocs::cli::exit_code_for_error_domain(D::CONFIG) == 2);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::DATA) == 2);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::SCIENCE_PRECONDITION) == 4);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::BACKEND) == 5);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::IO) == 7);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::RESOURCE) == 10);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::CANCELLED) == 9);
  CHECK(astrocs::cli::exit_code_for_error_domain(D::INTERNAL) == 70);
}


// F-EXIT-MAP 可达矩阵：强制每个 ErrorDomain 走 run_pipeline 的**实际**退出码路径
// （ASTROCS_TEST_FORCE_ERROR_DOMAIN 钩子），期望值逐条硬编码 =
// docs/engineering/LOG_AND_ERROR_CONTRACT.md §5「域 → 码」表（独立 oracle；
// 不复用 exit_code_for_error_domain ⇒ 实现漂移必被抓住）。
static void test_f_exit_map_domain_matrix_is_reachable() {
  struct Row { const char* name; int want; };
  const Row rows[] = {
      {"CONFIG", 2}, {"DATA", 2}, {"SCIENCE_PRECONDITION", 4}, {"BACKEND", 5},
      {"IO", 7}, {"RESOURCE", 10}, {"CANCELLED", 9}, {"INTERNAL", 70},
  };
  const std::string cfg = R"({"output_dir":"/tmp","inputs":{"lights":["a.fits"]}})";
  for (const Row& r : rows) {
    set_test_env("ASTROCS_TEST_FORCE_ERROR_DOMAIN", r.name);
    std::string fr;
    const int rc = astrocs::cli::run_pipeline({1}, cfg, 2u, &fr);
    set_test_env("ASTROCS_TEST_FORCE_ERROR_DOMAIN", "");
    if (rc != r.want) {
      std::fprintf(stderr, "F-EXIT-MAP %s: rc=%d want=%d (LOG_AND_ERROR_CONTRACT §5)\n",
                   r.name, rc, r.want);
      ++failures;
    }
    if (fr.empty()) {
      std::fprintf(stderr, "F-EXIT-MAP %s: fail_reason 未上行\n", r.name);
      ++failures;
    }
  }
  // 未知域名必须 fail-closed 到 70（不得静默当成功）
  set_test_env("ASTROCS_TEST_FORCE_ERROR_DOMAIN", "NO_SUCH_DOMAIN");
  std::string fr2;
  const int rc2 = astrocs::cli::run_pipeline({1}, cfg, 2u, &fr2);
  set_test_env("ASTROCS_TEST_FORCE_ERROR_DOMAIN", "");
  CHECK(rc2 == 70);
}

int main() {
  test_ir_builder_3phase();
  test_ir_builder_single_phase();
  test_ir_builder_missing_config();
  test_register_modules();
  test_run_pipeline_validation_error();
  test_p158_upstream_load_error_domain_is_data_and_maps_to_2();
  test_p158_run_pipeline_load_failure_exit_code_is_2();
  test_p158_domain_matrix_matches_contract_section5();
  test_f_exit_map_domain_matrix_is_reachable();
  if (failures == 0) {
    std::printf("RT-008_CLIENT_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "RT-008_CLIENT_FAIL failures=%d\n", failures);
  return 1;
}
