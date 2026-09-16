// p1noise_saturation_test.cpp — SAT-001 门：饱和过滤在默认配置下必须生效（claim SC-008）
//
// 权威: docs/science/NOISE_MODEL.md §4「饱和域」/§8 首行/§11 饱和域 oracle（claim SC-008）
//       docs/algorithms/NOISE_ESTIMATION.md §13.2/§13.4
//       docs/contracts/DATA_SEMANTICS.md §13.1（data / cfg 行）
// 被测面: 生产源零改动 lib/algorithms/noise_snr/cpp/src/noise_model.cpp
//         + 策略头 lib/algorithms/noise_snr/include/astrocs/noise/saturation_policy.h
//
// 合成帧模型（校准后 ADU，DATA_SEMANTICS §13.1）:
//   空背景 N(mu=1000, sigma=5^2) + Moffat4(beta=4, FWHM=3) 亮星 → 总量在 SAT 处硬截断
//   → 平台乘平场响应残差 (1+eps), eps~N(0, 0.01^2)：平台散布 = 0.01*SAT，对 sky 仅 0.01*mu。
//   该模型是「校准后饱和平台不是常数」的最小忠实表达（SAT-001 取证 §2）。
//
// 用法: p1noise_saturation_test [contract|selfcheck]
#include "snr_estimator.h"
#include "astrocs/noise/saturation_policy.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::printf("FAIL: %s\n", what); }
    else     { std::printf("ok  : %s\n", what); }
}

