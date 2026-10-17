// Issue #12 —— 严格等距 Simpson 数值合同单元测试（独立解析 oracle）。
//
// 正本: docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md §Simpson 有效域（2026-10-17）。
// 实现: lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp。
//
// 判据:
//  A. 非等距/非法输入必须拒绝（quiet NaN），绝不返回 0 伪装"合法零积分"；
//     特别复现 x=[0,1e-12,3e-12],y=x（旧实现返回 3.5e-24，真值 4.5e-24，-22.22%）。
//  B. 合法严格等距网格上，n>=2 的 Simpson 组成对 0/1/2/3 次多项式在精确算术下到三次
//     精确；用解析原函数 (b^{k+1}-a^{k+1})/(k+1) 为 oracle（不复用旧积分核）。
//     n=1（N=2）梯形仅对 ≤1 次精确：对高次只断言等于梯形定义式，不冒充解析精确。
//  C. 多尺度/平移缩放/生成舍入：nm 轴在 336..1020（h=2）与 500 附近微小 h（h=1e-3）
//     及整体平移到 0 附近，合法生成网格必须被接受且数值正确。
//  D. cache/生产 API 边界的 0 vs NaN 口径：T≡0 ⇒ 合法真零；flux 量化参数非法或
//     spectrum_count 失配 ⇒ NaN；非法 nm 轴 ⇒ prepare_filter_cache 返回空缓存。
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "spectrum_integrator.h"

using photo_calib::SpectrumIntegratorCache;
using photo_calib::compute_f_syn_cached_xpsd;
using photo_calib::prepare_filter_cache;
using photo_calib::simpson_integrate;

