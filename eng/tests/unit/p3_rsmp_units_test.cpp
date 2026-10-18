// eng/tests/unit/p3_rsmp_units_test.cpp — Issue #9 冻结单位表（线性基单位）单元测试
//
// 被测对象 = 唯一实现 acsd::p3rsmp（lib/algorithms/resample/p3_rsmp_units.cpp）:
//   * 线性基词表 {ADU, XPMODEL} 与 canonical 面亮度链（signal/variance/ivar）；
//   * resolve_bunit 对显式立体角幂次串与裸线性基单位的归类；
//   * 二次律 is_quadratic_variance / is_inverse_pair（跨基单位必须不可判）。
//
// 期望值来源（非被测实现自身）:
//   * docs/science/unified/DATA_SEMANTICS.md §3「线性基单位词表（#9 冻结）」；
//   * docs/science/photometry/PHOTOMETRY.md §4；
//   * FZ-P3-BUNIT-QUADRATIC（variance = signal^2; ivar = 1/variance）。
#include <cstdio>
#include <string>
#include <vector>

#include "p3_rsmp.h"

namespace {

int g_fail = 0;
int g_total = 0;

void check(bool ok, const std::string& name, const std::string& detail) {
    ++g_total;
    if (!ok) ++g_fail;
    std::printf("%-4s %-42s %s\n", ok ? "PASS" : "FAIL", name.c_str(),
                detail.c_str());
}

void check_str(const std::string& got, const std::string& want,
               const std::string& name) {
    check(got == want, name, "got '" + got + "' want '" + want + "'");
}

}  // namespace

int main() {
    using namespace acsd::p3rsmp;

    // ── canonical 面亮度链（两族）────────────────────────────────────────────
    check_str(surface_brightness_bunit(LinearBase::Adu), "ADU/sr", "sb_bunit_adu");
    check_str(surface_brightness_bunit(LinearBase::XpModel), "XPMODEL/sr",
              "sb_bunit_xpmodel");
    check_str(surface_brightness_variance_bunit(LinearBase::Adu), "ADU^2/sr^2",
              "sb_var_bunit_adu");
    check_str(surface_brightness_variance_bunit(LinearBase::XpModel),
              "XPMODEL^2/sr^2", "sb_var_bunit_xpmodel");
    check_str(surface_brightness_ivar_bunit(LinearBase::Adu), "sr^2/ADU^2",
              "sb_ivar_bunit_adu");
    check_str(surface_brightness_ivar_bunit(LinearBase::XpModel),
              "sr^2/XPMODEL^2", "sb_ivar_bunit_xpmodel");
    check_str(std::string(to_string(LinearBase::Adu)), "ADU", "to_string_adu");
    check_str(std::string(to_string(LinearBase::XpModel)), "XPMODEL",
              "to_string_xpmodel");

    // ── 冻结 units:: 常量与派生串一致（单一来源，不漂移）─────────────────────
    check_str(units::signal_sb_model.canonical(), "XPMODEL/sr", "units_signal_sb_model");
    check_str(units::sb_variance_out_model.canonical(), "XPMODEL^2/sr^2",
              "units_sb_variance_out_model");
    check_str(units::sb_ivar_out_model.canonical(), "sr^2/XPMODEL^2",
              "units_sb_ivar_out_model");
    check_str(units::signal_sb.canonical(), "ADU/sr", "units_signal_sb");
    check_str(units::sb_variance_out.canonical(), "ADU^2/sr^2", "units_sb_variance_out");
    check_str(units::sb_ivar_out.canonical(), "sr^2/ADU^2", "units_sb_ivar_out");

    // ── resolve_bunit: 显式立体角幂次串（含模型族）恒可判 ─────────────────────
    const BunitProvenance prov{};
    {
        const BunitResolution r = resolve_bunit("XPMODEL/sr", prov);
        check(r.resolvable, "resolve_xpmodel_sb", r.reason);
        check_str(r.resolved.canonical(), "XPMODEL/sr", "resolve_xpmodel_sb_canonical");
        check(r.resolved.base == LinearBase::XpModel, "resolve_xpmodel_sb_base", "");
    }
    {
        const BunitResolution r = resolve_bunit("ADU/sr", prov);
        check(r.resolvable, "resolve_adu_sb", r.reason);
        check(r.resolved.base == LinearBase::Adu, "resolve_adu_sb_base", "");
    }
    {
        const BunitResolution r = resolve_bunit("sr^2/XPMODEL^2", prov);
        check(r.resolvable, "resolve_xpmodel_ivar", r.reason);
        check(r.resolved.adu_power == -2 && r.resolved.px_power == 4,
              "resolve_xpmodel_ivar_exponents", "");
    }
    {
        // 跨基乘积串：不可判（不得静默取其一）。
        const BunitResolution r = resolve_bunit("ADU*XPMODEL/sr", prov);
        check(!r.resolvable, "resolve_mixed_base_rejected", r.reason);
    }
    {
        // 裸线性基单位无 provenance ⇒ 不可判（ADU 与 XPMODEL 同判）。
        const BunitResolution r = resolve_bunit("XPMODEL", prov);
        check(!r.resolvable, "resolve_bare_xpmodel_undecidable", r.reason);
    }
    {
        BunitProvenance sb;
        sb.pixel_semantics = PixelSemantics::SurfaceBrightness;
        sb.pixel_area_power_present = true;
        sb.pixel_area_power = -2;
        const BunitResolution r = resolve_bunit("XPMODEL", sb);
        check(r.resolvable, "resolve_bare_xpmodel_with_provenance", r.reason);
        check_str(r.resolved.canonical(), "XPMODEL/sr",
                  "resolve_bare_xpmodel_canonical");
    }

    // ── 二次律（两族各自成立；跨基不成立）────────────────────────────────────
    check(is_quadratic_variance(units::signal_sb, units::sb_variance_out),
          "quadratic_adu", "");
    check(is_quadratic_variance(units::signal_sb_model, units::sb_variance_out_model),
          "quadratic_xpmodel", "");
    check(is_inverse_pair(units::sb_variance_out, units::sb_ivar_out),
          "inverse_adu", "");
    check(is_inverse_pair(units::sb_variance_out_model, units::sb_ivar_out_model),
          "inverse_xpmodel", "");
    check(!is_quadratic_variance(units::signal_sb, units::sb_variance_out_model),
          "quadratic_cross_base_must_fail", "");
    check(!is_inverse_pair(units::sb_variance_out, units::sb_ivar_out_model),
          "inverse_cross_base_must_fail", "");

    // ── 跨基相乘显式标记 mixed_base（不静默取其一）────────────────────────────
    {
        const Bunit m = bunit_mul(units::signal_sb, units::signal_sb_model);
        check(m.mixed_base, "mul_cross_base_sets_mixed", "");
        check_str(m.canonical(), "MIXED-BASE", "mul_cross_base_canonical");
    }

    std::printf("\n%d/%d passed, %d failed\n", g_total - g_fail, g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}
