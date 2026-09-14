// ============================================================================
// p1snr_frame_parity_test.cpp - P14-N-08/N-09 (RQS V2-N-08 + V2-N-09)
//   交付 SNR/极限星等样本真实性 + 性能开关解耦 parity 回归锁
//
// 缺陷 (改前): p1_op_noise 的 SNR 目录 = 上游 psf_params (受 P2 性能开关
//   psf.max_stars 截断的「最亮 ≤5000 颗」) ∩ sources ⇒ 交付的 median_snr /
//   frame_depth_m5_mag 由最亮子集决定 (系统性偏乐观), 且下游读不出被截断;
//   psf_mode 恒字面量 "fast"。
//
// 本锁经**生产模块注册表** (register_phase_modules) 端到端跑真实节点 op:
//   star-psf (detect_sources, 读 config.psf.max_stars) → noise-snr (estimate_snr,
//   读上游 p1_sources.json), 断言:
//   A. parity   : psf.max_stars=0(不限) / 5000 / 2(强制截断) 三次运行, 交付
//                 SNR/深度逐位一致 ⇒ 性能开关不得改变交付数值。
//   B. 真实性   : 产物如实记录 psf_mode(真实模式)/n_fit_input/n_sources/
//                 truncated/psf_fit_truncated/snr_sample 样本定义。
//   C. 非恒真   : 人为用 snr.max_sources 截断样本 ⇒ truncated=true 且数值确实
//                 不同 (证明 A 的锁不是恒真)。
// 该文件在改前(pristine)生产源上同样可编译运行 ⇒ parity 断言 RED; 改后 GREEN。
// ============================================================================
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/context.h"

#include "p1sess_fixtures.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#define P14_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P14_GETPID static_cast<long>(::getpid())
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace astrocs::core;

static int g_fail = 0;
static int g_check = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    ++g_check;                                                                 \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__,        \
                   #cond, (msg));                                              \
      ++g_fail;                                                                \
    } else {                                                                   \
      std::printf("ok  : %s\n", (msg));                                        \
    }                                                                          \
  } while (0)

// ── 合成星场: 6 颗不同亮度的孤立高斯星 + 确定性伪噪声背景 ───────────────────
constexpr int kW = 64, kH = 64;

inline float field_pixel(int i, void*) {
  const int x = i % kW, y = i / kW;
  double v = 100.0;
  // 4 颗孤立高斯星 (前两颗 = p1001_real_nodes_test 已验证可 Moffat4 收敛的双星对)。
  static const double sx[4] = {10, 22, 40, 52};
  static const double sy[4] = {10, 22, 40, 12};
  static const double amp[4] = {8000, 6000, 5000, 4000};
  for (int k = 0; k < 4; ++k) {
    const double dx = x - sx[k], dy = y - sy[k];
    v += amp[k] * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  }
  return static_cast<float>(v);
}

