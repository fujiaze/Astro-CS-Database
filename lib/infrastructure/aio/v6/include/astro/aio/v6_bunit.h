/* v6_bunit.h — 冻结单位表 / 量纲代数 / BUNIT 可判性
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/infrastructure/aio/v6/。
 * 语义锚 (逐条不得偏离):
 *   FZ-UNIT-SIGNAL-SB   signal_sb          = ADU/sr
 *   FZ-UNIT-VAR-IN      pixel_variance_in  = ADU^2
 *   FZ-UNIT-VAR-SB      sb_variance_out    = ADU^2/sr^2
 *   FZ-UNIT-IVAR-SB     sb_ivar_out        = sr^2/ADU^2
 *   FZ-UNIT-WINFO       W_info             = ADU^-2
 *   FZ-UNIT-Q           Q                  = ADU^-1
 *   FZ-UNIT-FLUX        flux (F_hat)       = ADU
 *   FZ-UNIT-PSFSW       psfsw_robust_weight= 1 (无量纲)
 *   FZ-P3-BUNIT-QUADRATIC  variance = signal^2; ivar = 1/variance
 *   FZ-BUNIT-SEMANTICS  BUNIT 必须量纲可判: (a) 显式立体角幂次; 或
 *                       (b) BUNIT=ADU + pixel_semantics=surface_brightness +
 *                           pixel_area_power=-2 + 目标像素面积
 *
 * 本文件只做符号/量纲判定，不发明任何科学数值。
 */
#ifndef ASTROCS_V6_AIO_BUNIT_H
#define ASTROCS_V6_AIO_BUNIT_H

#include <string>

#include "astro/aio/v6_validation.h"

namespace astrocs {
namespace aio {

// 本模块单位词汇表条目。
enum class Quantity {
  kSignalSb,
  kPixelVarianceIn,
  kSbVarianceOut,
  kSbIvarOut,
  kWInfo,
  kQ,
  kFlux,
  kPsfswRobustWeight,
  kUnknown,
};

const char* quantity_symbol(Quantity q);
// 数量 -> 对应冻结条款 id (FZ-UNIT-*)；用于违规可追溯。
const char* quantity_freeze_id(Quantity q);

// 单位串 -> 幂次 (unit = ADU^adu_power * sr^px_power; px_power 记立体角维幂次)。解析失败返回 false。
// 接受: "1", "ADU", "ADU^-2", "ADU/sr", "ADU^2/sr^2", "sr^2/ADU^2"。
struct UnitExponents {
  int adu_power = 0;
  int px_power = 0;
};
bool parse_unit_exponents(const std::string& unit, UnitExponents* out);
std::string format_unit_exponents(const UnitExponents& e);

// 冻结表：数量 -> 冻结单位串。
const char* frozen_unit_string(Quantity q);
// 给定单位与冻结单位量纲等价 (幂次相同) 才通过；否则 append violation。
bool unit_matches_frozen(Quantity q, const std::string& unit,
                         ValidationReport* report);

// 二次律: variance == signal^2, ivar == 1/variance。
// signal/variance/ivar 必须可解析；ivar 可为空 (不检查)。
bool quadratic_law_holds(const std::string& signal_unit,
                         const std::string& variance_unit,
                         const std::string& ivar_unit, std::string* reason);

// BUNIT 可判性 (FZ-BUNIT-SEMANTICS)。
struct BunitCheck {
  bool decidable = false;
  std::string reason;
};
BunitCheck bunit_dimension_decidable(const std::string& bunit,
                                     const std::string& pixel_semantics,
                                     int pixel_area_power,
                                     bool has_target_pixel_area,
                                     double target_pixel_area);

// canonical pixel_area_power 缺省 (由 DATA §01 冻结表; 二次律唯一导出)。
int default_pixel_area_power(Quantity q);

// psfsw 禁止词 (FZ-UNIT-PSFSW: 出现 flux^-2/ivar 即 REJECT)。
bool psfsw_unit_forbidden(const std::string& unit);

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_BUNIT_H
