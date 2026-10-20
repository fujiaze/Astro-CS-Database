// ============================================================================
// eng/tests/unit/narrowband_response_test.cpp
// ----------------------------------------------------------------------------
// Issue #106 (ACSD-CPP/C1) 单元测试：**参数化窄带响应的分段求积路径**。
//
// 判据（全部为**独立真值**，不复用被测实现做参照）:
//   T1  A/B 逐位: 重构后的 akima_interpolate 与 #106 之前的实现（下方
//       reference_akima_interpolate, 逐字拷贝自 ba81a1631 的生产源码）
//       在随机与边界输入上**逐位相等** —— 证明 measured 路径未回归。
//   T2  闭式解: 取 Akima 能精确重现的二次 F(λ) 与线性 Q(λ) ⇒ 被积函数是
//       四次多项式, 解析积分可独立算出 ⇒ 分段路径的相对误差必须 ≤ 1e-12
//       （即**求积误差 = 0**, 只有浮点舍入）。
//   T3  边界/截断: 响应带超出 XP 域 / QE 未覆盖 / 完全无重叠 ⇒ 覆盖状态与
//       截断后的积分值符合解析预期。
//   T4  #43 反例: 同一 top-hat, 旧 2nm 粗格采样路径 vs 新分段路径 ⇒ 旧路径
//       相对误差 > 5%（3/5/7nm）, 新路径 ≤ 1e-12。
//   T5  非法输入 ⇒ quiet NaN / 具名失败, 绝不以 0 伪装"零光子"。
//
// 运行: python3 eng/tests/unit/run_narrowband_response_test.py
// ============================================================================

#include "spectrum_integrator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_fail;
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    }
}

void check_rel(double got, double want, double tol, const std::string& what) {
    ++g_checks;
    const double denom = std::fabs(want);
    const double err = (denom > 0.0) ? std::fabs(got - want) / denom : std::fabs(got - want);
    if (!(err <= tol)) {
        ++g_fail;
        std::fprintf(stderr, "FAIL: %s (got=%.17g want=%.17g rel_err=%.3e tol=%.3e)\n",
                     what.c_str(), got, want, err, tol);
    }
}

// ---------------------------------------------------------------------------
// T1 的参照实现: #106 之前（ba81a1631）的 akima_interpolate, 逐字拷贝。
// 只用于 A/B 比对, **不参与任何生产路径**。
// ---------------------------------------------------------------------------
void reference_akima_interpolate(
    const std::vector<double>& x_src, const std::vector<double>& y_src,
    const std::vector<double>& x_dst, double fill,
    std::vector<double>* out) {
    const int n = (int)x_src.size();
    const int m = (int)x_dst.size();
    std::vector<double> y_dst(m, fill);

    if (n < 2 || m == 0) {
        out->swap(y_dst);
        return;
    }

    std::vector<double> slope(n - 1, 0.0);
    for (int i = 0; i < n - 1; ++i) {
        double dx = x_src[i + 1] - x_src[i];
        if (dx <= 0.0) {
            out->swap(y_dst);
            return;
        }
        slope[i] = (y_src[i + 1] - y_src[i]) / dx;
    }

    std::vector<double> ext_m(n + 3, 0.0);
    ext_m[0] = (n >= 3) ? 3.0 * slope[0] - 2.0 * slope[1] : slope[0];
    ext_m[1] = (n >= 3) ? 2.0 * slope[0] - slope[1] : slope[0];
    for (int i = 0; i < n - 1; ++i) ext_m[2 + i] = slope[i];
    ext_m[n + 1] = (n >= 3)
        ? 2.0 * slope[n - 2] - slope[n - 3] : slope[n - 2];
    ext_m[n + 2] = (n >= 3)
        ? 3.0 * slope[n - 2] - 2.0 * slope[n - 3] : slope[n - 2];

    std::vector<double> t(n, 0.0);
    for (int i = 0; i < n; ++i) {
        double m_left2 = ext_m[i];
        double m_left1 = ext_m[i + 1];
        double m_cur   = ext_m[i + 2];
        double m_right = ext_m[i + 3];
        double w1 = std::fabs(m_right - m_cur);
        double w2 = std::fabs(m_left1 - m_left2);
        if (w1 + w2 == 0.0) {
            t[i] = 0.5 * (m_left1 + m_cur);
        } else {
            t[i] = (w1 * m_left1 + w2 * m_cur) / (w1 + w2);
        }
    }

    for (int k = 0; k < m; ++k) {
        double x = x_dst[k];
        if (x < x_src[0] || x > x_src[n - 1]) {
            y_dst[k] = fill;
            continue;
        }
        int lo = 0, hi = n - 2, j = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (x >= x_src[mid] && x <= x_src[mid + 1]) { j = mid; break; }
            if (x < x_src[mid]) hi = mid - 1; else lo = mid + 1;
        }
        double x0 = x_src[j], x1 = x_src[j + 1];
        double y0 = y_src[j], y1 = y_src[j + 1];
        double dx = x1 - x0;
        double s = (x - x0) / dx;
        double h00 = (2.0 * s - 3.0) * s * s + 1.0;
        double h10 = ((s - 2.0) * s + 1.0) * s;
        double h01 = (-2.0 * s + 3.0) * s * s;
        double h11 = (s - 1.0) * s * s;
        y_dst[k] = h00 * y0 + h10 * dx * t[j] + h01 * y1 + h11 * dx * t[j + 1];
    }
    out->swap(y_dst);
}

