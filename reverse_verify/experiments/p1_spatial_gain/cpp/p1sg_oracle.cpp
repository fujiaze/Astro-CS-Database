// ============================================================================
// p1sg_oracle.cpp —— P1-SPATIAL-GAIN 独立 C++ Oracle (逆向验收)
//
// 目的: 用**独立实现**(不同语言/不同 RNG/不同线性代数路径)复现 Python 实验
//   (reverse_verify/synthetic/synth_gain.py) 的核心结论, 并给出**能红能绿**的判据。
//
// 模型 (与 Python 侧同构, 但代码独立):
//   r_i = log10(F_instr,i / F_syn,i) = log10(a_k) + log10(m_k(p_i)) + eps_i
//   拟合 r 的低阶多项式曲面 surf(p); 校正场 = 10^(-surf(p))
//     k_photo = 10^(-mean_surf),  m(p) = 10^(-(surf(p)-mean_surf))   (星集合上 log 均值 0)
//   仪器空间乘法响应 m_true 与校正场互为倒数 (m_fit ≈ 1/m_true)。
//
// 判据 (判据先行, 阈值在实现前写定, 见 THRESH_*):
//   C1 正例恢复: m_true pp=7%, N=200, sigma_int=0.010 dex
//        order=1 必须把帧间乘性残差场 PTP 从 >=8% 降到 <=3.0% (相对下降 >=60%)
//   C2 负例归零: m_true==1 时 order=1 的 m 拟合 PTP <= 2.0%, 校正后残差场 PTP <= 0.5%
//   C3 高阶有害: **负例噪声底**的 p90: order=3 >= 3 x order=1
//                (正例的 m 幅度含真值信号, 不能当"底"; 判据只看负例尾部)
//   C4 可辨识性: 背景上 (m,g) 变换 max|dy| <= 1e-9 ADU 且星点流量比 pp >= 1%
//   C5 小样本退化: N=20 时 order=2 的**负例**伪 m 的 p90 >= 1.0 x 真值 pp
//                (尾部与真信号同量级 => 该配置必须 fail-closed 或降阶)
//   C6 施加语义: 像素乘 10^(-surf) 后, 局部孔径流量按模型值缩放, 相对误差 <= 1e-3
//
// 退出码: 0 = 全部 PASS; 1 = 有 FAIL。
// ============================================================================
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

// ---------------------------------------------------------------- 判据阈值
static const double THRESH_C1_AFTER_PTP   = 3.0;    // %
static const double THRESH_C1_BEFORE_PTP  = 8.0;    // %
static const double THRESH_C2_M_PTP       = 2.0;    // %
static const double THRESH_C2_FIELD_PTP   = 0.5;    // %
static const double THRESH_C3_RATIO       = 3.0;    // order3 / order1 噪声底
static const double THRESH_C4_DY_ADU      = 1e-9;   // ADU
static const double THRESH_C4_STAR_PP     = 1.0;    // %
static const double THRESH_C5_RATIO       = 1.0;    // N=20 order2 伪 m 的 p90 / 真值 pp
static const double THRESH_C6_REL        = 1e-3;

// ---------------------------------------------------------------- RNG
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint64_t next() {                       // splitmix64
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uni() { return (double)(next() >> 11) * (1.0 / 9007199254740992.0); }
    double norm() {                          // Box-Muller
        double u1 = std::max(uni(), 1e-12), u2 = uni();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    }
};

// ---------------------------------------------------------------- 常数
static const int    W = 1024, H = 1024;
static const double S0 = 1000.0;
static const double SIG_PX = 5.0;
static const double AP_R = 6.0, ANN_IN = 10.0, ANN_OUT = 16.0;
static const double AP_NPIX = M_PI * AP_R * AP_R;
static const double ANN_NPIX = M_PI * (ANN_OUT * ANN_OUT - ANN_IN * ANN_IN);
static const double SIG_AP = SIG_PX * std::sqrt(AP_NPIX * (1.0 + AP_NPIX / ANN_NPIX));
static const double QUAD_REL = 0.20;

