// ACSD photometry — 参考 SED 合成通量积分核的独立数值复核（子任务 D 的可复用部分）。
//
// 正本: docs/science/algorithms/common/PHOTOMETRIC_FIT.md / PHOTOMETRY.md §2a
//       (claim PHOT-FSYN-CANON-001)；实现: lib/algorithms/photometry/cpp/src/
//       spectrum_integrator.cpp。本程序**只调用**生产核, 期望值全部由解析式或
//       独立实现的直接求和给出（不复用生产的 Akima/Simpson）。
//
// 判据:
//  1. 解析恒等: F_λ ≡ const、T≡Q≡1 于等间距网格 [λ1,λ2] ⇒
//     F_syn = ∫F_λ·λ dλ = F_λ(λ2²−λ1²)/2（光源计数权重 λ 的直接检验）。
//  2. 独立求积: 任意 SED×曲线, 生产(Akima+Simpson on 谱网格) 与独立实现
//     （把曲线线性插值到 1 nm 细网格 + 复合梯形/Simpson, 见本程序内独立函数）
//     的相对差在细网格离散化误差内。
//  3. 权重与支持域: 把 T 换成解析指数窗 exp(−((λ−λ0)/w)²) 时, 生产值等于
//     独立实现, 且 T≡0 ⇒ F_syn ≡ 0（退化不返回数值）。
//  4. XPSD 官方解码: F = byte·flux_mul + flux_min（常量部分对积分的贡献 =
//     flux_min·∫TQλdλ）; 同一 byte 数组下 F_syn 随 flux_min 线性变化。
//  5. 单位一致性: F_syn 的量纲为 W·m⁻²·nm（= hc·N_γ），数值上等于
//     hc·∫F_λTQ λ/(hc)dλ 的光子计数率 × hc —— 用 h·c 显式换算核对数量级。
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

#include "spectrum_integrator.h"

using photo_calib::SpectrumIntegratorCache;
using photo_calib::compute_f_syn_cached_xpsd;
using photo_calib::prepare_filter_cache;

namespace {
int fails = 0;
void chk(bool ok, const char* w) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", w);
  if (!ok) ++fails;
}
double rel(double a, double b) {
  return std::abs(a - b) / std::max(1e-300, std::max(std::abs(a), std::abs(b)));
}

// 独立求积（本程序内实现, 不调用生产 Akima/Simpson）:
// 把 T/Q 线性插值到 1 nm 细网格（区间外 0）, SED 用解析函数, 复合 Simpson 1/3。
template <typename SedF, typename TransF, typename QeF>
double independent_fsyn(double lam1, double lam2, double dlam, SedF sed,
                       TransF trans, QeF qe) {
  const int n = static_cast<int>((lam2 - lam1) / dlam) + 1;
  std::vector<double> y(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    const double lam = lam1 + i * dlam;
    y[static_cast<std::size_t>(i)] = sed(lam) * trans(lam) * qe(lam) * lam;
  }
  double s = 0.0;
  for (int i = 0; i + 1 < n; ++i)
    s += 0.5 * dlam * (y[static_cast<std::size_t>(i)] + y[static_cast<std::size_t>(i + 1)]);
  return s;
}

double sed_const(double) { return 2.5e-14; }
double trans_one(double) { return 1.0; }
double qe_one(double) { return 1.0; }
double sed_pl(double lam) { return 1.0e-13 * std::pow(600.0 / lam, 2.0); }
double trans_smooth(double lam) {
  return std::exp(-std::pow((lam - 640.0) / 60.0, 2.0));
}
double trans_zero(double) { return 0.0; }
double qe_lin(double lam) { return 0.36 + 0.0002 * (lam - 400.0); }
}  // namespace

