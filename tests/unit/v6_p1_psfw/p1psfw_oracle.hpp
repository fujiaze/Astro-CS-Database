/* p1psfw_oracle.hpp - IMPL-P1-PSFW-001 独立 Oracle
 * 与被测实现零共享代码: 线性代数走显式逆 (Gauss-Jordan 全主元, long double),
 * 生产走 Cholesky/Woodbury; 稳健统计独立实现; MC 数据生成用独立 RNG。 */
#ifndef P1PSFW_ORACLE_HPP
#define P1PSFW_ORACLE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p1psfw {
namespace oracle {

struct Rng {
    uint64_t state;
    explicit Rng(uint64_t seed) : state(seed ? seed : 0x9e3779b97f4a7c15ULL) {}
    uint64_t next() {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    double uniform01() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
    double gaussian() {
        double u1 = uniform01();
        if (u1 <= 0.0) u1 = 1e-15;
        const double u2 = uniform01();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    }
};

inline std::vector<long double> invert_matrix(std::vector<long double> a, std::size_t n) {
    std::vector<long double> inv(n * n, 0.0L);
    for (std::size_t i = 0; i < n; ++i) inv[i * n + i] = 1.0L;
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        long double best = std::fabs(a[col * n + col]);
        for (std::size_t i = col + 1; i < n; ++i) {
            const long double v = std::fabs(a[i * n + col]);
            if (v > best) { best = v; piv = i; }
        }
        if (!(best > 0.0L)) return std::vector<long double>();
        if (piv != col) {
            for (std::size_t j = 0; j < n; ++j) {
                std::swap(a[col * n + j], a[piv * n + j]);
                std::swap(inv[col * n + j], inv[piv * n + j]);
            }
        }
        const long double d = a[col * n + col];
        for (std::size_t j = 0; j < n; ++j) { a[col * n + j] /= d; inv[col * n + j] /= d; }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == col) continue;
            const long double f = a[i * n + col];
            if (f == 0.0L) continue;
            for (std::size_t j = 0; j < n; ++j) {
                a[i * n + j] -= f * a[col * n + j];
                inv[i * n + j] -= f * inv[col * n + j];
            }
        }
    }
    return inv;
}

inline long double o_anea(const double* p, std::size_t n) {
    long double s2 = 0.0L;
    for (std::size_t i = 0; i < n; ++i) s2 += static_cast<long double>(p[i]) * p[i];
    return s2 > 0.0L ? 1.0L / s2 : 0.0L;
}

inline void o_qw_dense(const std::vector<double>& P, const std::vector<double>& C,
                       const std::vector<double>& d, double a,
                       long double* out_w, long double* out_q) {
    const std::size_t m = P.size();
    std::vector<long double> Cl(m * m);
    for (std::size_t i = 0; i < m * m; ++i) Cl[i] = C[i];
    const std::vector<long double> Cinv = invert_matrix(Cl, m);
    long double w = 0.0L, q = 0.0L;
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            w += static_cast<long double>(P[i]) * Cinv[i * m + j] * P[j];
            q += static_cast<long double>(P[i]) * Cinv[i * m + j] * d[j];
        }
    if (out_w) *out_w = a * a * w;
    if (out_q) *out_q = a * q;
}

inline long double o_w_diagonal(const std::vector<double>& P,
                                const std::vector<double>& sigma2, double a) {
    long double s = 0.0L;
    for (std::size_t i = 0; i < P.size(); ++i)
        s += static_cast<long double>(P[i]) * P[i] / sigma2[i];
    return a * a * s;
}

inline long double o_w_white_noise(const std::vector<double>& P, double sigma2, double a) {
    const long double anea = o_anea(P.data(), P.size());
    return (static_cast<long double>(a) * a) / (static_cast<long double>(sigma2) * anea);
}

inline long double o_gls_flux_dense(const std::vector<double>& P, const std::vector<double>& C,
                                    const std::vector<double>& d, double a) {
    long double w = 0.0L, q = 0.0L;
    o_qw_dense(P, C, d, a, &w, &q);
    return w > 0.0L ? q / w : 0.0L;
}

inline double o_median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline double o_robust_scale_mad(const std::vector<double>& x) {
    const double med = o_median(x);
    std::vector<double> dev(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) dev[i] = std::fabs(x[i] - med);
    return 1.482602218505602 * o_median(dev);
}

inline double o_quantile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    if (p <= 0.0) return v.front();
    if (p >= 1.0) return v.back();
    const double pos = p * static_cast<double>(v.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
    if (lo == hi) return v[lo];
    const double f = pos - static_cast<double>(lo);
    return v[lo] * (1.0 - f) + v[hi] * f;
}

