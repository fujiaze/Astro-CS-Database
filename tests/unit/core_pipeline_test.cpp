// CORE-004 / RT-004 单元测试: Pipeline IR 解析 + 静态验证 (nlohmann + ModuleRegistry)
#include "astrocs/core/pipeline.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// ── Windows 窄路径编码墙 ──
// 编译期: 根 CMakeLists 的 /utf-8 (WIN-001) 已保证中文字面量为 UTF-8 字节;
// 运行期: Windows 窄字符 std::ifstream 经 CreateFileA 按 ACP (简中系统 GBK)
// 解释路径字节 — UTF-8 字节被再误解为 GBK → 打不开磁盘上真实存在 (Git
// checkout 为 UTF-8 名) 的 "工程控制" 目录 → 探测全空 → CHECK 失败。
// 修法: 把 UTF-8 字节显式转 UTF-16, 经 fs::path(宽) / CreateFileW 打开;
// 非 Windows 路径语义不变 (fopen 按 UTF-8 字节, 本就正确)。
#if defined(_WIN32)
static std::filesystem::path utf8_path(const std::string& u8) {
  if (u8.empty()) return std::filesystem::path();
  const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(),
                                    static_cast<int>(u8.size()), nullptr, 0);
  std::wstring wide(n > 0 ? n : 0, L'\0');
  if (n > 0) {
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), static_cast<int>(u8.size()),
                        &wide[0], n);
  }
  return std::filesystem::path(wide);
}
#else
static std::filesystem::path utf8_path(const std::string& u8) {
  return std::filesystem::path(u8);
}
#endif

static std::string read_file(const std::string& path) {
  std::ifstream f(utf8_path(path));
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

static ModuleRegistry make_registry() {
  ModuleRegistry reg;
  // resample: cpu_heavy, 输入 hips(PIXEL,ADU), 输出 tile(HEALPIX,SURFACE_BRIGHTNESS)
  ModuleDescriptor m1;
  m1.module_id = "astrocs.phase3.resample";
  m1.version = "1.0.0";
  m1.abi = "c++17";
  m1.execution_class = "cpu_heavy";
  m1.parallel_ok = true;
  m1.ports = {
      {"hips", "DATA-HIPS-001", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"tile", "DATA-TILE-001", false, UnitId::SURFACE_BRIGHTNESS, CoordinateFrame::HEALPIX},
  };
  reg.register_module(m1);
  // m: 通用 io 模块, in/out 端口
  ModuleDescriptor m2;
  m2.module_id = "astrocs.m";
  m2.version = "1.0.0";
  m2.abi = "c++17";
  m2.execution_class = "io";
  m2.ports = {
      {"in", "DATA-X-001", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"out", "DATA-X-001", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  reg.register_module(m2);
  // m_elec: 输入端口要求 ELECTRON（消费 ADU 产物 → 单位冲突）
  ModuleDescriptor m3;
  m3.module_id = "astrocs.m_elec";
  m3.version = "1.0.0";
  m3.abi = "c++17";
  m3.execution_class = "io";
  m3.ports = {
      {"in", "DATA-X-001", true, UnitId::ELECTRON, CoordinateFrame::PIXEL},
      {"out", "DATA-X-001", false, UnitId::ELECTRON, CoordinateFrame::PIXEL},
  };
  reg.register_module(m3);
  // m_healpix: 输入端口要求 HEALPIX 坐标系（消费 PIXEL 产物 → 坐标冲突）
  ModuleDescriptor m4;
  m4.module_id = "astrocs.m_healpix";
  m4.version = "1.0.0";
  m4.abi = "c++17";
  m4.execution_class = "io";
  m4.ports = {
      {"in", "DATA-X-001", true, UnitId::ADU, CoordinateFrame::HEALPIX},
      {"out", "DATA-X-001", false, UnitId::ADU, CoordinateFrame::HEALPIX},
  };
  reg.register_module(m4);
  // m_data: 输入端口要求 DATA-Y-999 schema（消费 DATA-X-001 产物 → schema 冲突）
  ModuleDescriptor m5;
  m5.module_id = "astrocs.m_data";
  m5.version = "1.0.0";
  m5.abi = "c++17";
  m5.execution_class = "io";
  m5.ports = {
      {"in", "DATA-Y-999", true, UnitId::ADU, CoordinateFrame::PIXEL},
      {"out", "DATA-Y-999", false, UnitId::ADU, CoordinateFrame::PIXEL},
  };
  reg.register_module(m5);
  return reg;
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
    CHECK(r.value().nodes[0].config_json.find("bilinear") != std::string::npos);
  }
}

static void test_parse_serial_heavy_rejected() {
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
  // 非 JSON
  auto r2 = parser.parse("not json {");
  CHECK(r2.failed());
  // 缺 nodes
  auto r3 = parser.parse(R"({"schema":"astrocs.pipeline/v1","pipeline_id":"x","version":"1"})");
  CHECK(r3.failed());
}

static void test_validate_unknown_module() {
  PipelineIRParser parser;
  auto r = parser.parse(VALID);
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  CHECK(issues.empty());
  // 未注册模块
  auto r2 = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "x", "version": "1",
    "nodes": [{"node_id": "a", "module_id": "astrocs.other", "module_api": "1.x", "config": {},
      "inputs": {"in": "artifact:x"}, "outputs": {"out": "artifact:y"},
      "resources": {"class": "io", "parallel": true}}],
    "outputs": {"out": "artifact:y"}})");
  auto issues2 = parser.validate(r2.value(), make_registry());
  CHECK(issues2.size() >= 1);
  bool has_unknown = false;
  for (const auto& i : issues2) if (i.kind == IrError::UNKNOWN_MODULE) has_unknown = true;
  CHECK(has_unknown);
}

