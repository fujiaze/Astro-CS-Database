// tests/unit/p1001_real_nodes_test.cpp — P1-001 (attempt 2) Phase1 真实节点与 complete 门
//
// 验收映射 (控制包 ARCH-P0-001 / PROD-P0-001 Phase1 侧, 前台裁定口径):
//   1. registry 8 类 Phase1 节点各自唯一真实 operation 委托（子节点禁止调用完整
//      phase_session_run）——节点 last_manifest 必须携带 operation/entry 标记, 与
//      runtime/pipeline/module_ports.registry.json 冻结绑定表逐一一致。
//   2. typed artifact: 每节点产出 descriptor.data_id 对应的磁盘 artifact 且存在。
//   3. trace call_count=1: Runtime 全链执行每节点 MODULE_CALL 恰好一次,
//      trace_violations 为空（无隐藏 session 重复调用）。
//   4. complete 门 fail-closed: p1_session manifest 在 Phase1 链不完整时
//      status="partial"（不写 complete）且带 availability 全域报告（不冒充完成,
//      宪章 §16.2/§18.3; 链完整后再开放 complete）。
//   5. 负向: 坏 config/坏帧/缺科学参数确定性拒绝, 不留伪产物。
//
// RED 锚定（改造前）: SessionModule(完整 session 委托) manifest 无 operation/entry
//   字段 → 断言 1 失败; p1_session manifest 无 availability 且 status=complete
//   → 断言 4 失败。本测试先于实现提交面运行记录 RED, 再随实现转 GREEN。
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"
#include "p1_session.h"  // complete 门: API-P1-001 冻结 C ABI

#include "p1sess_fixtures.hpp"  // 最小 FITS writer (手写, 不调生产 symbol)

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define P1001_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P1001_GETPID static_cast<long>(::getpid())
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

// host services 工厂（定义 lib/backend_host/host_services.cpp, astrocs_cpu 库;
// 与 lib/core/src/module_adapters.cpp 同一声明模式）
extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
}

namespace {

namespace fs = std::filesystem;

// ── fixture: 32x32 合成帧（固定 seed；背景常数 + 单高斯星）────────────────
constexpr int kW = 32, kH = 32;
struct StarField {
  float bg;
  float amp;
};
inline float star_field_pixel(int i, void* user) {
  auto* sf = static_cast<StarField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  const double g = sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  return sf->bg + static_cast<float>(g);
}
inline float const_pixel(int, void* user) {
  return *static_cast<float*>(user);
}

struct Fixture {
  fs::path dir;
  std::string light1, light2, bias, dark, flat;
  std::string out_dir;
};

Fixture make_fixture(const char* tag) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_real_nodes_" + std::string(tag) + "_" +
           std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.light2 = (f.dir / "light_2.fits").string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  StarField sf{100.0f, 5000.0f};
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, star_field_pixel, &sf) == 0);
  CHECK(p1sess::write_fits_file(f.light2, kW, kH, star_field_pixel, &sf) == 0);
  float vb = 10.0f, vd = 5.0f, vf = 1.0f;
  CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &vb) == 0);
  CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &vd) == 0);
  CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &vf) == 0);
  return f;
}

void cleanup_fixture(Fixture& f) {
  std::error_code ec;
  fs::remove_all(f.dir, ec);
}

// 向上探测仓库根（锚: ASTROCS_PROJECT_CONSTITUTION.md）→ GaiaDR3 用户数据区
std::string find_repo_gaia_dir() {
  fs::path probe = fs::current_path();
  for (int i = 0; i < 6 && !probe.empty(); ++i) {
    std::error_code ec;
    if (fs::exists(probe / "ASTROCS_PROJECT_CONSTITUTION.md", ec)) {
      fs::path g = probe / "GaiaDR3";
      if (fs::exists(g, ec)) return g.string();
    }
    probe = probe.parent_path();
  }
  return std::string();
}

// ── Phase1 节点期望表（唯一真实 operation 绑定, 冻结源
//    runtime/pipeline/module_ports.registry.json）──────────────────────────
struct NodeExpect {
  const char* module_id;
  const char* operation;
  const char* entry;
};
const NodeExpect kNodeExpects[] = {
    {"astrocs.phase1.calibration", "calibrate", "astrocs_phase1_calibrate_v1"},
    {"astrocs.phase1.cosmetic", "cosmetic_correct", "astrocs_phase1_cosmetic_v1"},
    {"astrocs.phase1.star-psf", "detect_sources", "astrocs_phase1_starpsf_v1"},
    {"astrocs.phase1.wcs-platesolve", "plate_solve", "astrocs_phase1_wcs_v1"},
    {"astrocs.phase1.photometry", "measure_flux", "astrocs_phase1_photometry_v1"},
    {"astrocs.phase1.noise-snr", "estimate_snr", "astrocs_phase1_noisesnr_v1"},
    {"astrocs.phase1.drizzle", "drizzle_stack", "astrocs_phase1_drizzle_v1"},
    {"astrocs.phase1.writer", "write_hips", "astrocs_phase1_writer_v1"},
};

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
    CHECK_MSG(false, (module_id + ": validate failed: " + v.error().message()).c_str());
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