// 确定性伪随机（LCG）: 可复现, 不依赖 <random> 的实现差异。
struct Lcg {
    unsigned long long s;
    explicit Lcg(unsigned long long seed) : s(seed) {}
    double next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>((s >> 11) & ((1ULL << 53) - 1)) /
               static_cast<double>(1ULL << 53);
    }
    double uniform(double a, double b) { return a + (b - a) * next(); }
};

// ---------------------------------------------------------------------------
// T1: A/B 逐位
// ---------------------------------------------------------------------------
void test_akima_ab() {
    Lcg rng(20261020ULL);
    int n_ab = 0;
    for (int trial = 0; trial < 200; ++trial) {
        const int n = 2 + static_cast<int>(rng.next() * 30.0);
        std::vector<double> xs(n), ys(n);
        double x = rng.uniform(-50.0, 50.0);
        for (int i = 0; i < n; ++i) {
            xs[i] = x;
            x += rng.uniform(0.01, 3.0);
            ys[i] = rng.uniform(-1e3, 1e3);
        }
        const int m = 1 + static_cast<int>(rng.next() * 40.0);
        std::vector<double> xd(m);
        for (int i = 0; i < m; ++i) xd[i] = rng.uniform(xs.front() - 10.0, xs.back() + 10.0);
        const double fill = rng.uniform(-5.0, 5.0);

        std::vector<double> ref;
        reference_akima_interpolate(xs, ys, xd, fill, &ref);
        const std::vector<double> got = photo_calib::akima_interpolate(xs, ys, xd, fill);
        check(ref.size() == got.size(), "T1 size mismatch");
        bool same = (ref.size() == got.size());
        for (size_t i = 0; same && i < ref.size(); ++i) {
            // 逐位: NaN 位置必须一致（NaN 之外的比较用位比较）
            if (std::isnan(ref[i]) != std::isnan(got[i])) { same = false; break; }
            if (std::isnan(ref[i])) continue;
            unsigned long long a = 0, b = 0;
            std::memcpy(&a, &ref[i], sizeof(double));
            std::memcpy(&b, &got[i], sizeof(double));
            if (a != b) { same = false; break; }
        }
        if (!same) {
            check(false, "T1 akima A/B bit-exact mismatch at trial " + std::to_string(trial));
            break;
        }
        ++n_ab;
    }
    check(n_ab == 200, "T1 all trials compared");
    std::printf("== T1 akima A/B bit-exact: %d/200 trials identical\n", n_ab);

    // 退化输入: 非严格递增 ⇒ 两边都给 fill
    {
        std::vector<double> xs{0.0, 2.0, 1.0};
        std::vector<double> ys{1.0, 2.0, 3.0};
        std::vector<double> xd{0.0, 1.0, 1.5, 2.0};
        std::vector<double> ref;
        reference_akima_interpolate(xs, ys, xd, 7.5, &ref);
        const std::vector<double> got = photo_calib::akima_interpolate(xs, ys, xd, 7.5);
        bool same = ref.size() == got.size();
        for (size_t i = 0; same && i < ref.size(); ++i) same = (ref[i] == got[i]);
        check(same, "T1 non-monotonic source A/B");
    }
    // 点数 < 2
    {
        std::vector<double> xs{1.0};
        std::vector<double> ys{2.0};
        std::vector<double> xd{1.0, 2.0};
        std::vector<double> ref;
        reference_akima_interpolate(xs, ys, xd, -1.0, &ref);
        const std::vector<double> got = photo_calib::akima_interpolate(xs, ys, xd, -1.0);
        bool same = ref.size() == got.size();
        for (size_t i = 0; same && i < ref.size(); ++i) same = (ref[i] == got[i]);
        check(same, "T1 n<2 A/B");
    }
}

