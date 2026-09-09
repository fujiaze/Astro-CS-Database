// P1-PHOT-TEST · 独立 oracle (期望值不经被测函数生成)
//
// 模板 <prefix>-TEST 验收: "oracle 不调用被测函数、不复制同一实现; 可用
// 解析解、高精度朴素实现"。本头只依赖 C++ 标准库 (不含任何被测域 src 头,
// 不链接 star_matcher/spectrum_integrator/wcs_transform 符号); 全部期望值
// 由下列独立实现生成:
//   oracle_median / oracle_mad / oracle_irls_tukey — ALG-PHOT-001
//     (PHOTOMETRIC_FIT.md §13.1 冻结定义) 独立朴素重写, 供 SCI-PHOT-001 §11
//     "NumPy 参考复算 rtol 1e-9" 口径的 C++ 对应物;
//   oracle_wcs_pixel_to_sky / oracle_wcs_sky_to_pixel / oracle_eval_sip —
//     TAN (Calabretta & Greisen 2002 gnomonic 标准式) + SIP 多项式独立式;
//   oracle_akima / oracle_simpson / oracle_f_syn_xpsd — Akima 子样条
//     (Akima 1970) + Simpson 1/3 复合 (尾 3/8) + XPSD 线性解码端到端独立式
//     (ALG §13.1 F_syn 行, F(λ)=byte·flux_mul+flux_min);
//   oracle_bruteforce_match — O(n²) 暴力双向互最近邻唯一配对 (ALG-PHOT-002
//     定义, KD-tree 的独立对拍参照);
//   oracle_aperture_flux — 天空环中值背景 + 孔径和独立式 (README §9 合同)。
//
// 冻结常数 (SCI-PHOT-001 §10/ALG §13.1, 不得改动): _MAD_SCALE=0.6745,
// c=4.685, max_iter=50, tol=1e-6, mag_tolerance=3.0, match_radius=2.0px,
// F_syn 网格 1.0nm, Akima fill=0。
#ifndef P1PHOT_ORACLE_HPP
#define P1PHOT_ORACLE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace p1phot {
namespace oracle {

// ─── 基础统计 (独立实现; 不引用被测域符号) ─────────────────────────────

// 偶数样本取平均 (与被测 medianOf 同一离散合同 — 上中位口径会在偶数
// inlier 上产生 ~30% sigma 偏差, 属口径差非算法差)
inline double oracle_median_avg(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n == 0) return 0.0;
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline double oracle_median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline double oracle_mad(const std::vector<double>& v) {
    // (口径: oracle_median_avg — 同被测 medianOf)
    const double med = oracle_median(v);
    std::vector<double> dev;
    dev.reserve(v.size());
    for (double x : v) dev.push_back(std::fabs(x - med));
    return oracle_median_avg(dev);
}

// ALG-PHOT-001 IRLS-Tukey 稳健位置估计独立参考实现。
// 输入 r_i=log10(F_instr/F_syn) 的有效集 (调用方保证有限正值来源);
// 输出 location/sigma_residual/迭代次数/逐点 inlier 标记。
// 返回 false: 有效集为空 (无估计)。
inline bool oracle_irls_tukey(const std::vector<double>& r,
                              double& location_out, double& sigma_out,
                              int& iters_out, std::vector<char>* inlier_mask) {
    const double MAD_SCALE = 0.6745;   // ALG-PHOT-001 冻结
    const double TUKEY_C = 4.685;      // SCI-PHOT-001 §10 冻结
    const int MAX_ITER = 50;           // SCI-PHOT-001 §10 冻结
    const double CONVERGE = 1e-6;      // SCI-PHOT-001 §10 冻结

    if (inlier_mask) inlier_mask->assign(r.size(), 0);
    if (r.empty()) return false;

    double location = oracle_median_avg(r);
    const double S = (oracle_mad(r) > 0.0) ? oracle_mad(r) / MAD_SCALE : 0.0;
    int iters = 0;
    if (S <= 0.0) {
        // S=0 门: 所有 r 相同, 不迭代 (SCI-PHOT-001 §11 "S=0 门")
        location_out = location;
        sigma_out = 0.0;
        iters_out = 0;
        if (inlier_mask) inlier_mask->assign(r.size(), 1);
        return true;
    }
    double prev = location;
    for (int iter = 0; iter < MAX_ITER; ++iter) {
        iters = iter + 1;
        double sum_wr = 0.0, sum_w = 0.0;
        const double cS = TUKEY_C * S;
        for (double ri : r) {
            const double u = (ri - location) / cS;
            double w = 0.0;
            if (std::fabs(u) < 1.0) {
                const double t = 1.0 - u * u;
                w = t * t;  // Tukey biweight (1-u²)²
            }
            sum_wr += w * ri;
            sum_w += w;
        }
        if (sum_w <= 0.0) break;
        const double new_location = sum_wr / sum_w;
        const double diff = std::fabs(new_location - prev);
        location = new_location;
        if (diff < CONVERGE) break;
        prev = new_location;
    }
    // inliers: |u| < 1; sigma_residual = MAD(r_inliers)/0.6745
    const double cS = TUKEY_C * S;
    std::vector<double> r_in;
    for (std::size_t k = 0; k < r.size(); ++k) {
        const double u = (r[k] - location) / cS;
        if (std::fabs(u) < 1.0) {
            r_in.push_back(r[k]);
            if (inlier_mask) (*inlier_mask)[k] = 1;
        }
    }
    location_out = location;
    // sigma = median(|r_in − location|) 平均式偶数中位 / 0.6745 (同被测)
    sigma_out = 0.0;
    if (!r_in.empty()) {
        std::vector<double> dev;
        dev.reserve(r_in.size());
        for (double x : r_in) dev.push_back(std::fabs(x - location));
        sigma_out = oracle_median_avg(dev) / MAD_SCALE;
    }
    iters_out = iters;
    return true;
}

// ─── TAN + SIP 独立 WCS (Calabretta & Greisen 2002 标准式) ─────────────

constexpr double ORACLE_D2R = 3.14159265358979323846 / 180.0;
constexpr double ORACLE_R2D = 180.0 / 3.14159265358979323846;

struct OracleWcs {
    double crval1, crval2;      // 度
    double crpix1, crpix2;      // 1-based (FITS 约定)
    double cd11, cd12, cd21, cd22;
    int sip_order;              // [0,5]
    std::vector<double> a, b, ap, bp;  // 36 项 (i*6+j), 空表示无

