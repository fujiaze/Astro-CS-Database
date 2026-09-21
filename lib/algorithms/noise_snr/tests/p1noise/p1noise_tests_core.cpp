// P1-NOISE-TEST · 单元/性质/oracle/负面/fill 测试组 (单执行器 core)
//
// 控制包任务: P1-NOISE-TEST (SA-P1N-T, queue 41, lock-P1-NOISE; 依赖
// P1-NOISE-DOC 闭环)。合同锚: docs/algorithms/NOISE_ESTIMATION.md §13.4
// TEST-NOISE-DESIGN-001 (P1-NOISE-DOC 冻结, 2026-09-07, wave W1) +
// ALG-NOISE-001..003 + §13.3 DISP-NOISE-001..009 现状行为断言 +
// SCI-NOISE-001..015 (NOISE_MODEL.md, FROZEN T104, 冻结容差不放宽)。
//
// 组结构 (模块 tests/{unit,properties,oracle,negative} 语义落位; fill 语义
// FIX-NOISE-G 单列组; performance 与故障注入自检为独立可执行, 见
// CMakeLists.txt):
//   units      : FIX-NOISE-A σ 5% (4 seed) + oracle bitwise 复算
//              + FIX-NOISE-B 平面系数 10% + FIX-NOISE-E gain 解析 bitwise
//   properties : I1-I6 不变量 (互倒/floor/degenerate/确定性/patch 计数/free 幂等)
//   oracle     : o1 全模型 oracle bitwise + FIX-NOISE-D 掩膜解耦
//              + DISP-NOISE-003 use_gain_model 惰性 + v1/f64 parity
//   negative   : 参数域 rc=3 / cfg=NULL 默认 / 全掩膜退化 / NaN-Inf-饱和
//              过滤 / NaN 星跳过 / fill 参数域 / floor clamp / 静默钳位现状
//   fill       : FIX-NOISE-G 常量/平面/可空输出/无空间场语义
//
// 被测面: 现状唯一生产实现 snr_noise_model_v1(+_f64)/fill/free/scale_law/
// gain_variance (lib/algorithms/noise_snr/cpp/src/noise_model.cpp, 本测试面独立
// 编译 astrocs_p1_noise_prod); 期望值一律由 p1noise_oracle.hpp 独立 oracle
// 推导, 绝不调用被测函数生成。
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "p1noise_fixtures.hpp"
#include "p1noise_oracle.hpp"
#include "p1noise_test_main.hpp"
#include "snr_estimator.h"

using namespace p1noise;

namespace {

// ---- 共享小工具 -----------------------------------------------------------

struct BuildResult {
    int rc = -999;
    NoiseWeightModelV1 model{};
};

inline void free_model(NoiseWeightModelV1* m) {
    snr_noise_model_v1_free(m);
}

// MASK-002 (claim SC-009): cfg 可空 (=模块默认配置); flux/fwhm 为可选逐星数组,
// 与 sx/sy 同序等长 (缺省 = NULL ⇒ 模块按 SCI §5a 回调规则降级)。
inline BuildResult build_f64(const std::vector<double>& data, int w, int h,
                             const std::vector<float>* mask,
                             const std::vector<double>* sx,
                             const std::vector<double>* sy,
                             const SnrNoiseModelConfig* cfg,
                             const std::vector<double>* flux = nullptr,
                             const std::vector<double>* fwhm = nullptr) {
    BuildResult r;
    r.rc = snr_noise_model_v1_f64(data.data(), h, w,
                                  mask ? mask->data() : nullptr,
                                  sx ? sx->data() : nullptr,
                                  sy ? sy->data() : nullptr,
                                  flux ? flux->data() : nullptr,
                                  fwhm ? fwhm->data() : nullptr,
                                  sx ? static_cast<int>(sx->size()) : 0,
                                  cfg, &r.model);
    return r;
}

inline BuildResult build_f32(const std::vector<float>& data, int w, int h,
                             const std::vector<float>* mask,
                             const SnrNoiseModelConfig* cfg,
                             const std::vector<double>* sx = nullptr,
                             const std::vector<double>* sy = nullptr,
                             const std::vector<double>* flux = nullptr,
                             const std::vector<double>* fwhm = nullptr) {
    BuildResult r;
    r.rc = snr_noise_model_v1(data.data(), h, w,
                              mask ? mask->data() : nullptr,
                              sx ? sx->data() : nullptr,
                              sy ? sy->data() : nullptr,
                              flux ? flux->data() : nullptr,
                              fwhm ? fwhm->data() : nullptr,
                              sx ? static_cast<int>(sx->size()) : 0,
                              cfg, &r.model);
    return r;
}

// 模型标量+数组逐位比较 (I4/掩膜解耦/惰性断言)
inline bool model_bitwise_equal(const NoiseWeightModelV1& a,
                                const NoiseWeightModelV1& b) {
    if (a.n_control_points != b.n_control_points) return false;
    if (a.n_qualified_patches != b.n_qualified_patches) return false;
    if (a.n_rejected_patches != b.n_rejected_patches) return false;
    if (a.source != b.source) return false;
    if (a.has_spatial_field != b.has_spatial_field) return false;
    if (a.degenerate != b.degenerate) return false;
    if (!bit_eq(a.sigma_bg_global, b.sigma_bg_global)) return false;
    if (!bit_eq(a.variance_bg_global, b.variance_bg_global)) return false;
    if (!bit_eq(a.ivar_bg_global, b.ivar_bg_global)) return false;
    const std::size_t n = a.n_control_points;
    if (n > 0) {
        if (std::memcmp(a.ctrl_x_px, b.ctrl_x_px, n * sizeof(double)) != 0) return false;
        if (std::memcmp(a.ctrl_y_px, b.ctrl_y_px, n * sizeof(double)) != 0) return false;
        if (std::memcmp(a.ctrl_sigma, b.ctrl_sigma, n * sizeof(double)) != 0) return false;
        if (std::memcmp(a.ctrl_variance, b.ctrl_variance, n * sizeof(double)) != 0) return false;
        if (std::memcmp(a.ctrl_ivar, b.ctrl_ivar, n * sizeof(double)) != 0) return false;
    }
    return true;
}

inline SnrNoiseModelConfig default_cfg() {
    SnrNoiseModelConfig c{};
    const int rc = snr_noise_model_v1_default_config(&c);
    (void)rc;
    return c;
}

inline SnrNoiseModelConfig default_cfg_floor(double floor_v) {
    SnrNoiseModelConfig c = default_cfg();
    c.variance_floor = floor_v;
    return c;
}

inline double dbits(double v) {  // bitwise 断言辅助: 转可比较整数表示
    double z = v;
    return z;  // 占位, 真比较用 bit_eq
}

// ---- units 组 -------------------------------------------------------------

// FIX-NOISE-A: N(0,σ²) 空背景帧 σ=5 ADU, ≥4 组独立 seed →
// sigma_bg_global 相对真值 ≤5% (SNR-004)。
int check_a1_sigma(CheckState& cs) {
    const std::uint64_t seeds[4] = {20260907ull, 20260908ull, 20260909ull, 20260910ull};
    for (int k = 0; k < 4; ++k) {
        const FixNoiseA fx = fix_noise_a_gaussian(seeds[k], 256, 256, 5.0);
        BuildResult r;
        r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                      nullptr, nullptr, nullptr, nullptr, nullptr, 0,
                                      nullptr, &r.model);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        const double rel = rel_diff(r.model.sigma_bg_global, fx.sigma_true);
        std::fprintf(stdout,
                     "[p1noise][A] seed=%llu sigma_bg=%.6f (true 5) rel=%.4f\n",
                     static_cast<unsigned long long>(seeds[k]),
                     r.model.sigma_bg_global, rel);
        P1NOISE_CHECK(cs, rel <= kSigmaRtol, "a1_sigma_rtol");
        // I5 (默认 8×8): n_qualified + n_rejected == 64
        P1NOISE_CHECK_EQ(cs, r.model.n_qualified_patches + r.model.n_rejected_patches, 64);
        free_model(&r.model);
    }
    return cs.failures == 0 ? 0 : 1;
}