// ---------------------------------------------------------------- 多项式基
struct Term { int i, j; };
static std::vector<Term> terms_of(int order) {
    std::vector<Term> t;
    for (int d = 1; d <= order; ++d)
        for (int i = d; i >= 0; --i) t.push_back(Term{i, d - i});
    return t;
}
static double poly_eval(const std::vector<double>& c, const std::vector<Term>& t,
                        double xn, double yn) {
    double z = c[0];
    for (size_t k = 0; k < t.size(); ++k) z += c[k + 1] * std::pow(xn, t[k].i) * std::pow(yn, t[k].j);
    return z;
}

// ---------------------------------------------------------------- 线性代数
// 解 A x = b (n x n), 高斯消元 + 部分主元
static bool solve_dense(std::vector<double> A, std::vector<double> b, int n, std::vector<double>& x) {
    for (int col = 0; col < n; ++col) {
        int piv = col; double best = std::fabs(A[(size_t)col * n + col]);
        for (int r = col + 1; r < n; ++r) {
            double v = std::fabs(A[(size_t)r * n + col]);
            if (v > best) { best = v; piv = r; }
        }
        if (best < 1e-300) return false;
        if (piv != col) {
            for (int k = 0; k < n; ++k) std::swap(A[(size_t)col * n + k], A[(size_t)piv * n + k]);
            std::swap(b[col], b[piv]);
        }
        double d = A[(size_t)col * n + col];
        for (int r = col + 1; r < n; ++r) {
            double f = A[(size_t)r * n + col] / d;
            if (f == 0.0) continue;
            for (int k = col; k < n; ++k) A[(size_t)r * n + k] -= f * A[(size_t)col * n + k];
            b[r] -= f * b[col];
        }
    }
    x.assign(n, 0.0);
    for (int r = n - 1; r >= 0; --r) {
        double s = b[r];
        for (int k = r + 1; k < n; ++k) s -= A[(size_t)r * n + k] * x[k];
        x[r] = s / A[(size_t)r * n + r];
    }
    return true;
}

static double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
static double percentile_of(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    double pos = q * (v.size() - 1);
    size_t lo = (size_t)std::floor(pos), hi = (size_t)std::ceil(pos);
    return v[lo] + (v[hi] - v[lo]) * (pos - lo);
}
static double mad_sigma(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    double m = median_of(v);
    std::vector<double> d(v.size());
    for (size_t i = 0; i < v.size(); ++i) d[i] = std::fabs(v[i] - m);
    return median_of(d) / 0.6744897501960817;
}

