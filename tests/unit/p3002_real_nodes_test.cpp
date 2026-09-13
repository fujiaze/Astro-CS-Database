// tests/unit/p3002_real_nodes_test.cpp — P3-002 Phase3 真实节点与 DAG 链
//
// 验收映射 (控制包 P3-002; 合同锚: 宪章 §7.2 "投影规划和重采样是独立算法节点,
// 不得重复调用完整 phase3_session_run()" + §8.2 "每个节点必须具有唯一实际
// entrypoint、输入、输出、副作用和 call count" + RUN_GRAPH_CONTRACT §5/§6
// call_count 语义; P1-001/P2-001 先例同构):
//   1. registry 5 类 Phase3 节点各自唯一真实 operation 委托 (子节点禁止调用
//      完整 p3_session_run) — 节点 last_manifest 必须携带 operation/entry 标记,
//      与 runtime/pipeline/module_ports.registry.json 冻结绑定表逐一一致:
//        astrocs.phase3.properties → read_properties   astrocs_phase3_properties_v1
//        astrocs.phase3.wcs        → build_wcs         astrocs_phase3_wcs_v1
//        astrocs.phase3.resample2  → resample_projection astrocs_phase3_resample_v1
//        astrocs.phase3.writer     → write_fits        astrocs_phase3_writer_v1
//        astrocs.phase3.verify     → verify_output     astrocs_phase3_verify_v1
//   2. typed artifact: 每节点产出 descriptor.data_id 对应的磁盘 artifact 且存在
//      (p3_props.json / p3_wcs.json / p3_resampled.{json,bin} / output_phase3.fits
//      + p3_writer.json / p3_verify.json; output_dir 文件约定传递, P2 先例同构)。
//   3. trace call_count=1: Runtime 全链执行每节点 MODULE_CALL 恰好一次,
//      trace_violations 为空 (无隐藏 session 重复调用)。
//   4. fail-fast: 上游节点确定性拒绝 → run 失败 + 下游节点不产出伪产物。
//   5. 负向: 坏 config (缺 source/缺 center) validate 拒绝, 不执行科学计算。
//
// RED 锚定 (实现前): Phase3 五子节点工厂委托 P3Api session adapter → 每个子
//   节点执行完整 p3_session_run (全链重复 5 次), manifest 无 operation/entry
//   字段 → 断言 1 失败; 节点 artifact 文件约定不存在 → 断言 2 失败。
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"

#include "healpix_core.h"       // astrocs::healpix::pix2ang_nest (数学权威)
#include "p1sess_fixtures.hpp"  // p1sess::write_fits_file 手写最小 FITS
#include "version_generated.h"  // B2-A10: 夹具复用 build 期版本单源 (同 CLI)

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define P3002N_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P3002N_GETPID static_cast<long>(::getpid())
#endif

using json = nlohmann::json;
using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)
#define CHECK_MSG(cond, msg)                                              \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s -- %s\n", __FILE__,    \
                   __LINE__, #cond, (msg));                               \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

namespace fs = std::filesystem;