// FIX-NOISE-A oracle bitwise: NumPy 等价 median/MAD sort 路径独立复算
// (rtol 1e-9 的最强形式 = bitwise, 因统计路径仅选取顺序不同、无算术差异)。
int check_a2_oracle_bitwise(CheckState& cs) {
    const FixNoiseA fx = fix_noise_a_gaussian(20260907ull, 256, 256, 5.0);
    const SnrNoiseModelConfig cfg = default_cfg();
    BuildResult r;
    r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                  nullptr, nullptr, nullptr, nullptr, nullptr, 0, &cfg, &r.model);
    P1NOISE_CHECK_EQ(cs, r.rc, 0);
    const BlankSkyOracle o = blank_sky_oracle(fx.data, fx.w, fx.h, {}, cfg);
    // 全局兜底 = 合格 patch variance median (oracle sort 路径)
    P1NOISE_CHECK(cs, bit_eq(r.model.sigma_bg_global, o.sigma_global),
                  "a2_oracle_bitwise");
    P1NOISE_CHECK(cs, bit_eq(r.model.variance_bg_global, o.variance_global),
                  "a2_oracle_bitwise");
    P1NOISE_CHECK(cs, bit_eq(r.model.ivar_bg_global, o.ivar_global),
                  "a2_oracle_bitwise");
    P1NOISE_CHECK_EQ(cs, r.model.n_control_points, o.n_qualified);
    // 每 patch σ/variance/ivar/中心 bitwise
    for (std::size_t i = 0; i < o.qualified.size(); ++i) {
        if (!o.qualified[i]) continue;
        P1NOISE_CHECK(cs, bit_eq(r.model.ctrl_sigma[i], o.ctrl_sigma[i]),
                      "a2_oracle_bitwise");
        P1NOISE_CHECK(cs, bit_eq(r.model.ctrl_variance[i], o.ctrl_variance[i]),
                      "a2_oracle_bitwise");
        P1NOISE_CHECK(cs, bit_eq(r.model.ctrl_ivar[i], o.ctrl_ivar[i]),
                      "a2_oracle_bitwise");
        P1NOISE_CHECK(cs, bit_eq(r.model.ctrl_x_px[i], o.ctrl_x[i]),
                      "a2_oracle_bitwise");
        P1NOISE_CHECK(cs, bit_eq(r.model.ctrl_y_px[i], o.ctrl_y[i]),
                      "a2_oracle_bitwise");
        if (cs.failures != 0) break;
    }
    free_model(&r.model);
    return cs.failures == 0 ? 0 : 1;
}

// FIX-NOISE-B: 平面场 var(x,y)=a+b·x+c·y (b,c 非零, 正梯度) → 拟合系数
// 10% 内复现 (SNR-006)。系数复算用独立 plane LS oracle 于被测 build 的
// ctrl_variance 数组 (fill 合法输入面)。
int check_b1_plane_ls(CheckState& cs) {
    // var ∈ [4, 4+0.02·255+0.03·255] ≈ [4, 23.6], σ ∈ [2, 4.9]
    const FixNoiseB fx = fix_noise_b_plane(20260911ull, 256, 256,
                                           4.0, 0.02, 0.03, 0.0);
    BuildResult r;
    r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                  nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, &r.model);
    P1NOISE_CHECK_EQ(cs, r.rc, 0);
    P1NOISE_CHECK_EQ(cs, r.model.n_control_points, 64);
    const PlaneOracle p = plane_ls_oracle(r.model.ctrl_x_px, r.model.ctrl_y_px,
                                          r.model.ctrl_variance,
                                          r.model.n_control_points);
    P1NOISE_CHECK(cs, p.solvable, "b1_plane_ls_rtol");
    const double rel_b = rel_diff(p.b, fx.b);
    const double rel_c = rel_diff(p.c, fx.c);
    const double rel_a = rel_diff(p.a, fx.a);
    std::fprintf(stdout,
                 "[p1noise][B] LS a=%.4f (true 4) b=%.6f (true 0.02) "
                 "c=%.6f (true 0.03) rel=%.4f/%.4f/%.4f\n",
                 p.a, p.b, p.c, rel_a, rel_b, rel_c);
    P1NOISE_CHECK(cs, rel_b <= kPlaneRtol, "b1_plane_ls_rtol");
    P1NOISE_CHECK(cs, rel_c <= kPlaneRtol, "b1_plane_ls_rtol");
    P1NOISE_CHECK(cs, rel_a <= kPlaneRtol, "b1_plane_ls_rtol");
    free_model(&r.model);
    return cs.failures == 0 ? 0 : 1;
}

// FIX-NOISE-E (解析部分): snr_noise_gain_variance 与 var_th=μ/gain+(rn/gain)²
// 解析式逐位一致 (SCI-NOISE-005; oracle 独立编码)。含 signal≤0 clamp、
// gain≤0 显式无效 0.0。
int check_e1_gain_bitwise(CheckState& cs) {
    struct Case { double s, g, rn; };
    const Case cases[] = {
        {100.0, 2.0, 5.0},
        {0.0, 2.0, 5.0},
        {-7.5, 2.0, 5.0},   // max(signal,0)
        {1234.5, 1.5, 3.25},
        {1e6, 0.5, 0.0},
        {100.0, 0.0, 5.0},  // gain=0 → 显式无效 0.0
        {100.0, -2.0, 5.0}, // gain<0 → 0.0
    };
    for (const Case& tc : cases) {
        const double got = snr_noise_gain_variance(tc.s, tc.g, tc.rn);
        const double want = gain_variance_oracle(tc.s, tc.g, tc.rn);
        P1NOISE_CHECK(cs, bit_eq(got, want), "e1_gain_bitwise");
        if (cs.failures != 0) {
            std::fprintf(stderr, "[p1noise][E] s=%g g=%g rn=%g got=%.17g want=%.17g\n",
                         tc.s, tc.g, tc.rn, got, want);
            break;
        }
    }
    return cs.failures == 0 ? 0 : 1;
}

int test_units() {
    CheckState cs;
    if (check_a1_sigma(cs) != 0) return 1;
    if (check_a2_oracle_bitwise(cs) != 0) return 1;
    if (check_b1_plane_ls(cs) != 0) return 1;
    if (check_e1_gain_bitwise(cs) != 0) return 1;
    return cs.failures == 0 ? 0 : 1;
}

// ---- properties 组 (I1-I6) ------------------------------------------------

int test_properties() {
    CheckState cs;
    const FixNoiseA fx = fix_noise_a_gaussian(20260912ull, 256, 256, 5.0);

    // I4 确定性: 同输入同 cfg 两次运行 bitwise (现状单线程; 迁移并行后复验)
    BuildResult r1;
    r1.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                   nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, &r1.model);
    BuildResult r2;
    r2.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                   nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, &r2.model);
    P1NOISE_CHECK_EQ(cs, r1.rc, 0);
    P1NOISE_CHECK_EQ(cs, r2.rc, 0);
    P1NOISE_CHECK(cs, model_bitwise_equal(r1.model, r2.model), "i4_determinism");

    // I5: n_qualified + n_rejected == 64 (8×8)
    P1NOISE_CHECK_EQ(cs, r1.model.n_qualified_patches + r1.model.n_rejected_patches, 64);
    P1NOISE_CHECK_EQ(cs, r1.model.n_control_points, r1.model.n_qualified_patches);

    // I2/I1 (build 面): ctrl_variance ≥ floor 处处; ctrl_ivar = 1/ctrl_variance
    {
        const double floor_v = default_cfg().variance_floor;
        for (std::uint32_t i = 0; i < r1.model.n_control_points; ++i) {
            P1NOISE_CHECK(cs, r1.model.ctrl_variance[i] >= floor_v, "i2_var_ge_floor");
            P1NOISE_CHECK(cs, bit_eq(r1.model.ctrl_ivar[i],
                                     1.0 / r1.model.ctrl_variance[i]),
                          "i1_ivar_reciprocal");
            if (cs.failures != 0) break;
        }
    }

    // I1/I2 (fill 面): 逐像素 ivar=1/variance 互倒 + variance≥floor。
    // fill 输出为 float32 ABI (double 域互倒后各自 cast float), 故 float
    // 输出面的最强可执行互倒形式 = 相对误差 ≤ 1e-6 (float32 表示精度
    // 3 倍余量); double 域逐位互倒已在 ctrl 面上方断言。
    {
        std::vector<float> var_f(static_cast<std::size_t>(fx.w) * fx.h, 0.0f);
        std::vector<float> ivar_f(static_cast<std::size_t>(fx.w) * fx.h, 0.0f);
        const int rc = snr_noise_model_v1_fill(&r1.model, fx.h, fx.w,
                                               var_f.data(), ivar_f.data());
        P1NOISE_CHECK_EQ(cs, rc, 0);
        const float floor_f = static_cast<float>(kVarFloor);
        for (std::size_t i = 0; i < var_f.size(); ++i) {
            P1NOISE_CHECK(cs, !(var_f[i] < floor_f), "i2_var_ge_floor");
            P1NOISE_CHECK(cs, rel_diff(static_cast<double>(ivar_f[i]),
                                       1.0 / static_cast<double>(var_f[i])) <= 1e-6,
                          "i1_ivar_reciprocal");
            if (cs.failures != 0) {
                std::fprintf(stderr, "[p1noise][I] fill I1/I2 break at i=%zu var=%.9g ivar=%.9g\n",
                             i, static_cast<double>(var_f[i]), static_cast<double>(ivar_f[i]));
                break;
            }
        }
    }

    // I3: degenerate=1 ⇒ ivar_bg_global==0.0 bitwise (FIX-NOISE-C 常量场)
    {
        const std::vector<double> cdata = fix_noise_c_const(42.0, 64, 64);
        BuildResult rc1;
        rc1.rc = snr_noise_model_v1_f64(cdata.data(), 64, 64,
                                        nullptr, nullptr, nullptr, nullptr, nullptr, 0,
                                        nullptr, &rc1.model);
        P1NOISE_CHECK_EQ(cs, rc1.rc, 1);
        P1NOISE_CHECK_EQ(cs, rc1.model.degenerate, 1);
        P1NOISE_CHECK(cs, bit_eq(rc1.model.ivar_bg_global, 0.0),
                      "i3_degenerate_zero_ivar");
        P1NOISE_CHECK(cs, bit_eq(rc1.model.variance_bg_global, 0.0),
                      "i3_degenerate_zero_ivar");
        P1NOISE_CHECK_EQ(cs, rc1.model.n_qualified_patches + rc1.model.n_rejected_patches, 64);
        P1NOISE_CHECK_EQ(cs, rc1.model.n_control_points, 0);
        free_model(&rc1.model);
    }

    // I6: free 后再 free 不崩 (幂等); free(nullptr) 安全
    // r2 (I4 确定性第二 build) 同样须显式释放 — 未 free 即丢弃指针
    // = 注册表泄漏 (DISP-NOISE-009), 测试面自身不得示范泄漏。
    free_model(&r2.model);
    free_model(&r1.model);
    free_model(&r1.model);
    snr_noise_model_v1_free(nullptr);
    P1NOISE_CHECK_EQ(cs, r1.model.n_control_points, 0);

    return cs.failures == 0 ? 0 : 1;
}

