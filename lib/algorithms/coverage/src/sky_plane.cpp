// lib/algorithms/coverage/src/sky_plane.cpp — Phase2 稀疏天光面实现
//
// 语义与权威见 include/astro/phase2/sky_plane.h 文件头。
//
// 数值结构（为什么这样做）：
// - B_ref 用切平面上的张量积均匀 B 样条（阶数 d=1 或 3），只存 nx*ny 系数；
// - δ_k 用归一化切平面坐标的低阶多项式（阶数 p<=2），每帧 m 个系数；
// - 联合加权最小二乘对 δ_k 做 **按帧 Schur 消元**：
//       H_red = Σ_k (A_kᵀ W A_k − S_k M_k⁻¹ S_kᵀ),  S_k = A_kᵀ W P_k, M_k = P_kᵀ W P_k
//   H_red 只有 nx*ny 阶，因此求解内存与帧数无关，也不随像素数增长；
// - 粗糙度惩罚 λ·DᵀD（二阶差分）只加到求解矩阵，秩/κ 诊断用未惩罚数据矩阵；
//   该惩罚的零空间 = {1, ix, iy, ix·iy}（bilinear，4 维）；当数据权重与惩罚量级
//   悬殊时，惩罚矩阵的浮点舍入会淹没数据在该零空间上的曲率，使法方程失去正定性。
//   求解先走直接 Cholesky（常规档逐位不变）；失败时对零空间做极小 Tikhonov 锚并
//   用 deflation 校正扣回锚偏置（见 build 内注释），不改变科学解；
// - 稳健 IRLS：Huber 权重按标准化残差更新；
// - gauge：参考帧 δ≡0（reference_frame）或 δ 常数项和为零（sum）。
#include "astro/phase2/sky_plane.h"

#include "astrocs/probe.h"  // RELEASE-02 探针 (ASTROCS_PROBES=OFF 时宏为空语句)

#include "crypto/sha256.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMadToSigma = 1.482602218505602;   // 1/Φ⁻¹(3/4)
constexpr double kPiHalf = 1.57079632679489661923;  // π/2

inline bool finite_pos(double v) { return std::isfinite(v) && v > 0.0; }

inline double huber_w(double r, double d) {
    const double a = std::fabs(r);
    if (a <= d) return 1.0;
    return d / a;
}

// 偶数 n 取上下中位数平均；NaN 视为缺失（调用方过滤）。
double median_sorted_inplace(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    const std::size_t mid = n / 2;
    auto mid_it = v.begin() + static_cast<std::ptrdiff_t>(mid);
    std::nth_element(v.begin(), mid_it, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    const double b = *std::max_element(v.begin(), mid_it);
    return 0.5 * (a + b);
}

// 切平面（gnomonic）投影：返回 (u,v) 角（度）；cosc<=0 表示在背半球。
bool gnomonic(double ra0_deg, double dec0_deg, double ra_deg, double dec_deg,
              double* out_u_deg, double* out_v_deg, double* out_cosc) {
    const double d2r = kPi / 180.0;
    const double r0 = ra0_deg * d2r, d0 = dec0_deg * d2r;
    const double r = ra_deg * d2r, d = dec_deg * d2r;
    double dl = r - r0;
    while (dl > kPi) dl -= 2.0 * kPi;
    while (dl < -kPi) dl += 2.0 * kPi;
    const double cd = std::cos(d), sd = std::sin(d);
    const double cd0 = std::cos(d0), sd0 = std::sin(d0);
    const double cdl = std::cos(dl), sdl = std::sin(dl);
    const double cosc = sd0 * sd + cd0 * cd * cdl;
    if (out_cosc) *out_cosc = cosc;
    if (cosc <= 1e-12) return false;
    const double u = cd * sdl / cosc;
    const double v = (cd0 * sd - sd0 * cd * cdl) / cosc;
    if (out_u_deg) *out_u_deg = u / d2r;
    if (out_v_deg) *out_v_deg = v / d2r;
    return true;
}

// 均匀 B 样条基：节点 t_j = t0 + j*h，阶数 d，基函数个数 n_coef。
// 求值 u 处 d+1 个非零基函数：out_j[r] 为系数下标，out_n[r] 为值（r=0..d）。
void bspline_basis(double u, double t0, double h, int n_coef, int d,
                   int* out_j, double* out_n) {
    double s = std::floor((u - t0) / h);
    int span = static_cast<int>(s);
    if (span < d) span = d;
    if (span > n_coef - 1) span = n_coef - 1;
    double N[8];
    double left[8], right[8];
    N[0] = 1.0;
    for (int j = 1; j <= d; ++j) {
        left[j] = u - (t0 + static_cast<double>(span + 1 - j) * h);
        right[j] = (t0 + static_cast<double>(span + j) * h) - u;
        double saved = 0.0;
        for (int r = 0; r < j; ++r) {
            const double denom = right[r + 1] + left[j - r];
            const double temp = (denom != 0.0) ? N[r] / denom : 0.0;
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }
    for (int r = 0; r <= d; ++r) {
        out_j[r] = span - d + r;
        out_n[r] = N[r];
    }
}

int delta_basis_size(int order) { return (order + 1) * (order + 2) / 2; }

// 归一化多项式基：ξ=(u-uc)/us, η=(v-vc)/vs；顺序 (a+b) 升序，再 a 升序。
void delta_basis(double u, double v, double uc, double vc, double us, double vs,
                 int order, double* out) {
    const double x = (u - uc) / us;
    const double y = (v - vc) / vs;
    int k = 0;
    for (int total = 0; total <= order; ++total)
        for (int a = 0; a <= total; ++a) {
            const int b = total - a;
            out[k++] = std::pow(x, a) * std::pow(y, b);
        }
}

// 无 jitter Cholesky：A = L Lᵀ（A 对称 SPD）。返回 false 表示非 SPD。
bool chol_spd(const std::vector<double>& A, int n, std::vector<double>& L) {
    L.assign(static_cast<std::size_t>(n) * n, 0.0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = A[static_cast<std::size_t>(i) * n + j];
            for (int k = 0; k < j; ++k)
                sum -= L[static_cast<std::size_t>(i) * n + k] *
                       L[static_cast<std::size_t>(j) * n + k];
            if (i == j) {
                if (!(sum > 0.0) || !std::isfinite(sum)) return false;
                L[static_cast<std::size_t>(i) * n + i] = std::sqrt(sum);
            } else {
                const double d = L[static_cast<std::size_t>(j) * n + j];
                if (!(d > 0.0)) return false;
                L[static_cast<std::size_t>(i) * n + j] = sum / d;
            }
        }
    }
    return true;
}

void chol_solve(const std::vector<double>& L, int n, std::vector<double>& b) {
    std::vector<double> y(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double sum = b[static_cast<std::size_t>(i)];
        for (int k = 0; k < i; ++k)
            sum -= L[static_cast<std::size_t>(i) * n + k] * y[static_cast<std::size_t>(k)];
        y[static_cast<std::size_t>(i)] = sum / L[static_cast<std::size_t>(i) * n + i];
    }
    for (int i = n - 1; i >= 0; --i) {
        double sum = y[static_cast<std::size_t>(i)];
        for (int k = i + 1; k < n; ++k)
            sum -= L[static_cast<std::size_t>(k) * n + i] * b[static_cast<std::size_t>(k)];
        b[static_cast<std::size_t>(i)] = sum / L[static_cast<std::size_t>(i) * n + i];
    }
}

double sym_rayleigh(const std::vector<double>& A, int n, const std::vector<double>& x) {
    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; ++i) {
        double ax = 0.0;
        const double* row = &A[static_cast<std::size_t>(i) * n];
        for (int j = 0; j < n; ++j) ax += row[j] * x[static_cast<std::size_t>(j)];
        num += x[static_cast<std::size_t>(i)] * ax;
        den += x[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)];
    }
    return (den > 0.0) ? num / den : 0.0;
}

// λ_max（幂迭代）与 λ_min（逆幂迭代，用 Cholesky）。
double lambda_max_power(const std::vector<double>& A, int n) {
    std::vector<double> x(static_cast<std::size_t>(n), 1.0 / std::sqrt((double)n)), y(static_cast<std::size_t>(n));
    double prev = 0.0;
    for (int it = 0; it < 300; ++it) {
        for (int i = 0; i < n; ++i) {
            double s = 0.0;
            const double* row = &A[static_cast<std::size_t>(i) * n];
            for (int j = 0; j < n; ++j) s += row[j] * x[static_cast<std::size_t>(j)];
            y[static_cast<std::size_t>(i)] = s;
        }
        double norm = 0.0;
        for (int i = 0; i < n; ++i) norm += y[static_cast<std::size_t>(i)] * y[static_cast<std::size_t>(i)];
        norm = std::sqrt(norm);
        if (!(norm > 0.0) || !std::isfinite(norm)) break;
        for (int i = 0; i < n; ++i) x[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(i)] / norm;
        const double lam = sym_rayleigh(A, n, x);
        if (it > 3 && std::fabs(lam - prev) <= 1e-13 * std::max(1.0, std::fabs(lam))) { prev = lam; break; }
        prev = lam;
    }
    return sym_rayleigh(A, n, x);
}

double lambda_min_inverse(const std::vector<double>& A, const std::vector<double>& L, int n) {
    std::vector<double> x(static_cast<std::size_t>(n), 1.0 / std::sqrt((double)n));
    for (int it = 0; it < 300; ++it) {
        std::vector<double> w = x;
        chol_solve(L, n, w);   // w = A^-1 x
        double norm = 0.0;
        for (int i = 0; i < n; ++i) norm += w[static_cast<std::size_t>(i)] * w[static_cast<std::size_t>(i)];
        norm = std::sqrt(norm);
        if (!(norm > 0.0) || !std::isfinite(norm)) return 0.0;
        for (int i = 0; i < n; ++i) x[static_cast<std::size_t>(i)] = w[static_cast<std::size_t>(i)] / norm;
    }
    return sym_rayleigh(A, n, x);
}

