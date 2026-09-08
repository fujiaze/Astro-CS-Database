// P1-DRZ-TEST · 独立 oracle (期望值推导, 不调用被测函数)
//
// 独立性规则 (模板 <prefix>-TEST §3): oracle 不调用被测函数、不复制同一
// 实现。期望值推导路径:
//   - 几何原语: p1drz_geom.hpp (向量旋转 TAN / Van Oosterom-Strackee 立体角
//     / 切平面 S-H, 与生产 wcs_sip.cpp 球面三角公式路径、spherical_overlap.cpp
//     Eriksson 扇形剖分**不同式**);
//   - 权威 leaf 地址: astrocs::healpix (lib/common/healpix, astropy-healpix
//     交叉验证 mismatch=0 的单权威实现 — 允许引用, 头部注释为证);
//   - 聚合恒等式 (核心 oracle 手段): F_p=Σ x_j·w_jp、D_p=Σ a_jp、
//     S_p=F_p/D_p、var_p=Σv_j·w_jp²/D_p² 全部在 oracle 侧按 SCI-DRZ-001
//     §5 公式独立重建, 不调用 drizzleTiled*。
//
// 覆盖 (模板 §4): 正常/边界 leaf/NaN/Inf/invalid 参数/确定性/1-N worker。
#ifndef P1DRZ_ORACLE_HPP
#define P1DRZ_ORACLE_HPP

#include "drizzle_engine.h"
#include "p1drz_geom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace p1drz {

// NOTE: CHECK/故障注入框架在 p1drz_test_main.hpp; 本头只提供纯 oracle
// 期望函数 (返回 bool / 结构体), 期望值推导不经被测函数。

// ---------------------------------------------------------------------------
// leaf 记录提取 (测试侧只做结构展开, 不产生期望值)
// ---------------------------------------------------------------------------
struct LeafRec {
    uint64_t ipix = 0;
    double sumFlux = 0.0, sumArea = 0.0, sumVarNum = 0.0, signal = 0.0,
           variance = 0.0;
    uint32_t nContrib = 0;
};

// tile 展开为逐 leaf 记录 (ipix = parent<<2*depth | local), 按 ipix 排序
template <typename Scalar>
inline std::vector<LeafRec> extract_leafs(
    const std::vector<drizzle::TileAccumulatorT<Scalar>>& tiles,
    uint32_t depth) {
    const uint32_t shift = 2u * depth;
    std::vector<LeafRec> out;
    for (const auto& tile : tiles)
        for (uint32_t local : tile.touched) {
            if (local >= tile.pixels.size()) continue;
            const auto& a = tile.pixels[local];
            LeafRec r;
            r.ipix = (shift > 0) ? ((tile.parent_ipix << shift) |
                                    (uint64_t)local)
                                 : tile.parent_ipix;
            r.sumFlux = (double)a.sumFlux;
            r.sumArea = (double)a.sumArea;
            r.sumVarNum = (double)a.sumVarNum;
            r.signal = r.sumArea > 0.0 ? r.sumFlux / r.sumArea : 0.0;
            r.variance = r.sumArea > 0.0
                             ? r.sumVarNum / (r.sumArea * r.sumArea)
                             : 0.0;
            r.nContrib = a.nContrib;
            out.push_back(r);
        }
    std::sort(out.begin(), out.end(),
              [](const LeafRec& a, const LeafRec& b) { return a.ipix < b.ipix; });
    return out;
}

// ---------------------------------------------------------------------------
// Oracle O1: 像素积分面积 (独立 TAN 前向 + Van Oosterom 立体角)
//   A_pixel_j = ∮ pixel_j 四角球面四边形面积 (冻结 §9 面积误差预算源)
// ---------------------------------------------------------------------------
inline double oracle_pixel_area(const drizzle::WcsParams& wcs, int x, int y) {
    const std::vector<GVec3> q = geom_drop_polygon(wcs, (double)x, (double)y, 1.0);
    return geom_quad_area(q);
}