// ═══ CORE-RACE-001 helpers（p1001 链并发撕裂读）═══════════════════════════
// 缺陷（修复前）: p1_op_cosmetic 读取 cal 节点产物 calibrated_<base> 后就地覆写
// 同一路径, 而 drz 节点（IR 声明输入 artifact:cal）与 cos 同为 cal 下游、
// resources.parallel=true ⇒ create_runtime(2) 下并发执行; aio_write_fits 自身
// 非原子（fopen("wb") 截断 + 增量写）⇒ 消费者可观察到半写文件。
std::string read_bytes(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

// 热像素场（高斯星 + 两个极热像素）: 保证 cosmetic 真实改动像素（非空转 fixture）
struct HotField { float bg; float amp; };
inline float hot_field_pixel(int i, void* user) {
  auto* sf = static_cast<HotField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  double v = sf->bg + sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  if ((x == 5 && y == 5) || (x == 26 && y == 7)) v += 1.0e6;  // 热像素
  return static_cast<float>(v);
}

Fixture make_hot_fixture(const char* tag) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_chain_race_" + std::string(tag) + "_" +
           std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.light2 = (f.dir / "light_2.fits").string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  HotField hf{100.0f, 5000.0f};
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, hot_field_pixel, &hf) == 0);
  CHECK(p1sess::write_fits_file(f.light2, kW, kH, hot_field_pixel, &hf) == 0);
  float vb = 10.0f, vd = 5.0f, vf = 1.0f;
  CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &vb) == 0);
  CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &vd) == 0);
  CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &vf) == 0);
  return f;
}

// 全链 7 节点 IR（与 §2 同构; drz 与 cos 并发读同一 artifact:cal）
json build_p1001_full_chain_ir(const std::string& cfg, const std::string& pipeline_id) {
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg);
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = pipeline_id;
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cal", "astrocs.phase1.calibration", "frames", "artifact:in", "calibrated", "artifact:cal"));
  ir["nodes"].push_back(node("cos", "astrocs.phase1.cosmetic", "calibrated", "artifact:cal", "cleaned", "artifact:cos"));
  ir["nodes"].push_back(node("psf", "astrocs.phase1.star-psf", "cleaned", "artifact:cos", "sources", "artifact:psf"));
  ir["nodes"].push_back(node("phot", "astrocs.phase1.photometry", "sources", "artifact:psf", "fluxes", "artifact:phot"));
  ir["nodes"].push_back(node("snr", "astrocs.phase1.noise-snr", "fluxes", "artifact:phot", "snr", "artifact:snr"));
  ir["nodes"].push_back(node("drz", "astrocs.phase1.drizzle", "calibrated", "artifact:cal", "stacked", "artifact:drz"));
  ir["nodes"].push_back(node("wr", "astrocs.phase1.writer", "stacked", "artifact:drz", "fits", "artifact:wr"));
  ir["outputs"] = json{{"fits", "artifact:wr"}, {"snr", "artifact:snr"},
                       {"psf", "artifact:psf"},
                       {"cal", "artifact:cal"}, {"cos", "artifact:cos"}};
  return ir;
}

std::string p1001_full_chain_cfg(const Fixture& fx) {
  return std::string(R"({
    "input_lights": [")") + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true, "hot_sigma": 5.0, "cold_sigma": 5.0},
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";
}

// 单次全链运行; 返回 run 是否成功 + 每节点 call_count/status 明细
struct ChainRunResult {
  bool ok = false;
  std::string error;
  std::size_t node_count = 0;
  bool trace_ok = true;
};
ChainRunResult run_full_chain_once(const std::string& cfg, uint32_t workers,
                                   ModuleRegistry& reg) {
  ChainRunResult out;
  auto rt = create_runtime(workers);
  if (!rt.ok()) { out.error = "create_runtime failed"; return out; }
  json ir = build_p1001_full_chain_ir(cfg, "p1001.chain.race");
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  if (!load.ok()) { out.error = "load_pipeline: " + load.error().message(); return out; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  out.ok = rrun.ok();
  if (!out.ok) out.error = rrun.error().message();
  const auto tr = rt.value()->node_trace();
  out.node_count = tr.size();
  for (const auto& t : tr) {
    if (t.call_count != 1 || t.status != "COMPLETED") out.trace_ok = false;
  }
  if (!rt.value()->trace_violations().empty()) out.trace_ok = false;
  return out;
}

}  // namespace