    // SIP 多项式: Σ coeffs[i*6+j]·dx^i·dy^j, i+j<=order
    static double eval_sip(const std::vector<double>& c, double dx, double dy, int order) {
        if (order <= 0 || c.empty()) return 0.0;
        double s = 0.0;
        for (int i = 0; i <= order; ++i)
            for (int j = 0; j <= order - i; ++j)
                s += c[(std::size_t)i * 6 + j] * std::pow(dx, i) * std::pow(dy, j);
        return s;
    }

    // TAN 正投影: (ra,dec)度 → 中间坐标 (xi,eta)度 (gnomonic)
    void tan_world_to_intermediate(double ra, double dec, double& xi, double& eta) const {
        const double ra_rad = ra * ORACLE_D2R, dec_rad = dec * ORACLE_D2R;
        const double ra0_rad = crval1 * ORACLE_D2R, dec0_rad = crval2 * ORACLE_D2R;
        const double sdec0 = std::sin(dec0_rad), cdec0 = std::cos(dec0_rad);
        const double sdec = std::sin(dec_rad), cdec = std::cos(dec_rad);
        const double dra = ra_rad - ra0_rad;
        const double cosc = sdec0 * sdec + cdec0 * cdec * std::cos(dra);
        if (std::fabs(cosc) < 1e-12) {
            xi = 1e6; eta = 1e6;
            return;
        }
        xi = (cdec * std::sin(dra) / cosc) * ORACLE_R2D;
        eta = ((cdec0 * sdec - sdec0 * cdec * std::cos(dra)) / cosc) * ORACLE_R2D;
    }