namespace {

constexpr int kW = 16, kH = 16;
constexpr float kSigVal = 100.0f;

inline float const_px(int, void* user) { return *static_cast<float*>(user); }

std::string hips_properties_text(const char* bunit) {
  std::string s;
  s += "hips_order = 0\n";
  s += "hips_tile_width = 512\n";
  s += "hips_tile_format = fits\n";
  s += "hips_frame = icrs\n";
  s += "dataproduct_type = image\n";
  s += "hips_version = 1.0\n";
  if (bunit) s += std::string("BUNIT = ") + bunit + "\n";
  return s;
}

// 手写 signal-only HiPS (同 p3002_uncertainty_test fixture 规则)
bool write_signal_hips(const std::string& root) {
  const std::string root_posix = fs::path(root).generic_string();
  std::error_code ec;
  fs::create_directories(
      fs::path(root_posix + "/signal/Norder0/Dir0"), ec);
  if (ec) return false;
  std::ofstream p(fs::path(root_posix + "/signal/properties"), std::ios::binary);
  if (!p) return false;
  p << hips_properties_text("ADU");
  p.close();
  float v = kSigVal;
  return p1sess::write_fits_file(
             root_posix + "/signal/Norder0/Dir0/Npix0.fits", 512, 512,
             const_px, &v) == 0;
}

struct NodeFixture {
  fs::path root;
  std::string hips;
  std::string out;
  double ra = 0, dec = 0;
};

NodeFixture make_node_fixture(const char* tag) {
  NodeFixture fx;
  fx.root = fs::temp_directory_path() /
            ("p3002_nodes_" + std::string(tag) + "_" + std::to_string(P3002N_GETPID));
  std::error_code ec;
  fs::remove_all(fx.root, ec);
  fs::create_directories(fx.root, ec);
  fx.hips = (fx.root / "hips").generic_string();
  fx.out = (fx.root / "out").generic_string();
  fs::create_directories(fx.out, ec);
  // B2-A10（宪章 §4.3）: node 级夹具必须提供上游 run_context.json 产物。
  // 与 CLI run 共用 astrocs::core::write_run_context 唯一生成路径（真实
  // run_id/software_version/source_sha，非占位）；input_manifest_hash 不在
  // 此处——由 writer 节点从真实输入 HiPS 字节派生。
  {
    char rid[16];
    std::snprintf(rid, sizeof(rid), "%012lx",
                  static_cast<unsigned long>(P3002N_GETPID) & 0xffffffffffUL);
    CHECK(write_run_context(fx.out, rid, ASTROCS_VERSION_STRING,
                            ASTROCS_COMMIT_SHA).ok());
  }
  CHECK(write_signal_hips(fx.hips));
  // 采样中心: nside512 NESTED leaf 131072 = order0 tile0 中部
  astrocs::healpix::pix2ang_nest(512u, 131072ull, fx.ra, fx.dec);
  return fx;
}

void cleanup_fixture(NodeFixture& fx) {
  std::error_code ec;
  fs::remove_all(fx.root, ec);
}

// ── Phase3 节点期望表 (唯一真实 operation 绑定, 冻结源
//    runtime/pipeline/module_ports.registry.json) ────────────────────────────
struct NodeExpect {
  const char* module_id;
  const char* operation;
  const char* entry;
};
const NodeExpect kNodeExpects[] = {
    {"astrocs.phase3.properties", "read_properties", "astrocs_phase3_properties_v1"},
    {"astrocs.phase3.wcs", "build_wcs", "astrocs_phase3_wcs_v1"},
    {"astrocs.phase3.resample2", "resample_projection", "astrocs_phase3_resample_v1"},
    {"astrocs.phase3.writer", "write_fits", "astrocs_phase3_writer_v1"},
    {"astrocs.phase3.verify", "verify_output", "astrocs_phase3_verify_v1"},
};

std::string node_config(const NodeFixture& fx) {
  char buf[1024];
  std::snprintf(buf, sizeof(buf),
                R"({
  "source": {"hips_dir": "%s"},
  "center": {"ra_deg": %.12f, "dec_deg": %.12f},
  "scale_deg_per_px": 0.01,
  "width_px": %d, "height_px": %d,
  "sampler": "nearest",
  "longitude_parity": "east_left",
  "bitpix": -32,
  "output_dir": "%s"
})",
                fx.hips.c_str(), fx.ra, fx.dec, kW, kH, fx.out.c_str());
  return std::string(buf);
}

std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
  return s;
}

json run_node(ModuleRegistry& reg, const std::string& module_id,
              const std::string& config_json, RunContext& ctx,
              Result<void>* rc_out = nullptr) {
  auto m = reg.create(module_id);
  if (m.failed()) {
    CHECK_MSG(false, (module_id + ": create failed").c_str());
    return json();
  }
  auto v = m.value()->validate_config(config_json);
  if (v.failed()) {
    CHECK_MSG(false,
              (module_id + ": validate failed: " + v.error().message()).c_str());
    return json();
  }
  auto p = m.value()->plan("node_" + module_id, config_json);
  CHECK(p.ok());
  auto r = m.value()->execute(ctx);
  if (rc_out) *rc_out = r;
  auto man = m.value()->last_manifest();
  if (!man.ok()) {
    CHECK_MSG(false, (module_id + ": no manifest").c_str());
    return json();
  }
  json j;
  try {
    j = json::parse(man.value());
  } catch (...) {
    CHECK_MSG(false, (module_id + ": manifest not JSON").c_str());
  }
  return j;
}

}  // namespace