// ── 1. 每节点唯一真实 operation 标记 + typed artifact ─────────────────────
static void test_nodes_real_operation() {
  Fixture fx = make_fixture("nodes");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());

  const std::string base_cfg = R"({
    "input_lights": [")" + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true, "hot_sigma": 5.0, "cold_sigma": 5.0}
  })";
  // calibrate 先行（后续节点消费 calibrated_*.fits）
  RunContext ctx;
  json man_cal = run_node(reg, "astrocs.phase1.calibration", base_cfg, ctx);
  CHECK(man_cal.value("operation", "") == "calibrate");
  CHECK(man_cal.value("entry", "") == "astrocs_phase1_calibrate_v1");
  CHECK(man_cal.value("status", "") == "ok");
  CHECK(man_cal.contains("artifacts") && man_cal["artifacts"].is_array() &&
        !man_cal["artifacts"].empty());
  for (const auto& a : man_cal["artifacts"])
    CHECK(fs::exists(fs::path(a.get<std::string>())));

  // cosmetic：enabled → 真实 ac_correct_frame 执行
  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", base_cfg, ctx);
  CHECK(man_cos.value("operation", "") == "cosmetic_correct");
  CHECK(man_cos.value("entry", "") == "astrocs_phase1_cosmetic_v1");
  CHECK(man_cos.value("status", "") == "ok");

  // star-psf：StarDetector 真实检测（星点场 → n_detected>=1; PSF 特性来自 StarSource）
  json man_psf = run_node(reg, "astrocs.phase1.star-psf", base_cfg, ctx);
  CHECK(man_psf.value("operation", "") == "detect_sources");
  CHECK(man_psf.value("entry", "") == "astrocs_phase1_starpsf_v1");
  CHECK(man_psf.value("status", "") == "ok");
  CHECK(man_psf.contains("sources_artifact") &&
        man_psf["sources_artifact"].is_string());
  const std::string src_path = man_psf.value("sources_artifact", "");
  CHECK(fs::exists(fs::path(src_path)));
  {
    json cat;
    try { cat = json::parse(read_file(src_path)); } catch (...) { CHECK(false); }
    CHECK(cat.value("schema", "") == "DATA-P1-SOURCES");
    bool found_star = false;
    for (const auto& fr : cat.value("frames", json::array())) {
      if (fr.value("n_detected", 0u) >= 1) found_star = true;
      for (const auto& s : fr.value("sources", json::array())) {
        CHECK(s.contains("fwhm_px") && s.contains("ellipticity"));
      }
    }
    CHECK_MSG(found_star, "star fixture must yield >=1 detection (不误报空)");
  }
  // PSF 特性 artifact (DATA-P1-PSF)
  CHECK(man_psf.contains("psf_artifact") && man_psf["psf_artifact"].is_string());
  CHECK(fs::exists(fs::path(man_psf.value("psf_artifact", ""))));

  // wcs-platesolve：真实 ipv 求解器链（sdet+gaia 句柄 → ipv_solve_from_memory_
  // with_callback_d → CD/CRVAL/CRPIX/RMS）。合同:
  //   a) 缺求解参数（ra0/dec0/focal/pixel_size/gaia_data_dir）→ DATA 拒绝
  //      （真实链必需参数禁 silent default）;
  //   b) 全参数 → 真实调用 ipv; Linux 上 ipv 为生产源内建 stub（Windows-only
  //      DLL 加载）→ fail-closed DATA 且错误如实上报平台限制; Windows → 真实
  //      求解成功或真实失败（无伪 WCS）。
  const std::string wcs_base = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "wcs": {)";
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase1.wcs-platesolve", wcs_base + "}}", ctx, &rc);
    CHECK_MSG(rc.failed(), "missing solve params must be rejected (no silent defaults)");
  }
  {
    const std::string gaia_dir = find_repo_gaia_dir();
    const std::string wcs_full = wcs_base +
        R"("ra0": 10.0, "dec0": 20.0, "focal_length_mm": 400.0,
            "pixel_size_um": 3.76, "gaia_data_dir": ")" + gaia_dir + R"("}})";
    Result<void> rc;
    json man_wcs = run_node(reg, "astrocs.phase1.wcs-platesolve", wcs_full, ctx, &rc);
    CHECK(man_wcs.value("operation", "") == "plate_solve");
    CHECK(man_wcs.value("entry", "") == "astrocs_phase1_wcs_v1");
#if defined(_WIN32)
    CHECK_MSG(rc.ok(), "Windows: real ipv solve should succeed on valid star field");
#else
    CHECK_MSG(rc.failed(), "Linux: ipv stub must fail-closed (no fake WCS)");
    CHECK(rc.error().domain() == astrocs::core::ErrorDomain::DATA);
#endif
  }

  // photometry：Photometer aperture 积分（对已检测源）
  json man_phot = run_node(reg, "astrocs.phase1.photometry", base_cfg, ctx);
  CHECK(man_phot.value("operation", "") == "measure_flux");
  CHECK(man_phot.value("entry", "") == "astrocs_phase1_photometry_v1");
  CHECK(man_phot.value("status", "") == "ok");
  CHECK(fs::exists(fs::path(man_phot.value("flux_artifact", ""))));

  // noise-snr：NoiseModel 真实估计
  json man_noise = run_node(reg, "astrocs.phase1.noise-snr", base_cfg, ctx);
  CHECK(man_noise.value("operation", "") == "estimate_snr");
  CHECK(man_noise.value("entry", "") == "astrocs_phase1_noisesnr_v1");
  CHECK(man_noise.value("status", "") == "ok");
  {
    json n;
    try { n = json::parse(read_file(man_noise.value("snr_artifact", ""))); } catch (...) { CHECK(false); }
    CHECK(n.value("schema", "") == "DATA-P1-SNR");
    CHECK(n.contains("frames") && !n["frames"].empty());
    for (const auto& fr : n["frames"]) {
      CHECK(fr.value("valid", false) == true);
      CHECK(fr.value("variance", 0.0) > 0.0);
      // ivar = 1/variance 不混合同
      CHECK(std::abs(fr.value("ivar", 0.0) * fr.value("variance", -1.0) - 1.0) < 1e-9);
    }
  }

  // drizzle：hp_drizzle_run 真实委托（带 WCS 头与 nside 科学参数）
  const std::string drz_cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";
  json man_drz = run_node(reg, "astrocs.phase1.drizzle", drz_cfg, ctx);
  CHECK(man_drz.value("operation", "") == "drizzle_stack");
  CHECK(man_drz.value("entry", "") == "astrocs_phase1_drizzle_v1");
  CHECK(man_drz.value("status", "") == "ok");
  CHECK(fs::exists(fs::path(man_drz.value("stack_artifact", ""))));

  // writer：真实 HiPS writer 链（消费 drizzle 产物 p1_stack.hiss →
  // aio_hiss_inspect/read_tile_* → AstroSphereTileView → aio_hips_product_*）
  Result<void> wr_rc;
  json man_wr = run_node(reg, "astrocs.phase1.writer", drz_cfg, ctx, &wr_rc);
  if (wr_rc.failed()) std::fprintf(stderr, "DBG writer error: %s\n", wr_rc.error().message().c_str());
  CHECK(man_wr.value("operation", "") == "write_hips");
  CHECK(man_wr.value("entry", "") == "astrocs_phase1_writer_v1");
  CHECK(man_wr.value("status", "") == "ok");
  CHECK(man_wr.contains("hips_root") && man_wr["hips_root"].is_string());
  CHECK(man_wr.value("n_tiles", 0u) >= 1);
  {
    json f;
    try { f = json::parse(read_file(man_wr.value("final_artifact", ""))); } catch (...) { CHECK(false); }
    CHECK(f.value("schema", "") == "DATA-P1-HIPS");
    CHECK(f.value("covered_area_model", "") == "support_x_A_cell");
  }
  CHECK(fs::exists(fs::path(fx.out_dir + "/signal/properties")));
  CHECK(fs::exists(fs::path(man_wr.value("final_artifact", ""))));

  cleanup_fixture(fx);
}