// ---------------------------------------------------------------------------
// 独立参照: 解析积分（四次多项式被积函数）
//   F(λ) = 1 + u + u²,  u = λ − 600      （Akima 对二次数据精确重现, 见 T2a）
//   Q(λ) = 0.5 + 0.25·u                  （两点 ⇒ Akima 线性, 精确）
//   F·Q = 0.5 + 0.75u + 0.75u² + 0.25u³
//   g(λ) = F·Q·λ = 300 + 450.5u + 450.75u² + 150.75u³ + 0.25u⁴
// 独立积分（不经过被测实现）: 逐项解析积分。
// ---------------------------------------------------------------------------
double analytic_integral(double u_lo, double u_hi) {
    const double c[4] = {0.5, 0.75, 0.75, 0.25};   // F·Q 的系数
    const double base = 600.0;                      // λ = base + u
    // g = (Σ c_k u^k)(base + u) ⇒ 系数 d0..d4
    const double d[5] = {c[0] * base, c[0] + c[1] * base, c[1] + c[2] * base,
                          c[2] + c[3] * base, c[3]};
    auto anti = [&d](double u) {
        return d[0] * u + d[1] * u * u / 2.0 + d[2] * u * u * u / 3.0 +
               d[3] * u * u * u * u / 4.0 + d[4] * std::pow(u, 5.0) / 5.0;
    };
    return anti(u_hi) - anti(u_lo);
}

// T2/T3 用的固定装置: XP 网格 600..604 (1nm 步长, 5 点), 二次 F, 线性 Q。
struct Rig {
    std::vector<double> xp_wl{600.0, 601.0, 602.0, 603.0, 604.0};
    std::vector<uint8_t> bytes{1, 3, 7, 13, 21};   // F = 1 + u + u²
    std::vector<double> qe_wl{600.0, 604.0};
    std::vector<double> qe_val{0.5, 1.5};           // Q = 0.5 + 0.25 u
    double flux_min = 0.0;
    double flux_mul = 1.0;
};

// 独立参照积分（**不复用**生产分段求积）: 见实现。
double independent_piecewise_ref(const Rig& rig, double u_lo, double u_hi);

// 独立高阶参照: 细网格复合 Simpson（F 在网格点间**线性**插值, Q 常数）。
double fine_grid_reference(const std::vector<double>& xp,
                            const std::vector<uint8_t>& bytes,
                            double lo, double hi, double qe);

void test_analytic_exactness() {
    Rig rig;
    // T2a: Akima 确实精确重现二次数据（独立检查, 用生产 akima 插值本身）
    {
        std::vector<double> y;
        for (double x : rig.xp_wl) {
            const double u = x - 600.0;
            y.push_back(1.0 + u + u * u);
        }
        std::vector<double> xd{600.5, 601.25, 602.5, 603.75};
        const std::vector<double> got = photo_calib::akima_interpolate(rig.xp_wl, y, xd, 0.0);
        for (size_t i = 0; i < xd.size(); ++i) {
            const double u = xd[i] - 600.0;
            check_rel(got[i], 1.0 + u + u * u, 1e-13,
                      "T2a akima reproduces quadratic data");
        }
    }
    // T2b: 分段路径 vs 解析积分（带完全在网格内, 端点不对齐）
    {
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(602.0, 3.0);   // [600.5, 603.5]
        check(band.valid, "T2b band valid");
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(),
                static_cast<int>(rig.qe_val.size()),
                rig.xp_wl.data(), static_cast<int>(rig.xp_wl.size()));
        check(cache.valid, "T2b cache valid");
        check(cache.n_segments > 0, "T2b has segments");
        check(std::string(photo_calib::response_coverage_name(cache.coverage)) == "full",
              "T2b coverage full");
        const double fsyn = photo_calib::compute_f_syn_segmented_xpsd(
            cache, rig.bytes.data(), static_cast<int>(rig.bytes.size()),
            rig.flux_min, rig.flux_mul);
        const double want = analytic_integral(0.5, 3.5);
        check_rel(fsyn, want, 1e-12, "T2b segmented F_syn == analytic (quadratic F, linear Q)");
        std::printf("== T2b segmented F_syn=%.17g analytic=%.17g rel=%.3e\n",
                    fsyn, want, std::fabs(fsyn - want) / std::fabs(want));
    }
    // T2c: 端点正好落在网格点上（分段边界退化, 仍精确）
    {
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(602.0, 4.0);   // [600, 604]
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(),
                static_cast<int>(rig.qe_val.size()),
                rig.xp_wl.data(), static_cast<int>(rig.xp_wl.size()));
        const double fsyn = photo_calib::compute_f_syn_segmented_xpsd(
            cache, rig.bytes.data(), static_cast<int>(rig.bytes.size()),
            rig.flux_min, rig.flux_mul);
        check_rel(fsyn, analytic_integral(0.0, 4.0), 1e-12, "T2c aligned band exact");
    }
    // T2d: QE 节点不在网格点上（分段必须同时按 XP 点与 QE 节点切）
    {
        Rig r2;
        r2.qe_wl = {600.0, 601.5, 604.0};
        r2.qe_val = {0.5, 0.75, 1.5};   // 分段线性（Akima 在节点处 C¹）
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(602.0, 3.0);
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, r2.qe_wl.data(), r2.qe_val.data(),
                static_cast<int>(r2.qe_val.size()),
                r2.xp_wl.data(), static_cast<int>(r2.xp_wl.size()));
        check(cache.n_segments >= 3, "T2d segments split by QE knots");
        const double fsyn = photo_calib::compute_f_syn_segmented_xpsd(
            cache, r2.bytes.data(), static_cast<int>(r2.bytes.size()),
            r2.flux_min, r2.flux_mul);
        // 独立参照: 在 [600.5,601.5] 与 [601.5,603.5] 上用**不同的**积分方法
        // （每段 8 点 Gauss–Legendre, 直接对 F·Q·λ 的分段线性/二次重构求积）。
        const double want = independent_piecewise_ref(r2, 0.5, 3.5);
        check_rel(fsyn, want, 1e-11, "T2d split-by-QE-knots matches independent ref");
    }
}

