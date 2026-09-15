// ============================================================================
// p1snr_frame_match_test.cpp - P33-FMATCH (P25-F2 静默数据丢失修复) 回归锁
//
// 缺陷 (P25 实测, NGC55): p1_op_noise 按**精确文件名(含扩展名)** join 上游
//   p1_sources.json 的 frames[].file。上游记录 "X.fts" 而本节点解析到 "X.fits"
//   (或反之) 时比较失败 -> snr_catalogue_status="unavailable_no_upstream_frame"
//   且全部 SNR 字段为 null, 而节点**仍返回成功** => 整帧 SNR 目录静默丢失。
//
// 本锁经**生产模块注册表** (register_phase_modules) 端到端跑真实节点 op
// (star-psf 产出真实 p1_sources.json -> 篡改 file 字段 -> 单独跑 noise-snr):
//   A. exact    : 精确名匹配 (正例; snr_catalogue_match="exact", snr_phot 有限)
//   B. stem     : 仅扩展名不一致 (.fits vs .fts) -> 必须按 stem 匹配成功,
//                 snr_catalogue_match="stem", snr_phot 与 exact 案**逐位一致**,
//                 manifest n_snr_frame_match_stem==1 (旧实现此处静默全 null)
//   C. missing  : 无任何匹配 -> **fail-closed**: 节点必须失败, 且不得写
//                 p1_snr.json (不允许"成功但全 null")
//   B2. prefix   : 上游名带 calibrated_ 前缀 (P25 NGC55 实测形态) -> 归一化成功
//   D. ambiguous: 两个同归一化键、均非精确名 -> 显式歧义失败
// 阴性对照: B 案先断言篡改后的 file 名确实 != 精确名 (证明 B 非恒真)。
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
#define P33_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P33_GETPID static_cast<long>(::getpid())
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

static bool write_json(const std::string& p, const json& j) {
  std::ofstream f(p, std::ios::binary);
  if (!f) return false;
  f << j.dump(2);
  return f.good();
}

// 单节点执行 (p1snr_frame_parity_test 同构)。
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
                    const std::string& light, std::string* err) {
  json cfg;
  cfg["input_lights"] = json::array({light});
  cfg["output_dir"] = out_dir;
  cfg["psf"] = json{{"max_stars", 0}};
  return run_node(reg, "astrocs.phase1.star-psf", cfg.dump(), nullptr, err);
}

static bool run_noise(ModuleRegistry& reg, const std::string& out_dir,
                      const std::string& light, json* frame, json* man,
                      std::string* err) {
  json cfg;
  cfg["input_lights"] = json::array({light});
  cfg["output_dir"] = out_dir;
  cfg["snr"] = json{{"zero_point_mag", 25.0}, {"gain_e_per_adu", 1.5},
                    {"read_noise_e", 5.0}};
  if (!run_node(reg, "astrocs.phase1.noise-snr", cfg.dump(), man, err))
    return false;
  const json snr = read_json(out_dir + "/p1_snr.json");
  if (!snr.contains("frames") || !snr["frames"].is_array() || snr["frames"].empty()) {
    *err = "p1_snr.json has no frames";
    return false;
  }
  if (frame != nullptr) *frame = snr["frames"][0];
  return true;
}