// ---- oracle 组 ------------------------------------------------------------

// o2 (MASK-002 / claim SC-009 替换旧 o2_mask_channel_parity): 掩膜半径不变量。
// 旧门断言「亮星与暗星掩膜 bitwise 一致」= 把**被证伪的**「半径与亮度解耦」
// 机器化; 新判据 = SCI §5a: 半径对 F 与 FWHM **单调不减** + 同输入可复现。
// 手工 source_mask 通道语义保持不变 (仍与 oracle 光栅化逐位一致),
// 但其与 star 通道**不再**要求 bitwise 一致 (两通道半径语义本就不同)。
int check_o2_mask_radius_monotone(CheckState& cs) {
    const FixNoiseA bg = fix_noise_a_gaussian(20260913ull, 256, 256, 5.0);
    const SnrNoiseModelConfig cfg = default_cfg();
    const std::vector<double> sx{128.0}, sy{96.0};
    const std::vector<double> fw3{3.0};
    auto p50 = [&](double flux, double fwhm) {
        std::vector<double> fl{flux}, fw{fwhm};
        BuildResult r = build_f64(bg.data, bg.w, bg.h, nullptr, &sx, &sy, &cfg, &fl, &fw);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        const double v = r.model.mask_radius_p50;
        free_model(&r.model);
        return v;
    };
    const double r_dim = p50(1.0e2, 3.0);
    const double r_mid = p50(1.0e4, 3.0);
    const double r_bright = p50(1.0e6, 3.0);
    std::fprintf(stdout, "[p1noise][o2] r50: F=1e2 %.4f  F=1e4 %.4f  F=1e6 %.4f px\n",
                 r_dim, r_mid, r_bright);
    // (1) 对 F 单调不减 (旧实现三者相同 ⇒ 本条必红)
    P1NOISE_CHECK(cs, r_bright > r_mid && r_mid > r_dim, "o2_mask_radius_monotone_F");
    // (2) 对 FWHM 单调不减
    const double r_narrow = p50(1.0e4, 2.0);
    const double r_wide = p50(1.0e4, 6.0);
    std::fprintf(stdout, "[p1noise][o2] r50: FWHM=2 %.4f  FWHM=3 %.4f  FWHM=6 %.4f px\n",
                 r_narrow, r_mid, r_wide);
    P1NOISE_CHECK(cs, r_wide > r_mid && r_mid > r_narrow, "o2_mask_radius_monotone_fwhm");
    // (3) 同 (F,FWHM) 两次运行逐位可复现 (确定性)
    {
        std::vector<double> fl{1.0e4}, fw{3.0};
        BuildResult a1 = build_f64(bg.data, bg.w, bg.h, nullptr, &sx, &sy, &cfg, &fl, &fw);
        BuildResult a2 = build_f64(bg.data, bg.w, bg.h, nullptr, &sx, &sy, &cfg, &fl, &fw);
        P1NOISE_CHECK(cs, model_bitwise_equal(a1.model, a2.model), "o2_mask_radius_reproducible");
        P1NOISE_CHECK(cs, a1.model.mask_degraded == 0u, "o2_mask_no_degrade_with_flux_fwhm");
        free_model(&a1.model);
        free_model(&a2.model);
    }
    // (4) 负例注入 / 门牙证明: 只给 flux、不给 FWHM ⇒ §5a 回落到统一 rmax,
    //     半径与 F 无关 ⇒ 严格单调判据在该通道上**必红**; 实现必须置
    //     MASK_LEGACY (bit0), 不得静默当成逐星半径消费。
    {
        std::vector<double> fl_lo{1.0e2}, fl_hi{1.0e6};
        BuildResult rl_lo = build_f64(bg.data, bg.w, bg.h, nullptr, &sx, &sy, &cfg, &fl_lo, nullptr);
        BuildResult rl_hi = build_f64(bg.data, bg.w, bg.h, nullptr, &sx, &sy, &cfg, &fl_hi, nullptr);
        P1NOISE_CHECK_EQ(cs, rl_lo.rc, 0);
        P1NOISE_CHECK_EQ(cs, rl_hi.rc, 0);
        const bool strict_monotone_holds =
            rl_hi.model.mask_radius_p50 > rl_lo.model.mask_radius_p50;
        P1NOISE_CHECK(cs, !strict_monotone_holds, "o2_mask_negative_legacy_not_monotone");
        P1NOISE_CHECK(cs, (rl_lo.model.mask_degraded & 1u) != 0u, "o2_mask_legacy_flag");
        P1NOISE_CHECK(cs, (rl_hi.model.mask_degraded & 1u) != 0u, "o2_mask_legacy_flag");
        free_model(&rl_lo.model);
        free_model(&rl_hi.model);
    }
    // (5) 手工 source_mask 通道: 与 oracle 独立光栅化逐位一致 (语义保留)
    {
        const FixNoiseD fxd = fix_noise_d_mask(20260913ull, 256, 256,
                                               128.0, 96.0, 5.0, 10.0, 6.0);
        BuildResult rm = build_f64(fxd.bright, fxd.w, fxd.h, &fxd.mask, nullptr, nullptr, &cfg);
        P1NOISE_CHECK_EQ(cs, rm.rc, 0);
        const BlankSkyOracle o = blank_sky_oracle(fxd.bright, fxd.w, fxd.h, fxd.mask, cfg);
        P1NOISE_CHECK(cs, bit_eq(rm.model.sigma_bg_global, o.sigma_global),
                      "o2_manual_channel_oracle_bitwise");
        P1NOISE_CHECK(cs, bit_eq(rm.model.variance_bg_global, o.variance_global),
                      "o2_manual_channel_oracle_bitwise");
        // 手工通道不参与逐星半径; p50 = 0 且覆盖比 > 0
        P1NOISE_CHECK(cs, bit_eq(rm.model.mask_radius_p50, 0.0), "o2_manual_channel_p50_zero");
        P1NOISE_CHECK(cs, rm.model.mask_frac > 0.0, "o2_manual_channel_frac");
        free_model(&rm.model);
    }
    return cs.failures == 0 ? 0 : 1;
}

// o2: DISP-NOISE-003 现状 — use_gain_model=1 + gain/rn 填值不改变生产输出
// (cfg gain 三字段零读取); 不得断言"已校验"。
int check_o3_gain_model_inert(CheckState& cs) {
    const FixNoiseE fx = fix_noise_e_poisson(20260914ull, 256, 256,
                                             100.0, 2.0, 5.0);
    const SnrNoiseModelConfig c0 = default_cfg();
    SnrNoiseModelConfig c1 = c0;
    c1.use_gain_model = 1;
    c1.gain_e_per_adu = fx.gain;
    c1.read_noise_e = fx.rn;
    BuildResult r0 = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c0);
    BuildResult r1 = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c1);
    P1NOISE_CHECK_EQ(cs, r0.rc, 0);
    P1NOISE_CHECK_EQ(cs, r1.rc, 0);
    P1NOISE_CHECK(cs, model_bitwise_equal(r0.model, r1.model),
                  "o3_gain_model_inert");
    free_model(&r0.model);
    free_model(&r1.model);
    return cs.failures == 0 ? 0 : 1;
}

