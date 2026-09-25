// ============================================================================
// p1snr_noise_cfg_wiring_test.cpp — WIRING-W34-01 (CHK-PROD-WIRING W3)
//   doc["noise"] 配置块的**消费点行为证据**锁
//
// 缺陷（改前）: eng/packaging/config/defaults.json 的 noise.* 键在 lib/** 生产面
//   **零消费点**（无任何生产源出现键名 token）⇒ CHK-PROD-WIRING W3 红：
//   配置键存在而无消费点。改后消费点 = module_adapters.cpp::p1_noise_cfg_apply
//   （由 p1_noise_model_for_frame 调用；SNR 节点与 drizzle 逐像素 variance 块共用）。
//
// 本锁经**生产模块注册表**（register_phase_modules）端到端跑真实节点 op:
//   star-psf（产出 p1_sources.json 供掩膜）→ noise-snr（读 doc["noise"]）。
// 逐键断言（8 个 W3 在册键 + 该段词表其余 6 键）:
//   A. 回执: 产物 manifest 的诊断键 noise_cfg_applied 必须含该键与生效值
//      （证明「被消费」而非「被静默忽略」）；
//   B. 行为: 改了该键，交付数值或生效参数**真的变**（sigma/variance 变化，或
//      帧内适配诊断键的出现/消失随配置值改变）；缺省（无 noise 块）⇒ 不出现
//      noise_cfg_applied，数值与改前逐位一致；
//   C. 非退化负例: 越界值 / 未知键 / 非对象块 ⇒ 节点 execute **显式失败**
//      （rc=3，禁静默忽略、禁静默夹取）；
//   D. 确定性: 同一配置两次运行 ⇒ 交付 sigma 逐位相同。
// ============================================================================
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/context.h"

#include "p1sess_fixtures.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#define W34_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define W34_GETPID static_cast<long>(::getpid())
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
      std::fprintf(stderr, "FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__,       \
                   #cond, (msg));                                              \
      ++g_fail;                                                                \
    } else {                                                                   \
      std::printf("ok  : %s\n", (msg));                                        \
    }                                                                          \
  } while (0)

// ── 合成星场: 9 颗孤立高斯星 + 确定性伪噪声背景（128x128）─────────────────
constexpr int kW = 128, kH = 128;

inline float field_pixel(int i, void*) {
  const int x = i % kW, y = i / kW;
  double v = 100.0;
  static const double sx[9] = {16, 44, 72, 100, 30, 58, 86, 114, 64};
  static const double sy[9] = {16, 20, 24, 28, 60, 64, 68, 72, 104};
  static const double amp[9] = {9000, 8000, 7000, 6000, 5000, 4000, 3000, 2000, 1500};
  for (int k = 0; k < 9; ++k) {
    const double dx = x - sx[k], dy = y - sy[k];
    v += amp[k] * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.6 * 1.6));
  }
  // 确定性伪噪声（splitmix64 派生; 与 fixtures 同族算法, 无 std::random 状态）
  uint64_t s = 0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(i + 1);
  s ^= s >> 30; s *= 0xbf58476d1ce4e5b9ULL;
  s ^= s >> 27; s *= 0x94d049bb133111ebULL;
  s ^= s >> 31;
  v += (static_cast<double>(s >> 11) / 9007199254740992.0 - 0.5) * 24.0;
  return static_cast<float>(v);
}

