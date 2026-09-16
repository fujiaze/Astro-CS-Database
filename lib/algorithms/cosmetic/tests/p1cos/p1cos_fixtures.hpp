// P1-COS-TEST · FIX-COS-A..F 合成 fixture generator
//
// 合同锚: docs/algorithms/COSMETIC_ALGORITHMS.md §9 TEST-COS-DESIGN-001
// (P1-COS-DOC 冻结, 2026-09-07, wave W1); 上游 SCI-CAL-001 (§2 参数表 /
// §6 坏点稀疏假设 / §9a mask 极性 1=坏点 / §11 oracle 容差标度)。
//
// 规则 (模板 <prefix>-TEST §2):
//   - 全 fixture 由固定 seed + 参数确定生成, 零随机硬件依赖, 不提交大二进制。
//   - splitmix64 统一 PRNG 约定 (与 core_artifact_test.cpp / p1cal fixture
//     谱系一致): 64 位状态, 输出 [0,1) double。
//   - fixture 值只进测试面与 oracle 的"输入"侧; 期望值一律由 oracle 独立
//     推导, 绝不经过被测函数 (模板 <prefix>-TEST §3)。
//   - 检测判定设计为远离阈值边界 (spike 幅度 ≥ 8σ), 使掩码判定对
//     float/double 统计域的 1ulp 差异不敏感 → oracle 掩码精确相等安全。
#ifndef P1COS_FIXTURES_HPP
#define P1COS_FIXTURES_HPP

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace p1cos {

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

// ---------------------------------------------------------------------------
// FIX-COS-A 常量场 (ALG §9): data/dark/bias 全常量 C → mad=0 → 阈值退化为
// ±med, 严格不等式判定下无坏点; 期望 out==data bitwise, out_hot=out_cold=0。
// ---------------------------------------------------------------------------
inline void fix_cos_a_const_field(float C, std::size_t w, std::size_t h,
                                  std::vector<float>* data,
                                  std::vector<float>* dark,
                                  std::vector<float>* bias) {
    const std::size_t npix = w * h;
    data->assign(npix, C);
    dark->assign(npix, C);
    bias->assign(npix, C);
}

// ---------------------------------------------------------------------------
// FIX-COS-B 解析注入坏点 (ALG §9): 常量/温和基场注入孤立单像素 hot spike
// (含角点/边线/中心 + seed 派生内部点, 互距 > 2 保证 5×5 邻域互不干扰),
// cold 域无注入。spike 幅度 = med + 1990 ADU ≫ thr → 判定远离边界。
// spikes[] 返回注入索引 (oracle 期望输入侧, 不经被测函数)。
// ---------------------------------------------------------------------------
struct FixCosB {
    std::size_t w, h;
    std::vector<float> data;   // 基场 + 坏点位 data=999
    std::vector<float> dark;   // 5.0 常量 + spike 1000
    std::vector<float> bias;   // 3.0 常量 (无冷点)
    std::vector<std::size_t> spikes;  // 注入位置 (行主序索引)
};

inline FixCosB fix_cos_b_spike_field(std::uint64_t seed, std::size_t w, std::size_t h,
                                     bool gradient_base) {
    FixCosB fx;
    fx.w = w;
    fx.h = h;
    const std::size_t npix = w * h;
    fx.data.resize(npix);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            // 基场: 常量 10 (解析变体) 或 10 + 0.25x (梯度 rtol 变体);
            // 值全部 float 可精确表示
            fx.data[i] = gradient_base ? (10.0f + 0.25f * static_cast<float>(x))
                                       : 10.0f;
        }
    }
    fx.dark.assign(npix, 5.0f);
    fx.bias.assign(npix, 3.0f);

    // 解析锚点: 角点 / 边线 / 中心 (ALG §9 FIX-COS-B: 镜像边界与 IDW
    // 路径分别命中; (0,0) 角、(0,h/2) 左边线、(w/2,0) 上边线、中心)
    const std::size_t cy = h / 2, cx = w / 2;
    const std::size_t anchors[] = {
        0,               // (0,0) 角点
        cy * w,          // (0,h/2) 左边线
        cx,              // (w/2,0) 上边线
        cx + cy * w,     // 中心
        (w - 1) + cy * w,   // (w-1,h/2) 右边线
        cx + (h - 1) * w,   // (w/2,h-1) 下边线
    };
    fx.spikes.assign(std::begin(anchors), std::end(anchors));
    // seed 派生内部点: 避开边界 3 像素环, 与既有注入点切比雪夫距离 > 2;
    // used[] 去重 + 尝试上限 (小帧候选区耗尽 → 保留锚点确定性返回)
    std::vector<char> used(npix, 0);
    for (const std::size_t idx : fx.spikes) used[idx] = 1;
    std::uint64_t st = seed;
    std::size_t tries = 0;
    while (fx.spikes.size() < 9 && tries < 500) {
        ++tries;
        const std::size_t x = 3 + static_cast<std::size_t>(uniform01(st) * static_cast<double>(w - 6));
        const std::size_t y = 3 + static_cast<std::size_t>(uniform01(st) * static_cast<double>(h - 6));
        const std::size_t idx = y * w + x;
        if (used[idx]) continue;
        bool far = true;
        for (const std::size_t s : fx.spikes) {
            const std::size_t sx = s % w, sy = s / w;
            std::size_t dx = (sx > x) ? sx - x : x - sx;
            std::size_t dy = (sy > y) ? sy - y : y - sy;
            if (dx < 3 && dy < 3) { far = false; break; }  // 5×5 邻域重叠
        }
        if (!far) continue;
        used[idx] = 1;
        fx.spikes.push_back(idx);
    }
    for (const std::size_t idx : fx.spikes) {
        fx.data[idx] = 999.0f;
        fx.dark[idx] = 1000.0f;
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-COS-C 连通域结构 (ALG §9): L 形 5 像素 (size 5) + 2 像素对 + 孤立单点,
// dark 注入 1000, data 坏点位 999 / 基场 10。max_size=4 → L 形被清除保留
// 原值, 2px 对与单点被修复。各结构 5×5 邻域互不干扰。
// ---------------------------------------------------------------------------
struct FixCosC {
    std::size_t w, h;
    std::vector<float> data;
    std::vector<float> dark;
    std::vector<float> bias;
    std::vector<std::size_t> l_shape;   // 5 像素 L 形 (将被 size 过滤剔除)
    std::vector<std::size_t> pair;      // 2 像素对 (被修复)
    std::vector<std::size_t> single;    // 孤立单点 (被修复)
};

inline FixCosC fix_cos_c_structure(std::size_t w = 32, std::size_t h = 32) {
    FixCosC fx;
    fx.w = w;
    fx.h = h;
    const std::size_t npix = w * h;
    fx.data.assign(npix, 10.0f);
    fx.dark.assign(npix, 5.0f);
    fx.bias.assign(npix, 3.0f);
    // L 形: (3,3)(4,3)(5,3)(5,4)(5,5) — 8 连通链 size=5
    const std::size_t L[] = {
        3 + 3 * w, 4 + 3 * w, 5 + 3 * w, 5 + 4 * w, 5 + 5 * w,
    };
    fx.l_shape.assign(std::begin(L), std::end(L));
    // 2 像素对: (10,10)(11,10) — 水平相邻
    const std::size_t P[] = {10 + 10 * w, 11 + 10 * w};
    fx.pair.assign(std::begin(P), std::end(P));
    // 孤立单点: (15,15)
    fx.single.push_back(15 + 15 * w);
    for (const std::size_t idx : fx.l_shape) { fx.data[idx] = 999.0f; fx.dark[idx] = 1000.0f; }
    for (const std::size_t idx : fx.pair)     { fx.data[idx] = 999.0f; fx.dark[idx] = 1000.0f; }
    for (const std::size_t idx : fx.single)   { fx.data[idx] = 999.0f; fx.dark[idx] = 1000.0f; }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-COS-D NaN/Inf 注入 (ALG §9): 现状行为断言素材 (DISP-COS-002)。
//   - NaN 槽位注在 data (透传路径) 与 dark (统计污染路径) 的变体开关。
//   - data 槽位放常量 10 基场中央, 保证不与 dark spike 邻域重叠。
// ---------------------------------------------------------------------------
struct FixCosD {
    std::size_t w, h;
    std::vector<float> data;
    std::vector<float> dark;
    std::vector<float> bias;
    std::size_t nan_slot;   // data NaN 槽位 (非坏点)
    std::size_t inf_slot;   // data +Inf 槽位 (非坏点)
    bool dark_has_nan;      // true: dark 中央位注入 NaN (统计污染变体)
};

inline FixCosD fix_cos_d_nan_inf(bool dark_has_nan) {
    const std::size_t w = 16, h = 16;
    FixCosD fx;
    fx.w = w;
    fx.h = h;
    fx.dark_has_nan = dark_has_nan;
    const std::size_t npix = w * h;
    fx.data.assign(npix, 10.0f);
    fx.dark.assign(npix, 5.0f);
    fx.bias.assign(npix, 3.0f);
    fx.nan_slot = 8 + 8 * w;
    fx.inf_slot = 7 + 8 * w;
    fx.data[fx.nan_slot] = std::numeric_limits<float>::quiet_NaN();
    fx.data[fx.inf_slot] = std::numeric_limits<float>::infinity();
    if (dark_has_nan) {
        fx.dark[4 + 4 * w] = std::numeric_limits<float>::quiet_NaN();
    } else {
        fx.dark[4 + 4 * w] = 1000.0f;  // 对照变体: 正常单 spike (保证对照路径有坏点)
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-COS-E IDW 方向性 (ALG §9): 竖直 3 像素坏点列 + 水平 3 像素坏点行
// (两构型同场、互不干扰; DISP-COS-003 现状 = 4 方向最近好像素 1/dist 加权,
// 实现方向序 {左,右,上,下})。
//   竖列 (8,11)(8,12)(8,13): 中心修复 = L*1 + R*1 + U*0.5 + D*0.5 (权:
//     左右 dist=1 → 1; 上下沿坏列 dist=2 → 0.5) = (9+9+2+4)/3 = 8.0。
//   横行 (15,12)(16,12)(17,12): 中心修复 = L*0.5 + R*0.5 + U*1 + D*1
//     = (2+4+9+9)/3 = 8.0。
//   邻值全为 ≤2^24 整数/半整数 → 实现序 float 累加全程零舍入 → 期望
//   bitwise; 权重错乱 (两构型互换 / dist 全 1) 给出 7.0/7.5 → 方向敏感。
//   variant_rtol: 非常值邻居 (float 可精确表示) → oracle double 域同序
//   复算 rtol=1e-6 对照 (float 逐步舍入 vs double 单次舍入差 ≪ 1e-6)。
// ---------------------------------------------------------------------------
struct FixCosE {
    std::size_t w, h;
    std::vector<float> data;
    std::vector<float> dark;
    std::vector<float> bias;
    std::size_t col_mid, col_u, col_d;   // 竖直列 中/上/下 坏点索引
    std::size_t row_mid, row_l, row_r;   // 水平行 中/左/右 坏点索引
    std::size_t col_up, col_down, col_left, col_right;    // 竖列邻居
    std::size_t row_up, row_down, row_left, row_right;    // 横行邻居
};

inline FixCosE fix_cos_e_idw(bool variant_rtol) {
    const std::size_t w = 24, h = 24;
    FixCosE fx;
    fx.w = w;
    fx.h = h;
    const std::size_t npix = w * h;
    fx.data.assign(npix, 10.0f);
    fx.dark.assign(npix, 5.0f);
    fx.bias.assign(npix, 3.0f);
    const std::size_t cx1 = 8, cy = 12;   // 竖列中心
    const std::size_t cx2 = 16;           // 横行中心
    fx.col_mid = cx1 + cy * w;
    fx.col_u   = cx1 + (cy - 1) * w;
    fx.col_d   = cx1 + (cy + 1) * w;
    fx.row_mid = cx2 + cy * w;
    fx.row_l   = (cx2 - 1) + cy * w;
    fx.row_r   = (cx2 + 1) + cy * w;
    fx.col_up    = cx1 + (cy - 2) * w;
    fx.col_down  = cx1 + (cy + 2) * w;
    fx.col_left  = (cx1 - 1) + cy * w;
    fx.col_right = (cx1 + 1) + cy * w;
    fx.row_up    = cx2 + (cy - 1) * w;
    fx.row_down  = cx2 + (cy + 1) * w;
    fx.row_left  = (cx2 - 2) + cy * w;
    fx.row_right = (cx2 + 2) + cy * w;
    const std::size_t bad[] = {fx.col_u, fx.col_mid, fx.col_d, fx.row_l, fx.row_mid, fx.row_r};
    for (const std::size_t idx : bad) {
        fx.data[idx] = 999.0f;
        fx.dark[idx] = 1000.0f;
    }
    if (variant_rtol) {
        // 非常值变体: 全 float 可精确表示, 期望由 oracle double 同序复算
        fx.data[fx.col_up] = 7.5f;     fx.data[fx.col_down] = 9.25f;
        fx.data[fx.col_left] = 11.125f; fx.data[fx.col_right] = 13.375f;
        fx.data[fx.row_up] = 7.5f;     fx.data[fx.row_down] = 9.25f;
        fx.data[fx.row_left] = 11.125f; fx.data[fx.row_right] = 13.375f;
    } else {
        // 2 幂/整数变体 (bitwise): 竖列 U=4 D=8 L=R=9; 横行 U=D=9 L=4 R=8
        fx.data[fx.col_up] = 4.0f;   fx.data[fx.col_down] = 8.0f;
        fx.data[fx.col_left] = 9.0f; fx.data[fx.col_right] = 9.0f;
        fx.data[fx.row_up] = 9.0f;   fx.data[fx.row_down] = 9.0f;
        fx.data[fx.row_left] = 4.0f; fx.data[fx.row_right] = 8.0f;
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-COS-F 双中位均值 (ALG §9): 5×5 镜像窗内偶数好邻居 (24 个) →
//   median = (hi+lo)*0.5, hi=上中位、lo=下半区最大。
//   even (主案): 窗内棋盘 12×10 + 12×20 → (20+10)*0.5 = 15.0 bitwise。
//   odd (变体): 窗内 23 好 (12×10 + 11×20) → 中值 = v[11] = 10.0 bitwise。
//   坏点 (8,8) 远离边界 (w=h=16) → 无镜像折叠; odd 变体第二坏点 (10,10)
//   由 dark 同步注入。
// ---------------------------------------------------------------------------
struct FixCosF {
    std::size_t w, h;
    std::vector<float> data;
    std::vector<float> dark;
    std::vector<float> bias;
    std::size_t slot;       // 断言槽位 (8,8)
    bool odd_variant;
};

inline FixCosF fix_cos_f_twin_median(bool odd_variant) {
    const std::size_t w = 16, h = 16;
    FixCosF fx;
    fx.w = w;
    fx.h = h;
    fx.odd_variant = odd_variant;
    const std::size_t npix = w * h;
    fx.data.assign(npix, 10.0f);
    fx.dark.assign(npix, 5.0f);
    fx.bias.assign(npix, 3.0f);
    fx.slot = 8 + 8 * w;
    // 5×5 窗 (x,y ∈ [6,10]) 棋盘赋值: x+y 奇 → 10, 偶 → 20。
    //   even: 窗 25 位 - 自身(偶,20) = 12×10 + 12×20 → (hi+lo)*0.5 = 15.0
    //   odd : 再减 (10,10)(偶,20)     = 12×10 + 11×20 → 中值 v[11] = 10.0
    for (std::size_t y = 6; y <= 10; ++y) {
        for (std::size_t x = 6; x <= 10; ++x) {
            const std::size_t i = x + y * w;
            fx.data[i] = ((x + y) % 2 == 1) ? 10.0f : 20.0f;
        }
    }
    fx.data[fx.slot] = 999.0f;
    fx.dark[fx.slot] = 1000.0f;
    if (odd_variant) {
        const std::size_t slot2 = 10 + 10 * w;
        fx.data[slot2] = 999.0f;
        fx.dark[slot2] = 1000.0f;
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-COS-PERF 负载场 (performance 组, ALG §9 串并行/资源): 大场 + spike
// 群 (允许相邻, perf 不做逐像素正确性断言)。固定 seed 确定生成。
// ---------------------------------------------------------------------------
struct FixCosPerf {
    std::size_t w, h;
    std::vector<float> data;
    std::vector<float> dark;
    std::vector<float> bias;
};

inline FixCosPerf fix_cos_perf_field(std::uint64_t seed, std::size_t w, std::size_t h) {
    FixCosPerf fx;
    fx.w = w;
    fx.h = h;
    const std::size_t npix = w * h;
    fx.data.resize(npix);
    fx.dark.assign(npix, 5.0f);
    fx.bias.assign(npix, 3.0f);
    for (std::size_t i = 0; i < npix; ++i) {
        // 温和梯度 + 2 的幂量化噪声 (float 精确), 幅度 ≪ 检测阈值
        const double v = 10.0 + 0.001 * static_cast<double>(i % 64);
        fx.data[i] = static_cast<float>(v);
    }
    std::uint64_t st = seed;
    const std::size_t n_spikes = 512;
    for (std::size_t k = 0; k < n_spikes; ++k) {
        const std::size_t x = static_cast<std::size_t>(uniform01(st) * static_cast<double>(w));
        const std::size_t y = static_cast<std::size_t>(uniform01(st) * static_cast<double>(h));
        fx.dark[y * w + x] = 1000.0f;
    }
    return fx;
}

}  // namespace p1cos

#endif  // P1COS_FIXTURES_HPP