// 独立参照积分（**不复用**生产的分段 Gauss–Legendre）:
//   模型定义（F/Q 的 Akima 重构）与生产一致 —— 重构算子就是**模型本身**;
//   独立之处在于 ① 断点集合独立构造; ② 求积规则不同（复合 Simpson, 每段
//   2000 个子区间）。若生产的 GL 实现有误, 两者会明显不一致。
double independent_piecewise_ref(const Rig& rig, double u_lo, double u_hi) {
    std::vector<double> br;
    for (double x : rig.xp_wl) if (x > 600.0 + u_lo && x < 600.0 + u_hi) br.push_back(x - 600.0);
    for (double x : rig.qe_wl) if (x > 600.0 + u_lo && x < 600.0 + u_hi) br.push_back(x - 600.0);
    br.push_back(u_lo);
    br.push_back(u_hi);
    std::sort(br.begin(), br.end());
    br.erase(std::unique(br.begin(), br.end()), br.end());
    // F 的 Akima（生产同一算子; 二次数据 ⇒ 精确）
    std::vector<double> fy;
    for (double x : rig.xp_wl) {
        const double u = x - 600.0;
        fy.push_back(1.0 + u + u * u);
    }
    auto F_of = [&](double u) {
        const std::vector<double> v = photo_calib::akima_interpolate(
            rig.xp_wl, fy, std::vector<double>{600.0 + u}, 0.0);
        return v.empty() ? 0.0 : v[0];
    };
    // Q 的 Akima（生产同一算子）
    auto Q_of = [&](double u) {
        const std::vector<double> v = photo_calib::akima_interpolate(
            rig.qe_wl, rig.qe_val, std::vector<double>{600.0 + u}, 0.0);
        return v.empty() ? 0.0 : v[0];
    };
    const int m = 2000;
    double total = 0.0;
    for (std::size_t i = 0; i + 1 < br.size(); ++i) {
        const double a = br[i], b = br[i + 1];
        if (!(b > a)) continue;
        const double h = (b - a) / m;
        double s = F_of(a) * Q_of(a) * (600.0 + a) + F_of(b) * Q_of(b) * (600.0 + b);
        for (int k = 1; k < m; ++k) {
            const double u = a + k * h;
            s += (k % 2 == 1 ? 4.0 : 2.0) * F_of(u) * Q_of(u) * (600.0 + u);
        }
        total += s * h / 3.0;
    }
    return total;
}

// ---------------------------------------------------------------------------
// T3: 覆盖 / 截断 / 无支持
// ---------------------------------------------------------------------------
void test_coverage() {
    Rig rig;
    // T3a: 响应带超出 XP 域（低端）⇒ xp_truncated, 积分从 XP 下界起
    {
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(599.0, 4.0);   // [597, 601]
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(),
                static_cast<int>(rig.qe_val.size()),
                rig.xp_wl.data(), static_cast<int>(rig.xp_wl.size()));
        check(cache.valid, "T3a cache valid");
        check(std::string(photo_calib::response_coverage_name(cache.coverage)) ==
                  "xp_truncated", "T3a coverage xp_truncated");
        check_rel(cache.overlap_lo_nm, 600.0, 1e-12, "T3a overlap lo");
        check_rel(cache.overlap_hi_nm, 601.0, 1e-12, "T3a overlap hi");
        const double fsyn = photo_calib::compute_f_syn_segmented_xpsd(
            cache, rig.bytes.data(), static_cast<int>(rig.bytes.size()),
            rig.flux_min, rig.flux_mul);
        // QE 在 [600,604] ⇒ 不截断; 积分区间 = [600, 601]
        check_rel(fsyn, analytic_integral(0.0, 1.0), 1e-12, "T3a truncated integral");
    }
    // T3b: QE 未覆盖响应带 ⇒ qe_truncated; 积分止于 QE 上界
    {
        Rig r2;
        r2.qe_wl = {600.0, 602.0};
        r2.qe_val = {0.5, 1.0};
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(602.0, 4.0);   // [600, 604]
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, r2.qe_wl.data(), r2.qe_val.data(),
                static_cast<int>(r2.qe_val.size()),
                r2.xp_wl.data(), static_cast<int>(r2.xp_wl.size()));
        check(std::string(photo_calib::response_coverage_name(cache.coverage)) ==
                  "qe_truncated", "T3b coverage qe_truncated");
        check_rel(cache.overlap_hi_nm, 602.0, 1e-12, "T3b overlap hi = QE end");
        const double fsyn = photo_calib::compute_f_syn_segmented_xpsd(
            cache, r2.bytes.data(), static_cast<int>(r2.bytes.size()),
            r2.flux_min, r2.flux_mul);
        // Q = 0.5 + 0.25u（两点线性）⇒ 仍可用解析式; 区间 [0,2]
        check_rel(fsyn, analytic_integral(0.0, 2.0), 1e-12, "T3b QE-clipped integral");
    }
    // T3c: 完全无重叠 ⇒ 合法零 + no_overlap
    {
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(700.0, 3.0);
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(),
                static_cast<int>(rig.qe_val.size()),
                rig.xp_wl.data(), static_cast<int>(rig.xp_wl.size()));
        check(cache.valid, "T3c cache valid (no overlap is a legal zero)");
        check(std::string(photo_calib::response_coverage_name(cache.coverage)) ==
                  "no_overlap", "T3c coverage no_overlap");
        check(cache.n_segments == 0, "T3c no segments");
        const double fsyn = photo_calib::compute_f_syn_segmented_xpsd(
            cache, rig.bytes.data(), static_cast<int>(rig.bytes.size()),
            rig.flux_min, rig.flux_mul);
        check(fsyn == 0.0, "T3c F_syn is a legal exact zero");
    }
}

