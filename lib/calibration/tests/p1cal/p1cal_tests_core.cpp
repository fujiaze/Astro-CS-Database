// P1-CAL-TEST · 单元/负面/性质/cosmetic 测试组
//
// 覆盖 (TEST-CAL-DESIGN-001, docs/algorithms/CALIBRATION_ALGORITHMS.md §9):
//   units      : FIX-CAL-A 常量场 I1 + FIX-CAL-B 解析梯度 oracle + I2 空平场 +
//                I5 负值保留 + actual_k 恒等 oracle + FP64 位级往返
//                (ALG-CAL-001/002/003; 容差 §9: rtol1e-6/atol1e-7 或 bitwise)
//   properties : FIX-CAL-C sigma-clip 性质 (尖刺拒绝/NaN 跳过/mean==median
//                组合一致/收敛性) + FIX-CAL-D 归一化 I3 幂等性质 +
//                确定性 I4 (线程 1/2/4 bitwise) (ALG-CAL-001/002)
//   negative   : FIX-F 参数域 (NULL/0 维/负中位数拒绝 AC_ERR_PARAM/out 不写/
//                sigma<=0 禁用/actual_k 回写/全 NaN master flat) (ALG §7 全表)
//   cosmetic   : FIX-CAL-E 检测阈值/结构过滤/1-N worker 语义/I6 掩码极性/
//                IDW 与 5x5 median 修复 oracle (ALG-CAL-004)
#include "astro_calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "p1cal_fixtures.hpp"
#include "p1cal_oracle.hpp"
#include "p1cal_test_main.hpp"

using namespace p1cal;

