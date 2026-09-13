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
#include "p1_session.h"
#include "wcs_tan.h"     // B2-A17: linear WCS forward Oracle  // complete 门: API-P1-001 冻结 C ABI

#include "p1sess_fixtures.hpp"  // 最小 FITS writer (手写, 不调生产 symbol)

// B2-A12/A13/A14/A16: HISS 读面 + FITS 读面 (Oracle 对拍用; AIO 由
// astrocs_module_adapters PUBLIC 传递 include 与 AIO_ENABLE_HEALPIX=1)
#include "aio_healpix_io.h"
#include "astro_image_io.h"
#include "hiss_format.h"  // B2-A15: 稀疏多 tile HISS 夹具

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
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
    CHECK(f.value("covered_area_model", "") == "hiss_support_ratio_x_A_cell");
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

// ══════════════════════════════════════════════════════════════════════════
// B2-A12/A13/A14/A16 科学缺省 / provenance / fail-closed 新门
// (findings: aud-p1 P1-1/P2-9=P1 precision; P1-5=dark K; P1-6=PHOTAPPL;
//  P1-4=photometry fail-open)
// RED→GREEN: 以下断言在修复前失败、修复后通过。
// ══════════════════════════════════════════════════════════════════════════

// HISS header meta_json (只读头, 不加载 tile 数据)
std::string hiss_meta_json(const std::string& path) {
  uint32_t nside = 0, tn = 0, depth = 0, nleaf = 0;
  uint64_t ntiles = 0, npix = 0;
  char* meta = nullptr;
  const int rc = aio_hiss_inspect(path.c_str(), &nside, &tn, &depth, &nleaf,
                                  &ntiles, &npix, &meta, nullptr);
  std::string s;
  if (rc == 0 && meta) s = meta;
  if (meta) aio_hio_free(meta);
  return s;
}

// 读 .hiss 全部 tile signal (FP32/FP64), key = parent_ipix
bool hiss_tile_signals(const std::string& path, bool f64,
                       std::map<uint64_t, std::vector<double>>* out) {
  uint32_t nside = 0, tn = 0, depth = 0, nleaf = 0;
  uint64_t ntiles = 0, npix = 0;
  char* meta = nullptr;
  uint64_t* ipix = nullptr;
  if (aio_hiss_inspect(path.c_str(), &nside, &tn, &depth, &nleaf, &ntiles, &npix,
                       &meta, &ipix) != 0) {
    if (meta) aio_hio_free(meta);
    return false;
  }
  bool ok = true;
  for (uint64_t t = 0; t < ntiles && ok; ++t) {
    std::vector<double> vals;
    if (f64) {
      double* s = nullptr; uint32_t n = 0;
      if (aio_hiss_read_tile_signal_f64(path.c_str(), ipix[t], &s, &n) != 0) ok = false;
      else vals.assign(s, s + n);
      if (s) aio_hio_free(s);
    } else {
      float* s = nullptr; uint32_t n = 0;
      if (aio_hiss_read_tile_signal(path.c_str(), ipix[t], &s, &n) != 0) ok = false;
      else for (uint32_t i = 0; i < n; ++i) vals.push_back(static_cast<double>(s[i]));
      if (s) aio_hio_free(s);
    }
    if (ok) (*out)[ipix[t]] = std::move(vals);
  }
  if (ipix) aio_hio_free(ipix);
  if (meta) aio_hio_free(meta);
  return ok;
}

// 常数场 + 可控 FITS EXPTIME 的校准 fixture (B2-A13 Oracle 期望可解析)。
// exptime < 0 → 不写 EXPTIME 卡 (缺失负例)。
Fixture make_exptime_fixture(const char* tag, float lv, float bv, float dv,
                             float fv, double lexp = -1.0, double dexp = -1.0,
                             double bexp = 10.0, double fexp = 1.0) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_expt_" + std::string(tag) + "_" + std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.light2 = (f.dir / "light_2.fits").string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  float v = lv;
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, const_pixel, &v, 0, lexp) == 0);
  CHECK(p1sess::write_fits_file(f.light2, kW, kH, const_pixel, &v, 0, lexp) == 0);
  v = bv; CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &v, 0, bexp) == 0);
  v = dv; CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &v, 0, dexp) == 0);
  v = fv; CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &v, 0, fexp) == 0);
  return f;
}

// ── B2-A16: 测光节点缺上游 artifact / 缺帧 → DATA fail-closed ─────────────
static void test_b2a16_photometry_fail_closed() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  // 16a: 缺 p1_sources.json → 必须失败, 不得写 p1_flux.json
  {
    Fixture fx = make_fixture("b2a16a");
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A16: missing p1_sources.json must fail (older code returned success)");
    if (rc.failed()) CHECK(rc.error().domain() == ErrorDomain::DATA);
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/p1_flux.json")),
              "B2-A16: no p1_flux.json on missing upstream artifact");
    cleanup_fixture(fx);
  }
  // 16b: sources 引用缺失帧 → 失败 (帧缺失按合同上抛, 不静默跳过)
  {
    Fixture fx = make_fixture("b2a16b");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"missing_frame.fits","sources":[{"id":"s1","x":16,"y":16}]}]})";
    }
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A16: frame referenced by p1_sources.json missing must fail");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/p1_flux.json")),
              "B2-A16: no p1_flux.json when a frame is missing");
    cleanup_fixture(fx);
  }
  // 16c: 正向 — 上游 sources 合法 → success 且写 p1_flux.json + p1_phot.json
  {
    Fixture fx = make_fixture("b2a16c");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"light_1.fits","sources":[{"id":"s1","x":16,"y":16}]}]})";
    }
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "B2-A16: valid upstream sources must succeed");
    CHECK(man.value("operation", "") == "measure_flux");
    CHECK(fs::exists(fs::path(fx.out_dir + "/p1_flux.json")));
    cleanup_fixture(fx);
  }
}