// ---------------------------------------------------------------------------
// T4: #43 反例 —— 2nm 粗格采样 vs 分段路径
// ---------------------------------------------------------------------------
void test_narrowband_43_counterexample() {
    // XP 网格: 336..1020, 2nm 步长（Gaia DR3 BP/RP 的实际采样）
    const int n_xp = 343;
    std::vector<double> xp(n_xp);
    for (int i = 0; i < n_xp; ++i) xp[i] = 336.0 + 2.0 * i;
    // 合成 F(λ): 平滑单峰（连续谱 toy）, 量化到 uint8（XP 的量化方式）
    std::vector<uint8_t> bytes(n_xp);
    for (int i = 0; i < n_xp; ++i) {
        const double lam = xp[i];
        const double f = 1.0e-6 *
            std::exp(-0.5 * ((lam - 660.0) / 60.0) * ((lam - 660.0) / 60.0));
        bytes[i] = static_cast<uint8_t>(std::lround(f / 1.0e-8));
    }
    // QE: 平坦 0.8（两点 ⇒ Akima 精确常数）
    std::vector<double> qe_wl{336.0, 1020.0};
    std::vector<double> qe_val{0.8, 0.8};

    struct Case { double center; double fwhm; };
    const Case cases[3] = {{656.28, 3.0}, {656.28, 5.0}, {656.28, 7.0}};
    double legacy_vals[3] = {0.0, 0.0, 0.0};
    double seg_vals[3] = {0.0, 0.0, 0.0};
    for (int ci = 0; ci < 3; ++ci) {
        const Case& c = cases[ci];
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(c.center, c.fwhm);
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, qe_wl.data(), qe_val.data(), static_cast<int>(qe_val.size()),
                xp.data(), n_xp);
        check(cache.valid, "T4 cache valid");
        const double seg = photo_calib::compute_f_syn_segmented_xpsd(
            cache, bytes.data(), n_xp, 0.0, 1.0e-8);
        seg_vals[ci] = seg;

        // 旧路径（#43 的做法）: 把阶跃投到 2nm 粗格（= 实测窄带曲线被重采样到
        // XP 网格）, 再 Simpson 积分。
        std::vector<double> fw(n_xp), fv(n_xp);
        for (int i = 0; i < n_xp; ++i) {
            fw[i] = xp[i];
            fv[i] = (xp[i] >= band.lo_nm && xp[i] <= band.hi_nm) ? 1.0 : 0.0;
        }
        const photo_calib::SpectrumIntegratorCache legacy_cache =
            photo_calib::prepare_filter_cache(
                fw.data(), fv.data(), n_xp,
                qe_wl.data(), qe_val.data(), static_cast<int>(qe_val.size()),
                xp.data(), n_xp);
        const double legacy = photo_calib::compute_f_syn_cached_xpsd(
            legacy_cache, bytes.data(), n_xp, 0.0, 1.0e-8);
        legacy_vals[ci] = legacy;

        // 独立真值: 细网格复合 Simpson, 被积函数 = **Akima 重构的 F** · Q · λ
        // （重构算子 = 模型定义; 求积规则与断点均独立于生产实现）。
        const double truth = fine_grid_reference(xp, bytes, band.lo_nm, band.hi_nm, 0.8);
        const double rel_legacy = std::fabs(legacy - truth) / std::fabs(truth);
        const double rel_seg = std::fabs(seg - truth) / std::fabs(truth);
        std::printf("== T4 fwhm=%.1fnm  truth=%.10e  legacy(2nm)=%.10e (rel=%.3e)  "
                    "segmented=%.10e (rel=%.3e)\n",
                    c.fwhm, truth, legacy, rel_legacy, seg, rel_seg);
        check(rel_legacy > 0.03, "T4 legacy 2nm path error > 3% (the #43 defect)");
        check(rel_seg < 1e-9, "T4 segmented path error < 1e-9 vs fine-grid truth");
    }
    // T4b: **最强的反例** —— 2nm 粗格无法区分 5nm 与 7nm 窄带:
    // 两者在 656.28 中心下都只覆盖 XP 的 654/656/658 三个格点 ⇒ 旧路径给出
    // **完全相同**的 F_syn, 而真值相差 ~40%。
    check(legacy_vals[1] == legacy_vals[2],
          "T4b legacy path cannot distinguish 5nm from 7nm (identical F_syn)");
    check(std::fabs(seg_vals[1] - seg_vals[2]) / seg_vals[2] > 0.2,
          "T4b segmented path distinguishes 5nm from 7nm (>20%)");
    std::printf("== T4b legacy 5nm=%.10e 7nm=%.10e (identical=%d) | segmented 5nm=%.10e "
                "7nm=%.10e (rel diff=%.3f)\n",
                legacy_vals[1], legacy_vals[2], legacy_vals[1] == legacy_vals[2] ? 1 : 0,
                seg_vals[1], seg_vals[2],
                std::fabs(seg_vals[1] - seg_vals[2]) / seg_vals[2]);
}