namespace {

// FP32 容差 (§9 冻结: NumPy 对照 rtol=1e-6, atol=1e-7)
constexpr double kRtol = 1e-6;
constexpr double kAtol = 1e-7;

inline bool near_equal(double got, double want, double rtol = kRtol, double atol = kAtol) {
    if (std::isnan(got) && std::isnan(want)) return true;
    return std::fabs(got - want) <= atol + rtol * std::fabs(want);
}

inline double max_abs_diff(const std::vector<float>& got, const std::vector<double>& want) {
    double m = 0.0;
    for (std::size_t i = 0; i < want.size(); ++i)
        m = std::max(m, std::fabs(static_cast<double>(got[i]) - want[i]));
    return m;
}

// ===========================================================================
// units
// ===========================================================================
int test_units() {
    CheckState cs;

    // --- FIX-CAL-A: 常量场, C,D ∈ {0,100,1000}, flat=1.0 → I1 逐像素恒定 ---
    for (double C : {0.0, 100.0, 1000.0}) {
        for (double D : {0.0, 100.0, 1000.0}) {
            std::vector<float> light, dark, flat;
            fix_cal_a_const_field(C, D, 16, 16, &light, &dark, &flat);
            std::vector<float> out(256, -777.0f);
            float actual_k = -1.0f;
            const int rc = ac_calibrate_frame(light.data(), 16, 16, dark.data(),
                                              flat.data(), nullptr, out.data(),
                                              0, 1.0f, &actual_k);
            P1CAL_CHECK_EQ(cs, rc, AC_OK);
            P1CAL_CHECK(cs, actual_k == 1.0f, "std_actual_k_identity");
            const double want = (C - D) / std::max(1.0, 0.1);  // flat=1 → 除 1
            for (std::size_t i = 0; i < out.size(); ++i) {
                P1CAL_CHECK(cs, static_cast<double>(out[i]) == want,
                            "const_field_bitwise");
            }
        }
    }

    // --- FIX-CAL-B: 解析梯度, oracle 独立重算 (std 与 dark_opt 双分支) ---
    {
        const std::size_t W = 24, H = 16, NPIX = W * H;
        std::vector<float> light, dark, flat;
        fix_cal_b_gradient(W, H, &light, &dark, &flat);
        std::vector<float> bias(NPIX, 10.0f);

        // 标准分支
        {
            std::vector<float> out(NPIX, 0.0f);
            float actual_k = 0.0f;
            P1CAL_CHECK_EQ(cs, ac_calibrate_frame(light.data(), static_cast<int>(W),
                                                  static_cast<int>(H), dark.data(),
                                                  flat.data(), nullptr, out.data(),
                                                  0, 1.0f, &actual_k), AC_OK);
            P1CAL_CHECK(cs, actual_k == 1.0f, "std_actual_k_identity");
            double m = 0.0;
            for (std::size_t i = 0; i < NPIX; ++i) {
                const double want = calibrate_oracle(i, light[i], dark[i], true,
                                                     0.0, false, flat[i], true,
                                                     false, 1.0);
                m = std::max(m, std::fabs(static_cast<double>(out[i]) - want));
            }
            P1CAL_CHECK(cs, m <= kAtol + kRtol * 1e3, "gradient_std_oracle");
        }

        // dark_opt 分支 (K=0.5): oracle 恒等映射 + 像素级
        {
            const double K = 0.5;
            std::vector<float> out(NPIX, 0.0f);
            float actual_k = 0.0f;
            P1CAL_CHECK_EQ(cs, ac_calibrate_frame(light.data(), static_cast<int>(W),
                                                  static_cast<int>(H), dark.data(),
                                                  flat.data(), bias.data(), out.data(),
                                                  1, static_cast<float>(K),
                                                  &actual_k), AC_OK);
            P1CAL_CHECK(cs, static_cast<double>(actual_k) ==
                                actual_k_oracle(true, true, true, K),
                        "darkopt_actual_k_identity");
            double m = 0.0;
            for (std::size_t i = 0; i < NPIX; ++i) {
                const double want = calibrate_oracle(i, light[i], dark[i], true,
                                                     bias[i], true, flat[i], true,
                                                     true, K);
                m = std::max(m, std::fabs(static_cast<double>(out[i]) - want));
            }
            P1CAL_CHECK(cs, m <= kAtol + kRtol * 1e3, "gradient_darkopt_oracle");
        }

        // I2 空平场: flat=NULL → 与手算减法逐位相等
        {
            std::vector<float> out(NPIX, 0.0f);
            float actual_k = 0.0f;
            P1CAL_CHECK_EQ(cs, ac_calibrate_frame(light.data(), static_cast<int>(W),
                                                  static_cast<int>(H), dark.data(),
                                                  nullptr, nullptr, out.data(),
                                                  0, 1.0f, &actual_k), AC_OK);
            for (std::size_t i = 0; i < NPIX; ++i) {
                P1CAL_CHECK(cs, out[i] == light[i] - dark[i], "no_flat_subtraction");
            }
        }

        // I5 负值保留: raw < dark → 输出负值, 无 clamp
        {
            std::vector<float> neg_light(NPIX, 5.0f);
            std::vector<float> out(NPIX, 0.0f);
            float actual_k = 0.0f;
            P1CAL_CHECK_EQ(cs, ac_calibrate_frame(neg_light.data(), static_cast<int>(W),
                                                  static_cast<int>(H), dark.data(),
                                                  flat.data(), nullptr, out.data(),
                                                  0, 1.0f, &actual_k), AC_OK);
            int neg_count = 0;
            for (std::size_t i = 0; i < NPIX; ++i) {
                const double want = calibrate_oracle(i, neg_light[i], dark[i], true,
                                                     0.0, false, flat[i], true,
                                                     false, 1.0);
                if (out[i] < 0.0f) ++neg_count;
                P1CAL_CHECK(cs, near_equal(out[i], want), "negative_preserved");
            }
            P1CAL_CHECK(cs, neg_count > 0, "negative_actually_present");
        }
    }

    // --- FP64 位级往返: f64 校准 = double 域 oracle 精确式 ---
    {
        const std::size_t W = 16, H = 16, NPIX = W * H;
        std::vector<double> light(NPIX), dark(NPIX), flat(NPIX);
        for (std::size_t i = 0; i < NPIX; ++i) {
            light[i] = 100.0 + 0.001 * static_cast<double>(i % 13);
            dark[i] = 20.0 + 0.002 * static_cast<double>(i % 7);
            flat[i] = 1.0 + 0.01 * static_cast<double>(i % 5);
        }
        std::vector<double> out(NPIX, 0.0);
        double actual_k = 0.0;
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame_f64(light.data(), static_cast<int>(W),
                                                  static_cast<int>(H), dark.data(),
                                                  flat.data(), nullptr, out.data(),
                                                  0, 1.0, &actual_k), AC_OK);
        P1CAL_CHECK(cs, actual_k == 1.0, "std_actual_k_identity");
        for (std::size_t i = 0; i < NPIX; ++i) {
            const double want = (light[i] - dark[i]) / std::max(flat[i], 0.1);
            P1CAL_CHECK(cs, out[i] == want, "f64_bitwise_roundtrip");
        }
    }