static void test_validate_missing_port() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "x", "version": "1",
    "nodes": [{"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
      "inputs": {"wrong_port": "artifact:x"}, "outputs": {"out": "artifact:y"},
      "resources": {"class": "io", "parallel": true}}],
    "outputs": {"out": "artifact:y"}})");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  bool has = false;
  for (const auto& i : issues) if (i.kind == IrError::MISSING_PORT) has = true;
  CHECK(has);
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
  auto issues = parser.validate(r.value(), make_registry());
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
  auto issues = parser.validate(r.value(), make_registry());
  bool has_dup = false;
  for (const auto& i : issues) if (i.kind == IrError::DUPLICATE_PRODUCER) has_dup = true;
  CHECK(has_dup);
}

static void test_validate_unit_mismatch() {
  PipelineIRParser parser;
  // a 输出 ADU (astrocs.m) → b 输入 ADU 但 b 是 astrocs.m_elec 输出 electron
  // 构造: a(out ADU) → b(in ADU) 无冲突; 用 c(in electron 输入) 消费 a 的 ADU 产物
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "unit", "version": "1",
    "nodes": [
      {"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:z"}, "outputs": {"out": "artifact:aduart"},
       "resources": {"class": "io", "parallel": true}},
      {"node_id": "b", "module_id": "astrocs.m_elec", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:aduart"}, "outputs": {"out": "artifact:final"},
       "resources": {"class": "io", "parallel": true}}
    ],
    "outputs": {"out": "artifact:final"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  bool has = false;
  for (const auto& i : issues) if (i.kind == IrError::UNIT_MISMATCH) has = true;
  CHECK(has);
}

static void test_validate_coordinate_mismatch() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "coord", "version": "1",
    "nodes": [
      {"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:z"}, "outputs": {"out": "artifact:pix"},
       "resources": {"class": "io", "parallel": true}},
      {"node_id": "b", "module_id": "astrocs.m_healpix", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:pix"}, "outputs": {"out": "artifact:final"},
       "resources": {"class": "io", "parallel": true}}
    ],
    "outputs": {"out": "artifact:final"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  bool has = false;
  for (const auto& i : issues) if (i.kind == IrError::COORDINATE_MISMATCH) has = true;
  CHECK(has);
}