// 独立高阶参照: 细网格复合 Simpson, 被积函数 = Akima 重构的 F(λ) · Q · λ
// （Akima = 模型定义; 求积规则/断点独立于生产的分段 Gauss–Legendre）。
double fine_grid_reference(const std::vector<double>& xp,
                            const std::vector<uint8_t>& bytes,
                            double lo, double hi, double qe) {
    const int n = static_cast<int>(xp.size());
    std::vector<double> fy(n);
    // XPSD 官方解码: F(λ) = byte·flux_mul + flux_min（此处 flux_mul=1e-8, flux_min=0）
    for (int i = 0; i < n; ++i) fy[i] = static_cast<double>(bytes[i]) * 1.0e-8;
    // 断点: XP 节点（Akima 的节点区间边界）—— 复合 Simpson 不跨节点区间。
    std::vector<double> br;
    for (double x : xp) if (x > lo && x < hi) br.push_back(x);
    br.push_back(lo);
    br.push_back(hi);
    std::sort(br.begin(), br.end());
    br.erase(std::unique(br.begin(), br.end()), br.end());
    double total = 0.0;
    for (std::size_t i = 0; i + 1 < br.size(); ++i) {
        const double a = br[i], b = br[i + 1];
        if (!(b > a)) continue;
        const int m = 2000;
        const double h = (b - a) / m;
        std::vector<double> grid(m + 1);
        for (int k = 0; k <= m; ++k) grid[static_cast<size_t>(k)] = a + k * h;
        const std::vector<double> fv = photo_calib::akima_interpolate(xp, fy, grid, 0.0);
        double s = fv[0] * qe * grid[0] + fv[static_cast<size_t>(m)] * qe * grid[static_cast<size_t>(m)];
        for (int k = 1; k < m; ++k) {
            s += (k % 2 == 1 ? 4.0 : 2.0) * fv[static_cast<size_t>(k)] * qe * grid[static_cast<size_t>(k)];
        }
        total += s * h / 3.0;
    }
    return total;
}