inline double o_spearman(const std::vector<double>& x, const std::vector<double>& y) {
    const std::size_t n = x.size();
    if (n < 2 || x.size() != y.size()) return 0.0;
    std::vector<std::pair<double, std::size_t>> px(n), py(n);
    for (std::size_t i = 0; i < n; ++i) { px[i] = {x[i], i}; py[i] = {y[i], i}; }
    std::sort(px.begin(), px.end());
    std::sort(py.begin(), py.end());
    std::vector<double> rx(n), ry(n);
    for (std::size_t i = 0; i < n; ++i) {
        rx[px[i].second] = static_cast<double>(i);
        ry[py[i].second] = static_cast<double>(i);
    }
    double mx = 0.0, my = 0.0;
    for (std::size_t i = 0; i < n; ++i) { mx += rx[i]; my += ry[i]; }
    mx /= static_cast<double>(n); my /= static_cast<double>(n);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = rx[i] - mx, dy = ry[i] - my;
        sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
    }
    if (sxx <= 0.0 || syy <= 0.0) return 0.0;
    return sxy / std::sqrt(sxx * syy);
}

inline std::vector<double> o_cholesky_factor(const std::vector<double>& C, std::size_t m, bool* ok) {
    std::vector<double> L(m * m, 0.0);
    if (ok) *ok = false;
    for (std::size_t i = 0; i < m; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double s = C[i * m + j];
            for (std::size_t k = 0; k < j; ++k) s -= L[i * m + k] * L[j * m + k];
            if (i == j) {
                if (!(s > 0.0)) return L;
                L[i * m + j] = std::sqrt(s);
            } else {
                L[i * m + j] = s / L[j * m + j];
            }
        }
    }
    if (ok) *ok = true;
    return L;
}

struct McData {
    std::vector<std::vector<double>> d;   /* [trial][K*m] */
    double true_flux = 0.0;
};

inline McData o_mc_generate(const std::vector<std::vector<double>>& P_k,
                            const std::vector<std::vector<double>>& L_k,
                            const std::vector<double>& a_k, double F_true,
                            int ntrials, uint64_t seed) {
    McData out;
    out.true_flux = F_true;
    const std::size_t K = P_k.size();
    const std::size_t m = P_k[0].size();
    out.d.assign(static_cast<std::size_t>(ntrials), std::vector<double>(K * m, 0.0));
    Rng rng(seed);
    for (int t = 0; t < ntrials; ++t) {
        for (std::size_t k = 0; k < K; ++k) {
            std::vector<double> z(m);
            for (std::size_t i = 0; i < m; ++i) z[i] = rng.gaussian();
            for (std::size_t i = 0; i < m; ++i) {
                double nz = 0.0;
                for (std::size_t j = 0; j <= i; ++j) nz += L_k[k][i * m + j] * z[j];
                out.d[t][k * m + i] = a_k[k] * F_true * P_k[k][i] + nz;
            }
        }
    }
    return out;
}

inline double o_sample_variance(const std::vector<double>& x) {
    const std::size_t n = x.size();
    if (n < 2) return 0.0;
    double mean = 0.0;
    for (double v : x) mean += v;
    mean /= static_cast<double>(n);
    double s = 0.0;
    for (double v : x) s += (v - mean) * (v - mean);
    return s / static_cast<double>(n - 1);
}

inline std::vector<double> o_mc_coadd_var(
    const std::vector<std::vector<double>>& alpha, const std::vector<double>& c_in,
    int ntrials, uint64_t seed) {
    const std::size_t K = alpha.size();
    const std::size_t npix = alpha[0].size();
    bool ok = false;
    const std::vector<double> L = o_cholesky_factor(c_in, K, &ok);
    std::vector<double> var(npix, 0.0);
    if (!ok) return var;
    Rng rng(seed);
    std::vector<double> acc(npix, 0.0), acc2(npix, 0.0);
    for (int t = 0; t < ntrials; ++t) {
        std::vector<double> z(K);
        for (std::size_t k = 0; k < K; ++k) z[k] = rng.gaussian();
        std::vector<double> noise(K, 0.0);
        for (std::size_t a = 0; a < K; ++a) {
            double row = 0.0;
            for (std::size_t b = 0; b <= a; ++b) row += L[a * K + b] * z[b];
            noise[a] = row;
        }
        for (std::size_t p = 0; p < npix; ++p) {
            double v = 0.0;
            for (std::size_t a = 0; a < K; ++a) v += alpha[a][p] * noise[a];
            acc[p] += v;
            acc2[p] += v * v;
        }
    }
    for (std::size_t p = 0; p < npix; ++p)
        var[p] = (acc2[p] - acc[p] * acc[p] / ntrials) / (ntrials - 1);
    return var;
}

}  /* namespace oracle */
}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif /* P1PSFW_ORACLE_HPP */
