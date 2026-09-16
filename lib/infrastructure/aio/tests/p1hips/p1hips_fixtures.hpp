// P1-HIPS-TEST · 固定 seed fixture generator (FIX-HIPS-A..F)
//
// 控制包任务: P1-HIPS-TEST; 合同锚 HIPS_WRITER.md §9 TEST-HIPS-DESIGN-001
// fixture 清单 (P1-HIPS-DOC 冻结):
//   F1 合成小天区 (K=9 → nside=512, 12 cell 数量级; flux/area/var_num 解析可控)
//   F2 多 tile 覆盖同父 (hierarchy 聚合闭合)
//   F3 含 NaN/0-area/负 flux 边界像素
//   F4 SNR 点集 (已知 ra/dec → cell)
//   F5 prov 键完整/缺省两态
//   F6 f32/f64 双 dtype
// 生成器规则 (§9): fixture generator, 不内嵌生产算法 —— 本文件只做
// splitmix64 PRNG + 解析可控的合成 accumulator 视图, 不调用生产 symbol。
#ifndef P1HIPS_FIXTURES_HPP
#define P1HIPS_FIXTURES_HPP

#include "aio_hips.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace p1hips {

// ---------------------------------------------------------------------------
// splitmix64 固定 seed PRNG (fixture 可复现; 谱系对齐 p1drz_fixtures.hpp)
// ---------------------------------------------------------------------------
struct SplitMix64 {
    std::uint64_t state;
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // [0,1) 双精度
    double unit() {
        return (double)(next() >> 11) * (1.0 / 9007199254740992.0);
    }
};

// 叶级 tile 参数 (HiPS 标准: tile 512×512, nside=512 → K=9)
constexpr std::uint32_t FIX_LEAF_ORDER = 9;    // L
constexpr std::uint32_t FIX_TILE_ORDER = 0;    // K = L - 9
constexpr std::uint32_t FIX_NSIDE = 512;       // 2^9
constexpr std::uint32_t FIX_WIDTH = 512;
constexpr std::size_t   FIX_NPIX = 512u * 512u;
// Norder K=0 → 12 个 tile 单元
constexpr std::uint64_t FIX_ORDER_CELLS = 12;

// 叶级 cell 面积 (sr): 4π / (12·nside²) —— 与生产同式的独立解析引用,
// 该式为 HEALPix 球面划分定义 (Ω=4π/12nside²), 非 writer 私有公式。
constexpr double fix_a_cell_sr(std::uint32_t nside) {
    return 4.0 * 3.14159265358979323846 / (12.0 * (double)nside * (double)nside);
}

// ---------------------------------------------------------------------------
// FIX-HIPS-A: 单 tile 合成 accumulator (解析可控: 常数面亮度 B + 常数覆盖
// 面积 A, var_num = sigma²·A) → oracle 逐像素期望可用闭式解。
// dt: AIO_HIPS_FLOAT64 (默认) / AIO_HIPS_FLOAT32 — f32 产品视图必须同为
// float (生产按 view.data_type 解释指针)。
struct FixViewF64 {
    std::vector<double> flux_sum;
    std::vector<double> covered_area;
    std::vector<double> var_num_sum;
    std::vector<float> flux_f;
    std::vector<float> area_f;
    std::vector<float> var_f;
    std::vector<std::uint8_t> valid_mask;
    AstroSphereTileView view{};
};

inline FixViewF64 fix_hips_a_tile(std::uint64_t parent_ipix,
                                  double sb_magnitude,     // 常数面亮度 (flux/area)
                                  double area_factor,      // 覆盖面积 = factor·A_cell
                                  double sigma,            // 单位面积噪声 (var/area)
                                  bool with_valid_mask,
                                  bool with_variance,
                                  int dt = AIO_HIPS_FLOAT64,
                                  std::uint32_t leaf_order = FIX_LEAF_ORDER) {
    FixViewF64 f;
    f.view.data_type = dt;
    f.view.leaf_order = leaf_order;
    f.view.width = FIX_WIDTH;
    f.view.parent_ipix = parent_ipix;
    f.view.valid_mask = nullptr;
    f.view.var_num_sum = nullptr;
    if (dt == AIO_HIPS_FLOAT64) {
        f.flux_sum.assign(FIX_NPIX, sb_magnitude * area_factor * fix_a_cell_sr(FIX_NSIDE));
        f.covered_area.assign(FIX_NPIX, area_factor * fix_a_cell_sr(FIX_NSIDE));
        if (with_variance)
            f.var_num_sum.assign(FIX_NPIX, (sigma * sigma) * (area_factor * fix_a_cell_sr(FIX_NSIDE)));
        if (with_valid_mask)
            f.valid_mask.assign(FIX_NPIX, 1);
        f.view.flux_sum = f.flux_sum.data();
        f.view.covered_area = f.covered_area.data();
        f.view.valid_mask = f.valid_mask.empty() ? nullptr : f.valid_mask.data();
        f.view.var_num_sum = with_variance ? f.var_num_sum.data() : nullptr;
    } else {
        f.flux_f.assign(FIX_NPIX, (float)(sb_magnitude * area_factor * fix_a_cell_sr(FIX_NSIDE)));
        f.area_f.assign(FIX_NPIX, (float)(area_factor * fix_a_cell_sr(FIX_NSIDE)));
        if (with_variance)
            f.var_f.assign(FIX_NPIX, (float)((sigma * sigma) * (area_factor * fix_a_cell_sr(FIX_NSIDE))));
        if (with_valid_mask)
            f.valid_mask.assign(FIX_NPIX, 1);
        f.view.flux_sum = f.flux_f.data();
        f.view.covered_area = f.area_f.data();
        f.view.valid_mask = f.valid_mask.empty() ? nullptr : f.valid_mask.data();
        f.view.var_num_sum = with_variance ? f.var_f.data() : nullptr;
    }
    return f;
}