struct SkyFrame {
    std::uint64_t frame_id = 0;
    std::vector<std::uint64_t> idx;   // usable sample indices
};

struct SkyPlaneModel {
    P2SkyPlaneConfig cfg{};
    P2SkyPlaneInfo info{};
    double ra0_deg = 0.0, dec0_deg = 0.0;
    double t0u = 0.0, t0v = 0.0, h = 1.0;
    int degree = 1;
    int nx = 0, ny = 0;
    int delta_order = 1;
    int m = 3;
    std::vector<double> coeff;         // B_ref nx*ny, row-major [iy*nx+ix]
    double gauge_shift = 0.0;
    double u_min = 0.0, u_max = 0.0, v_min = 0.0, v_max = 0.0;   // 采样包围盒
    double du_min = 0.0, du_max = 0.0, dv_min = 0.0, dv_max = 0.0; // 求值有效域
    double uc = 0.0, vc = 0.0, us = 1.0, vs = 1.0;               // δ 归一化
    std::vector<std::uint64_t> frame_ids;
    std::map<std::uint64_t, std::size_t> frame_index;
    std::vector<std::vector<double>> deltas;
    int ref_frame = 0;
    std::vector<double> last_weights;   // 最后使用的样本权重（诊断/残差）
    std::vector<std::uint64_t> used_idx;
};

void sky_plane_free(void* p) { delete static_cast<SkyPlaneModel*>(p); }

}  // namespace

extern "C" {

// ===========================================================================
// patch estimator / star mask
// ===========================================================================

P2SkyPatchConfig p2_sky_patch_default_config(void) {
    P2SkyPatchConfig c{};
    c.min_samples = 5;
    c.clip_iters = 3;
    c.clip_sigma = 3.0;
    c.contamination_sigma = 3.0;
    c.min_retained_fraction = 0.60;
    c.k_corr = 1.4;
    return c;
}

int p2_sky_patch_estimate(const double* values, const std::uint8_t* valid,
                          std::uint64_t n, const P2SkyPatchConfig* cfg_in,
                          P2SkyPatchEstimate* out,
                          char* err, std::size_t err_size) {
    if (!out) return P2_SKY_PATCH_INVALID_ARGS;
    std::memset(out, 0, sizeof(*out));
    if (!values || n == 0) {
        if (err && err_size) std::snprintf(err, err_size, "empty patch");
        out->status = P2_SKY_PATCH_INVALID_ARGS;
        return P2_SKY_PATCH_INVALID_ARGS;
    }
    P2SkyPatchConfig cfg = cfg_in ? *cfg_in : p2_sky_patch_default_config();
    if (cfg.min_samples < 1) cfg.min_samples = 1;
    if (cfg.clip_iters < 0) cfg.clip_iters = 0;
    if (!(cfg.clip_sigma > 0.0)) cfg.clip_sigma = 3.0;
    if (!(cfg.contamination_sigma > 0.0)) cfg.contamination_sigma = 3.0;
    if (!(cfg.min_retained_fraction > 0.0)) cfg.min_retained_fraction = 0.0;
    if (!(cfg.k_corr > 0.0)) cfg.k_corr = 1.4;

    std::vector<double> vals;
    vals.reserve(static_cast<std::size_t>(n));
    for (std::uint64_t i = 0; i < n; ++i) {
        if (valid && valid[i] == 0) continue;
        const double v = values[i];
        if (!std::isfinite(v)) continue;
        vals.push_back(v);
    }
    const int n_total = static_cast<int>(vals.size());
    out->n_total = n_total;
    if (n_total < cfg.min_samples) {
        if (err && err_size) std::snprintf(err, err_size, "n_total=%d < min_samples=%d", n_total, cfg.min_samples);
        out->status = P2_SKY_PATCH_INSUFFICIENT_SAMPLES;
        return P2_SKY_PATCH_INSUFFICIENT_SAMPLES;
    }
    double m0 = median_sorted_inplace(vals);
    {
        std::vector<double> dev;
        dev.reserve(vals.size());
        for (double v : vals) dev.push_back(std::fabs(v - m0));
        double s0 = kMadToSigma * median_sorted_inplace(dev);
        std::vector<double> ret = vals;
        for (int it = 0; it < cfg.clip_iters; ++it) {
            std::vector<double> nr;
            nr.reserve(ret.size());
            for (double v : ret)
                if (v <= m0 + cfg.clip_sigma * s0) nr.push_back(v);
            if (static_cast<int>(nr.size()) < cfg.min_samples) break;
            const double nm = median_sorted_inplace(nr);
            if (std::fabs(nm - m0) < 1e-12 * std::max(std::fabs(m0), 1e-12)) { ret = nr; break; }
            m0 = nm;
            ret = std::move(nr);
            std::vector<double> dev2;
            dev2.reserve(ret.size());
            for (double v : ret) dev2.push_back(std::fabs(v - m0));
            const double s1 = kMadToSigma * median_sorted_inplace(dev2);
            if (s1 <= 0.0) break;
            s0 = s1;
        }
        const double y = m0;
        const double sigma = (s0 > 0.0) ? s0 : 1e-12;
        int nbright = 0;
        for (double v : vals)
            if (v > y + cfg.contamination_sigma * sigma) ++nbright;
        const int n_retained = static_cast<int>(ret.size());
        out->value = y;
        out->sigma_mad = sigma;
        out->n_retained = n_retained;
        out->bright_fraction = static_cast<double>(nbright) / static_cast<double>(n_total);
        const double n_ret = std::max(static_cast<double>(n_retained), 1.0);
        out->variance = cfg.k_corr * kPiHalf * sigma * sigma / n_ret;
        out->ivar = (out->variance > 0.0) ? 1.0 / out->variance : 0.0;
        out->uncertainty = std::sqrt(out->variance);
        if (static_cast<double>(n_retained) < cfg.min_retained_fraction * static_cast<double>(n_total)) {
            out->status = P2_SKY_PATCH_INSUFFICIENT_RETAINED;
            if (err && err_size)
                std::snprintf(err, err_size, "n_retained=%d < %.2f*n_total=%d",
                              n_retained, cfg.min_retained_fraction, n_total);
            return out->status;
        }
        if (out->bright_fraction > 0.20) {
            out->status = P2_SKY_PATCH_HIGH_CONTAMINATION;
            if (err && err_size) std::snprintf(err, err_size, "bright_fraction=%.3f", out->bright_fraction);
            return out->status;
        }
        out->status = P2_SKY_PATCH_OK;
        return P2_SKY_PATCH_OK;
    }
}

int p2_star_mask_caps(const double* ra_deg, const double* dec_deg,
                      const double* snr, std::uint64_t n,
                      double snr_threshold, double radius_deg,
                      P2StarMaskCap* out, std::uint64_t cap,
                      std::uint64_t* out_n) {
    if (!ra_deg || !dec_deg || !snr || n == 0) return 1;
    if (!(radius_deg > 0.0) || !std::isfinite(radius_deg)) return 1;
    std::uint64_t k = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        if (!std::isfinite(ra_deg[i]) || !std::isfinite(dec_deg[i]) || !std::isfinite(snr[i])) continue;
        if (!(snr[i] > snr_threshold)) continue;
        if (out && k < cap) {
            out[k].ra_deg = ra_deg[i];
            out[k].dec_deg = dec_deg[i];
            out[k].radius_deg = radius_deg;
            out[k].kind = P2_STAR_MASK_STAR;
        }
        ++k;
    }
    if (out_n) *out_n = k;
    return 0;
}

int p2_star_mask_contains(const P2StarMaskCap* caps, std::uint64_t n,
                          double ra_deg, double dec_deg) {
    if (!caps) return -1;
    if (!std::isfinite(ra_deg) || !std::isfinite(dec_deg)) return -1;
    const double d2r = kPi / 180.0;
    const double r = ra_deg * d2r, d = dec_deg * d2r;
    const double sd = std::sin(d), cd = std::cos(d);
    for (std::uint64_t i = 0; i < n; ++i) {
        const double c0 = caps[i].ra_deg * d2r, d0 = caps[i].dec_deg * d2r;
        const double rad = caps[i].radius_deg * d2r;
        double c = std::sin(d0) * sd + std::cos(d0) * cd * std::cos(r - c0);
        c = std::min(1.0, std::max(-1.0, c));
        if (std::acos(c) <= rad) return 1;
    }
    return 0;
}

// ===========================================================================
// sky plane
// ===========================================================================

P2SkyPlaneConfig p2_sky_plane_default_config(void) {
    P2SkyPlaneConfig c{};
    c.spline_degree = 3;   // 研究 Q4：三次张量 B 样条条件数 ~30 且与节点数无关
    c.node_spacing_deg = 1.0;
    c.frame_gradient_order = 1;
    c.roughness_penalty = 1e-3;
    c.huber_delta = 1.345;
    c.max_iterations = 30;
    c.tolerance = 1e-10;
    c.gauge_mode = 0;
    c.weight_mode = 0;
    c.kappa_max = 1e8;
    c.rank_rtol = 1e-10;
    c.min_samples = 8;
    c.min_samples_per_frame = 4;
    c.max_nodes = 2048;
    c.max_extrapolation_deg = 0.0;
    // 无 worker 数字段：本求解内在串行（Schur 消元 + 稳健 IRLS 整面一次），
    // 不设并行路径；将来并行化须由 Runtime 预算/租约注入（QA-002/P2-002）。
    return c;
}