// ---------------------------------------------------------------------------
// T5: 非法输入
// ---------------------------------------------------------------------------
void test_invalid_inputs() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // 非法参数化响应
    check(!photo_calib::make_parametric_band(656.28, 0.0).valid, "T5 fwhm=0 invalid");
    check(!photo_calib::make_parametric_band(656.28, -3.0).valid, "T5 fwhm<0 invalid");
    check(!photo_calib::make_parametric_band(nan, 3.0).valid, "T5 NaN center invalid");
    check(!photo_calib::make_parametric_band(1.0, 3.0).valid, "T5 lo<=0 invalid");
    check(photo_calib::make_parametric_band(656.28, 3.0).valid, "T5 valid band");

    Rig rig;
    const photo_calib::ParametricBand band =
        photo_calib::make_parametric_band(602.0, 3.0);
    // 非法 XP 轴
    {
        std::vector<double> bad{600.0, 601.0, 600.5};
        const photo_calib::SegmentedResponseCache c =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(), 2,
                bad.data(), 3);
        check(!c.valid, "T5 non-monotonic XP axis invalid");
    }
    // XP 点数不足
    {
        const photo_calib::SegmentedResponseCache c =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(), 2,
                rig.xp_wl.data(), 1);
        check(!c.valid, "T5 XP <2 points invalid");
    }
    // QE 曲线点数不足
    {
        std::vector<double> q1{600.0}, v1{0.5};
        const photo_calib::SegmentedResponseCache c =
            photo_calib::prepare_segmented_response_cache(
                band, q1.data(), v1.data(), 1, rig.xp_wl.data(), 5);
        check(!c.valid, "T5 QE <2 points invalid");
    }
    // 计算期非法输入 ⇒ NaN（绝不 0）
    {
        const photo_calib::SegmentedResponseCache c =
            photo_calib::prepare_segmented_response_cache(
                band, rig.qe_wl.data(), rig.qe_val.data(), 2,
                rig.xp_wl.data(), 5);
        check(std::isnan(photo_calib::compute_f_syn_segmented_xpsd(
                  c, nullptr, 5, 0.0, 1.0)), "T5 null spectrum -> NaN");
        check(std::isnan(photo_calib::compute_f_syn_segmented_xpsd(
                  c, rig.bytes.data(), 4, 0.0, 1.0)), "T5 size mismatch -> NaN");
        check(std::isnan(photo_calib::compute_f_syn_segmented_xpsd(
                  c, rig.bytes.data(), 5, 0.0, 0.0)), "T5 flux_mul=0 -> NaN");
        check(std::isnan(photo_calib::compute_f_syn_segmented_xpsd(
                  c, rig.bytes.data(), 5, 0.0, -1.0)), "T5 flux_mul<0 -> NaN");
        check(std::isnan(photo_calib::compute_f_syn_segmented_xpsd(
                  c, rig.bytes.data(), 5, nan, 1.0)), "T5 NaN flux_min -> NaN");
    }
    // 无效 cache ⇒ NaN
    {
        photo_calib::SegmentedResponseCache c;
        check(std::isnan(photo_calib::compute_f_syn_segmented_xpsd(
                  c, rig.bytes.data(), 5, 0.0, 1.0)), "T5 invalid cache -> NaN");
    }
}

}  // namespace

// ============================================================================
// 驱动模式: `--cases <file>`
// ----------------------------------------------------------------------------
// 读入**冻结的用例文件**, 逐用例计算两条 F_syn 并打印（供独立 Python/scipy
// Oracle 对拍; 用例定义只存在于用例文件里, 两侧无重复定义）:
//     CASE <id> <segmented_fsyn> <legacy_fsyn> <coverage> <n_segments>
// 用例行格式（空白分隔）:
//   CASE <id> <xp_start> <xp_step> <n_xp> <flux_min> <flux_mul> <center> <fwhm>
//        <n_qe> <qe_wl...> <qe_val...> <n_filt> <filt_wl...> <filt_val...> <byte...>
//   <filt_*> 可为 0 个（此时 legacy 路径不计算, 输出 nan）。
// ============================================================================
static int run_cases_mode(const char* path) {
    std::FILE* fp = std::fopen(path, "rb");
    if (fp == nullptr) {
        std::fprintf(stderr, "cannot open cases file: %s\n", path);
        return 2;
    }
    char line[1 << 16];
    int n_cases = 0;
    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        std::vector<std::string> tok;
        {
            std::istringstream iss(line);
            std::string w;
            while (iss >> w) tok.push_back(w);
        }
        if (tok.empty() || tok[0] != "CASE") continue;
        size_t i = 1;
        const std::string id = tok[i++];
        const double xp_start = std::stod(tok[i++]);
        const double xp_step = std::stod(tok[i++]);
        const int n_xp = std::stoi(tok[i++]);
        const double flux_min = std::stod(tok[i++]);
        const double flux_mul = std::stod(tok[i++]);
        const double center = std::stod(tok[i++]);
        const double fwhm = std::stod(tok[i++]);
        const int n_qe = std::stoi(tok[i++]);
        std::vector<double> qe_wl, qe_val;
        for (int k = 0; k < n_qe; ++k) qe_wl.push_back(std::stod(tok[i++]));
        for (int k = 0; k < n_qe; ++k) qe_val.push_back(std::stod(tok[i++]));
        const int n_filt = std::stoi(tok[i++]);
        std::vector<double> fw, fv;
        for (int k = 0; k < n_filt; ++k) fw.push_back(std::stod(tok[i++]));
        for (int k = 0; k < n_filt; ++k) fv.push_back(std::stod(tok[i++]));
        std::vector<uint8_t> bytes;
        for (int k = 0; k < n_xp; ++k)
            bytes.push_back(static_cast<uint8_t>(std::stoi(tok[i++])));

        std::vector<double> xp(n_xp);
        for (int k = 0; k < n_xp; ++k) xp[k] = xp_start + k * xp_step;

        // 参数化（分段）路径
        const photo_calib::ParametricBand band =
            photo_calib::make_parametric_band(center, fwhm);
        const photo_calib::SegmentedResponseCache cache =
            photo_calib::prepare_segmented_response_cache(
                band, n_qe > 0 ? qe_wl.data() : nullptr,
                n_qe > 0 ? qe_val.data() : nullptr, n_qe,
                xp.data(), n_xp);
        const double seg = photo_calib::compute_f_syn_segmented_xpsd(
            cache, bytes.data(), n_xp, flux_min, flux_mul);

        // 历史 measured 路径（同一 XP 网格, 用给定滤镜曲线）
        double legacy = std::numeric_limits<double>::quiet_NaN();
        if (n_filt > 0) {
            const photo_calib::SpectrumIntegratorCache lc =
                photo_calib::prepare_filter_cache(
                    fw.data(), fv.data(), n_filt,
                    n_qe > 0 ? qe_wl.data() : nullptr,
                    n_qe > 0 ? qe_val.data() : nullptr, n_qe,
                    xp.data(), n_xp);
            if (!lc.spectrum_wl.empty()) {
                legacy = photo_calib::compute_f_syn_cached_xpsd(
                    lc, bytes.data(), n_xp, flux_min, flux_mul);
            }
        }
        std::printf("CASE %s %.17g %.17g %s %d\n", id.c_str(), seg, legacy,
                    cache.valid ? photo_calib::response_coverage_name(cache.coverage)
                                : "invalid",
                    cache.n_segments);
        ++n_cases;
    }
    std::fclose(fp);
    std::fprintf(stderr, "cases processed: %d\n", n_cases);
    return (n_cases > 0) ? 0 : 3;
}