// ── B2-A13: dark_opt K 由 FITS EXPTIME 推导 + 缺失/不一致 fail-closed ──────
static void test_b2a13_dark_scale_from_exptime() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const float L = 100.0f, B = 10.0f, D = 5.0f, F = 1.0f;
  const double tl = 300.0, td = 600.0;
  const double K = tl / td;  // 0.5 (SCI-CAL-001 §5: K=t_light/t_dark)
  // 13a: K == t_light/t_dark, 且校准像素 == 独立 Oracle (L-B-K*(D-B))/F
  {
    Fixture fx = make_exptime_fixture("b2a13a", L, B, D, F, tl, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), ("B2-A13: dark_opt K from EXPTIME must succeed: " +
                        (rc.failed() ? rc.error().message() : std::string())).c_str());
    if (rc.ok()) {
      double got = -1.0;
      if (man.contains("stages") && man["stages"].is_array()) {
        for (const auto& st : man["stages"]) {
          if (st.value("name", "") == "calibrate" && st.contains("per_frame") &&
              st["per_frame"].is_array() && !st["per_frame"].empty())
            got = st["per_frame"][0].value("dark_scale", -1.0);
        }
      }
      CHECK_MSG(std::fabs(got - K) < 1e-9,
                ("B2-A13: dark_scale must equal t_light/t_dark=" + std::to_string(K) +
                 " got=" + std::to_string(got)).c_str());
      // Oracle: 独立解析期望 = (L-B-K*(D-B))/F
      const std::string out_fits = fx.out_dir + "/calibrated_light_1.fits";
      CHECK(fs::exists(fs::path(out_fits)));
      AIOImageData* im = aio_read(out_fits.c_str());
      CHECK_MSG(im != nullptr, "B2-A13: calibrated FITS must be readable");
      if (im) {
        const float* px = aio_get_pixel_data(im);
        const double expect = (L - B - K * (D - B)) / F;
        double maxdiff = 0.0;
        const int64_t n = static_cast<int64_t>(kW) * kH;
        for (int64_t i = 0; i < n; ++i)
          maxdiff = std::max(maxdiff, std::fabs(static_cast<double>(px[i]) - expect));
        CHECK_MSG(maxdiff < 1e-4,
                  ("B2-A13: K=EXPTIME Oracle max|out-expected|=" +
                   std::to_string(maxdiff)).c_str());
        aio_free_image_data(im);
      }
    }
    cleanup_fixture(fx);
  }
  // 13b: dark EXPTIME 缺失 → DATA fail
  {
    Fixture fx = make_exptime_fixture("b2a13b", L, B, D, F, tl, -1.0);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A13: dark EXPTIME missing must fail-closed");
    if (rc.failed()) CHECK(rc.error().domain() == ErrorDomain::DATA);
    CHECK(!fs::exists(fs::path(fx.out_dir + "/calibrated_light_1.fits")));
    cleanup_fixture(fx);
  }
  // 13c: light EXPTIME 缺失 → DATA fail
  {
    Fixture fx = make_exptime_fixture("b2a13c", L, B, D, F, -1.0, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A13: light EXPTIME missing must fail-closed");
    cleanup_fixture(fx);
  }
  // 13d: 配置 dark_scale_factor 与 EXPTIME 比不一致 → DATA fail (禁标量冒充科学输入)
  {
    Fixture fx = make_exptime_fixture("b2a13d", L, B, D, F, tl, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true,
      "dark_scale_factor": 2.0
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A13: dark_scale_factor disagreeing with EXPTIME K must fail");
    cleanup_fixture(fx);
  }
}

// ── B2-A12: drizzle precision_mode 缺省门 + FP32/FP64 等价性 ───────────────
static void test_b2a12_precision_default_and_equiv() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const char* wcs = R"("wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},)";
  // 12a: 缺 precision_mode → DATA 拒绝 (无 silent FP32 缺省), 不写 p1_stack.json
  {
    Fixture fx = make_fixture("b2a12a");
    RunContext ctx;
    const std::string cfg = std::string(R"({
      "input_lights": [")") + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      )" + wcs + R"(
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0}
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A12: missing precision_mode must be rejected (no silent FP32)");
    if (rc.failed()) CHECK(rc.error().domain() == ErrorDomain::DATA);
    CHECK(!fs::exists(fs::path(fx.out_dir + "/p1_stack.json")));
    cleanup_fixture(fx);
  }
  // 12b: FP32/FP64 等价性 + p1_stack.json 记录 precision_mode
  {
    Fixture fx32 = make_fixture("b2a12f32");
    Fixture fx64 = make_fixture("b2a12f64");
    ModuleRegistry reg2;
    CHECK(register_phase_modules(reg2).ok());
    auto run = [&](Fixture& fx, int mode, const char* tag) -> json {
      RunContext ctx;
      const std::string cfg = std::string(R"({
        "input_lights": [")") + fx.light1 + R"("],
        "output_dir": ")" + fx.out_dir + R"(",
        )" + wcs + R"(
        "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": )" +
        std::to_string(mode) + R"(}
      })";
      Result<void> rc;
      json man = run_node(reg2, "astrocs.phase1.drizzle", cfg, ctx, &rc);
      CHECK_MSG(rc.ok(), (std::string("B2-A12: drizzle ") + tag +
                          " must succeed: " +
                          (rc.failed() ? rc.error().message() : std::string())).c_str());
      return man;
    };
    json m32 = run(fx32, 0, "FP32");
    json m64 = run(fx64, 1, "FP64");
    // p1_stack.json provenance: precision_mode 显式记录 (禁隐式缺省)
    auto stack_prec = [](Fixture& fx) -> int {
      json s;
      try { s = json::parse(read_file(fx.out_dir + "/p1_stack.json")); } catch (...) { return -99; }
      return s.value("precision_mode", -99);
    };
    CHECK_MSG(stack_prec(fx32) == 0, "B2-A12: p1_stack.json must record precision_mode=0");
    CHECK_MSG(stack_prec(fx64) == 1, "B2-A12: p1_stack.json must record precision_mode=1");
    // 等价性: 同一输入 FP32/FP64 累积逐 tile signal 相对一致 (输出窄化 FP32)
    std::map<uint64_t, std::vector<double>> s32, s64;
    const bool ok32 = hiss_tile_signals(fx32.out_dir + "/p1_stack.hiss", false, &s32);
    const bool ok64 = hiss_tile_signals(fx64.out_dir + "/p1_stack.hiss", true, &s64);
    CHECK_MSG(ok32 && ok64, "B2-A12: FP32/FP64 .hiss signal tiles must be readable");
    CHECK_MSG(s32.size() == s64.size() && !s32.empty(),
              "B2-A12: FP32/FP64 must touch the same tile set");
    double max_rel = 0.0;
    for (const auto& [ipix, v32] : s32) {
      auto it = s64.find(ipix);
      if (it == s64.end()) { max_rel = 1e9; break; }
      const std::vector<double>& v64 = it->second;
      if (v64.size() != v32.size()) { max_rel = 1e9; break; }
      for (size_t i = 0; i < v32.size(); ++i) {
        const double a = v32[i], b = v64[i];
        const double denom = std::max(1.0, std::fabs(b));
        max_rel = std::max(max_rel, std::fabs(a - b) / denom);
      }
    }
    CHECK_MSG(max_rel < 1e-5,
              ("B2-A12: FP32/FP64 signal equivalence max_rel=" +
               std::to_string(max_rel)).c_str());
    cleanup_fixture(fx32);
    cleanup_fixture(fx64);
  }
}

// ── B2-A14: drizzle PHOTAPPL/PHOTSCAL 由真实 provenance 决定 (禁硬编码 1) ──
static void test_b2a14_photappl_provenance() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const char* wcs = R"("wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},)";
  auto run_drz = [&](Fixture& fx, json* meta_out) -> bool {
    RunContext ctx;
    const std::string cfg = std::string(R"({
      "input_lights": [")") + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      )" + wcs + R"(
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1}
    })";
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    if (rc.failed()) {
      std::fprintf(stderr, "DBG B2-A14 drizzle failed: %s\n", rc.error().message().c_str());
      return false;
    }
    try { *meta_out = json::parse(hiss_meta_json(fx.out_dir + "/p1_stack.hiss")); }
    catch (...) { return false; }
    return true;
  };
  // 14a: 无测光 provenance → PHOTAPPL=0 / BUNIT=ADU (绝不伪造 1)
  {
    Fixture fx = make_fixture("b2a14a");
    json meta = json::object();
    const bool ok = run_drz(fx, &meta);
    CHECK_MSG(ok, "B2-A14: drizzle must complete with explicit ADU degradation");
    if (ok) {
      CHECK_MSG(meta.value("photappl", -1) == 0,
                "B2-A14: PHOTAPPL must be 0 when no photometry provenance (forged 1 removed)");
      CHECK_MSG(meta.value("bunit", std::string()) == "ADU",
                "B2-A14: BUNIT must degrade to ADU, not RELATIVE_FLUX");
    }
    cleanup_fixture(fx);
  }
  // 14b: 真实测光 provenance 声明已应用 → PHOTAPPL=1 + PHOTSCAL 原样透传
  {
    Fixture fx = make_fixture("b2a14b");
    {
      std::ofstream o(fx.out_dir + "/p1_phot.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTPROV","node":"astrocs.phase1.photometry",)"
           R"("operation":"measure_flux","photometry_applied":true,"photscal":0.5,"pixel_scaling":"applied"})";
    }
    json meta = json::object();
    const bool ok = run_drz(fx, &meta);
    CHECK_MSG(ok, "B2-A14: drizzle with valid photometry provenance must succeed");
    if (ok) {
      CHECK_MSG(meta.value("photappl", -1) == 1,
                "B2-A14: PHOTAPPL=1 must be driven by real photometry provenance");
      CHECK_MSG(std::fabs(meta.value("photscal", -1.0) - 0.5) < 1e-12,
                "B2-A14: PHOTSCAL must come from provenance, not hardcoded/config");
      CHECK_MSG(meta.value("bunit", std::string()) == "ASTROCS_RELATIVE_FLUX",
                "B2-A14: BUNIT=RELATIVE_FLUX only when provenance says applied");
    }
    cleanup_fixture(fx);
  }
}

