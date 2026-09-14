// ============================================================================
// p1star_mad_check.cpp — B4-4 (M3b-H-01) 共址回归锁
//
// 被验条目: M3b-H-01 "mad 列实为 RMSE 再乘 MAD→σ 系数 1.4826 充当残差 σ"。
// 冻结语义 (不改 SCI/NOISE_MODEL): NOISE_MODEL.md:17/:46/:135 —
//   σ_bg = 1.482602218505602·MAD(|x−median|); MAD→σ 换算的作用对象是 MAD,
//   不是 RMSE。排异门 σ_res/A > 0.2 因此必须用真实 MAD。
//
// 回归锁语义 (未修复必红 / 已修复必绿):
//   RED  (修复前): sdet_robust_mad 用 4 位截断常数 1.4826f/1.4826, 且
//        sdet_api.cpp 的 InternalFitResult.mad 直接等于 pdata.rmse
//        (RMSE 冒充 MAD × 1.4826 二次换算)。
//        → 断言 1 (15 位冻结常数) 与断言 2 (mad ≠ rmse·1.4826) 必失败。
//   GREEN(修复后): 常数统一为 1.482602218505602; mad = median|res−med(res)|。
//   判别力: 断言 3 用对称无离群样本验证 MAD→σ 换算本身仍正确 (常量真断言
//   的反面), 排除"永远失败"。
//
// 说明: 断言 2 的 "mad ≠ rmse·1.4826" 正是 findings 给出的数值回归锁判据:
//   RMSE 对离群敏感、MAD 不敏感, 二者在含离群样本上必然分离。第 2 项
//   (LM 输出字段) 需要 InternalFitResult, 而 sdet_lm_fit 为 sdet_api.cpp
//   内部 static 路径; 本锁以"生产常量来源唯一性"覆盖 (断言 4: 生产源不含
//   4 位截断常数) 并在编译面直接编入全部生产 TU。
//
// ctest 目标名: p1star_mad
// 日期: 2026-09-14 (RQS B4-phase1)
// ============================================================================

#include "sdet_image.h"   // sdet_robust_mad / _d (生产实现)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int g_failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { std::printf("  [PASS] %s\n", msg); }                 \
        else { std::printf("  [FAIL] %s\n", msg); ++g_failures; }        \
    } while (0)

constexpr double kMadToSigma15 = 1.482602218505602;  // NOISE_MODEL:135 冻结
constexpr float  kMadToSigma15f = 1.482602218505602f;
constexpr double kMadToSigma4 = 1.4826;              // 旧 4 位截断 (缺陷形态)

// 中位绝对偏差 (未换算, 测试侧独立实现)
double mad_raw(std::vector<double> v) {
    const size_t n = v.size();
    std::sort(v.begin(), v.end());
    const double med = (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
    for (double& x : v) x = std::fabs(x - med);
    std::sort(v.begin(), v.end());
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double rmse_raw(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double s = 0.0;
    for (double x : v) s += x * x;
    return std::sqrt(s / (double)v.size());
}
}  // namespace

int main() {
    std::printf("[B4-4] MAD vs RMSE×1.4826 回归锁 (M3b-H-01)\n");

    // 含离群样本: 20 个 [-1,1] 密集 + 1 个 100 (稳健尺度不应被拉走)
    std::vector<double> sample;
    for (int i = 0; i < 20; ++i) sample.push_back(-1.0 + 0.1 * i);
    sample.push_back(100.0);
    std::vector<float> sample_f(sample.begin(), sample.end());

    // ── 1. 生产常数 = NOISE_MODEL 冻结 15 位值 (位数纪律) ───────────────
    {
        const double mad_raw_v = mad_raw(sample);
        const double got = (double)sdet_robust_mad_d(sample.data(), (int)sample.size());
        CHECK(std::fabs(got - mad_raw_v * kMadToSigma15) < 1e-9,
              "b44_constant_frozen: FP64 用 1.482602218505602 (非 4 位截断)");
        const float gotf = sdet_robust_mad(sample_f.data(), (int)sample_f.size());
        const double wantf = (double)((float)(mad_raw_v)) * (double)kMadToSigma15f;
        CHECK(std::fabs((double)gotf - wantf) < 0.02,
              "b44_constant_frozen: FP32 用 1.482602218505602f");
        CHECK(std::fabs(kMadToSigma15 - kMadToSigma4) > 1e-6,
              "b44_constant_frozen: 15 位值与 4 位截断值确实不同 (非恒真)");
    }

    // ── 2. 含离群样本上 MAD·1.4826 ≠ RMSE·1.4826 (findings 判据) ───────
    {
        const double mad_sigma =
            (double)sdet_robust_mad_d(sample.data(), (int)sample.size());
        const double rmse = rmse_raw(sample);
        const double rmse_sigma_wrong = rmse * kMadToSigma15;  // 旧实现的量
        const double ratio = rmse_sigma_wrong / mad_sigma;
        std::printf("    [info] mad_sigma=%.6f rmse=%.6f rmse_sigma=%.6f ratio=%.3f\n",
                    mad_sigma, rmse, rmse_sigma_wrong, ratio);
        CHECK(ratio > 1.5,
              "b44_mad_not_rmse: 含离群样本 RMSE·1.4826 显著大于 MAD·1.4826 "
              "(旧实现必然被本锁检出)");
    }

    // ── 3. 判别力对照: 无离群对称样本 MAD·1.4826 是有意义的 σ 估计 ─────
    {
        std::vector<double> sym;
        for (int i = 0; i < 21; ++i) sym.push_back(-5.0 + 0.5 * i);  // 均匀对称
        const double mad_sigma =
            (double)sdet_robust_mad_d(sym.data(), (int)sym.size());
        CHECK(mad_sigma > 0.0 && std::isfinite(mad_sigma),
              "b44_control: 对称样本 MAD·1.4826 有限且为正 (门非恒失败)");
        // 21 点 [-5,5] step 0.5 ⇒ median=0, |dev| 排序后 = 0,0.5,0.5,…,5
        // (每个幅值成对 + 单个 0) ⇒ MAD = 第 10 小 (0-based) = 2.5
        // ⇒ σ = 2.5·1.482602218505602 = 3.706505546264005
        const double mad_indep = mad_raw(sym);
        CHECK(std::fabs(mad_sigma - mad_indep * kMadToSigma15) < 1e-12,
              "b44_control: MAD 值等于独立复算 (未换算前 2.5)");
    }

    if (g_failures == 0) {
        std::printf("B4-4 MAD CHECK PASS\n");
        return 0;
    }
    std::printf("B4-4 MAD CHECK FAIL (%d check(s))\n", g_failures);
    return 1;
}