// o3: FIX-NOISE-E 经验交叉 — 已知 gain/rn 合成帧的经验 variance_bg_global
// 对 var_th=μ/gain+(rn/gain)² 相对差 ≤5% (SNR-005)。
int check_o3b_poisson_cross(CheckState& cs) {
    const FixNoiseE fx = fix_noise_e_poisson(20260915ull, 256, 256,
                                             100.0, 2.0, 5.0);
    BuildResult r = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, nullptr);
    P1NOISE_CHECK_EQ(cs, r.rc, 0);
    const double rel = rel_diff(r.model.variance_bg_global, fx.var_th);
    std::fprintf(stdout,
                 "[p1noise][E] var_bg=%.4f var_th=%.4f rel=%.4f\n",
                 r.model.variance_bg_global, fx.var_th, rel);
    P1NOISE_CHECK(cs, rel <= kPoissonRtol, "o3_poisson_cross_rtol");
    free_model(&r.model);
    return cs.failures == 0 ? 0 : 1;
}

// o4: v1/f64 通道 parity — data 值逐像素相等 (float 化帧) 时两模板通道
// bitwise 一致。
int check_o4_f64_parity(CheckState& cs) {
    const FixNoiseA fxd = fix_noise_a_gaussian(20260916ull, 128, 128, 5.0);
    std::vector<float> f32(fxd.data.size());
    for (std::size_t i = 0; i < f32.size(); ++i) f32[i] = static_cast<float>(fxd.data[i]);
    BuildResult rf = build_f32(f32, 128, 128, nullptr, nullptr);
    P1NOISE_CHECK_EQ(cs, rf.rc, 0);
    BuildResult rd = build_f64(fxd.data, 128, 128, nullptr, nullptr, nullptr, nullptr);
    free_model(&rd.model);  // 中间帧模型立即释放 (DISP-NOISE-009 所有权)
    // f64 通道必须吃 float 化后的同值帧: 重新构造 double 帧
    std::vector<double> d32(f32.size());
    for (std::size_t i = 0; i < d32.size(); ++i) d32[i] = static_cast<double>(f32[i]);
    BuildResult rd2;
    rd2.rc = snr_noise_model_v1_f64(d32.data(), 128, 128, nullptr, nullptr, nullptr, nullptr, nullptr, 0,
                                    nullptr, &rd2.model);
    P1NOISE_CHECK_EQ(cs, rd2.rc, 0);
    P1NOISE_CHECK(cs, model_bitwise_equal(rf.model, rd2.model), "o4_f64_parity");
    free_model(&rf.model);
    free_model(&rd2.model);
    (void)rd;
    return cs.failures == 0 ? 0 : 1;
}

// o1: 全模型 oracle bitwise (FIX-NOISE-A 第二 seed + 控制点全数组)
int check_o1_full_oracle(CheckState& cs) {
    const FixNoiseA fx = fix_noise_a_gaussian(20260918ull, 200, 120, 3.0);
    const SnrNoiseModelConfig cfg = default_cfg();
    BuildResult r;
    r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                  nullptr, nullptr, nullptr, nullptr, nullptr, 0, &cfg, &r.model);
    P1NOISE_CHECK_EQ(cs, r.rc, 0);
    const BlankSkyOracle o = blank_sky_oracle(fx.data, fx.w, fx.h, {}, cfg);
    P1NOISE_CHECK(cs, bit_eq(r.model.sigma_bg_global, o.sigma_global),
                  "o1_oracle_stats_bitwise");
    P1NOISE_CHECK(cs, bit_eq(r.model.variance_bg_global, o.variance_global),
                  "o1_oracle_stats_bitwise");
    P1NOISE_CHECK(cs, bit_eq(r.model.ivar_bg_global, o.ivar_global),
                  "o1_oracle_stats_bitwise");
    P1NOISE_CHECK_EQ(cs, r.model.n_control_points, o.n_qualified);
    for (std::size_t i = 0; i < o.qualified.size(); ++i) {
        if (!o.qualified[i]) continue;
        P1NOISE_CHECK(cs, bit_eq(r.model.ctrl_sigma[i], o.ctrl_sigma[i]) &&
                              bit_eq(r.model.ctrl_variance[i], o.ctrl_variance[i]) &&
                              bit_eq(r.model.ctrl_ivar[i], o.ctrl_ivar[i]),
                      "o1_oracle_stats_bitwise");
        if (cs.failures != 0) break;
    }
    free_model(&r.model);
    return cs.failures == 0 ? 0 : 1;
}

int test_oracle() {
    CheckState cs;
    if (check_o1_full_oracle(cs) != 0) return 1;
    if (check_o2_mask_radius_monotone(cs) != 0) return 1;
    if (check_o3_gain_model_inert(cs) != 0) return 1;
    if (check_o3b_poisson_cross(cs) != 0) return 1;
    if (check_o4_f64_parity(cs) != 0) return 1;
    return cs.failures == 0 ? 0 : 1;
}

// ---- negative 组 ----------------------------------------------------------