    // TAN 反投影: (xi,eta)度 → (ra,dec)度
    void tan_intermediate_to_world(double xi, double eta, double& ra, double& dec) const {
        const double xi_rad = xi * ORACLE_D2R, eta_rad = eta * ORACLE_D2R;
        const double dec0_rad = crval2 * ORACLE_D2R;
        const double sdec0 = std::sin(dec0_rad), cdec0 = std::cos(dec0_rad);
        const double rho = std::sqrt(xi_rad * xi_rad + eta_rad * eta_rad);
        if (rho < 1e-12) {
            ra = crval1; dec = crval2;
            return;
        }
        const double c = std::atan(rho);
        const double sinc = std::sin(c), cosc = std::cos(c);
        double sin_dec = cosc * sdec0 + eta_rad * sinc * cdec0 / rho;
        sin_dec = std::min(1.0, std::max(-1.0, sin_dec));
        const double dec_rad = std::asin(sin_dec);
        const double dra = std::atan2(xi_rad * sinc,
                                      rho * cdec0 * cosc - eta_rad * sdec0 * sinc);
        double ra_rad = crval1 * ORACLE_D2R + dra;
        while (ra_rad < 0.0) ra_rad += 2 * 3.14159265358979323846;
        while (ra_rad >= 2 * 3.14159265358979323846) ra_rad -= 2 * 3.14159265358979323846;
        ra = ra_rad * ORACLE_R2D;
        dec = dec_rad * ORACLE_R2D;
    }

    // 像素(0-based) → 天球(度): SIP 前向 + CD + TAN 反投影
    void pixel_to_sky(double x, double y, double& ra, double& dec) const {
        double dx = x - (crpix1 - 1.0);
        double dy = y - (crpix2 - 1.0);
        if (sip_order > 0 && !a.empty() && !b.empty()) {
            dx += eval_sip(a, dx, dy, sip_order);
            dy += eval_sip(b, dx, dy, sip_order);
        }
        const double xi = cd11 * dx + cd12 * dy;
        const double eta = cd21 * dx + cd22 * dy;
        tan_intermediate_to_world(xi, eta, ra, dec);
    }