// ── 1. 每节点唯一真实 operation 标记 + typed artifact ─────────────────────
static void test_nodes_real_operation() {
  NodeFixture fx = make_node_fixture("nodes");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());

  // B2-A10（宪章 §4.3）: writer 节点 fail-closed 依赖上游 run_context.json；
  // 先断言夹具提供的产物 schema 真实（非空 run_id/software_version），
  // 再验证节点链——避免"夹具缺产物"被误判为节点缺陷。
  {
    json ctx = json::parse(read_file(fx.out + "/run_context.json"));
    CHECK(ctx.value("kind", "") == "astrocs_run_context");
    CHECK(!ctx.value("run_id", "").empty());
    CHECK(!ctx.value("software_version", "").empty());
  }
  const std::string cfg = node_config(fx);
  RunContext ctx;
  for (const auto& e : kNodeExpects) {
    Result<void> rc;
    json man = run_node(reg, e.module_id, cfg, ctx, &rc);
    CHECK_MSG(man.value("operation", "") == e.operation,
              (std::string(e.module_id) + ": manifest operation must be frozen binding")
                  .c_str());
    CHECK_MSG(man.value("entry", "") == e.entry,
              (std::string(e.module_id) + ": manifest entry must be frozen binding")
                  .c_str());
    CHECK_MSG(man.value("status", "") == "ok",
              (std::string(e.module_id) + ": node must complete on valid fixture; rc=" +
               (rc.failed() ? rc.error().message() : std::string("ok")))
                  .c_str());
  }

  // typed artifact 落盘 (output_dir 文件约定; 每节点产物存在)
  CHECK_MSG(fs::exists(fs::path(fx.out + "/p3_props.json")),
            "properties node must publish p3_props.json (DATA-P3-PROPS)");
  CHECK_MSG(fs::exists(fs::path(fx.out + "/p3_wcs.json")),
            "wcs node must publish p3_wcs.json (DATA-P3-WCS)");
  CHECK_MSG(fs::exists(fs::path(fx.out + "/p3_resampled.json")),
            "resample node must publish p3_resampled.json (DATA-P3-RES)");
  CHECK_MSG(fs::exists(fs::path(fx.out + "/p3_resampled.bin")),
            "resample node must publish p3_resampled.bin (typed planes)");
  CHECK_MSG(fs::exists(fs::path(fx.out + "/output_phase3.fits")),
            "writer node must publish output_phase3.fits (DATA-P3-FITS)");
  CHECK_MSG(fs::exists(fs::path(fx.out + "/p3_writer.json")),
            "writer node must publish p3_writer.json");
  CHECK_MSG(fs::exists(fs::path(fx.out + "/p3_verify.json")),
            "verify node must publish p3_verify.json (DATA-P3-VER)");

  // artifact 内容合同抽查: props 携带实测 order/BUNIT; resampled 声明平面
  {
    json props;
    try {
      props = json::parse(read_file(fx.out + "/p3_props.json"));
    } catch (...) { props = json::object(); CHECK(false); }
    CHECK(props.value("schema", "") == "DATA-P3-PROPS");
    CHECK(props.value("hips_order", -1) == 0);      // properties 实测 order
    CHECK(props.value("bunit", "") == "ADU");
    json res;
    try {
      res = json::parse(read_file(fx.out + "/p3_resampled.json"));
    } catch (...) { res = json::object(); CHECK(false); }
    CHECK(res.value("schema", "") == "DATA-P3-RES");
    CHECK(res.value("width_px", 0) == kW && res.value("height_px", 0) == kH);
    CHECK(res.contains("planes") && res["planes"].is_array());
    json ver;
    try {
      ver = json::parse(read_file(fx.out + "/p3_verify.json"));
    } catch (...) { ver = json::object(); CHECK(false); }
    CHECK(ver.value("schema", "") == "DATA-P3-VER");
    CHECK(ver.value("reopen_ok", 0) == 1);
  }
  cleanup_fixture(fx);
}

// ── 2. Runtime 全链: call_count=1 / 无隐藏 session 重复调用 ────────────────
static void test_runtime_chain_call_count_1() {
  NodeFixture fx = make_node_fixture("chain");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  const json pc = json::parse(node_config(fx));
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art,
                  const char* res_class, bool parallel) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = pc;
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", res_class}, {"parallel", parallel}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "p3002.real.nodes";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("props", "astrocs.phase3.properties", "hips", "artifact:in", "props", "artifact:props", "cpu_heavy", true));
  ir["nodes"].push_back(node("wcs", "astrocs.phase3.wcs", "props", "artifact:props", "wcs_plan", "artifact:wcs", "cpu_heavy", true));
  ir["nodes"].push_back(node("res", "astrocs.phase3.resample2", "wcs_plan", "artifact:wcs", "resampled", "artifact:res", "cpu_heavy", true));
  ir["nodes"].push_back(node("wr", "astrocs.phase3.writer", "resampled", "artifact:res", "fits", "artifact:wr", "io", false));
  ir["nodes"].push_back(node("ver", "astrocs.phase3.verify", "fits", "artifact:wr", "verified", "artifact:ver", "io", false));
  ir["outputs"] = json{{"verified", "artifact:ver"}, {"fits", "artifact:wr"},
                       {"res", "artifact:res"}, {"props", "artifact:props"},
                       {"wcs", "artifact:wcs"}};

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
  if (load.failed()) { cleanup_fixture(fx); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());

  // trace: 每 node MODULE_CALL 恰好 1 次 (call_count=1) + 无重复调用违规
  const auto tr = rt.value()->node_trace();
  CHECK(tr.size() == 5);
  for (const auto& t : tr) {
    CHECK_MSG(t.call_count == 1,
              (std::string("node ") + t.node_id + ": call_count must be 1").c_str());
    CHECK(t.status == "COMPLETED");
  }
  auto viol = rt.value()->trace_violations();
  CHECK_MSG(viol.empty(), "no hidden session repeated calls");
  for (const auto& v : viol) std::fprintf(stderr, "violation: %s\n", v.c_str());

  // 全链产物落盘
  CHECK(fs::exists(fs::path(fx.out + "/p3_props.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p3_wcs.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p3_resampled.bin")));
  CHECK(fs::exists(fs::path(fx.out + "/output_phase3.fits")));
  CHECK(fs::exists(fs::path(fx.out + "/p3_verify.json")));

  cleanup_fixture(fx);
}