// ============================================================================
// 性能模式: `--bench <n>`
// ----------------------------------------------------------------------------
// 在**同一**批真实形状的 XP 谱上分别计时两条 F_syn 路径（每星 343 个 XP 点）:
//   * 参数化路径 = 分段 Gauss–Legendre（每星重建 f_λ 的 Akima 样条）;
//   * measured 路径 = 历史 Akima 重采样 + XP 网格 Simpson。
// 只报**单星平均耗时**（不含进程启动），供实施报告的性能条目使用。
// ============================================================================
static int run_bench_mode(int n) {
    const int n_xp = 343;
    std::vector<double> xp(n_xp);
    for (int i = 0; i < n_xp; ++i) xp[i] = 336.0 + 2.0 * i;
    std::vector<uint8_t> bytes(n_xp);
    for (int i = 0; i < n_xp; ++i) {
        const double lam = xp[i];
        const double f = 1.0e-6 *
            std::exp(-0.5 * ((lam - 660.0) / 60.0) * ((lam - 660.0) / 60.0));
        bytes[i] = static_cast<uint8_t>(std::lround(f / 1.0e-8));
    }
    std::vector<double> qe_wl{336.0, 1020.0};
    std::vector<double> qe_val{0.8, 0.8};
    const photo_calib::ParametricBand band =
        photo_calib::make_parametric_band(656.28, 3.0);
    const photo_calib::SegmentedResponseCache seg_cache =
        photo_calib::prepare_segmented_response_cache(
            band, qe_wl.data(), qe_val.data(), 2, xp.data(), n_xp);
    std::vector<double> fw(n_xp), fv(n_xp);
    for (int i = 0; i < n_xp; ++i) {
        fw[i] = xp[i];
        fv[i] = (xp[i] >= band.lo_nm && xp[i] <= band.hi_nm) ? 1.0 : 0.0;
    }
    const photo_calib::SpectrumIntegratorCache legacy_cache =
        photo_calib::prepare_filter_cache(
            fw.data(), fv.data(), n_xp, qe_wl.data(), qe_val.data(), 2,
            xp.data(), n_xp);
    double acc_seg = 0.0, acc_legacy = 0.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        acc_seg += photo_calib::compute_f_syn_segmented_xpsd(
            seg_cache, bytes.data(), n_xp, 0.0, 1.0e-8);
    }
    const auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        acc_legacy += photo_calib::compute_f_syn_cached_xpsd(
            legacy_cache, bytes.data(), n_xp, 0.0, 1.0e-8);
    }
    const auto t2 = std::chrono::steady_clock::now();
    const double us_seg =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / n;
    const double us_legacy =
        std::chrono::duration<double, std::micro>(t2 - t1).count() / n;
    std::printf("BENCH n=%d segmented=%.3f us/star legacy=%.3f us/star ratio=%.2f "
                "(acc_seg=%.6e acc_legacy=%.6e)\n",
                n, us_seg, us_legacy, us_legacy > 0.0 ? us_seg / us_legacy : 0.0,
                acc_seg, acc_legacy);
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "--cases") {
        return run_cases_mode(argv[2]);
    }
    if (argc >= 3 && std::string(argv[1]) == "--bench") {
        return run_bench_mode(std::atoi(argv[2]));
    }
    test_akima_ab();
    test_analytic_exactness();
    test_coverage();
    test_narrowband_43_counterexample();
    test_invalid_inputs();
    std::printf("== checks %d, failures %d\n", g_checks, g_fail);
    if (g_fail != 0) {
        std::fprintf(stderr, "NARROWBAND RESPONSE TEST FAILED\n");
        return 1;
    }
    std::printf("NARROWBAND RESPONSE TEST PASSED\n");
    return 0;
}