    // 天球(度) → 像素(0-based): TAN 正投影 + CD 逆 + 逆 SIP (AP/BP 直接式;
    // 无 AP/BP 时一次迭代近似, 与 ALG §13.1 skyToPixel 定义一致)
    void sky_to_pixel(double ra, double dec, double& x, double& y) const {
        double xi, eta;
        tan_world_to_intermediate(ra, dec, xi, eta);
        const double det = cd11 * cd22 - cd12 * cd21;
        double dx = (cd22 * xi - cd12 * eta) / det;
        double dy = (-cd21 * xi + cd11 * eta) / det;
        if (sip_order > 0 && !a.empty() && !b.empty()) {
            if (!ap.empty() && !bp.empty()) {
                dx += eval_sip(ap, dx, dy, sip_order);
                dy += eval_sip(bp, dx, dy, sip_order);
            } else {
                const double u = dx + eval_sip(a, dx, dy, sip_order);
                const double v = dy + eval_sip(b, dx, dy, sip_order);
                dx = u - eval_sip(a, u, v, sip_order);
                dy = v - eval_sip(b, u, v, sip_order);
            }
        }
        x = dx + (crpix1 - 1.0);
        y = dy + (crpix2 - 1.0);
    }
};

// ─── Akima 子样条 + Simpson (独立式; 与被测同一数学定义) ────────────────

// Akima 1970 子样条 (范围外 fill=0)。独立重写: 切线 t[i] 由加权差商
// (w1·m[i-1]+w2·m[i])/(w1+w2), w1=|m[i+1]-m[i]|, w2=|m[i-1]-m[i-2]|;
// 边界差商扩展 m[-1]=2m[0]-m[1], m[-2]=3m[0]-2m[1] (及右端对称)。
inline std::vector<double> oracle_akima(const std::vector<double>& xs,
                                        const std::vector<double>& ys,
                                        const std::vector<double>& dst,
                                        double fill = 0.0) {
    const std::size_t n = xs.size();
    std::vector<double> out(dst.size(), fill);
    if (n < 2 || dst.empty()) return out;
    // 区间差商 m[0..n-2]
    std::vector<double> m(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i) m[i] = (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]);
    auto ext = [&](long k) -> double {
        // m[j] 对 j<0 / j>n-2 的 Akima 线性外推 (n>=3); n==2 时退化复制
        if (k >= 0 && k <= (long)n - 2) return m[(std::size_t)k];
        if (n < 3) return m[0];
        if (k == -1) return 2.0 * m[0] - m[1];
        if (k == -2) return 3.0 * m[0] - 2.0 * m[1];
        if (k == (long)n - 1) return 2.0 * m[n - 2] - m[n - 3];
        return 3.0 * m[n - 2] - 2.0 * m[n - 3];  // k == n
    };
    std::vector<double> t(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double m_l2 = ext((long)i - 2), m_l1 = ext((long)i - 1);
        const double m_c = ext((long)i), m_r = ext((long)i + 1);
        const double w1 = std::fabs(m_r - m_c), w2 = std::fabs(m_l1 - m_l2);
        t[i] = (w1 + w2 == 0.0) ? 0.5 * (m_l1 + m_c) : (w1 * m_l1 + w2 * m_c) / (w1 + w2);
    }
    for (std::size_t k = 0; k < dst.size(); ++k) {
        const double x = dst[k];
        if (x < xs.front() || x > xs.back()) { out[k] = fill; continue; }
        std::size_t j = 0;
        std::size_t lo = 0, hi = n - 2;
        while (lo <= hi) {
            const std::size_t mid = (lo + hi) / 2;
            if (x >= xs[mid] && x <= xs[mid + 1]) { j = mid; break; }
            if (x < xs[mid]) { if (mid == 0) break; hi = mid - 1; }
            else lo = mid + 1;
        }
        const double dx = xs[j + 1] - xs[j];
        const double s = (x - xs[j]) / dx;
        const double h00 = (2.0 * s - 3.0) * s * s + 1.0;
        const double h10 = ((s - 2.0) * s + 1.0) * s;
        const double h01 = (-2.0 * s + 3.0) * s * s;
        const double h11 = (s - 1.0) * s * s;
        out[k] = h00 * ys[j] + h10 * dx * t[j] + h01 * ys[j + 1] + h11 * dx * t[j + 1];
    }
    return out;
}

// Simpson 1/3 复合 (等间距; 奇数区间末 3 用 3/8; 单区间梯形) — 独立重写
inline double oracle_simpson(const std::vector<double>& x, const std::vector<double>& y) {
    const std::size_t n_pts = x.size();
    if (n_pts < 2 || y.size() < n_pts) return 0.0;
    const double h = (x[n_pts - 1] - x[0]) / (double)(n_pts - 1);
    const std::size_t n_int = n_pts - 1;
    if (n_int % 2 == 0) {
        double s = y[0] + y[n_pts - 1];
        for (std::size_t i = 1; i + 1 < n_pts; ++i) s += (i % 2 == 1 ? 4.0 : 2.0) * y[i];
        return s * h / 3.0;
    }
    if (n_int >= 3) {
        const std::size_t n13 = n_int - 3;
        double s = y[0] + y[n13];
        for (std::size_t i = 1; i < n13; ++i) s += (i % 2 == 1 ? 4.0 : 2.0) * y[i];
        double total = s * h / 3.0;
        total += (y[n13] + 3.0 * y[n13 + 1] + 3.0 * y[n13 + 2] + y[n_pts - 1]) * 3.0 * h / 8.0;
        return total;
    }
    return 0.5 * h * (y[0] + y[1]);
}