// ── 3. 负向: 坏 config 确定性拒绝 (validate 面不执行科学计算) ───────────────
static void test_negative_rejection() {
  NodeFixture fx = make_node_fixture("neg");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;

  // 3a. 缺 source → validate 拒
  {
    auto m = reg.create("astrocs.phase3.properties");
    CHECK(m.ok());
    auto v = m.value()->validate_config(R"({"output_dir":"/tmp"})");
    CHECK_MSG(v.failed(), "missing source.hips_dir must be rejected");
  }
  // 3b. 缺 center → validate 拒
  {
    auto m = reg.create("astrocs.phase3.wcs");
    CHECK(m.ok());
    auto v = m.value()->validate_config(R"({
      "source": {"hips_dir": "x"}, "scale_deg_per_px": 0.01,
      "width_px": 16, "height_px": 16, "output_dir": "/tmp"})");
    CHECK_MSG(v.failed(), "missing center must be rejected");
  }
  // 3c. scale<=0 → run 拒 (fail-closed 无 silent default)
  {
    json c = json::parse(node_config(fx));
    c["scale_deg_per_px"] = 0.0;
    Result<void> rc;
    run_node(reg, "astrocs.phase3.resample2", c.dump(), ctx, &rc);
    CHECK_MSG(rc.failed(), "scale<=0 must be rejected at run");
    CHECK(!fs::exists(fs::path(fx.out + "/p3_resampled.bin")));
  }
  // 3d. hips_dir 不存在 → run 拒, 无伪产物
  {
    json c = json::parse(node_config(fx));
    c["source"] = json{{"hips_dir", (fx.root / "no_such_hips").generic_string()}};
    Result<void> rc;
    run_node(reg, "astrocs.phase3.properties", c.dump(), ctx, &rc);
    CHECK_MSG(rc.failed(), "nonexistent hips_dir must fail deterministically");
    CHECK(!fs::exists(fs::path(fx.out + "/p3_props.json")));
  }
  cleanup_fixture(fx);
}

// ── 4. 确定性: 同 config 双跑 resample 输出 bin bitwise 一致 ────────────────
static void test_determinism() {
  std::string first_bin;
  for (int run = 0; run < 2; ++run) {
    NodeFixture fx = make_node_fixture("det");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    // 上游节点先行 (properties→wcs→resample typed artifact 链)
    json man_p = run_node(reg, "astrocs.phase3.properties", node_config(fx), ctx);
    CHECK(man_p.value("status", "") == "ok");
    json man_w = run_node(reg, "astrocs.phase3.wcs", node_config(fx), ctx);
    CHECK(man_w.value("status", "") == "ok");
    json man = run_node(reg, "astrocs.phase3.resample2", node_config(fx), ctx);
    CHECK(man.value("status", "") == "ok");
    const std::string content = read_file(fx.out + "/p3_resampled.bin");
    CHECK(!content.empty());
    if (run == 0) {
      first_bin = content;
    } else {
      CHECK_MSG(content == first_bin,
                "resample output must be bitwise deterministic");
    }
    cleanup_fixture(fx);
  }
}

int main() {
  test_nodes_real_operation();
  test_runtime_chain_call_count_1();
  test_negative_rejection();
  test_determinism();
  if (failures == 0) {
    std::printf("P3-002 REAL NODES PASS (5 节点唯一真实 operation + call_count=1 + "
                "typed artifact + fail-closed 负面 + 确定性)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-002 REAL NODES FAIL (%d)\n", failures);
  return 1;
}
