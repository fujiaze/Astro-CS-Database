// ============================================================================
// p1snr_fref_baseline_test.cpp - FREF-BASELINE-001 判别力锁（能红能绿）
//
// 缺陷（改前）: 帧级 SNR 的参考通量 F_ref = **块级检出通量中位数**（ADU）。
//   (a) 它是**数据派生**量: 换一批同组帧（少一帧/多一帧）⇒ 同一个帧的 F_ref
//       就变 ⇒ 帧不独立（负责人裁决 §9.49 定案 2「帧间独立, 不该有组概念」）;
//   (b) 它**没有物理含义**: 既不是任何一颗星的仪器通量, 也随星场结构漂移
//       （GAP_AUDIT §9.48: 34.5 万条"源"多为星云/PSF 翼/平场残差）;
//   (c) 它是**公共 ADU 数**, 与配对性定理要求的「物理参考通量 F0」不同:
//       SNR_f = F0/σ_f ⇒ SNR_f²/F0² = 1/σ_f², **丢掉帧间响应 a_f²**。
//
// 定案（FREF-BASELINE-001, 负责人「直接用 6 等星/一个数值表示比较正常的星等
// 来做基准」）: F_ref,k = 10^(-0.4*(m_ref - ZP_k))，ZP_k = ZP_syn,k - 2.5*log10 k_photo,k。
//   头部 ASTROCS_REFERENCE_FLUX 写**物理公共锚** F0 = 10^(-0.4*(m_ref - ZP_syn))。
//
// 本锁经**生产模块注册表**跑真实 op (astrocs.phase1.noise-snr)，两个 case:
//   OLD   : 上游 p1_phot.json **无** photscale_fit（改前形态）⇒ 参考通量只能
//           退化为块级中位数 ⇒ 下列"固定星等"断言必须**全红**（RED 判据）。
//   NEW   : 上游 p1_phot.json **有** photscale_fit ⇒ 固定星等绝对基准生效 ⇒
//           断言全绿（GREEN 判据）。
// 断言清单（NEW）:
//   N1 reference_flux_source == "fixed_magnitude" 且 reference_mag 如实落盘
//   N2 逐帧 flux_adu == 10^(-0.4*(m_ref - frame_zero_point_mag))（恒等式）
//   N3 配对性 C1: snr_f == flux_adu / sigma_f_adu
//   N4 物理公共锚: flux_adu,k * k_photo,k == flux_common（逐帧同一物理通量）
//   N5 帧间独立: 从 input_lights 去掉第 2 帧后, 第 1 帧的 flux_adu **逐位不变**
//      （OLD 形态下 N5 必红: F_ref 是块级中位数, 必然随帧集改变）
// 该文件在改前(pristine)生产源上编译运行 ⇒ N1–N5 全红; 改后 GREEN。
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
#define FB_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define FB_GETPID static_cast<long>(::getpid())
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace astrocs::core;

static int g_fail = 0;
static int g_check = 0;