int p2_sky_plane_build(const P2SkySample* samples, std::uint64_t n,
                       const P2SkyPlaneConfig* cfg_in, void** out_model,
                       char* err, std::size_t err_size) {
    if (!out_model || !samples || n == 0) return P2_SKY_PLANE_INVALID_ARGS;
    // [RELEASE-02 probe] Phase2 天光面构建 (整面一次; RAII 覆盖所有 return)
    ASTROCS_PROBE_SCOPE("phase2", "sky_plane.build");
    ASTROCS_PROBE_GAUGE("phase2", "sky_plane.build_samples", static_cast<double>(n));
    *out_model = nullptr;
    P2SkyPlaneConfig cfg = cfg_in ? *cfg_in : p2_sky_plane_default_config();
    if (cfg.spline_degree != 1 && cfg.spline_degree != 3) cfg.spline_degree = 1;
    if (!(cfg.node_spacing_deg > 0.0) || !std::isfinite(cfg.node_spacing_deg)) cfg.node_spacing_deg = 1.0;
    if (cfg.frame_gradient_order < 0) cfg.frame_gradient_order = 0;
    if (cfg.frame_gradient_order > 2) cfg.frame_gradient_order = 2;
    if (!(cfg.huber_delta > 0.0)) cfg.huber_delta = 1.345;
    if (cfg.max_iterations < 1) cfg.max_iterations = 1;
    if (!(cfg.tolerance > 0.0)) cfg.tolerance = 1e-10;
    if (!(cfg.kappa_max > 0.0)) cfg.kappa_max = 1e8;
    if (!(cfg.rank_rtol > 0.0)) cfg.rank_rtol = 1e-10;
    if (cfg.min_samples < 1) cfg.min_samples = 1;
    if (cfg.min_samples_per_frame < 1) cfg.min_samples_per_frame = 1;
    if (cfg.max_nodes < 4) cfg.max_nodes = 4;
    if (!(cfg.roughness_penalty >= 0.0)) cfg.roughness_penalty = 0.0;
    if (cfg.gauge_mode != 0 && cfg.gauge_mode != 1) cfg.gauge_mode = 0;
    if (cfg.weight_mode != 0 && cfg.weight_mode != 1) cfg.weight_mode = 0;
    if (cfg.max_extrapolation_deg < 0.0) cfg.max_extrapolation_deg = 0.0;

    // ---- 采样点过滤 ----
    std::vector<std::uint64_t> used;
    used.reserve(static_cast<std::size_t>(n));
    std::uint64_t n_masked = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        const P2SkySample& s = samples[i];
        const std::uint32_t bad = P2_SKY_FLAG_MASKED | P2_SKY_FLAG_LOW_SUPPORT |
                                  P2_SKY_FLAG_HIGH_CONTAMINATION | P2_SKY_FLAG_REJECTED;
        if (s.flags & bad) { ++n_masked; continue; }
        if (!std::isfinite(s.ra_deg) || !std::isfinite(s.dec_deg) || !std::isfinite(s.value)) { ++n_masked; continue; }
        if (cfg.weight_mode == 0) {
            if (!finite_pos(s.variance)) { ++n_masked; continue; }
        } else {
            if (!finite_pos(s.snr)) { ++n_masked; continue; }
        }
        used.push_back(i);
    }
    const std::uint64_t n_used0 = used.size();
    if (n_used0 == 0) {
        if (err && err_size) std::snprintf(err, err_size, "no usable sky samples (n=%llu, masked=%llu)",
                                           (unsigned long long)n, (unsigned long long)n_masked);
        return P2_SKY_PLANE_NO_USABLE_SAMPLES;
    }
    if (n_used0 < static_cast<std::uint64_t>(cfg.min_samples)) {
        if (err && err_size) std::snprintf(err, err_size, "n_used=%llu < min_samples=%d",
                                           (unsigned long long)n_used0, cfg.min_samples);
        return P2_SKY_PLANE_TOO_FEW_SAMPLES;
    }

    // ---- 帧分组（升序，确定性） ----
    std::vector<std::uint64_t> frame_ids;
    for (std::uint64_t i : used) frame_ids.push_back(samples[i].frame_id);
    std::sort(frame_ids.begin(), frame_ids.end());
    frame_ids.erase(std::unique(frame_ids.begin(), frame_ids.end()), frame_ids.end());
    const int n_frames = static_cast<int>(frame_ids.size());
    std::map<std::uint64_t, int> frame_pos;
    for (int k = 0; k < n_frames; ++k) frame_pos[frame_ids[static_cast<std::size_t>(k)]] = k;

    const int d = cfg.spline_degree;
    const int order = cfg.frame_gradient_order;
    const int m = delta_basis_size(order);
    if (m < 1) return P2_SKY_PLANE_INVALID_ARGS;

    std::vector<SkyFrame> frames(static_cast<std::size_t>(n_frames));
    for (int k = 0; k < n_frames; ++k) frames[static_cast<std::size_t>(k)].frame_id = frame_ids[static_cast<std::size_t>(k)];
    for (std::uint64_t i : used)
        frames[static_cast<std::size_t>(frame_pos[samples[i].frame_id])].idx.push_back(i);

    // 每帧 δ 可辨识性：n_k >= max(m, min_samples_per_frame)
    const std::uint64_t need_k = static_cast<std::uint64_t>(
        std::max<int>(m, cfg.min_samples_per_frame));
    for (int k = 0; k < n_frames; ++k) {
        if (frames[static_cast<std::size_t>(k)].idx.size() < need_k) {
            if (err && err_size)
                std::snprintf(err, err_size,
                              "frame %llu has %llu samples < required %llu (delta order %d)",
                              (unsigned long long)frames[static_cast<std::size_t>(k)].frame_id,
                              (unsigned long long)frames[static_cast<std::size_t>(k)].idx.size(),
                              (unsigned long long)need_k, order);
            return P2_SKY_PLANE_FRAME_UNDERDETERMINED;
        }
    }

    SkyPlaneModel* model = new (std::nothrow) SkyPlaneModel();
    if (!model) return P2_SKY_PLANE_INVALID_ARGS;
    model->cfg = cfg;
    model->degree = d;
    model->delta_order = order;
    model->m = m;

    // ---- 切平面中心 = 采样点球面均值方向（确定性） ----
    {
        double sx = 0, sy = 0, sz = 0;
        for (std::uint64_t i : used) {
            const double d2r = kPi / 180.0;
            const double r = samples[i].ra_deg * d2r, dd = samples[i].dec_deg * d2r;
            sx += std::cos(dd) * std::cos(r);
            sy += std::cos(dd) * std::sin(r);
            sz += std::sin(dd);
        }
        const double nrm = std::sqrt(sx * sx + sy * sy + sz * sz);
        if (!(nrm > 0.0)) { delete model; return P2_SKY_PLANE_INVALID_ARGS; }
        sx /= nrm; sy /= nrm; sz /= nrm;
        model->dec0_deg = std::asin(std::min(1.0, std::max(-1.0, sz))) * 180.0 / kPi;
        model->ra0_deg = std::atan2(sy, sx) * 180.0 / kPi;
        if (model->ra0_deg < 0.0) model->ra0_deg += 360.0;
    }

    // ---- 投影到切平面，确定网格 ----
    std::vector<double> su(used.size()), sv(used.size());
    {
        double umin = std::numeric_limits<double>::infinity(), umax = -umin;
        double vmin = umin, vmax = -umin;
        for (std::size_t t = 0; t < used.size(); ++t) {
            double u = 0, v = 0, cosc = 0;
            if (!gnomonic(model->ra0_deg, model->dec0_deg, samples[used[t]].ra_deg,
                          samples[used[t]].dec_deg, &u, &v, &cosc)) {
                delete model;
                if (err && err_size) std::snprintf(err, err_size, "sample outside tangent hemisphere");
                return P2_SKY_PLANE_INVALID_ARGS;
            }
            su[t] = u; sv[t] = v;
            umin = std::min(umin, u); umax = std::max(umax, u);
            vmin = std::min(vmin, v); vmax = std::max(vmax, v);
        }
        // 节点间距取用户显式值（fail-closed：n_nodes 超 max_nodes 由下方守卫拒绝，
        // 不静默放粗，避免掩盖欠定样条）。
        const double h = cfg.node_spacing_deg;
        // 最小跨度 = h，避免退化网格
        if (umax - umin < h) { const double c = 0.5 * (umin + umax); umin = c - 0.5 * h; umax = c + 0.5 * h; }
        if (vmax - vmin < h) { const double c = 0.5 * (vmin + vmax); vmin = c - 0.5 * h; vmax = c + 0.5 * h; }
        model->u_min = umin; model->u_max = umax; model->v_min = vmin; model->v_max = vmax;
        model->h = h;
        model->t0u = umin - static_cast<double>(d) * h;
        model->t0v = vmin - static_cast<double>(d) * h;
        model->nx = static_cast<int>(std::ceil((umax - umin) / h)) + d;
        model->ny = static_cast<int>(std::ceil((vmax - vmin) / h)) + d;
        if (model->nx < d + 1) model->nx = d + 1;
        if (model->ny < d + 1) model->ny = d + 1;
        const std::uint64_t n_nodes = static_cast<std::uint64_t>(model->nx) * static_cast<std::uint64_t>(model->ny);
        if (n_nodes > static_cast<std::uint64_t>(cfg.max_nodes)) {
            delete model;
            if (err && err_size)
                std::snprintf(err, err_size, "n_nodes=%llu > max_nodes=%d (increase node_spacing_deg)",
                              (unsigned long long)n_nodes, cfg.max_nodes);
            return P2_SKY_PLANE_TOO_MANY_NODES;
        }
        model->du_min = model->t0u + static_cast<double>(d) * h;
        model->du_max = model->t0u + static_cast<double>(model->nx) * h;
        model->dv_min = model->t0v + static_cast<double>(d) * h;
        model->dv_max = model->t0v + static_cast<double>(model->ny) * h;
        // δ 归一化
        model->uc = 0.5 * (umin + umax);
        model->vc = 0.5 * (vmin + vmax);
        model->us = std::max(0.5 * (umax - umin), 1e-9);
        model->vs = std::max(0.5 * (vmax - vmin), 1e-9);
    }
    const int n_full = model->nx * model->ny;
    model->frame_ids = frame_ids;
    for (int k = 0; k < n_frames; ++k) model->frame_index[frame_ids[static_cast<std::size_t>(k)]] = static_cast<std::size_t>(k);
    model->ref_frame = 0;   // 最小 frame_id
    model->deltas.assign(static_cast<std::size_t>(n_frames), std::vector<double>(static_cast<std::size_t>(m), 0.0));
    model->coeff.assign(static_cast<std::size_t>(n_full), 0.0);
    model->used_idx = used;

    // ---- 基础权重 ----
    std::vector<double> base_w(used.size(), 0.0);
    for (std::size_t t = 0; t < used.size(); ++t) {
        const P2SkySample& s = samples[used[t]];
        if (cfg.weight_mode == 0) base_w[t] = 1.0 / s.variance;
        else base_w[t] = s.snr * s.snr;
    }
    std::vector<double> w = base_w;

    // 每帧 δ 归一化坐标
    std::vector<std::vector<double>> pu(used.size());
    for (std::size_t t = 0; t < used.size(); ++t) {
        pu[t].resize(static_cast<std::size_t>(m));
        delta_basis(su[t], sv[t], model->uc, model->vc, model->us, model->vs, order, pu[t].data());
    }
    // B 样条行（稀疏）：每点 (d+1)² 项；同时统计每节点数据支撑。
    // 切平面 bbox 的角点可能无采样（球面矩形投影后为曲边四边形）：
    // 无支撑节点不进入自由参数集（由粗糙度/最近邻填充），否则正规矩阵奇异。
    const int nb = (d + 1) * (d + 1);
    std::vector<int> bidx(used.size() * static_cast<std::size_t>(nb), 0);
    std::vector<double> bval(used.size() * static_cast<std::size_t>(nb), 0.0);
    std::vector<double> node_weight(static_cast<std::size_t>(n_full), 0.0);
    for (std::size_t t = 0; t < used.size(); ++t) {
        int jx[8]; double nxv[8];
        int jy[8]; double nyv[8];
        bspline_basis(su[t], model->t0u, model->h, model->nx, d, jx, nxv);
        bspline_basis(sv[t], model->t0v, model->h, model->ny, d, jy, nyv);
        int c = 0;
        for (int a = 0; a <= d; ++a)
            for (int b = 0; b <= d; ++b) {
                const int idx = jy[a] * model->nx + jx[b];
                bidx[t * static_cast<std::size_t>(nb) + static_cast<std::size_t>(c)] = idx;
                const double bvv = nyv[a] * nxv[b];
                bval[t * static_cast<std::size_t>(nb) + static_cast<std::size_t>(c)] = bvv;
                node_weight[static_cast<std::size_t>(idx)] += bvv * bvv;
                ++c;
            }
    }
    double max_node_weight = 0.0;
    for (int i = 0; i < n_full; ++i)
        max_node_weight = std::max(max_node_weight, node_weight[static_cast<std::size_t>(i)]);
    // 自由节点门：Σ b² 相对最大节点 > support_rtol。仅"被 stencil 触及但基函数
    // 在采样点近零"的 bbox 边界节点会被剔除（否则列近零 → 正规矩阵近奇异）。
    const double support_floor = 1e-9 * std::max(max_node_weight, 1e-300);
    std::vector<int> free_of_full(static_cast<std::size_t>(n_full), -1);
    std::vector<int> full_of_free;
    full_of_free.reserve(static_cast<std::size_t>(n_full));
    for (int i = 0; i < n_full; ++i)
        if (node_weight[static_cast<std::size_t>(i)] > support_floor) {
            free_of_full[static_cast<std::size_t>(i)] = static_cast<int>(full_of_free.size());
            full_of_free.push_back(i);
        }
    const int n_free = static_cast<int>(full_of_free.size());
    if (n_free < 4 || n_free < (d + 1) * (d + 1)) {
        sky_plane_free(model);
        if (err && err_size) std::snprintf(err, err_size, "data-supported nodes n_free=%d too few (n_full=%d)", n_free, n_full);
        return P2_SKY_PLANE_TOO_FEW_SAMPLES;
    }
    std::vector<int> bfree(used.size() * static_cast<std::size_t>(nb), -1);
    for (std::size_t e = 0; e < bfree.size(); ++e)
        bfree[e] = free_of_full[static_cast<std::size_t>(bidx[e])];

    // ---- 惩罚零空间基（bilinear: {1, ix, iy, ix*iy}）----
    // 两个方向的二阶差分 stencil（v={1,-2,1}）各自湮灭"关于该方向仿射"的系数向量，
    // 公共零空间 = 关于 ix、iy 均仿射的 bilinear 系数空间 a + b·ix + c·iy + d·ix·iy
    // （4 维；在均匀张量 B 样条下等价于函数空间 {1, u, v, u·v}）。
    // 注意：它比 11_upm.md §4.3 的 δ_k 一次多项式 gauge（3 维 {1, ξ, η}）**多一维**——
    // δ_k 只到一阶、无法吸收 bilinear 项，故 bilinear 是真实模型方向，由数据决定，
    // 不是规范自由度。这里只用于：(i) 求解前把惩罚零空间方向锚成正定；(ii) 求解后
    // 用 deflation 校正精确抵消锚偏置。两处都只作用在该 4 维零空间上。
    std::vector<double> gauge_q;   // n_free x mq，行主序 [f*mq + k]
    int mq = 0;
    {
        std::vector<std::vector<double>> cols;
        for (int c = 0; c < 4; ++c) {
            std::vector<double> v(static_cast<std::size_t>(n_free), 0.0);
            for (int f = 0; f < n_free; ++f) {
                const int idx = full_of_free[static_cast<std::size_t>(f)];
                const double ix = static_cast<double>(idx % model->nx);
                const double iy = static_cast<double>(idx / model->nx);
                v[static_cast<std::size_t>(f)] =
                    (c == 0) ? 1.0 : (c == 1) ? ix : (c == 2) ? iy : ix * iy;
            }
            double orig = 0.0;
            for (double x : v) orig += x * x;
            orig = std::sqrt(orig);
            for (const auto& q : cols) {   // 修正 Gram-Schmidt（固定列序，确定性）
                double d = 0.0;
                for (int f = 0; f < n_free; ++f)
                    d += q[static_cast<std::size_t>(f)] * v[static_cast<std::size_t>(f)];
                for (int f = 0; f < n_free; ++f)
                    v[static_cast<std::size_t>(f)] -= d * q[static_cast<std::size_t>(f)];
            }
            double nrm = 0.0;
            for (double x : v) nrm += x * x;
            nrm = std::sqrt(nrm);
            if (orig > 0.0 && nrm > 1e-9 * orig) {
                for (double& x : v) x /= nrm;
                cols.push_back(std::move(v));
            }
        }
        mq = static_cast<int>(cols.size());
        gauge_q.assign(static_cast<std::size_t>(n_free) * static_cast<std::size_t>(mq), 0.0);
        for (int k = 0; k < mq; ++k)
            for (int f = 0; f < n_free; ++f)
                gauge_q[static_cast<std::size_t>(f) * static_cast<std::size_t>(mq) +
                        static_cast<std::size_t>(k)] =
                    cols[static_cast<std::size_t>(k)][static_cast<std::size_t>(f)];
    }

    // ---- 稳健 IRLS ----
    std::vector<double> H_data(static_cast<std::size_t>(n_free) * n_free, 0.0);
    std::vector<double> H_red(static_cast<std::size_t>(n_free) * n_free, 0.0);
    std::vector<double> H_solve;
    std::vector<double> rhs(static_cast<std::size_t>(n_free), 0.0);
    std::vector<double> L;
    std::vector<double> B(static_cast<std::size_t>(n_free), 0.0);
    int iterations = 0;
    for (int iter = 0; iter < cfg.max_iterations; ++iter) {
        ++iterations;
        std::fill(H_data.begin(), H_data.end(), 0.0);
        std::fill(rhs.begin(), rhs.end(), 0.0);
        std::vector<std::vector<double>> Mk(static_cast<std::size_t>(n_frames));
        std::vector<std::vector<double>> Sk(static_cast<std::size_t>(n_frames));
        std::vector<std::vector<double>> tk(static_cast<std::size_t>(n_frames));
        for (int k = 0; k < n_frames; ++k) {
            Mk[static_cast<std::size_t>(k)].assign(static_cast<std::size_t>(m) * m, 0.0);
            Sk[static_cast<std::size_t>(k)].assign(static_cast<std::size_t>(n_free) * m, 0.0);
            tk[static_cast<std::size_t>(k)].assign(static_cast<std::size_t>(m), 0.0);
        }
        for (int k = 0; k < n_frames; ++k) {
            const bool ref = (k == model->ref_frame);
            std::vector<double>& Mk_ = Mk[static_cast<std::size_t>(k)];
            std::vector<double>& Sk_ = Sk[static_cast<std::size_t>(k)];
            std::vector<double>& tk_ = tk[static_cast<std::size_t>(k)];
            for (std::uint64_t gi : frames[static_cast<std::size_t>(k)].idx) {
                // 找到该样本在 used 中的局部下标 t
                const std::size_t t = static_cast<std::size_t>(
                    std::lower_bound(used.begin(), used.end(), gi) - used.begin());
                const double wi = w[t];
                if (!(wi > 0.0) || !std::isfinite(wi)) continue;
                const double y = samples[gi].value;
                const int* bi = &bfree[t * static_cast<std::size_t>(nb)];
                const double* bv = &bval[t * static_cast<std::size_t>(nb)];
                for (int a = 0; a < nb; ++a) {
                    const double av = wi * bv[a];
                    const int ia = bi[a];
                    if (ia < 0) continue;
                    rhs[static_cast<std::size_t>(ia)] += av * y;
                    for (int b = 0; b < nb; ++b) {
                        // bi[b] == -1 表示该节点无数据支撑（自由节点外）；必须跳过，
                        // 否则 H_data[ia*n_free - 1] 越界写（heap-buffer-overflow）。
                        const int ib = bi[b];
                        if (ib < 0) continue;
                        H_data[static_cast<std::size_t>(ia) * n_free + ib] += av * bv[b];
                    }
                    if (!ref) {
                        for (int q = 0; q < m; ++q)
                            Sk_[static_cast<std::size_t>(ia) * m + q] += av * pu[t][static_cast<std::size_t>(q)];
                    }
                }
                if (!ref) {
                    for (int p = 0; p < m; ++p) {
                        const double pw = wi * pu[t][static_cast<std::size_t>(p)];
                        tk_[static_cast<std::size_t>(p)] += pw * y;
                        for (int q = 0; q < m; ++q)
                            Mk_[static_cast<std::size_t>(p) * m + q] += pw * pu[t][static_cast<std::size_t>(q)];
                    }
                }
            }
        }
        // Schur 消元：H_red = H_data − Σ S_k M_k⁻¹ S_kᵀ；rhs −= Σ S_k M_k⁻¹ t_k
        // H_red 是 B_ref 的**约化数据矩阵**（δ_k 已被 profile out），
        // 秩/κ 诊断必须用它，而不是原始 Σ AᵀWA（后者恒满秩、无鉴别力）。
        H_red = H_data;
        for (int k = 0; k < n_frames; ++k) {
            if (k == model->ref_frame) continue;
            std::vector<double> Lk;
            if (!chol_spd(Mk[static_cast<std::size_t>(k)], m, Lk)) {
                sky_plane_free(model);
                if (err && err_size) std::snprintf(err, err_size, "frame %llu delta block singular",
                                                   (unsigned long long)frame_ids[static_cast<std::size_t>(k)]);
                return P2_SKY_PLANE_FRAME_UNDERDETERMINED;
            }
            const std::vector<double>& Sk_ = Sk[static_cast<std::size_t>(k)];
            std::vector<double> Minv_tk = tk[static_cast<std::size_t>(k)];
            chol_solve(Lk, m, Minv_tk);
            for (int i = 0; i < n_free; ++i) {
                double s = 0.0;
                for (int q = 0; q < m; ++q)
                    s += Sk_[static_cast<std::size_t>(i) * m + q] * Minv_tk[static_cast<std::size_t>(q)];
                rhs[static_cast<std::size_t>(i)] -= s;
            }
            // Z = M⁻¹ Sᵀ（m×n_free）：逐列解 M z = S[:,i]
            std::vector<double> Z(static_cast<std::size_t>(m) * n_free, 0.0);
            for (int i = 0; i < n_free; ++i) {
                std::vector<double> col(static_cast<std::size_t>(m));
                for (int q = 0; q < m; ++q) col[static_cast<std::size_t>(q)] = Sk_[static_cast<std::size_t>(i) * m + q];
                chol_solve(Lk, m, col);
                for (int q = 0; q < m; ++q)
                    Z[static_cast<std::size_t>(q) * n_free + i] = col[static_cast<std::size_t>(q)];
            }
            for (int i = 0; i < n_free; ++i)
                for (int j = 0; j < n_free; ++j) {
                    double s = 0.0;
                    for (int q = 0; q < m; ++q)
                        s += Sk_[static_cast<std::size_t>(i) * m + q] * Z[static_cast<std::size_t>(q) * n_free + j];
                    H_red[static_cast<std::size_t>(i) * n_free + j] -= s;
                }
        }
        // 求解矩阵 = 约化数据矩阵 + 粗糙度惩罚 λ·DᵀD（二阶差分，两个方向）
        H_solve = H_red;
        if (cfg.roughness_penalty > 0.0) {
            const double lam = cfg.roughness_penalty;
            auto add_stencil = [&](int i0, int i1, int i2) {
                const int f0 = free_of_full[static_cast<std::size_t>(i0)];
                const int f1 = free_of_full[static_cast<std::size_t>(i1)];
                const int f2 = free_of_full[static_cast<std::size_t>(i2)];
                if (f0 < 0 || f1 < 0 || f2 < 0) return;   // 空节点不参与
                const double v[3] = {1.0, -2.0, 1.0};
                const int ii[3] = {f0, f1, f2};
                for (int a = 0; a < 3; ++a)
                    for (int b = 0; b < 3; ++b)
                        H_solve[static_cast<std::size_t>(ii[a]) * n_free + ii[b]] += lam * v[a] * v[b];
            };
            for (int iy = 0; iy < model->ny; ++iy)
                for (int ix = 0; ix + 2 < model->nx; ++ix)
                    add_stencil(iy * model->nx + ix, iy * model->nx + ix + 1, iy * model->nx + ix + 2);
            for (int ix = 0; ix < model->nx; ++ix)
                for (int iy = 0; iy + 2 < model->ny; ++iy)
                    add_stencil(iy * model->nx + ix, (iy + 1) * model->nx + ix, (iy + 2) * model->nx + ix);
        }
        // ---- 求解：先直接 Cholesky；失败才对惩罚零空间做极小锚 + deflation 校正 ----
        // 数值根因（对生产数据的复核）：惩罚项 ~1e-3 与约化数据项 ~1e-18 相差 ~1e15
        // 量级，惩罚矩阵自身的浮点舍入（~1e-18）会淹没数据在惩罚零空间方向上的曲率
        // （~1e-20），使 H_solve = H_red + λDᵀD 在浮点下失去正定性（:770 rc=6）。
        // 数据项与惩罚项同量级的常规档（如单元测试 σ~0.05）直接 Cholesky 成功，本
        // 路径与结果逐位不变；仅当直接 Cholesky 失败时，才对**惩罚零空间方向**做极小
        // Tikhonov 锚使其严格 SPD，再用 deflation 校正把锚偏置精确扣回（科学解不变）。
        bool solved = false;
        std::vector<double> Bnew;
        if (chol_spd(H_solve, n_free, L)) {
            Bnew = rhs;
            chol_solve(L, n_free, Bnew);
            solved = true;
        }
        if (!solved && mq > 0) {
            // alpha 取惩罚项对角量级；只加在 mq 个惩罚零空间方向（Q Qᵀ）上。
            double alpha = 0.0;
            for (int i = 0; i < n_free; ++i) {
                const double pdiag = H_solve[static_cast<std::size_t>(i) * n_free + i] -
                                     H_red[static_cast<std::size_t>(i) * n_free + i];
                if (pdiag > alpha) alpha = pdiag;
            }
            if (alpha > 0.0) {
                for (int i = 0; i < n_free; ++i)
                    for (int j = 0; j < n_free; ++j) {
                        double s = 0.0;
                        for (int k = 0; k < mq; ++k)
                            s += gauge_q[static_cast<std::size_t>(i) * mq + k] *
                                 gauge_q[static_cast<std::size_t>(j) * mq + k];
                        H_solve[static_cast<std::size_t>(i) * n_free + j] += alpha * s;
                    }
                if (chol_spd(H_solve, n_free, L)) {
                    Bnew = rhs;
                    chol_solve(L, n_free, Bnew);
                    // Deflation 校正（只对惩罚零空间方向；O(n_free^2 * mq)）：
                    //   B = B_a + Q · G^{-1} · (Qᵀ rhs − Qᵀ H_red B_a), G = Qᵀ H_red Q
                    // 其中 B_a = (H_red + λDᵀD + alpha·Q Qᵀ)^{-1} rhs。因 P Q = 0，
                    // 校正后的 B 满足 (H_red + λDᵀD) B = rhs（至浮点精度），即把锚
                    // 偏置精确扣回，与直接求解惩罚法方程等价。
                    std::vector<double> HrQ(static_cast<std::size_t>(n_free) * mq, 0.0);
                    for (int i = 0; i < n_free; ++i) {
                        const double* row = &H_red[static_cast<std::size_t>(i) * n_free];
                        for (int k = 0; k < mq; ++k) {
                            double s = 0.0;
                            for (int j = 0; j < n_free; ++j)
                                s += row[j] * gauge_q[static_cast<std::size_t>(j) * mq + k];
                            HrQ[static_cast<std::size_t>(i) * mq + k] = s;
                        }
                    }
                    std::vector<double> G(static_cast<std::size_t>(mq) * mq, 0.0);
                    for (int p = 0; p < mq; ++p)
                        for (int q = 0; q < mq; ++q) {
                            double s = 0.0;
                            for (int i = 0; i < n_free; ++i)
                                s += gauge_q[static_cast<std::size_t>(i) * mq + p] *
                                     HrQ[static_cast<std::size_t>(i) * mq + q];
                            G[static_cast<std::size_t>(p) * mq + q] = s;
                        }
                    std::vector<double> corr(static_cast<std::size_t>(mq), 0.0);
                    for (int p = 0; p < mq; ++p) {
                        double s = 0.0;
                        for (int i = 0; i < n_free; ++i)
                            s += gauge_q[static_cast<std::size_t>(i) * mq + p] *
                                 rhs[static_cast<std::size_t>(i)];
                        for (int j = 0; j < n_free; ++j)
                            s -= HrQ[static_cast<std::size_t>(j) * mq + p] *
                                 Bnew[static_cast<std::size_t>(j)];
                        corr[static_cast<std::size_t>(p)] = s;
                    }
                    std::vector<double> Lg;
                    if (chol_spd(G, mq, Lg)) {
                        chol_solve(Lg, mq, corr);
                        for (int i = 0; i < n_free; ++i) {
                            double s = 0.0;
                            for (int k = 0; k < mq; ++k)
                                s += gauge_q[static_cast<std::size_t>(i) * mq + k] *
                                     corr[static_cast<std::size_t>(k)];
                            Bnew[static_cast<std::size_t>(i)] += s;
                        }
                        solved = true;
                    }
                }
            }
        }
        if (!solved) {
            sky_plane_free(model);
            if (err && err_size) std::snprintf(err, err_size, "reduced normal matrix not SPD");
            return P2_SKY_PLANE_RANK_DEFICIENT;
        }
        // 收敛检查
        double db = 0.0, sb = 0.0;
        for (int i = 0; i < n_free; ++i) {
            db = std::max(db, std::fabs(Bnew[static_cast<std::size_t>(i)] - B[static_cast<std::size_t>(i)]));
            sb = std::max(sb, std::fabs(Bnew[static_cast<std::size_t>(i)]));
        }
        B = Bnew;
        // δ_k
        for (int k = 0; k < n_frames; ++k) {
            if (k == model->ref_frame) { std::fill(model->deltas[static_cast<std::size_t>(k)].begin(), model->deltas[static_cast<std::size_t>(k)].end(), 0.0); continue; }
            std::vector<double> d = tk[static_cast<std::size_t>(k)];
            const std::vector<double>& Sk_ = Sk[static_cast<std::size_t>(k)];
            for (int q = 0; q < m; ++q) {
                double s = 0.0;
                for (int i = 0; i < n_free; ++i) s += Sk_[static_cast<std::size_t>(i) * m + q] * B[static_cast<std::size_t>(i)];
                d[static_cast<std::size_t>(q)] -= s;
            }
            std::vector<double> Lk;
            chol_spd(Mk[static_cast<std::size_t>(k)], m, Lk);
            chol_solve(Lk, m, d);
            model->deltas[static_cast<std::size_t>(k)] = d;
        }
        // 残差 → Huber 权重
        std::uint64_t nrej = 0;
        for (std::size_t t = 0; t < used.size(); ++t) {
            const std::uint64_t gi = used[t];
            const P2SkySample& s = samples[gi];
            double fit = 0.0;
            const int* bi = &bfree[t * static_cast<std::size_t>(nb)];
            const double* bv = &bval[t * static_cast<std::size_t>(nb)];
            for (int a = 0; a < nb; ++a) { if (bi[a] < 0) continue; fit += bv[a] * B[static_cast<std::size_t>(bi[a])]; }
            const int k = frame_pos[s.frame_id];
            const std::vector<double>& dk = model->deltas[static_cast<std::size_t>(k)];
            for (int q = 0; q < m; ++q) fit += pu[t][static_cast<std::size_t>(q)] * dk[static_cast<std::size_t>(q)];
            const double sigma = (cfg.weight_mode == 0) ? std::sqrt(s.variance) : (1.0 / std::max(s.snr, 1e-12));
            const double r = s.value - fit;
            const double z = (sigma > 0.0) ? r / sigma : 0.0;
            w[t] = base_w[t] * huber_w(z, cfg.huber_delta);
            if (std::fabs(z) > 5.0) ++nrej;
        }
        if (iter > 0 && db <= cfg.tolerance * std::max(1.0, sb)) break;
    }
    // 写回全节点系数；无数据支撑节点（bbox 空角）由最近自由节点填充
    // （SWarp/SEP 对无效 mesh 节点做最近邻填充的先例，只影响数据域外角点）。
    model->coeff.assign(static_cast<std::size_t>(n_full), 0.0);
    for (int f = 0; f < n_free; ++f) model->coeff[static_cast<std::size_t>(full_of_free[static_cast<std::size_t>(f)])] = B[static_cast<std::size_t>(f)];
    for (int iy = 0; iy < model->ny; ++iy)
        for (int ix = 0; ix < model->nx; ++ix) {
            const int idx = iy * model->nx + ix;
            if (free_of_full[static_cast<std::size_t>(idx)] >= 0) continue;
            int best = -1;
            long bestd = 0;
            for (int f = 0; f < n_free; ++f) {
                const int fi = full_of_free[static_cast<std::size_t>(f)];
                const long d = std::abs(static_cast<long>(fi % model->nx - ix)) +
                               std::abs(static_cast<long>(fi / model->nx - iy));
                if (best < 0 || d < bestd) { bestd = d; best = f; }
            }
            model->coeff[static_cast<std::size_t>(idx)] = (best >= 0) ? B[static_cast<std::size_t>(best)] : 0.0;
        }

    // ---- 诊断（未惩罚约化数据矩阵 H_red；无 jitter Cholesky） ----
    std::vector<double> Ld;
    if (!chol_spd(H_red, n_free, Ld)) {
        sky_plane_free(model);
        if (err && err_size) std::snprintf(err, err_size, "reduced data matrix rank-deficient (n_free=%d)", n_free);
        return P2_SKY_PLANE_RANK_DEFICIENT;
    }
    const double lam_max = lambda_max_power(H_red, n_free);
    const double lam_min = lambda_min_inverse(H_red, Ld, n_free);
    double kappa = 0.0;
    std::uint64_t rank = 0;
    if (!(lam_min > 0.0) || !std::isfinite(lam_min) || !std::isfinite(lam_max)) {
        rank = 0;
    } else {
        kappa = lam_max / lam_min;
        rank = (lam_min > cfg.rank_rtol * lam_max) ? static_cast<std::uint64_t>(n_free) : 0;
    }
    if (rank == 0) {
        sky_plane_free(model);
        if (err && err_size) std::snprintf(err, err_size, "rank-deficient reduced system (lambda_min=%.3e)", lam_min);
        return P2_SKY_PLANE_RANK_DEFICIENT;
    }
    if (kappa > cfg.kappa_max) {
        sky_plane_free(model);
        if (err && err_size) std::snprintf(err, err_size, "kappa=%.3e > kappa_max=%.3e", kappa, cfg.kappa_max);
        return P2_SKY_PLANE_KAPPA_EXCEEDED;
    }

    // ---- gauge ----
    model->gauge_shift = 0.0;
    if (cfg.gauge_mode == 1 && n_frames > 1) {
        double c = 0.0;
        for (int k = 0; k < n_frames; ++k) c += model->deltas[static_cast<std::size_t>(k)][0];
        c /= static_cast<double>(n_frames);
        for (int k = 0; k < n_frames; ++k) model->deltas[static_cast<std::size_t>(k)][0] -= c;
        model->gauge_shift = c;
    }

    // ---- 统计与 hash ----
    double sw = 0.0, swr2 = 0.0, sr2 = 0.0;
    std::uint64_t nrej = 0;
    for (std::size_t t = 0; t < used.size(); ++t) {
        const std::uint64_t gi = used[t];
        const P2SkySample& s = samples[gi];
        double fit = 0.0;
        const int* bi = &bfree[t * static_cast<std::size_t>(nb)];
        const double* bv = &bval[t * static_cast<std::size_t>(nb)];
        for (int a = 0; a < nb; ++a) { if (bi[a] < 0) continue; fit += bv[a] * B[static_cast<std::size_t>(bi[a])]; }
        const int k = frame_pos[s.frame_id];
        const std::vector<double>& dk = model->deltas[static_cast<std::size_t>(k)];
        for (int q = 0; q < m; ++q) fit += pu[t][static_cast<std::size_t>(q)] * dk[static_cast<std::size_t>(q)];
        fit += model->gauge_shift;
        const double r = s.value - fit;
        // 报告用加权 RMS 使用**基础逆方差权重**（不含 Huber 稳健因子），
        // 与 p2_sky_plane_residuals 的独立复算一致。
        sw += base_w[t];
        swr2 += base_w[t] * r * r;
        sr2 += r * r;
        if (std::fabs(r) / std::max((cfg.weight_mode == 0) ? std::sqrt(s.variance) : 1.0 / std::max(s.snr, 1e-12), 1e-12) > 5.0) ++nrej;
    }
    P2SkyPlaneInfo& info = model->info;
    info.version = 1;
    info.n_samples = n;
    info.n_used = n_used0;
    info.n_frames = static_cast<std::uint64_t>(n_frames);
    info.n_nodes = static_cast<std::uint64_t>(n_full);
    info.n_params = static_cast<std::uint64_t>(n_free) + static_cast<std::uint64_t>(n_frames - 1) * static_cast<std::uint64_t>(m);
    info.rank = rank;
    info.kappa = kappa;
    info.rms_weighted = (sw > 0.0) ? std::sqrt(swr2 / sw) : 0.0;
    info.rms_unweighted = std::sqrt(sr2 / static_cast<double>(used.size()));
    const double dof = static_cast<double>(used.size()) - static_cast<double>(info.n_params);
    info.chi2_red = (dof > 0.0) ? (swr2 / dof) : 0.0;
    info.iterations = iterations;
    info.gauge_mode = cfg.gauge_mode;
    info.weight_mode = cfg.weight_mode;
    info.frame_gradient_order = order;
    info.spline_degree = d;
    info.node_spacing_deg = model->h;
    info.ra0_deg = model->ra0_deg;
    info.dec0_deg = model->dec0_deg;
    info.u_min_deg = model->u_min; info.u_max_deg = model->u_max;
    info.v_min_deg = model->v_min; info.v_max_deg = model->v_max;
    info.gauge_shift = model->gauge_shift;
    info.n_masked = n_masked;
    info.n_rejected = nrej;

    {
        std::string blob;
        blob.reserve(static_cast<std::size_t>(n_full) * 8 + 4096);
        auto add = [&](const void* p, std::size_t len) { blob.append(static_cast<const char*>(p), len); };
        add(&model->ra0_deg, sizeof(double));
        add(&model->dec0_deg, sizeof(double));
        add(&model->t0u, sizeof(double));
        add(&model->t0v, sizeof(double));
        add(&model->h, sizeof(double));
        add(&d, sizeof(int));
        add(&order, sizeof(int));
        add(&model->nx, sizeof(int));
        add(&model->ny, sizeof(int));
        add(model->coeff.data(), model->coeff.size() * sizeof(double));
        for (const auto& dk : model->deltas) add(dk.data(), dk.size() * sizeof(double));
        add(&model->gauge_shift, sizeof(double));
        const std::string hx = astrocs::crypto::sha256_hex(blob.data(), blob.size());
        std::snprintf(info.model_hash, sizeof(info.model_hash), "%s", hx.c_str());
    }
    model->last_weights = w;
    *out_model = model;
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_info(const void* model, P2SkyPlaneInfo* out) {
    if (!model || !out) return P2_SKY_PLANE_INVALID_ARGS;
    *out = static_cast<const SkyPlaneModel*>(model)->info;
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_eval(const void* model_in, std::uint64_t frame_id,
                      double ra_deg, double dec_deg,
                      double* out_value, int* out_status) {
    if (out_status) *out_status = P2_SKY_EVAL_INVALID;
    if (!model_in || !out_value) return P2_SKY_PLANE_INVALID_ARGS;
    const SkyPlaneModel* m = static_cast<const SkyPlaneModel*>(model_in);
    const auto it = m->frame_index.find(frame_id);
    if (it == m->frame_index.end()) {
        if (out_status) *out_status = P2_SKY_EVAL_UNKNOWN_FRAME;
        return P2_SKY_PLANE_OK;
    }
    double u = 0, v = 0, cosc = 0;
    if (!gnomonic(m->ra0_deg, m->dec0_deg, ra_deg, dec_deg, &u, &v, &cosc)) {
        if (out_status) *out_status = P2_SKY_EVAL_OUT_OF_DOMAIN;
        return P2_SKY_PLANE_OK;
    }
    const double mg = m->cfg.max_extrapolation_deg;
    const double eps = 1e-9;   // 边界浮点舍入容差（不构成外插）
    if (u < m->du_min - mg - eps || u > m->du_max + mg + eps ||
        v < m->dv_min - mg - eps || v > m->dv_max + mg + eps) {
        if (out_status) *out_status = P2_SKY_EVAL_OUT_OF_DOMAIN;
        return P2_SKY_PLANE_OK;
    }
    int jx[8], jy[8];
    double nxv[8], nyv[8];
    bspline_basis(u, m->t0u, m->h, m->nx, m->degree, jx, nxv);
    bspline_basis(v, m->t0v, m->h, m->ny, m->degree, jy, nyv);
    double b = m->gauge_shift;
    for (int a = 0; a <= m->degree; ++a)
        for (int c = 0; c <= m->degree; ++c) {
            const int idx = jy[a] * m->nx + jx[c];
            if (idx >= 0 && idx < m->nx * m->ny) b += nyv[a] * nxv[c] * m->coeff[static_cast<std::size_t>(idx)];
        }
    std::vector<double> basis(static_cast<std::size_t>(m->m), 0.0);
    delta_basis(u, v, m->uc, m->vc, m->us, m->vs, m->delta_order, basis.data());
    const std::vector<double>& dk = m->deltas[it->second];
    double dv = 0.0;
    for (int q = 0; q < m->m; ++q) dv += basis[static_cast<std::size_t>(q)] * dk[static_cast<std::size_t>(q)];
    *out_value = b + dv;
    if (out_status) *out_status = P2_SKY_EVAL_OK;
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_eval_block(const void* model, std::uint64_t frame_id,
                            const double* ra_deg, const double* dec_deg,
                            std::uint64_t n, double* out_values,
                            std::uint8_t* out_status) {
    if (!model || !ra_deg || !dec_deg || !out_values) return P2_SKY_PLANE_INVALID_ARGS;
    // [RELEASE-02 probe] Phase2 天光面应用 (逐 tile 块; 非逐像素)
    ASTROCS_PROBE_SCOPE("phase2", "sky_plane.eval_block");
    ASTROCS_PROBE_GAUGE("phase2", "sky_plane.eval_points", static_cast<double>(n));
    int rc = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        double val = 0.0;
        int st = P2_SKY_EVAL_INVALID;
        p2_sky_plane_eval(model, frame_id, ra_deg[i], dec_deg[i], &val, &st);
        out_values[i] = (st == P2_SKY_EVAL_OK) ? val : 0.0;
        if (out_status) out_status[i] = static_cast<std::uint8_t>(st);
        if (st != P2_SKY_EVAL_OK) rc = 2;
    }
    return rc;
}

// FIX-GK / 方案 B：只求值逐帧偏差 δ_k(ra,dec) = b_k(x) − B_ref(x)。
// 与 p2_sky_plane_eval 共用同一 gnomonic/越域/basis 约定，但**不触碰**
// p2_sky_plane_eval 本体（其他调用方仍取 b_k=B_ref+δ_k，语义不变）。
// δ_k = gauge_shift + δ_k 多项式项（gauge_mode=0 ⇒ gauge_shift=0，参考帧 δ≡0）。
int p2_sky_plane_eval_delta(const void* model_in, std::uint64_t frame_id,
                            double ra_deg, double dec_deg,
                            double* out_value, int* out_status) {
    if (out_status) *out_status = P2_SKY_EVAL_INVALID;
    if (!model_in || !out_value) return P2_SKY_PLANE_INVALID_ARGS;
    const SkyPlaneModel* m = static_cast<const SkyPlaneModel*>(model_in);
    const auto it = m->frame_index.find(frame_id);
    if (it == m->frame_index.end()) {
        if (out_status) *out_status = P2_SKY_EVAL_UNKNOWN_FRAME;
        return P2_SKY_PLANE_OK;
    }
    double u = 0, v = 0, cosc = 0;
    if (!gnomonic(m->ra0_deg, m->dec0_deg, ra_deg, dec_deg, &u, &v, &cosc)) {
        if (out_status) *out_status = P2_SKY_EVAL_OUT_OF_DOMAIN;
        return P2_SKY_PLANE_OK;
    }
    const double mg = m->cfg.max_extrapolation_deg;
    const double eps = 1e-9;   // 边界浮点舍入容差（不构成外插）
    if (u < m->du_min - mg - eps || u > m->du_max + mg + eps ||
        v < m->dv_min - mg - eps || v > m->dv_max + mg + eps) {
        if (out_status) *out_status = P2_SKY_EVAL_OUT_OF_DOMAIN;
        return P2_SKY_PLANE_OK;
    }
    std::vector<double> basis(static_cast<std::size_t>(m->m), 0.0);
    delta_basis(u, v, m->uc, m->vc, m->us, m->vs, m->delta_order, basis.data());
    const std::vector<double>& dk = m->deltas[it->second];
    double dv = 0.0;
    for (int q = 0; q < m->m; ++q)
        dv += basis[static_cast<std::size_t>(q)] * dk[static_cast<std::size_t>(q)];
    // B 口径：δ_k = b_k − B_ref = gauge_shift + δ_k 多项式项。
    *out_value = m->gauge_shift + dv;
    if (out_status) *out_status = P2_SKY_EVAL_OK;
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_eval_delta_block(const void* model, std::uint64_t frame_id,
                                  const double* ra_deg, const double* dec_deg,
                                  std::uint64_t n, double* out_values,
                                  std::uint8_t* out_status) {
    if (!model || !ra_deg || !dec_deg || !out_values)
        return P2_SKY_PLANE_INVALID_ARGS;
    int rc = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        double val = 0.0;
        int st = P2_SKY_EVAL_INVALID;
        p2_sky_plane_eval_delta(model, frame_id, ra_deg[i], dec_deg[i], &val, &st);
        out_values[i] = (st == P2_SKY_EVAL_OK) ? val : 0.0;
        if (out_status) out_status[i] = static_cast<std::uint8_t>(st);
        if (st != P2_SKY_EVAL_OK) rc = 2;
    }
    return rc;
}

int p2_sky_plane_frame_delta(const void* model_in, std::uint64_t frame_id,
                             double* out_coeffs, std::uint64_t cap,
                             std::uint64_t* out_n) {
    if (!model_in) return P2_SKY_PLANE_INVALID_ARGS;
    const SkyPlaneModel* m = static_cast<const SkyPlaneModel*>(model_in);
    const auto it = m->frame_index.find(frame_id);
    if (it == m->frame_index.end()) return P2_SKY_PLANE_INVALID_ARGS;
    const std::vector<double>& dk = m->deltas[it->second];
    if (out_n) *out_n = dk.size();
    if (out_coeffs) {
        const std::uint64_t k = std::min<std::uint64_t>(cap, dk.size());
        for (std::uint64_t i = 0; i < k; ++i) out_coeffs[i] = dk[static_cast<std::size_t>(i)];
    }
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_residuals(const void* model_in,
                           const P2SkySample* samples, std::uint64_t n,
                           double* out_rms_weighted,
                           double* out_rms_unweighted,
                           std::uint64_t* out_n_used) {
    if (!model_in || !samples || n == 0) return P2_SKY_PLANE_INVALID_ARGS;
    const SkyPlaneModel* m = static_cast<const SkyPlaneModel*>(model_in);
    double sw = 0, swr2 = 0, sr2 = 0;
    std::uint64_t used = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        const P2SkySample& s = samples[i];
        const std::uint32_t bad = P2_SKY_FLAG_MASKED | P2_SKY_FLAG_LOW_SUPPORT |
                                  P2_SKY_FLAG_HIGH_CONTAMINATION | P2_SKY_FLAG_REJECTED;
        if (s.flags & bad) continue;
        if (!std::isfinite(s.value) || !std::isfinite(s.ra_deg) || !std::isfinite(s.dec_deg)) continue;
        double w = 0.0;
        if (m->cfg.weight_mode == 0) { if (!finite_pos(s.variance)) continue; w = 1.0 / s.variance; }
        else { if (!finite_pos(s.snr)) continue; w = s.snr * s.snr; }
        double val = 0.0;
        int st = 0;
        p2_sky_plane_eval(model_in, s.frame_id, s.ra_deg, s.dec_deg, &val, &st);
        if (st != P2_SKY_EVAL_OK) continue;
        const double r = s.value - val;
        sw += w; swr2 += w * r * r; sr2 += r * r; ++used;
    }
    if (out_rms_weighted) *out_rms_weighted = (sw > 0.0) ? std::sqrt(swr2 / sw) : 0.0;
    if (out_rms_unweighted) *out_rms_unweighted = (used > 0) ? std::sqrt(sr2 / static_cast<double>(used)) : 0.0;
    if (out_n_used) *out_n_used = used;
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_save(const void* model_in, const char* path) {
    if (!model_in || !path) return P2_SKY_PLANE_INVALID_ARGS;
    const SkyPlaneModel* m = static_cast<const SkyPlaneModel*>(model_in);
    nlohmann::json j;
    try {
        j["format"] = "astrocs-sky-plane-v1";
        j["ra0_deg"] = m->ra0_deg;
        j["dec0_deg"] = m->dec0_deg;
        j["t0u"] = m->t0u; j["t0v"] = m->t0v; j["h"] = m->h;
        j["degree"] = m->degree;
        j["delta_order"] = m->delta_order;
        j["nx"] = m->nx; j["ny"] = m->ny;
        j["gauge_shift"] = m->gauge_shift;
        j["uc"] = m->uc; j["vc"] = m->vc; j["us"] = m->us; j["vs"] = m->vs;
        j["du_min"] = m->du_min; j["du_max"] = m->du_max;
        j["dv_min"] = m->dv_min; j["dv_max"] = m->dv_max;
        j["cfg"] = {
            {"spline_degree", m->cfg.spline_degree},
            {"node_spacing_deg", m->cfg.node_spacing_deg},
            {"frame_gradient_order", m->cfg.frame_gradient_order},
            {"roughness_penalty", m->cfg.roughness_penalty},
            {"huber_delta", m->cfg.huber_delta},
            {"gauge_mode", m->cfg.gauge_mode},
            {"weight_mode", m->cfg.weight_mode},
            {"kappa_max", m->cfg.kappa_max},
            {"rank_rtol", m->cfg.rank_rtol},
            {"max_extrapolation_deg", m->cfg.max_extrapolation_deg}};
        j["coeff"] = m->coeff;
        j["frame_ids"] = m->frame_ids;
        j["deltas"] = m->deltas;
        j["info"] = {
            {"n_samples", m->info.n_samples}, {"n_used", m->info.n_used},
            {"n_frames", m->info.n_frames}, {"n_nodes", m->info.n_nodes},
            {"n_params", m->info.n_params}, {"rank", m->info.rank},
            {"kappa", m->info.kappa}, {"rms_weighted", m->info.rms_weighted},
            {"rms_unweighted", m->info.rms_unweighted},
            {"chi2_red", m->info.chi2_red}, {"iterations", m->info.iterations},
            {"n_masked", m->info.n_masked}, {"n_rejected", m->info.n_rejected},
            {"model_hash", std::string(m->info.model_hash)}};
        std::FILE* f = std::fopen(path, "wb");
        if (!f) return P2_SKY_PLANE_IO_ERROR;
        const std::string s = j.dump(2);
        const std::size_t wrote = std::fwrite(s.data(), 1, s.size(), f);
        std::fclose(f);
        if (wrote != s.size()) return P2_SKY_PLANE_IO_ERROR;
    } catch (...) {
        return P2_SKY_PLANE_IO_ERROR;
    }
    return P2_SKY_PLANE_OK;
}

int p2_sky_plane_open(const char* path, void** out_model) {
    if (!path || !out_model) return P2_SKY_PLANE_INVALID_ARGS;
    *out_model = nullptr;
    try {
        std::FILE* f = std::fopen(path, "rb");
        if (!f) return P2_SKY_PLANE_IO_ERROR;
        std::string s;
        char buf[65536];
        std::size_t got = 0;
        while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, got);
        std::fclose(f);
        nlohmann::json j = nlohmann::json::parse(s);
        if (j.value("format", std::string()) != "astrocs-sky-plane-v1") return P2_SKY_PLANE_IO_ERROR;
        SkyPlaneModel* m = new (std::nothrow) SkyPlaneModel();
        if (!m) return P2_SKY_PLANE_IO_ERROR;
        m->ra0_deg = j["ra0_deg"].get<double>();
        m->dec0_deg = j["dec0_deg"].get<double>();
        m->t0u = j["t0u"].get<double>(); m->t0v = j["t0v"].get<double>();
        m->h = j["h"].get<double>();
        m->degree = j["degree"].get<int>();
        m->delta_order = j["delta_order"].get<int>();
        m->m = delta_basis_size(m->delta_order);
        m->nx = j["nx"].get<int>(); m->ny = j["ny"].get<int>();
        m->gauge_shift = j["gauge_shift"].get<double>();
        m->uc = j["uc"].get<double>(); m->vc = j["vc"].get<double>();
        m->us = j["us"].get<double>(); m->vs = j["vs"].get<double>();
        m->du_min = j["du_min"].get<double>(); m->du_max = j["du_max"].get<double>();
        m->dv_min = j["dv_min"].get<double>(); m->dv_max = j["dv_max"].get<double>();
        m->cfg = p2_sky_plane_default_config();
        if (j.contains("cfg")) {
            const auto& c = j["cfg"];
            m->cfg.spline_degree = c.value("spline_degree", m->cfg.spline_degree);
            m->cfg.node_spacing_deg = c.value("node_spacing_deg", m->cfg.node_spacing_deg);
            m->cfg.frame_gradient_order = c.value("frame_gradient_order", m->cfg.frame_gradient_order);
            m->cfg.roughness_penalty = c.value("roughness_penalty", m->cfg.roughness_penalty);
            m->cfg.huber_delta = c.value("huber_delta", m->cfg.huber_delta);
            m->cfg.gauge_mode = c.value("gauge_mode", m->cfg.gauge_mode);
            m->cfg.weight_mode = c.value("weight_mode", m->cfg.weight_mode);
            m->cfg.kappa_max = c.value("kappa_max", m->cfg.kappa_max);
            m->cfg.rank_rtol = c.value("rank_rtol", m->cfg.rank_rtol);
            m->cfg.max_extrapolation_deg = c.value("max_extrapolation_deg", m->cfg.max_extrapolation_deg);
        }
        m->coeff = j["coeff"].get<std::vector<double>>();
        m->frame_ids = j["frame_ids"].get<std::vector<std::uint64_t>>();
        m->deltas = j["deltas"].get<std::vector<std::vector<double>>>();
        for (std::size_t k = 0; k < m->frame_ids.size(); ++k) m->frame_index[m->frame_ids[k]] = k;
        m->ref_frame = 0;
        if (j.contains("info")) {
            const auto& i = j["info"];
            m->info.version = 1;
            m->info.n_samples = i.value("n_samples", (std::uint64_t)0);
            m->info.n_used = i.value("n_used", (std::uint64_t)0);
            m->info.n_frames = i.value("n_frames", (std::uint64_t)0);
            m->info.n_nodes = i.value("n_nodes", (std::uint64_t)0);
            m->info.n_params = i.value("n_params", (std::uint64_t)0);
            m->info.rank = i.value("rank", (std::uint64_t)0);
            m->info.kappa = i.value("kappa", 0.0);
            m->info.rms_weighted = i.value("rms_weighted", 0.0);
            m->info.rms_unweighted = i.value("rms_unweighted", 0.0);
            m->info.chi2_red = i.value("chi2_red", 0.0);
            m->info.iterations = i.value("iterations", 0);
            m->info.n_masked = i.value("n_masked", (std::uint64_t)0);
            m->info.n_rejected = i.value("n_rejected", (std::uint64_t)0);
            m->info.gauge_mode = m->cfg.gauge_mode;
            m->info.weight_mode = m->cfg.weight_mode;
            m->info.frame_gradient_order = m->delta_order;
            m->info.spline_degree = m->degree;
            m->info.node_spacing_deg = m->h;
            m->info.ra0_deg = m->ra0_deg; m->info.dec0_deg = m->dec0_deg;
            m->info.u_min_deg = m->du_min; m->info.u_max_deg = m->du_max;
            m->info.v_min_deg = m->dv_min; m->info.v_max_deg = m->dv_max;
            m->info.gauge_shift = m->gauge_shift;
            const std::string hx = i.value("model_hash", std::string());
            std::snprintf(m->info.model_hash, sizeof(m->info.model_hash), "%s", hx.c_str());
        }
        const std::size_t expect = static_cast<std::size_t>(m->nx) * static_cast<std::size_t>(m->ny);
        if (m->coeff.size() != expect || m->deltas.size() != m->frame_ids.size()) {
            delete m;
            return P2_SKY_PLANE_IO_ERROR;
        }
        for (const auto& dk : m->deltas)
            if (dk.size() != static_cast<std::size_t>(m->m)) { delete m; return P2_SKY_PLANE_IO_ERROR; }
        *out_model = m;
        return P2_SKY_PLANE_OK;
    } catch (...) {
        return P2_SKY_PLANE_IO_ERROR;
    }
}

void p2_sky_plane_close(void* model) { sky_plane_free(model); }

int p2_sky_estimate_gain_dc(const P2SkySample* samples, std::uint64_t n,
                              std::uint64_t ref_frame, std::uint64_t frame,
                              double* out_g) {
    if (!samples || n == 0 || !out_g) return 1;
    if (frame == ref_frame) { *out_g = 1.0; return 0; }
    // control_id -> value（各帧），仅在 ref 与 frame 公共 control 上求和。
    std::map<std::uint64_t, double> refv, framv;
    for (std::uint64_t i = 0; i < n; ++i) {
        const P2SkySample& s = samples[i];
        if (!std::isfinite(s.value)) continue;
        if (s.frame_id == ref_frame) refv[s.control_id] = s.value;
        else if (s.frame_id == frame) framv[s.control_id] = s.value;
    }
    double num = 0.0, den = 0.0;
    std::uint64_t common = 0;
    for (const auto& kv : framv) {
        const auto it = refv.find(kv.first);
        if (it == refv.end()) continue;
        num += kv.second;
        den += it->second;
        ++common;
    }
    if (common == 0 || !(std::fabs(den) > 0.0) || !std::isfinite(num) ||
        !std::isfinite(den))
        return 2;
    const double g = num / den;
    if (!std::isfinite(g) || g <= 0.0) return 3;
    *out_g = g;
    return 0;
}
}  // extern "C"