// ── 5. 确定性: 同 config 双跑 star-psf 输出 bitwise 一致 ───────────────────
// ── B2-A15: 稀疏多 standard tile HISS 夹具 + HiPS 读面 ────────────────
// nside=512 → HISS tile_nside=16, 256 leaf/tile; 每个 IVOA 标准 512 tile 由
// 64 个 HISS tile 覆盖。写 2 个 HISS tile (parent 0 与 64) 分落于 standard
// tile 0 与 1, 各自只覆盖前 512 个 local leaf (部分覆盖且稀疏)。
constexpr uint64_t kA15CoverLeaves = 512;   // 兼容旧引用 (已废弃)
// 每个 HISS tile 的覆盖卷数必须不同: 第二个 tile 的未覆盖偏移
// 落在第一个 tile 的覆盖区 = 幽灵可观测。
constexpr uint64_t kA15Cover0 = 768;   // tile 0: HISS local 0..767 (partial)
constexpr uint64_t kA15Cover1 = 255;   // tile 1: HISS local 0..254 (partial)
// HISS tile span = 1024 leaf (depth=5, tile_nside=16); IVOA standard 512 tile =
// 262144 leaf = 256 HISS tile. parent 0 与 256 因此分落在 standard tile 0/1
// (各自 leaf 0 与 524288 → standard tile 内偏移均为 0, 故旧实现 stale buffer
// 会把 tile0 的 signal/coverage 泄露到 tile1 的同偏移)。
constexpr uint64_t kA15Tile1Parent = 256;   // H*1024 >> 18 = 1 (standard tile 1)
constexpr uint8_t kA15Tile0Support = 128;   // 部分覆盖 128/255 ≈ 0.502
constexpr uint8_t kA15Tile1Support = 64;    // 64/255 ≈ 0.251
constexpr double kA15SignalV = 0.5;

bool make_sparse_hiss(const std::string& path) {
  const uint32_t nside = 512;
  const uint32_t depth = hiss::compute_tile_depth(nside);
  const uint32_t tile_nside = hiss::compute_tile_nside(nside);
  const uint32_t n_leaf = 1u << (2 * depth);
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(nside) *
                         static_cast<double>(nside));
  hiss::HissGridSpec grid;
  grid.nside = nside; grid.tile_nside = tile_nside;
  grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
  hiss::HissMetadata hmeta;
  hmeta.nside = nside; hmeta.tile_nside = tile_nside;
  hmeta.ordering = 1; hmeta.radesys = 0; hmeta.pixfrac = 1.0;
  hmeta.photappl = 0;
  std::snprintf(hmeta.bunit, sizeof(hmeta.bunit), "ADU");
  hiss::HissWriter writer;
  if (writer.open(path, grid, hmeta) != 0) return false;
  const uint64_t parents[2] = {0, kA15Tile1Parent};
  const uint64_t covers[2] = {kA15Cover0, kA15Cover1};
  const uint8_t sups[2] = {kA15Tile0Support, kA15Tile1Support};
  for (int t = 0; t < 2; ++t) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = parents[t];
    acc.pixel_area = a_cell;
    acc.pixels.resize(n_leaf);
    const uint64_t n_cover = covers[t];
    for (uint64_t i = 0; i < n_cover; ++i) {
      acc.pixels[i].sum_flux = kA15SignalV;
      acc.pixels[i].sum_area =
          (static_cast<double>(sups[t]) / 255.0) * a_cell;
    }
    if (writer.add_tile(parents[t], acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
      writer.cancel();
      return false;
    }
  }
  return writer.finalize() == 0;
}

bool read_hips_tile(const std::string& path, std::vector<float>* out) {
  AIOImageData* im = aio_read(path.c_str());
  if (!im) return false;
  const int iw = aio_get_width(im), ih = aio_get_height(im);
  const float* p = aio_get_pixel_data(im);
  if (!p || iw != 512 || ih != 512) { aio_free_image_data(im); return false; }
  out->assign(p, p + static_cast<size_t>(iw) * static_cast<size_t>(ih));
  aio_free_image_data(im);
  return true;
}

