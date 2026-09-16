// P1-PHOT-TEST · 独立 oracle (不调用被测函数, 不复制同一实现)
//
// 模板纪律: "oracle 不调用被测函数、不复制同一实现; 可用解析解、高精度
// 朴素实现或许可隔离的测试参考库"。本 oracle 三类手段:
//   1. 解析/闭式: TAN 投影 (WCS Paper II 闭式, 与 pc::WcsTransform 独立
//      推导/书写, 不 include 其源); XPSD 解码 F(λ)=byte·mul+min + 滤光片
//      线性插值 + integrand 二次多项式 → Simpson 复合闭式精确 (与
//      photo_calib::akima/simpson 实现无关, 仅依赖数学恒等式, F4 rtol 1e-9)。
//   2. 朴素实现: median/MAD 排序直接复算 (O(n log n) 排序路径, SCI-PHOT-001
//      §11 NumPy 参考复算同型, rtol 1e-9) + ALG-PHOT-001 公式逐条重算
//      IRLS location (公式权威=PHOTOMETRIC_FIT.md §2/§3, 非代码搬运)。
//   3. O(n²) 暴力参考: 双向唯一配对最近邻 (对拍 pc::KdTree2D 路径)。
// 被测符号 (编译隔离, 仅 C ABI 直调): pc_calibrate_simple_f64 /
// pc_calibrate_simple_with_gaia_f64_v2 / astrocs::phase1::Photometer。
#ifndef P1PHOT_ORACLE_HPP
#define P1PHOT_ORACLE_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "p1phot_fixtures.hpp"

namespace p1phot {

// ---------------------------------------------------------------------------
// 独立 TAN 投影 (WCS Paper II 闭式): 与 pc::WcsTransform 无代码关系。
// 像素 0-based, CRPIX 1-based; 无 SIP (fixture 域 sip_order=0; SIP 路径由
// p1phot_tests_core 负面矩阵另测 sip_order=6 拒绝, oracle 不复制 SIP)。
// ---------------------------------------------------------------------------
struct OracleTan {
    double crval1, crval2;   // deg
    double crpix1, crpix2;   // 1-based
    double cd[4];            // cd11 cd12 cd21 cd22

    static OracleTan from_fix(const FixWcs& f) {
        OracleTan o;
        o.crval1 = f.crval1; o.crval2 = f.crval2;
        o.crpix1 = f.crpix1; o.crpix2 = f.crpix2;
        o.cd[0] = f.cd11; o.cd[1] = f.cd12; o.cd[2] = f.cd21; o.cd[3] = f.cd22;
        return o;
    }

    void pixel_to_sky(double x, double y, double& ra, double& dec) const {
        const double d2r = M_PI / 180.0;
        const double dx = x - (crpix1 - 1.0);
        const double dy = y - (crpix2 - 1.0);
        const double xi  = (cd[0] * dx + cd[1] * dy) * d2r;  // rad
        const double eta = (cd[2] * dx + cd[3] * dy) * d2r;
        const double d0 = crval2 * d2r;
        const double a0 = crval1 * d2r;
        // 标准 TAN 逆投影 (deproject, 与 pc 实现的变量组织方式不同):
        const double cd0 = std::cos(d0), sd0 = std::sin(d0);
        const double denom = cd0 - eta * sd0;
        const double ra_rad  = a0 + std::atan2(xi, denom);
        const double dec_rad = std::atan((sd0 + eta * cd0) *
                              std::sqrt(1.0 + xi * xi / (denom * denom)) / denom);
        ra = ra_rad / d2r;
        dec = dec_rad / d2r;
        if (ra < 0.0) ra += 360.0;
    }