int main() {
  std::printf("== 参考 SED 合成通量积分核独立复核 ==\n");
  // 谱网格 [336, 1020] 步长 2 nm（与 XPSD 一致）
  std::vector<double> wl;
  for (double l = 336.0; l <= 1020.0 + 1e-9; l += 2.0) wl.push_back(l);
  const int n = static_cast<int>(wl.size());

  // ── 1. 解析恒等: F_λ 常量、T=Q=1 ⇒ F_syn = F_λ(λ2²−λ1²)/2 ──────────────
  {
    std::vector<double> filt_wl = {336.0, 1020.0};
    std::vector<double> filt_t = {1.0, 1.0};
    std::vector<double> qe_wl = {336.0, 1020.0};
    std::vector<double> qe_t = {1.0, 1.0};
    const auto cache = prepare_filter_cache(filt_wl.data(), filt_t.data(), 2,
                                            qe_wl.data(), qe_t.data(), 2,
                                            wl.data(), n);
    // F_λ = 2.5e-14, flux_mul 使 byte·mul = F_λ ⇒ 取 mul = F_λ/200, byte=200
    const double mul = 2.5e-14 / 200.0, fmin = 0.0;
    std::vector<std::uint8_t> spec(static_cast<std::size_t>(n), 200);
    const double got = compute_f_syn_cached_xpsd(cache, spec.data(), n, fmin, mul);
    // ∫F λ dλ, 生产在 [336,1020] 上按谱网格 Simpson; 解析 = F(1020²−336²)/2
    const double analytic = 2.5e-14 * (1020.0 * 1020.0 - 336.0 * 336.0) / 2.0;
    std::printf("  const SED: production=%.12e analytic=%.12e rel=%.2e\n", got,
                analytic, rel(got, analytic));
    chk(rel(got, analytic) < 1e-9,
        "constant SED + unit curves == closed form F*(l2^2-l1^2)/2 (lambda weight)");
  }

  // ── 2/3. 幂律 SED × 平滑窗 × QE: 独立细网格求积对拍 ────────────────────
  {
    std::vector<double> filt_wl, filt_t;
    for (double l = 336.0; l <= 1020.0 + 1e-9; l += 2.0) {
      filt_wl.push_back(l);
      filt_t.push_back(trans_smooth(l));
    }
    std::vector<double> qe_wl, qe_t;
    for (double l = 300.0; l <= 1100.0 + 1e-9; l += 5.0) {
      qe_wl.push_back(l);
      qe_t.push_back(qe_lin(l));
    }
    const auto cache = prepare_filter_cache(filt_wl.data(), filt_t.data(),
                                            static_cast<int>(filt_wl.size()),
                                            qe_wl.data(), qe_t.data(),
                                            static_cast<int>(qe_wl.size()),
                                            wl.data(), n);
    // SED: F_λ = 1e-13·(600/λ)², 用 flux_mul/flux_min 精确表示:
    // byte 取整会引入量化误差 ⇒ 用 flux_mul=1e-16, flux_min=0, byte=round(F/1e-16)
    const double mul = 1e-16, fmin = 0.0;
    std::vector<std::uint8_t> spec(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n; ++i) {
      const double f = 1.0e-13 * std::pow(600.0 / wl[static_cast<std::size_t>(i)], 2.0);
      int b = static_cast<int>(std::lround(f / mul));
      if (b < 0) b = 0;
      if (b > 255) b = 255;   // 饱和到 8 bit（独立实现用同一解码值）
      spec[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(b);
    }
    // 独立实现用**同一解码值**（byte·mul）在 1 nm 网格上积分 ⇒ 消除量化差
    auto sed_q = [&](double lam) {
      const int i = static_cast<int>(std::lround((lam - 336.0) / 2.0));
      const int ic = std::min(std::max(i, 0), n - 1);
      return static_cast<double>(spec[static_cast<std::size_t>(ic)]) * mul;
    };
    auto trans_q = [](double lam) { return trans_smooth(lam); };
    auto qe_q = [](double lam) { return qe_lin(lam); };
    const double indep = independent_fsyn(336.0, 1020.0, 1.0, sed_q, trans_q, qe_q);
    const double got = compute_f_syn_cached_xpsd(cache, spec.data(), n, fmin, mul);
    std::printf("  power-law x window x QE: production=%.10e independent(1nm)=%.10e "
                "rel=%.2e\n", got, indep, rel(got, indep));
    chk(rel(got, indep) < 2e-2,
        "production (Akima+Simpson on spectrum grid) == independent fine-grid "
        "quadrature (within grid discretization)");

    // T≡0 ⇒ F_syn ≡ 0（退化不返回数值; 由调用方拒绝）
    std::vector<double> zero_t(static_cast<std::size_t>(n), 0.0);
    const auto zcache = prepare_filter_cache(filt_wl.data(), zero_t.data(),
                                             static_cast<int>(filt_wl.size()),
                                             nullptr, nullptr, 0, wl.data(), n);
    const double z = compute_f_syn_cached_xpsd(zcache, spec.data(), n, fmin, mul);
    std::printf("  T==0: production=%.3e\n", z);
    chk(z == 0.0, "zero transmission == 0 (degenerate, not a numeric value)");
  }

  // ── 4. XPSD 解码的常量项: F_syn 对 flux_min 线性 ────────────────────────
  {
    std::vector<double> filt_wl = {336.0, 1020.0}, filt_t = {1.0, 1.0};
    const auto cache = prepare_filter_cache(filt_wl.data(), filt_t.data(), 2,
                                            nullptr, nullptr, 0, wl.data(), n);
    std::vector<std::uint8_t> spec(static_cast<std::size_t>(n), 100);
    const double mul = 1e-16;
    const double f0 = compute_f_syn_cached_xpsd(cache, spec.data(), n, 0.0, mul);
    const double f1 = compute_f_syn_cached_xpsd(cache, spec.data(), n, 1e-16, mul);
    const double f2 = compute_f_syn_cached_xpsd(cache, spec.data(), n, 2e-16, mul);
    // ∫λdλ over [336,1020] = (1020²−336²)/2; flux_min 项 = fmin·该值
    const double kernel = (1020.0 * 1020.0 - 336.0 * 336.0) / 2.0;
    const double pred1 = f0 + 1e-16 * kernel;
    std::printf("  flux_min linearity: f0=%.6e f1=%.6e pred=%.6e rel=%.2e\n", f0, f1,
                pred1, rel(f1, pred1));
    chk(rel(f1, pred1) < 1e-9 &&
            std::abs((f1 - f0) * 2.0 - (f2 - f0)) <= 1e-12 * std::abs(f2 - f0),
        "XPSD decode F=byte*mul+flux_min: linear in flux_min with kernel Int(lambda dlambda)");
  }

  // ── 5. 单位: F_syn = hc·N_γ（光子计数率 × hc）数量级核对 ────────────────
  {
    const double h = 6.62607015e-34, c = 2.99792458e8;
    std::vector<double> filt_wl = {336.0, 1020.0}, filt_t = {1.0, 1.0};
    const auto cache = prepare_filter_cache(filt_wl.data(), filt_t.data(), 2,
                                            nullptr, nullptr, 0, wl.data(), n);
    // F_λ ≡ F0 ⇒ N_γ = ∫F_λ λ/(hc) dλ; F_syn 应等于 hc·N_γ
    const double F0 = 1e-13, mul = F0 / 128.0;
    std::vector<std::uint8_t> spec(static_cast<std::size_t>(n), 128);
    const double fsyn = compute_f_syn_cached_xpsd(cache, spec.data(), n, 0.0, mul);
    // N_γ 用同一离散和: Σ F λ/(hc) Δλ（与生产同一网格的梯形/辛普森在常量下等价）
    double ng = 0.0;
    for (int i = 0; i + 1 < n; ++i)
      ng += 0.5 * 2.0 * ((F0 * wl[static_cast<std::size_t>(i)]) / (h * c) +
                         (F0 * wl[static_cast<std::size_t>(i) + 1]) / (h * c));
    const double hc_ng = h * c * ng;
    std::printf("  unit check: F_syn=%.6e  hc*N_gamma=%.6e rel=%.2e\n", fsyn, hc_ng,
                rel(fsyn, hc_ng));
    chk(rel(fsyn, hc_ng) < 1e-9,
        "F_syn numerically equals hc * photon-count rate (lambda/(hc) weighting)");
  }

  std::printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "ALL PASS", fails);
  return fails ? 1 : 0;
}