bool hips_tile_signal(const std::string& root, uint64_t tile, std::vector<float>* out) {
  return read_hips_tile(root + "/signal/Norder0/Dir" + std::to_string(tile / 10000) +
                        "/Npix" + std::to_string(tile % 10000) + ".fits", out);
}
bool hips_tile_support(const std::string& root, uint64_t tile, std::vector<float>* out) {
  return read_hips_tile(root + "/support/Norder0/Dir" + std::to_string(tile / 10000) +
                        "/Npix" + std::to_string(tile % 10000) + ".fits", out);
}
// ── B2-A15: writer stale buffer / support 量化 (P1-7 + P1-8) ───────────
// 稀疏 HISS: 2 个 standard tile 各被一个 HISS tile 部分覆盖 (support 128/64
// of 255)。不依赖 FITS 数组与 NESTED 的具体转换, 用不变量判定:
//   (a) 每个标准 tile 有效像素数 == HISS 覆盖卷数 (1024),
//       无幻灵 / 无跨 parent 泄露 (旧实现 buffer 不清零 → 多余有效像素);
//   (b) support 严格按 HISS 面积比连续缩放 (128/255, 64/255),
//       而非塑成 0/1 (P1-8)。
static void test_b2a15_writer_stale_buffer_and_support() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  Fixture fx = make_fixture("b2a15");
  const std::string hiss = fx.out_dir + "/p1_stack.hiss";
  CHECK_MSG(make_sparse_hiss(hiss), "B2-A15: sparse multi-tile HISS fixture");
  {
    uint32_t ns = 0, tn = 0, dp = 0, nl = 0; uint64_t nt = 0, np = 0;
    char* meta = nullptr; uint64_t* tips = nullptr;
    CHECK(aio_hiss_inspect(hiss.c_str(), &ns, &tn, &dp, &nl, &nt, &np, &meta,
                           &tips) == 0);
    CHECK(ns == 512 && nt == 2 && tn == 16);
    CHECK(tips[0] == 0 && tips[1] == kA15Tile1Parent);
    CHECK(nl * kA15Tile1Parent == 262144u);
    if (meta) aio_hio_free(meta);
    if (tips) aio_hio_free(tips);
  }
  RunContext ctx;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "filter_passband": "R"
  })";
  Result<void> wrc;
  json wman = run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("B2-A15: writer must consume sparse HISS: " +
                       (wrc.failed() ? wrc.error().message() : std::string())).c_str());
  if (wrc.failed()) { cleanup_fixture(fx); return; }
  std::vector<float> sig0, sup0, sig1, sup1;
  CHECK_MSG(hips_tile_signal(fx.out_dir, 0, &sig0), "B2-A15: signal tile 0 readable");
  CHECK_MSG(hips_tile_support(fx.out_dir, 0, &sup0), "B2-A15: support tile 0 readable");
  CHECK_MSG(hips_tile_signal(fx.out_dir, 1, &sig1), "B2-A15: signal tile 1 readable");
  CHECK_MSG(hips_tile_support(fx.out_dir, 1, &sup1), "B2-A15: support tile 1 readable");
  if (sig0.size() != 512ull * 512ull || sup0.size() != sig0.size() ||
      sig1.size() != sig0.size() || sup1.size() != sig0.size()) {
    CHECK_MSG(false, "B2-A15: HiPS tile size must be 512x512");
    cleanup_fixture(fx); return;
  }
  {  // n_tiles_written 属 p1_final.json 产物面字段 (节点 manifest 只报 B2-A10 字段)
    json fin0;
    try { fin0 = json::parse(read_file(fx.out_dir + "/p1_final.json")); } catch (...) {}
    CHECK(fin0.value("n_tiles_written", 0) == 2);
    CHECK(fin0.value("n_tiles", 0u) == 2u);
  }
  const double exp_sup0 = static_cast<double>(kA15Tile0Support) / 255.0;
  const double exp_sup1 = static_cast<double>(kA15Tile1Support) / 255.0;
  uint64_t valid0 = 0, valid1 = 0;
  uint64_t sup0_hits = 0, sup1_hits = 0;
  double max_dev0 = 0.0, max_dev1 = 0.0;
  bool collapsed = false, nan_leak = false;
  for (size_t i = 0; i < sig0.size(); ++i) {
    const bool fin0 = std::isfinite(sig0[i]);
    const double s0 = sup0[i];
    if (fin0) {
      ++valid0;
      if (!(std::fabs(s0 - exp_sup0) < 0.01)) collapsed = true;
      max_dev0 = std::max(max_dev0, std::fabs(s0 - exp_sup0));
    } else if (std::fabs(s0) > 1e-6) {
      nan_leak = true;   // signal invalid 但 support > 0 = 自相矛盾
    }
  }
  for (size_t i = 0; i < sig1.size(); ++i) {
    const bool fin1 = std::isfinite(sig1[i]);
    const double s1 = sup1[i];
    if (fin1) {
      ++valid1;
      if (!(std::fabs(s1 - exp_sup1) < 0.01)) collapsed = true;
      max_dev1 = std::max(max_dev1, std::fabs(s1 - exp_sup1));
    } else if (std::fabs(s1) > 1e-6) {
      nan_leak = true;
    }
  }
  CHECK_MSG(valid0 == kA15Cover0,
            ("B2-A15: tile 0 valid pixel count must equal HISS coverage (no ghost): " +
             std::to_string(valid0)).c_str());
  CHECK_MSG(valid1 == kA15Cover1,
            ("B2-A15: tile 1 valid pixel count must equal HISS coverage (no stale leak): " +
             std::to_string(valid1)).c_str());
  CHECK_MSG(!collapsed,
            ("B2-A15: support must scale continuously with HISS ratio " +
             std::to_string(kA15Tile0Support) + "/255 (tile0 dev=" +
             std::to_string(max_dev0) + " tile1 dev=" + std::to_string(max_dev1) +
             "), not collapse to 0/1").c_str());
  CHECK_MSG(!nan_leak, "B2-A15: invalid signal pixels must have zero support");
  CHECK_MSG(std::fabs(exp_sup0 - exp_sup1) > 0.1,
            "B2-A15 fixture must expose non-full support scaling");
  {
    json fin;
    try { fin = json::parse(read_file(fx.out_dir + "/p1_final.json")); } catch (...) {}
    CHECK_MSG(fin.value("covered_area_model", "") == "hiss_support_ratio_x_A_cell",
              "B2-A15: covered_area_model must record HISS support ratio scaling");
  }
  cleanup_fixture(fx);
}

// ── RESCUE A15 独立注入证明: 稀疏 / 多 parent / 覆盖不连续 ghost 场景 ──────
// 两个 HISS tile 落在**不同** standard parent, 且在各 parent 内的缓冲偏移不同
// (parent0 r=0 → offsets 0..1023; parent1 tile parent_ipix=257 → base_leaf
// 263168 → r=1024 → offsets 1024..2047)。旧实现 (每 parent 不清零) 会把
// parent0 的 signal/coverage/seen 残留给 parent1 的前 1024 偏移 → 幽灵像素。
// 回退 "每 parent 清零" 或 "valid_mask=seen" 任一 → 本测试必红。
constexpr uint64_t kA15GhostTile1 = 257;    // 263168>>18 = 1, 缓冲偏移 r=1024
constexpr uint64_t kA15GhostCover0 = 768;
constexpr uint64_t kA15GhostCover1 = 255;
constexpr uint8_t  kA15GhostSup0 = 128;
constexpr uint8_t  kA15GhostSup1 = 64;

bool make_sparse_hiss_offset(const std::string& path) {
  const uint32_t nside = 512;
  const uint32_t depth = hiss::compute_tile_depth(nside);
  const uint32_t tile_nside = hiss::compute_tile_nside(nside);
  const uint32_t n_leaf = 1u << (2 * depth);
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(nside) *
                         static_cast<double>(nside));
  hiss::HissGridSpec grid;
  grid.nside = nside; grid.tile_nside = tile_nside;
  grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
  hiss::HissMetadata hmeta;
  hmeta.nside = nside; hmeta.tile_nside = tile_nside;
  hmeta.ordering = 1; hmeta.radesys = 0; hmeta.pixfrac = 1.0;
  hmeta.photappl = 0;
  std::snprintf(hmeta.bunit, sizeof(hmeta.bunit), "ADU");
  hiss::HissWriter writer;
  if (writer.open(path, grid, hmeta) != 0) return false;
  const uint64_t parents[2] = {0, kA15GhostTile1};
  const uint64_t covers[2] = {kA15GhostCover0, kA15GhostCover1};
  const uint8_t sups[2] = {kA15GhostSup0, kA15GhostSup1};
  for (int t = 0; t < 2; ++t) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = parents[t];
    acc.pixel_area = a_cell;
    acc.pixels.resize(n_leaf);
    for (uint64_t i = 0; i < covers[t]; ++i) {
      acc.pixels[i].sum_flux = kA15SignalV;
      acc.pixels[i].sum_area = (static_cast<double>(sups[t]) / 255.0) * a_cell;
    }
    if (writer.add_tile(parents[t], acc, nullptr, hiss::OccupancyMode::FULL) != 0) {
      writer.cancel();
      return false;
    }
  }
  return writer.finalize() == 0;
}

