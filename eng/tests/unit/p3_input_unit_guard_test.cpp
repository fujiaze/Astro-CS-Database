// eng/tests/unit/p3_input_unit_guard_test.cpp — Issue #91/#9 输入单位身份裁决单元测试
//
// 被测对象 = 唯一实现 acsd::phase3::p3_input_unit_verdict_from_text
// （lib/algorithms/resample/p3_input_unit.cpp），单位串归类经冻结单位表
// acsd::p3rsmp::resolve_bunit（p3_rsmp_units.cpp）。
//
// 期望值来源（非被测实现自身）:
//   * docs/science/unified/DATA_SEMANTICS.md §3「区分观测计数、线性通量和面亮度」
//     （线性基单位词表 {ADU, XPMODEL}；canonical 串 L/sr；裸 L 须像素语义声明补足）；
//   * docs/science/photometry/PHOTOMETRY.md §4（已施加 Gaia XP 模型标度 ⇒ XPMODEL）；
//   * FZ-BUNIT-SEMANTICS（unique-declaration / 禁 silent default）；
//   * 本 Issue 的判决合同表（REPORT §判决合同）。
//
// 负例必须是**有牙齿**的：若把裁决退回旧行为（缺 BUNIT 得 canonical "ADU/sr"），
// 本测试立即变红。
#include <cstdio>
#include <string>
#include <vector>

#include "p3_input_unit.h"

namespace {

int g_fail = 0;
int g_total = 0;

const char* kHead =
    "creator_did = ivo://acsd/issue91/unit-test\n"
    "hips_version = 1.4\n"
    "hips_order = 0\n"
    "hips_tile_width = 512\n"
    "hips_frame = equatorial\n"
    "dataproduct_type = image\n"
    "hips_tile_format = fits\n";

struct Case {
    const char* name;
    std::string body;
    bool accepted;
    bool input_error;      // 仅 accepted=false 时有意义
    const char* code;      // 仅 accepted=false 时有意义
    const char* bunit_raw; // accepted=true 时为期望原始声明；false 时忽略（nullptr）
    // accepted=true 时期望的 canonical 产品串；nullptr = kP3SignalBunitCanonical（ADU/sr）
    const char* bunit_canonical = nullptr;
};

void run(const Case& c) {
    ++g_total;
    const acsd::phase3::P3InputUnitVerdict v =
        acsd::phase3::p3_input_unit_verdict_from_text(kHead + c.body,
                                                      "/fixture/signal/properties");
    const char* want_canon =
        c.bunit_canonical ? c.bunit_canonical : acsd::phase3::kP3SignalBunitCanonical;
    bool ok = (v.accepted == c.accepted);
    std::string why;
    if (!ok) {
        why = "accepted mismatch";
    } else if (c.accepted) {
        if (v.bunit_canonical != want_canon) {
            ok = false;
            why = std::string("canonical != '") + want_canon + "' (got '" +
                  v.bunit_canonical + "')";
        } else if (v.bunit_raw != c.bunit_raw) {
            ok = false;
            why = "bunit_raw mismatch (got '" + v.bunit_raw + "')";
        }
    } else {
        if (v.code != c.code) {
            ok = false;
            why = "code mismatch (got '" + v.code + "', want '" + c.code + "')";
        }
        if (v.input_error != c.input_error) {
            ok = false;
            why += " input_error mismatch";
        }
        // 负例硬不变量：未通过时**绝不**给出 canonical 产品串
        if (!v.bunit_canonical.empty()) {
            ok = false;
            why += " rejected verdict must not carry a canonical unit";
        }
    }
    if (!ok) ++g_fail;
    std::printf("%-4s %-30s %s%s\n", ok ? "PASS" : "FAIL", c.name,
                c.accepted ? "accepted" : c.code,
                why.empty() ? "" : ("  <- " + why).c_str());
}

}  // namespace