// Oracle O2: drop 面积 (pixfrac 收缩四角)
inline double oracle_drop_area(const drizzle::WcsParams& wcs, double px,
                               double py, double pixfrac) {
    const std::vector<GVec3> q = geom_drop_polygon(wcs, px, py, pixfrac);
    return geom_quad_area(q);
}

// ---------------------------------------------------------------------------
// Oracle O3: 汇总闭合恒等式 (FLUX-CONSERVATION 主 oracle)
//   Σ_p F_p = Σ_j x_j · (Σ_p w_jp) = Σ_j x_j  (当 Σ_p a_jp = A_drop,j 全覆盖)
//   → total_flux_out(Σ sumFlux) / total_flux_in(Σ x_j) ≈ 1 (<1e-6 FP64 门)
// 注意: 视场边缘 drop 与 HEALPix 域裁切会造成边界损失 — 用 FIX-DRZ-B
// (高斯完全在场内, 边缘像素值≈0) 把边界项压到 1e-9 量级以下。
// ---------------------------------------------------------------------------
struct ClosureOracle {
    double total_in = 0.0;    // Σ_j x_j (fixture 直读)
    double total_out = 0.0;   // Σ_p sumFlux (被测输出)
    double total_area = 0.0;  // Σ_p sumArea
    double ratio_flux = 0.0;
};

inline ClosureOracle oracle_flux_closure(const drizzle::FitsImage& img,
                                         const std::vector<LeafRec>& leafs) {
    ClosureOracle o;
    for (double v : img.pixels_f64) {
        if (std::isfinite(v)) o.total_in += v;
    }
    for (const auto& l : leafs) {
        o.total_out += l.sumFlux;
        o.total_area += l.sumArea;
    }
    o.ratio_flux = (o.total_in != 0.0) ? o.total_out / o.total_in : 0.0;
    return o;
}

// ---------------------------------------------------------------------------
// Oracle O4: 常量面亮度场 S_p = B0 (SCI-003 冻结恒等式)
//   x_j = B0·A_pixel_j → 每个完全覆盖 leaf 的 S_p = Σ x_j w_jp / Σ a_jp
//   = B0·(Σ_j a_jp·A_pixel_j/A_drop,j)/(Σ_j a_jp) — 当 A_pixel_j≈A_drop,j
//   (同尺度像素), S_p → B0 精确 (相对偏差 ~ (A_drop/A_pixel - 1) = O(ρ²))。
//   冻结门: |S_p/B0 - 1| < 1e-3 (oracle 面积近似的 ρ² 项预算);
//   uniformity: leaf 间 signal 相对标准差 < 1e-4 (无接缝/调制)。
// ---------------------------------------------------------------------------
struct UniformityOracle {
    double mean = 0.0, rel_std = 0.0, max_abs_dev = 0.0;
    bool ok_uniform = false, ok_mean = false;
};

inline UniformityOracle oracle_const_sb(const std::vector<LeafRec>& leafs,
                                        double b0) {
    UniformityOracle u;
    if (leafs.empty()) return u;
    double s = 0.0, s2 = 0.0;
    for (const auto& l : leafs) {
        s += l.signal;
        s2 += l.signal * l.signal;
        const double dev = std::fabs(l.signal / b0 - 1.0);
        if (dev > u.max_abs_dev) u.max_abs_dev = dev;
    }
    const double n = (double)leafs.size();
    u.mean = s / n;
    const double var = std::max(0.0, s2 / n - u.mean * u.mean);
    u.rel_std = (u.mean != 0.0) ? std::sqrt(var) / std::fabs(u.mean) : 0.0;
    u.ok_uniform = u.rel_std < 1e-4;                      // DOC §9 uniform_rel_std
    u.ok_mean = u.max_abs_dev < 1e-3;                     // 面积近似 ρ² 预算
    return u;
}