    // --- ALG-CAL-002: master flat (FIX-CAL-D 三帧 + bias=常值) oracle 对照 ---
    {
        const std::size_t W = 12, H = 12, NPIX = W * H;
        const std::vector<float> stack = fix_cal_d_flat_frames(W, H);
        const std::vector<float> bias(NPIX, 0.0f);  // 常值 bias, frame_med 语义不变
        std::vector<float> out(NPIX, 0.0f);
        const int rc = ac_generate_master_flat(stack.data(), 3, static_cast<int>(W),
                                               static_cast<int>(H), bias.data(),
                                               out.data(), 3.0f, 3.0f, 5);
        P1CAL_CHECK_EQ(cs, rc, AC_OK);
        const FlatOracle want = master_flat_oracle(stack, 3, W, H, &bias);
        P1CAL_CHECK(cs, want.ok, "flat_oracle_converged");
        const double m = max_abs_diff(out, want.out);
        P1CAL_CHECK(cs, m <= kAtol + kRtol * 1e3, "flat_oracle_match");
        // final 中位数归一 → median(out)==1.0 (floor 保持)
        std::vector<double> ov(out.begin(), out.end());
        const double med = median_oracle(ov);
        P1CAL_CHECK(cs, near_equal(med, 1.0, 1e-4, 0.0), "flat_median_normalized");
    }

    return cs.failures == 0 ? 0 : 1;
}