static void test_b2a15_ghost_discontinuous_multiparent() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  Fixture fx = make_fixture("b2a15g");
  const std::string hiss = fx.out_dir + "/p1_stack.hiss";
  CHECK_MSG(make_sparse_hiss_offset(hiss), "A15 ghost fixture must be written");
  {
    uint32_t ns=0,tn=0,dp=0,nl=0; uint64_t nt=0,np=0;
    char* meta=nullptr; uint64_t* tips=nullptr;
    CHECK(aio_hiss_inspect(hiss.c_str(), &ns,&tn,&dp,&nl,&nt,&np,&meta,&tips) == 0);
    CHECK(ns == 512 && nt == 2 && nl == 1024);
    if (tips) CHECK(tips[0] == 0 && tips[1] == kA15GhostTile1);
    CHECK(nl * kA15GhostTile1 == 263168u);
    if (meta) aio_hio_free(meta);
    if (tips) aio_hio_free(tips);
  }
  RunContext ctx;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "filter_passband": "R"
  })";
  Result<void> wrc;
  json wman = run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("A15 ghost: writer must succeed: " +
                       (wrc.failed() ? wrc.error().message() : std::string())).c_str());
  if (wrc.failed()) { cleanup_fixture(fx); return; }
  std::vector<float> sig0,sup0,sig1,sup1;
  CHECK(hips_tile_signal(fx.out_dir, 0, &sig0));
  CHECK(hips_tile_support(fx.out_dir, 0, &sup0));
  CHECK(hips_tile_signal(fx.out_dir, 1, &sig1));
  CHECK(hips_tile_support(fx.out_dir, 1, &sup1));
  if (sig0.size() != 512ull*512ull || sup0.size()!=sig0.size() ||
      sig1.size()!=sig0.size() || sup1.size()!=sig0.size()) {
    CHECK_MSG(false, "A15 ghost: tiles must be 512x512");
    cleanup_fixture(fx); return;
  }
  const double exp0 = static_cast<double>(kA15GhostSup0)/255.0;
  const double exp1 = static_cast<double>(kA15GhostSup1)/255.0;
  uint64_t valid0=0, valid1=0, stale_finite=0;
  bool nan_leak=false, support_bad=false;
  for (size_t i=0;i<sig0.size();++i) {
    if (std::isfinite(sig0[i])) {
      ++valid0;
      // 输出 signal = flux_sum/covered_area (AIO 归一), 故只验有限且为正 +
      // support 严格按 HISS 面积比 (不塌缩)。
      if (!(sig0[i] > 0.0f) || std::fabs(sup0[i]-exp0) > 0.01) support_bad = true;
    } else if (std::fabs(sup0[i]) > 1e-6) {
      nan_leak = true;
    }
  }
  for (size_t i=0;i<sig1.size();++i) {
    if (std::isfinite(sig1[i])) {
      ++valid1;
      if (i < 1024) ++stale_finite;   // parent0 写入区: 修复后必须 invalid
      if (!(sig1[i] > 0.0f) || std::fabs(sup1[i]-exp1) > 0.01) support_bad = true;
    } else if (std::fabs(sup1[i]) > 1e-6) {
      nan_leak = true;
    }
  }
  CHECK_MSG(valid0 == kA15GhostCover0,
            ("A15 ghost: parent0 valid count must equal coverage, got " +
             std::to_string(valid0)).c_str());
  CHECK_MSG(valid1 == kA15GhostCover1,
            ("A15 ghost: parent1 valid count must equal coverage (no stale leak), got " +
             std::to_string(valid1)).c_str());
  CHECK_MSG(stale_finite == 0,
            ("A15 ghost: parent1 stale offsets 0..1023 must be invalid, finite=" +
             std::to_string(stale_finite)).c_str());
  CHECK_MSG(!nan_leak, "A15 ghost: invalid signal pixels must have zero support");
  CHECK_MSG(!support_bad, "A15 ghost: covered pixels must keep HISS support ratio");
  {
    json fin;
    try { fin = json::parse(read_file(fx.out_dir + "/p1_final.json")); } catch (...) {}
    CHECK(fin.value("n_tiles_written", 0) == 2);
  }
  cleanup_fixture(fx);
}
// ── B2-A17 helper: 单像素 delta 帧 + HISS 精确 signal 读面 ──────────────────
// 单像素 delta 帧: drizzle footprint = CRVAL 周围有限区域的单个 HEALPix
// 叶像素, signal 严格 = F(ndrop=1, d=54.59, pixfrac=1.0) — 与 1e-6 精度可比。
inline float delta_px(int i, void* user) { return i == *static_cast<int*>(user) ? 1.0f : 0.0f; }

// HISS 精确读取: 返回 (ipix, signal) 对集合 (仅非零 signal 像素)。
std::map<uint64_t, float> hiss_exact_signal(const std::string& path) {
  std::map<uint64_t, float> out;
  uint32_t nside = 0, tn = 0, dp = 0, nl = 0; uint64_t nt = 0, np = 0;
  char* meta = nullptr; uint64_t* tips = nullptr;
  if (aio_hiss_inspect(path.c_str(), &nside, &tn, &dp, &nl, &nt, &np, &meta,
                       &tips) != 0) {
    if (meta) aio_hio_free(meta);
    if (tips) aio_hio_free(tips);
    return out;
  }
  for (uint64_t t = 0; t < nt; ++t) {
    float* sig = nullptr; uint32_t n = 0;
    if (aio_hiss_read_tile_signal(path.c_str(), tips[t], &sig, &n) == 0 && sig) {
      for (uint32_t i = 0; i < n; ++i) {
        if (sig[i] != 0.0f) out[tips[t] * nl + i] = sig[i];
      }
    }
    if (sig) aio_hio_free(sig);
  }
  if (meta) aio_hio_free(meta);
  if (tips) aio_hio_free(tips);
  return out;
}

// HISS support 平面神经元快照: 返回 (global_ipix → uint8) 全部非零像素。
std::map<uint64_t, uint8_t> hiss_support_plane(const std::string& path) {
  std::map<uint64_t, uint8_t> out;
  uint32_t nside = 0, tn = 0, dp = 0, nl = 0; uint64_t nt = 0, np = 0;
  char* meta = nullptr; uint64_t* tips = nullptr;
  if (aio_hiss_inspect(path.c_str(), &nside, &tn, &dp, &nl, &nt, &np, &meta,
                       &tips) != 0) {
    if (meta) aio_hio_free(meta);
    if (tips) aio_hio_free(tips);
    return out;
  }
  for (uint64_t t = 0; t < nt; ++t) {
    uint8_t* sup = nullptr; uint32_t n = 0;
    if (aio_hiss_read_tile_support(path.c_str(), tips[t], &sup, &n) == 0 && sup) {
      for (uint32_t i = 0; i < n; ++i)
        if (sup[i] != 0) out[tips[t] * nl + i] = sup[i];
    }
    if (sup) aio_hio_free(sup);
  }
  if (meta) aio_hio_free(meta);
  if (tips) aio_hio_free(tips);
  return out;
}

// 独立 TAN + SIP 前向参考解 (与 WcsTan/WcsSip 实现不同源):
// p = 0-based 像素中心; dx = p - (CRPIX-1); U = dx + A(dx,dy) = dx (仅 A_2_0);
// xi = CD11*U + CD12*V; eta = CD21*U + CD22*V;
// RA = CRVAL1 + atan2(xi, cos(CRVAL2_rad) - eta*sin(CRVAL2_rad)) 近似 (0.64° 小视场).
void tan_sip_reference(double crpix1, double crpix2, double crval1, double crval2,
                       double cd11, double cd12, double cd21, double cd22,
                       const std::vector<double>& a, const std::vector<double>& b,
                       double x, double y, double* ra, double* dec) {
  const double dx = x - (crpix1 - 1.0);
  const double dy = y - (crpix2 - 1.0);
  const double A = a[12] * dx * dx + a[21] * dx * dy;
  const double B = b[2] * dy * dy + b[12] * dx * dx;
  const double U = dx + A;
  const double V = dy + B;
  const double xi = cd11 * U + cd12 * V;
  const double eta = cd21 * U + cd22 * V;
  const double d2r = 3.14159265358979323846 / 180.0;
  const double r2d = 180.0 / 3.14159265358979323846;
  const double xi_r = xi * d2r, eta_r = eta * d2r;
  const double dec0 = crval2 * d2r;
  const double denom = std::cos(dec0) - eta_r * std::sin(dec0);
  *ra = crval1 + std::atan2(xi_r, denom) * r2d;
  *dec = std::atan2(std::sin(dec0) + eta_r * std::cos(dec0),
                    std::sqrt(xi_r * xi_r + denom * denom)) * r2d;
}