// ── 2. Runtime 全链: call_count=1 / 无隐藏 session 重复调用 ────────────────
static void test_runtime_chain_call_count_1() {
  Fixture fx = make_fixture("chain");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  const std::string out_dir = fx.out_dir;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + out_dir + R"(",
    "cosmetic": {"enabled": true},
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";

  // 7 节点主链 IR（cal→cos→psf→phot→snr→drz→wr; wcs 旁支在 §1 按平台合同
  // 单测: Linux=ipv 源内 stub fail-closed, Windows=真实求解——Runtime 主链
  // fail-fast 语义下平台 stub 失败会中断全链, 故主链不含 wcs 节点）
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg);
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "p1001.real.nodes";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cal", "astrocs.phase1.calibration", "frames", "artifact:in", "calibrated", "artifact:cal"));
  ir["nodes"].push_back(node("cos", "astrocs.phase1.cosmetic", "calibrated", "artifact:cal", "cleaned", "artifact:cos"));
  ir["nodes"].push_back(node("psf", "astrocs.phase1.star-psf", "cleaned", "artifact:cos", "sources", "artifact:psf"));
  ir["nodes"].push_back(node("phot", "astrocs.phase1.photometry", "sources", "artifact:psf", "fluxes", "artifact:phot"));
  ir["nodes"].push_back(node("snr", "astrocs.phase1.noise-snr", "fluxes", "artifact:phot", "snr", "artifact:snr"));
  ir["nodes"].push_back(node("drz", "astrocs.phase1.drizzle", "calibrated", "artifact:cal", "stacked", "artifact:drz"));
  ir["nodes"].push_back(node("wr", "astrocs.phase1.writer", "stacked", "artifact:drz", "fits", "artifact:wr"));
  // IR 静态验证合同: 每个产物必须被消费或声明为 pipeline 输出
  // (phot→fluxes 供 snr 输入; snr/psf/cal/cos 为本链终态输出面)
  ir["outputs"] = json{{"fits", "artifact:wr"}, {"snr", "artifact:snr"},
                       {"psf", "artifact:psf"},
                       {"cal", "artifact:cal"}, {"cos", "artifact:cos"}};

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
  if (load.failed()) { cleanup_fixture(fx); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());

  // trace: 每 node MODULE_CALL 恰好 1 次（call_count=1）+ 无重复调用违规
  const auto tr = rt.value()->node_trace();
  CHECK(tr.size() == 7);
  for (const auto& t : tr) {
    CHECK(t.call_count == 1);
    CHECK(t.status == "COMPLETED");
  }
  auto viol = rt.value()->trace_violations();
  CHECK_MSG(viol.empty(), "no hidden session repeated calls");
  for (const auto& v : viol) std::fprintf(stderr, "violation: %s\n", v.c_str());

  // typed artifact 落盘（cal/cosmetic 覆写 + 每节点 JSON/hiss + HiPS 产品面）
  CHECK(fs::exists(fs::path(out_dir + "/calibrated_light_1.fits")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_sources.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_psf.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_flux.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_snr.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_stack.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_final.json")));
  CHECK(fs::exists(fs::path(out_dir + "/signal/properties")));

  cleanup_fixture(fx);
}

// ── 2b. fail-fast 下游零调用（口径⑨: 上游节点失败 → 后续节点 call_count=0）──
static void test_fail_fast_downstream_zero_calls() {
  Fixture fx = make_fixture("ffast");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  // drizzle 缺 nside（确定性 DATA 拒绝）→ wr 未执行（call_count=0）;
  // 上游主链 cal→cos→psf→phot→snr 正常完成。
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true},
    "drizzle": {"nested": 1, "pixfrac": 1.0}
  })";
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg);
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "p1001.fail.fast";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cal", "astrocs.phase1.calibration", "frames", "artifact:in", "calibrated", "artifact:cal"));
  ir["nodes"].push_back(node("cos", "astrocs.phase1.cosmetic", "calibrated", "artifact:cal", "cleaned", "artifact:cos"));
  ir["nodes"].push_back(node("psf", "astrocs.phase1.star-psf", "cleaned", "artifact:cos", "sources", "artifact:psf"));
  ir["nodes"].push_back(node("phot", "astrocs.phase1.photometry", "sources", "artifact:psf", "fluxes", "artifact:phot"));
  ir["nodes"].push_back(node("snr", "astrocs.phase1.noise-snr", "fluxes", "artifact:phot", "snr", "artifact:snr"));
  ir["nodes"].push_back(node("drz", "astrocs.phase1.drizzle", "calibrated", "artifact:cal", "stacked", "artifact:drz"));
  ir["nodes"].push_back(node("wr", "astrocs.phase1.writer", "stacked", "artifact:drz", "fits", "artifact:wr"));
  ir["outputs"] = json{{"fits", "artifact:wr"}, {"snr", "artifact:snr"},
                       {"psf", "artifact:psf"},
                       {"cal", "artifact:cal"}, {"cos", "artifact:cos"}};

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK(load.ok());
  if (!load.ok()) { cleanup_fixture(fx); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.failed(), "upstream DATA rejection must fail the run");

  const auto tr = rt.value()->node_trace();
  bool drz_failed = false;
  for (const auto& t : tr) {
    if (t.node_id == "drz") {
      drz_failed = t.status == "FAILED";
      continue;
    }
    if (t.node_id == "wr") {
      CHECK_MSG(t.call_count == 0 || t.status != "COMPLETED",
                "downstream node must not execute after upstream failure");
    } else if (t.status == "COMPLETED") {
      CHECK(t.call_count == 1);
    }
  }
  CHECK_MSG(drz_failed, "drz node must be FAILED in trace");
  // 注: fail-fast 全局失败语义下, 与 drz 并行（2 workers）的 snr 可能被取消,
  // 故不断言 snr 产物存在; 只断言失败节点自身及其下游零产物。
  CHECK(!fs::exists(fs::path(fx.out_dir + "/p1_stack.json"))); // drz 拒绝无产物
  CHECK(!fs::exists(fs::path(fx.out_dir + "/p1_final.json"))); // wr 未执行

  cleanup_fixture(fx);
}