int main() {
    const std::vector<Case> cases = {
        // ── 合法（身份已知且允许传递）────────────────────────────────────────
        {"legal_canonical_explicit", "BUNIT=ADU/sr\n", true, false, nullptr, "ADU/sr"},
        {"legal_canonical_with_semantics",
         "BUNIT=ADU/sr\nACSD_SIGNAL_UNIT=ADU/sr\nACSD_PIXEL_SEMANTICS=surface_brightness\n"
         "ACSD_PIXEL_AREA_POWER=-2\n", true, false, nullptr, "ADU/sr"},
        {"legal_sr_power_spelling", "BUNIT=ADU^1/sr^1\n", true, false, nullptr, "ADU^1/sr^1"},
        {"legal_bare_adu_with_provenance",
         "BUNIT=ADU\nACSD_PIXEL_SEMANTICS=surface_brightness\nACSD_PIXEL_AREA_POWER=-2\n",
         true, false, nullptr, "ADU"},

        // ── Issue #9: 已施加 Gaia XP 模型标度的模型通带单位族（XPMODEL）─────────
        // 期望值来源 = DATA_SEMANTICS §3 线性基词表 + PHOTOMETRY §4。
        {"legal_xpmodel_canonical", "BUNIT=XPMODEL/sr\n", true, false, nullptr,
         "XPMODEL/sr", "XPMODEL/sr"},
        {"legal_xpmodel_with_semantics",
         "BUNIT=XPMODEL/sr\nACSD_SIGNAL_UNIT=XPMODEL/sr\n"
         "ACSD_PIXEL_SEMANTICS=surface_brightness\nACSD_PIXEL_AREA_POWER=-2\n",
         true, false, nullptr, "XPMODEL/sr", "XPMODEL/sr"},
        {"legal_bare_xpmodel_with_provenance",
         "BUNIT=XPMODEL\nACSD_PIXEL_SEMANTICS=surface_brightness\n"
         "ACSD_PIXEL_AREA_POWER=-2\n",
         true, false, nullptr, "XPMODEL", "XPMODEL/sr"},

        // ── 否定例：缺失 / 空 / 重复 / 冲突 ──────────────────────────────────
        {"missing_bunit", "", false, true, "P3-INPUT-BUNIT-MISSING", nullptr},
        {"missing_bunit_only_alias", "ACSD_SIGNAL_UNIT=ADU/sr\n", false, true,
         "P3-INPUT-BUNIT-MISSING", nullptr},
        {"empty_bunit", "BUNIT=\n", false, true, "P3-INPUT-BUNIT-EMPTY", nullptr},
        {"empty_bunit_spaces", "BUNIT=   \n", false, true, "P3-INPUT-BUNIT-EMPTY", nullptr},
        {"duplicate_same_value", "BUNIT=ADU/sr\nBUNIT=ADU/sr\n", false, true,
         "P3-INPUT-BUNIT-DUPLICATE", nullptr},
        {"duplicate_case_alias", "BUNIT=ADU/sr\nbunit=ADU/sr\n", false, true,
         "P3-INPUT-BUNIT-DUPLICATE", nullptr},
        {"duplicate_different_value", "BUNIT=ADU/sr\nBUNIT=ADU\n", false, true,
         "P3-INPUT-BUNIT-CONFLICT", nullptr},
        {"declaration_surface_conflict",
         "BUNIT=ADU/sr\nACSD_SIGNAL_UNIT=ADU\n", false, true,
         "P3-INPUT-UNIT-DECLARATION-CONFLICT", nullptr},
        {"declaration_surface_conflict_model",
         "BUNIT=XPMODEL/sr\nACSD_SIGNAL_UNIT=ADU/sr\n", false, true,
         "P3-INPUT-UNIT-DECLARATION-CONFLICT", nullptr},

        // ── 否定例：格式 / 表外语汇 ─────────────────────────────────────────
        {"unparsable_garbage", "BUNIT=banana\n", false, true,
         "P3-INPUT-BUNIT-UNPARSABLE", nullptr},
        {"outside_frozen_vocabulary", "BUNIT=Jy/sr\n", false, true,
         "P3-INPUT-BUNIT-UNPARSABLE", nullptr},
        {"relative_flux_misuse", "BUNIT=ACSD_RELATIVE_FLUX\n", false, true,
         "P3-INPUT-BUNIT-UNPARSABLE", nullptr},
        {"bare_adu_without_provenance", "BUNIT=ADU\n", false, true,
         "P3-INPUT-BUNIT-UNDECIDABLE", nullptr},
        {"bare_adu_with_power_only", "BUNIT=ADU\nACSD_PIXEL_AREA_POWER=-2\n", false, true,
         "P3-INPUT-BUNIT-UNDECIDABLE", nullptr},
        {"bare_xpmodel_without_provenance", "BUNIT=XPMODEL\n", false, true,
         "P3-INPUT-BUNIT-UNDECIDABLE", nullptr},
        {"duplicate_non_unit_key", "BUNIT=ADU/sr\nobs_title=a\nobs_title=b\n", false, true,
         "P3-INPUT-PROPERTIES-DUPLICATE-KEY", nullptr},
        {"malformed_line", "BUNIT=ADU/sr\nnot-a-kv-line\n", false, true,
         "P3-INPUT-PROPERTIES-MALFORMED", nullptr},
        {"empty_key", "=x\nBUNIT=ADU/sr\n", false, true,
         "P3-INPUT-PROPERTIES-MALFORMED", nullptr},
        {"unparsable_area_power", "BUNIT=ADU\nACSD_PIXEL_SEMANTICS=surface_brightness\n"
                                  "ACSD_PIXEL_AREA_POWER=-2x\n", false, true,
         "P3-INPUT-PIXEL-AREA-POWER-UNPARSABLE", nullptr},

        // ── 否定例：跨基单位混合（Issue #9：量纲不可判）──────────────────────
        // 解析层已拒绝含两个线性基因子的串（P3-INPUT-BUNIT-UNPARSABLE，input_error）；
        // verdict 的 MIXED-BASE 分支是防御性兜底（resolve_bunit 若返回 mixed 才触发）。
        {"mixed_base_product_unparsable", "BUNIT=ADU*XPMODEL/sr\n", false, true,
         "P3-INPUT-BUNIT-UNPARSABLE", nullptr},

        // ── 否定例：已声明但非 signal 面亮度（科学语义）──────────────────────
        {"not_surface_brightness_variance", "BUNIT=ADU^2/sr^2\n", false, false,
         "P3-INPUT-NOT-SURFACE-BRIGHTNESS", nullptr},
        {"not_surface_brightness_variance_model", "BUNIT=XPMODEL^2/sr^2\n", false, false,
         "P3-INPUT-NOT-SURFACE-BRIGHTNESS", nullptr},
        {"not_surface_brightness_flux",
         "BUNIT=ADU\nACSD_PIXEL_SEMANTICS=integrated_flux\nACSD_PIXEL_AREA_POWER=0\n",
         false, false, "P3-INPUT-NOT-SURFACE-BRIGHTNESS", nullptr},
        {"unknown_pixel_semantics",
         "BUNIT=ADU/sr\nACSD_PIXEL_SEMANTICS=per_pixel_counts\n", false, false,
         "P3-INPUT-PIXEL-SEMANTICS-UNSUPPORTED", nullptr},
        // legacy 像元面积写法：维度等价但冻结口径明确否定 → 科学语义拒（读侧兼容 ≠ 写侧可接受）
        {"legacy_px_area_alias",
         "BUNIT=ADU/px^2\nACSD_PIXEL_SEMANTICS=surface_brightness\nACSD_PIXEL_AREA_POWER=-2\n",
         false, false, "P3-INPUT-NOT-SURFACE-BRIGHTNESS", nullptr},
        {"legacy_pixel_area_alias",
         "BUNIT=ADU/pixel^2\nACSD_PIXEL_SEMANTICS=surface_brightness\nACSD_PIXEL_AREA_POWER=-2\n",
         false, false, "P3-INPUT-NOT-SURFACE-BRIGHTNESS", nullptr},

        // ── 否定例：单位 ↔ 像素语义/面积幂次 矛盾（禁自行猜测）───────────────
        {"explicit_sb_vs_integrated_flux",
         "BUNIT=ADU/sr\nACSD_PIXEL_SEMANTICS=integrated_flux\nACSD_PIXEL_AREA_POWER=0\n",
         false, false, "P3-INPUT-UNIT-SEMANTICS-CONFLICT", nullptr},
        {"explicit_sb_vs_power_zero",
         "BUNIT=ADU/sr\nACSD_PIXEL_AREA_POWER=0\n", false, false,
         "P3-INPUT-UNIT-SEMANTICS-CONFLICT", nullptr},
        {"sb_semantics_vs_power_zero",
         "BUNIT=ADU\nACSD_PIXEL_SEMANTICS=surface_brightness\nACSD_PIXEL_AREA_POWER=0\n",
         false, false, "P3-INPUT-UNIT-SEMANTICS-CONFLICT", nullptr},
        {"flux_semantics_vs_power_minus2",
         "BUNIT=ADU\nACSD_PIXEL_SEMANTICS=integrated_flux\nACSD_PIXEL_AREA_POWER=-2\n",
         false, false, "P3-INPUT-UNIT-SEMANTICS-CONFLICT", nullptr},
    };

    for (const Case& c : cases) run(c);
    std::printf("\n%d/%d passed, %d failed\n", g_total - g_fail, g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}