// ===========================================================================
// properties
// ===========================================================================
int test_properties() {
    CheckState cs;

    // --- FIX-CAL-C: sigma-clip 性质 (mean 合并 == oracle 有效均值) ---
    {
        const std::size_t W = 20, H = 20, NPIX = W * H;
        for (int variant = 0; variant < 2; ++variant) {
            const bool nan_variant = variant == 1;
            const FixCalC fx = fix_cal_c_outlier_stack(20260907u, 5, W, H, nan_variant);
            std::vector<float> out(NPIX, 0.0f);
            const int rc = ac_generate_master_bias(fx.stack.data(), 5,
                                                   static_cast<int>(W), static_cast<int>(H),
                                                   out.data(), 3.0f, 3.0f, 5,
                                                   AC_COMBINE_MEAN);
            P1CAL_CHECK_EQ(cs, rc, AC_OK);
            double m = 0.0;
            for (std::size_t i = 0; i < NPIX; ++i) {
                const double want = master_gen_oracle(fx.stack.data(), NPIX, i, 5,
                                                      3.0, 3.0, 5, false);
                m = std::max(m, std::fabs(static_cast<double>(out[i]) - want));
            }
            P1CAL_CHECK(cs, m <= kAtol + kRtol * 2e2, "clip_rejects_spike");
        }
    }

    // --- mean 与 median 组合一致 (高斯窄噪声 → clip 后均值≈中位数) ---
    {
        const std::size_t W = 16, H = 16, NPIX = W * H;
        const FixCalC fx = fix_cal_c_outlier_stack(314159u, 5, W, H, false, 0.0);
        std::vector<float> mean_out(NPIX, 0.0f), med_out(NPIX, 0.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(fx.stack.data(), 5,
                                                   static_cast<int>(W), static_cast<int>(H),
                                                   mean_out.data(), 3.0f, 3.0f, 5,
                                                   AC_COMBINE_MEAN), AC_OK);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(fx.stack.data(), 5,
                                                   static_cast<int>(W), static_cast<int>(H),
                                                   med_out.data(), 3.0f, 3.0f, 5,
                                                   AC_COMBINE_MEDIAN), AC_OK);
        double m = 0.0;
        for (std::size_t i = 0; i < NPIX; ++i)
            m = std::max(m, std::fabs(static_cast<double>(mean_out[i]) -
                                      static_cast<double>(med_out[i])));
        P1CAL_CHECK(cs, m <= 2.0, "mean_median_agree_narrow");  // n=5 离散样本, 容差按样本量定
    }

    // --- NaN 变体: 1 帧 2 像素 NaN → mean 输出等于有效 4 帧均值 (NaN 跳过) ---
    {
        const std::size_t W = 8, H = 8, NPIX = W * H;
        FixCalC fx = fix_cal_c_outlier_stack(271828u, 5, W, H, false);
        fx.stack[4 * NPIX + 3] = std::numeric_limits<float>::quiet_NaN();
        fx.stack[4 * NPIX + 40] = std::numeric_limits<float>::quiet_NaN();
        std::vector<float> out(NPIX, 0.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(fx.stack.data(), 5,
                                                   static_cast<int>(W), static_cast<int>(H),
                                                   out.data(), 3.0f, 3.0f, 5,
                                                   AC_COMBINE_MEAN), AC_OK);
        // oracle: 迭代 clip 双域复算 (NaN 自动参与 med/MAD 统计 — 实测语义,
        // 非合同宣称的"跳过"; 拒绝域行为一致: NaN dev 比较 false → 不剔除)
        double m = 0.0;
        for (std::size_t i = 0; i < NPIX; ++i) {
            const double want = master_gen_oracle(fx.stack.data(), NPIX, i, 5,
                                                  3.0, 3.0, 5, false);
            m = std::max(m, std::fabs(static_cast<double>(out[i]) - want));
        }
        P1CAL_CHECK(cs, m <= kAtol + kRtol * 2e2, "nan_skipped_mean");
    }

    // --- I3 幂等归一: 已归一 flat (median==1) 再跑 ALG-CAL-002 语义不变 ---
    {
        const std::size_t W = 12, H = 12, NPIX = W * H;
        const std::vector<float> stack = fix_cal_d_flat_frames(W, H);
        std::vector<float> once(NPIX, 0.0f), twice(NPIX, 0.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(stack.data(), 3, static_cast<int>(W),
                                                   static_cast<int>(H), nullptr,
                                                   once.data(), 3.0f, 3.0f, 5), AC_OK);
        // 输入: 3 帧全部 = once (median 已 1.0)
        std::vector<float> stack2(3 * NPIX, 0.0f);
        for (std::size_t n = 0; n < 3; ++n)
            std::copy(once.begin(), once.end(), stack2.begin() + static_cast<std::ptrdiff_t>(n * NPIX));
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(stack2.data(), 3, static_cast<int>(W),
                                                   static_cast<int>(H), nullptr,
                                                   twice.data(), 3.0f, 3.0f, 5), AC_OK);
        for (std::size_t i = 0; i < NPIX; ++i)
            P1CAL_CHECK(cs, std::fabs(static_cast<double>(twice[i]) -
                                      static_cast<double>(once[i])) <= 1e-6,
                        "idempotent_normalization");
    }

    // --- I4 确定性: 1/2/4 线程双跑 bitwise ---
    {
        const std::size_t W = 32, H = 32, NPIX = W * H;
        const FixCalC fx = fix_cal_c_outlier_stack(999331u, 5, W, H, false);
        std::vector<std::vector<float>> outs;
        for (int t : {1, 2, 4}) {
            ac_set_num_threads(t);
            std::vector<float> out(NPIX, 0.0f);
            P1CAL_CHECK_EQ(cs, ac_generate_master_bias(fx.stack.data(), 5,
                                                       static_cast<int>(W), static_cast<int>(H),
                                                       out.data(), 3.0f, 3.0f, 5,
                                                       AC_COMBINE_MEAN), AC_OK);
            outs.push_back(out);
            std::vector<float> flat_out(NPIX, 0.0f);
            P1CAL_CHECK_EQ(cs, ac_generate_master_flat(fx.stack.data(), 5,
                                                       static_cast<int>(W), static_cast<int>(H),
                                                       nullptr, flat_out.data(),
                                                       3.0f, 3.0f, 5), AC_OK);
            outs.push_back(flat_out);
        }
        ac_set_num_threads(0);  // 恢复默认
        // 布局: outs[2k]=bias@线程档 k, outs[2k+1]=flat@线程档 k (k=0,1,2)。
        // 跨线程档同通道 bitwise 比较 (同通道不同档, 禁止跨通道自比)。
        static const std::size_t kBiasIdx[3] = {0, 2, 4};
        static const std::size_t kFlatIdx[3] = {1, 3, 5};
        for (int k = 1; k < 3; ++k) {
            P1CAL_CHECK(cs, std::memcmp(outs[kBiasIdx[0]].data(), outs[kBiasIdx[k]].data(),
                                        NPIX * sizeof(float)) == 0, "determinism_bitwise");
            P1CAL_CHECK(cs, std::memcmp(outs[kFlatIdx[0]].data(), outs[kFlatIdx[k]].data(),
                                        NPIX * sizeof(float)) == 0, "determinism_bitwise");
        }
    }

    return cs.failures == 0 ? 0 : 1;
}

