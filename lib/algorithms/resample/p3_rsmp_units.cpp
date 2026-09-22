// lib/algorithms/resample/p3_rsmp_units.cpp
// 单位表 / BUNIT 二次律 / 模式枚举（冻结表 docs/contracts/DATA_SEMANTICS.md §31）。
#include "p3_rsmp.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace astrocs {
namespace p3rsmp {

const char* to_string(Status s) {
  switch (s) {
    case Status::Ok: return "Ok";
    case Status::Reject: return "Reject";
    case Status::Unavailable: return "Unavailable";
    case Status::NotImplemented: return "NotImplemented";
    case Status::InvalidArgument: return "InvalidArgument";
  }
  return "Unknown";
}

const char* to_string(P3Mode m) {
  switch (m) {
    case P3Mode::SurfaceBrightness: return "surface_brightness";
    case P3Mode::PointSourceFlux: return "point_source_flux";
    case P3Mode::Visualization: return "visualization";
  }
  return "unknown";
}
const char* mode_token(P3Mode m) { return to_string(m); }

bool is_production_mode(P3Mode) { return true; }  // 枚举本身即生产集合（FZ-P3-MODES）

bool is_retired_mode_token(const std::string& t) {
  // legacy {auto, support_x_snr2, 0, 1, 2}（FZ-FIELD-WEIGHTMODE / FZ-P3-MODES）
  return t == "auto" || t == "support_x_snr2" || t == "0" || t == "1" || t == "2" ||
         t == "legacy" || t == "support_x_snr2_legacy";
}

bool is_deferred_weight_mode_token(const std::string& t) { return t == "psf_snr_power"; }

Status parse_mode(const std::string& token, P3Mode* out) {
  if (out == nullptr) return Status::InvalidArgument;
  if (token == "surface_brightness") { *out = P3Mode::SurfaceBrightness; return Status::Ok; }
  if (token == "point_source_flux") { *out = P3Mode::PointSourceFlux; return Status::Ok; }
  if (token == "visualization") { *out = P3Mode::Visualization; return Status::Ok; }
  // legacy / deferred / 未知 → REJECT（FZ-P3-MODES / FZ-MODE-DEFERRED / C-004.1）
  return Status::Reject;
}

// ---------------------------------------------------------------------------
// Bunit
// ---------------------------------------------------------------------------
// 冻结单位表 canonical **产品 BUNIT 串**（docs/contracts/DATA_SEMANTICS.md §31.1/§31.1a）:
// 面亮度链（ADU 承载）的立体角维一律写 "sr"，符号幂次 = px_power/2
// （px_power = -2 ⇔ "/sr"、-4 ⇔ "/sr^2"、+4 ⇔ "sr^2/…"；px_power 是内部线性像元幂次编码）；
// 纯像元面积单位（adu_power == 0，support/coverage 的 px^2，§12.2）保持冻结符号 "px^2"。
std::string Bunit::canonical() const {
  if (adu_power == 0 && px_power == 0) return "1";
  if (adu_power == 0) {
    std::string s = "px";
    if (px_power != 1) s += "^" + std::to_string(px_power);
    return s;
  }
  const int sr = px_power / 2;   // 内部线性像元幂次 → 立体角符号幂次
  if (adu_power > 0) {
    std::string s = "ADU";
    if (adu_power != 1) s += "^" + std::to_string(adu_power);
    if (sr != 0) {
      s += "/sr";
      if (sr != -1) s += "^" + std::to_string(-sr);
    }
    return s;
  }
  if (sr > 0) {
    std::string s = "sr";
    if (sr != 1) s += "^" + std::to_string(sr);
    s += "/ADU";
    if (adu_power != -1) s += "^" + std::to_string(-adu_power);
    return s;
  }
  if (sr == 0) return std::string("ADU^-") + std::to_string(-adu_power);
  // 冻结词汇表外的组合（ADU 负幂次 × 立体角负幂次）: 保序书写，不静默改写
  return "ADU^" + std::to_string(adu_power) + "/sr^" + std::to_string(-sr);
}

Bunit bunit_mul(const Bunit& a, const Bunit& b) {
  return Bunit{a.adu_power + b.adu_power, a.px_power + b.px_power};
}
Bunit bunit_square(const Bunit& a) { return bunit_mul(a, a); }
Bunit bunit_inverse(const Bunit& a) { return Bunit{-a.adu_power, -a.px_power}; }

namespace units {
const Bunit signal_sb{1, -2};          // ADU/sr
const Bunit pixel_variance_in{2, 0};   // ADU^2
const Bunit sb_variance_out{2, -4};    // ADU^2/sr^2
const Bunit sb_ivar_out{-2, 4};        // sr^2/ADU^2
const Bunit w_info{-2, 0};             // ADU^-2
const Bunit q_stat{-1, 0};             // ADU^-1
const Bunit flux{1, 0};                // ADU
const Bunit psfsw{0, 0};               // 1
}  // namespace units

bool is_quadratic_variance(const Bunit& signal, const Bunit& variance) {
  const Bunit sq = bunit_square(signal);
  return sq.adu_power == variance.adu_power && sq.px_power == variance.px_power;
}
bool is_inverse_pair(const Bunit& variance, const Bunit& ivar) {
  const Bunit inv = bunit_inverse(variance);
  return inv.adu_power == ivar.adu_power && inv.px_power == ivar.px_power;
}

namespace {
// 解析 canonical BUNIT 串；返回 false 表示串本身不可解析（不是不可判）。
// canonical 面亮度三串（DATA_SEMANTICS §31.1a）: "ADU/sr"（signal）、
// "ADU^2/sr^2"（VARIANCE）、"sr^2/ADU^2"（IVAR）；读侧兼容旧冻结串 "px"/"pixel"
// （旧表把像元面积记作 px^N ⇒ 与 sr^(N/2) 同一立体角维，映射到同一内部幂次）。
bool parse_bunit_string(const std::string& s, Bunit* out) {
  std::string t;
  for (char c : s) {
    if (!std::isspace(static_cast<unsigned char>(c))) t += c;
  }
  if (t.empty()) return false;
  if (t == "1") { *out = Bunit{0, 0}; return true; }
  // 单因子符号幂次（幂次可省略 = 1、可带负号）。
  auto factor_pow = [](const std::string& f, const char* sym, int* pow_out) -> bool {
    const std::size_t n = std::string(sym).size();
    if (f.rfind(sym, 0) != 0) return false;
    if (f.size() == n) { *pow_out = 1; return true; }
    if (f.size() > n + 1 && f[n] == '^') {
      try { *pow_out = std::stoi(f.substr(n + 1)); } catch (...) { return false; }
      return true;
    }
    return false;
  };
  // 立体角/像元面积因子 → 内部 px_power（线性像元幂次编码）:
  // "sr^e" ⇒ 2e（立体角符号幂次 ×2，与 units::* 冻结编码一致）；
  // legacy "px^e"/"pixel^e" ⇒ e（像元面积幂次，须为偶，否则无整数 sr 等价）。
  auto area_power = [&factor_pow](const std::string& f, int sign, int* px_power) -> bool {
    int e = 0;
    if (factor_pow(f, "sr", &e)) { *px_power = 2 * e * sign; return true; }
    if (factor_pow(f, "pixel", &e) || factor_pow(f, "px", &e)) {
      if (e % 2 != 0) return false;
      *px_power = e * sign;
      return true;
    }
    return false;
  };
  const std::size_t slash = t.find('/');
  const std::string left = (slash == std::string::npos) ? t : t.substr(0, slash);
  const std::string right =
      (slash == std::string::npos) ? std::string() : t.substr(slash + 1);
  int adu = 0, px = 0;
  if (left.rfind("ADU", 0) == 0) {
    // "ADU^a" 或 "ADU^a/<立体角因子>"
    if (!factor_pow(left, "ADU", &adu)) return false;
    if (!right.empty() && !area_power(right, -1, &px)) return false;
  } else {
    // 冻结表 ivar 形态 "<立体角因子>/ADU^a"（分母的 ADU ⇒ 负幂次）
    if (right.rfind("ADU", 0) != 0) return false;
    if (!factor_pow(right, "ADU", &adu)) return false;
    adu = -adu;
    if (!area_power(left, +1, &px)) return false;
  }
  *out = Bunit{adu, px};
  return true;
}
}  // namespace

BunitResolution resolve_bunit(const std::string& bunit_str, const BunitProvenance& prov) {
  BunitResolution r;
  Bunit parsed;
  if (parse_bunit_string(bunit_str, &parsed)) {
    // (a) 显式含立体角幂次 → 恒可判
    if (parsed.px_power != 0) {
      r.resolvable = true;
      r.resolved = parsed;
      r.reason = "explicit_solid_angle_power";
      return r;
    }
    // 裸 ADU(^N)：须 provenance 声明像素语义 + pixel_area_power（FZ-BUNIT-SEMANTICS）
    const bool flux_ok = prov.pixel_semantics == PixelSemantics::IntegratedFlux &&
                         prov.pixel_area_power_present && prov.pixel_area_power == 0;
    const bool sb_ok = prov.pixel_semantics == PixelSemantics::SurfaceBrightness &&
                       prov.pixel_area_power_present && prov.pixel_area_power == -2;
    if (parsed.adu_power == 1 && flux_ok) {
      r.resolvable = true;
      r.resolved = parsed;
      r.reason = "adu_with_integrated_flux_provenance";
      return r;
    }
    if (parsed.adu_power == 1 && sb_ok) {
      r.resolvable = true;
      r.resolved = Bunit{parsed.adu_power, -2};
      r.reason = "adu_with_surface_brightness_provenance";
      return r;
    }
    r.resolvable = false;
    r.reason = "bare_adu_without_pixel_semantics_and_pixel_area_power";
    return r;
  }
  r.resolvable = false;
  r.reason = "unparsable_bunit_string";
  return r;
}

}  // namespace p3rsmp
}  // namespace astrocs