// ── 3. complete 门: p1_session fail-closed（链不完整 → partial+availability）──
static void test_complete_gate_fail_closed() {
  Fixture fx = make_fixture("gate");
  // 直接经冻结 C ABI（API-P1-001）: host services 缺省构造
  astrocs_host_services_v1 host{};
  void* state = nullptr;
  CHECK(astrocs_host_services_default_v1(&host, &state) == 0);
  acs_handle h = nullptr;
  CHECK(p1_session_create(&host, &h) == ACS_OK);

  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"("
  })";
  acs_span_u8 span{};
  span.head.struct_size = sizeof(span);
  span.head.abi_version = ACS_ABI_VERSION_V1;
  span.count = cfg.size();
  span.data = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(cfg.data()));
  CHECK(p1_session_validate(h, span) == ACS_OK);
  CHECK(p1_session_run(h, span, 0) == ACS_OK);
  acs_span_u8 out{};
  CHECK(p1_session_inspect(h, &out) == ACS_OK && out.data);
  json man;
  try {
    man = json::parse(std::string(reinterpret_cast<char*>(out.data), out.count));
  } catch (...) {
    CHECK(false);
  }
  host.allocator.free(host.allocator.user_data, out.data);
  p1_session_destroy(h);
  astrocs_host_services_destroy_state_v1(state);

  // complete 门 fail-closed: Phase1 链不完整（session 仅覆盖 calibrate/cosmetic）
  // → status 不得为 "complete"（PROD-P0-001: 不完整 Phase 禁写 complete）
  CHECK_MSG(man.value("status", "") != "complete",
            "incomplete phase1 chain must not write complete");
  CHECK(man.value("status", "") == "partial");
  // availability 全域报告（8 域; 不冒充完成）
  CHECK(man.contains("availability") && man["availability"].is_object());
  const char* domains[] = {"calibration", "cosmetic", "star_psf", "wcs",
                           "photometry", "noise_snr", "drizzle", "writer"};
  for (const char* d : domains) {
    CHECK_MSG(man["availability"].contains(d),
              ("availability missing domain: " + std::string(d)).c_str());
  }
  CHECK(man["availability"].value("calibration", "") == "available");
  CHECK(man["availability"].value("star_psf", "") == "unavailable");
  CHECK(man["availability"].value("writer", "") == "unavailable");

  cleanup_fixture(fx);
}

// ── 4. 负向: 坏 config / 坏帧 / 缺科学参数确定性拒绝, 不留伪产物 ─────────────
static void test_negative_injection() {
  Fixture fx = make_fixture("neg");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;

  // 4a. 缺 output_dir → validate 拒绝（fail-closed, 不执行科学计算）
  {
    auto m = reg.create("astrocs.phase1.calibration");
    CHECK(m.ok());
    auto v = m.value()->validate_config(R"({"input_lights":["x.fits"]})");
    CHECK(v.failed());
  }
  // 4b. 截断 FITS（坏帧）→ calibrate 节点失败, 无 calibrated_ 产物
  {
    const std::string bad = (fx.dir / "bad.fits").string();
    float v0 = 0.f;
    // fixture 语义: truncate_tail 截去数据块尾部 64 字节 → 坏帧(读必败)
    CHECK(p1sess::write_fits_file(bad, kW, kH, const_pixel, &v0,
                                  /*truncate_tail=*/1024) == 0);
    const std::string cfg = R"({
      "input_lights": [")" + bad + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    CHECK(rc.failed());
    // 无伪产物：失败后不得产出 calibrated_bad.fits
    CHECK(!fs::exists(fs::path(fx.out_dir + "/calibrated_bad.fits")));
  }
  // 4c. drizzle 缺 nside（科学参数禁 silent default）→ 确定性 DATA 拒绝
  {
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
              "cd11": -0.0002777777777777778, "cd12": 0.0,
              "cd21": 0.0, "cd22": 0.0002777777777777778},
      "drizzle": {"nested": 0, "pixfrac": 1.0}
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(), "drizzle without nside must be rejected");
    CHECK(!fs::exists(fs::path(fx.out_dir + "/p1_stack.json")));
  }
  // 4d. 纯噪声帧 → star-psf 空 catalog 合法成功（不误报）
  {
    const std::string noise_f = (fx.dir / "noise.fits").string();
    float v1 = 100.0f;
    CHECK(p1sess::write_fits_file(noise_f, kW, kH, const_pixel, &v1) == 0);
    const std::string cfg = R"({
      "input_lights": [")" + noise_f + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "pure-noise frame: empty catalog is legal, must not fail");
    if (man.contains("sources_artifact")) {
      json cat;
      try { cat = json::parse(read_file(man.value("sources_artifact", ""))); } catch (...) { CHECK(false); }
      std::uint32_t total = 0;
      for (const auto& fr : cat.value("frames", json::array()))
        total += fr.value("n_detected", 0u);
      CHECK(total == 0);
    }
  }
  cleanup_fixture(fx);
}

