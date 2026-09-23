// ============================================================================
// variance_floor_test.cpp — 「按 dtype 导出的方差地板」验收面（独立于实现者）
//
// 被测面: lib/include/astrocs/core/variance_floor.h
// 权威: docs/science/NOISE_MODEL.md §7（floor 只作用于**可用**方差；不可用一律
//       ivar=0，**不得由 clamp 产生**；每帧生效 floor 及其来源必须随帧产品登记）、
//       §9（floor 是**纯数值保护**，保证 ivar 有限，不是最小可分辨方差/读出噪声下限）；
//       docs/contracts/DATA_SEMANTICS.md §13.2（fill 输出为 float32 产品）。
//
// 用例（每条都能红能绿，无恒 PASS 占位）:
//   V1 adu_scale            ADU 标度（α=1）：floor 可表示、不吞掉真实方差、ivar 有限
//   V2 alpha2_scale         α² 标度（α=2.3846837130250378e-17）：绝对常数 1e-12·α²
//                           在 float32 下**精确下溢为 0**（红锚），按 dtype 导出的
//                           floor 可表示且 ivar 有限（绿）
//   V3 not_swallowing       地板不吞掉真实方差：远大于 floor 的方差逐位不变
//   V4 unavailable_untouched 不可用（<=0）与非有限**原样透传**，绝不被 floor 伪装
//   V5 underflow_rescue     正值但 float32 下溢为 0 的样本被救援（保住「可用」态）；
//                           精确 0 不被救援（那是「不可用」，不是可表示性问题）
//   V6 provenance           floor/来源/dtype/中位数齐备（§7 登记要求）
//   V7 red_anchor_absolute  判别力红锚：把地板换成与数据无关的绝对常数，V2 必判红
// ============================================================================
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "astrocs/core/variance_floor.h"

static int g_fail = 0;
static int g_pass = 0;
static const char* g_case = "(init)";

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL [%s] %s:%d: %s\n", g_case, __FILE__,    \
                         __LINE__, #cond);                                     \
            ++g_fail;                                                          \
        } else {                                                               \
            ++g_pass;                                                          \
        }                                                                      \
    } while (0)

#define CHECK_MSG(cond, ...)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL [%s] ", g_case);                        \
            std::fprintf(stderr, __VA_ARGS__);                                 \
            std::fprintf(stderr, "\n");                                        \
            ++g_fail;                                                          \
        } else {                                                               \
            ++g_pass;                                                          \
        }                                                                      \
    } while (0)

using astrocs::VarianceFloor;
using astrocs::VarianceFloorSource;

// M42 实测帧测光标度（run/PERF-501/out/t2_16f_after/.../p1_stack.json photscal）
static const double kAlpha = 2.3846837130250378e-17;
static const double kFloorAdu = 1e-12;              // NOISE_MODEL §5/§9 冻结默认（ADU²）
static const double kEpsRel = 1e-12;                // 相对系数（该帧方差尺度的 1e-12）