// ---------------------------------------------------------------------------
// FIX-HIPS-B: 多 tile 覆盖 (hierarchy 聚合闭合 F2): n_leaf 个不同 parent,
// 常数面亮度 → 任一祖先像素 sig = B, sup ≤ 1; 由解析聚合检查闭合。
// parent 集合: parent 0 的 4 个 order-1 子单元 (i=0..3 → parent_ipix=i)
// + order-0 其余 parent, 保证"同父多子"聚合可检。
// ---------------------------------------------------------------------------
inline std::vector<std::uint64_t> fix_hips_b_parents(int n_leaf) {
    std::vector<std::uint64_t> ps;
    ps.reserve((std::size_t)n_leaf);
    // tile_order=0 → parent_ipix ∈ [0,12); order-1 子单元位于同一 order-0
    // 单元内不存在 (K=0 叶级 tile 即最粗单元), 多 tile 聚合闭合退化为
    // "全部 12 cell 覆盖 + 单 cell 多 pixel 聚合"; 为覆盖 F2 语义取
    // parent 0..n_leaf-1 (含同 hierarchy 树)。
    for (int i = 0; i < n_leaf; ++i) ps.push_back((std::uint64_t)i);
    return ps;
}

// ---------------------------------------------------------------------------
// FIX-HIPS-C: 边界像素视图 (F3): 每类缺陷固定位置注入
//   idx 0        — NaN flux (valid=1)
//   idx 1        — 0-area (valid=1)
//   idx 2        — 负 flux (valid=1, area>0)   [按合同: flux 有限负仍计算]
//   idx 3        — Inf flux (valid=1)
//   idx 4        — valid=0 (flux/area 正常)
//   其余         — 正常解析像素
// ---------------------------------------------------------------------------
inline FixViewF64 fix_hips_c_edge_tile(std::uint64_t parent_ipix,
                                       double sb_magnitude, double area_factor) {
    FixViewF64 f = fix_hips_a_tile(parent_ipix, sb_magnitude, area_factor, 0.0, true, false);
    const double A = area_factor * fix_a_cell_sr(FIX_NSIDE);
    f.flux_sum[0] = std::numeric_limits<double>::quiet_NaN();
    f.covered_area[1] = 0.0;
    f.flux_sum[2] = -sb_magnitude * A;   // 负 flux (面积正常 → 按公式产生负 sig)
    f.flux_sum[3] = std::numeric_limits<double>::infinity();
    f.valid_mask[4] = 0;
    return f;
}

// ---------------------------------------------------------------------------
// FIX-HIPS-D: SNR 点集 (F4): 固定 seed 在单位球均匀抽样 → 已知 ra/dec,
// 由独立 oracle (fix_snr_cell_of) 判 cell; 与生产 ang2pix 独立。
// ---------------------------------------------------------------------------
struct FixSnrPointF : AioHipsSnrPoint {};

inline std::vector<FixSnrPointF> fix_hips_d_snr_points(std::uint64_t seed, int n) {
    SplitMix64 rng(seed);
    std::vector<FixSnrPointF> out;
    out.reserve((std::size_t)n);
    for (int i = 0; i < n; ++i) {
        FixSnrPointF p{};
        // 均匀球面抽样: z ∈ [-1,1], ra ∈ [0,360)
        const double z = 2.0 * rng.unit() - 1.0;
        const double ra = 360.0 * rng.unit();
        p.ra_deg = ra;
        p.dec_deg = std::asin(z) * 180.0 / 3.14159265358979323846;
        p.snr = 1.0 + 99.0 * rng.unit();
        p.star_id = (std::int64_t)(1000000 + i);
        p.quality_flags = (std::uint32_t)(rng.next() & 31u);
        p.photometric_status = (std::uint32_t)(i % 3);
        out.push_back(p);
    }
    return out;
}

// ---------------------------------------------------------------------------
// FIX-HIPS-E: variance 半有效视图: 前半像素 var_num>0, 后半 var_num=0
// (any_valid=true → 写出合法; 对半分界用于 ivar=1/var 互倒检查)
// ---------------------------------------------------------------------------
inline FixViewF64 fix_hips_e_half_var_tile(std::uint64_t parent_ipix,
                                           double sb, double area_factor,
                                           double sigma) {
    FixViewF64 f = fix_hips_a_tile(parent_ipix, sb, area_factor, sigma, true, true);
    for (std::size_t i = FIX_NPIX / 2; i < FIX_NPIX; ++i)
        f.var_num_sum[i] = 0.0;
    return f;
}

// FIX-HIPS-F: 全无效 variance 视图 (var_num 全 0 → write_variance rc=-5)
inline FixViewF64 fix_hips_f_all_invalid_var_tile(std::uint64_t parent_ipix) {
    FixViewF64 f = fix_hips_a_tile(parent_ipix, 10.0, 0.5, 1.0, true, true);
    for (auto& v : f.var_num_sum) v = 0.0;
    return f;
}

}  // namespace p1hips

#endif  // P1HIPS_FIXTURES_HPP
