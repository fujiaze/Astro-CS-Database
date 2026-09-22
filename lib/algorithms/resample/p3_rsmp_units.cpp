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
std::string Bunit::canonical() const {
  if (adu_power == 0 && px_power == 0) return "1";
  std::string s;
  if (adu_power != 0) {
    s += "ADU";
    if (adu_power != 1) s += "^" + std::to_string(adu_power);
  }
  if (px_power != 0) {
    if (!s.empty()) s += "/";
    s += "sr";
    if (px_power != 1) s += "^" + std::to_string(px_power);
  }
  return s;
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
bool parse_bunit_string(const std::string& s, Bunit* out) {
  std::string t;
  for (char c : s) {
    if (!std::isspace(static_cast<unsigned char>(c))) t += c;
  }
  if (t.empty()) return false;
  if (t == "1") { *out = Bunit{0, 0}; return true; }
  int adu = 0;
  int px = 0;
  std::size_t slash = std::string::npos;
  // 先提取 ADU 段
  std::string left = t;
  slash = t.find('/');
  if (slash != std::string::npos) {
    left = t.substr(0, slash);
    std::string right = t.substr(slash + 1);
    // right 必须是 sr 或 sr^N（legacy "px"/"pixel" 读侧别名同幂次）
    const bool is_sr = right.rfind("sr", 0) == 0;
    const bool is_px = right.rfind("px", 0) == 0;
    const bool is_pixel = right.rfind("pixel", 0) == 0;
    if (!is_sr && !is_px && !is_pixel) return false;
    const std::size_t sym_len = is_sr ? 2 : (is_px ? 2 : 5);
    if (right.size() == sym_len) {
      px = 1;
    } else if (right.size() > sym_len + 1 && right[sym_len] == '^') {
      try { px = std::stoi(right.substr(sym_len + 1)); } catch (...) { return false; }
    } else {
      return false;
    }
  }
  if (left.rfind("ADU", 0) != 0) return false;
  if (left.size() == 3) {
    adu = 1;
  } else if (left.size() > 4 && left[3] == '^') {
    try { adu = std::stoi(left.substr(4)); } catch (...) { return false; }
  } else {
    return false;
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
      r.reason = "explicit_pixel_power";
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