static json read_json(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return json::parse(std::string((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>()));
}

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

// 一个运行 = 一个独立 output_dir（star-psf 产出 p1_sources.json, noise-snr 消费）。
struct RunOut {
  json frame;
  json man;
  double sigma = 0.0;
  double variance = 0.0;
  bool valid = false;
};

static bool run_case(ModuleRegistry& reg, const fs::path& base, const char* name,
                     const json& noise_block, bool with_noise_block, RunOut* out,
                     std::string* err) {
  const std::string od = (base / name).string();
  std::error_code ec;
  fs::create_directories(od, ec);
  const std::string light = od + "/light_1.fits";
  if (p1sess::write_fits_file(light, kW, kH, field_pixel, nullptr) != 0) {
    *err = "cannot write fixture fits";
    return false;
  }
  {
    json cfg;
    cfg["input_lights"] = json::array({light});
    cfg["output_dir"] = od;
    if (!run_node(reg, "astrocs.phase1.star-psf", cfg.dump(), nullptr, err)) return false;
  }
  json cfg;
  cfg["input_lights"] = json::array({light});
  cfg["output_dir"] = od;
  cfg["snr"] = json{{"zero_point_mag", 25.0}, {"gain_e_per_adu", 1.5},
                    {"read_noise_e", 5.0}};
  if (with_noise_block) cfg["noise"] = noise_block;
  if (!run_node(reg, "astrocs.phase1.noise-snr", cfg.dump(), &out->man, err)) return false;
  const json snr = read_json(od + "/p1_snr.json");
  if (!snr.contains("frames") || !snr["frames"].is_array() || snr["frames"].empty()) {
    *err = "p1_snr.json has no frames";
    return false;
  }
  out->frame = snr["frames"][0];
  out->sigma = out->frame.value("sigma", 0.0);
  out->variance = out->frame.value("variance", 0.0);
  out->valid = out->frame.value("valid", false);
  return true;
}

// 逐键用例: 键名 + 该键的越界负例值 + 是否期望「交付数值真的变」。
struct KeyCase {
  const char* key;
  const char* block;      // doc["noise"] 的 JSON 文本（含该键）
  bool expect_numeric;    // true = sigma 必须与缺省运行不同
  const char* note;
};

int main() {
  const fs::path base = fs::temp_directory_path() /
      ("w34_noise_cfg_" + std::to_string(W34_GETPID));
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
  std::string err;

  // ── 基线（无 noise 块）: 不得出现回执, 数值即改前行为 ─────────────────────
  RunOut d1, d2;
  if (!run_case(reg, base, "default_a", json::object(), false, &d1, &err)) {
    std::fprintf(stderr, "FAIL: default run: %s\n", err.c_str());
    return 1;
  }
  if (!run_case(reg, base, "default_b", json::object(), false, &d2, &err)) {
    std::fprintf(stderr, "FAIL: default rerun: %s\n", err.c_str());
    return 1;
  }
  CHECK(!d1.man.contains("noise_cfg_applied"),
        "缺省: 无 doc[\"noise\"] ⇒ 不出现回执键 noise_cfg_applied");
  CHECK(d1.sigma == d2.sigma && d1.variance == d2.variance,
        "确定性: 同一配置两次运行交付 sigma/variance 逐位相同");
  std::printf("    [基线] valid=%d sigma=%.9g variance=%.9g\n", (int)d1.valid,
              d1.sigma, d1.variance);

  // ── 逐键: 回执 + 行为变化 ────────────────────────────────────────────────
  const KeyCase cases[] = {
    {"patch_grid", "{\"patch_grid\": [2, 2]}", true, "8x8 -> 2x2 (patch 数 64 -> 4)"},
    {"clip_sigma", "{\"clip_sigma\": 0.5}", true, "5.0 -> 0.5 (激进裁剪)"},
    {"spatial_field_enabled", "{\"spatial_field_enabled\": 0}", true,
     "平面空间方差场 -> 全局常量场"},
    {"mask_r_min_px", "{\"mask_r_min_px\": 30.0}", true, "1.5 -> 30 px (掩膜盖满画幅)"},
    {"mask_k_sigma", "{\"mask_k_sigma\": 1e-06}", true, "0.1 -> 1e-6 (半径顶到 rmax)"},
    {"mask_fwhm_floor_scale", "{\"mask_fwhm_floor_scale\": 1000.0}", true,
     "0.75 -> 1000 (半径顶到 rmax)"},
    {"mask_budget_min_patches", "{\"mask_budget_min_patches\": 1000000}", false,
     "8 -> 1e6 (帧内收缩为 patch_cap, 适配诊断出现)"},
    {"mask_budget_min_sky", "{\"mask_budget_min_sky\": 100}", false,
     "9216 -> 100 (缺省的帧内收缩消失)"},
    {"min_patch_samples", "{\"min_patch_samples\": 16384}", false,
     "64 -> 16384 (每 patch 样本不足 ⇒ 网格收缩/退化)"},
    {"max_clip_rounds", "{\"max_clip_rounds\": 0}", false, "2 -> 0 (不做裁剪轮)"},
    {"source_mask_radius_px", "{\"source_mask_radius_px\": 0.0}", false,
     "10 -> 0 (无固定掩膜半径)"},
    {"mask_radius_scale", "{\"mask_radius_scale\": 1000.0}", false,
     "6.0 -> 1000 (rmax 硬上界放大)"},
    {"variance_floor", "{\"variance_floor\": 1.0}", true, "1e-12 -> 1.0 ADU^2 (地板抬升)"},
    {"saturation_level", "{\"saturation_level\": 1e+09}", false,
     "0 -> 1e9 ADU (来源登记为 noise_config)"},
  };
  for (const KeyCase& c : cases) {
    RunOut o;
    err.clear();
    if (!run_case(reg, base, (std::string("case_") + c.key).c_str(),
                  json::parse(c.block), true, &o, &err)) {
      std::fprintf(stderr, "FAIL: case %s: %s\n", c.key, err.c_str());
      ++g_fail;
      ++g_check;
      continue;
    }
    // A. 回执: 该键必须在 noise_cfg_applied 里（被消费, 非静默忽略）
    const bool has_receipt = o.man.contains("noise_cfg_applied") &&
                             o.man["noise_cfg_applied"].is_object() &&
                             o.man["noise_cfg_applied"].contains(c.key);
    CHECK(has_receipt, (std::string("回执: noise_cfg_applied 含 ") + c.key).c_str());
    // B. 行为: 数值或生效参数真的变
    bool behaviour_changed = (o.sigma != d1.sigma) || (o.variance != d1.variance) ||
                             (o.valid != d1.valid);
    if (!behaviour_changed) {
      // 帧内适配诊断键的出现/消失同样由配置值驱动（生效参数改变）。
      static const char* kAdaptKeys[] = {"noise_patch_grid_adapted",
                                         "noise_mask_rmax_adapted",
                                         "noise_mask_sky_budget_adapted",
                                         "noise_mask_patch_budget_adapted"};
      for (const char* ak : kAdaptKeys) {
        if (o.man.contains(ak) != d1.man.contains(ak)) behaviour_changed = true;
        else if (o.man.contains(ak) && o.man[ak] != d1.man[ak]) behaviour_changed = true;
      }
    }
    CHECK(behaviour_changed,
          (std::string("行为: 改 ") + c.key + " 后交付数值或生效参数确实改变 (" +
           c.note + ")").c_str());
    if (c.expect_numeric) {
      CHECK(o.sigma != d1.sigma,
            (std::string("数值: ") + c.key + " 的改动必须改变交付 sigma").c_str());
    }
    std::printf("    [%s] valid=%d sigma=%.9g (base %.9g) variance=%.9g applied=%s\n",
                c.key, (int)o.valid, o.sigma, d1.sigma, o.variance,
                o.man.contains("noise_cfg_applied")
                    ? o.man["noise_cfg_applied"].dump().c_str() : "(none)");
  }

  // saturation_level 的来源登记（noise > snr > FITS 头）
  {
    RunOut o;
    err.clear();
    if (run_case(reg, base, "case_sat_src", json::parse("{\"saturation_level\": 1e+09}"),
                 true, &o, &err)) {
      CHECK(o.man.value("saturation_source", std::string("")) == "noise_config",
            "饱和电平来源: noise.saturation_level 生效时登记为 noise_config");
    } else {
      ++g_fail; ++g_check;
      std::fprintf(stderr, "FAIL: saturation source case: %s\n", err.c_str());
    }
  }

  // ── C. 非退化负例: 越界/未知键/非对象 ⇒ 显式失败 ─────────────────────────
  const char* bad[] = {
      "{\"clip_sigma\": -1.0}",
      "{\"patch_grid\": [1, 8]}",
      "{\"spatial_field_enabled\": 2}",
      "{\"bogus_key\": 1}",
      "{\"mask_budget_min_sky\": 0}",
      "5",
  };
  for (const char* b : bad) {
    RunOut o;
    err.clear();
    const bool ok = run_case(reg, base,
                             (std::string("bad_") + std::to_string(g_check)).c_str(),
                             json::parse(b), true, &o, &err);
    CHECK(!ok, (std::string("负例: noise=") + b + " 必须显式失败 (禁静默忽略)").c_str());
    if (!ok) std::printf("    [负例] noise=%s -> %s\n", b, err.c_str());
  }

  std::printf("\n=== noise 配置块接线锁: %d 通过, %d 失败 ===\n",
              g_check - g_fail, g_fail);
  fs::remove_all(base, ec);
  return g_fail == 0 ? 0 : 1;
}