// XPSD 端到端 F_syn 独立式: F(λ)=byte·flux_mul+flux_min (绝对谱辐照度),
// F_syn = ∫ F(λ)·T(λ)·Q(λ)·λ dλ, T/Q 由 oracle_akima 重采样到光谱网格 (fill=0,
// 无 QE 时 Q=1)。与 ALG §13.1 compute_f_syn_cached_xpsd 定义一致, 不调用被测。
inline double oracle_f_syn_xpsd(const std::vector<double>& spec_wl,
                                const std::vector<uint8_t>& spectrum,
                                const std::vector<double>& filter_wl,
                                const std::vector<double>& filter_trans,
                                const std::vector<double>& qe_wl,
                                const std::vector<double>& qe_trans,
                                double flux_min, double flux_mul) {
    const std::size_t n = spec_wl.size();
    if (n < 2 || spectrum.size() < n) return 0.0;
    std::vector<double> t_grid = oracle_akima(filter_wl, filter_trans, spec_wl, 0.0);
    std::vector<double> q_grid;
    const bool has_qe = !qe_wl.empty() && !qe_trans.empty();
    if (has_qe) q_grid = oracle_akima(qe_wl, qe_trans, spec_wl, 0.0);
    std::vector<double> integrand(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double f = (double)spectrum[i] * flux_mul + flux_min;
        const double q = has_qe ? q_grid[i] : 1.0;
        integrand[i] = f * t_grid[i] * q * spec_wl[i];
    }
    return oracle_simpson(spec_wl, integrand);
}

// 旧通道 (pc_calibrate_simple 直通) F_syn 独立式: F(λ)=byte·10^(-0.4·mag_g),
// F_syn = ∫ F·T·Q·λ dλ (mag 归一化)。被测 compute_f_syn 的积分域为
// SED/滤光片/QE 三曲线波长**交集子网格** (1.0nm 均匀, n=round(跨度/1.0)+1,
// ALG §13.1 "重叠区 1.0nm 网格" 定义) — oracle 独立实现同一离散化定义。
inline double oracle_f_syn_mag(const std::vector<double>& spec_wl,
                               const std::vector<uint8_t>& spectrum,
                               const std::vector<double>& filter_wl,
                               const std::vector<double>& filter_trans,
                               const std::vector<double>& qe_wl,
                               const std::vector<double>& qe_trans,
                               double mag_g) {
    const std::size_t n = spec_wl.size();
    if (n < 2 || spectrum.size() < n || filter_wl.size() < 2) return 0.0;
    const double mag_factor = std::pow(10.0, -0.4 * mag_g);
    // 交集子网格 (被测 compute_f_syn :226-243 定义)
    double wl_min = std::max(spec_wl.front(), filter_wl.front());
    double wl_max = std::min(spec_wl.back(), filter_wl.back());
    if (!qe_wl.empty() && !qe_trans.empty()) {
        wl_min = std::max(wl_min, qe_wl.front());
        wl_max = std::min(wl_max, qe_wl.back());
    }
    if (!(wl_min < wl_max)) return 0.0;
    const int n_points = (int)std::round((wl_max - wl_min) / 1.0) + 1;
    if (n_points < 2) return 0.0;
    std::vector<double> grid((std::size_t)n_points);
    for (int i = 0; i < n_points; ++i) grid[(std::size_t)i] = wl_min + (double)i * 1.0;
    // SED 值于网格: 线性插值 (子网格步长 1.0nm < 光谱步长 2.0nm, SED 原值
    // 定义在 spec_wl 上 — 被测走 Akima 插值; oracle 对 SED 用同 oracle_akima
    // 保证同一定义)
    std::vector<double> s_full((std::size_t)n);
    for (std::size_t i = 0; i < n; ++i) s_full[i] = (double)spectrum[i] * mag_factor;
    const std::vector<double> s_grid = oracle_akima(spec_wl, s_full, grid, 0.0);
    const std::vector<double> t_grid = oracle_akima(filter_wl, filter_trans, grid, 0.0);
    const bool has_qe = !qe_wl.empty() && !qe_trans.empty();
    std::vector<double> q_grid;
    if (has_qe) q_grid = oracle_akima(qe_wl, qe_trans, grid, 0.0);
    std::vector<double> integrand((std::size_t)n_points, 0.0);
    for (int i = 0; i < n_points; ++i) {
        const double q = has_qe ? q_grid[(std::size_t)i] : 1.0;
        integrand[(std::size_t)i] = s_grid[(std::size_t)i] * t_grid[(std::size_t)i] * q * grid[(std::size_t)i];
    }
    return oracle_simpson(grid, integrand);
}

