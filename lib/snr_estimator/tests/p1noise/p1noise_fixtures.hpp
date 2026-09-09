// P1-NOISE-TEST · FIX-NOISE-A..G 合成 fixture generator
//
// 控制包任务: P1-NOISE-TEST (SA-P1N-T, queue 41, lock-P1-NOISE; 依赖
// P1-NOISE-DOC 闭环)。合同锚: docs/algorithms/NOISE_ESTIMATION.md §13.4
// TEST-NOISE-DESIGN-001 (P1-NOISE-DOC 冻结, 2026-09-07, wave W1); 矩阵行
// P1-NOISE (MOD astrocs.p1.noise-snr)。上游 SCI-NOISE-001..015
// (docs/science/NOISE_MODEL.md, FROZEN T104 2026-08-23, 不改)。
//
// 规则 (对齐 p1cal/p1cos/p1drz 先例):
//   - 全 fixture 由固定 seed + 参数确定生成, 零随机硬件依赖, 零真实数据
//     依赖 (TEST-NOISE-DESIGN-001 "全离线合成数据"), 不提交大二进制。
//   - splitmix64 统一 PRNG 约定 (与 p1cos fixture 谱系一致): 64 位状态,
//     输出 [0,1) double; Gaussian 经 Box-Muller 独立推导。
//   - fixture 值只进测试面与 oracle 的"输入"侧; 期望值一律由
//     p1noise_oracle.hpp 独立推导, 绝不经过被测函数。
//   - 被测面: 现状唯一生产实现 snr_noise_model_v1(+_f64)/fill/free/
//     scale_law/gain_variance (lib/snr_estimator/cpp/src/noise_model.cpp,
//     独立编译为本测试面链接的 astrocs_p1_noise_prod 静态目标);
//     lib/snr_estimator/ 为 P1-NOISE 迁移目标目录 (P1-NOISE-IMPL 落码后
//     本测试面直接复用, 冻结容差不变)。
#ifndef P1NOISE_FIXTURES_HPP
#define P1NOISE_FIXTURES_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace p1noise {

// 统一 PRNG: splitmix64 (fixture 谱系约定, seed 完全决定序列)
inline std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline double uniform01(std::uint64_t& state) {
    return static_cast<double>(splitmix64(state) >> 11) * (1.0 / 9007199254740992.0);
}

// Box-Muller Gaussian (oracle 侧用同 seed 同变换独立复算同帧;
// fixture 与 oracle 的输入一致由本函数单一来源保证, 期望统计量由
// oracle 对样本重算, 不由本函数"宣称"的 sigma 生成)
inline double gauss01(std::uint64_t& state) {
    double u1 = uniform01(state);
    const double u2 = uniform01(state);
    if (u1 <= 0.0) u1 = 4.656612873077393e-10;  // 2^-31 下界, 防-log(0)
    const double r = std::sqrt(-2.0 * std::log(u1));
    return r * std::cos(2.0 * 3.14159265358979323846 * u2);
}

// ---------------------------------------------------------------------------
// FIX-NOISE-A Gaussian 合成空背景帧 (ALG §13.4): N(0, sigma_true^2) 全帧,
// 无星无掩膜。sigma_true=5 ADU, ≥4 组独立 seed。期望: sigma_bg_global 对
// 真值 ≤5% (SNR-004); NumPy 等价 median/MAD 独立复算 rtol 1e-9 (SCI §11)。
// ---------------------------------------------------------------------------
struct FixNoiseA {
    int w = 0, h = 0;
    double sigma_true = 0.0;
    std::vector<double> data;  // [h*w] 行主序
};