int test_negative() {
    CheckState cs;
    const FixNoiseA fx = fix_noise_a_gaussian(20260919ull, 64, 64, 5.0);

    // n1: data/out NULL、h/w≤0 → rc=3
    {
        NoiseWeightModelV1 m{};
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(nullptr, 64, 64, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr, 0, nullptr, &m), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), 64, 64, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), 0, 64, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr, 0, nullptr, &m), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), 64, 0, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr, 0, nullptr, &m), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), -1, 64, nullptr,
                                                    nullptr, nullptr, nullptr, nullptr, 0, nullptr, &m), 3);
        std::vector<float> f32(fx.data.size());
        for (std::size_t i = 0; i < f32.size(); ++i) f32[i] = static_cast<float>(fx.data[i]);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1(nullptr, 64, 64, nullptr,
                                                nullptr, nullptr, nullptr, nullptr, 0, nullptr, &m), 3);
        (void)f32;
    }
    P1NOISE_CHECK(cs, true, "n1_null_args_rc3");

    // n2: cfg=NULL → 默认配置 rc=0 (非退化帧)
    {
        BuildResult r = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, nullptr);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        P1NOISE_CHECK_EQ(cs, r.model.n_qualified_patches + r.model.n_rejected_patches, 64);
        free_model(&r.model);
    }
    P1NOISE_CHECK(cs, true, "n2_default_cfg_ok");

    // n3: source_mask 全 1 (无 sky) → rc=1、degenerate=1、ivar_bg_global==0.0
    {
        std::vector<float> all1(static_cast<std::size_t>(fx.w) * fx.h, 1.0f);
        BuildResult r = build_f64(fx.data, fx.w, fx.h, &all1, nullptr, nullptr, nullptr);
        P1NOISE_CHECK_EQ(cs, r.rc, 1);
        P1NOISE_CHECK_EQ(cs, r.model.degenerate, 1);
        P1NOISE_CHECK(cs, bit_eq(r.model.ivar_bg_global, 0.0),
                      "n3_all_mask_degenerate");
        free_model(&r.model);
    }

    // n4: NaN/Inf 像素过滤不计入 (valid_pixel) + 饱和电平以上像素排除
    {
        const FixNoiseA base = fix_noise_a_gaussian(20260920ull, 64, 64, 5.0);
        const SnrNoiseModelConfig cfg = default_cfg();
        // NaN/Inf 注入 (8 像素, 全部落 (0,0) patch → 该 patch 拒绝, 其余 63 合格)
        std::vector<double> dn = base.data;
        const std::size_t inj[8] = {0, 1, 2, 3, 64, 65, 128, 129};
        const double vals[8] = {std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN(),
                                -std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN()};
        for (int k = 0; k < 8; ++k) dn[inj[k]] = vals[k];
        BuildResult rn_ = build_f64(dn, 64, 64, nullptr, nullptr, nullptr, &cfg);
        P1NOISE_CHECK_EQ(cs, rn_.rc, 0);
        P1NOISE_CHECK_EQ(cs, rn_.model.n_qualified_patches, 63);
        P1NOISE_CHECK_EQ(cs, rn_.model.n_rejected_patches, 1);
        // NaN 帧模型与 oracle (同过滤规则) bitwise 一致 → 证明过滤按合同发生
        const BlankSkyOracle on = blank_sky_oracle(dn, 64, 64, {}, cfg);
        P1NOISE_CHECK(cs, bit_eq(rn_.model.sigma_bg_global, on.sigma_global),
                      "n4_nonfinite_filtered");
        P1NOISE_CHECK(cs, std::isfinite(rn_.model.sigma_bg_global),
                      "n4_nonfinite_filtered");
        free_model(&rn_.model);
        // 饱和: 4 像素 ≥1e5 → (0,0) patch 样本 60 < 64 → 拒绝; 其余合格
        std::vector<double> ds = base.data;
        const std::size_t sat[4] = {0, 1, 64, 65};
        for (int k = 0; k < 4; ++k) ds[sat[k]] = 1.0e6;
        SnrNoiseModelConfig csat = cfg;
        csat.saturation_level = 1.0e5;
        BuildResult rs = build_f64(ds, 64, 64, nullptr, nullptr, nullptr, &csat);
        P1NOISE_CHECK_EQ(cs, rs.rc, 0);
        P1NOISE_CHECK_EQ(cs, rs.model.n_qualified_patches, 63);
        P1NOISE_CHECK_EQ(cs, rs.model.n_rejected_patches, 1);
        P1NOISE_CHECK(cs, rs.model.sigma_bg_global < 1.0e4,
                      "n4_nonfinite_filtered");  // 饱和像素未进统计
        free_model(&rs.model);
    }

    // n5: star 坐标 NaN → 跳过该星 (不 lround, 不产生掩膜) →
    // NaN 星帧 σ 与同帧无星 build bitwise 一致。
    // 对比帧必须同为 bright 帧: NaN 星跳过后掩膜全空, 唯一变量是
    // "星坐标 NaN"; base 帧数据本身无星注入, 与 bright 帧统计不同,
    // 用 base 对比会把数据帧差异误判为 NaN 星行为 (已修正的前任缺陷)。
    {
        const FixNoiseD fx2 = fix_noise_d_mask(20260921ull, 128, 128,
                                               64.0, 64.0, 5.0, 10.0, 6.0);
        BuildResult r_no_star = build_f64(fx2.bright, 128, 128, nullptr,
                                          nullptr, nullptr, nullptr);
        P1NOISE_CHECK_EQ(cs, r_no_star.rc, 0);
        const double nan_x = std::numeric_limits<double>::quiet_NaN();
        std::vector<double> sx{nan_x}, sy{fx2.star_y};
        BuildResult r_nan = build_f64(fx2.bright, 128, 128, nullptr, &sx, &sy, nullptr);
        P1NOISE_CHECK_EQ(cs, r_nan.rc, 0);
        P1NOISE_CHECK(cs, bit_eq(r_nan.model.sigma_bg_global,
                                 r_no_star.model.sigma_bg_global),
                      "n5_nan_star_skipped");
        P1NOISE_CHECK(cs, model_bitwise_equal(r_nan.model, r_no_star.model),
                      "n5_nan_star_skipped");
        free_model(&r_no_star.model);
        free_model(&r_nan.model);
    }

    // n6: fill 参数域 — model NULL / h·w≤0 / 双 NULL 输出 → rc=3
    {
        BuildResult r = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, nullptr);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        std::vector<float> v(fx.data.size(), 0.0f), iv(fx.data.size(), 0.0f);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(nullptr, fx.h, fx.w,
                                                     v.data(), iv.data()), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, 0, fx.w,
                                                     v.data(), iv.data()), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, fx.h, -1,
                                                     v.data(), iv.data()), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, fx.h, fx.w,
                                                     nullptr, nullptr), 3);
        free_model(&r.model);
    }
    P1NOISE_CHECK(cs, true, "n6_fill_null_rc3");

    // n7: FIX-NOISE-B 负梯度 — 帧内右上区 LS 预测 <0 → variance==floor
    // (1e-12) 且 ivar==1e12 逐位 (clamp 行为, 合同 FIX-NOISE-B 逐位断言)
    {
        // 参数设计 (触发 clamp 的几何约束): 全部 8×8 patch 中心平面值
        // 必须 >0 (控制点不被生成下限污染), 而 fill 在图像角落外推 <0:
        //   plane(x,y) = 1 - 0.0024x - 0.0060y
        //   plane(112,112) = 0.0592 > 0 (128 帧 patch 中心最大, 8×8 → 16px)
        //   plane(127,127) = -0.0668 < 0 (fill 角落预测为负 → clamp)
        // 前任参数 (256 帧, b+c=-0.005) 使 (x+y)/2 > 1/0.005 的右上大半
        // patch 生成侧即被 1e-6 下限钉零, LS 平面被零簇拉平 → fill 预测
        // 全场 ≥0, n_clamped==0 (已修正)。
        const FixNoiseB fneg = fix_noise_b_plane(20260922ull, 128, 128,
                                                 1.0, -0.0024, -0.0060, 1.0e-6);
        BuildResult r;
        r.rc = snr_noise_model_v1_f64(fneg.data.data(), fneg.h, fneg.w,
                                      nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, &r.model);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        P1NOISE_CHECK_EQ(cs, r.model.degenerate, 0);
        std::vector<float> vf(static_cast<std::size_t>(fneg.w) * fneg.h, 0.0f);
        std::vector<float> ivf(static_cast<std::size_t>(fneg.w) * fneg.h, 0.0f);
        const int rc = snr_noise_model_v1_fill(&r.model, fneg.h, fneg.w,
                                               vf.data(), ivf.data());
        P1NOISE_CHECK_EQ(cs, rc, 0);
        const float floor_f = static_cast<float>(kVarFloor);
        const float ifloor_f = static_cast<float>(1.0 / kVarFloor);
        std::size_t n_clamped = 0;
        for (std::size_t i = 0; i < vf.size(); ++i) {
            P1NOISE_CHECK(cs, !(vf[i] < floor_f), "i2_var_ge_floor");
            if (vf[i] == floor_f) ++n_clamped;
            if (cs.failures != 0) break;
        }
        // 存在负预测像素 → clamp 逐位断言 (variance==floor, ivar==1e12)
        P1NOISE_CHECK(cs, n_clamped > 0, "n7_floor_clamp_bitwise");
        std::fprintf(stdout, "[p1noise][B-] clamped pixels: %zu / %zu\n",
                     n_clamped, vf.size());
        if (n_clamped > 0) {
            for (std::size_t i = 0; i < vf.size(); ++i) {
                if (vf[i] == floor_f) {
                    P1NOISE_CHECK(cs, ivf[i] == ifloor_f, "n7_floor_clamp_bitwise");
                    break;
                }
            }
        }
        free_model(&r.model);
    }

    // n8: DISP-NOISE-007 现状 — 参数下限静默钳位, rc 仍 0 (登记不修):
    // patch_grid=1 → 钳 2 (2×2=4 patch); cosmic_clip=0.5 → 钳 1.0。
    {
        SnrNoiseModelConfig c = default_cfg();
        c.patch_grid_x = 1;
        c.patch_grid_y = 1;
        c.cosmic_clip_sigma = 0.5;
        BuildResult r = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        P1NOISE_CHECK_EQ(cs, r.model.n_qualified_patches + r.model.n_rejected_patches, 4);
        free_model(&r.model);
    }
    P1NOISE_CHECK(cs, true, "n8_silent_clamp");

    // n10 (MASK-002 / claim SC-009): ABI 头部 fail-closed。
    // 无头部 (全零 = 未版本化旧调用方) / struct_size 错 / abi_version 错
    // 一律 rc = SNR_ABI_MISMATCH(-9); 模型头部被篡改 ⇒ fill 亦 -9。
    {
        SnrNoiseModelConfig c_ok = default_cfg();
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_abi_check_config(&c_ok), 0);
        BuildResult r_ok = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c_ok);
        P1NOISE_CHECK_EQ(cs, r_ok.rc, 0);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_abi_check_model(&r_ok.model), 0);
        std::vector<float> vf((std::size_t)fx.w * fx.h, 0.0f);
        std::vector<float> ivf((std::size_t)fx.w * fx.h, 0.0f);
        // 篡改模型头部 ⇒ fill fail-closed (不静默按错布局消费)
        r_ok.model.abi_version = SNR_NOISE_MODEL_ABI_VERSION + 1u;
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r_ok.model, fx.h, fx.w,
                                                     vf.data(), ivf.data()),
                         SNR_ABI_MISMATCH);
        r_ok.model.abi_version = SNR_NOISE_MODEL_ABI_VERSION;
        r_ok.model.struct_size = 0;
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r_ok.model, fx.h, fx.w,
                                                     vf.data(), ivf.data()),
                         SNR_ABI_MISMATCH);
        free_model(&r_ok.model);

        SnrNoiseModelConfig c_zero{};   // 全零 = 无头部 (旧调用方)
        BuildResult r0 = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c_zero);
        P1NOISE_CHECK_EQ(cs, r0.rc, SNR_ABI_MISMATCH);
        SnrNoiseModelConfig c_ver = default_cfg();
        c_ver.abi_version = SNR_NOISE_CONFIG_ABI_VERSION + 1u;
        BuildResult r1 = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c_ver);
        P1NOISE_CHECK_EQ(cs, r1.rc, SNR_ABI_MISMATCH);
        SnrNoiseModelConfig c_sz = default_cfg();
        c_sz.struct_size = (uint32_t)sizeof(SnrNoiseModelConfig) - 4u;
        BuildResult r2 = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, &c_sz);
        P1NOISE_CHECK_EQ(cs, r2.rc, SNR_ABI_MISMATCH);
        // cfg=NULL 仍走模块默认配置 (rc=0), 与头部纪律不冲突
        BuildResult rn = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr, nullptr, nullptr);
        P1NOISE_CHECK_EQ(cs, rn.rc, 0);
        free_model(&rn.model);
        std::fprintf(stdout, "[p1noise][n10] ABI fail-closed: zero/ver/size → rc=%d\n",
                     (int)SNR_ABI_MISMATCH);
    }


    // n11 (FIX-405 G3-6): variance_floor 钳位 fail-open → fail-closed。
    // ① 非法 floor (NaN / 0 / 负) 一律显式拒绝 (SNR_FLOOR_UNBOUND), 不产模型;
    // ② 手工拼装 (未注册) 模型 fill ⇒ 显式拒绝, 不再静默回退 1e-12;
    // ③ 显式绑定极大 floor (1e6) 后 fill 必须**真的**用 1e6 —— 旧实现在生产
    //    fill 路径上静默回退 1e-12, 本断言在旧代码上必红 (非退化)。
    {
        // ① 非法 floor ⇒ fail-closed
        const double bad_floors[3] = {
            std::numeric_limits<double>::quiet_NaN(), 0.0, -1.0};
        for (double bf : bad_floors) {
            SnrNoiseModelConfig cb = default_cfg();
            cb.variance_floor = bf;
            BuildResult rb = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr,
                                       nullptr, &cb);
            P1NOISE_CHECK_EQ(cs, rb.rc, SNR_FLOOR_UNBOUND);
            free_model(&rb.model);
        }
        P1NOISE_CHECK(cs, true, "n11_invalid_floor_rejected");

        // ② 未注册模型 (无 floor 绑定) ⇒ fill fail-closed (禁静默 1e-12)。
        //    注意: floor 只在**空间场**分支被消费 (全局常量分支用
        //    variance_bg_global/ivar_bg_global, 不碰 floor), 故此处构造
        //    真正的 4 控制点空间场模型, 才能覆盖 floor 消费路径。
        {
            NoiseWeightModelV1 orphan;
            std::memset(&orphan, 0, sizeof(orphan));
            snr_noise_model_v1_abi_stamp_model(&orphan);
            orphan.has_spatial_field = 1;
            orphan.n_control_points = 4;
            orphan.variance_bg_global = 1.0;
            orphan.ivar_bg_global = 1.0;
            orphan.ctrl_x_px = (double*)std::malloc(4 * sizeof(double));
            orphan.ctrl_y_px = (double*)std::malloc(4 * sizeof(double));
            orphan.ctrl_sigma = (double*)std::malloc(4 * sizeof(double));
            orphan.ctrl_variance = (double*)std::malloc(4 * sizeof(double));
            orphan.ctrl_ivar = (double*)std::malloc(4 * sizeof(double));
            for (int k = 0; k < 4; ++k) {
                orphan.ctrl_x_px[k] = (double)(k % 2) * 8.0;
                orphan.ctrl_y_px[k] = (double)(k / 2) * 8.0;
                orphan.ctrl_sigma[k] = 1.0;
                orphan.ctrl_variance[k] = 1.0;
                orphan.ctrl_ivar[k] = 1.0;
            }
            std::vector<float> vf((std::size_t)fx.w * fx.h, 0.0f);
            std::vector<float> ivf((std::size_t)fx.w * fx.h, 0.0f);
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&orphan, fx.h, fx.w,
                                                         vf.data(), ivf.data()),
                             SNR_FLOOR_UNBOUND);
            // 显式绑定非法 floor 同样拒绝
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_bind_variance_floor(
                                     &orphan, 0.0), SNR_FLOOR_UNBOUND);
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_bind_variance_floor(
                                     &orphan,
                                     std::numeric_limits<double>::infinity()),
                             SNR_FLOOR_UNBOUND);
            // 显式绑定合法 floor ⇒ 放行
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_bind_variance_floor(
                                     &orphan, kVarFloor), 0);
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&orphan, fx.h, fx.w,
                                                         vf.data(), ivf.data()),
                             0);
            free_model(&orphan);
        }
        P1NOISE_CHECK(cs, true, "n11_unbound_model_fail_closed");

        // ③ 配置的 floor 必须真的生效 (旧实现静默 1e-12 ⇒ 本断言必红)
        {
            const double big_floor = 1.0e6;
            SnrNoiseModelConfig cbig = default_cfg();
            cbig.variance_floor = big_floor;
            BuildResult rbig = build_f64(fx.data, fx.w, fx.h, nullptr, nullptr,
                                         nullptr, &cbig);
            P1NOISE_CHECK_EQ(cs, rbig.rc, 0);
            // 钳位触发 ⇒ 计数 > 0 且与"被 floor 抬升的控制点数"一致
            const int64_t n_clamped =
                snr_noise_model_v1_floor_clamp_count(&rbig.model);
            P1NOISE_CHECK(cs, n_clamped > 0, "n11_floor_clamp_counted");
            int64_t expect_clamped = 0;
            for (std::uint32_t i = 0; i < rbig.model.n_control_points; ++i)
                if (rbig.model.ctrl_variance[i] == big_floor) ++expect_clamped;
            P1NOISE_CHECK(cs, n_clamped >= expect_clamped,
                          "n11_floor_clamp_count_consistent");
            std::vector<float> vf((std::size_t)fx.w * fx.h, 0.0f);
            std::vector<float> ivf((std::size_t)fx.w * fx.h, 0.0f);
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&rbig.model, fx.h, fx.w,
                                                         vf.data(), ivf.data()),
                             0);
            std::size_t n_at_floor = 0;
            for (std::size_t i = 0; i < vf.size(); ++i)
                if (vf[i] == (float)big_floor) ++n_at_floor;
            P1NOISE_CHECK(cs, n_at_floor == vf.size(),
                          "n11_configured_floor_reaches_fill");
            free_model(&rbig.model);
            // 未注册模型计数 = -1 (显式"未知", 不冒充 0)
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_floor_clamp_count(nullptr), -1);
            std::fprintf(stdout,
                         "[p1noise][n11] floor fail-closed: clamp=%lld at_floor=%zu/%zu\n",
                         (long long)n_clamped, n_at_floor, vf.size());
        }
    }

    // n9: M3-A-005 平面几何退化 (DISP-NOISE-010) —— 控制点共线/近共线时
    // 必须 has_spatial_field=0 且 fill 为全局常量场。原实现用绝对阈值
    // fabs(det)>1e-24: 精确共线 (det==0) 仍报 spat=1, fill 退化为"控制点
    // 方差的算术平均"冒充空间场 (与 variance_bg_global 的稳健中位数不是
    // 同一个量); 近共线虽 det>1e-24, 平面把方差场病态外推到真值 ±62%。
    // 判据 = 中心化控制点点云 Gram 特征值比 λlo/λhi ≥ 1/16 (κ≤4)。
    {
        const int H9 = 512, W9 = 512, G9 = 8, PH9 = H9 / G9;
        const FixNoiseA fx9 = fix_noise_a_gaussian(20260926ull, W9, H9, 5.0);
        // cells: patch 线性号 py*G9+px; 掩膜全 1 (无 sky), 仅放开这些 patch
        auto mask_of = [&](const std::vector<int>& cells) {
            std::vector<float> m(static_cast<std::size_t>(H9) * W9, 1.0f);
            for (int c : cells) {
                const int px = c % G9, py = c / G9;
                for (int y = py * PH9; y < (py + 1) * PH9; ++y)
                    for (int x = px * PH9; x < (px + 1) * PH9; ++x)
                        m[static_cast<std::size_t>(y) * W9 + x] = 0.0f;
            }
            return m;
        };
        std::vector<int> rowA, rowB, rowC;
        for (int px = 0; px < 8; ++px) rowA.push_back(4 * G9 + px);
        rowB = rowA; rowB.push_back(5 * G9 + 0);
        for (int px = 0; px < 4; ++px) { rowC.push_back(4 * G9 + px); rowC.push_back(5 * G9 + px); }
        const std::vector<int>* cell_sets[3] = {&rowA, &rowB, &rowC};
        const char* names[3] = {"A_collinear", "B_near_collinear", "C_two_rows_control"};
        const int want_n[3] = {8, 9, 8};
        const int want_spat[3] = {0, 0, 1};   // C 为几何合格对照 (λlo/λhi=0.20)
        for (int k = 0; k < 3; ++k) {
            const std::vector<float> mk = mask_of(*cell_sets[k]);
            const SnrNoiseModelConfig cfg9 = default_cfg();
            BuildResult r9 = build_f64(fx9.data, W9, H9, &mk, nullptr, nullptr, &cfg9);
            P1NOISE_CHECK_EQ(cs, r9.rc, 0);
            P1NOISE_CHECK_EQ(cs, (int)r9.model.n_qualified_patches, want_n[k]);
            P1NOISE_CHECK_EQ(cs, (int)r9.model.has_spatial_field, want_spat[k]);
            std::vector<float> v9(static_cast<std::size_t>(H9) * W9, -1.0f);
            P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r9.model, H9, W9,
                                                         v9.data(), nullptr), 0);
            double vmin = v9[0], vmax = v9[0];
            for (float v : v9) { if (v < vmin) vmin = v; if (v > vmax) vmax = v; }
            if (want_spat[k] == 0) {
                // 几何退化 ⇒ 全场常量 = variance_bg_global (禁伪空间场)。
                // out_variance 是 FLOAT32 产品 (DATA_SEMANTICS §13.2), 故按
                // float 存储精度逐位比较 (与 g1 同口径)
                const float want_v = static_cast<float>(r9.model.variance_bg_global);
                P1NOISE_CHECK(cs, vmin == vmax && vmax == (double)want_v,
                              "n9_geom_degenerate_const_fill");
            } else {
                P1NOISE_CHECK(cs, vmax > vmin, "n9_geom_ok_spatial_fill");
            }
            std::fprintf(stdout, "[p1noise][n9] %s nq=%u spat=%u field[%.4f,%.4f]\n",
                         names[k], r9.model.n_qualified_patches,
                         (unsigned)r9.model.has_spatial_field, vmin, vmax);
            free_model(&r9.model);
        }
        P1NOISE_CHECK(cs, true, "n9_plane_geometry_gate");
    }

    return cs.failures == 0 ? 0 : 1;
}