static json read_json(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return json::parse(std::string((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>()));
}

// 单节点执行 (p1001_real_nodes_test 同构): registry.create -> validate -> plan -> execute。
static bool run_node(ModuleRegistry& reg, const char* module_id,
                     const std::string& config_json, json* manifest_out,
                     std::string* err) {
  auto m = reg.create(module_id);
  if (m.failed()) { *err = std::string(module_id) + ": create failed: " + m.error().message(); return false; }
  auto v = m.value()->validate_config(config_json);
  if (v.failed()) { *err = std::string(module_id) + ": validate failed: " + v.error().message(); return false; }
  auto p = m.value()->plan(std::string("n_") + module_id, config_json);
  if (p.failed()) { *err = std::string(module_id) + ": plan failed: " + p.error().message(); return false; }
  RunContext ctx;
  auto rc = m.value()->execute(ctx);
  if (rc.failed()) { *err = std::string(module_id) + ": execute failed: " + rc.error().message(); return false; }
  if (manifest_out) {
    auto man = m.value()->last_manifest();
    if (man.ok()) { try { *manifest_out = json::parse(man.value()); } catch (...) {} }
  }
  return true;
}

static bool run_psf(ModuleRegistry& reg, const std::string& out_dir,
                    const std::string& light, int max_stars, std::string* err) {
  json cfg;
  cfg["input_lights"] = json::array({light});
  cfg["output_dir"] = out_dir;
  cfg["psf"] = json{{"max_stars", max_stars}};
  return run_node(reg, "astrocs.phase1.star-psf", cfg.dump(), nullptr, err);
}

static bool run_noise(ModuleRegistry& reg, const std::string& out_dir,
                      const std::string& light, int max_sources, json* frame,
                      std::string* err) {
  json cfg;
  cfg["input_lights"] = json::array({light});
  cfg["output_dir"] = out_dir;
  cfg["snr"] = json{{"zero_point_mag", 25.0}, {"gain_e_per_adu", 1.5},
                    {"read_noise_e", 5.0}};
  if (max_sources > 0) cfg["snr"]["max_sources"] = max_sources;
  if (!run_node(reg, "astrocs.phase1.noise-snr", cfg.dump(), nullptr, err)) return false;
  const json snr = read_json(out_dir + "/p1_snr.json");
  if (!snr.contains("frames") || !snr["frames"].is_array() || snr["frames"].empty()) {
    *err = "p1_snr.json has no frames";
    return false;
  }
  *frame = snr["frames"][0];
  return true;
}

static bool same_f64(const json& a, const json& b, const char* k) {
  if (!a.contains(k) || !b.contains(k)) return false;
  return a[k].get<double>() == b[k].get<double>();
}

int main() {
  const fs::path base = fs::temp_directory_path() /
      ("p14_snr_parity_" + std::to_string(P14_GETPID));
  std::error_code ec;
  fs::remove_all(base, ec);
  fs::create_directories(base, ec);
  // 每个 run 目录都要有同名输入帧 (photometry/noise 按 <out_dir>/<file> 读取)。
  auto write_light = [](const std::string& dir) -> bool {
    return p1sess::write_fits_file(dir + "/light_1.fits", kW, kH, field_pixel,
                                   nullptr) == 0;
  };
  if (!write_light(base.string())) {
    std::fprintf(stderr, "FAIL: cannot write fixture fits\n");
    return 1;
  }

  ModuleRegistry reg;
  auto rreg = register_phase_modules(reg);
  if (rreg.failed()) {
    std::fprintf(stderr, "FAIL: register_phase_modules: %s\n", rreg.error().message().c_str());
    return 1;
  }

  struct Case { const char* name; int max_stars; };
  const Case cases[3] = {{"run_unlimited", 0}, {"run_fast5000", 5000},
                         {"run_fast2", 2}};
  json frames[3];
  for (int i = 0; i < 3; ++i) {
    const std::string od = (base / cases[i].name).string();
    fs::create_directories(od, ec);
    if (!write_light(od)) {
      std::fprintf(stderr, "FAIL: cannot write fixture fits (%s)\n", cases[i].name);
      return 1;
    }
    const std::string light = od + "/light_1.fits";
    std::string err;
    if (!run_psf(reg, od, light, cases[i].max_stars, &err)) {
      std::fprintf(stderr, "FAIL: star-psf (%s): %s\n", cases[i].name, err.c_str());
      return 1;
    }
    if (!run_noise(reg, od, light, 0, &frames[i], &err)) {
      std::fprintf(stderr, "FAIL: noise-snr (%s): %s\n", cases[i].name, err.c_str());
      return 1;
    }
    // P14 既有产物零变化对照用: unlimited 运行额外跑 photometry 产出 p1_flux.json
    // (该产物只依赖 DATA-P1-SOURCES.sources, 与 psf.max_stars/SNR 改动无关)。
    if (cases[i].max_stars == 0) {
      json pcfg;
      pcfg["input_lights"] = json::array({light});
      pcfg["output_dir"] = od;
      if (!run_node(reg, "astrocs.phase1.photometry", pcfg.dump(), nullptr, &err)) {
        std::fprintf(stderr, "FAIL: photometry (%s): %s\n", cases[i].name, err.c_str());
        return 1;
      }
      std::error_code bec;
      CHECK(fs::exists(fs::u8path(od + "/p1_flux.json"), bec),
            "既有产物: photometry 产出 p1_flux.json (字节对照用)");
    }
  }

  // ── A. parity: 交付 SNR/深度 与 max_stars 无关 ─────────────────────────
  const char* num_fields[] = {"snr_phot", "median_snr", "median_source_snr",
                              "frame_depth_flux5_adu", "frame_depth_m5_mag"};
  for (int i = 1; i < 3; ++i) {
    for (const char* k : num_fields) {
      CHECK(same_f64(frames[0], frames[i], k),
            (std::string("parity: ") + k + " 与 max_stars=0 逐位一致 (" +
             cases[i].name + ")").c_str());
    }
    CHECK(frames[0].value("n_snr_input", -1) == frames[i].value("n_snr_input", -2),
          (std::string("parity: n_snr_input 与 max_stars=0 一致 (") +
           cases[i].name + ")").c_str());
    CHECK(frames[0].value("n_snr_catalogue", -1) ==
              frames[i].value("n_snr_catalogue", -2),
          (std::string("parity: n_snr_catalogue 与 max_stars=0 一致 (") +
           cases[i].name + ")").c_str());
  }

  // ── B. 真实性 provenance ───────────────────────────────────────────────
  CHECK(frames[0].value("psf_mode", "") == "precise",
        "真实性: max_stars=0 -> psf_mode=precise (真实模式, 非字面量)");
  CHECK(frames[1].value("psf_mode", "") == "fast",
        "真实性: max_stars=5000 -> psf_mode=fast");
  CHECK(frames[2].value("psf_mode", "") == "fast",
        "真实性: max_stars=2 -> psf_mode=fast");
  CHECK(frames[0].value("psf_fit_truncated", true) == false,
        "真实性: max_stars=0 -> psf_fit_truncated=false");
  CHECK(frames[1].value("psf_fit_truncated", true) == false,
        "真实性: max_stars=5000 (6 星<上限) -> psf_fit_truncated=false");
  CHECK(frames[2].value("psf_fit_truncated", true) == true,
        "真实性: max_stars=2 -> psf_fit_truncated=true (拟合输入被截断)");
  CHECK(frames[2].value("n_fit_input", -1) == 2,
        "真实性: max_stars=2 -> n_fit_input==2 (真正参与拟合星数)");
  CHECK(frames[0].value("n_fit_input", -1) == frames[0].value("n_sources", -2),
        "真实性: 不截断时 n_fit_input == n_sources");
  CHECK(frames[0].value("n_sources", 0) >= 3,
        "真实性: n_sources >= 3 (可用源总数非退化)");
  CHECK(frames[0].value("n_snr_catalogue", 0) >= 3,
        "真实性: 交付 SNR 目录 >= 3 颗 (全量样本, 非截断子集)");
  CHECK(frames[0].value("truncated", true) == false,
        "真实性: 默认交付样本未被截断 -> truncated=false");
  CHECK(frames[0].contains("snr_sample") && frames[0]["snr_sample"].is_string() &&
            frames[0]["snr_sample"].get<std::string>().find("sources") !=
                std::string::npos,
        "真实性: 产物记录所用样本定义 snr_sample");

  // ── C. 非恒真: 人为把交付样本截断 -> truncated=true 且数值不同 ─────────
  {
    const std::string od = (base / "run_capped").string();
    fs::create_directories(od, ec);
    fs::copy_file((base / "run_fast2").string() + "/p1_sources.json",
                  od + "/p1_sources.json", fs::copy_options::overwrite_existing, ec);
    if (!write_light(od)) {
      std::fprintf(stderr, "FAIL: cannot write fixture fits (run_capped)\n");
      return 1;
    }
    const std::string light = od + "/light_1.fits";
    std::string err;
    json capped;
    if (!run_noise(reg, od, light, /*max_sources=*/1, &capped, &err)) {
      std::fprintf(stderr, "FAIL: capped noise-snr: %s\n", err.c_str());
      return 1;
    }
    CHECK(capped.value("truncated", false) == true,
          "非恒真: snr.max_sources=1 -> truncated=true");
    CHECK(capped.value("n_snr_catalogue", -1) == 1,
          "非恒真: 截断样本 n_snr_catalogue==1");
    CHECK(!same_f64(frames[0], capped, "snr_phot"),
          "非恒真: 截断样本 snr_phot 与全量样本确实不同");
    CHECK(!same_f64(frames[0], capped, "frame_depth_m5_mag"),
          "非恒真: 截断样本 frame_depth_m5_mag 与全量样本确实不同");
  }

  // 证据留存: P14_KEEP_DIR=1 时保留产物目录 (pristine/patched 逐字节对照用)。
  if (std::getenv("P14_KEEP_DIR")) {
    std::printf("KEEP_DIR=%s\n", base.c_str());
  } else {
    fs::remove_all(base, ec);
  }
  std::printf("=== P14 SNR parity checks=%d fail=%d ===\n", g_check, g_fail);
  if (g_fail != 0) { std::printf("RESULT: FAIL\n"); return 1; }
  std::printf("RESULT: PASS\n");
  return 0;
}
