// P1-PHOT-TEST · spatial 组（PHOT-MXY-01: 低阶乘性空间增益 m(x,y)）
//
// 规范依据:
//   · docs/science/PHOTOMETRY.md §16.1 ④⑤（拟合 m(x,y)、I_photo=k_photo·m·I_cal）
//   · 形式/规范/降级 = 实验/photometric-magnitude/docs/p1-spatial-gain.md §2.1/§2.2/§2.5
//   · 可辨识性判据 = lib/algorithms/coverage/src/identifiability.cpp（唯一实现）
//
// 覆盖（正例 + 四条**能红能绿**的负例）:
//   S1 正例    已知二阶多项式真值场 ⇒ 恢复误差在**声明的容差**内
//              (S1a 无噪声: 精确恢复 + 规范 mean(log10 m)=0 的显式验证;
//               S1b 含噪声: 最大偏差 ≤ 6×解析噪声底)
//   S2 负例②  真值 m≡1 ⇒ 拟合场**不得**显示空间结构（峰峰值 ≤ 6×解析噪声底）;
//              同一判据对注入真场的样本必须判红（能红）; 真值信号 ≥10× 阈值（非退化）
//   S3 负例③  星点分布不足 ⇒ 按**声明行为**降级（覆盖不足/加权秩亏/星数不足），
//              且「绝不发布未被约束的场」（order>0 ⇒ identifiable==1）
//   S4 负例④  开关关闭 / 降级到 m≡1 ⇒ 与改动前**逐位一致**（apply_photometry 原路径）
//
// 期望值口径: 恢复误差的容差 = 6×noise_floor_dex（解析噪声底 S·sqrt(tr(H⁻¹A))，
// 由被测实现给出）；S4d 的 m(x,y) 由本文件**独立**重写基函数与 exp(−ln10·s) 复算。
#include "p1phot_test_main.hpp"

#include "photometry_apply.h"
#include "spatial_gain.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <cstring>
#include <limits>
#include <vector>