// ---- fill 组 (FIX-NOISE-G) ------------------------------------------------

int test_fill() {
    CheckState cs;

    // g1: 常量退化模型 (n_ctrl=0, has_spatial_field=0) → 全场常量
    // variance_bg_global/ivar_bg_global (degenerate 时为 0/0, 不产生伪权重)
    {
        const std::vector<double> cdata = fix_noise_c_const(42.0, 64, 64);
        BuildResult r;
        r.rc = snr_noise_model_v1_f64(cdata.data(), 64, 64, nullptr, nullptr,
                                      nullptr, nullptr, nullptr, 0, nullptr, &r.model);
        P1NOISE_CHECK_EQ(cs, r.rc, 1);
        std::vector<float> vf(64 * 64, -1.0f), ivf(64 * 64, -1.0f);
        const int rc = snr_noise_model_v1_fill(&r.model, 64, 64, vf.data(), ivf.data());
        P1NOISE_CHECK_EQ(cs, rc, 0);
        const float want_v = static_cast<float>(r.model.variance_bg_global);
        const float want_i = static_cast<float>(r.model.ivar_bg_global);
        for (std::size_t i = 0; i < vf.size(); ++i) {
            P1NOISE_CHECK(cs, vf[i] == want_v && ivf[i] == want_i, "g1_const_fill");
            if (cs.failures != 0) break;
        }
        P1NOISE_CHECK(cs, want_v == 0.0f && want_i == 0.0f, "g1_const_fill");
        free_model(&r.model);
    }

    // g2: 平面模型 (n_ctrl=64 ≥4) → 逐像素与独立 LS oracle bitwise
    // (out float32; oracle double 场 cast float 后逐位相等 — 1e-9 rtol 的
    // float 输出面最强形式, LS 公式为 ALG 冻结 var=a+b·x+c·y)
    {
        const FixNoiseB fx = fix_noise_b_plane(20260923ull, 256, 256,
                                               4.0, 0.02, 0.03, 0.0);
        BuildResult r;
        r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w, nullptr,
                                      nullptr, nullptr, nullptr, nullptr, 0, nullptr, &r.model);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        P1NOISE_CHECK_EQ(cs, r.model.has_spatial_field, 1);
        const PlaneOracle p = plane_ls_oracle(r.model.ctrl_x_px, r.model.ctrl_y_px,
                                              r.model.ctrl_variance,
                                              r.model.n_control_points);
        P1NOISE_CHECK(cs, p.solvable, "g2_plane_fill_refbitwise");
        std::vector<float> vf(static_cast<std::size_t>(fx.w) * fx.h, 0.0f);
        std::vector<float> ivf(static_cast<std::size_t>(fx.w) * fx.h, 0.0f);
        const int rc = snr_noise_model_v1_fill(&r.model, fx.h, fx.w, vf.data(), ivf.data());
        P1NOISE_CHECK_EQ(cs, rc, 0);
        for (int y = 0; y < fx.h; ++y) {
            for (int x = 0; x < fx.w; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * fx.w + x;
                const double var_o = fill_variance_oracle(p, static_cast<double>(x),
                                                          static_cast<double>(y), kVarFloor);
                P1NOISE_CHECK(cs, vf[i] == static_cast<float>(var_o),
                              "g2_plane_fill_refbitwise");
                P1NOISE_CHECK(cs, ivf[i] == static_cast<float>(1.0 / var_o),
                              "g2_plane_fill_refbitwise");
                if (cs.failures != 0) {
                    std::fprintf(stderr,
                                 "[p1noise][G] plane fill break at (%d,%d): got %.9g want %.9g\n",
                                 x, y, static_cast<double>(vf[i]), var_o);
                    break;
                }
            }
            if (cs.failures != 0) break;
        }
        free_model(&r.model);
    }

    // g3: out_variance/out_ivar 任一可 NULL (可空输出), 值与双输出一致
    {
        const FixNoiseB fx = fix_noise_b_plane(20260924ull, 64, 64,
                                               9.0, 0.01, 0.01, 0.0);
        BuildResult r;
        r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w, nullptr,
                                      nullptr, nullptr, nullptr, nullptr, 0, nullptr, &r.model);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        std::vector<float> v_both(64 * 64, 0.0f), i_both(64 * 64, 0.0f);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, 64, 64,
                                                     v_both.data(), i_both.data()), 0);
        std::vector<float> v_only(64 * 64, 0.0f), i_only(64 * 64, 0.0f);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, 64, 64,
                                                     v_only.data(), nullptr), 0);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, 64, 64,
                                                     nullptr, i_only.data()), 0);
        P1NOISE_CHECK(cs, v_only == v_both && i_only == i_both, "g3_nullable_outputs");
        free_model(&r.model);
    }

    // g4: enable_spatial_field=0 (n_ctrl≥4 但场禁用) → 全场常量兜底
    {
        const FixNoiseB fx = fix_noise_b_plane(20260925ull, 64, 64,
                                               9.0, 0.01, 0.01, 0.0);
        SnrNoiseModelConfig c = default_cfg();
        c.enable_spatial_field = 0;
        BuildResult r;
        r.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w, nullptr,
                                      nullptr, nullptr, nullptr, nullptr, 0, &c, &r.model);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        P1NOISE_CHECK_EQ(cs, r.model.has_spatial_field, 0);
        std::vector<float> vf(64 * 64, -1.0f);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, 64, 64,
                                                     vf.data(), nullptr), 0);
        const float want = static_cast<float>(r.model.variance_bg_global);
        for (std::size_t i = 0; i < vf.size(); ++i) {
            P1NOISE_CHECK(cs, vf[i] == want, "g4_nospatial_const");
            if (cs.failures != 0) break;
        }
        free_model(&r.model);
    }

    return cs.failures == 0 ? 0 : 1;
}