namespace {

int g_checks = 0;
int g_fails = 0;

void chk(bool ok, const char* what) {
    ++g_checks;
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fails;
}

double rel_err(double got, double want) {
    const double d = std::fabs(got - want);
    const double s = std::max(1e-300, std::max(std::fabs(got), std::fabs(want)));
    return d / s;
}

// 解析原函数 ∫_a^b x^k dx = (b^{k+1} - a^{k+1})/(k+1)
double analytic_pow(double a, double b, int k) {
    return (std::pow(b, k + 1) - std::pow(a, k + 1)) / static_cast<double>(k + 1);
}

std::vector<double> make_grid(double x0, double h, int N) {
    std::vector<double> x(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) x[static_cast<size_t>(i)] = x0 + static_cast<double>(i) * h;
    return x;
}

std::vector<double> pow_y(const std::vector<double>& x, int k) {
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = std::pow(x[i], k);
    return y;
}

bool rejected(double v) { return std::isnan(v); }

// ── A. 反例与非法输入 ────────────────────────────────────────────────────────
void test_rejections() {
    std::printf("A. 非法/非等距输入必须拒绝 (quiet NaN)\n");
    // 冻结反例: 22.22% 误差必须被拒绝
    {
        std::vector<double> x{0.0, 1e-12, 3e-12};
        std::vector<double> y{0.0, 1e-12, 3e-12};
        const double got = simpson_integrate(x, y);
        const double truth = analytic_pow(0.0, 3e-12, 1); // 4.5e-24
        chk(rejected(got), "x=[0,1e-12,3e-12],y=x 被拒绝（旧实现 3.5e-24 vs 真值 4.5e-24）");
        std::printf("        (analytic=%.6e, old_bad_value=%.6e, rel_err_old=%.4f)\n",
                    truth, 3.5e-24, rel_err(3.5e-24, truth));
    }
    // 0.1% 局部拉伸（500nm 尺度）必须拒绝
    {
        std::vector<double> x{500.0, 501.0, 502.005};
        std::vector<double> y{1.0, 1.0, 1.0};
        chk(rejected(simpson_integrate(x, y)), "0.1% 拉伸 h0=1,h1=1.005 被拒绝");
    }
    // 巨大横坐标 + 微小步长：坐标分辨率前提不满足，必须拒绝
    {
        std::vector<double> x = make_grid(500.0, 1e-4, 343);
        std::vector<double> y = pow_y(x, 0);
        chk(rejected(simpson_integrate(x, y)),
            "x0=500nm,h=1e-4 非等距舍入不可辨（16*eps*M>1e-8*h）被拒绝");
    }
    // 尺寸/长度/非有限/重复/倒序
    chk(rejected(simpson_integrate({}, {})), "空输入被拒绝");
    chk(rejected(simpson_integrate({1.0}, {1.0})), "N=1 被拒绝");
    chk(rejected(simpson_integrate({0.0, 1.0, 2.0}, {0.0, 1.0})), "y 更短被拒绝");
    chk(rejected(simpson_integrate({0.0, 1.0, 2.0}, {0.0, 1.0, 2.0, 3.0})), "y 更长被拒绝");
    chk(rejected(simpson_integrate({0.0, std::numeric_limits<double>::quiet_NaN(), 2.0},
                                   {0.0, 1.0, 2.0})), "x 含 NaN 被拒绝");
    chk(rejected(simpson_integrate({0.0, 1.0, 2.0},
                                   {0.0, std::numeric_limits<double>::infinity(), 2.0})),
        "y 含 Inf 被拒绝");
    chk(rejected(simpson_integrate({1.0, 1.0, 2.0}, {0.0, 1.0, 2.0})), "重复 x 节点被拒绝");
    chk(rejected(simpson_integrate({2.0, 1.0, 0.0}, {2.0, 1.0, 0.0})), "倒序 x 被拒绝");
    // 累加溢出 ⇒ 非有限结果 ⇒ 拒绝
    chk(rejected(simpson_integrate({0.0, 1.0, 2.0}, {1e308, 1e308, 1e308})),
        "中间累加溢出被拒绝（非有限不是成功）");
}

// ── B. 多项式解析对照 ────────────────────────────────────────────────────────
void test_analytic(int N) {
    // 单位步长、简单起点；N=2 梯形，N=3 纯1/3，N=4 尾3/8，N=5 纯1/3，N=6 1/3+3/8
    const double x0 = 10.0, h = 1.0;
    std::vector<double> x = make_grid(x0, h, N);
    const double a = x.front(), b = x.back();
    for (int k = 0; k <= 3; ++k) {
        std::vector<double> y = pow_y(x, k);
        const double got = simpson_integrate(x, y);
        const double want = analytic_pow(a, b, k);
        char buf[160];
        if (N == 2 && k >= 2) {
            // 单区间梯形仅对 ≤1 次精确：断言等于梯形定义式（不冒充解析精确）
            const double trap = 0.5 * h * (y[0] + y[1]);
            std::snprintf(buf, sizeof buf, "N=2 k=%d 梯形定义式 (got-match=%d, analytic_rel_err=%.3e)",
                          k, (got == trap) ? 1 : 0, rel_err(got, want));
            chk(got == trap, buf);
        } else {
            const double re = rel_err(got, want);
            std::snprintf(buf, sizeof buf, "N=%d k=%d got=%.12g rel_err=%.3e", N, k, got, re);
            chk(std::isfinite(got) && re <= 1e-9, buf);
        }
    }
}

// ── C. 多尺度 / 平移缩放 / 生成舍入 ─────────────────────────────────────────
void test_scales() {
    std::printf("C. 多尺度/平移缩放/生成舍入\n");
    struct Case { double x0, h; int N; const char* what; };
    const Case cases[] = {
        {336.0, 2.0, 343, "Gaia-like 336..1020nm h=2 N=343 (n=342 纯1/3)"},
        {500.0, 0.001, 343, "x0=500nm h=1e-3 生成舍入"},
        {0.0, 0.001, 343, "x0=0 h=1e-3 (整体平移对照)"},
        {500.0, 0.5, 21, "x0=500nm h=0.5 N=21"},
    };
    for (const Case& c : cases) {
        std::vector<double> x = make_grid(c.x0, c.h, c.N);
        // 先确认网格本身通过合法域（以零被积函数探测：合法网格 + y≡0 ⇒ 精确 0）
        std::vector<double> zeros(x.size(), 0.0);
        const double z = simpson_integrate(x, zeros);
        chk(z == 0.0, (std::string("合法零函数返回真 0: ") + c.what).c_str());
        // 常数与一次式解析精确
        std::vector<double> ones(x.size(), 2.5);
        const double got1 = simpson_integrate(x, ones);
        const double want1 = 2.5 * (x.back() - x.front());
        chk(rel_err(got1, want1) <= 1e-12, (std::string("常数 k=0 解析精确: ") + c.what).c_str());
        std::vector<double> lin = pow_y(x, 1);
        const double gotl = simpson_integrate(x, lin);
        const double wantl = analytic_pow(x.front(), x.back(), 1);
        chk(rel_err(gotl, wantl) <= 1e-9, (std::string("一次式 k=1 解析精确: ") + c.what).c_str());
        // 带符号常数
        std::vector<double> neg(x.size(), -3.0);
        const double gotn = simpson_integrate(x, neg);
        chk(rel_err(gotn, -3.0 * (x.back() - x.front())) <= 1e-12,
            (std::string("带符号常数 k=0: ") + c.what).c_str());
    }
}

// ── D. cache/生产 API 边界 0 vs NaN ─────────────────────────────────────────
void test_cache_boundary() {
    std::printf("D. cache/API 边界 0 vs NaN 口径\n");
    const double filt_wl[3] = {500.0, 510.0, 520.0};
    // 合法网格
    std::vector<double> spec = make_grid(500.0, 1.0, 21);
    std::vector<uint8_t> sp(21, 10);

    // D1. T≡0 ⇒ 合法真零 (F_syn ≡ 0, 不是 NaN)
    {
        const double t0[3] = {0.0, 0.0, 0.0};
        SpectrumIntegratorCache c = prepare_filter_cache(filt_wl, t0, 3, nullptr, nullptr, 0,
                                                         spec.data(), (int)spec.size());
        chk(!c.spectrum_wl.empty(), "合法等距网格 ⇒ cache 有效");
        const double z = compute_f_syn_cached_xpsd(c, sp.data(), 21, 1e-16, 1e-16);
        chk(z == 0.0, "T≡0 ⇒ F_syn==0 合法零（保留，不报错）");
    }
    // D2. 非法 nm 轴 (非等距) ⇒ cache 空 (具名 INVALID_SPECTRAL_GRID)
    {
        std::vector<double> bad{500.0, 501.0, 502.005};
        SpectrumIntegratorCache c = prepare_filter_cache(filt_wl, filt_wl, 3, nullptr, nullptr, 0,
                                                         bad.data(), (int)bad.size());
        chk(c.spectrum_wl.empty(), "非等距 nm 轴 ⇒ prepare_filter_cache 返回空缓存");
    }
    {
        std::vector<double> bad{0.0, 1e-12, 3e-12}; // 冻结反例经 cache 入口
        SpectrumIntegratorCache c = prepare_filter_cache(filt_wl, filt_wl, 3, nullptr, nullptr, 0,
                                                         bad.data(), (int)bad.size());
        chk(c.spectrum_wl.empty(), "冻结反例 x=[0,1e-12,3e-12] 经 cache 入口被拒");
    }
    // D3. 合法 cache + 非法量化参数 ⇒ NaN (不是 0)
    {
        const double t1[3] = {1.0, 1.0, 1.0};
        SpectrumIntegratorCache c = prepare_filter_cache(filt_wl, t1, 3, nullptr, nullptr, 0,
                                                         spec.data(), (int)spec.size());
        chk(!c.spectrum_wl.empty(), "合法 cache (T≡1)");
        chk(rejected(compute_f_syn_cached_xpsd(c, sp.data(), 21, 1e-16, 0.0)),
            "flux_mul=0 ⇒ NaN（非 0）");
        chk(rejected(compute_f_syn_cached_xpsd(
                c, sp.data(), 21, std::numeric_limits<double>::quiet_NaN(), 1e-16)),
            "flux_min=NaN ⇒ NaN");
        chk(rejected(compute_f_syn_cached_xpsd(c, sp.data(), 20, 1e-16, 1e-16)),
            "spectrum_count 失配 ⇒ NaN");
        chk(rejected(compute_f_syn_cached_xpsd(c, nullptr, 21, 1e-16, 1e-16)),
            "空谱指针 ⇒ NaN");
        // 合法量化参数 ⇒ 有限正值
        const double good = compute_f_syn_cached_xpsd(c, sp.data(), 21, 1e-16, 1e-15);
        chk(std::isfinite(good) && good > 0.0, "合法输入 ⇒ 有限正 F_syn");
    }
}

} // namespace

int main() {
    std::printf("== Issue #12 strict-equidistant Simpson domain test ==\n");
    test_rejections();
    std::printf("B. 多项式解析对照 (N=2/3/4/5/6/343)\n");
    for (int N : {2, 3, 4, 5, 6, 343}) test_analytic(N);
    test_scales();
    test_cache_boundary();
    std::printf("== checks=%d fails=%d ==\n", g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
