// P1-CAL-TEST · FIX-CAL-A..F 合成 fixture generator
//
// 合同锚: docs/algorithms/CALIBRATION_ALGORITHMS.md §9 TEST-CAL-DESIGN-001
// (P1-CAL-DOC 冻结, 2026-09-07, wave W1)。
//
// 规则 (模板 <prefix>-TEST §2):
//   - 全 fixture 由固定 seed + 参数确定生成, 零随机硬件依赖, 不提交大二进制。
//   - 这里提供本仓既有 splitmix64 统一 PRNG 约定 (与 core_artifact_test.cpp
//     的 fixture 谱系一致): 64 位状态, 输出 [0,1) double / 正态 double。
//   - fixture 值只进测试面与 oracle 的"输入"侧; 期望值一律由 oracle 独立
//     推导, 绝不经过被测函数 (模板 <prefix>-TEST §3)。
#ifndef P1CAL_FIXTURES_HPP
#define P1CAL_FIXTURES_HPP

#include <cmath>
#include <limits>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace p1cal {

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

inline double normal01(std::uint64_t& state) {
    // Box-Muller; u1,u2 ∈ (0,1] 防止 log(0)
    double u1 = uniform01(state);
    while (u1 <= 0.0) u1 = uniform01(state);
    double u2 = uniform01(state);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.28318530717958647 * u2);
}

// ---- FIX-CAL-A 常量场: raw=C / dark=D / flat=1.0, C,D ∈ {0,100,1000} ----
// (I1 不变量素材; 期望由 oracle 常量表冻结, max_abs==0)
inline void fix_cal_a_const_field(double C, double D, std::size_t w, std::size_t h,
                                  std::vector<float>* light,
                                  std::vector<float>* dark,
                                  std::vector<float>* flat) {
    const std::size_t npix = w * h;
    light->assign(npix, static_cast<float>(C));
    dark->assign(npix, static_cast<float>(D));
    flat->assign(npix, 1.0f);
}

// ---- FIX-CAL-B 解析梯度: raw=x+2y / dark=0.5x / flat=1+0.01x ----
// (期望由 oracle 逐像素独立重算解析式)
inline void fix_cal_b_gradient(std::size_t w, std::size_t h,
                               std::vector<float>* light,
                               std::vector<float>* dark,
                               std::vector<float>* flat) {
    const std::size_t npix = w * h;
    light->resize(npix);
    dark->resize(npix);
    flat->resize(npix);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            (*light)[i] = static_cast<float>(static_cast<double>(x) + 2.0 * static_cast<double>(y));
            (*dark)[i]  = static_cast<float>(0.5 * static_cast<double>(x));
            (*flat)[i]  = static_cast<float>(1.0 + 0.01 * static_cast<double>(x));
        }
    }
}

// ---- FIX-CAL-C 离群 stack: n=5 帧, 帧 4 注入 spike1000*δ + NaN 变体 ----
// sigma-clip 素材; δ 由 seed 决定 (期望密度 p≈0.05)。
struct FixCalC {
    std::vector<float> stack;     // [n][npix]
    std::vector<float> spike;     // δ 图样 (0/1), 逐像素
    std::size_t n_frames;
    std::size_t w, h;
};

inline FixCalC fix_cal_c_outlier_stack(std::uint64_t seed, std::size_t n_frames,
                                       std::size_t w, std::size_t h,
                                       bool nan_variant, double spike_prob = 0.05) {
    FixCalC fx;
    fx.n_frames = n_frames;
    fx.w = w;
    fx.h = h;
    const std::size_t npix = w * h;
    fx.stack.assign(n_frames * npix, 0.0f);
    fx.spike.assign(npix, 0.0f);

    std::uint64_t st = seed;
    for (std::size_t i = 0; i < npix; ++i) {
        const double delta = (uniform01(st) < spike_prob) ? 1.0 : 0.0;
        fx.spike[i] = static_cast<float>(delta);
        const double base = 100.0 + 10.0 * normal01(st);  // 背景 100 ± 10
        for (std::size_t n = 0; n < n_frames; ++n) {
            fx.stack[n * npix + i] = static_cast<float>(base + 2.0 * normal01(st));
        }
        // 帧 4 注入 +1000·δ 尖刺 (NaN 变体: 尖刺像素置 NaN 而非 +1000)
        if (n_frames >= 5) {
            fx.stack[4 * npix + i] = nan_variant
                ? std::numeric_limits<float>::quiet_NaN()
                : static_cast<float>(fx.stack[4 * npix + i] + 1000.0 * delta);
        }
    }
    return fx;
}