// ---- scale law (FIX-NOISE-F; 单列函数供 core 注册) -------------------------

int test_scale_law() {
    CheckState cs;
    const double alphas[] = {0.5, 2.0, 10.0};
    for (double alpha : alphas) {
        const FixNoiseF fx = fix_noise_f_pair(25.0);  // σ=5 → var=25
        double var = fx.variance, ivar = fx.ivar;
        snr_noise_scale_law(alpha, &var, &ivar);
        double var_o = fx.variance, ivar_o = fx.ivar;
        scale_law_oracle(alpha, &var_o, &ivar_o);
        P1NOISE_CHECK(cs, bit_eq(var, var_o), "f1_scale_law_bitwise");
        P1NOISE_CHECK(cs, bit_eq(ivar, ivar_o), "f1_scale_law_bitwise");
        // var'=α²var 逐位 (25·α² 可精确表示: 0.25/4/100)
        P1NOISE_CHECK(cs, bit_eq(var, fx.variance * alpha * alpha),
                      "f1_scale_law_bitwise");
        P1NOISE_CHECK(cs, bit_eq(ivar, fx.ivar / (alpha * alpha)),
                      "f1_scale_law_bitwise");
        // 回乘恒等: α·(1/α) 往返 — 2 的幂 α (0.5/2.0) 缩放为 2 的幂次
        // 乘除, 逐位无损 → bitwise; 非 2 幂 α (10.0) 的 1/α 与两次除法
        // 各带 IEEE 舍入, 逐位恒等在浮点下不成立 (数学事实, 非实现缺
        // 陷), 断言 IEEE 往返误差界 ≤1e-15 相对 (~4 ulp, 远紧于任何
        // 冻结科学容差, 不构成放宽)。
        double v2 = fx.variance, i2 = fx.ivar;
        snr_noise_scale_law(alpha, &v2, &i2);
        snr_noise_scale_law(1.0 / alpha, &v2, &i2);
        if (alpha == 0.5 || alpha == 2.0) {
            P1NOISE_CHECK(cs, bit_eq(v2, fx.variance), "f2_scale_roundtrip");
            P1NOISE_CHECK(cs, bit_eq(i2, fx.ivar), "f2_scale_roundtrip");
        } else {
            P1NOISE_CHECK(cs, rel_diff(v2, fx.variance) <= 1e-15,
                          "f2_scale_roundtrip");
            P1NOISE_CHECK(cs, rel_diff(i2, fx.ivar) <= 1e-15,
                          "f2_scale_roundtrip");
        }
        // 互倒不变量: var'·ivar'==1 (α=2 的幂域)
        if (alpha == 2.0) {
            P1NOISE_CHECK(cs, bit_eq(var * ivar, 1.0), "f3_scale_reciprocal");
        }
    }
    // 负面 (DISP-NOISE-005 现状): alpha=NaN → variance=NaN 直传、ivar 不变;
    // 不得断言"已校验"。alpha=0 → ivar 除零保护 (a2=0 → ivar 不变, 现状)。
    {
        double var = 25.0, ivar = 1.0 / 25.0;
        const double nan_a = std::numeric_limits<double>::quiet_NaN();
        snr_noise_scale_law(nan_a, &var, &ivar);
        P1NOISE_CHECK(cs, is_nan_bits(var), "f4_scale_nan_passthrough");
        P1NOISE_CHECK(cs, bit_eq(ivar, 1.0 / 25.0), "f4_scale_nan_passthrough");
        var = 25.0;
        snr_noise_scale_law(0.0, &var, &ivar);
        P1NOISE_CHECK(cs, bit_eq(var, 0.0), "f4_scale_nan_passthrough");
        P1NOISE_CHECK(cs, bit_eq(ivar, 1.0 / 25.0), "f4_scale_nan_passthrough");
    }
    return cs.failures == 0 ? 0 : 1;
}


