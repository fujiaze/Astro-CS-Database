/* v6_bunit.cpp — 冻结单位表与量纲代数实现。 */
#include "astro/aio/v6_bunit.h"

#include <cctype>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <vector>

namespace astrocs {
namespace aio {
namespace {

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

std::string trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

// 解析单个因子 "ADU"、"sr"、"ADU^2"、"sr^-2"、"1"。
// 立体角维符号 = "sr"（FITS 4.0 Table 3 IAU 基本单位）；legacy "px"/"pixel"
// 仅作读侧迁移别名（旧产品把同一物理量误写作 px 幂次（数值本就按立体角归一，未变）），写侧一律出 "sr"。
bool parse_factor(const std::string& factor, UnitExponents* acc) {
  const std::string f = trim(factor);
  if (f.empty()) return false;
  if (f == "1") return true;
  std::string name = f;
  int exp = 1;
  const std::size_t caret = f.find('^');
  if (caret != std::string::npos) {
    name = trim(f.substr(0, caret));
    const std::string es = trim(f.substr(caret + 1));
    if (es.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(es.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') return false;
    if (v < -64 || v > 64) return false;
    exp = static_cast<int>(v);
  }
  if (name == "ADU") {
    acc->adu_power += exp;
    return true;
  }
  if (name == "sr") {
    acc->px_power += exp;
    return true;
  }
  if (name == "px" || name == "pixel") {  // legacy 读侧别名（同一立体角维）
    acc->px_power += exp;
    return true;
  }
  return false;
}

}  // namespace

const char* quantity_symbol(Quantity q) {
  switch (q) {
    case Quantity::kSignalSb: return "signal_sb";
    case Quantity::kPixelVarianceIn: return "pixel_variance_in";
    case Quantity::kSbVarianceOut: return "sb_variance_out";
    case Quantity::kSbIvarOut: return "sb_ivar_out";
    case Quantity::kWInfo: return "W_info";
    case Quantity::kQ: return "Q";
    case Quantity::kFlux: return "flux";
    case Quantity::kPsfswRobustWeight: return "psfsw_robust_weight";
    default: return "unknown";
  }
}

const char* quantity_freeze_id(Quantity q) {
  switch (q) {
    case Quantity::kSignalSb: return "FZ-UNIT-SIGNAL-SB";
    case Quantity::kPixelVarianceIn: return "FZ-UNIT-VAR-IN";
    case Quantity::kSbVarianceOut: return "FZ-UNIT-VAR-SB";
    case Quantity::kSbIvarOut: return "FZ-UNIT-IVAR-SB";
    case Quantity::kWInfo: return "FZ-UNIT-WINFO";
    case Quantity::kQ: return "FZ-UNIT-Q";
    case Quantity::kFlux: return "FZ-UNIT-FLUX";
    case Quantity::kPsfswRobustWeight: return "FZ-UNIT-PSFSW";
    default: return "FZ-UNIT-UNKNOWN";
  }
}

bool parse_unit_exponents(const std::string& unit, UnitExponents* out) {
  if (out == nullptr) return false;
  const std::string u = trim(unit);
  if (u.empty()) return false;
  if (u.find('*') != std::string::npos) {
    // 乘积形式：逐因子累加 (显式支持，避免误判)。
    UnitExponents acc;
    for (const std::string& f : split(u, '*')) {
      if (!parse_factor(f, &acc)) return false;
    }
    *out = acc;
    return true;
  }
  const std::vector<std::string> parts = split(u, '/');
  if (parts.size() > 2 || parts.empty()) return false;
  UnitExponents acc;
  for (const std::string& f : split(parts[0], '*')) {
    if (!parse_factor(f, &acc)) return false;
  }
  if (parts.size() == 2) {
    for (const std::string& f : split(parts[1], '*')) {
      UnitExponents d;
      if (!parse_factor(f, &d)) return false;
      acc.adu_power -= d.adu_power;
      acc.px_power -= d.px_power;
    }
  }
  *out = acc;
  return true;
}

std::string format_unit_exponents(const UnitExponents& e) {
  if (e.adu_power == 0 && e.px_power == 0) return "1";
  std::ostringstream os;
  bool first = true;
  if (e.adu_power != 0) {
    os << "ADU";
    if (e.adu_power != 1) os << "^" << e.adu_power;
    first = false;
  }
  if (e.px_power != 0) {
    if (!first) os << "*";
    os << "sr";
    if (e.px_power != 1) os << "^" << e.px_power;
  }
  return os.str();
}

const char* frozen_unit_string(Quantity q) {
  switch (q) {
    case Quantity::kSignalSb: return "ADU/sr";
    case Quantity::kPixelVarianceIn: return "ADU^2";
    case Quantity::kSbVarianceOut: return "ADU^2/sr^2";
    case Quantity::kSbIvarOut: return "sr^2/ADU^2";
    case Quantity::kWInfo: return "ADU^-2";
    case Quantity::kQ: return "ADU^-1";
    case Quantity::kFlux: return "ADU";
    case Quantity::kPsfswRobustWeight: return "1";
    default: return "";
  }
}

bool unit_matches_frozen(Quantity q, const std::string& unit,
                         ValidationReport* report) {
  const char* frozen = frozen_unit_string(q);
  UnitExponents want, got;
  const bool want_ok = parse_unit_exponents(frozen, &want);
  const bool got_ok = parse_unit_exponents(unit, &got);
  const bool same = want_ok && got_ok && want.adu_power == got.adu_power &&
                    want.px_power == got.px_power;
  if (!same && report != nullptr) {
    report->add("G-UNIT-TABLE", quantity_freeze_id(q),
                std::string("quantity ") + quantity_symbol(q) + " unit '" + unit +
                    "' != frozen '" + frozen + "'");
  }
  return same;
}

bool quadratic_law_holds(const std::string& signal_unit,
                         const std::string& variance_unit,
                         const std::string& ivar_unit, std::string* reason) {
  UnitExponents s, v, i;
  if (!parse_unit_exponents(signal_unit, &s)) {
    if (reason) *reason = "signal unit unparsable: " + signal_unit;
    return false;
  }
  if (!parse_unit_exponents(variance_unit, &v)) {
    if (reason) *reason = "variance unit unparsable: " + variance_unit;
    return false;
  }
  if (v.adu_power != 2 * s.adu_power || v.px_power != 2 * s.px_power) {
    if (reason) {
      *reason = "variance unit != signal^2 (" + variance_unit + " vs " + signal_unit + ")";
    }
    return false;
  }
  if (!ivar_unit.empty()) {
    if (!parse_unit_exponents(ivar_unit, &i)) {
      if (reason) *reason = "ivar unit unparsable: " + ivar_unit;
      return false;
    }
    if (i.adu_power != -v.adu_power || i.px_power != -v.px_power) {
      if (reason) *reason = "ivar unit != 1/variance (" + ivar_unit + ")";
      return false;
    }
  }
  return true;
}

BunitCheck bunit_dimension_decidable(const std::string& bunit,
                                     const std::string& pixel_semantics,
                                     int pixel_area_power,
                                     bool has_target_pixel_area,
                                     double target_pixel_area) {
  BunitCheck out;
  UnitExponents e;
  if (!parse_unit_exponents(bunit, &e)) {
    out.reason = "BUNIT unparsable: '" + bunit + "'";
    return out;
  }
  // (a) 显式含立体角幂次（canonical "sr"）-> 量纲可判。
  if (e.px_power != 0) {
    out.decidable = true;
    out.reason = "explicit solid-angle power";
    return out;
  }
  // (b) BUNIT=ADU + provenance 声明 surface_brightness + pixel_area_power=-2。
  if (e.adu_power == 1 && e.px_power == 0 &&
      pixel_semantics == "surface_brightness" && pixel_area_power == -2) {
    if (!has_target_pixel_area || !(target_pixel_area > 0.0)) {
      out.reason = "surface_brightness ADU requires positive target_pixel_area";
      return out;
    }
    out.decidable = true;
    out.reason = "surface_brightness + pixel_area_power=-2 + target_pixel_area";
    return out;
  }
  out.reason = "bare '" + bunit + "' without surface_brightness pixel semantics";
  return out;
}

int default_pixel_area_power(Quantity q) {
  switch (q) {
    case Quantity::kSignalSb: return -2;
    case Quantity::kSbVarianceOut: return -4;
    case Quantity::kSbIvarOut: return 4;
    case Quantity::kFlux:
    case Quantity::kQ:
    case Quantity::kWInfo:
    case Quantity::kPsfswRobustWeight:
    case Quantity::kPixelVarianceIn:
      return 0;
    default: return 0;
  }
}

bool psfsw_unit_forbidden(const std::string& unit) {
  const std::string u = trim(unit);
  if (u == "1") return false;
  // 出现 flux/ivar 词，或非无量纲幂次 -> 禁止。
  if (u.find("flux") != std::string::npos) return true;
  if (u.find("ivar") != std::string::npos) return true;
  UnitExponents e;
  if (!parse_unit_exponents(u, &e)) return true;
  return !(e.adu_power == 0 && e.px_power == 0);
}

}  // namespace aio
}  // namespace astrocs