// ---------------------------------------------------------------- Tukey-IRLS 曲面拟合
struct FitResult {
    std::vector<double> coef;      // [1+nterm]
    double mean_surf = 0.0;        // 星集合上曲面的加权均值
    double sigma_dex = 0.0;
    bool ok = false;
};
static FitResult fit_surface(const std::vector<double>& x, const std::vector<double>& y,
                             const std::vector<double>& r, const std::vector<double>& sig,
                             int order) {
    FitResult out;
    const int n = (int)x.size();
    const std::vector<Term> t = terms_of(order);
    const int P = 1 + (int)t.size();
    if (n < P) return out;
    double xmin = *std::min_element(x.begin(), x.end()), xmax = *std::max_element(x.begin(), x.end());
    double ymin = *std::min_element(y.begin(), y.end()), ymax = *std::max_element(y.begin(), y.end());
    double x0 = 0.5 * (xmin + xmax), sx = std::max(0.5 * (xmax - xmin), 1e-9);
    double y0 = 0.5 * (ymin + ymax), sy = std::max(0.5 * (ymax - ymin), 1e-9);

    std::vector<std::vector<double>> M(n, std::vector<double>(P));
    for (int i = 0; i < n; ++i) {
        double xn = (x[i] - x0) / sx, yn = (y[i] - y0) / sy;
        M[i][0] = 1.0;
        for (size_t k = 0; k < t.size(); ++k) M[i][k + 1] = std::pow(xn, t[k].i) * std::pow(yn, t[k].j);
    }
    std::vector<double> w0(n), wt(n);
    for (int i = 0; i < n; ++i) { w0[i] = 1.0 / (sig[i] * sig[i]); wt[i] = w0[i]; }
    std::vector<double> c(P, 0.0);
    for (int iter = 0; iter < 50; ++iter) {
        std::vector<double> A((size_t)P * P, 0.0), b(P, 0.0);
        for (int i = 0; i < n; ++i) {
            for (int a = 0; a < P; ++a) {
                double wa = wt[i] * M[i][a];
                for (int bb = 0; bb < P; ++bb) A[(size_t)a * P + bb] += wa * M[i][bb];
                b[a] += wa * r[i];
            }
        }
        std::vector<double> cn;
        if (!solve_dense(A, b, P, cn)) return out;
        std::vector<double> resid(n);
        for (int i = 0; i < n; ++i) {
            double m = 0.0;
            for (int a = 0; a < P; ++a) m += cn[a] * M[i][a];
            resid[i] = r[i] - m;
        }
        double s = mad_sigma(resid);
        std::vector<double> wn(n);
        if (s <= 0.0) { wn = w0; }
        else {
            for (int i = 0; i < n; ++i) {
                double u = resid[i] / (4.685 * s);
                wn[i] = (std::fabs(u) >= 1.0) ? 0.0 : w0[i] * (1.0 - u * u) * (1.0 - u * u);
            }
        }
        double diff = 0.0;
        for (int a = 0; a < P; ++a) diff = std::max(diff, std::fabs(cn[a] - c[a]));
        c = cn; wt = wn;
        if (iter > 0 && diff < 1e-6) break;
    }
    // mean_surf (加权) 与 sigma
    double sw = 0.0, sws = 0.0;
    std::vector<double> resid(n);
    for (int i = 0; i < n; ++i) {
        double xn = (x[i] - x0) / sx, yn = (y[i] - y0) / sy;
        double s = poly_eval(c, t, xn, yn);
        sw += w0[i]; sws += w0[i] * s;
        resid[i] = r[i] - s;
    }
    out.coef = c;
    out.mean_surf = (sw > 0) ? sws / sw : 0.0;
    out.sigma_dex = mad_sigma(resid);
    out.ok = true;
    // 归一化参数随系数一起用: 打包到静态 (单线程 oracle, 可接受)
    static double g_x0, g_sx, g_y0, g_sy;
    g_x0 = x0; g_sx = sx; g_y0 = y0; g_sy = sy;
    out.coef.push_back(g_x0); out.coef.push_back(g_sx);
    out.coef.push_back(g_y0); out.coef.push_back(g_sy);
    return out;
}
static void unpack(const FitResult& f, int order, std::vector<double>& c,
                   double& x0, double& sx, double& y0, double& sy) {
    const int P = 1 + (int)terms_of(order).size();
    c.assign(f.coef.begin(), f.coef.begin() + P);
    x0 = f.coef[P]; sx = f.coef[P + 1]; y0 = f.coef[P + 2]; sy = f.coef[P + 3];
}

// ---------------------------------------------------------------- 合成真值
struct Truth { std::vector<double> x, y, ftrue, lgA, lgB; double aA = 1.0, aB = 1.0; };
static std::vector<double> make_field(Rng& rng, double pp_pct, double unit, bool quad) {
    std::vector<double> c(5, 0.0);
    c[0] = rng.norm(); c[1] = rng.norm();
    if (quad) { c[2] = QUAD_REL * rng.norm(); c[3] = QUAD_REL * rng.norm(); c[4] = QUAD_REL * rng.norm(); }
    // 网格零均值 + 定标到 pp
    double lo = 1e300, hi = -1e300, sum = 0.0; int cnt = 0;
    for (int iy = 0; iy <= 64; ++iy) for (int ix = 0; ix <= 64; ++ix) {
        double xn = 2.0 * ix / 64.0 - 1.0, yn = 2.0 * iy / 64.0 - 1.0;
        double z = c[0] * xn + c[1] * yn + c[2] * xn * xn + c[3] * xn * yn + c[4] * yn * yn;
        lo = std::min(lo, z); hi = std::max(hi, z); sum += z; ++cnt;
    }
    double mean = sum / cnt;
    double span = hi - lo;
    double scale = (span > 0) ? unit / span : 0.0;
    for (double& v : c) v *= scale;
    (void)mean;
    return c;
}
static double field_at(const std::vector<double>& c, double x, double y) {
    double xn = (x - W / 2.0) / (W / 2.0), yn = (y - H / 2.0) / (H / 2.0);
    return c[0] * xn + c[1] * yn + c[2] * xn * xn + c[3] * xn * yn + c[4] * yn * yn;
}