// ---- FIX-CAL-D master flat 三帧: 逐帧 median=100/200/400 的比例场 ----
// 构造保证每帧中位数精确等于目标值 (base + 三值周期场, 中位数=base)。
inline std::vector<float> fix_cal_d_flat_frames(std::size_t w, std::size_t h) {
    const std::size_t npix = w * h;
    // npix 偶数 → 中位数 = (v[med_lo]+v[med_hi])/2; 三值周期场 {0,1,2}
    // 让两个中位槽都是 base (w*h 为偶数且 >=8 时成立, 见断言)。
    std::vector<float> stack(3 * npix, 0.0f);
    const double bases[3] = {100.0, 200.0, 400.0};
    for (std::size_t n = 0; n < 3; ++n) {
        for (std::size_t i = 0; i < npix; ++i) {
            const double off = 100.0 * static_cast<double>(i % 3);
            stack[n * npix + i] = static_cast<float>(bases[n] + off);
        }
    }
    return stack;
}

// ---- FIX-CAL-E cosmetic: 20x20, 热点孤立点 + L 形 3 连通 + 12 像素方块 ----
struct FixCalE {
    std::size_t w, h;
    std::vector<float> dark;      // 背景场 (含噪声)
    std::vector<float> bias;      // 近常值场
    std::vector<float> data;      // 待修复帧 (背景 1000 + 噪声)
    std::size_t iso_x, iso_y;     // 孤立热像素
    std::size_t L_x0, L_y0;       // L 形 3 连通左上角
    std::size_t box_x0, box_y0;   // 12 像素方块 (4x3) 左上角
};

inline FixCalE fix_cal_e_cosmetic(std::uint64_t seed, std::size_t w = 20, std::size_t h = 20) {
    FixCalE fx;
    fx.w = w;
    fx.h = h;
    const std::size_t npix = w * h;
    fx.dark.assign(npix, 0.0f);
    fx.bias.assign(npix, 0.0f);
    fx.data.assign(npix, 0.0f);

    std::uint64_t st = seed;
    for (std::size_t i = 0; i < npix; ++i) {
        fx.dark[i] = static_cast<float>(100.0 + 2.0 * normal01(st));
        fx.bias[i] = static_cast<float>(50.0 + 0.5 * normal01(st));
        fx.data[i] = static_cast<float>(1000.0 + 5.0 * normal01(st));
    }

    // 孤立热像素: (3,3), 远超阈值
    fx.iso_x = 3; fx.iso_y = 3;
    fx.dark[fx.iso_y * w + fx.iso_x] = 100000.0f;
    // L 形 3 连通: (5,5),(6,5),(5,6) — 8 连通尺寸 3
    fx.L_x0 = 5; fx.L_y0 = 5;
    fx.dark[(fx.L_y0 + 0) * w + fx.L_x0 + 0] = 100000.0f;
    fx.dark[(fx.L_y0 + 0) * w + fx.L_x0 + 1] = 100000.0f;
    fx.dark[(fx.L_y0 + 1) * w + fx.L_x0 + 0] = 100000.0f;
    // 12 像素方块 (星点模拟): 4x3 @ (10,10) — 结构过滤应保留
    fx.box_x0 = 10; fx.box_y0 = 10;
    for (std::size_t dy = 0; dy < 3; ++dy)
        for (std::size_t dx = 0; dx < 4; ++dx)
            fx.dark[(fx.box_y0 + dy) * w + fx.box_x0 + dx] = 100000.0f;
    return fx;
}

// ---- FIX-CAL-F 负面: 参数域素材生成器 (值域无关, 仅形状) ----
inline std::vector<float> fix_cal_f_buffer(std::size_t npix, float fill) {
    return std::vector<float>(npix, fill);
}

}  // namespace p1cal

#endif  // P1CAL_FIXTURES_HPP