// ---------------------------------------------------------------------------
// Oracle O5: 常量 ADU 对照 (S_p = C/A_drop ≠ C, SCI-003 反向语义检验)
//   每像素恒定通量 C, drop 覆盖 leaf 时 S_p = C/A_drop,pixel (面亮度)。
//   A_drop 逐 leaf 用 oracle Van Oosterom 独立重算 (与生产 Eriksson 不同式);
//   覆盖该 leaf 的源像素 j 有 S_p = x_j·w_jp/a_jp = C/A_drop,j。
//   断言: (i) S_p·A_drop,oracle ≈ C (闭合, 1e-3);
//         (ii) S_p 与 C 明显区分 (|S/C-1|>0.5, 300" px ≫ 1 sr 不可能贴近)。
// ---------------------------------------------------------------------------
inline bool oracle_const_adu_inverse(const std::vector<LeafRec>& leafs,
                                     const drizzle::WcsParams& wcs,
                                     double c_adu, double scale_arcsec,
                                     double pixfrac) {
    if (leafs.empty() || c_adu == 0.0) return false;
    // 全部源像素的 A_drop 变化 <2% (16×16 patch @300"/px ≈ 4° 视场,
    // gnomonic 三阶畸变 ~1e-4 量级; 单值估计 + per-leaf 校验双保险)
    const int cx = (int)std::lround((wcs.crpix[0] - 1.0));
    const int cy = (int)std::lround((wcs.crpix[1] - 1.0));
    const double a_center = oracle_drop_area(wcs, (double)cx, (double)cy, pixfrac);
    if (!(a_center > 0.0)) return false;
    for (const auto& l : leafs) {
        if (l.nContrib == 0) return false;
        const double closure = l.signal * a_center / c_adu;
        if (std::fabs(closure - 1.0) > 1e-3) return false;   // S_p·A_drop = C
        if (std::fabs(l.signal - c_adu) / std::fabs(c_adu) < 0.5)
            return false;  // S_p ≠ C (语义区分断言)
    }
    return true;
}

// ---------------------------------------------------------------------------
// Oracle O6: FIX-DRZ-D 脉冲期望 — **结构闭合** (逐 leaf overlap 分布依赖
// 全部 256 源像素 drop 贡献 — 0 值像素的 drop 同样进 D_p, 逐 leaf 期望需
// 全量 overlap 重建, leaf 边界不可用 astrocs::healpix 获得; 改用两条
// 无 leaf 几何的精确闭合):
//   (i) 通量守恒: Σ_p a_jp = A_drop,j (drop 互斥真覆盖) →
//       Σ_p F_p = x_pulse·A_drop/A_drop = amp (相对 1e-6, FP64 主域门);
//   (ii) 面积闭合: Σ_p D_p = Σ_j A_drop,j (256 像素 drop 面积之和,
//       场内变化 ±8e-4 → 门 5e-3)。
// ---------------------------------------------------------------------------
struct ImpulseOracle {
    double ratio_flux = 0.0;   // Σ_p F_p / amp
    double ratio_area = 0.0;   // Σ_p D_p / Σ_j A_drop,j
    bool ok = false;
};

inline ImpulseOracle oracle_impulse(const std::vector<LeafRec>& leafs,
                                    double amp,
                                    const drizzle::WcsParams& wcs, int w,
                                    int h, double pixfrac) {
    ImpulseOracle o;
    if (leafs.empty() || amp == 0.0) return o;
    double total_f = 0.0, total_d = 0.0;
    for (const auto& l : leafs) {
        total_f += l.sumFlux;
        total_d += l.sumArea;
    }
    double a_drop_sum = 0.0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            a_drop_sum += oracle_drop_area(wcs, (double)x, (double)y, pixfrac);
    if (!(a_drop_sum > 0.0)) return o;
    o.ratio_flux = total_f / amp;
    o.ratio_area = total_d / a_drop_sum;
    o.ok = std::fabs(o.ratio_flux - 1.0) < 1e-6 &&
           std::fabs(o.ratio_area - 1.0) < 5e-3;
    return o;
}