int main() {
    // ── V1: ADU 标度（α=1）───────────────────────────────────────────────
    {
        g_case = "V1_adu_scale";
        // 该帧可用方差：真实空背景方差 ~1e-4 ADU²（σ=0.01 ADU）量级
        std::vector<double> frame(1000, 1.0e-4);
        const double med = astrocs::variance_median_usable(frame);
        CHECK_MSG(std::fabs(med - 1.0e-4) < 1e-18, "median=%.17g", med);
        const VarianceFloor f = astrocs::derive_variance_floor(med, kEpsRel, false);
        CHECK(f.value > 0.0);
        CHECK(std::isfinite(f.value));
        CHECK_MSG(f.source == VarianceFloorSource::kRelativeFrameScale,
                  "source=%s", astrocs::variance_floor_source_name(f.source));
        // 地板可表示：float32(floor) > 0
        CHECK((float)f.value > 0.0f);
        // 不吞掉真实方差：1e-4 ≫ floor ⇒ 逐位不变
        CHECK(astrocs::apply_variance_floor(1.0e-4, f) == 1.0e-4);
        // ivar 有限
        CHECK(std::isfinite(1.0 / astrocs::apply_variance_floor(1.0e-4, f)));
        // 比地板小的**可用**方差被抬到地板（§7 Floor 夹逼不变量）
        const double tiny = f.value * 0.5;
        CHECK(astrocs::apply_variance_floor(tiny, f) == f.value);
        CHECK(std::isfinite(1.0 / astrocs::apply_variance_floor(tiny, f)));
    }

    // ── V2: α² 标度（红锚 + 绿）─────────────────────────────────────────
    {
        g_case = "V2_alpha2_scale";
        const double abs_floor_alpha2 = kFloorAdu * kAlpha * kAlpha;
        // 红锚：绝对常数地板换算后**在 float32 下精确下溢为 0**
        CHECK_MSG(abs_floor_alpha2 > 0.0, "alpha^2*1e-12=%.6e", abs_floor_alpha2);
        CHECK_MSG((float)abs_floor_alpha2 == 0.0f,
                  "float32(alpha^2*1e-12)=%.6e (expect exact 0)",
                  (double)(float)abs_floor_alpha2);
        CHECK_MSG(1.0 / (double)(float)abs_floor_alpha2 ==
                      std::numeric_limits<double>::infinity(),
                  "1/float32(alpha^2*1e-12) 必须为 inf（证明绝对常数不可实现）");
        // 该帧真实可用方差（数组标度）：M42 实测 ~2.1e-29 ~ 3e-31 量级
        std::vector<double> frame(1000, 3.0e-31);
        const double med = astrocs::variance_median_usable(frame);
        const VarianceFloor f = astrocs::derive_variance_floor(med, kEpsRel, false);
        // 绿：dtype 导出的 floor 在 float32 下**可表示**
        CHECK_MSG((float)f.value > 0.0f, "float32(floor)=%.6e", (double)(float)f.value);
        CHECK_MSG(f.value >= astrocs::variance_dtype_min_positive(false),
                  "floor=%.6e < dtype_min=%.6e", f.value,
                  astrocs::variance_dtype_min_positive(false));
        CHECK(std::isfinite(f.value));
        // 绿：真实可用方差经地板后 ivar 有限
        const double v = astrocs::apply_variance_floor(med, f);
        CHECK(std::isfinite(v));
        CHECK_MSG(std::isfinite(1.0 / v), "1/v=%.6e", 1.0 / v);
        // 绿：真实方差不被地板吞掉（3e-31 > dtype_min 1.4e-45 ⇒ 不变）
        CHECK(v == med);
        // 相对项在本标度下确实占优
        CHECK_MSG(f.source == VarianceFloorSource::kRelativeFrameScale,
                  "source=%s value=%.6e", astrocs::variance_floor_source_name(f.source),
                  f.value);
    }

    // ── V3: 地板不吞掉真实方差（跨 8 个数量级扫描）────────────────────────
    {
        g_case = "V3_not_swallowing";
        const double med = 1.0e-30;
        const VarianceFloor f = astrocs::derive_variance_floor(med, kEpsRel, false);
        for (double m = 1.0e-2; m <= 1.0e6; m *= 10.0) {
            const double v = med * m;   // 全部 ≥ med ⇒ 全部高于地板
            CHECK_MSG(astrocs::apply_variance_floor(v, f) == v,
                      "m=%.0e v=%.6e floor=%.6e 被吞", m, v, f.value);
        }
        // f64 产品同样成立（dtype 下界更小）
        const VarianceFloor f64 = astrocs::derive_variance_floor(med, kEpsRel, true);
        CHECK(astrocs::apply_variance_floor(med, f64) == med);
        CHECK(std::isfinite(1.0 / astrocs::apply_variance_floor(med, f64)));
    }

    // ── V4: 不可用/损坏 原样透传（不得由 clamp 产生）──────────────────────
    {
        g_case = "V4_unavailable_untouched";
        const VarianceFloor f = astrocs::derive_variance_floor(1.0e-30, kEpsRel, false);
        CHECK(astrocs::apply_variance_floor(0.0, f) == 0.0);          // 不可用
        CHECK(astrocs::apply_variance_floor(-1.0, f) == -1.0);        // 损坏
        CHECK(astrocs::apply_variance_floor(-515.0, f) == -515.0);    // M42 角点实测值
        const double nan_v = std::nan("");
        CHECK(std::isnan(astrocs::apply_variance_floor(nan_v, f)));
        const double inf_v = std::numeric_limits<double>::infinity();
        CHECK(std::isinf(astrocs::apply_variance_floor(inf_v, f)));
        // 无可用方差帧：地板退到 dtype 下界，但仍不改写任何输入
        const VarianceFloor fz = astrocs::derive_variance_floor(0.0, kEpsRel, false);
        CHECK_MSG(fz.source == VarianceFloorSource::kNoUsableVariance, "source=%s",
                  astrocs::variance_floor_source_name(fz.source));
        CHECK(fz.value == astrocs::variance_dtype_min_positive(false));
        // 地板恒为正且在其 dtype 中可表示（否则「保证 ivar 有限」不成立）
        CHECK_MSG(fz.value > 0.0, "无可用方差帧的 floor=%.6e 必须 >0", fz.value);
        CHECK_MSG((float)fz.value > 0.0f, "float32(floor)=%.6e 必须 >0",
                  (double)(float)fz.value);
        CHECK(std::isfinite(1.0 / fz.value));
        CHECK(astrocs::apply_variance_floor(0.0, fz) == 0.0);
    }

    // ── V5: dtype 下溢救援 ──────────────────────────────────────────────
    {
        g_case = "V5_underflow_rescue";
        const double med = 1.0e-30;
        const VarianceFloor f = astrocs::derive_variance_floor(med, kEpsRel, false);
        // (a) 正但 float32 下溢：α²·1e-12 = 5.6867e-46 ⇒ f32 == 0
        const double under = kFloorAdu * kAlpha * kAlpha;
        bool rescued = false;
        const float out = astrocs::to_product_dtype_keeping_availability(under, f, &rescued);
        CHECK_MSG(rescued, "未报告下溢救援");
        CHECK_MSG(out > 0.0f, "out=%.6e (必须 >0：保住「可用」态)", (double)out);
        CHECK(std::isfinite(1.0 / (double)out));
        // (b) 精确 0（= 「不可用」）**不得**被救援
        bool r0 = false;
        const float z = astrocs::to_product_dtype_keeping_availability(0.0, f, &r0);
        CHECK(!r0);
        CHECK(z == 0.0f);
        // (c) 负值（= 损坏）不得被救援
        bool rn = false;
        const float n = astrocs::to_product_dtype_keeping_availability(-515.0, f, &rn);
        CHECK(!rn);
        CHECK(n == -515.0f);
        // (d) 正常可表示的正值逐位不变
        bool rp = false;
        const float p = astrocs::to_product_dtype_keeping_availability(3.0e-31, f, &rp);
        CHECK(!rp);
        CHECK(p == (float)3.0e-31);
    }

    // ── V6: 登记载荷（§7）──────────────────────────────────────────────
    {
        g_case = "V6_provenance";
        const VarianceFloor f = astrocs::derive_variance_floor(2.1e-29, kEpsRel, false);
        const std::string js = astrocs::variance_floor_provenance_json(f);
        for (const char* key : {"\"variance_floor\"", "\"variance_floor_source\"",
                                "\"variance_floor_dtype\"", "\"variance_floor_eps_rel\"",
                                "\"variance_floor_frame_median\""}) {
            CHECK_MSG(js.find(key) != std::string::npos, "缺键 %s: %s", key, js.c_str());
        }
        CHECK_MSG(js.find("\"float32\"") != std::string::npos, "dtype 未登记: %s", js.c_str());
        CHECK_MSG(js.find("relative_frame_scale") != std::string::npos,
                  "来源未登记: %s", js.c_str());
    }

    // ── V7: 判别力红锚 —— 绝对常数地板在本实验下必判红 ────────────────────
    {
        g_case = "V7_red_anchor_absolute";
        // 「修前」口径：floor = 1e-12·α²（绝对常数换算）
        const double bad_floor = kFloorAdu * kAlpha * kAlpha;
        const float bad_f32 = (float)bad_floor;
        const bool bad_ok = (bad_f32 > 0.0f) && std::isfinite(1.0 / (double)bad_f32);
        CHECK_MSG(!bad_ok,
                  "红锚失效: 绝对常数地板竟然可用 (f32=%.6e)", (double)bad_f32);
        // 「修后」口径：同一帧、同一 dtype ⇒ 可用
        const VarianceFloor f = astrocs::derive_variance_floor(3.0e-31, kEpsRel, false);
        const bool good_ok = ((float)f.value > 0.0f) &&
                             std::isfinite(1.0 / astrocs::apply_variance_floor(3.0e-31, f));
        CHECK_MSG(good_ok, "绿锚失效: dtype 导出地板不可用");
    }

    std::fprintf(stdout, "variance_floor_test: %d PASS / %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