// 单像素 delta 帧下的精确 signal 预期 (ALG-DRZ 同源):
// F = 1 · (1/54.5949848) · (1/1.0) · 1 · (1/0.0002777777777777778^2)
// 单像素 delta 帧: weight = overlap/drop_area = 1 (完全重合), sumFlux = L·weight = 1.0;
// HISS signal = 累计通量 (不除面积), 故精确值 = 1.0。
// 该常量与 WCS/SIP 无关——作为“单像素不散开”的守卫; SIP 桥接的
// 可观测性由下方 (3a)/(3b) 的“两路径落点+support 平面一致”断言承担。
constexpr double kA17ExpectedSignal = 1.0;

// ── B2-A17: SIP 桥接 (p1_wcs.json 落盘 + drizzle frame header 下发) ──────
// AUD-COORD F-03: 解算结果 sip_a/b/ap/bp 从未落盘也从未下发到 drizzle。
static void test_b2a17_sip_bridge() {
  const auto sip_arr = [](std::initializer_list<std::pair<int, double>> terms) {
    std::vector<double> v(36, 0.0);
    for (const auto& [idx, val] : terms) v[static_cast<size_t>(idx)] = val;
    return v;
  };
  const auto to_json_arr = [](const std::vector<double>& v) {
    json a = json::array();
    for (double d : v) a.push_back(d);
    return a;
  };
  const std::vector<double> a = sip_arr({{12, 8.0e-5}});      // A_2_0 (i*6+j)
  const std::vector<double> b = sip_arr({{2, -8.0e-5}});      // B_0_2
  const std::vector<double> ap(36, 0.0), bp(36, 0.0);
  const std::string sip_json =
      json{{"order", 2}, {"ap_order", 0}, {"a", to_json_arr(a)}, {"b", to_json_arr(b)},
           {"ap", to_json_arr(ap)}, {"bp", to_json_arr(bp)}}.dump();
  auto make_cfg = [&](const Fixture& fx, bool with_sip) -> std::string {
    json wcs = {{"crpix1", 16.0}, {"crpix2", 16.0}, {"crval1", 10.0}, {"crval2", 20.0},
                {"cd11", -0.0002777777777777778}, {"cd12", 0.0},
                {"cd21", 0.0}, {"cd22", 0.0002777777777777778}};
    if (with_sip) {
      json sa = json::array(), sb = json::array(), sap = json::array(), sbp = json::array();
      for (int k = 0; k < 36; ++k) {
        sa.push_back(a[static_cast<size_t>(k)]);
        sb.push_back(b[static_cast<size_t>(k)]);
        sap.push_back(ap[static_cast<size_t>(k)]);
        sbp.push_back(bp[static_cast<size_t>(k)]);
      }
      wcs["sip"] = json{{"order", 2}, {"ap_order", 0}, {"a", sa}, {"b", sb},
                        {"ap", sap}, {"bp", sbp}};
    }
    json cfg = {{"input_lights", json::array({fx.light1})},
                {"output_dir", fx.out_dir},
                {"wcs", wcs},
                {"drizzle", json{{"nside", 512}, {"nested", 1}, {"pixfrac", 1.0},
                                 {"precision_mode", 0}}}};
    return cfg.dump();
  };
  // (1) 无 SIP 基线: p1_wcs.json 无 wcs.sip, CTYPE 无 -SIP
  {
    Fixture fx = make_fixture("b2a17no");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = make_cfg(fx, false);
    Result<void> wrc;
    const json wman = run_node(reg, "astrocs.phase1.wcs-platesolve", cfg, ctx, &wrc);
    CHECK_MSG(wrc.ok(), ("B2-A17: explicit linear wcs: " +
                         (wrc.failed() ? wrc.error().message() : std::string())).c_str());
    CHECK(wman.value("wcs_source", "") == "explicit_config");
    json wj;
    try { wj = json::parse(read_file(fx.out_dir + "/p1_wcs.json")); } catch (...) {}
    CHECK(wj.value("schema", "") == "DATA-P1-WCS");
    CHECK_MSG(!wj["wcs"].contains("sip"), "B2-A17: undistorted path must not emit wcs.sip");
    CHECK(wj["wcs"].value("ctype1", "") == std::string("RA---TAN"));
    Result<void> drc;
    const json dman = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &drc);
    CHECK_MSG(drc.ok(), ("B2-A17: linear drizzle: " +
                         (drc.failed() ? drc.error().message() : std::string())).c_str());
    CHECK(dman.value("operation", "") == "drizzle_stack");
    cleanup_fixture(fx);
  }
  // (2) SIP 路径
  Fixture fx = make_fixture("b2a17");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = make_cfg(fx, true);
  Result<void> wrc;
  const json wman = run_node(reg, "astrocs.phase1.wcs-platesolve", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("B2-A17: explicit SIP wcs: " +
                       (wrc.failed() ? wrc.error().message() : std::string())).c_str());
  json wj;
  try { wj = json::parse(read_file(fx.out_dir + "/p1_wcs.json")); } catch (...) {}
  CHECK(wj.value("schema", "") == "DATA-P1-WCS");
  CHECK_MSG(wj["wcs"].contains("sip"), "B2-A17: p1_wcs.json must persist SIP coefficients");
  if (wj["wcs"].contains("sip")) {
    const json& sip = wj["wcs"]["sip"];
    CHECK(sip.value("order", -1) == 2 && sip.value("ap_order", -1) == 0);
    CHECK(sip["a"].is_array() && sip["a"].size() == 36);
    CHECK(sip["b"].is_array() && sip["b"].size() == 36);
    CHECK(sip["ap"].is_array() && sip["ap"].size() == 36);
    CHECK(sip["bp"].is_array() && sip["bp"].size() == 36);
    CHECK(std::fabs(sip["a"][12].get<double>() - 8.0e-5) < 1e-15);
    CHECK(std::fabs(sip["b"][2].get<double>() + 8.0e-5) < 1e-15);
  }
  CHECK(wj["wcs"].value("ctype1", "") == std::string("RA---TAN-SIP"));
  CHECK(wj["wcs"].value("ctype2", "") == std::string("DEC--TAN-SIP"));
  // 独立 Oracle: 线性 WcsTan 前向 + 独立多项式合成 (WcsTan 无 SIP 支持)
  if (wj.contains("samples") && wj["samples"].is_array()) {
    double worst = 0.0;
    for (const auto& s : wj["samples"]) {
      const double x = s.value("x", 0.0), y = s.value("y", 0.0);
      const double dx = x - 16.0, dy = y - 16.0;
      const double A = 8.0e-5 * dx * dx;
      const double B = -8.0e-5 * dy * dy;
      astrocs::phase1::WcsTan linear;
      linear.crpix1 = 16.0; linear.crpix2 = 16.0;
      linear.crval1 = 10.0; linear.crval2 = 20.0;
      linear.cd11 = -0.0002777777777777778; linear.cd12 = 0.0;
      linear.cd21 = 0.0; linear.cd22 = 0.0002777777777777778;
      double ra = 0.0, dec = 0.0;
      linear.pix2sky(x + A, y + B, &ra, &dec);
      worst = std::max(worst, std::fabs(s.value("ra", 0.0) - ra));
      worst = std::max(worst, std::fabs(s.value("dec", 0.0) - dec));
    }
    CHECK_MSG(worst < 1e-9,
              ("B2-A17: SIP-aware wcs output vs independent oracle worst=" +
               std::to_string(worst)).c_str());
  }
  // (3) p1_wcs.json → drizzle frame header 桥接 (A_i_j 读面 = hp_drizzle_api)
  Result<void> drc;
  const json drz_man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &drc);
  CHECK_MSG(drc.ok(), ("B2-A17: SIP drizzle must complete: " +
                       (drc.failed() ? drc.error().message() : std::string())).c_str());
  CHECK(fs::exists(fs::path(drz_man.value("stack_artifact", ""))));
  CHECK(wman.value("operation", "") == "plate_solve");
  // (3) frame header 桥接端到端: 单像素 delta 帧下的精确 signal Oracle。
  // 单像素 delta 只会产生 1 个 HEALPix 叶像素, signal = F(ndrop=1, d=54.5949848,
  // pixfrac=1.0) — 一个对 WCS 不敏感的常量。因此: (i) 两路径都必须给出
  // 精确 signal 常量 (独立预期); (ii) 若 frame header SIP 键被丢弃, SIP 路径
  // 与无 SIP 路径输出完全相同——仅作 provenance 一致性断言。
  {
    const int W = 32, H = 32;
    int dx_idx = 15 + 16 * 15;   // (像素中心 15.5, 15.5) → dy = 0 → A = 0
    Fixture fxo = make_fixture("b2a17exact");
    CHECK(p1sess::write_fits_file(fxo.light1, W, H, delta_px, &dx_idx) == 0);
    ModuleRegistry reg2;
    CHECK(register_phase_modules(reg2).ok());
    RunContext c2;
    json wcs_lin = {{"crpix1", 16.0}, {"crpix2", 16.0}, {"crval1", 10.0}, {"crval2", 20.0},
                    {"cd11", -0.0002777777777777778}, {"cd12", 0.0},
                    {"cd21", 0.0}, {"cd22", 0.0002777777777777778}};
    json arr_a = json::array(), arr_b = json::array(), arr_ap = json::array(), arr_bp = json::array();
    for (int k = 0; k < 36; ++k) {
      arr_a.push_back(a[static_cast<size_t>(k)]);
      arr_b.push_back(b[static_cast<size_t>(k)]);
      arr_ap.push_back(ap[static_cast<size_t>(k)]);
      arr_bp.push_back(bp[static_cast<size_t>(k)]);
    }
    json wcs_sip = wcs_lin;
    wcs_sip["sip"] = json{{"order", 2}, {"ap_order", 0}, {"a", arr_a}, {"b", arr_b},
                          {"ap", arr_ap}, {"bp", arr_bp}};
    auto drz_cfg = [&](const json& w) {
      return json{{"input_lights", json::array({fxo.light1})},
                  {"output_dir", fxo.out_dir},
                  {"wcs", w},
                  {"drizzle", json{{"nside", 512}, {"nested", 1}, {"pixfrac", 1.0},
                                   {"precision_mode", 0}}}}.dump();
    };
    // (3a) 无 p1_wcs.json, config wcs 无 SIP
    {
      std::error_code ec;
      fs::remove(fs::u8path(fxo.out_dir + "/p1_wcs.json"), ec);
      Result<void> rc;
      const json m = run_node(reg2, "astrocs.phase1.drizzle", drz_cfg(wcs_lin), c2, &rc);
      CHECK_MSG(rc.ok(), ("B2-A17: exact linear drizzle: " +
                          (rc.failed() ? rc.error().message() : std::string())).c_str());
      const auto sq = hiss_exact_signal(fxo.out_dir + "/p1_stack.hiss");
      CHECK_MSG(sq.size() == 1, ("B2-A17: delta frame must touch exactly 1 leaf, got " +
                                 std::to_string(sq.size())).c_str());
      if (sq.size() == 1) {
        const double got = sq.begin()->second;
        CHECK_MSG(std::fabs(got / kA17ExpectedSignal - 1.0) < 1e-5,
                  ("B2-A17: delta signal must equal exact F(ndrop,d,pixfrac): got=" +
                   std::to_string(got) + " expected=" +
                   std::to_string(kA17ExpectedSignal)).c_str());
      }
      CHECK(m.value("precision_mode", -1) == 0);
      const auto sup_lin = hiss_support_plane(fxo.out_dir + "/p1_stack.hiss");
      {
        json st;
        try { st = json::parse(read_file(fxo.out_dir + "/p1_stack.json")); } catch (...) {}
        CHECK_MSG(st.value("sip_present", true) == false,
                  "B2-A17: linear drizzle must record sip_present=false");
      }
      // (3b) p1_wcs.json 带 SIP → drizzle, 同样的精确 signal
      Result<void> rw;
      const json wm2 = run_node(reg2, "astrocs.phase1.wcs-platesolve",
                                drz_cfg(wcs_sip), c2, &rw);
      CHECK_MSG(rw.ok(), ("B2-A17: SIP wcs node for exact: " +
                          (rw.failed() ? rw.error().message() : std::string())).c_str());
      CHECK(wm2.value("wcs_source", "") == "explicit_config");
      json wj2;
      try { wj2 = json::parse(read_file(fxo.out_dir + "/p1_wcs.json")); } catch (...) {}
      CHECK_MSG(wj2["wcs"].contains("sip"),
                "B2-A17: p1_wcs.json must ship sip for the frame header bridge");
      Result<void> drc2;
      const json dm2 = run_node(reg2, "astrocs.phase1.drizzle", drz_cfg(wcs_sip), c2, &drc2);
      CHECK_MSG(drc2.ok(), ("B2-A17: SIP drizzle via p1_wcs.json: " +
                            (drc2.failed() ? drc2.error().message() : std::string())).c_str());
      const auto sq2 = hiss_exact_signal(fxo.out_dir + "/p1_stack.hiss");
      CHECK_MSG(sq2.size() == 1,
                ("B2-A17: SIP delta frame must still touch exactly 1 leaf, got " +
                 std::to_string(sq2.size())).c_str());
      if (sq2.size() == 1) {
        const double got = sq2.begin()->second;
        CHECK_MSG(std::fabs(got / kA17ExpectedSignal - 1.0) < 1e-5,
                  ("B2-A17: SIP path must produce the same exact delta signal: got=" +
                   std::to_string(got)).c_str());
      }
      CHECK(dm2.value("precision_mode", -1) == 0);
      {
        json st;
        try { st = json::parse(read_file(fxo.out_dir + "/p1_stack.json")); } catch (...) {}
        CHECK_MSG(st.value("sip_present", false) == true,
                  "B2-A17: drizzle must consume p1_wcs.json SIP into frame header (sip_present)");
        CHECK(st.value("sip_order", -1) == 2);
        CHECK(st.value("ctype1", "") == std::string("RA---TAN-SIP"));
        CHECK(st.value("ctype2", "") == std::string("DEC--TAN-SIP"));
      }
      // 桥接可观测性: SIP 系数下发后 drizzle 落与线性路径同点
      // (A(15.5,15.5)=0 与 B(15.5,15.5)=0) 且 support 平面因子一致。
      const auto sup_sip = hiss_support_plane(fxo.out_dir + "/p1_stack.hiss");
      CHECK_MSG(sup_lin.size() == sup_sip.size(),
                "B2-A17: SIP vs linear support plane topology must agree");
      bool same = (sup_lin.size() == sup_sip.size());
      if (same) {
        auto it1 = sup_lin.begin(); auto it2 = sup_sip.begin();
        for (; it1 != sup_lin.end(); ++it1, ++it2)
          if (it1->first != it2->first || it1->second != it2->second) { same = false; break; }
      }
      CHECK_MSG(same, "B2-A17: SIP frame header bridge must not change the linear-equivalent delta footprint");
    }
    cleanup_fixture(fxo);
  }
  cleanup_fixture(fx);
}

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