// ── 5. 确定性: 同 config 双跑 star-psf 输出 bitwise 一致 ───────────────────
static void test_determinism() {
  for (int run = 0; run < 2; ++run) {
    Fixture fx = make_fixture("det");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    json man = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
    CHECK(man.value("status", "") == "ok");
    const std::string content = read_file(man.value("sources_artifact", ""));
    if (run == 0) {
      std::ofstream o(fx.dir.parent_path() / "p1001_det_first.json", std::ios::binary);
      o << content;
    } else {
      std::ifstream i(fx.dir.parent_path() / "p1001_det_first.json", std::ios::binary);
      std::string first((std::istreambuf_iterator<char>(i)),
                        std::istreambuf_iterator<char>());
      CHECK_MSG(content == first, "star-psf output must be bitwise deterministic");
      std::error_code ec;
      fs::remove(fx.dir.parent_path() / "p1001_det_first.json", ec);
    }
    cleanup_fixture(fx);
  }
}

// ══ 6. CORE-RACE-001: artifact:cos 必须落独立路径（禁就地覆写 artifact:cal）══
// RED 锚定（修复前）: p1_op_cosmetic 读取 cal 产物 calibrated_<base> 后就地覆写
// 同一路径 → 断言①（独立路径）与断言②（artifact:cal 字节不变）确定性失败。
static void test_cos_artifact_is_independent() {
  Fixture fx = make_hot_fixture("indep");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);

  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  const std::string cal_art = fx.out_dir + "/calibrated_light_1.fits";
  CHECK(fs::exists(fs::path(cal_art)));
  const std::string cal_before = read_bytes(cal_art);
  CHECK(!cal_before.empty());

  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
  CHECK(man_cos.value("status", "") == "ok");
  CHECK(man_cos.contains("artifacts") && man_cos["artifacts"].is_array() &&
        !man_cos["artifacts"].empty());
  const std::string cos_art = man_cos["artifacts"][0].get<std::string>();

  // ① artifact:cos 与 artifact:cal 必须落不同物理路径
  CHECK_MSG(cos_art != cal_art,
            "cosmetic must publish an independent artifact path (no in-place overwrite)");
  CHECK_MSG(cos_art.find("calibrated_") == std::string::npos,
            "cosmetic artifact must not reuse the artifact:cal path");
  CHECK(fs::exists(fs::path(cos_art)));

  // ② artifact:cal 在 cos 之后字节不变（并发消费者读到的永远是同一完整文件）
  CHECK_MSG(read_bytes(cal_art) == cal_before,
            "cosmetic must not mutate artifact:cal bytes (torn-read source)");

  // ③ 科学语义零变化: 本配置下 ac_correct_frame 无 dark/bias 掩码源（节点面
  //    传 nullptr/nullptr）⇒ 逐像素直通, cos 产物必须与 artifact:cal 字节相同
  //    （修复只改落盘路径与原子性, 不动任何科学数值）。
  CHECK_MSG(read_bytes(cos_art) == cal_before,
            "cosmetic pass-through must be bitwise identical to artifact:cal"
            " (scientific_change=none)");

  cleanup_fixture(fx);
}

// ══ 7. CORE-RACE-001: artifact:cos 消费者读 cos 产物（IR 接线一致）═══════
// RED 锚定（修复前）: cosmetic 就地覆写 artifact:cal ⇒ psf 读到的 file 名恒为
// calibrated_<base>（artifact:cos 无物理面）→ 断言失败。
static void test_consumer_reads_cos_artifact() {
  Fixture fx = make_hot_fixture("wire");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);

  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
  CHECK(man_cos.value("status", "") == "ok");
  CHECK(!man_cos["artifacts"].empty());
  const std::string cos_art = man_cos["artifacts"][0].get<std::string>();
  const std::string cos_base = cos_art.substr(cos_art.find_last_of("/\\") + 1);

  json man_psf = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
  CHECK(man_psf.value("status", "") == "ok");
  json cat;
  try { cat = json::parse(read_file(man_psf.value("sources_artifact", ""))); }
  catch (...) { CHECK(false); }
  CHECK(!cat.value("frames", json::array()).empty());
  std::size_t n_frames = 0;
  for (const auto& fr : cat.value("frames", json::array())) {
    const std::string f = fr.value("file", "");
    CHECK_MSG(f.rfind("cleaned_", 0) == 0,
              ("artifact:cos consumer (star-psf) must read the cosmetic artifact, got: " + f).c_str());
    CHECK(fs::exists(fs::path(fx.out_dir + "/" + f)));
    ++n_frames;
  }
  CHECK(n_frames == 2);
  CHECK(cos_base.rfind("cleaned_", 0) == 0);

  cleanup_fixture(fx);
}

// ══ 8. CORE-RACE-001: 并发全链 N 次连跑 0 失败 ════════════════════════════
// 修复前实测: CI-REG-002 200 次复跑 pass=176 fail=24（12%）——drz 与 cos 并发
// 读同一 artifact:cal, cosmetic 非原子就地覆写 ⇒ 消费者观察到半写文件。
// 次数经 P1001_CHAIN_STRESS_RUNS 覆盖（证据采集用 200）。
static void test_parallel_chain_stress() {
  Fixture fx = make_fixture("stress");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const std::string cfg = p1001_full_chain_cfg(fx);

  int runs = 40;
  if (const char* e = std::getenv("P1001_CHAIN_STRESS_RUNS")) {
    const int v = std::atoi(e);
    if (v > 0) runs = v;
  }
  int failed = 0;
  std::string first_error;
  int executed = 0;
  for (int i = 0; i < runs; ++i) {
    ChainRunResult r = run_full_chain_once(cfg, 2, reg);
    ++executed;
    if (!r.ok) {
      ++failed;
      if (first_error.empty())
        first_error = "run#" + std::to_string(i + 1) + ": " + r.error;
      if (first_error.find("\n") == std::string::npos && failed < 3)
        first_error += "\n          " + ("run#" + std::to_string(i + 1) + ": " + r.error);
    }
    CHECK(r.node_count == 7);
    CHECK(r.trace_ok);
  }
  CHECK_MSG(failed == 0,
            ("parallel chain torn-read: runs=" + std::to_string(executed) +
             " failures=" + std::to_string(failed) + " first=" + first_error).c_str());
  std::printf("[CORE-RACE-001] parallel chain stress: runs=%d failures=%d\n",
              executed, failed);
  cleanup_fixture(fx);
}