// --- 确定性 RNG (splitmix64 + Box-Muller)，与 fixture 族同源思想，全离线 ---
struct Rng {
    unsigned long long s;
    explicit Rng(unsigned long long seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    unsigned long long next() {
        s += 0x9E3779B97F4A7C15ULL;
        unsigned long long z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double u01() { return (double)(next() >> 11) * (1.0 / 9007199254740992.0); }
    double normal() {
        double u1 = u01();
        if (u1 < 1e-300) u1 = 1e-300;
        const double u2 = u01();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }
};

const double SAT = 65535.0;
const double MU = 1000.0;
const double SIG = 5.0;
const double TRUE_VAR = SIG * SIG;      // 25 ADU^2
const double FLAT_REL = 0.01;

// 合成：sky + 单颗极亮饱和星（平台半径 ~44 px @ amp=5e13, FWHM=3）
std::vector<double> synth(int h, int w, double amp, bool with_star, unsigned long long seed) {
    Rng rng(seed);
    std::vector<double> img((size_t)h * (size_t)w);
    for (size_t i = 0; i < img.size(); ++i) img[i] = MU + SIG * rng.normal();
    if (with_star) {
        const double fwhm = 3.0;
        const double alpha = fwhm / (2.0 * std::sqrt(std::pow(2.0, 0.25) - 1.0));
        const double cx = 256.0, cy = 256.0;
        const int rad = 64;
        for (int y = (int)cy - rad; y <= (int)cy + rad; ++y) {
            if (y < 0 || y >= h) continue;
            for (int x = (int)cx - rad; x <= (int)cx + rad; ++x) {
                if (x < 0 || x >= w) continue;
                const double rr2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                const double prof = amp / std::pow(1.0 + rr2 / (alpha * alpha), 4.0);
                double v = img[(size_t)y * w + x] + prof;
                if (v >= SAT) v = SAT * (1.0 + FLAT_REL * rng.normal());
                img[(size_t)y * w + x] = v;
            }
        }
    }
    return img;
}

struct RunResult {
    int rc = 0;
    unsigned n_qual = 0;
    unsigned n_rej = 0;
    double sigma = 0.0, var_global = 0.0, ivar_global = 0.0;
    unsigned degenerate = 0, spatial = 0;
    unsigned n_polluted = 0;      // ctrl_variance > 100 * TRUE_VAR
    double ctrl_var_max = 0.0;
    double mean_ivar = 0.0;       // fill 后逐像素 ivar 均值
    double var_median = 0.0, var_max = 0.0;
};

RunResult run(const std::vector<double>& img, int h, int w, double sat_level) {
    RunResult r;
    SnrNoiseModelConfig cfg;
    snr_noise_model_v1_default_config(&cfg);
    cfg.source_mask_radius_px = 2.0;    // PSF 尺度掩膜 (rmax=6 px，MASK-002 目标语义)
    cfg.mask_radius_scale = 3.0;
    cfg.saturation_level = sat_level;
    std::vector<double> sx{256.0}, sy{256.0};
    NoiseWeightModelV1 m;
    std::memset(&m, 0, sizeof(m));
    r.rc = snr_noise_model_v1_f64(img.data(), h, w, nullptr, sx.data(), sy.data(), 1, &cfg, &m);
    r.n_qual = m.n_qualified_patches;
    r.n_rej = m.n_rejected_patches;
    r.sigma = m.sigma_bg_global;
    r.var_global = m.variance_bg_global;
    r.ivar_global = m.ivar_bg_global;
    r.degenerate = m.degenerate;
    r.spatial = m.has_spatial_field;
    for (unsigned i = 0; i < m.n_control_points; ++i) {
        const double v = m.ctrl_variance[i];
        if (v > r.ctrl_var_max) r.ctrl_var_max = v;
        if (v > 100.0 * TRUE_VAR) ++r.n_polluted;
    }
    std::vector<float> var((size_t)h * (size_t)w, 0.0f), ivar((size_t)h * (size_t)w, 0.0f);
    if (snr_noise_model_v1_fill(&m, h, w, var.data(), ivar.data()) == 0) {
        double s = 0.0, vmax = 0.0;
        std::vector<double> vv(var.begin(), var.end());
        for (size_t i = 0; i < vv.size(); ++i) { s += (double)ivar[i]; if (vv[i] > vmax) vmax = vv[i]; }
        r.mean_ivar = s / (double)vv.size();
        std::vector<double> tmp = vv;
        const size_t mid = tmp.size() / 2;
        std::nth_element(tmp.begin(), tmp.begin() + (long)mid, tmp.end());
        r.var_median = tmp[mid];
        r.var_max = vmax;
    }
    snr_noise_model_v1_free(&m);
    return r;
}

int group_contract() {
    using astrocs::noise::resolve_saturation_level;
    using astrocs::noise::resolve_effective_saturation;
    using astrocs::noise::saturation_filter_state;
    using astrocs::noise::saturation_level_source;

    // --- G1 电平解析策略（SCI §4 优先级 cfg > SATURATE > DATAMAX）---
    check(std::fabs(resolve_saturation_level("65535", "") - 65535.0) < 1e-9,
          "G1a SATURATE 生效");
    check(std::fabs(resolve_saturation_level("", "50000") - 50000.0) < 1e-9,
          "G1b DATAMAX 兜底");
    check(std::fabs(resolve_saturation_level("65535", "50000") - 65535.0) < 1e-9,
          "G1c SATURATE 优先于 DATAMAX");
    check(resolve_saturation_level(nullptr, nullptr) == 0.0, "G1d 双缺 ⇒ 0 (unset)");
    check(resolve_saturation_level("", "") == 0.0, "G1e 空串 ⇒ 0 (unset)");
    check(resolve_saturation_level("abc", "12x") == 0.0, "G1f 非数值 ⇒ 0 (unset)");
    check(resolve_saturation_level("nan", "inf") == 0.0, "G1g NaN/Inf ⇒ 0 (unset)");
    check(resolve_saturation_level("-5", "0") == 0.0, "G1h 负/零 ⇒ 0 (unset)");
    check(std::fabs(resolve_effective_saturation(1000.0, "65535", "") - 1000.0) < 1e-9,
          "G1i 显式 cfg 优先于元数据");
    check(std::strcmp(saturation_filter_state(0.0), "DISABLED_NO_METADATA") == 0,
          "G1j 0 ⇒ 显式降级声明 DISABLED_NO_METADATA");
    check(std::strcmp(saturation_filter_state(65535.0), "ENABLED") == 0,
          "G1k >0 ⇒ ENABLED");
    check(std::strcmp(saturation_level_source(0.0, "65535", ""), "HEADER_SATURATE") == 0,
          "G1l 来源标签 HEADER_SATURATE");

    // --- 默认配置：0 = 未提供（unset），必须可与「已提供」区分 ---
    SnrNoiseModelConfig dcfg;
    snr_noise_model_v1_default_config(&dcfg);
    check(dcfg.saturation_level == 0.0, "G1m 默认 saturation_level == 0（未提供）");
    check(std::strcmp(saturation_filter_state(dcfg.saturation_level), "DISABLED_NO_METADATA") == 0,
          "G1n 默认状态必须显式声明为 DISABLED_NO_METADATA（不得静默）");

    // --- G2/G3 生产实现行为（512^2, patch=64^2；掩膜 rmax=6 px）---
    const int h = 512, w = 512;
    const std::vector<double> img = synth(h, w, 5e13, true, 20260917ULL);
    const RunResult on = run(img, h, w, SAT);
    const RunResult off = run(img, h, w, 0.0);

    std::printf("  [meas] on : rc=%d nq=%u sig=%.4f var_med=%.4f var_max=%.4g poll=%u\n",
                on.rc, on.n_qual, on.sigma, on.var_median, on.var_max, on.n_polluted);
    std::printf("  [meas] off: rc=%d nq=%u sig=%.4f var_med=%.4f var_max=%.4g poll=%u\n",
                off.rc, off.n_qual, off.sigma, off.var_median, off.var_max, off.n_polluted);

    // G2 正例：提供电平 ⇒ 无污染控制点且权场与真值同阶
    check(on.n_polluted == 0, "G2a level=SAT: 无污染控制点 (ctrl_var <= 100*sigma_bg^2)");
    check(on.var_median > 0.5 * TRUE_VAR && on.var_median < 2.0 * TRUE_VAR,
          "G2b level=SAT: 权场中位数方差在真值 2 倍内");
    check(on.mean_ivar > 0.5 / TRUE_VAR,
          "G2c level=SAT: 帧平均 ivar >= 0.5/真值 (帧权重未被静默丢弃)");

    // G3 负例（缺陷必须可检出 = 门不得恒真）：未提供电平 ⇒ 污染控制点 + 帧权重崩塌
    check(off.n_polluted >= 1,
          "G3a level=0: 检出污染控制点（缺陷特征化；若不再成立须重推导本门判据）");
    check(off.ctrl_var_max > 1.0e6,
          "G3b level=0: 污染控制点方差 > 1e6 ADU^2（真值 25）");
    check(off.mean_ivar > 0.0 && on.mean_ivar > 0.0 &&
              (off.mean_ivar / on.mean_ivar) < 0.01,
          "G3c level=0: 帧平均 ivar 比 < 1e-2（平面权场被击穿 = 静默失权）");
    return 0;
}

// 自检：证明 G2/G3 判据**可失败**（不是恒真/恒假）
int group_selfcheck() {
    const int h = 512, w = 512;
    const std::vector<double> pure = synth(h, w, 0.0, false, 20260918ULL);
    const RunResult p_on = run(pure, h, w, SAT);
    const RunResult p_off = run(pure, h, w, 0.0);
    std::printf("  [selfcheck] pure-sky  on: poll=%u var_med=%.4f\n", p_on.n_polluted, p_on.var_median);
    std::printf("  [selfcheck] pure-sky  off: poll=%u var_med=%.4f\n", p_off.n_polluted, p_off.var_median);
    // 无源帧上「污染控制点」判据必须为假 ⇒ G3a 不是恒真
    check(p_on.n_polluted == 0, "S1 无源帧 level=SAT: 无污染控制点（判据可假）");
    check(p_off.n_polluted == 0, "S2 无源帧 level=0: 无污染控制点（判据可假）");
    check(p_on.var_median > 0.5 * TRUE_VAR && p_on.var_median < 2.0 * TRUE_VAR,
          "S3 无源帧权场恢复真值（G2b 判据可假）");
    // 有源帧上必须为真 ⇒ G3a 不是恒假
    const std::vector<double> img = synth(h, w, 5e13, true, 20260917ULL);
    const RunResult off = run(img, h, w, 0.0);
    check(off.n_polluted >= 1, "S4 有源帧 level=0: 必检出污染（G3a 判据可假性已排除）");
    check(std::strcmp(astrocs::noise::saturation_filter_state(0.0), "DISABLED_NO_METADATA") == 0,
          "S5 降级状态串非空且非 ENABLED（G1j 可假性已排除）");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string g = (argc > 1) ? argv[1] : "contract";
    std::printf("== p1noise_saturation group=%s ==\n", g.c_str());
    if (g == "contract")      group_contract();
    else if (g == "selfcheck") group_selfcheck();
    else { std::printf("unknown group\n"); return 2; }
    std::printf("P1NOISE SATURATION %s (group=%s, checks=%d, failures=%d)\n",
                g_fail == 0 ? "PASS" : "FAIL", g.c_str(), g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