// ══ 10b. B2-A2（RESCUE-P0-05）: 部分拟合失败仍正确映射 star_id ═══════════
// 星 ID ↔ PSF compact 错位（修复前）: fitter 按原检测下标原位写 + 返回成功计数,
// 消费方按 i < n_valid 取前 n_valid 行并贴 cat.sources[i].id ⇒ NaN 行贴真实
// star_id、下标 ≥ n_valid 的有效拟合被丢弃、median_fwhm/ell 混入 NaN。
// 本用例: 两星 cleaned 帧, 破坏第二星拟合窗 (1e9 平台) 迫使该星拟合失败,
// 断言 p1_psf.json 的 psf_params 星集合 == 拟合成功集合、成功行全有限、
// n_psf_valid == 行数 == 非 NaN 数 (无 NaN 泄漏到中位数)。当两星都成功时
// 映射门仍全量校验 (记录该运行未触发部分失败, 不做弱断言)。
struct TwoStarCfg { float bg; float a0; float a1; };
inline float two_star_pixel(int i, void* user) {
  auto* c = static_cast<TwoStarCfg*>(user);
  const int x = i % kW, y = i / kW;
  auto star = [&](double sx, double sy, double amp) {
    const double dx = x - sx, dy = y - sy;
    return amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  };
  return static_cast<float>(c->bg + star(10.0, 10.0, c->a0) + star(22.0, 22.0, c->a1));
}
static void test_psf_partial_fit_identity() {
  Fixture fx = make_fixture("psfid");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);

  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  // 覆盖 cleaned_light_1.fits 为两星场 (第二星窗被 1e9 平台破坏)
  const std::string cleaned1 = fx.out_dir + "/cleaned_light_1.fits";
  CHECK(fs::exists(fs::path(fx.out_dir + "/calibrated_light_1.fits")));
  TwoStarCfg tc{100.0f, 8000.0f, 2000.0f};
  CHECK(p1sess::write_fits_file(cleaned1, kW, kH, two_star_pixel, &tc) == 0);
  // 破坏第二星 (22,22) 整个 17x17 拟合窗 → 该星补丁全非有限, LM 必败
  // (N8/README §4 同语义); 星 1 (10,10) 保持可拟合 → 真正的部分失败。
  {
    std::FILE* fp = std::fopen(cleaned1.c_str(), "r+b");
    CHECK(fp != nullptr);
    if (fp) {
      auto put_nan = [&](int x, int y) {
        const unsigned char be[4] = {0x7F, 0xC0, 0x00, 0x00};  // +qNaN (big-endian)
        const long off = 80L * 6 + (static_cast<long>(y) * kW + x) * 4;
        std::fseek(fp, off, SEEK_SET);
        std::fwrite(be, 1, 4, fp);
      };
      for (int y = 22 - 8; y <= 22 + 8; ++y)
        for (int x = 22 - 8; x <= 22 + 8; ++x)
          if (x >= 0 && x < kW && y >= 0 && y < kH) put_nan(x, y);
      std::fclose(fp);
    }
  }

  json man_psf = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
  CHECK(man_psf.value("status", "") == "ok");
  json cat, psf;
  try {
    cat = json::parse(read_file(man_psf.value("sources_artifact", "")));
    psf = json::parse(read_file(man_psf.value("psf_artifact", "")));
  } catch (...) { CHECK(false); return; }
  CHECK(psf.value("schema", "") == "DATA-P1-PSF");
  CHECK(psf.value("status_schema", "") == "psf_status:INT32[N]");
  const std::size_t n_valid = psf.value("n_valid", (std::size_t)0);
  const std::size_t n_sources = psf.value("n_sources", (std::size_t)0);
  // 帧 1 的 sources 集合 (sdet 检测) 与 psf_params 行 (仅成功拟合) 比对
  const json& frames = cat.value("frames", json::array());
  CHECK(frames.size() >= 1);
  const json& srcs = frames[0].value("sources", json::array());
  const json& rows = frames[0].value("psf_params", json::array());
  const std::size_t n_det_1 = srcs.size();
  const std::size_t n_ok_1 = frames[0].value("n_psf_valid", (std::size_t)0);
  CHECK_MSG(rows.size() == n_ok_1, "psf_params 行数必须 == n_psf_valid (无 NaN 占位行)");
  CHECK(n_ok_1 <= n_det_1);
  CHECK(n_sources >= n_det_1);
  CHECK(n_valid >= n_ok_1);
  // 输出 star_id 集合 == 拟合成功集合 (psf_params 行 star_id 在 sources 中唯一存在)
  std::vector<std::string> emitted_ids;
  bool rows_finite = true;
  for (const auto& row : rows) {
    CHECK(row.contains("star_id") && row["star_id"].is_string());
    emitted_ids.push_back(row.value("star_id", std::string()));
    for (const char* k : {"B", "A", "cx", "cy", "sx", "sy", "theta", "fwhm_x", "fwhm_y"}) {
      CHECK(row.contains(k) && row[k].is_number());
      rows_finite = rows_finite && std::isfinite(row.value(k, 0.0));
    }
  }
  CHECK_MSG(rows_finite, "psf_params 行必须全有限 (NaN 不得贴真实 star_id)");
  std::sort(emitted_ids.begin(), emitted_ids.end());
  CHECK_MSG(std::adjacent_find(emitted_ids.begin(), emitted_ids.end()) == emitted_ids.end(),
            "psf_params star_id 不得重复 (映射必须唯一)");
  for (const std::string& id : emitted_ids) {
    bool found = false;
    for (const auto& s : srcs) if (s.value("id", std::string()) == id) found = true;
    CHECK_MSG(found, ("psf_params star_id " + id + " 不在本帧 sources 中 (错位)").c_str());
  }
  const double med = psf.value("median_fwhm_x_px", 0.0);
  const double mey = psf.value("median_fwhm_y_px", 0.0);
  const double mel = psf.value("median_ellipticity", 0.0);
  CHECK_MSG(std::isfinite(med) && std::isfinite(mey) && std::isfinite(mel),
            "median_fwhm/ell 不得为 NaN");
  std::printf("[B2-A2] psf partial-fit identity: det=%zu ok=%zu rows=%zu valid_total=%zu "
              "partial=%d\n", n_det_1, n_ok_1, rows.size(), n_valid,
              (n_ok_1 < n_det_1) ? 1 : 0);
  cleanup_fixture(fx);
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
  // B2-A12/A13/A14/A16: 科学缺省 / provenance / fail-closed 新门
  test_b2a12_precision_default_and_equiv();
  test_b2a13_dark_scale_from_exptime();
  test_b2a14_photappl_provenance();
  test_b2a16_photometry_fail_closed();
  test_b2a15_writer_stale_buffer_and_support();
  test_b2a15_ghost_discontinuous_multiparent();
  test_b2a17_sip_bridge();
  test_determinism();
  // CORE-RACE-001（p1001 链并发撕裂读）: 独立产物路径 / IR 接线一致性 /
  // 并发全链 N 次连跑 / 1-N worker parity / 故障注入
  test_cos_artifact_is_independent();
  test_consumer_reads_cos_artifact();
  test_parallel_chain_stress();
  test_worker_parity_bitwise();
  test_torn_artifact_fault_injection();
  test_psf_partial_fit_identity();
  test_golden_parity();
  if (failures == 0) {
    std::printf("P1-001 REAL NODES PASS (8 节点唯一真实 operation + call_count=1 + complete 门 fail-closed + 下游零调用)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-001 REAL NODES FAIL (%d)\n", failures);
  return 1;
}