struct Case {
    std::vector<double> x, y, rA, rB, sig, lgA, lgB;
    double aA = 1.0, aB = 1.0;
    std::vector<double> mA, mB;   // 真值场系数 (log10 m, 网格零均值)
};
static Case make_case(Rng& rng, int n_star, double sig_int_dex, double pp_m, bool neg) {
    Case cs;
    const int POOL = 1000;
    std::vector<double> px(POOL), py(POOL), pf(POOL);
    for (int i = 0; i < POOL; ++i) {
        px[i] = 32.0 + rng.uni() * (W - 65.0);
        py[i] = 32.0 + rng.uni() * (H - 65.0);
        pf[i] = std::pow(10.0, 3.0 + 2.0 * rng.uni());
    }
    std::vector<int> idx(POOL); for (int i = 0; i < POOL; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return pf[a] > pf[b]; });
    n_star = std::min(n_star, POOL);
    cs.mA = neg ? std::vector<double>(5, 0.0) : make_field(rng, pp_m, std::log10(1.0 + pp_m / 100.0), true);
    cs.mB = neg ? std::vector<double>(5, 0.0) : make_field(rng, pp_m, std::log10(1.0 + pp_m / 100.0), true);
    cs.aA = 1.0; cs.aB = std::pow(10.0, 0.05 + 0.10 * rng.uni());
    for (int k = 0; k < n_star; ++k) {
        int i = idx[k];
        double lgA = field_at(cs.mA, px[i], py[i]);
        double lgB = field_at(cs.mB, px[i], py[i]);
        double f = pf[i];
        double eps = (sig_int_dex > 0) ? sig_int_dex * rng.norm() : 0.0;
        // F_instr = a*m*F_true + 噪声;  F_syn = F_true*10^eps
        double fA = cs.aA * std::pow(10.0, lgA) * f + SIG_AP * rng.norm();
        double fB = cs.aB * std::pow(10.0, lgB) * f + SIG_AP * rng.norm();
        if (fA <= 0 || fB <= 0) continue;
        double fsyn = f * std::pow(10.0, eps);
        cs.x.push_back(px[i]); cs.y.push_back(py[i]);
        cs.rA.push_back(std::log10(fA / fsyn));
        cs.rB.push_back(std::log10(fB / fsyn));
        double sg = std::sqrt(2.0) * SIG_AP / (std::log(10.0) * std::max(fA, 1e-9));
        cs.sig.push_back(sg);
        cs.lgA.push_back(lgA); cs.lgB.push_back(lgB);
    }
    return cs;
}