// ---- mask 组: 源污染 oracle (MASK-002 / SCI-NOISE-001 §11, claim SC-009) -------
// 结构性缺口 (MASK-001 §4.1): 原 §11 只有无星纯高斯 oracle —— 掩膜在原理上
// 不影响其结果 (实测 r=0/8/60 px 输出逐位相同)。本组注入含星帧:
//   正例  : 默认逐星掩膜 ⇒ |σ̂_bg/σ_bg − 1| ≤ 2% (MASK-001 EXP-C 实测 worst 1.11%),
//           且 nq ≥ 8、N_sky ≥ 9216、无降级位标;
//   负例① : 手工欠掩膜 (r=2 px, F_max=1e6 ADU) ⇒ 必须检出 |偏差| > 2%
//           (证明 oracle 对半径错误有判别力, 不是恒绿门);
//   负例② : 256²/50 星且不给逐星 F/FWHM (§5a 回调统一 rmax=60 px) ⇒ 旧实现
//           rc=1 + 100% 零权重 (R-5 §5.14 / MASK-001 EXP-A 复现), 新实现必须
//           rc=0 且零权重 < 5% (天空预算收缩生效) 并置 MASK_LEGACY。
int test_mask() {
    CheckState cs;
    const SnrNoiseModelConfig cfg = default_cfg();
    double worst_bias = 0.0;
    const std::uint64_t seeds[3] = {20260930ull, 20260931ull, 20260932ull};
    for (int si = 0; si < 3; ++si) {
        const std::uint64_t seed = seeds[si];
        const FixNoiseH fx = fix_noise_h_starfield(seed, 512, 512, 5.0, 60,
                                                   2.0e2, 1.0e5, 3.0);
        BuildResult r = build_f64(fx.data, fx.w, fx.h, nullptr, &fx.star_x,
                                  &fx.star_y, &cfg, &fx.star_flux, &fx.star_fwhm);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        const double bias = std::fabs(r.model.sigma_bg_global / fx.sigma_bg - 1.0);
        if (bias > worst_bias) worst_bias = bias;
        std::fprintf(stdout,
                     "[p1noise][mask] seed=%llu sigma=%.4f (true 5) bias=%.4f "
                     "nq=%u r50=%.3f frac=%.4f degraded=%u\n",
                     (unsigned long long)seed, r.model.sigma_bg_global, bias,
                     r.model.n_qualified_patches, r.model.mask_radius_p50,
                     r.model.mask_frac, r.model.mask_degraded);
        P1NOISE_CHECK(cs, bias <= 0.02, "mask_source_bias_le_2pct");
        P1NOISE_CHECK(cs, r.model.n_qualified_patches >= 8, "mask_budget_patches");
        P1NOISE_CHECK(cs, (1.0 - r.model.mask_frac) * (double)(fx.w * fx.h) >= 9216.0,
                      "mask_budget_sky");
        P1NOISE_CHECK_EQ(cs, r.model.mask_degraded, 0u);
        P1NOISE_CHECK(cs, r.model.mask_radius_p50 > 0.0, "mask_radius_p50_positive");
        free_model(&r.model);
    }
    std::fprintf(stdout, "[p1noise][mask] worst |bias| = %.4f (门限 2%%)\n", worst_bias);

    // 负例①: 手工欠掩膜 (r=1 px, F=1e7 同亮度亮星) ⇒ |偏差| > 2% (门必须能红)。
    // 逐星无偏所需 r_local(F=1e7, FWHM=3) ≈ 39 px ⇒ 1 px 掩膜是**明确欠掩膜**
    // (欠掩膜偏差由 run/PROJECT-GOVERNANCE-01/MASK-002/probe_mask002 1 标定:
    //  r=1.5 px ⇒ 3.9%、r=3 px ⇒ 5.2%; 本夹具 seed 流不同, 取 r=1 px 留裕量)。
    double neg_bias = 0.0;
    for (int k = 0; k < 3; ++k) {
        const FixNoiseH fx = fix_noise_h_starfield(20260933ull + k, 512, 512, 5.0, 20,
                                                   1.0e7, 1.0e7, 3.0);
        const std::vector<float> m = fix_noise_h_disk_mask(fx, 1.0);
        BuildResult r = build_f64(fx.data, fx.w, fx.h, &m, nullptr, nullptr, &cfg);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        const double bias = std::fabs(r.model.sigma_bg_global / fx.sigma_bg - 1.0);
        std::fprintf(stdout, "[p1noise][mask-neg1] undermined r=1px seed=%d sigma=%.4f bias=%.4f\n",
                     k, r.model.sigma_bg_global, bias);
        P1NOISE_CHECK(cs, bias > 0.02, "mask_negative_undermask_detected");
        neg_bias += bias;
        free_model(&r.model);
    }
    std::fprintf(stdout, "[p1noise][mask-neg1] mean |bias| = %.4f (门限 2%%)\n",
                 neg_bias / 3.0);

    // 负例②: 256²/50 星, 无逐星 F/FWHM ⇒ 不得整帧失权 (旧实现 rc=1)
    {
        const FixNoiseH fx = fix_noise_h_starfield(20260934ull, 256, 256, 5.0, 50,
                                                   2.0e2, 1.0e5, 3.0);
        BuildResult r = build_f64(fx.data, fx.w, fx.h, nullptr, &fx.star_x,
                                  &fx.star_y, &cfg);
        P1NOISE_CHECK_EQ(cs, r.rc, 0);
        std::vector<float> vf((std::size_t)fx.w * fx.h, 0.0f);
        std::vector<float> ivf((std::size_t)fx.w * fx.h, 1.0f);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_fill(&r.model, fx.h, fx.w,
                                                     vf.data(), ivf.data()), 0);
        const double zw = zero_weight_frac(ivf);
        std::fprintf(stdout,
                     "[p1noise][mask-neg2] 256^2/50 stars legacy-rmax: rc=%d r50=%.3f "
                     "degraded=%u zero_weight=%.4f\n",
                     r.rc, r.model.mask_radius_p50, r.model.mask_degraded, zw);
        P1NOISE_CHECK(cs, zw < 0.05, "mask_legacy_no_full_frame_loss");
        P1NOISE_CHECK(cs, (r.model.mask_degraded & 1u) != 0u, "mask_legacy_flag_set");
        P1NOISE_CHECK(cs, r.model.mask_radius_p50 < 60.0, "mask_budget_shrink_applied");
        free_model(&r.model);
    }
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

// selfcheck 重入入口 (带注入环境子进程直接跑指定组; TU 外部符号)
int p1noise_run_core_groups(int argc, char** argv) {
    const p1noise::TestGroup groups[] = {
        {"units", test_units},
        {"properties", test_properties},
        {"oracle", test_oracle},
        {"negative", test_negative},
        {"scale_law", test_scale_law},
        {"fill", test_fill},
        {"mask", test_mask},
    };
    return p1noise::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