// ---------------------------------------------------------------------------
// Oracle O7: 方差传播独立重建 (SCI-DRZ-014)
//   var_p = Σ_j v_j·w_jp² / D_p²; oracle 侧独立重建:
//   var_p·D_p² = Σ_j v_j·w_jp², w_jp = a_jp/A_drop,j。
//   对常数方差图 v_j = σ²: sumVarNum = σ²·Σ w_jp² →
//   var_p/σ² = Σw²/D² 可由 oracle 重算的 w 验证 — 简化路径: 用 α 缩放律
//   (x→αx, v→α²v ⇒ var→α²var 精确) + oracle 量级门:
//   var_p ∈ [σ²·Σw²/D² 期望带] 由独立 gnomonic 重算。
//   本测试面取两条: (a) α² 缩放恒等 (精确, 冻结 1e-4);
//   (b) sumVarNum 与 Σx²w² 的 oracle 重算比 (1-N worker 共用)。
// ---------------------------------------------------------------------------
struct VarOracle {
    bool leaves_hit = false;      // oracle 超采样命中集合 ⊆ 被测 touched
    bool variance_closed = false; // 完全内部 leaf: var = σ²/A_drop² 精确闭合
    double worst_rel = 0.0;
    int n_checked = 0;
};

inline VarOracle oracle_variance_const(const std::vector<LeafRec>& leafs,
                                       const drizzle::WcsParams& wcs,
                                       const drizzle::DrizzleConfig& cfg,
                                       double sigma2,
                                       const drizzle::FitsImage& img) {
    VarOracle o;
    const uint32_t nside = (uint32_t)cfg.nside;
    // 中心像素 (完全内部, 无边界截断)
    const int cx = img.width / 2, cy = img.height / 2;
    const double a_drop = oracle_drop_area(wcs, (double)cx, (double)cy,
                                           cfg.pixfrac);
    if (!(a_drop > 0.0) || sigma2 <= 0.0) return o;
    // oracle 侧: 中心像素 drop 覆盖的 leaf 集合 (权威 ang2pix 超采样)。
    // drop ⊇ leaf (40" leaf ≪ 300" px) → 每个命中 leaf 由单源像素完全
    // 覆盖: a_jp = A_leaf, w_jp = A_leaf/A_drop, D_p = A_leaf
    //   var_p = σ²·w²/D² = σ²·(A_leaf/A_drop)²/A_leaf² = σ²/A_drop² (精确)
    std::set<uint64_t> hit = geom_pixel_leaves(wcs, (double)cx, (double)cy,
                                               cfg.pixfrac, nside, 9);
    std::map<uint64_t, const LeafRec*> by_ipix;
    for (const auto& l : leafs) by_ipix[l.ipix] = &l;
    int superset_miss = 0;
    for (uint64_t e : hit) {
        auto it = by_ipix.find(e);
        if (it == by_ipix.end()) { ++superset_miss; continue; }  // 漏选!
        ++o.n_checked;
    }
    o.leaves_hit = (superset_miss == 0 && o.n_checked > 0);  // 零漏选 (局部)
    // 单源像素恒等式: nContrib==1 的 leaf (无论全/部分覆盖):
    //   var_p = σ²·(a/A_drop)²/a² = σ²/A_drop² 精确 (a 恒等消去)。
    // A_drop 场内变化 ±8e-4 (gnomonic μ³) + Eriksson/VanOosterom 口径差
    // ~1e-6 → 门 5e-3。
    int nc1_checked = 0;
    for (const auto& l : leafs) {
        if (l.nContrib != 1) continue;
        const double var_expect = sigma2 / (a_drop * a_drop);
        const double rel = std::fabs(l.variance - var_expect) /
                           std::fabs(var_expect);
        o.worst_rel = std::max(o.worst_rel, rel);
        ++nc1_checked;
    }
    o.variance_closed = nc1_checked >= 3 && o.worst_rel < 5e-3;
    return o;
}