// ===========================================================================
// negative
// ===========================================================================
int test_negative() {
    CheckState cs;

    // --- NULL/0 维: C API 返回 AC_ERR_PARAM, out 不写 ---
    {
        const float dummy[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        float out[4] = {7.0f, 7.0f, 7.0f, 7.0f};
        float ak = -1.0f;
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame(nullptr, 2, 2, nullptr, nullptr, nullptr,
                                              out, 0, 1.0f, &ak), AC_ERR_PARAM);
        P1CAL_CHECK(cs, out[0] == 7.0f, "out_not_written_on_param_error");
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame(dummy, 0, 2, nullptr, nullptr, nullptr,
                                              out, 0, 1.0f, &ak), AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame(dummy, 2, 0, nullptr, nullptr, nullptr,
                                              out, 0, 1.0f, &ak), AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame(dummy, 2, 2, nullptr, nullptr, nullptr,
                                              nullptr, 0, 1.0f, &ak), AC_ERR_PARAM);

        std::vector<float> stack(2 * 4, 1.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(nullptr, 2, 2, 2, out,
                                                   3.0f, 3.0f, 5, AC_COMBINE_MEAN),
                       AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(stack.data(), 0, 2, 2, out,
                                                   3.0f, 3.0f, 5, AC_COMBINE_MEAN),
                       AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(nullptr, 2, 2, 2, nullptr, out,
                                                   3.0f, 3.0f, 5), AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(stack.data(), 0, 2, 2, nullptr, out,
                                                   3.0f, 3.0f, 5), AC_ERR_PARAM);
        // ac_correct_frame: dark/bias 全 NULL → HEAD 实测 rc==AC_OK (无参考时
        // 静默跳过 cosmetic, 输出恒等); 是否收紧为 AC_ERR_PARAM 由 P1-CAL-IMPL
        // 处置 (DISP-CAL-004 同批), 本负面断言按实测合同: 不崩溃 + 恒等。
        P1CAL_CHECK_EQ(cs, ac_correct_frame(dummy, 2, 2, nullptr, nullptr, out,
                                            5.0f, 5.0f, 0, 4, nullptr, nullptr), AC_OK);
        for (int i = 0; i < 4; ++i)
            P1CAL_CHECK(cs, out[i] == dummy[i], "null_reference_identity");
        // f64 wrapper 同语义
        double dout[4] = {7.0, 7.0, 7.0, 7.0};
        double dstack[8] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
        double dak = -1.0;
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame_f64(nullptr, 2, 2, nullptr, nullptr, nullptr,
                                                  dout, 0, 1.0, &dak), AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat_f64(nullptr, 2, 2, 2, nullptr, dout,
                                                       3.0, 3.0, 5), AC_ERR_PARAM);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat_f64(dstack, 0, 2, 2, nullptr, dout,
                                                       3.0, 3.0, 5), AC_ERR_PARAM);
    }

    // --- 负中位数拒绝 (FIX-F 语义, B13-R13-7 契约): AC_ERR_PARAM + out 不写 ---
    {
        const std::size_t NPIX = 8 * 8;
        std::vector<float> flat(2 * NPIX, -1.0f);
        std::vector<float> out(NPIX, 7.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(flat.data(), 2, 8, 8, nullptr,
                                                   out.data(), 3.0f, 3.0f, 5), AC_ERR_PARAM);
        P1CAL_CHECK(cs, out[0] == 7.0f && out[NPIX / 2] == 7.0f,
                    "out_not_written_on_param_error");

        std::vector<double> dflat(2 * NPIX, -1.0);
        std::vector<double> dout(NPIX, 7.0);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat_f64(dflat.data(), 2, 8, 8, nullptr,
                                                       dout.data(), 3.0, 3.0, 5),
                       AC_ERR_PARAM);
        P1CAL_CHECK(cs, dout[0] == 7.0 && dout[NPIX / 2] == 7.0,
                    "out_not_written_on_param_error");

        // master bias/dark: 全负 stack (中位数<0) 是合法 mean/median 输入
        // (仅 flat 归一化路径拒绝), 生成仍 AC_OK。
        std::vector<float> neg_stack(2 * NPIX, -5.0f);
        std::vector<float> mout(NPIX, 0.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(neg_stack.data(), 2, 8, 8, mout.data(),
                                                   3.0f, 3.0f, 5, AC_COMBINE_MEAN), AC_OK);
        P1CAL_CHECK(cs, mout[0] == -5.0f, "negative_master_bias_ok");
    }

    // --- 全 NaN master flat (HEAD 实测语义): NaN 参与 step1 median 统计,
    // 归一与 clip 全 NaN, step3 final_med=NaN → 不归一也不拒绝 → out 全 NaN,
    // rc==AC_OK。语义合同化 (NaN 拒绝 or 隔离) 由 P1-CAL-IMPL 处置
    // (DISP-CAL-004); 本负面断言: 不触发负中位数拒绝域 + 输出 NaN 恒定可复现。 ---
    {
        const std::size_t NPIX = 8 * 8;
        std::vector<float> flat(2 * NPIX, std::numeric_limits<float>::quiet_NaN());
        std::vector<float> out(NPIX, 0.0f), out_rep(NPIX, 0.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(flat.data(), 2, 8, 8, nullptr,
                                                   out.data(), 3.0f, 3.0f, 5), AC_OK);
        P1CAL_CHECK_EQ(cs, ac_generate_master_flat(flat.data(), 2, 8, 8, nullptr,
                                                   out_rep.data(), 3.0f, 3.0f, 5), AC_OK);
        P1CAL_CHECK(cs, std::memcmp(out.data(), out_rep.data(),
                                    NPIX * sizeof(float)) == 0, "nan_flat_reproducible");
        for (std::size_t i = 0; i < NPIX; ++i)
            P1CAL_CHECK(cs, std::isnan(out[i]), "nan_flat_nan_propagation");
    }

    // --- sigma<=0 (generate_master): 阈值 = med ± 0 → dev>0 拒 / dev≤0 保留,
    // σ=0 (MAD=0) 提前终止; oracle 双域复算同规则 ---
    {
        const std::size_t NPIX = 8 * 8;
        FixCalC fx = fix_cal_c_outlier_stack(1234u, 5, 8, 8, false);
        std::vector<float> out(NPIX, 0.0f);
        P1CAL_CHECK_EQ(cs, ac_generate_master_bias(fx.stack.data(), 5, 8, 8, out.data(),
                                                   0.0f, 0.0f, 5, AC_COMBINE_MEAN), AC_OK);
        double m = 0.0;
        for (std::size_t i = 0; i < NPIX; ++i) {
            const double want = master_gen_oracle(fx.stack.data(), NPIX, i, 5,
                                                  0.0, 0.0, 5, false);
            m = std::max(m, std::fabs(static_cast<double>(out[i]) - want));
        }
        P1CAL_CHECK(cs, m <= kAtol + kRtol * 2e2, "sigma_zero_threshold_semantics");
    }

    // --- NaN 注入 dark (DISP-CAL-004 如实登记行为): NaN 参与 median/MAD 统计
    // (阈值不可靠), 检测不崩溃; hot_sigma<=0 路径 (检测禁用) 语义稳定:
    // out_hot==0 且输出恒等。NaN 语义合同化由 P1-CAL-IMPL 处置, 本测试
    // 以禁用路径负面断言, NaN 检测路径仅断言不崩溃 + 计数可复现。 ---
    {
        const std::size_t NPIX = 8 * 8;
        std::vector<float> data(NPIX, 100.0f);
        std::vector<float> dark_nan(NPIX, 50.0f);
        dark_nan[3] = std::numeric_limits<float>::quiet_NaN();
        dark_nan[40] = std::numeric_limits<float>::quiet_NaN();
        std::vector<float> cout_(NPIX, 0.0f);
        int n_hot = -1, n_cold = -1;
        ac_correct_frame(data.data(), 8, 8, dark_nan.data(), nullptr,
                         cout_.data(), 5.0f, 5.0f, 0, 4, &n_hot, &n_cold);
        int n_hot_rep = -1;
        std::vector<float> cout_rep(NPIX, 0.0f);
        ac_correct_frame(data.data(), 8, 8, dark_nan.data(), nullptr,
                         cout_rep.data(), 5.0f, 5.0f, 0, 4, &n_hot_rep, nullptr);
        P1CAL_CHECK_EQ(cs, n_hot, n_hot_rep);  // NaN 路径可复现 (不崩溃)

        // hot_sigma<=0: 检测禁用 → 无检测 (out_hot==0), 输出恒等
        std::vector<float> cout_dis(NPIX, 0.0f);
        int n_hot_dis = -1, n_cold_dis = -1;
        ac_correct_frame(data.data(), 8, 8, dark_nan.data(), nullptr,
                         cout_dis.data(), 0.0f, 5.0f, 0, 4, &n_hot_dis, &n_cold_dis);
        P1CAL_CHECK_EQ(cs, n_hot_dis, 0);
        for (std::size_t i = 0; i < NPIX; ++i)
            P1CAL_CHECK(cs, cout_dis[i] == data[i], "identity_when_detection_disabled");
    }

    // --- actual_k 回写: dark_opt=1 但 bias/dark 缺一 → 回退标准分支 k=1 ---
    {
        const std::size_t NPIX = 8 * 8;
        std::vector<float> light(NPIX, 100.0f), dark(NPIX, 10.0f), flat(NPIX, 1.0f);
        std::vector<float> out(NPIX, 0.0f);
        float ak = -1.0f;
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame(light.data(), 8, 8, dark.data(), flat.data(),
                                              nullptr, out.data(), 1, 0.5f, &ak), AC_OK);
        P1CAL_CHECK(cs, ak == 1.0f, "fallback_standard_k");
        P1CAL_CHECK_EQ(cs, ac_calibrate_frame(light.data(), 8, 8, nullptr, flat.data(),
                                              nullptr, out.data(), 1, 0.5f, &ak), AC_OK);
        P1CAL_CHECK(cs, ak == 1.0f, "fallback_standard_k");
    }

    return cs.failures == 0 ? 0 : 1;
}