// 校正后帧间比值场 PTP (%) + m 拟合 PTP (%) + 形状误差 (%)
struct Metrics { double field_ptp_before = 0, field_ptp_after = 0, m_ptp = 0, shape_rms = 0, shape_ptp = 0; };
static Metrics evaluate(const Case& cs, int order) {
    Metrics mt;
    FitResult fA = fit_surface(cs.x, cs.y, cs.rA, cs.sig, order);
    FitResult fB = fit_surface(cs.x, cs.y, cs.rB, cs.sig, order);
    if (!fA.ok || !fB.ok) return mt;
    std::vector<double> cA, cB; double x0, sx, y0, sy;
    unpack(fA, order, cA, x0, sx, y0, sy);
    unpack(fB, order, cB, x0, sx, y0, sy);
    const std::vector<Term> t = terms_of(order);
    // 网格: 真值 (连续场, 网格零均值) 与拟合曲面
    std::vector<double> lgtA, lgtB, sA, sB;
    for (int iy = 0; iy < 96; ++iy) for (int ix = 0; ix < 96; ++ix) {
        double x = 32.0 + (W - 65.0) * ix / 95.0, y = 32.0 + (H - 65.0) * iy / 95.0;
        lgtA.push_back(field_at(cs.mA, x, y));
        lgtB.push_back(field_at(cs.mB, x, y));
        sA.push_back(poly_eval(cA, t, (x - x0) / sx, (y - y0) / sy));
        sB.push_back(poly_eval(cB, t, (x - x0) / sx, (y - y0) / sy));
    }
    auto demean = [](std::vector<double>& v) {
        double m = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        for (double& z : v) z -= m;
    };
    demean(lgtA); demean(lgtB);
    // 帧间真值比 (log10)
    std::vector<double> ratio(lgtA.size()), corr(lgtA.size()), shape(lgtA.size());
    for (size_t i = 0; i < ratio.size(); ++i) {
        ratio[i] = (std::log10(cs.aB) + lgtB[i]) - (std::log10(cs.aA) + lgtA[i]);
        corr[i] = ratio[i] + sA[i] - sB[i];               // 施加 gain_B/gain_A = 10^(surf_A-surf_B)
        // 拟合 m 是校正场 = 1/m_true; 形状误差比较 logm_fit 与 -lgm_true (去 gauge)
        shape[i] = (-(sA[i] - fA.mean_surf)) - (-lgtA[i]);
    }
    auto ptp = [](const std::vector<double>& v) {
        return *std::max_element(v.begin(), v.end()) - *std::min_element(v.begin(), v.end());
    };
    double m = std::accumulate(shape.begin(), shape.end(), 0.0) / shape.size();
    double ss = 0.0; for (double z : shape) ss += (z - m) * (z - m);
    mt.field_ptp_before = (std::pow(10.0, ptp(ratio)) - 1.0) * 100.0;
    mt.field_ptp_after  = (std::pow(10.0, ptp(corr)) - 1.0) * 100.0;
    mt.shape_rms = (std::pow(10.0, std::sqrt(ss / shape.size())) - 1.0) * 100.0;
    mt.shape_ptp = (std::pow(10.0, ptp(shape)) - 1.0) * 100.0;
    // m 拟合幅度 (在星位置上)
    double lo = 1e300, hi = -1e300;
    for (size_t i = 0; i < cs.x.size(); ++i) {
        double z = -(poly_eval(cA, t, (cs.x[i] - x0) / sx, (cs.y[i] - y0) / sy) - fA.mean_surf);
        lo = std::min(lo, z); hi = std::max(hi, z);
    }
    mt.m_ptp = (std::pow(10.0, hi - lo) - 1.0) * 100.0;
    return mt;
}