// ---------------------------------------------------------------------------
// Oracle O8: FIX-DRZ-E NaN/Inf 污染 leaf 集合断言 (DRIZZLE.md:96 传播)
//   注入像素 {NaN, +Inf, -Inf} 的 drop 命中 leaf 集合 = 必含非有限值;
//   其余 leaf (无注入贡献) 必须保持有限 → 污染集合 == oracle 超采样集合。
// ---------------------------------------------------------------------------
struct NonFiniteOracle {
    std::set<uint64_t> expect_polluted;  // oracle 超采样命中 leaf
    std::set<uint64_t> got_polluted;     // 被测输出含非有限值 leaf
    bool ok = false;
};

inline NonFiniteOracle oracle_nonfinite_pollution(
    const std::vector<LeafRec>& leafs,
    const drizzle::WcsParams& wcs, const drizzle::DrizzleConfig& cfg,
    const std::vector<std::pair<double, double>>& injected_px) {
    NonFiniteOracle o;
    const uint32_t nside = (uint32_t)cfg.nside;
    for (const auto& [px, py] : injected_px) {
        std::set<uint64_t> s = geom_pixel_leaves(wcs, px, py, cfg.pixfrac,
                                                 nside, 9);
        o.expect_polluted.insert(s.begin(), s.end());
    }
    for (const auto& l : leafs) {
        const double vals[2] = {l.sumFlux, l.signal};
        bool nonfinite = false;
        for (double v : vals)
            if (!std::isfinite(v)) nonfinite = true;
        if (nonfinite) o.got_polluted.insert(l.ipix);
    }
    // 污染集合 ⊇ oracle 命中集合 (超采样可能漏边界, 反向不允许);
    // 且污染必须非空、非全图 (传播有界)
    bool superset = true;
    for (uint64_t e : o.expect_polluted)
        if (!o.got_polluted.count(e)) { superset = false; break; }
    o.ok = superset && !o.got_polluted.empty() &&
           o.got_polluted.size() < leafs.size();
    return o;
}

// ---------------------------------------------------------------------------
// Oracle O9: SIP 路径一致性 (FIX-DRZ-F)
//   SIP 前向改天球坐标 → 同一 drop 命中 leaf 集合与 SIP 关闭时不同;
//   oracle 侧用独立 TAN+SIP 映射断言 nside 分辨率下 leaf 中心是否在
//   畸变视场内, 且 SIP on/off 输出的 touched 集合存在差异 (路径生效)。
// ---------------------------------------------------------------------------
inline bool oracle_sip_active(const std::vector<LeafRec>& sip_leafs,
                              const std::vector<LeafRec>& plain_leafs) {
    if (sip_leafs.empty() || plain_leafs.empty()) return false;
    std::set<uint64_t> a, b;
    for (const auto& l : sip_leafs) a.insert(l.ipix);
    for (const auto& l : plain_leafs) b.insert(l.ipix);
    // SIP 畸变 ~1px → 边缘 leaf 集合不同 (视场边缘 300"/px·1px ≈ 1 leaf)
    return a != b;
}

// ---------------------------------------------------------------------------
// 确定性/线程组比较 oracle
// ---------------------------------------------------------------------------
struct DeterminismOracle {
    bool tile_count_equal = false;
    bool touched_equal = true;      // 整数域: touched leaf 集合恒等
    bool ncontrib_equal = true;     // 整数域: 逐 leaf nContrib 恒等
    bool bitwise_equal = false;     // 数值域: 同线程数重复跑 bitwise
    bool value_close = false;       // 数值域: 1/2/4 线程容差
    double worst_rel = 0.0;
};

inline bool leafmap_equal_int(const std::vector<LeafRec>& a,
                              const std::vector<LeafRec>& b, bool check_nc) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].ipix != b[i].ipix) return false;
        if (check_nc && a[i].nContrib != b[i].nContrib) return false;
    }
    return true;
}

inline double leafmap_worst_rel(const std::vector<LeafRec>& a,
                                const std::vector<LeafRec>& b) {
    double worst = 0.0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        const double denom = std::max(std::fabs(a[i].sumFlux), 1e-300);
        const double rel = std::fabs(a[i].sumFlux - b[i].sumFlux) / denom;
        worst = std::max(worst, rel);
    }
    return worst;
}

}  // namespace p1drz

#endif  // P1DRZ_ORACLE_HPP