// 造一个 run 目录: 写输入帧 + 跑 star-psf 产出真实 p1_sources.json。
static bool prepare(ModuleRegistry& reg, const std::string& od,
                    std::string* err) {
  std::error_code ec;
  fs::create_directories(od, ec);
  if (p1sess::write_fits_file(od + "/light_1.fits", kW, kH, field_pixel,
                              nullptr) != 0) {
    *err = "cannot write fixture fits";
    return false;
  }
  return run_psf(reg, od, od + "/light_1.fits", err);
}

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "all";
  const fs::path base = fs::temp_directory_path() /
      ("p33_fmatch_" + std::to_string(P33_GETPID));
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

  // 正例/基准: 精确名匹配。
  double exact_snr_phot = 0.0;
  {
    const std::string od = (base / "exact").string();
    std::string err;
    if (!prepare(reg, od, &err)) {
      std::fprintf(stderr, "FAIL: prepare(exact): %s\n", err.c_str());
      return 1;
    }
    const json src = read_json(od + "/p1_sources.json");
    CHECK(src["frames"][0].value("file", std::string()) == "light_1.fits",
          "前置: 上游 p1_sources.json 记录 file=light_1.fits");
    json frame, man;
    if (!run_noise(reg, od, od + "/light_1.fits", &frame, &man, &err)) {
      std::fprintf(stderr, "FAIL: noise-snr(exact): %s\n", err.c_str());
      return 1;
    }
    if (mode == "all" || mode == "exact") {
      CHECK(frame.value("snr_catalogue_match", std::string()) == "exact",
            "A. exact: snr_catalogue_match=exact");
      CHECK(frame.contains("snr_phot") && frame["snr_phot"].is_number() &&
                std::isfinite(frame["snr_phot"].get<double>()),
            "A. exact: snr_phot 有限 (正例; 匹配成功)");
      CHECK(man.value("n_snr_frame_match_exact", -1) == 1 &&
                man.value("n_snr_frame_match_stem", -1) == 0,
            "A. exact: manifest n_snr_frame_match_exact=1 / _stem=0");
    }
    exact_snr_phot = frame.value("snr_phot", 0.0);
  }

  // B. stem 归一化: 仅扩展名不同 (.fits vs .fts) —— 旧实现此处静默全 null。
  {
    const std::string od = (base / "stem").string();
    std::string err;
    if (!prepare(reg, od, &err)) {
      std::fprintf(stderr, "FAIL: prepare(stem): %s\n", err.c_str());
      return 1;
    }
    json src = read_json(od + "/p1_sources.json");
    const std::string exact_name = src["frames"][0].value("file", std::string());
    src["frames"][0]["file"] = "light_1.fts";   // 只改扩展名
    if (!write_json(od + "/p1_sources.json", src)) {
      std::fprintf(stderr, "FAIL: cannot write tampered p1_sources.json\n");
      return 1;
    }
    CHECK(exact_name != "light_1.fts",
          "阴性对照: 篡改后的 file 名确实 != 精确名 (B 案非恒真)");
    json frame, man;
    const bool ok = run_noise(reg, od, od + "/light_1.fits", &frame, &man, &err);
    if (mode == "all" || mode == "stem") {
      if (ok)
        std::printf("info: stem case observed snr_catalogue_match=%s status=%s "
                    "snr_phot=%s\n",
                    frame.value("snr_catalogue_match", std::string("(none)")).c_str(),
                    frame.value("snr_catalogue_status", std::string("(none)")).c_str(),
                    frame.contains("snr_phot") ? frame["snr_phot"].dump().c_str() : "(absent)");
      std::fflush(stdout);
      if (ok) std::fflush(stdout);
      else
        std::printf("info: stem case node FAILED: %s\n", err.c_str());
      CHECK(ok, (std::string("B. stem: 扩展名不一致仍匹配成功 (旧实现静默全 null): ") + err).c_str());
      if (ok) {
        CHECK(frame.value("snr_catalogue_match", std::string()) == "normalized",
              "B. stem: snr_catalogue_match=normalized (provenance 可辨)");
        CHECK(frame.contains("snr_phot") && frame["snr_phot"].is_number() &&
                  std::isfinite(frame["snr_phot"].get<double>()),
              "B. stem: snr_phot 有限 (不再是 null)");
        CHECK(frame.contains("snr_phot") && frame["snr_phot"].is_number() &&
                  frame["snr_phot"].get<double>() == exact_snr_phot,
              "B. stem: snr_phot 与精确名案逐位一致 (同帧同数据)");
        CHECK(frame.value("snr_catalogue_status", std::string()) == "ok",
              "B. stem: snr_catalogue_status=ok (非 unavailable_no_upstream_frame)");
        CHECK(man.value("n_snr_frame_match_stem", -1) == 1,
              "B. stem: manifest n_snr_frame_match_stem=1 (显式计数, 不静默)");
      }
    }
  }

  // B2. 流水线前缀不一致 (上游 calibrated_<base>.fits, 本节点 <base>.fits)
  //     —— P25 NGC55 实测的真实形态 -> 归一化匹配必须成功。
  {
    const std::string od = (base / "prefix").string();
    std::string err;
    if (!prepare(reg, od, &err)) {
      std::fprintf(stderr, "FAIL: prepare(prefix): %s\n", err.c_str());
      return 1;
    }
    json src = read_json(od + "/p1_sources.json");
    src["frames"][0]["file"] = "calibrated_light_1.fits";
    if (!write_json(od + "/p1_sources.json", src)) return 1;
    json frame, man;
    const bool ok = run_noise(reg, od, od + "/light_1.fits", &frame, &man, &err);
    if (mode == "all" || mode == "prefix") {
      CHECK(ok, (std::string("B2. prefix: calibrated_ 前缀不一致仍匹配成功: ") + err).c_str());
      if (ok) {
        CHECK(frame.value("snr_catalogue_match", std::string()) == "normalized",
              "B2. prefix: snr_catalogue_match=normalized");
        CHECK(frame.contains("snr_phot") && frame["snr_phot"].is_number() &&
                  frame["snr_phot"].get<double>() == exact_snr_phot,
              "B2. prefix: snr_phot 与精确名案逐位一致");
        CHECK(man.value("n_snr_frame_match_stem", -1) == 1,
              "B2. prefix: manifest n_snr_frame_match_stem=1 (归一化计数)");
      }
    }
  }

  // C. 无匹配 -> fail-closed (不写 p1_snr.json, 不允许全 null 成功)。
  {
    const std::string od = (base / "missing").string();
    std::string err;
    if (!prepare(reg, od, &err)) {
      std::fprintf(stderr, "FAIL: prepare(missing): %s\n", err.c_str());
      return 1;
    }
    json src = read_json(od + "/p1_sources.json");
    src["frames"][0]["file"] = "totally_other_frame.fit";
    if (!write_json(od + "/p1_sources.json", src)) return 1;
    json man;
    std::string nerr;
    const bool ok = run_noise(reg, od, od + "/light_1.fits", nullptr, &man, &nerr);
    if (mode == "all" || mode == "missing") {
      CHECK(!ok, "C. missing: 无匹配 -> 节点显式失败 (fail-closed)");
      CHECK(nerr.find("no upstream") != std::string::npos,
            "C. missing: 错误信息指明无上游匹配帧");
      std::error_code e2;
      CHECK(!fs::exists(fs::u8path(od + "/p1_snr.json"), e2),
            "C. missing: 失败时不落 p1_snr.json (不静默全 null)");
    }
  }

  // D. 同 stem 多命中 (均非精确名) -> 显式歧义失败 (不猜)。
  {
    const std::string od = (base / "ambiguous").string();
    std::string err;
    if (!prepare(reg, od, &err)) {
      std::fprintf(stderr, "FAIL: prepare(ambiguous): %s\n", err.c_str());
      return 1;
    }
    json src = read_json(od + "/p1_sources.json");
    json a = src["frames"][0];
    json b = src["frames"][0];
    a["file"] = "light_1.fts";
    b["file"] = "light_1.fit";      // 两个同归一化键、均非精确名
    src["frames"] = json::array({a, b});
    if (!write_json(od + "/p1_sources.json", src)) return 1;
    json man;
    std::string nerr;
    const bool ok = run_noise(reg, od, od + "/light_1.fits", nullptr, &man, &nerr);
    if (mode == "all" || mode == "ambiguous") {
      CHECK(!ok, "D. ambiguous: 同 stem 多命中 -> 显式失败 (不猜)");
      CHECK(nerr.find("ambiguous") != std::string::npos,
            "D. ambiguous: 错误信息指明歧义 (normalized key 多命中)");
      std::error_code e2;
      CHECK(!fs::exists(fs::u8path(od + "/p1_snr.json"), e2),
            "D. ambiguous: 失败时不落 p1_snr.json");
    }
  }

  if (mode == "all" || mode == "exact" || mode == "stem" ||
      mode == "prefix" || mode == "missing" || mode == "ambiguous") {
    std::printf("%s: checks=%d fail=%d\n", mode.c_str(), g_check, g_fail);
    return g_fail == 0 ? 0 : 1;
  }
  std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
  return 2;
}