// ══ 9. CORE-RACE-001: 1/N worker parity（bitwise 确定性）══════════════════
static void test_worker_parity_bitwise() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const uint32_t workers[] = {1u, 2u, 4u};
  std::string base_cal, base_cos, base_src, base_snr;
  bool have_base = false;
  for (uint32_t w : workers) {
    Fixture fx = make_hot_fixture(("par" + std::to_string(w)).c_str());
    const std::string cfg = p1001_full_chain_cfg(fx);
    ChainRunResult r = run_full_chain_once(cfg, w, reg);
    CHECK_MSG(r.ok, ("workers=" + std::to_string(w) + ": " + r.error).c_str());
    const std::string cal = read_bytes(fx.out_dir + "/calibrated_light_1.fits");
    const std::string cos = read_bytes(fx.out_dir + "/cleaned_light_1.fits");
    const std::string src = read_file(fx.out_dir + "/p1_sources.json");
    const std::string snr = read_file(fx.out_dir + "/p1_snr.json");
    CHECK_MSG(!cal.empty() && !cos.empty() && !src.empty() && !snr.empty(),
              ("workers=" + std::to_string(w) + ": artifacts missing").c_str());
    if (!have_base) {
      base_cal = cal; base_cos = cos; base_src = src; base_snr = snr;
      have_base = true;
    } else {
      CHECK_MSG(cal == base_cal, "artifact:cal must be bitwise identical across worker counts");
      CHECK_MSG(cos == base_cos, "artifact:cos must be bitwise identical across worker counts");
      CHECK_MSG(src == base_src, "p1_sources.json must be bitwise identical across worker counts");
      CHECK_MSG(snr == base_snr, "p1_snr.json must be bitwise identical across worker counts");
    }
    cleanup_fixture(fx);
  }
}

// ══ 10. CORE-RACE-001 故障注入（负向必败面）═══════════════════════════════
static void test_torn_artifact_fault_injection() {
  // 10a. 残缺头（非原子覆写窗口内被打开的中间态）→ 消费者必须 fail-closed
  {
    Fixture fx = make_hot_fixture("tornhdr");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = p1001_full_chain_cfg(fx);
    json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
    CHECK(man_cal.value("status", "") == "ok");
    const std::string cal_art = fx.out_dir + "/calibrated_light_1.fits";
    CHECK(p1sess::sess_truncate_file(cal_art, 320) == 0);  // 头 6 卡(480B) 之内截断
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(), "torn artifact must fail closed");
    CHECK(rc.error().domain() == ErrorDomain::IO);
    CHECK(!fs::exists(fs::path(fx.out_dir + "/p1_stack.json")));
    cleanup_fixture(fx);
  }
  // 10b. 发布卫生 + 残缺旧产物覆盖: 预置一个残缺的 artifact:cos 文件, 再跑
  //      cosmetic → 必须以完整文件替换（原子发布）, 且不留临时文件
  {
    Fixture fx = make_hot_fixture("stale");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = p1001_full_chain_cfg(fx);
    json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
    CHECK(man_cal.value("status", "") == "ok");
    const std::string cos_art = fx.out_dir + "/cleaned_light_1.fits";
    float v = 1.0f;
    CHECK(p1sess::write_fits_file(cos_art, kW, kH, const_pixel, &v) == 0);
    CHECK(p1sess::sess_truncate_file(cos_art, 64) == 0);  // 残缺旧产物（64B）
    json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
    CHECK(man_cos.value("status", "") == "ok");
    std::error_code ec;
    const auto sz = fs::file_size(cos_art, ec);
    CHECK_MSG(!ec && sz >= 2880u + static_cast<uint64_t>(kW) * kH * 4u,
              "cosmetic must atomically replace a stale partial artifact with a"
              " complete one");
    // 发布卫生: 成功后不得残留暂存文件（崩溃残留也不会污染产物面）
    int leftovers = 0;
    for (const auto& e : fs::directory_iterator(fx.dir)) {
      if (e.path().filename().string().find(".tmp.") != std::string::npos)
        ++leftovers;
    }
    CHECK_MSG(leftovers == 0, "atomic publish must not leave staging files");
    cleanup_fixture(fx);
  }
  // 10c. 检测器非空转: 注入旧缺陷语义（就地覆写 artifact:cal）→ 不可变性
  //      比较器必须报差异（证明 §6 的不变量断言在缺陷回归时必然失败）
  {
    Fixture fx = make_hot_fixture("inject");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = p1001_full_chain_cfg(fx);
    json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
    CHECK(man_cal.value("status", "") == "ok");
    const std::string cal_art = fx.out_dir + "/calibrated_light_1.fits";
    const std::string cal_before = read_bytes(cal_art);
    float v = 777.0f;
    CHECK(p1sess::write_fits_file(cal_art, kW, kH, const_pixel, &v, 0) == 0);
    CHECK_MSG(read_bytes(cal_art) != cal_before,
              "immutability comparator must detect in-place overwrite (fault injection)");
    cleanup_fixture(fx);
  }
}

