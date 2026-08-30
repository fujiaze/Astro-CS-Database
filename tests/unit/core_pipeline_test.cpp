// CORE-004 单元测试: Pipeline IR 解析 + 静态验证 (含控制包 fixtures)
#include "astrocs/core/pipeline.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
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

static std::string read_file(const std::string& path) {
  std::ifstream f(path);
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

static const char* VALID = R"({
  "schema": "astrocs.pipeline/v1",
  "pipeline_id": "phase3.synthetic",
  "version": "1.0.0",
  "nodes": [{
    "node_id": "resample",
    "module_id": "astrocs.phase3.resample",
    "module_api": "1.x",
    "config": {"kernel": "bilinear"},
    "inputs": {"hips": "artifact:input.hips"},
    "outputs": {"tile": "artifact:phase3.tile"},
    "resources": {"class": "cpu_heavy", "parallel": true}
  }],
  "outputs": {"tile": "artifact:phase3.tile"}
})";

static void test_parse_valid() {
  PipelineIRParser parser;
  auto r = parser.parse(VALID);
  CHECK(r.ok());
  if (r.ok()) {
    CHECK(r.value().pipeline_id == "phase3.synthetic");
    CHECK(r.value().nodes.size() == 1);
    CHECK(r.value().nodes[0].module_id == "astrocs.phase3.resample");
    CHECK(r.value().nodes[0].resource_class == "cpu_heavy");
    CHECK(r.value().nodes[0].parallel);
  }
}

static void test_parse_serial_heavy_rejected() {
  // 控制包 invalid fixture: cpu_heavy + parallel=false 必须被拒
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1",
    "pipeline_id": "bad",
    "version": "1.0.0",
    "nodes": [{"node_id": "b", "module_id": "astrocs.m",
      "module_api": "1.x", "config": {},
      "inputs": {"a": "artifact:x"},
      "outputs": {"b": "artifact:y"},
      "resources": {"class": "cpu_heavy", "parallel": false}}],
    "outputs": {"b": "artifact:y"}
  })");
  CHECK(r.failed());
  CHECK(r.error().message().find("parallel") != std::string::npos);
}

static void test_parse_bad_schema() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({"schema":"wrong","pipeline_id":"x","version":"1"})");
  CHECK(r.failed());
}

static void test_validate_unknown_module() {
  PipelineIRParser parser;
  auto r = parser.parse(VALID);
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), {"astrocs.other"});
  CHECK(issues.size() == 1);
  CHECK(issues[0].kind == IrError::UNKNOWN_MODULE);
  auto ok_issues = parser.validate(r.value(), {"astrocs.phase3.resample"});
  CHECK(ok_issues.empty());
}

static void test_validate_cycle() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1",
    "pipeline_id": "cycle",
    "version": "1.0.0",
    "nodes": [
      {"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:ba"}, "outputs": {"out": "artifact:ab"},
       "resources": {"class": "io", "parallel": true}},
      {"node_id": "b", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:ab"}, "outputs": {"out": "artifact:ba"},
       "resources": {"class": "io", "parallel": true}}
    ],
    "outputs": {"out": "artifact:ab"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), {"astrocs.m"});
  bool has_cycle = false;
  for (const auto& i : issues) if (i.kind == IrError::CYCLE) has_cycle = true;
  CHECK(has_cycle);
}

static void test_validate_duplicate_producer() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1",
    "pipeline_id": "dup",
    "version": "1.0.0",
    "nodes": [
      {"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:x"}, "outputs": {"out": "artifact:same"},
       "resources": {"class": "io", "parallel": true}},
      {"node_id": "b", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:y"}, "outputs": {"out": "artifact:same"},
       "resources": {"class": "io", "parallel": true}}
    ],
    "outputs": {"out": "artifact:same"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), {"astrocs.m"});
  bool has_dup = false;
  for (const auto& i : issues) if (i.kind == IrError::DUPLICATE_PRODUCER) has_dup = true;
  CHECK(has_dup);
}

static void test_control_package_fixtures() {
  // 控制包 valid/invalid fixtures 直接校验
  PipelineIRParser parser;
  // 测试运行 cwd 为 build 目录; 用源码树绝对路径
  std::string base = std::string(std::getenv("ASTROCS_REPO") ? std::getenv("ASTROCS_REPO") : "../..")
      + "/工程控制/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/fixtures/";
  auto v = parser.parse(read_file(std::string(base) + "valid_pipeline.json"));
  CHECK(v.ok());
  if (v.ok()) {
    auto issues = parser.validate(v.value(), {"astrocs.phase3.resample"});
    CHECK(issues.empty());
  }
  auto inv = parser.parse(read_file(std::string(base) + "invalid_pipeline_serial_heavy.json"));
  CHECK(inv.failed());  // cpu_heavy serial 必须拒
}

int main() {
  test_parse_valid();
  test_parse_serial_heavy_rejected();
  test_parse_bad_schema();
  test_validate_unknown_module();
  test_validate_cycle();
  test_validate_duplicate_producer();
  test_control_package_fixtures();
  if (failures == 0) {
    std::printf("CORE-004 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
