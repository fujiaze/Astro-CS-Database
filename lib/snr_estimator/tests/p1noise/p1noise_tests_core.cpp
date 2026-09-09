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
// gain_variance (lib/snr_estimator/cpp/src/noise_model.cpp, 本测试面独立
// 编译 astrocs_p1_noise_prod); 期望值一律由 p1noise_oracle.hpp 独立 oracle
// 推导, 绝不调用被测函数生成。
#include <algorithm>
#include <cmath>
#include <cstdio>
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

inline BuildResult build_f64(const std::vector<double>& data, int w, int h,
                             const std::vector<float>* mask,
                             const std::vector<double>* sx,
                             const std::vector<double>* sy,
                             const SnrNoiseModelConfig* cfg) {
    BuildResult r;
    r.rc = snr_noise_model_v1_f64(data.data(), h, w,
                                  mask ? mask->data() : nullptr,
                                  sx ? sx->data() : nullptr,
                                  sy ? sy->data() : nullptr,
                                  sx ? static_cast<int>(sx->size()) : 0,
                                  cfg, &r.model);
    return r;
}

inline BuildResult build_f32(const std::vector<float>& data, int w, int h,
                             const std::vector<float>* mask,
                             const SnrNoiseModelConfig* cfg) {
    BuildResult r;
    r.rc = snr_noise_model_v1(data.data(), h, w,
                              mask ? mask->data() : nullptr,
                              nullptr, nullptr, 0, cfg, &r.model);
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
                                      nullptr, nullptr, nullptr, 0,
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
                                  nullptr, nullptr, nullptr, 0, &cfg, &r.model);
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
                                  nullptr, nullptr, nullptr, 0, nullptr, &r.model);
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
                                   nullptr, nullptr, nullptr, 0, nullptr, &r1.model);
    BuildResult r2;
    r2.rc = snr_noise_model_v1_f64(fx.data.data(), fx.h, fx.w,
                                   nullptr, nullptr, nullptr, 0, nullptr, &r2.model);
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
                                        nullptr, nullptr, nullptr, 0,
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

// o1: FIX-NOISE-D 掩膜解耦 — 亮星 (1e4) 与暗星 (10) 同坐标: 掩膜 rmax 与
// 振幅无关 → 两帧 star 通道模型 bitwise 一致; star 通道与手工 source_mask
// 通道 bitwise 一致; 手工掩膜与 oracle 独立圆盘光栅化逐位相同。
int check_o2_mask_decouple(CheckState& cs) {
    const FixNoiseD fx = fix_noise_d_mask(20260913ull, 256, 256,
                                          128.0, 96.0, 5.0, 10.0, 6.0);
    const SnrNoiseModelConfig cfg = default_cfg();
    // star 通道: 亮星帧
    std::vector<double> sx{fx.star_x}, sy{fx.star_y};
    BuildResult rb = build_f64(fx.bright, fx.w, fx.h, nullptr, &sx, &sy, &cfg);
    P1NOISE_CHECK_EQ(cs, rb.rc, 0);
    // star 通道: 暗星帧
    BuildResult rd = build_f64(fx.dim, fx.w, fx.h, nullptr, &sx, &sy, &cfg);
    P1NOISE_CHECK_EQ(cs, rd.rc, 0);
    // 手工 source_mask 通道: 亮星帧 + oracle 光栅化掩膜
    BuildResult rm = build_f64(fx.bright, fx.w, fx.h, &fx.mask, nullptr, nullptr, &cfg);
    P1NOISE_CHECK_EQ(cs, rm.rc, 0);

    // oracle 掩膜 vs 被测 star 通道内建掩膜: 以手工通道结果一致性间接证明
    // (掩膜不同 → 样本集不同 → σ bitwise 必异)
    P1NOISE_CHECK(cs, model_bitwise_equal(rb.model, rd.model),
                  "o2_mask_channel_parity");
    P1NOISE_CHECK(cs, model_bitwise_equal(rb.model, rm.model),
                  "o2_mask_channel_parity");

    // oracle blank-sky 复算 (手工掩膜输入) bitwise 对照
    const BlankSkyOracle o = blank_sky_oracle(fx.bright, fx.w, fx.h, fx.mask, cfg);
    P1NOISE_CHECK(cs, bit_eq(rm.model.sigma_bg_global, o.sigma_global),
                  "o2_mask_channel_parity");
    P1NOISE_CHECK(cs, bit_eq(rm.model.variance_bg_global, o.variance_global),
                  "o2_mask_channel_parity");
    free_model(&rb.model);
    free_model(&rd.model);
    free_model(&rm.model);
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
    rd2.rc = snr_noise_model_v1_f64(d32.data(), 128, 128, nullptr, nullptr, nullptr, 0,
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
                                  nullptr, nullptr, nullptr, 0, &cfg, &r.model);
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
    if (check_o2_mask_decouple(cs) != 0) return 1;
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
                                                    nullptr, nullptr, 0, nullptr, &m), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), 64, 64, nullptr,
                                                    nullptr, nullptr, 0, nullptr, nullptr), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), 0, 64, nullptr,
                                                    nullptr, nullptr, 0, nullptr, &m), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), 64, 0, nullptr,
                                                    nullptr, nullptr, 0, nullptr, &m), 3);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1_f64(fx.data.data(), -1, 64, nullptr,
                                                    nullptr, nullptr, 0, nullptr, &m), 3);
        std::vector<float> f32(fx.data.size());
        for (std::size_t i = 0; i < f32.size(); ++i) f32[i] = static_cast<float>(fx.data[i]);
        P1NOISE_CHECK_EQ(cs, snr_noise_model_v1(nullptr, 64, 64, nullptr,
                                                nullptr, nullptr, 0, nullptr, &m), 3);
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
                                      nullptr, nullptr, nullptr, 0, nullptr, &r.model);
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
                                      nullptr, 0, nullptr, &r.model);
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
                                      nullptr, nullptr, 0, nullptr, &r.model);
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
                                      nullptr, nullptr, 0, nullptr, &r.model);
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
                                      nullptr, nullptr, 0, &c, &r.model);
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
    };
    return p1noise::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