// ─── O(n²) 暴力双向互最近邻唯一配对 (ALG-PHOT-002 独立参照) ─────────────

struct OracleMatch {
    int psf_idx;
    int gaia_idx;
    double dist;
};

// 输入: Gaia 像素坐标 (gx,gy), PSF 像素坐标 (px,py, 仅 status==0 有效)。
// 双向最近邻: 对每颗 PSF 找最近 Gaia (d²<r²), 对每颗 Gaia 找最近 PSF;
// 仅保留互为最近邻的对 (与 ALG §13.1 :263-333 定义一致)。
inline std::vector<OracleMatch> oracle_bruteforce_match(
    const std::vector<double>& gx, const std::vector<double>& gy,
    const std::vector<double>& px, const std::vector<double>& py,
    double radius_px) {
    const double r2 = radius_px * radius_px;
    const std::size_t ng = gx.size(), np = px.size();
    std::vector<int> fwd(np, -1);      // PSF→Gaia 最近
    std::vector<double> fwd_d2(np, 0.0);
    for (std::size_t k = 0; k < np; ++k) {
        double best = r2; int bi = -1;
        for (std::size_t g = 0; g < ng; ++g) {
            const double dx = px[k] - gx[g], dy = py[k] - gy[g];
            const double d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; bi = (int)g; }
        }
        if (bi >= 0) { fwd[k] = bi; fwd_d2[k] = best; }
    }
    std::vector<int> bwd(ng, -1);      // Gaia→PSF 最近
    for (std::size_t g = 0; g < ng; ++g) {
        double best = r2; int bi = -1;
        for (std::size_t k = 0; k < np; ++k) {
            const double dx = px[k] - gx[g], dy = py[k] - gy[g];
            const double d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; bi = (int)k; }
        }
        if (bi >= 0) bwd[g] = bi;
    }
    std::vector<OracleMatch> out;
    for (std::size_t k = 0; k < np; ++k) {
        if (fwd[k] < 0) continue;
        if (bwd[(std::size_t)fwd[k]] != (int)k) continue;  // 非互为最近邻 → 歧义拒绝
        out.push_back({(int)k, fwd[k], std::sqrt(fwd_d2[k])});
    }
    return out;
}

// ─── aperture 测光独立式 (README §9 / ALG §13.5 合同) ──────────────────

// 天空环 [inner,outer] **上中位**背景 (排序后 [n/2]; README §9 "背景中值"
// 的被测 photometer.cpp:47-51 合同口径 — 偶数个不取平均) + 孔径 d²≤r² 内
// Σ(pixel−background)。与被测 Photometer 同一合同定义的独立重写 (不 include
// 其头)。返回 false: 环内无像素或孔径空 (显式失败三态的 oracle 侧)。
inline bool oracle_aperture_flux(const float* image, int w, int h,
                                 double cx, double cy,
                                 double aperture_r, double sky_in, double sky_out,
                                 double& flux_out, double& background_out) {
    std::vector<double> sky;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            if (d >= sky_in && d <= sky_out)
                sky.push_back(image[(std::size_t)y * w + x]);
        }
    if (sky.empty()) return false;
    std::sort(sky.begin(), sky.end());
    const double bg = sky[sky.size() / 2];  // 上中位 (合同口径)
    background_out = bg;
    double sum = 0.0;
    int n_in = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d2 <= aperture_r * aperture_r) {
                sum += (double)image[(std::size_t)y * w + x] - bg;
                ++n_in;
            }
        }
    if (n_in <= 0) return false;
    flux_out = sum;
    return true;
}

}  // namespace oracle
}  // namespace p1phot

#endif  // P1PHOT_ORACLE_HPP