inline FixNoiseA fix_noise_a_gaussian(std::uint64_t seed, int w, int h,
                                      double sigma_true) {
    FixNoiseA fx;
    fx.w = w;
    fx.h = h;
    fx.sigma_true = sigma_true;
    fx.data.assign(static_cast<std::size_t>(w) * h, 0.0);
    std::uint64_t st = seed;
    for (std::size_t i = 0; i < fx.data.size(); ++i) {
        fx.data[i] = sigma_true * gauss01(st);
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-NOISE-B 平面场 (ALG §13.4): var(x,y)=a+b·x+c·y (b,c 非零), 像素
// N(0, var(x,y))。两组: 正梯度 (b>0,c>0) 与负梯度 (b<0,c<0, 控制点区域
// 保持 var>0, 图像远角预测<0 → fill clamp 路径)。期望: LS 平面系数
// oracle 复算 10% 内复现 (SNR-006); 负预测像素 out_variance==floor(1e-12)
// 且 out_ivar==1e12 (逐位断言 clamp 行为)。
// ---------------------------------------------------------------------------
struct FixNoiseB {
    int w = 0, h = 0;
    double a = 0.0, b = 0.0, c = 0.0;   // var(x,y)=a+b·x+c·y 真值
    double var_floor_gen = 0.0;         // fixture 生成下限 (≥0)
    std::vector<double> data;
    std::vector<double> var_true;       // 生成用真值场 (输入侧参考)
};

inline FixNoiseB fix_noise_b_plane(std::uint64_t seed, int w, int h,
                                   double a, double b, double c,
                                   double var_floor_gen) {
    FixNoiseB fx;
    fx.w = w;
    fx.h = h;
    fx.a = a;
    fx.b = b;
    fx.c = c;
    fx.var_floor_gen = var_floor_gen;
    const std::size_t npix = static_cast<std::size_t>(w) * h;
    fx.data.resize(npix);
    fx.var_true.resize(npix);
    std::uint64_t st = seed;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * w + x;
            double v = a + b * static_cast<double>(x) + c * static_cast<double>(y);
            if (v < var_floor_gen) v = var_floor_gen;
            fx.var_true[i] = v;
            fx.data[i] = std::sqrt(v) * gauss01(st);
        }
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-NOISE-C 常量场 (ALG §13.4): 全帧常量 C → MAD=0 → 全部 patch 拒绝 →
// 全帧兜底 sigma=0 → rc=1、degenerate=1、ivar_bg_global==0.0 bitwise。
// ---------------------------------------------------------------------------
inline std::vector<double> fix_noise_c_const(double C, int w, int h) {
    return std::vector<double>(static_cast<std::size_t>(w) * h, C);
}

// ---------------------------------------------------------------------------
// FIX-NOISE-D 掩膜解耦 (ALG §13.4): 亮星 (振幅 1e4) 与暗星 (振幅 10) 同
// 坐标 → source_mask 逐位相同 (rmax 与振幅无关); star 通道与手工
// source_mask 通道结果 bitwise 一致。振幅只改注入亮度, 不改坐标与帧噪声。
// ---------------------------------------------------------------------------
struct FixNoiseD {
    int w = 0, h = 0;
    double star_x = 0.0, star_y = 0.0;
    std::vector<double> bright;   // 亮星帧 (amp=1e4)
    std::vector<double> dim;      // 暗星帧 (amp=10)
    std::vector<double> base;     // 无星基帧 (对照)
    std::vector<float> mask;      // oracle 独立光栅化的圆盘掩膜 (输入侧)
};

inline FixNoiseD fix_noise_d_mask(std::uint64_t seed, int w, int h,
                                  double sx, double sy, double sigma,
                                  double r0, double scale) {
    FixNoiseD fx;
    fx.w = w;
    fx.h = h;
    fx.star_x = sx;
    fx.star_y = sy;
    const std::size_t npix = static_cast<std::size_t>(w) * h;
    fx.bright.assign(npix, 0.0);
    fx.dim.assign(npix, 0.0);
    fx.base.assign(npix, 0.0);
    std::uint64_t st = seed;
    for (std::size_t i = 0; i < npix; ++i) {
        const double g = sigma * gauss01(st);
        fx.bright[i] = g;
        fx.dim[i] = g;
        fx.base[i] = g;
    }
    // 星注入 (振幅只进亮度通道; 高斯 PSF 峰值振幅 amp)
    const double amps[2] = {1.0e4, 10.0};
    for (int k = 0; k < 2; ++k) {
        std::vector<double>& frame = (k == 0) ? fx.bright : fx.dim;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const double dx = static_cast<double>(x) - sx;
                const double dy = static_cast<double>(y) - sy;
                const double r2 = dx * dx + dy * dy;
                const double sig_ps = 1.5;
                frame[static_cast<std::size_t>(y) * w + x] +=
                    amps[k] * std::exp(-r2 / (2.0 * sig_ps * sig_ps));
            }
        }
    }
    // oracle 独立圆盘光栅化 (与被测 star 通道独立实现: 逐像素距判,
    // rmax = max(1,r0)·max(1,scale) 冻结定义 ALG-NOISE-001 :144-145)
    const double rmax = std::max(1.0, r0) * std::max(1.0, scale);
    fx.mask.assign(npix, 0.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double dx = static_cast<double>(x) - sx;
            const double dy = static_cast<double>(y) - sy;
            if (dx * dx + dy * dy <= rmax * rmax) {
                fx.mask[static_cast<std::size_t>(y) * w + x] = 1.0f;
            }
        }
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-NOISE-E Poisson 诊断交叉 (ALG §13.4): 已知 gain/rn 合成帧
// N(mu_adu, var_th), var_th = mu/gain + (rn/gain)^2。
// 期望: snr_noise_gain_variance 与解析式逐位一致; 经验 variance_bg_global
// 对 var_th 相对差 ≤5% (SNR-005); use_gain_model=1 不改变生产输出
// (DISP-NOISE-003 现状: cfg gain 三字段零读取)。
// ---------------------------------------------------------------------------
struct FixNoiseE {
    int w = 0, h = 0;
    double mu_adu = 0.0, gain = 0.0, rn = 0.0, var_th = 0.0;
    std::vector<double> data;
};

inline FixNoiseE fix_noise_e_poisson(std::uint64_t seed, int w, int h,
                                     double mu_adu, double gain, double rn) {
    FixNoiseE fx;
    fx.w = w;
    fx.h = h;
    fx.mu_adu = mu_adu;
    fx.gain = gain;
    fx.rn = rn;
    fx.var_th = mu_adu / gain + (rn / gain) * (rn / gain);
    fx.data.assign(static_cast<std::size_t>(w) * h, 0.0);
    std::uint64_t st = seed;
    const double sig = std::sqrt(fx.var_th);
    for (std::size_t i = 0; i < fx.data.size(); ++i) {
        fx.data[i] = mu_adu + sig * gauss01(st);
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-NOISE-F scale law (ALG §13.4): alpha∈{0.5,2,10} → var'=α²var、
// ivar'=ivar/α² 逐位; 回乘恒等; 负面 alpha=NaN → 按 DISP-NOISE-005 断言
// 现状行为 (variance=NaN 直传, ivar 不变), 不得断言"已校验"。
// ---------------------------------------------------------------------------
struct FixNoiseF {
    double variance = 0.0, ivar = 0.0;  // 初始对 (var>0, ivar=1/var)
};

inline FixNoiseF fix_noise_f_pair(double variance) {
    FixNoiseF fx;
    fx.variance = variance;
    fx.ivar = 1.0 / variance;
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-NOISE-G fill 语义 (ALG §13.4): 常量场退化模型 (n_ctrl<4 →
// has_spatial_field=0) → 全场常量; 平面模型 n_ctrl≥4 → 平面像素与独立 LS
// 复算对照; out_variance/out_ivar 任一可 NULL。
// fill 输入模型由 FIX-NOISE-B (平面) / FIX-NOISE-C 产物 (常量) 供给,
// 本文件不单独生成帧。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FIX-NOISE-PERF 性能负载 (ALG §13.4 串并行/资源行): 大帧 Gaussian,
// O(h·w) 时间、O(h·w) 掩膜 + O(64) 控制点内存界断言; 无绝对速度门。
// ---------------------------------------------------------------------------
struct FixNoisePerf {
    int w = 0, h = 0;
    std::vector<double> data;
};

inline FixNoisePerf fix_noise_perf(std::uint64_t seed, int w, int h,
                                   double sigma_true) {
    FixNoisePerf fx;
    fx.w = w;
    fx.h = h;
    fx.data.assign(static_cast<std::size_t>(w) * h, 0.0);
    std::uint64_t st = seed;
    for (std::size_t i = 0; i < fx.data.size(); ++i) {
        fx.data[i] = sigma_true * gauss01(st);
    }
    return fx;
}

// ---------------------------------------------------------------------------
// 小帧 helper: gx·gy < 8×8 网格 (负面/边界: patch 划分整除性)
// ---------------------------------------------------------------------------
inline FixNoiseA fix_noise_a_small(std::uint64_t seed, int w, int h,
                                   double sigma_true) {
    return fix_noise_a_gaussian(seed, w, h, sigma_true);
}

}  // namespace p1noise

#endif  // P1NOISE_FIXTURES_HPP