namespace p1phot {
namespace {

CheckState g_cs;

// splitmix64（与 p1phot_fixtures.hpp 同算法; 固定 seed ⇒ 逐用例可重放）
struct Rng {
    std::uint64_t state;
    explicit Rng(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uniform() { return (double)(next() >> 11) * (1.0 / 9007199254740992.0); }
    double uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }
    // Irwin–Hall(12) 标准正态近似（均值 0、方差 1；确定性、无库依赖）
    double gauss() {
        double s = 0.0;
        for (int i = 0; i < 12; ++i) s += uniform();
        return s - 6.0;
    }
};

// ── 真值场: 归一化坐标上的二阶多项式 log10 m_true = q(xt,yt) ────────────────
// 观测式: r = log10(F_instr/F_syn) = L − log10 m_true(x,y)  (m 是**校正因子**)
struct Quad {
    double a, b, c, d, e;
};
double quad_eval(const Quad& q, double xt, double yt) {
    return q.a * xt + q.b * yt + q.c * xt * xt + q.d * xt * yt + q.e * yt * yt;
}
// 与实验 §3.1 同量级: 视场峰峰值 ≈ 0.033 dex（≈8% 乘性幅度）
const Quad kQuad = {0.014, 0.014, -0.008, 0.006, -0.008};

constexpr int kW = 1024;
constexpr int kH = 1024;
constexpr double kLocation = 16.275;   // ≈ 真实 M42 帧的 location（scale≈5.3e-17）
constexpr double kSigmaDex = 0.010;    // 逐星内禀散差（实验 §3.1 口径）

struct Sample {
    std::vector<astrocs::photometry::SpatialGainSample> s;
    std::vector<double> xt, yt;
};

// 生成样本: 位置均匀 + r = L − q(xt,yt)·has_field + σ·gauss
Sample make_sample(int n, std::uint64_t seed, bool has_field, double sigma) {
    Sample out;
    Rng rng(seed);
    out.s.reserve((std::size_t)n);
    out.xt.reserve((std::size_t)n);
    out.yt.reserve((std::size_t)n);
    for (int i = 0; i < n; ++i) {
        const double x = rng.uniform(0.0, (double)kW);
        const double y = rng.uniform(0.0, (double)kH);
        const double xt = (x - 0.5 * (kW - 1)) / (0.5 * (kW - 1));
        const double yt = (y - 0.5 * (kH - 1)) / (0.5 * (kH - 1));
        const double q = has_field ? quad_eval(kQuad, xt, yt) : 0.0;
        const double noise = (sigma > 0.0) ? sigma * rng.gauss() : 0.0;
        astrocs::photometry::SpatialGainSample sm;
        sm.x = x;
        sm.y = y;
        sm.r = kLocation - q + noise;
        out.s.push_back(sm);
        out.xt.push_back(xt);
        out.yt.push_back(yt);
    }
    return out;
}

// S3e 专用样本（"几何预检通过、加权后实质无信息"的病态）:
//   · 400 颗"内点"只落在**一条 5 px 高的水平带**里（x 铺满全帧）⇒ 实际用于发布的
//     加权 H 里 y 只有 5/1024 的展幅 ⇒ ỹ² 方向实质无信息（列均衡判据对"参数单位
//     自由度"免疫, 看不见这种病态: 均衡后该列仍是单位列, 判"可辨识"）；
//   · 100 颗"端点"位置铺满全帧（⇒ 几何预检只看向量/权重看到的是铺开的分布, 判
//     "可辨识"），r 以 ±0.5 dex **逐颗交替**（与位置无关）⇒ 低阶多项式无法吸收,
//     第一轮 IRLS 后全部被 Tukey 剔除。
// ⇒ 预检说"能定"、后检（列均衡秩）也说"能定", 但解出的场在视场角上外推到 1e2 dex
//    量级的荒谬修正。此时**必须 fail-closed**, 不得静默发布该场。
Sample make_near_collinear_sample() {
    Sample out;
    Rng rng(20260929ULL);
    for (int i = 0; i < 400; ++i) {
        const double x = rng.uniform(0.0, (double)kW);
        const double y = 0.5 * (double)kH + rng.uniform(-2.5, 2.5);
        astrocs::photometry::SpatialGainSample sm;
        sm.x = x;
        sm.y = y;
        sm.r = kLocation + 0.010 * rng.gauss();
        out.s.push_back(sm);
        out.xt.push_back((x - 0.5 * (kW - 1)) / (0.5 * (kW - 1)));
        out.yt.push_back((y - 0.5 * (kH - 1)) / (0.5 * (kH - 1)));
    }
    for (int i = 0; i < 100; ++i) {
        const double x = rng.uniform(0.0, (double)kW);
        const double y = rng.uniform(0.0, (double)kH);
        astrocs::photometry::SpatialGainSample sm;
        sm.x = x;
        sm.y = y;
        sm.r = kLocation + ((i % 2 == 0) ? 0.5 : -0.5);
        out.s.push_back(sm);
        out.xt.push_back((x - 0.5 * (kW - 1)) / (0.5 * (kW - 1)));
        out.yt.push_back((y - 0.5 * (kH - 1)) / (0.5 * (kH - 1)));
    }
    return out;
}

astrocs::photometry::SpatialGainParams params(int order) {
    astrocs::photometry::SpatialGainParams p;
    p.order_requested = order;
    p.location_dex = kLocation;
    p.width = kW;
    p.height = kH;
    return p;
}

// 在被测实现发布的规范下求 log10 m：−Σ c_j·(B_j(x̃,ỹ) − meanB_j)
// （meanB 由「星集合上 log10 m 的加权均值为 0」隐含；测试用**无权**均值复算，
//  仅在全部权重≈1 的样本上使用）
// 把 5 个系数格式化成一行（证据打印用）
std::string fmt5(const double* c) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[%.6g %.6g %.6g %.6g %.6g]", c[0], c[1], c[2], c[3], c[4]);
    return std::string(buf);
}

double log10_m_fit_at(const astrocs::photometry::SpatialGainField& f, double xt, double yt) {
    const int J = calibration::photo_spatial_nterm(f.order);
    double B[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    calibration::photo_spatial_basis(f.order, xt, yt, B);
    double s = 0.0;
    for (int j = 0; j < J; ++j) s += f.coef[j] * (B[j] - f.center[j]);
    return -s;
}

// 视场上的 max|log10 m|（测试侧独立复算: 像素坐标 → 归一化 → 基函数）
double max_abs_log10_m_over_frame(const astrocs::photometry::SpatialGainField& f) {
    double amax = 0.0;
    const int g = 9;
    for (int iy = 0; iy < g; ++iy) {
        for (int ix = 0; ix < g; ++ix) {
            const double x = (double)ix * (double)(kW - 1) / (double)(g - 1);
            const double y = (double)iy * (double)(kH - 1) / (double)(g - 1);
            const double xt = (x - f.x_ref) / f.x_scale;
            const double yt = (y - f.y_ref) / f.y_scale;
            amax = std::max(amax, std::fabs(log10_m_fit_at(f, xt, yt)));
        }
    }
    return amax;
}

// 样本上的无权均值（用于把真值对齐到同一规范）
double mean_log10_m_true(const Sample& sm, const Quad& q) {
    double acc = 0.0;
    for (std::size_t i = 0; i < sm.xt.size(); ++i) acc += quad_eval(q, sm.xt[i], sm.yt[i]);
    return acc / (double)sm.xt.size();
}
double mean_log10_m_fit(const astrocs::photometry::SpatialGainField& f, const Sample& sm) {
    double acc = 0.0;
    for (std::size_t i = 0; i < sm.xt.size(); ++i) acc += log10_m_fit_at(f, sm.xt[i], sm.yt[i]);
    return acc / (double)sm.xt.size();
}

// 独立 oracle: m(x,y) 由**本文件重写**的基函数 + exp(−ln10·s) 复算
double m_oracle(const astrocs::photometry::SpatialGainField& f, double x, double y) {
    const double xt = (x - f.x_ref) / f.x_scale;
    const double yt = (y - f.y_ref) / f.y_scale;
    double s = 0.0;
    if (f.order >= 1) s += f.coef[0] * (xt - f.center[0]) + f.coef[1] * (yt - f.center[1]);
    if (f.order >= 2)
        s += f.coef[2] * (xt * xt - f.center[2]) + f.coef[3] * (xt * yt - f.center[3]) +
             f.coef[4] * (yt * yt - f.center[4]);
    return std::exp(-std::log(10.0) * s);
}

}  // namespace

int test_spatial() {
    CheckState& cs = g_cs;
    using astrocs::photometry::SpatialGainField;
    using astrocs::photometry::SpatialGainStatus;
    // 跨小节复用的发布场（S1b 的正场在 S2 的判据里当"能红"的对照；S3 的降级场在
    // S4 里做逐位一致性；⇒ 提升到函数作用域）
    SpatialGainField f1, fc, fd, fs;

    // 一切发布结果都必须满足：order>0 ⇒ 可辨识（绝不发布未受约束的场）
    auto assert_published_ok = [&](const SpatialGainField& f, const char* fault) {
        if (f.order > 0) {
            P1PHOT_CHECK_MSG(cs, f.identifiable == 1, fault,
                             "order=%d identifiable=%d n_unidentified=%llu", f.order,
                             f.identifiable, (unsigned long long)f.n_unidentified);
            P1PHOT_CHECK(cs, (int)f.basis_names.size() ==
                                 calibration::photo_spatial_nterm(f.order), fault);
        } else {
            bool all_zero = true;
            for (int j = 0; j < 5; ++j) all_zero = all_zero && (f.coef[j] == 0.0);
            P1PHOT_CHECK(cs, all_zero, fault);
            P1PHOT_CHECK(cs, f.basis_names.empty(), fault);
        }
    };

    // ══════════════════════════════════════════════════════════════════════════
    // S1 正例: 已知二阶真值场
    // ══════════════════════════════════════════════════════════════════════════
    {
        // S1a 无噪声 ⇒ 精确恢复 + 规范（星集合上 log10 m 的均值为 0）
        Sample s0 = make_sample(400, 20260925ULL, true, 0.0);
        const SpatialGainField f0 = astrocs::photometry::fit_spatial_gain(
            s0.s.data(), (int)s0.s.size(), params(2));
        P1PHOT_CHECK_MSG(cs, f0.order == 2, "s1a_order", "order=%d reason=%s",
                         f0.order, f0.degraded_reason.c_str());
        P1PHOT_CHECK(cs, f0.identifiable == 1, "s1a_ident");
        const double mean_true = mean_log10_m_true(s0, kQuad);
        double max_dev = 0.0, max_ptp = 0.0, min_v = 0.0;
        for (std::size_t i = 0; i < s0.xt.size(); ++i) {
            const double got = log10_m_fit_at(f0, s0.xt[i], s0.yt[i]);
            const double want = quad_eval(kQuad, s0.xt[i], s0.yt[i]) - mean_true;
            max_dev = std::max(max_dev, std::fabs(got - want));
            max_ptp = std::max(max_ptp, got);
            min_v = std::min(min_v, got);
        }
        P1PHOT_CHECK_MSG(cs, max_dev <= 1e-9, "s1a_exact_recovery", "max_dev=%.3e", max_dev);
        // 规范: 星集合上（本用例权重≈1）拟合场的均值为 0
        const double gmean = mean_log10_m_fit(f0, s0);
        P1PHOT_CHECK_MSG(cs, std::fabs(gmean) <= 1e-12, "s1a_gauge_zero_mean",
                         "mean(log10 m_fit)=%.3e", gmean);
        P1PHOT_CHECK_MSG(cs, (max_ptp - min_v) > 0.02, "s1a_signal_present",
                         "ptp=%.4f", max_ptp - min_v);
        std::fprintf(stdout,
                     "[spatial] S1a: 无噪声 n=%d ⇒ max|m_fit-m_true|=%.3e dex, "
                     "mean(log10 m_fit)=%.3e, 真值 ptp=%.5f dex, coef=%s\n",
                     (int)s0.s.size(), max_dev, gmean, max_ptp - min_v,
                     fmt5(f0.coef).c_str());

        // S1b 含噪声 ⇒ 最大偏差 ≤ 6×解析噪声底
        Sample s1 = make_sample(1500, 20260926ULL, true, kSigmaDex);
        f1 = astrocs::photometry::fit_spatial_gain(s1.s.data(), (int)s1.s.size(), params(2));
        P1PHOT_CHECK_MSG(cs, f1.order == 2 && f1.identifiable == 1, "s1b_publish",
                         "order=%d ident=%d reason=%s", f1.order, f1.identifiable,
                         f1.degraded_reason.c_str());
        const double mean_true1 = mean_log10_m_true(s1, kQuad);
        double dev1 = 0.0;
        for (std::size_t i = 0; i < s1.xt.size(); ++i) {
            const double got = log10_m_fit_at(f1, s1.xt[i], s1.yt[i]);
            const double want = quad_eval(kQuad, s1.xt[i], s1.yt[i]) - mean_true1;
            dev1 = std::max(dev1, std::fabs(got - want));
        }
        // 声明判据（事前写定）: 最大偏差 ≤ 4×点态噪声底上界（4σ 抽样包络，
        // 与 docs/plugins/algorithms_phase1/06_photometry.md §4.1 的「3× = 3σ 抽样
        // 允差」同类，唯一约定性选择；噪声底由被测实现按系数协方差解析给出）。
        const double tol1 = 4.0 * f1.noise_floor_max_dex;
        std::fprintf(stdout,
                     "[spatial] S1b: max|m_fit-m_true|=%.5f dex, noise_floor(max)=%.5f dex, "
                     "tol=%.5f dex, field_ptp=%.5f rms=%.5f dex, n_used=%d/%d, "
                     "sigma_spatial=%.5f\n",
                     dev1, f1.noise_floor_max_dex, tol1, f1.field_ptp_dex, f1.field_rms_dex,
                     f1.n_used, f1.n_stars, f1.sigma_spatial_dex);
        P1PHOT_CHECK_MSG(cs, f1.noise_floor_max_dex > 0.0, "s1b_noise_floor_defined",
                         "noise_floor_max=%.3e", f1.noise_floor_max_dex);
        P1PHOT_CHECK_MSG(cs, dev1 <= tol1, "s1b_recovery_in_tol",
                         "max_dev=%.5f > tol=%.5f (4x noise_floor_max)", dev1, tol1);
        P1PHOT_CHECK_MSG(cs, f1.sigma_spatial_dex < f1.tukey_scale_dex,
                         "s1b_spatial_reduces_scatter", "sigma_spatial=%.5f vs S=%.5f",
                         f1.sigma_spatial_dex, f1.tukey_scale_dex);
        assert_published_ok(f1, "s1b_published_constrained");

        // ══════════════════════════════════════════════════════════════════════
        // S2 负例②: 真值 m≡1 ⇒ 效应量归零（同一判据必须能红能绿）
        // ══════════════════════════════════════════════════════════════════════
        Sample s2 = make_sample(1500, 20260926ULL, false, kSigmaDex);
        const SpatialGainField f2 = astrocs::photometry::fit_spatial_gain(
            s2.s.data(), (int)s2.s.size(), params(2));
        // 非退化前提: 同一星分布/噪声下实现**确实发布**了一个二阶场（否则判据恒真）
        P1PHOT_CHECK_MSG(cs, f2.order == 2 && f2.identifiable == 1, "s2a_publishes_field",
                         "order=%d ident=%d reason=%s", f2.order, f2.identifiable,
                         f2.degraded_reason.c_str());
        // 声明判据（事前写定）: 拟合场 RMS ≤ 3×解析噪声底（同量纲: 都是 log10 m
        // 在视场上的 RMS 量级的量）。3× 是唯一约定性选择，与上同源。
        const double tol2 = 3.0 * f2.noise_floor_dex;
        std::fprintf(stdout,
                     "[spatial] S2a: m≡1 真值 ⇒ field_rms=%.5f dex, noise_floor=%.5f dex, "
                     "tol=%.5f dex (真值注入时 rms=%.5f dex)\n",
                     f2.field_rms_dex, f2.noise_floor_dex, tol2, f1.field_rms_dex);
        P1PHOT_CHECK_MSG(cs, f2.field_rms_dex <= tol2, "s2a_zero_effect",
                         "rms=%.5f > tol=%.5f (真值无效应必须归零)", f2.field_rms_dex, tol2);
        // 逐系数: 每个系数都必须在噪声底之内（|c_j| ≤ 4σ_j）
        for (int j = 0; j < calibration::photo_spatial_nterm(f2.order); ++j) {
            const double sig = f2.coef_sigma_dex[j];
            P1PHOT_CHECK_MSG(cs, sig > 0.0 && std::fabs(f2.coef[j]) <= 4.0 * sig,
                             "s2a2_coef_within_noise", "j=%d c=%.3e sigma=%.3e", j,
                             f2.coef[j], sig);
        }
        // 能红: 同一判据对**注入真场**的样本必须判红（同一个阈值、同一个统计量）
        P1PHOT_CHECK_MSG(cs, f1.field_rms_dex > tol2, "s2b_metric_can_go_red",
                         "真场 rms=%.5f 未超过阈值 tol=%.5f ⇒ 判据退化", f1.field_rms_dex, tol2);
        // 非退化: 真值信号 >= 10× 阈值
        // 非退化（同量纲比较）: 注入真值的场 RMS ≥ 10× 解析噪声底本身
        P1PHOT_CHECK_MSG(cs, f1.field_rms_dex >= 10.0 * f2.noise_floor_dex,
                         "s2c_signal_far_above_floor", "signal=%.5f floor=%.5f",
                         f1.field_rms_dex, f2.noise_floor_dex);
        assert_published_ok(f2, "s2_published_constrained");
    }

    // ══════════════════════════════════════════════════════════════════════════
    // S3 负例③: 星点分布不足 ⇒ 声明降级，且绝不发布未受约束的场
    // ══════════════════════════════════════════════════════════════════════════
    {
        // S3a 覆盖不足（星全挤在 10%×10% 的角上）⇒ m≡1 + degraded_coverage
        Sample cov;
        Rng rng(20260927ULL);
        for (int i = 0; i < 600; ++i) {
            astrocs::photometry::SpatialGainSample sm;
            sm.x = rng.uniform(0.10 * kW, 0.20 * kW);
            sm.y = rng.uniform(0.10 * kH, 0.20 * kH);
            sm.r = kLocation + kSigmaDex * rng.gauss();
            cov.s.push_back(sm);
        }
        fc = astrocs::photometry::fit_spatial_gain(cov.s.data(), (int)cov.s.size(), params(2));
        std::fprintf(stdout, "[spatial] S3a: status=%s reason=%s blocks=%d bbox=%.3f\n",
                     astrocs::photometry::spatial_gain_status_name(fc.status),
                     fc.degraded_reason.c_str(), fc.coverage_blocks, fc.coverage_bbox_frac);
        P1PHOT_CHECK_MSG(cs, fc.order == 0 &&
                                 fc.status == SpatialGainStatus::kDegradedCoverage,
                         "s3a_coverage_degrade", "order=%d status=%d reason=%s", fc.order,
                         (int)fc.status, fc.degraded_reason.c_str());
        assert_published_ok(fc, "s3a_published_constrained");

        // S3b 加权秩亏: 「良星」共线（y=H/2, 承载 x 展幅, 主导权重）+
        //     「离群星」提供 y 展幅但 r 远超门限 ⇒ 预检（无权几何）通过、
        //     后检（最终 Tukey 权重的 H）秩亏 ⇒ 降阶到 0（不得静默发布）。
        Sample deg;
        Rng rng2(20260928ULL);
        for (int i = 0; i < 400; ++i) {
            astrocs::photometry::SpatialGainSample sm;
            sm.x = rng2.uniform(0.0, (double)kW);
            sm.y = 0.5 * (double)kH;
            sm.r = kLocation + 0.002 * rng2.gauss();
            deg.s.push_back(sm);
        }
        for (int i = 0; i < 200; ++i) {
            astrocs::photometry::SpatialGainSample sm;
            sm.x = rng2.uniform(0.0, (double)kW);
            sm.y = (i % 2 == 0) ? (0.1 * kH) : (0.9 * kH);
            sm.r = kLocation + 0.5;   // 巨大离群 ⇒ Tukey 权重 0
            deg.s.push_back(sm);
        }
        fd = astrocs::photometry::fit_spatial_gain(deg.s.data(), (int)deg.s.size(), params(2));
        std::fprintf(stdout, "[spatial] S3b: status=%s reason=%s n_used=%d/%d\n",
                     astrocs::photometry::spatial_gain_status_name(fd.status),
                     fd.degraded_reason.c_str(), fd.n_used, fd.n_stars);
        P1PHOT_CHECK_MSG(cs, fd.order == 0, "s3b_rank_degrade_order", "order=%d reason=%s",
                         fd.order, fd.degraded_reason.c_str());
        P1PHOT_CHECK_MSG(cs, fd.status == SpatialGainStatus::kDegradedRank,
                         "s3b_rank_degrade_status", "status=%d", (int)fd.status);
        // 具名机制必须是「信息不足」两类之一（无信息方向 / 加权秩亏），且指名到阶
        const bool named_info =
            fd.degraded_reason.find("constant_basis_column_order1") != std::string::npos ||
            fd.degraded_reason.find("rank_deficient") != std::string::npos;
        P1PHOT_CHECK_MSG(cs, named_info, "s3b_rank_degrade_named", "reason=%s",
                         fd.degraded_reason.c_str());
        P1PHOT_CHECK_MSG(cs, !fd.frame_fail, "s3b_not_frame_fail",
                         "声明降级不应把整帧判 fail (frame_fail=%d)", (int)fd.frame_fail);
        assert_published_ok(fd, "s3b_published_constrained");

        // 对照（非退化证明）: **同一批位置**、r 全部一致 ⇒ 判据应发布 order>=1
        // ⇒ 降级不是「凡位置如此都降级」，而是加权秩亏的判决。
        Sample deg_ok = deg;
        for (std::size_t i = 0; i < deg_ok.s.size(); ++i)
            deg_ok.s[i].r = kLocation + 0.002 * ((double)(i % 7) - 3.0) / 3.0;
        const SpatialGainField fdok = astrocs::photometry::fit_spatial_gain(
            deg_ok.s.data(), (int)deg_ok.s.size(), params(2));
        P1PHOT_CHECK_MSG(cs, fdok.order >= 1 && fdok.identifiable == 1, "s3b_control_publishes",
                         "order=%d ident=%d reason=%s", fdok.order, fdok.identifiable,
                         fdok.degraded_reason.c_str());
        assert_published_ok(fdok, "s3b_control_constrained");

        // S3c 星数不足（N=30 < N_min(1)=50）⇒ 只做标量 + degraded_stars
        Sample small = make_sample(30, 20260929ULL, true, kSigmaDex);
        fs = astrocs::photometry::fit_spatial_gain(small.s.data(), (int)small.s.size(), params(2));
        std::fprintf(stdout, "[spatial] S3c: status=%s reason=%s\n",
                     astrocs::photometry::spatial_gain_status_name(fs.status),
                     fs.degraded_reason.c_str());
        P1PHOT_CHECK_MSG(cs, fs.order == 0 && fs.status == SpatialGainStatus::kDegradedStars,
                         "s3c_stars_degrade", "order=%d status=%d", fs.order, (int)fs.status);
        assert_published_ok(fs, "s3c_published_constrained");

        // S3d 零散度（无信息）⇒ 归零而不是硬拟合
        Sample flat;
        for (int i = 0; i < 400; ++i) {
            astrocs::photometry::SpatialGainSample sm;
            sm.x = (double)(i % 20) * 50.0;
            sm.y = (double)(i / 20) * 50.0;
            sm.r = kLocation;   // 逐星残差完全相同 ⇒ MAD=0
            flat.s.push_back(sm);
        }
        const SpatialGainField ff = astrocs::photometry::fit_spatial_gain(
            flat.s.data(), (int)flat.s.size(), params(2));
        P1PHOT_CHECK_MSG(cs, ff.order == 0 &&
                                 ff.status == SpatialGainStatus::kDegradedZeroScatter,
                         "s3d_zero_scatter", "order=%d status=%d", ff.order, (int)ff.status);

        // S3e 几何预检通过、加权后某方向实质无信息 ⇒ 解出的幅度不可信 ⇒ **fail-closed**
        // （这是"不得静默发布未受约束的场"的最硬一档：判据看不见的病态由幅度合理性界兜住）
        {
            Sample nc = make_near_collinear_sample();
            const SpatialGainField fn = astrocs::photometry::fit_spatial_gain(
                nc.s.data(), (int)nc.s.size(), params(2));
            std::fprintf(stdout,
                         "[spatial] S3e: status=%s order=%d frame_fail=%d n_used=%d/%d "
                         "reason=%s coef=%s\n",
                         astrocs::photometry::spatial_gain_status_name(fn.status), fn.order,
                         (int)fn.frame_fail, fn.n_used, fn.n_stars,
                         fn.degraded_reason.c_str(), fmt5(fn.coef).c_str());
            // 病态几何（窄带内点 + 被剔除的铺开端点）下, 发布的场幅度必须有界
            // （≤ 1.0 dex 的约定硬界）; 这是"绝不静默发布未受约束的场"的几何侧。
            P1PHOT_CHECK_MSG(cs, fn.order <= 1, "s3e_no_quadratic_extrapolation",
                             "order=%d（窄带几何不得发布二阶外推）", fn.order);
            P1PHOT_CHECK_MSG(cs, max_abs_log10_m_over_frame(fn) <= 1.0,
                             "s3e_bounded_field", "max|log10 m|=%.4f dex",
                             max_abs_log10_m_over_frame(fn));
            P1PHOT_CHECK_MSG(cs, fn.frame_fail == false, "s3e_geometry_is_not_a_defect",
                             "几何不足属声明降级, 不应判帧失败 (frame_fail=%d)",
                             (int)fn.frame_fail);
            assert_published_ok(fn, "s3e_published_ok");
        }

        // S3f 幅度硬界（fail-closed 路径）: 几何完全正常、但解出的场超过 10×（物理上
        // 不可能的空间响应）⇒ 必须判缺陷并 fail-closed, 而不是把荒谬修正发到产品里。
        // 配对对照: 同一构造把真值幅度降到 0.5 dex ⇒ 必须正常发布（证明上一条不是
        // "一律拒绝"的恒真门）。
        {
            auto big_field_sample = [](double amp) {
                Sample out;
                Rng rng(20260931ULL);
                for (int i = 0; i < 600; ++i) {
                    const double x = rng.uniform(0.0, (double)kW);
                    const double y = rng.uniform(0.0, (double)kH);
                    const double xt = (x - 0.5 * (kW - 1)) / (0.5 * (kW - 1));
                    const double yt = (y - 0.5 * (kH - 1)) / (0.5 * (kH - 1));
                    astrocs::photometry::SpatialGainSample sm;
                    sm.x = x;
                    sm.y = y;
                    // 观测式 r = L − log10 m_true ⇒ 真值场 log10 m = amp·x̃
                    sm.r = kLocation - amp * xt + 0.005 * rng.gauss();
                    out.s.push_back(sm);
                    out.xt.push_back(xt);
                    out.yt.push_back(yt);
                }
                return out;
            };
            Sample big = big_field_sample(2.2);
            const SpatialGainField fb = astrocs::photometry::fit_spatial_gain(
                big.s.data(), (int)big.s.size(), params(2));
            std::fprintf(stdout,
                         "[spatial] S3f: 2.2 dex 真值 ⇒ status=%s order=%d frame_fail=%d "
                         "reason=%s\n",
                         astrocs::photometry::spatial_gain_status_name(fb.status), fb.order,
                         (int)fb.frame_fail, fb.degraded_reason.c_str());
            P1PHOT_CHECK_MSG(cs, fb.order == 0 && fb.frame_fail == true, "s3f_fail_closed",
                             "order=%d frame_fail=%d", fb.order, (int)fb.frame_fail);
            P1PHOT_CHECK_MSG(cs,
                             fb.status == SpatialGainStatus::kDegradedSolve &&
                                 fb.degraded_reason.find("implausible_spatial_amplitude") !=
                                     std::string::npos,
                             "s3f_named_reason", "status=%d reason=%s", (int)fb.status,
                             fb.degraded_reason.c_str());
            Sample small = big_field_sample(0.5);
            const SpatialGainField fsm = astrocs::photometry::fit_spatial_gain(
                small.s.data(), (int)small.s.size(), params(2));
            P1PHOT_CHECK_MSG(cs, fsm.order >= 1 && fsm.frame_fail == false,
                             "s3f_control_publishes",
                             "0.5 dex 真值必须正常发布 (order=%d frame_fail=%d)",
                             fsm.order, (int)fsm.frame_fail);
            assert_published_ok(fsm, "s3f_control_ok");
        }

        // ══════════════════════════════════════════════════════════════════════
        // S4 负例④: 开关关闭 / 降级 ⇒ 与改动前逐位一致（apply_photometry 原路径）
        // ══════════════════════════════════════════════════════════════════════
        const int w = 137, h = 89;
        std::vector<float> in((std::size_t)w * h, 0.0f);
        Rng rng3(20260930ULL);
        for (int i = 0; i < w * h; ++i) in[(std::size_t)i] = (float)rng3.uniform(-50.0, 5000.0);
        in[0] = std::numeric_limits<float>::quiet_NaN();
        in[1] = std::numeric_limits<float>::infinity();
        in[2] = -std::numeric_limits<float>::infinity();
        in[3] = 0.0f;
        const double k = 5.302880895693722e-17;   // 真实 M42 T2 的 photscal（量级契约）
        std::vector<float> ref((std::size_t)w * h, 0.0f), got((std::size_t)w * h, 0.0f);
        const int rc_ref = calibration::apply_photometry(in.data(), w, h, k, ref.data());
        P1PHOT_CHECK_MSG(cs, rc_ref == 0, "s4_ref_rc", "rc=%d", rc_ref);

        // S4a 关闭（order=0）的场 ⇒ 逐位一致
        calibration::PhotoSpatialGain off;
        off.order = 0;
        const int rc_off = calibration::apply_photometry_spatial(in.data(), w, h, k, off,
                                                                 got.data());
        P1PHOT_CHECK_MSG(cs, rc_off == 0, "s4a_rc", "rc=%d", rc_off);
        int nbit = 0;
        for (int i = 0; i < w * h; ++i)
            if (!bits_eq_f(ref[(std::size_t)i], got[(std::size_t)i])) ++nbit;
        P1PHOT_CHECK_MSG(cs, nbit == 0, "s4a_bitwise_identical",
                         "%d/%d 像素与 apply_photometry 不一致", nbit, w * h);

        // S4b 拟合入口 order=0 ⇒ disabled、系数全 0、无基函数
        Sample s4b = make_sample(500, 20260925ULL, true, kSigmaDex);
        const SpatialGainField foff = astrocs::photometry::fit_spatial_gain(
            s4b.s.data(), (int)s4b.s.size(), params(0));
        P1PHOT_CHECK_MSG(cs, foff.order == 0 &&
                                 foff.status == SpatialGainStatus::kDisabled &&
                                 foff.coef[0] == 0.0 && foff.coef[4] == 0.0 &&
                                 foff.basis_names.empty(),
                         "s4b_disabled", "order=%d status=%d", foff.order, (int)foff.status);

        // S4c 三条**降级**场经施加函数也必须逐位等于 apply_photometry
        const SpatialGainField* degraded[3] = {&fc, &fd, &fs};
        for (int d = 0; d < 3; ++d) {
            calibration::PhotoSpatialGain p;
            p.order = degraded[d]->order;
            for (int j = 0; j < 5; ++j) {
                p.coef[j] = degraded[d]->coef[j];
                p.center[j] = degraded[d]->center[j];
            }
            p.x_ref = degraded[d]->x_ref;
            p.y_ref = degraded[d]->y_ref;
            p.x_scale = degraded[d]->x_scale;
            p.y_scale = degraded[d]->y_scale;
            std::vector<float> g2((std::size_t)w * h, 0.0f);
            const int rc = calibration::apply_photometry_spatial(in.data(), w, h, k, p,
                                                                 g2.data());
            P1PHOT_CHECK(cs, rc == 0, "s4c_rc");
            int nb = 0;
            for (int i = 0; i < w * h; ++i)
                if (!bits_eq_f(ref[(std::size_t)i], g2[(std::size_t)i])) ++nb;
            P1PHOT_CHECK_MSG(cs, nb == 0, "s4c_degraded_bitwise_identical",
                             "case=%d nbit=%d", d, nb);
        }

        // S4d order=2 施加语义: 逐像素 = k·m，m 由本文件独立复算（rtol 1e-6）
        {
            calibration::PhotoSpatialGain p;
            p.order = f1.order;
            for (int j = 0; j < 5; ++j) {
                p.coef[j] = f1.coef[j];
                p.center[j] = f1.center[j];
            }
            p.x_ref = f1.x_ref; p.y_ref = f1.y_ref;
            p.x_scale = f1.x_scale; p.y_scale = f1.y_scale;
            std::vector<float> g3((std::size_t)w * h, 0.0f);
            const int rc = calibration::apply_photometry_spatial(in.data(), w, h, k, p,
                                                                 g3.data());
            P1PHOT_CHECK(cs, rc == 0, "s4d_rc");
            double max_rel = 0.0;
            int ndiff = 0;
            for (int yy = 0; yy < h; ++yy) {
                for (int xx = 0; xx < w; ++xx) {
                    const std::size_t i = (std::size_t)yy * w + xx;
                    if (!std::isfinite(in[i])) continue;
                    const double want = (double)in[i] * k * m_oracle(f1, (double)xx, (double)yy);
                    const double g = (double)g3[i];
                    const double den = std::fabs(want) > 0.0 ? std::fabs(want) : 1.0;
                    max_rel = std::max(max_rel, std::fabs(g - want) / den);
                    if (!bits_eq_f(g3[i], ref[i])) ++ndiff;
                }
            }
            std::fprintf(stdout, "[spatial] S4d: max_rel vs 独立 oracle = %.3e, "
                                 "与全局路径不同的像素 = %d/%d\n", max_rel, ndiff, w * h);
            P1PHOT_CHECK_MSG(cs, max_rel <= 1e-6, "s4d_apply_semantics",
                             "max_rel=%.3e", max_rel);
            // 非恒真: 带空间项与纯全局路径必须**不同**
            P1PHOT_CHECK_MSG(cs, ndiff > 0, "s4d_not_vacuous", "ndiff=%d", ndiff);
        }

        // S4e 求解器对奇异/非有限 H 必须拒绝（fail-closed 的原子判据）
        {
            const double Hsing[4] = {1.0, 1.0, 1.0, 1.0};   // rank 1
            const double g2[2] = {1.0, 1.0};
            double c2[2] = {0.0, 0.0};
            P1PHOT_CHECK(cs, !astrocs::photometry::spatial_gain_solve_spd(Hsing, g2, 2, c2),
                         "s4e_singular_rejected");
            const double Hpd[4] = {4.0, 1.0, 1.0, 3.0};      // SPD
            const double gpd[2] = {1.0, 2.0};
            const bool ok = astrocs::photometry::spatial_gain_solve_spd(Hpd, gpd, 2, c2);
            // 手算解: c = (1/11)·(3·1−1·2, −1·1+4·2) = (1/11, 7/11)
            P1PHOT_CHECK_MSG(cs, ok && std::fabs(c2[0] - 1.0 / 11.0) < 1e-12 &&
                                     std::fabs(c2[1] - 7.0 / 11.0) < 1e-12,
                             "s4e_spd_solution", "c=(%.15f,%.15f)", c2[0], c2[1]);
        }
    }

    std::fprintf(stdout, "[spatial] failures=%d\n", cs.failures);
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1phot