static void test_validate_data_mismatch() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "data", "version": "1",
    "nodes": [
      {"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:z"}, "outputs": {"out": "artifact:dataart"},
       "resources": {"class": "io", "parallel": true}},
      {"node_id": "b", "module_id": "astrocs.m_data", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:dataart"}, "outputs": {"out": "artifact:final"},
       "resources": {"class": "io", "parallel": true}}
    ],
    "outputs": {"out": "artifact:final"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  bool has = false;
  for (const auto& i : issues) if (i.kind == IrError::DATA_MISMATCH) has = true;
  CHECK(has);
}

static void test_validate_unproduced_output() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "noout", "version": "1",
    "nodes": [{"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
      "inputs": {"in": "artifact:z"}, "outputs": {"out": "artifact:made"},
      "resources": {"class": "io", "parallel": true}}],
    "outputs": {"out": "artifact:never_made"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  bool has = false;
  for (const auto& i : issues) if (i.kind == IrError::UNPRODUCED_OUTPUT) has = true;
  CHECK(has);
}

static void test_validate_unconsumed() {
  PipelineIRParser parser;
  auto r = parser.parse(R"({
    "schema": "astrocs.pipeline/v1", "pipeline_id": "uncon", "version": "1",
    "nodes": [
      {"node_id": "a", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:z"}, "outputs": {"out": "artifact:wasted"},
       "resources": {"class": "io", "parallel": true}},
      {"node_id": "b", "module_id": "astrocs.m", "module_api": "1.x", "config": {},
       "inputs": {"in": "artifact:z"}, "outputs": {"out": "artifact:final"},
       "resources": {"class": "io", "parallel": true}}
    ],
    "outputs": {"out": "artifact:final"}
  })");
  CHECK(r.ok());
  auto issues = parser.validate(r.value(), make_registry());
  bool has = false;
  for (const auto& i : issues) if (i.kind == IrError::UNCONSUMED) has = true;
  CHECK(has);
}

static void test_control_package_fixtures() {
  PipelineIRParser parser;
  // fixture 定位: 控制包解压布局演进过 (V6 控制包 fixtures 先在 工程控制/<pkg>/,
  // V6.1 重构后归档迁移到 engineering/control/archive/...), 候选按新旧顺序探测,
  // 首个存在者生效, 避免 single-path 硬编码随归档迁移后全平台 fixture 缺失。
  static const char* kFixtureRoots[] = {
      "/工程控制/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/fixtures",
      "/engineering/control/archive/2026-09-02_legacy_"
      "工程控制_v1.3-to-v6.1/CONTROL_V6/"
      "AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/fixtures",
  };
  const char* repo = std::getenv("ASTROCS_REPO");
  std::string base;
  for (const char* cand : kFixtureRoots) {
    // 探测/读取统一走 utf8_path: Windows 下窄路径按 ACP 解释, UTF-8 中文段
    // 必须转宽 (CreateFileW) 才能命中磁盘真实目录名; kFixtureRoots 机制不动。
    std::ifstream f(utf8_path(std::string(repo ? repo : "../..") + cand +
                              "/valid_pipeline.json"));
    if (f.good()) { base = std::string(repo ? repo : "../..") + cand + "/"; break; }
  }
  CHECK(!base.empty());
  if (base.empty()) {
    std::fprintf(stderr, "core_pipeline: 控制包 fixtures 目录缺失 (候选见 kFixtureRoots)\n");
    return;
  }
  auto v = parser.parse(read_file(base + "valid_pipeline.json"));
  CHECK(v.ok());
  if (v.ok()) {
    auto issues = parser.validate(v.value(), make_registry());
    CHECK(issues.empty());
  }
  auto inv = parser.parse(read_file(base + "invalid_pipeline_serial_heavy.json"));
  CHECK(inv.failed());  // cpu_heavy serial 必须拒
}

int main() {
  test_parse_valid();
  test_parse_serial_heavy_rejected();
  test_parse_bad_schema();
  test_validate_unknown_module();
  test_validate_missing_port();
  test_validate_cycle();
  test_validate_duplicate_producer();
  test_validate_unit_mismatch();
  test_validate_coordinate_mismatch();
  test_validate_data_mismatch();
  test_validate_unproduced_output();
  test_validate_unconsumed();
  test_control_package_fixtures();
  if (failures == 0) {
    std::printf("CORE-004 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