int main() {
    printf("=== P1-SPATIAL-GAIN 独立 C++ Oracle (reverse_verify) ===\n");
    printf("模型: y_k = a_k*m_k(p)*(T+S0) + g_k(p) + n ; r=log10(F_instr/F_syn) 低阶曲面拟合\n");
    printf("判据阈值: C1 after<=%.1f%% (before>=%.1f%%) | C2 m<=%.1f%% field<=%.1f%% | C3 ratio>=%.1f | "
           "C4 dy<=%.0e ADU star>=%.1f%% | C5 ratio>=%.1f | C6 rel<=%.0e\n\n",
           THRESH_C1_AFTER_PTP, THRESH_C1_BEFORE_PTP, THRESH_C2_M_PTP, THRESH_C2_FIELD_PTP,
           THRESH_C3_RATIO, THRESH_C4_DY_ADU, THRESH_C4_STAR_PP, THRESH_C5_RATIO, THRESH_C6_REL);

    const int NMC = 60;
    int fails = 0;
    auto check = [&](const char* name, bool pass, const char* detail) {
        printf("  [%s] %s %s\n", pass ? "PASS" : "FAIL", name, detail);
        if (!pass) ++fails;
    };

    // ---- 正例 / 负例 / 阶数扫描 ----
    std::vector<std::vector<double>> floor1(3), floor2(3), floor3(3);
    for (int cfg = 0; cfg < 3; ++cfg) {
        bool neg = (cfg >= 1);
        double sig_int = (cfg == 2) ? 0.0 : 0.010;
        std::vector<double> f0(NMC), f1(NMC), f2(NMC), f3(NMC), m1(NMC), m2(NMC), m3(NMC), s1(NMC);
        for (int k = 0; k < NMC; ++k) {
            Rng rng(20260919ull + 7919ull * (uint64_t)(cfg * 1000 + k));
            Case cs = make_case(rng, 200, sig_int, 7.0, neg);
            Metrics a = evaluate(cs, 0), b = evaluate(cs, 1), c = evaluate(cs, 2), d = evaluate(cs, 3);
            f0[k] = a.field_ptp_before; f1[k] = b.field_ptp_after; f2[k] = c.field_ptp_after; f3[k] = d.field_ptp_after;
            m1[k] = b.m_ptp; m2[k] = c.m_ptp; m3[k] = d.m_ptp; s1[k] = b.shape_rms;
        }
        floor1[cfg] = m1; floor2[cfg] = m2; floor3[cfg] = m3;
        auto med = [](std::vector<double> v) { return median_of(v); };
        printf("[cfg%d] %s N=200 sigma_int=%.3f dex  (%d MC)\n", cfg,
               neg ? "NEG-CTRL m_true==1" : "SIGNAL m_true pp=7%", sig_int, NMC);
        printf("    field PTP: order0(before)=%.3f%%  o1=%.3f%%  o2=%.3f%%  o3=%.3f%%\n",
               med(f0), med(f1), med(f2), med(f3));
        printf("    m_fit PTP: o1=%.3f%%  o2=%.3f%%  o3=%.3f%%   | o1 shape RMS vs 1/m_true=%.3f%%\n",
               med(m1), med(m2), med(m3), med(s1));
        if (cfg == 0) {
            char buf[256];
            snprintf(buf, sizeof buf, "(before %.2f%% -> after %.2f%%, need <=%.1f%%)", med(f0), med(f1), THRESH_C1_AFTER_PTP);
            check("C1 正例: 低阶 m 显著压低帧间乘性残差场", med(f0) >= THRESH_C1_BEFORE_PTP && med(f1) <= THRESH_C1_AFTER_PTP, buf);
        }
        if (cfg == 1) {
            char buf[256];
            snprintf(buf, sizeof buf, "(m_ptp %.2f%% <= %.1f%%, field %.2f%% <= %.1f%%)", med(m1), THRESH_C2_M_PTP, med(f1), THRESH_C2_FIELD_PTP);
            check("C2 负例归零: m_true==1 时拟合 m 收到噪声底", med(m1) <= THRESH_C2_M_PTP && med(f1) <= THRESH_C2_FIELD_PTP, buf);
        }
        printf("\n");
    }

    // ---- C3 高阶有害: 用**负例噪声底**衡量 (正例的 m_ptp 含真值信号, 不能当底) ----
    {
        double p1 = percentile_of(floor1[1], 0.90), p2 = percentile_of(floor2[1], 0.90), p3 = percentile_of(floor3[1], 0.90);
        char buf[256];
        snprintf(buf, sizeof buf, "(负例 p90 底: o1=%.2f%% o2=%.2f%% o3=%.2f%%; o3/o1=%.2f, need >= %.1f)",
                 p1, p2, p3, p3 / std::max(p1, 1e-9), THRESH_C3_RATIO);
        check("C3 高阶有害: order3 噪声底远大于 order1", p3 >= THRESH_C3_RATIO * p1, buf);
        printf("    (参考) 负例中位底: o1=%.2f%% o2=%.2f%% o3=%.2f%%\n\n",
               median_of(floor1[1]), median_of(floor2[1]), median_of(floor3[1]));
    }

    // ---- C5 小样本退化 ----
    {
        std::vector<double> f2(NMC), m2(NMC);
        for (int k = 0; k < NMC; ++k) {
            Rng rng(20260919ull + 31337ull + (uint64_t)k);
            Case cs = make_case(rng, 20, 0.010, 7.0, true);   // 负例: 纯噪声底
            Metrics c = evaluate(cs, 2);
            f2[k] = c.field_ptp_after; m2[k] = c.m_ptp;
        }
        double p90 = percentile_of(m2, 0.90);
        double r = p90 / 7.0;
        char buf[256];
        snprintf(buf, sizeof buf, "(N=20 order2 伪 m 的 p90=%.2f%% = %.2f x 真值 pp 7%%, 危险判据需 >= %.1f)", p90, r, THRESH_C5_RATIO);
        check("C5 小样本高阶退化 => 必须 fail-closed/降阶", r >= THRESH_C5_RATIO, buf);
        printf("    N=20 order2: field PTP(neg)=%.3f%%  m_ptp(neg) med=%.3f%% p90=%.3f%%\n\n",
               median_of(f2), median_of(m2), p90);
    }

    // ---- C4 可辨识性 ----
    {
        Rng rng(7);
        std::vector<double> mterms = make_field(rng, 7.0, std::log10(1.07), true);
        std::vector<double> dterms(5, 0.0);
        dterms[0] = 30.0; dterms[1] = -20.0; dterms[2] = 12.0; dterms[4] = -9.0;
        double maxdy = 0.0, glo = 1e300, ghi = -1e300, rlo = 1e300, rhi = -1e300;
        for (int i = 0; i < 200; ++i) {
            double x = rng.uni() * W, y = rng.uni() * H;
            double m = std::pow(10.0, field_at(mterms, x, y));
            double g = field_at(dterms, x, y);           // 以 ADU 计的 delta 场 (未定标)
            double gs = g * (40.0 / 97.0);               // 缩放到 pp ~40 ADU
            double ds = gs;
            double y1 = m * S0 + gs;
            double y2 = (m + ds / S0) * S0 + (gs - ds);
            maxdy = std::max(maxdy, std::fabs(y1 - y2));
            glo = std::min(glo, y1); ghi = std::max(ghi, y1);
            double ratio = (m + ds / S0) / m;
            rlo = std::min(rlo, ratio); rhi = std::max(rhi, ratio);
        }
        char buf[256];
        snprintf(buf, sizeof buf, "(bg max|dy|=%.3e ADU <= %.0e; 梯度 pp=%.1f ADU; 星点流量比 pp=%.2f%% >= %.1f%%)",
                 maxdy, THRESH_C4_DY_ADU, ghi - glo, (rhi - rlo) * 100.0, THRESH_C4_STAR_PP);
        check("C4 背景退化 / 星点破缺", maxdy <= THRESH_C4_DY_ADU && (rhi - rlo) * 100.0 >= THRESH_C4_STAR_PP, buf);
        printf("\n");
    }

    // ---- C6 施加语义: 像素乘 10^(-surf) 后孔径流量按模型缩放 ----
    {
        Rng rng(99);
        std::vector<double> coef(3);
        coef[0] = 0.01; coef[1] = -0.02; coef[2] = 0.015;   // log10 m 线性场
        const int N = 256;
        std::vector<double> img((size_t)N * N), img2((size_t)N * N);
        // 平滑天光 (低阶多项式, 局部环可准确估计) + 高斯源; 这样"整帧乘 10^-s"
        // 才严格等价于"局部孔径流量乘 10^-s" (源与天光同步缩放)。
        for (int iy = 0; iy < N; ++iy) for (int ix = 0; ix < N; ++ix) {
            double x = 1000 + ix, y = 1000 + iy;
            double xn = (x - W / 2.0) / (W / 2.0), yn = (y - H / 2.0) / (H / 2.0);
            double s = coef[0] + coef[1] * xn + coef[2] * yn;
            double sky = 500.0 + 6.0 * xn + 4.0 * yn + 2.0 * xn * yn;
            img[(size_t)iy * N + ix] = sky;
            img2[(size_t)iy * N + ix] = sky * std::pow(10.0, -s);
        }
        // 在固定位置放高斯源 (sigma=2px, 峰值 3000 ADU) —— 源随整帧一起被缩放
        std::vector<std::pair<double,double>> srcs;
        for (int t = 0; t < 12; ++t) srcs.push_back({30 + rng.uni() * (N - 60), 30 + rng.uni() * (N - 60)});
        for (auto& sp : srcs) {
            for (int dy = -10; dy <= 10; ++dy) for (int dx = -10; dx <= 10; ++dx) {
                int px = (int)std::lround(sp.first) + dx, py = (int)std::lround(sp.second) + dy;
                if (px < 0 || px >= N || py < 0 || py >= N) continue;
                double v = 3000.0 * std::exp(-(dx * dx + dy * dy) / (2.0 * 2.0 * 2.0));
                img[(size_t)py * N + px] += v;
                img2[(size_t)py * N + px] += v;   // 源的乘性缩放稍后统一处理
            }
        }
        // 重算 img2 = img * 10^-s (含源)
        for (int iy = 0; iy < N; ++iy) for (int ix = 0; ix < N; ++ix) {
            double x = 1000 + ix, y = 1000 + iy;
            double xn = (x - W / 2.0) / (W / 2.0), yn = (y - H / 2.0) / (H / 2.0);
            double s = coef[0] + coef[1] * xn + coef[2] * yn;
            img2[(size_t)iy * N + ix] = img[(size_t)iy * N + ix] * std::pow(10.0, -s);
        }
        // 孔径 (圆盘 r=6) 与环 (10..16) 天光
        auto apflux = [&](const std::vector<double>& im, double cx, double cy) {
            double sum = 0, sky = 0; int nap = 0, nan = 0;
            std::vector<double> ann;
            for (int dy = -16; dy <= 16; ++dy) for (int dx = -16; dx <= 16; ++dx) {
                int px = (int)std::lround(cx) + dx, py = (int)std::lround(cy) + dy;
                if (px < 0 || px >= N || py < 0 || py >= N) continue;
                double d2 = dx * dx + dy * dy;
                double v = im[(size_t)py * N + px];
                if (d2 <= AP_R * AP_R) { sum += v; ++nap; }
                if (d2 >= ANN_IN * ANN_IN && d2 <= ANN_OUT * ANN_OUT) ann.push_back(v);
            }
            sky = median_of(ann); (void)nan;
            return sum - sky * nap;
        };
        double worst = 0.0;
        for (auto& sp : srcs) {
            double cx = sp.first, cy = sp.second;
            double f1 = apflux(img, cx, cy), f2 = apflux(img2, cx, cy);
            double x = 1000 + cx, y = 1000 + cy;
            double xn = (x - W / 2.0) / (W / 2.0), yn = (y - H / 2.0) / (H / 2.0);
            double s = coef[0] + coef[1] * xn + coef[2] * yn;
            double expect = std::pow(10.0, -s);
            worst = std::max(worst, std::fabs(f2 / f1 / expect - 1.0));
        }
        char buf[256];
        snprintf(buf, sizeof buf, "(孔径流量缩放 vs 模型值: max rel err=%.2e <= %.0e)", worst, THRESH_C6_REL);
        check("C6 施加语义: 像素乘 10^(-surf) == 孔径流量乘模型值", worst <= THRESH_C6_REL, buf);
    }

    printf("\n=== 结论: %s (%d FAIL) ===\n", fails == 0 ? "ALL PASS" : "HAS FAILURE", fails);
    return fails == 0 ? 0 : 1;
}