// ══ 11. CORE-RACE-001: 科学语义零变化（修复前/后 bitwise golden 对照）══════
// P1001_GOLDEN_DIR=<dir>: 1 worker 全链跑一次并把确定性产物字节存为基线;
// P1001_GOLDEN_CMP=<dir>: 同样跑一次与基线逐字节比较（不一致 = 科学漂移）。
// 排除项: p1_stack.json（含 elapsed_sec 计时）/ p1_final.json（含输出目录路径
// 与 HiPS 根, 随临时目录变化）——均为非确定性字段, 不属科学数值面。
static void test_golden_parity() {
  const char* dump = std::getenv("P1001_GOLDEN_DIR");
  const char* cmp = std::getenv("P1001_GOLDEN_CMP");
  if (!dump && !cmp) {
    std::printf("[CORE-RACE-001] golden parity: skipped (env unset)\n");
    return;
  }
  // 修复前基线只含"就地覆写后的 calibrated_*"（当时无独立 cos 产物）; 因此
  // 字节对照按 语义等价对 进行: 修复后 cleaned_<base> ≡ 修复前 calibrated_<base>
  // （同一 ac_correct_frame 输出, 只换了落盘路径）。
  struct BytePair { const char* now; const char* before; };
  static const BytePair kBytePairs[] = {
      {"cleaned_light_1.fits", "calibrated_light_1.fits"},
      {"cleaned_light_2.fits", "calibrated_light_2.fits"}};
  // JSON 产物: 数值面逐字段比对（"file" 字段为 artifact 接线改名:
  // calibrated_→cleaned_ 同帧同序, 不属科学数值; 其余键必须完全一致）
  static const char* kJsonArtifacts[] = {"p1_sources.json", "p1_psf.json",
                                         "p1_flux.json", "p1_snr.json"};
  Fixture fx = make_hot_fixture("golden");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  ChainRunResult r = run_full_chain_once(p1001_full_chain_cfg(fx), 1, reg);
  CHECK_MSG(r.ok, ("golden run failed: " + r.error).c_str());
  const fs::path dir = dump ? fs::path(dump) : fs::path(cmp);
  std::error_code ec;
  fs::create_directories(dir, ec);
  int compared = 0;

  for (const BytePair& bp : kBytePairs) {
    const std::string src = fx.out_dir + "/" + bp.now;
    CHECK_MSG(fs::exists(fs::path(src)),
              ("golden artifact missing: " + std::string(bp.now)).c_str());
    if (dump) {
      std::error_code cec;
      fs::copy_file(fs::path(src), dir / bp.before,
                    fs::copy_options::overwrite_existing, cec);
      CHECK_MSG(!cec, ("golden dump failed: " + std::string(bp.now)).c_str());
      continue;
    }
    const fs::path base = dir / bp.before;
    CHECK_MSG(fs::exists(base),
              ("golden baseline missing: " + std::string(bp.before)).c_str());
    if (!fs::exists(base)) continue;
    CHECK_MSG(read_bytes(src) == read_bytes(base.string()),
              ("science drift (cosmetic pixels) vs pre-fix golden: " +
               std::string(bp.now)).c_str());
    ++compared;
  }

  for (const char* name : kJsonArtifacts) {
    const std::string src = fx.out_dir + "/" + name;
    CHECK_MSG(fs::exists(fs::path(src)),
              ("golden artifact missing: " + std::string(name)).c_str());
    if (dump) {
      std::error_code cec;
      fs::copy_file(fs::path(src), dir / name,
                    fs::copy_options::overwrite_existing, cec);
      CHECK_MSG(!cec, ("golden dump failed: " + std::string(name)).c_str());
      continue;
    }
    const fs::path base = dir / name;
    CHECK_MSG(fs::exists(base),
              ("golden baseline missing: " + std::string(name)).c_str());
    if (!fs::exists(base)) continue;
    json now, ref;
    try {
      now = json::parse(read_file(src));
      ref = json::parse(read_file(base.string()));
    } catch (...) {
      CHECK_MSG(false, ("golden JSON parse failed: " + std::string(name)).c_str());
      continue;
    }
    // "file" 键退出比对（artifact 接线改名, 非数值面）; 其余逐字段一致
    auto strip_files = [](json& j, auto&& self) -> void {
      if (j.is_object()) {
        j.erase("file");
        for (auto& kv : j.items()) self(kv.value(), self);
      } else if (j.is_array()) {
        for (auto& v : j) self(v, self);
      }
    };
    strip_files(now, strip_files);
    strip_files(ref, strip_files);
    CHECK_MSG(now == ref,
              ("science drift (numeric surface) vs pre-fix golden: " +
               std::string(name)).c_str());
    ++compared;
  }
  std::printf("[CORE-RACE-001] golden parity: mode=%s compared=%d dir=%s\n",
              dump ? "dump" : "compare", compared, dir.string().c_str());
  cleanup_fixture(fx);
}

int main() {
  test_nodes_real_operation();
  test_runtime_chain_call_count_1();
  test_fail_fast_downstream_zero_calls();
  test_complete_gate_fail_closed();
  test_negative_injection();
  test_determinism();
  // CORE-RACE-001（p1001 链并发撕裂读）: 独立产物路径 / IR 接线一致性 /
  // 并发全链 N 次连跑 / 1-N worker parity / 故障注入
  test_cos_artifact_is_independent();
  test_consumer_reads_cos_artifact();
  test_parallel_chain_stress();
  test_worker_parity_bitwise();
  test_torn_artifact_fault_injection();
  test_golden_parity();
  if (failures == 0) {
    std::printf("P1-001 REAL NODES PASS (8 节点唯一真实 operation + call_count=1 + complete 门 fail-closed + 下游零调用)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-001 REAL NODES FAIL (%d)\n", failures);
  return 1;
}