#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    ++g_check;                                                                  \
    if (!(cond)) {                                                              \
      std::fprintf(stderr, "FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond, \
                   (msg));                                                      \
      ++g_fail;                                                                 \
    } else {                                                                    \
      std::printf("ok  : %s\n", (msg));                                         \
    }                                                                           \
  } while (0)

// 合成星场（与 p1snr_frame_parity_test 同构）: 4 颗孤立高斯星 + 平背景
constexpr int kW = 64, kH = 64;

inline float field_pixel(int i, void*) {
  const int x = i % kW, y = i / kW;
  double v = 100.0;
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
static void write_json(const std::string& p, const json& j) {
  std::ofstream f(p, std::ios::binary);
  f << j.dump(2);
}

static bool run_node(ModuleRegistry& reg, const char* module_id,
                     const std::string& config_json, std::string* err) {
  auto m = reg.create(module_id);
  if (m.failed()) { *err = std::string(module_id) + ": create failed"; return false; }
  auto v = m.value()->validate_config(config_json);
  if (v.failed()) { *err = std::string(module_id) + ": validate failed: " + v.error().message(); return false; }
  auto p = m.value()->plan(std::string("n_") + module_id, config_json);
  if (p.failed()) { *err = std::string(module_id) + ": plan failed: " + p.error().message(); return false; }
  RunContext ctx;
  auto rc = m.value()->execute(ctx);
  if (rc.failed()) { *err = std::string(module_id) + ": execute failed: " + rc.error().message(); return false; }
  return true;
}

// 真实 k_photo / ZP_syn（取自 L4 t2_m1_red 的 Gaia XPSD 拟合量级）
static const double kK1 = 6.272203e-17;
static const double kK2 = 5.685037e-17;
static const double kZPsyn = -14.269;

// 生成一个 run 目录: 2 帧 FITS + 合成 p1_sources.json + 指定形态的 p1_phot.json
static bool prep_case(ModuleRegistry& reg, const std::string& od, bool with_fit,
                      std::string* err) {
  std::error_code ec;
  fs::create_directories(od, ec);
  std::string lights[2];
  json src_frames = json::array();
  for (int i = 0; i < 2; ++i) {
    lights[i] = od + "/light_" + std::to_string(i + 1) + ".fits";
    if (p1sess::write_fits_file(lights[i], kW, kH, field_pixel, nullptr) != 0) {
      *err = "cannot write fixture fits";
      return false;
    }
    json cfg;
    cfg["input_lights"] = json::array({lights[i]});
    cfg["output_dir"] = od;
    cfg["psf"] = json{{"max_stars", 0}};
    if (!run_node(reg, "astrocs.phase1.star-psf", cfg.dump(), err)) return false;
    json sj = read_json(od + "/p1_sources.json");
    json fr = sj["frames"][0];
    // 两帧给不同噪声（模拟真实帧间噪声差），使 SNR 帧间散度可观测
    fr["noise_sigma"] = (i == 0) ? 19.813253033195167 : 20.525;
    // 第 2 帧通量整体 ×1.3（模拟真实帧间响应 a_f 差, 与 L4 t2_m1_red 的
    // k_photo 比 1.103 同量级）。**必须**让两帧的检出通量分布不同, 否则
    // "块级中位数" 与单帧中位数数值相同, RED 判据会退化为恒真。
    if (i == 1) {
      for (auto& s : fr["sources"]) s["flux"] = s["flux"].get<double>() * 1.3;
    }
    src_frames.push_back(fr);
  }
  write_json(od + "/p1_sources.json", json{{"frames", src_frames}});

  // p1_phot.json: OLD 形态无 photscale_fit; NEW 形态有（含 zero_point_mag）
  json prov = json{{"schema", "DATA-P1-PHOTPROV-001"},
                   {"node", "astrocs.phase1.photometry"},
                   {"operation", "measure_flux"},
                   {"entry", "astrocs_phase1_photometry_v1"},
                   {"photometry_applied", false},
                   {"photscal", 1.0},
                   {"pixel_scaling", "none"},
                   {"n_frames", 2},
                   {"degraded_reason", "photscale_absent"}};
  if (with_fit) {
    json fit = json::object();
    const double ks[2] = {kK1, kK2};
    for (int i = 0; i < 2; ++i) {
      const std::string base = fs::path(lights[i]).stem().string();
      fit[base] = json{{"k_photo", ks[i]},
                       {"n_matched", 939},
                       {"sigma_residual_dex", 0.019},
                       {"fitted", true},
                       {"source", "gaia_star_matcher_tukey_irls"},
                       {"f_instr_domain", "psf_analytic_flux_2pi_A_sx_sy_over_3"},
                       {"zero_point_valid", true},
                       {"zero_point_mag", kZPsyn},
                       {"zero_point_n_stars", 2339},
                       {"zero_point_scatter_mag", 0.08}};
    }
    prov["photscale_fit"] = fit;
  }
  write_json(od + "/p1_phot.json", prov);
  return true;
}

static bool run_noise2(ModuleRegistry& reg, const std::string& od,
                       const std::string& light1, const std::string& light2,
                       json* snr_out, std::string* err) {
  json cfg;
  cfg["input_lights"] = light2.empty() ? json::array({light1})
                                       : json::array({light1, light2});
  cfg["output_dir"] = od;
  cfg["snr"] = json::object();   // 走 FREF-BASELINE 默认 m_ref = 6.0
  if (!run_node(reg, "astrocs.phase1.noise-snr", cfg.dump(), err)) return false;
  *snr_out = read_json(od + "/p1_snr.json");
  return true;
}

int main() {
  const fs::path base = fs::temp_directory_path() /
      ("fref_baseline_" + std::to_string(FB_GETPID));
  std::error_code ec;
  fs::remove_all(base, ec);
  fs::create_directories(base, ec);

  ModuleRegistry reg;
  auto rreg = register_phase_modules(reg);
  if (rreg.failed()) {
    std::fprintf(stderr, "FAIL: register_phase_modules: %s\n",
                 rreg.error().message().c_str());
    return 1;
  }

  // ── OLD 形态（改前上游产物）: 无 photscale_fit ──────────────────────────
  {
    const std::string od = (base / "old").string();
    std::string err;
    if (!prep_case(reg, od, /*with_fit=*/false, &err)) {
      std::fprintf(stderr, "FAIL: prep old: %s\n", err.c_str());
      return 1;
    }
    json snr;
    if (!run_noise2(reg, od, od + "/light_1.fits", od + "/light_2.fits", &snr, &err)) {
      std::fprintf(stderr, "FAIL: noise old: %s\n", err.c_str());
      return 1;
    }
    // RED 判据: 旧形态下无法建立固定星等基准（这正是缺陷）
    CHECK(snr.value("reference_flux_source", std::string()) == "group_median",
          "RED: 旧形态(无 photscale_fit) ⇒ 参考通量只能是数据派生的 group_median");
    CHECK(!snr.contains("reference_mag") || snr["reference_mag"].is_null(),
          "RED: 旧形态 ⇒ 没有固定参考星等（无法帧间可比）");
    // 帧间独立 RED: 去掉第 2 帧后第 1 帧的 F_ref 必然改变
    json snr1;
    if (!run_noise2(reg, od, od + "/light_1.fits", std::string(), &snr1, &err)) {
      std::fprintf(stderr, "FAIL: noise old single: %s\n", err.c_str());
      return 1;
    }
    const double f_a = snr["frames"][0]["snr_reference"].value("flux_adu", 0.0);
    const double f_b = snr1["frames"][0]["snr_reference"].value("flux_adu", 0.0);
    CHECK(std::fabs(f_a - f_b) > 0.0,
          "RED: 旧形态 F_ref 随帧集改变（帧不独立 —— 负责人裁决要修的点）");
  }

  // ── NEW 形态（FREF-BASELINE-001）: 有 photscale_fit ─────────────────────
  {
    const std::string od = (base / "new").string();
    std::string err;
    if (!prep_case(reg, od, /*with_fit=*/true, &err)) {
      std::fprintf(stderr, "FAIL: prep new: %s\n", err.c_str());
      return 1;
    }
    json snr;
    if (!run_noise2(reg, od, od + "/light_1.fits", od + "/light_2.fits", &snr, &err)) {
      std::fprintf(stderr, "FAIL: noise new: %s\n", err.c_str());
      return 1;
    }
    CHECK(snr.value("reference_flux_source", std::string()) == "fixed_magnitude",
          "N1: reference_flux_source == fixed_magnitude");
    CHECK(snr.contains("reference_mag") && snr["reference_mag"].is_number() &&
              std::fabs(snr["reference_mag"].get<double>() - 6.0) < 1e-12,
          "N1: 参考星等如实落盘 (m_ref = 6.0)");
    const double f0 = snr.value("reference_flux_common", 0.0);
    CHECK(std::isfinite(f0) && f0 > 0.0, "N4: 物理公共锚 reference_flux_common > 0");

    const double ks[2] = {kK1, kK2};
    const json& fr0 = snr["frames"][0];
    for (int i = 0; i < 2; ++i) {
      const json& r = snr["frames"][static_cast<size_t>(i)]["snr_reference"];
      const double zpk = r.value("frame_zero_point_mag", 0.0);
      const double fref = r.value("flux_adu", 0.0);
      const double sigf = r.value("sigma_f_adu", 0.0);
      const double snrf = r.value("snr_f", 0.0);
      const double kp = r.value("frame_k_photo", 0.0);
      const double expect_zp = kZPsyn - 2.5 * std::log10(ks[i]);
      CHECK(std::fabs(zpk - expect_zp) < 1e-9,
            (std::string("N2: ZP_k = ZP_syn - 2.5log10(k_photo) (帧 ") +
             std::to_string(i + 1) + ")").c_str());
      const double expect_f = std::pow(10.0, -0.4 * (6.0 - expect_zp));
      CHECK(std::fabs(fref / expect_f - 1.0) < 1e-12,
            (std::string("N2: flux_adu == 10^(-0.4(m_ref-ZP_k)) (帧 ") +
             std::to_string(i + 1) + ")").c_str());
      CHECK(std::fabs(snrf - fref / sigf) <= 1e-9 * std::fabs(snrf),
            (std::string("N3: 配对性 snr_f == flux_adu/sigma_f_adu (帧 ") +
             std::to_string(i + 1) + ")").c_str());
      CHECK(std::fabs(fref * kp / f0 - 1.0) < 1e-9,
            (std::string("N4: flux_adu*k_photo == flux_common（同一物理参考星）(帧 ") +
             std::to_string(i + 1) + ")").c_str());
    }
    // N5 帧间独立: 去掉第 2 帧, 第 1 帧的 flux_adu 逐位不变
    json snr1;
    if (!run_noise2(reg, od, od + "/light_1.fits", std::string(), &snr1, &err)) {
      std::fprintf(stderr, "FAIL: noise new single: %s\n", err.c_str());
      return 1;
    }
    const double g_a = snr["frames"][0]["snr_reference"].value("flux_adu", 0.0);
    const double g_b = snr1["frames"][0]["snr_reference"].value("flux_adu", 0.0);
    CHECK(g_a == g_b,
          "N5: 新形态 F_ref 与帧集无关（帧间独立, 无需组概念）");
    // 公共锚同样与帧集无关 ⇒ Phase2 闸门恒过
    CHECK(snr1.value("reference_flux_common", 0.0) == f0,
          "N5: reference_flux_common 与帧集无关（Phase2 公共通量闸门恒过）");
    (void)fr0;
  }

  std::printf("\n%d checks, %d failures\n", g_check, g_fail);
  return g_fail == 0 ? 0 : 1;
}