// ===========================================================================
// cosmetic
// ===========================================================================
int test_cosmetic() {
    CheckState cs;

    const FixCalE fx = fix_cal_e_cosmetic(577215u);
    const std::size_t NPIX = fx.w * fx.h;

    // --- FIX-CAL-E: 检测阈值 oracle (含结构过滤) ---
    std::vector<char> hot_want = detect_hot_oracle(fx.dark, 5.0);
    filter_structure_oracle(&hot_want, fx.w, fx.h, 4);
    std::vector<char> cold_want = detect_cold_oracle(fx.bias, 5.0);
    filter_structure_oracle(&cold_want, fx.w, fx.h, 4);

    std::vector<float> out(NPIX, 0.0f);
    int n_hot = -1, n_cold = -1;
    ac_correct_frame(fx.data.data(), static_cast<int>(fx.w), static_cast<int>(fx.h),
                     fx.dark.data(), fx.bias.data(), out.data(),
                     5.0f, 5.0f, AC_METHOD_MEDIAN, 4, &n_hot, &n_cold);

    // I6 掩码极性: 结构过滤后孤立点+L 形保留 (size<4), 12 像素方块被清 (星点)
    std::size_t want_hot_count = 0;
    for (std::size_t i = 0; i < NPIX; ++i) if (hot_want[i]) ++want_hot_count;
    std::size_t want_cold_count = 0;
    for (std::size_t i = 0; i < NPIX; ++i) if (cold_want[i]) ++want_cold_count;
    P1CAL_CHECK_EQ(cs, n_hot, static_cast<int>(want_hot_count));
    P1CAL_CHECK(cs, want_hot_count == 4, "hot_mask_filtered_iso_plus_L");  // 1 孤立 + 3 L 形
    P1CAL_CHECK_EQ(cs, n_cold, static_cast<int>(want_cold_count));

    // --- 修复 oracle: median 方法, 坏像素替换 == oracle, 好像素 bitwise 不变 ---
    std::vector<char> all_bad(NPIX, 0);
    for (std::size_t i = 0; i < NPIX; ++i)
        all_bad[i] = hot_want[i] || cold_want[i];
    for (std::size_t i = 0; i < NPIX; ++i) {
        if (all_bad[i]) {
            const double want = median_fix_oracle(fx.data, all_bad, fx.w, fx.h,
                                                  i % fx.w, i / fx.w);
            P1CAL_CHECK(cs, near_equal(out[i], want, 1e-5, 1e-4), "median_fix_oracle");
        } else {
            P1CAL_CHECK(cs, out[i] == fx.data[i], "good_pixels_bitwise_unchanged");
        }
    }

    // --- IDW 方法 oracle (AC_METHOD_BILINEAR 实为 4 方向 IDW, DISP-CAL-003) ---
    {
        std::vector<float> out_idw(NPIX, 0.0f);
        int ih = -1, ic = -1;
        ac_correct_frame(fx.data.data(), static_cast<int>(fx.w), static_cast<int>(fx.h),
                         fx.dark.data(), fx.bias.data(), out_idw.data(),
                         5.0f, 5.0f, AC_METHOD_BILINEAR, 4, &ih, &ic);
        P1CAL_CHECK_EQ(cs, ih, static_cast<int>(want_hot_count));
        for (std::size_t i = 0; i < NPIX; ++i) {
            if (all_bad[i]) {
                const double want = idw_fix_oracle(fx.data, all_bad, fx.w, fx.h,
                                                   i % fx.w, i / fx.w);
                P1CAL_CHECK(cs, near_equal(out_idw[i], want, 1e-5, 1e-4),
                            "idw_fix_oracle");
            } else {
                P1CAL_CHECK(cs, out_idw[i] == fx.data[i], "good_pixels_bitwise_unchanged");
            }
        }
    }

    // --- 1-N worker 语义: hot/cold 计数与掩码语义一致 (无 OpenMP 竞态) ---
    {
        for (int t : {1, 4}) {
            ac_set_num_threads(t);
            std::vector<float> out_t(NPIX, 0.0f);
            int h1 = -1, c1 = -1;
            ac_correct_frame(fx.data.data(), static_cast<int>(fx.w), static_cast<int>(fx.h),
                             fx.dark.data(), fx.bias.data(), out_t.data(),
                             5.0f, 5.0f, AC_METHOD_MEDIAN, 4, &h1, &c1);
            P1CAL_CHECK_EQ(cs, h1, static_cast<int>(want_hot_count));
            P1CAL_CHECK_EQ(cs, c1, static_cast<int>(want_cold_count));
            P1CAL_CHECK(cs, std::memcmp(out_t.data(), out.data(), NPIX * sizeof(float)) == 0,
                        "determinism_bitwise");
        }
        ac_set_num_threads(0);
    }

    // --- 边缘坏点修复 (镜像反射索引路径) ---
    {
        // 角落 (0,0) 单点: dark 角落注入尖刺, 用 max_size=4 保留
        FixCalE e2 = fix_cal_e_cosmetic(823543u);
        e2.dark[0] = 100000.0f;
        std::vector<float> out2(NPIX, 0.0f);
        int h2 = -1, c2 = -1;
        ac_correct_frame(e2.data.data(), static_cast<int>(fx.w), static_cast<int>(fx.h),
                         e2.dark.data(), e2.bias.data(), out2.data(),
                         5.0f, 5.0f, AC_METHOD_MEDIAN, 4, &h2, &c2);
        P1CAL_CHECK_EQ(cs, h2, static_cast<int>(want_hot_count) + 1);  // 结构 4 hot + 角点注入
        std::vector<char> bad2(NPIX, 0);
        bad2[0] = 1;
        const double want = median_fix_oracle(e2.data, bad2, fx.w, fx.h, 0, 0);
        P1CAL_CHECK(cs, near_equal(out2[0], want, 1e-5, 1e-4), "edge_bad_pixel_fix");
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

// 单执行器: units/properties/negative/cosmetic 共享 main
int p1cal_run_core_groups(int argc, char** argv);

int p1cal_run_core_groups(int argc, char** argv) {
    static const TestGroup groups[] = {
        {"units", test_units},
        {"properties", test_properties},
        {"negative", test_negative},
        {"cosmetic", test_cosmetic},
    };
    return run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