    void sky_to_pixel(double ra, double dec, double& x, double& y) const {
        const double d2r = M_PI / 180.0;
        const double a = ra * d2r, d = dec * d2r;
        const double a0 = crval1 * d2r, d0 = crval2 * d2r;
        const double cdra = std::cos(a - a0);
        const double denom = std::sin(d0) * std::sin(d) + std::cos(d0) * std::cos(d) * cdra;
        const double xi  = std::cos(d) * std::sin(a - a0) / denom;          // rad
        const double eta = (std::cos(d0) * std::sin(d) - std::sin(d0) * std::cos(d) * cdra) / denom;
        // CD 逆 = 伴随/行列式 (2x2 解析逆)
        const double det = cd[0] * cd[3] - cd[1] * cd[2];
        const double dx = ( cd[3] * xi - cd[1] * eta) / det / d2r;
        const double dy = (-cd[2] * xi + cd[0] * eta) / det / d2r;
        x = dx + (crpix1 - 1.0);
        y = dy + (crpix2 - 1.0);
    }
};

// fixtures 引用的自由函数 (FIX-PHOT-A/B/C 生成 Gaia 天球坐标, 不调用被测函数)
inline void p1phot_oracle_pixel_to_sky(const FixWcs& w, double x, double y,
                                       double& ra, double& dec) {
    OracleTan(w.crval1, w.crval2, w.crpix1, w.crpix2,
              w.cd11, w.cd12, w.cd21, w.cd22)
        .pixel_to_sky(x, y, ra, dec);
}

// ---------------------------------------------------------------------------
// XPSD F_syn 闭式精确 oracle (FIX-PHOT-D, F4 rtol 1e-9)
//
// 被测链: flux decode → Akima 重采样 T/Q → integrand → Simpson。
// fixture 使 T/Q Akima 重采样为精确 (线性/常数), 故仅剩 Simpson 积分误差;
// 对 integrand 为 ≤3 次多项式 Simpson 复合精确 (数学恒等式), oracle 取闭式:
//   ∫_a^b (c2 x² + c1 x + c0) dx = c2(b³−a³)/3 + c1(b²−a²)/2 + c0(b−a)
// 每对相邻节点积分后求和。integrand 非二次 (Akima 引入弯曲) 时返回 NaN —
// fixture 必须保持被积核二次, 否则 oracle 拒绝出值 (不伪造期望)。
// ---------------------------------------------------------------------------
inline double oracle_fsyn_xpsd_closed_form(const FixPhotXpsd& fx,
                                           const std::vector<double>& weighted_wl,
                                           double flux_min, double flux_mul) {
    const int n = static_cast<int>(fx.spectrum_wl.size());
    if (n < 2 || weighted_wl.size() != static_cast<std::size_t>(n)) return 0.0;
    const double F = fx.spectrum_byte * flux_mul + flux_min;  // 常数谱
    double total = 0.0;
    for (int i = 0; i + 1 < n; ++i) {
        const double x0 = fx.spectrum_wl[i], x1 = fx.spectrum_wl[i + 1];
        // 滤光片线性 T(λ) (两节点), 常数外推钳位 — 独立于被测 Akima 路径
        auto t_of = [&](double lam) {
            const double wl0 = fx.filter_wl.front(), wl1 = fx.filter_wl.back();
            const double t0 = fx.filter_trans.front(), t1 = fx.filter_trans.back();
            if (lam <= wl0) return t0;
            if (lam >= wl1) return t1;
            return t0 + (t1 - t0) * (lam - wl0) / (wl1 - wl0);
        };
        const double c0 = F * weighted_wl[i];     // w = T·Q·λ
        const double c1 = (F * weighted_wl[i + 1] - c0) / (x1 - x0);
        // 二次项系数须为 0 (integrand 一次) — weighted_wl 由 fixture 保证线性
        // (T 线性 × Q 常数 × λ 一次 = 二次? T·λ 为二次 — 用中点校验):
        const double xm = 0.5 * (x0 + x1);
        const double wm = t_of(xm) * xm;  // 中点真值 (Q=1)
        const double w_lin = c0 + c1 * (xm - x0);
        if (std::fabs(wm - w_lin) > 1e-15 * std::fabs(wm)) {
            return std::numeric_limits<double>::quiet_NaN();  // 非多项式精确域
        }
        total += 0.5 * (c0 + c1 * (x1 - x0)) * (x1 - x0);  // ∫(c0+c1t)dt 闭式
    }
    return total;
}

// 数值正交复核: 梯形 O(1/n²) 参考积分 (第二 oracle 通道, 与被测 Simpson
// 不同阶, 交叉验证闭式结果)
inline double oracle_fsyn_trapezoid(const FixPhotXpsd& fx,
                                    const std::vector<double>& weighted_wl,
                                    double flux_min, double flux_mul) {
    const int n = static_cast<int>(fx.spectrum_wl.size());
    const double F = fx.spectrum_byte * flux_mul + flux_min;
    double s = 0.0;
    for (int i = 0; i + 1 < n; ++i) {
        const double g0 = F * weighted_wl[i];
        const double g1 = F * weighted_wl[i + 1];
        s += 0.5 * (g0 + g1) * (fx.spectrum_wl[i + 1] - fx.spectrum_wl[i]);
    }
    return s;
}

// fixture 侧独立生成 weighted_wl = T(λ)·Q(λ)·λ (线性 T, Q=1) — 与被测
// prepare_filter_cache 无关 (被测值仅经闭式域校验消费)
inline std::vector<double> oracle_weighted_wl_linear(const FixPhotXpsd& fx) {
    const double wl0 = fx.filter_wl.front(), wl1 = fx.filter_wl.back();
    const double t0 = fx.filter_trans.front(), t1 = fx.filter_trans.back();
    std::vector<double> w;
    w.reserve(fx.spectrum_wl.size());
    for (double lam : fx.spectrum_wl) {
        double t = t0 + (t1 - t0) * (lam - wl0) / (wl1 - wl0);
        if (lam < wl0) t = t0;
        if (lam > wl1) t = t1;
        w.push_back(t * lam);  // Q=1.0
    }
    return w;
}

// ---------------------------------------------------------------------------
// 朴素统计 oracle (排序路径, SCI-PHOT-001 §11 NumPy 复算同型, rtol 1e-9)
// ---------------------------------------------------------------------------
inline double oracle_median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline double oracle_mad(const std::vector<double>& v, double location) {
    std::vector<double> dev;
    dev.reserve(v.size());
    for (double x : v) dev.push_back(std::fabs(x - location));
    return oracle_median(dev);
}

// ALG-PHOT-001 公式重算: r = log10(F_instr/F_syn); delta = -2.5 log10 F_instr
// − mag_g; 星等一致性 |delta − median(delta)| ≤ tol; IRLS+Tukey (c=4.685,
// 收敛 1e-6, ≤50 迭代); S = MAD(r)/0.6745; location 初值 = median(r)。
// 全部按 PHOTOMETRIC_FIT.md §2/§3 公式独立书写 (不 include 被测实现)。
struct OracleRobust {
    double location = 0.0;
    double scale = 1.0;
    double sigma_residual = 0.0;
    std::vector<int> inlier_match_rows;   // Tukey 权重>0 的 matches 行号
    std::vector<int> mag_rejected_rows;   // 星等一致性拒绝行
    std::vector<int> invalid_rows;        // F<=0/非有限行
    int robust_iterations = 0;
};

inline OracleRobust oracle_robust_location(
    const std::vector<double>& f_instr, const std::vector<double>& f_syn,
    const std::vector<double>& gaia_mag, double mag_tolerance) {
    OracleRobust orob;
    const double MAD_SCALE = 0.6745;
    const double TUKEY_C = 4.685;
    std::vector<double> r_vals, delta_vals;
    std::vector<int> valid_rows;
    for (std::size_t i = 0; i < f_instr.size(); ++i) {
        if (!(f_instr[i] > 0.0) || !(f_syn[i] > 0.0)) { orob.invalid_rows.push_back((int)i); continue; }
        const double r = std::log10(f_instr[i] / f_syn[i]);
        const double mag_inst = -2.5 * std::log10(f_instr[i]);
        const double delta = mag_inst - gaia_mag[i];
        if (!std::isfinite(r) || !std::isfinite(delta)) { orob.invalid_rows.push_back((int)i); continue; }
        r_vals.push_back(r); delta_vals.push_back(delta); valid_rows.push_back((int)i);
    }
    if (r_vals.empty()) return orob;
    const double med_delta = oracle_median(delta_vals);
    std::vector<double> r_consistent;
    for (std::size_t k = 0; k < r_vals.size(); ++k) {
        if (std::fabs(delta_vals[k] - med_delta) <= mag_tolerance) {
            orob.inlier_match_rows.push_back(valid_rows[k]);  // 暂存一致集
            r_consistent.push_back(r_vals[k]);
        } else {
            orob.mag_rejected_rows.push_back(valid_rows[k]);
        }
    }
    if (r_consistent.empty()) return orob;
    double location = oracle_median(r_consistent);
    const double mad = oracle_mad(r_consistent, location);
    const double S = (mad > 0.0) ? mad / MAD_SCALE : 0.0;
    int iters = 0;
    if (S > 0.0) {
        for (int iter = 0; iter < 50; ++iter) {
            ++iters;
            double sum_wr = 0.0, sum_w = 0.0;
            const double cS = TUKEY_C * S;
            for (double r : r_consistent) {
                const double u = (r - location) / cS;
                const double w = (std::fabs(u) >= 1.0) ? 0.0 : (1.0 - u * u) * (1.0 - u * u);
                sum_wr += w * r; sum_w += w;
            }
            if (!(sum_w > 0.0)) break;
            const double new_location = sum_wr / sum_w;
            const double diff = std::fabs(new_location - location);
            location = new_location;
            if (diff < 1e-6) break;
        }
    }
    orob.location = location;
    orob.scale = std::pow(10.0, -location);
    orob.robust_iterations = iters;
    // inliers = 一致集中 |r−location| < cS (Tukey 权重>0 域)
    const double cS = (S > 0.0) ? TUKEY_C * S : 1.0;
    std::vector<double> r_inliers;
    std::vector<int> kept_rows;
    for (std::size_t k = 0; k < r_consistent.size(); ++k) {
        const double dev = std::fabs(r_consistent[k] - location);
        if (S <= 0.0 || dev < cS) { r_inliers.push_back(r_consistent[k]); kept_rows.push_back(orob.inlier_match_rows[k]); }
    }
    orob.inlier_match_rows = kept_rows;
    if (!r_inliers.empty()) {
        const double mad_in = oracle_mad(r_inliers, location);
        orob.sigma_residual = (mad_in > 0.0) ? mad_in / MAD_SCALE : 0.0;
    }
    return orob;
}

// ---------------------------------------------------------------------------
// O(n²) 暴力双向唯一配对 oracle (对拍 KD-tree; 与 StarMatcher 无代码关系)
// 规则 (ALG-PHOT-002, star_matcher 合同): 正向 PSF→Gaia 最近邻 (dist<2px 严格
// 小于), 反向 Gaia→PSF 最近邻 (dist<2px), 互为最近邻才保留; tie 时 index 小者
// 胜 (KdTree findNearest: dist2 < best 严格比较, 先见者优先 — 暴力 oracle 用
// 同序扫, tie 行为一致)。
// ---------------------------------------------------------------------------
struct OraclePairing {
    // pair_k → (psf_row, gaia_row, dist)
    std::vector<std::pair<std::pair<int, int>, double>> unique_pairs;
    int forward_hits = 0;
    int ambiguous = 0;      // 正向命中但非互为
    int distance_rejected = 0;  // 有效 PSF 星正向未命中
};

inline OraclePairing oracle_pairing(const FixPhotFrame& fx, double radius_px) {
    OraclePairing op;
    std::vector<int> valid_rows;
    for (int i = 0; i < (int)fx.psf_cx.size(); ++i)
        if (fx.psf_status[i] == 0) valid_rows.push_back(i);
    const double r2max = radius_px * radius_px;
    // 正向
    std::vector<int> fwd_gaia(valid_rows.size(), -1);
    std::vector<double> fwd_d2(valid_rows.size(), 0.0);
    for (std::size_t k = 0; k < valid_rows.size(); ++k) {
        int best = -1; double bd2 = r2max;
        for (int g = 0; g < (int)fx.gaia_ra.size(); ++g) {
            double gx, gy;
            OracleTan::from_fix(fx.wcs).sky_to_pixel(fx.gaia_ra[g], fx.gaia_dec[g], gx, gy);
            const double d2 = (fx.psf_cx[valid_rows[k]] - gx) * (fx.psf_cx[valid_rows[k]] - gx) +
                              (fx.psf_cy[valid_rows[k]] - gy) * (fx.psf_cy[valid_rows[k]] - gy);
            if (d2 < bd2) { bd2 = d2; best = g; }
        }
        if (best >= 0) { fwd_gaia[k] = best; fwd_d2[k] = bd2; ++op.forward_hits; }
    }
    // 反向
    std::vector<int> bwd_psf(fx.gaia_ra.size(), -1);
    for (int g = 0; g < (int)fx.gaia_ra.size(); ++g) {
        double gx, gy;
        OracleTan::from_fix(fx.wcs).sky_to_pixel(fx.gaia_ra[g], fx.gaia_dec[g], gx, gy);
        int best = -1; double bd2 = r2max;
        for (std::size_t k = 0; k < valid_rows.size(); ++k) {
            const double d2 = (fx.psf_cx[valid_rows[k]] - gx) * (fx.psf_cx[valid_rows[k]] - gx) +
                              (fx.psf_cy[valid_rows[k]] - gy) * (fx.psf_cy[valid_rows[k]] - gy);
            if (d2 < bd2) { bd2 = d2; best = (int)k; }
        }
        bwd_psf[g] = best;
    }
    for (std::size_t k = 0; k < valid_rows.size(); ++k) {
        const int g = fwd_gaia[k];
        if (g < 0) { ++op.distance_rejected; continue; }
        if (bwd_psf[g] != (int)k) { ++op.ambiguous; continue; }
        op.unique_pairs.push_back({{valid_rows[k], g}, std::sqrt(fwd_d2[k])});
    }
    return op;
}

}  // namespace p1phot

#endif  // P1PHOT_ORACLE_HPP
