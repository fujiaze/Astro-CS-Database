// ACSD Normalize — 空间 alpha（P5）语义验证: NOISE_SNR §11.3 的
// α_j²/a_ref 配对 + PHOT-MXY-01 的 α_j = k_photo·m(x,y)。
//
// 判据（expected 不由被测实现反推）:
//  1. 恒等退化: α_j ≡ k（常量）⇒ 与旧标量口径逐位一致（接线的兼容性）;
//  2. a_ref 配对: 合成线性梯度场 α(x,y)=k·(1+g·x̃), 生产 V_F 等于闭式
//     I_F = Σ_j P_j²/v_j, v_j = blank + F·α_j²·P_j/(g·a_ref), a_ref=Σα_jP_j
//     （独立第三条路径直接算该和式）;
//  3. 条件模型世界 MC: 噪声方差取 v_j 本身 ⇒ 估计量无偏、散度 == √V_F、
//     95% 覆盖（α_j² 加权确实进入条件方差）;
//  4. 物理方向: 若 α_j 与 P_j 正相关（亮核处标度更大）, 有效 gain 变化使
//     V_F 相对常量 α=mean 的变化方向可独立复算（闭式比）。
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "acsd/pixel_integrated_psf.h"
#include "acsd/sparse_reference.h"

using namespace acsd::normalize;
using acsd::psf::PixelIntegrationRequest;
using acsd::psf::pixel_integrated_moffat4_profile;

namespace {
int fails = 0;
void chk(bool ok, const char* w) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", w);
  if (!ok) ++fails;
}
double rel(double a, double b) {
  const double d = std::abs(a - b);
  return d / std::max(1e-300, std::max(std::abs(a), std::abs(b)));
}

constexpr double kSigma = 2.211089;
constexpr double kGain = 1.0;
constexpr double kRN = 9.0;
constexpr double kSky = 58.0;
constexpr double kFRef = 1000.0;

// 9x9 patch + 生产 PSF 库 P
struct Patch {
  std::vector<double> px, py;   // 像元中心
  std::vector<double> P;
};
Patch make_patch(double cx, double cy) {
  Patch p;
  for (int y = -4; y <= 4; ++y)
    for (int x = -4; x <= 4; ++x) {
      p.px.push_back(cx + x);
      p.py.push_back(cy + y);
    }
  PixelIntegrationRequest r;
  r.sx = r.sy = kSigma; r.theta = 0.0; r.cx = cx; r.cy = cy;
  r.pixel_x = p.px.data(); r.pixel_y = p.py.data();
  r.n_pixels = p.px.size(); r.order = 10;
  if (!pixel_integrated_moffat4_profile(r, &p.P, nullptr)) {
    std::printf("FATAL: PSF integration failed\n");
    std::exit(2);
  }
  return p;
}

PlanarPatchWindow make_window(const Patch& p,
                              const std::vector<double>& alpha,
                              BackgroundNuisance bg) {
  PlanarPatchWindow w;
  w.control_id = "alpha5";
  w.leaf_order = 11;
  w.omega_leaf = 4.0 * M_PI / (12.0 * 2048.0 * 2048.0);
  const int m = static_cast<int>(p.px.size());
  w.pix_index.resize(m);
  for (int j = 0; j < m; ++j)
    w.pix_index[static_cast<std::size_t>(j)] =
        static_cast<std::int64_t>(p.py[static_cast<std::size_t>(j)]) * 1000 +
        static_cast<std::int64_t>(p.px[static_cast<std::size_t>(j)]);
  w.profile = p.P;
  w.variance.assign(m, kSky / kGain + kRN * kRN / (kGain * kGain));
  w.src_x = p.px;
  w.src_y = p.py;
  w.pix_omega.assign(m, 1.0);
  w.pix_a_drop.assign(m, 1.0);
  w.pixel_alpha = alpha;                       // 空间 alpha（逐窗, 与像元对齐）
  for (int j = 0; j < m; ++j) {
    w.op_pix.push_back(j);
    w.op_leaf.push_back(j);
    w.op_c.push_back(1.0);                     // R = I
  }
  for (int l = 0; l < m; ++l) {
    w.leaf_ipix.push_back(900000ULL + l);
    w.leaf_dp.push_back(1.0);
    w.leaf_n_full.push_back(1.0);
    w.leaf_n_covered.push_back(1.0);
  }
  w.has_target_leaf = true;
  w.target_leaf = 900000ULL;
  (void)bg;
  return w;
}

ReferenceDefinition make_ref() {
  ReferenceDefinition r;
  r.kind = ReferenceKind::kExplicitLinearFlux;
  r.reference_id = "alpha5:unit";
  r.flux_unit = "adu";
  r.normalization = "photo_applied";
  r.flux = kFRef;
  return r;
}

double prod_vf(const PlanarPatchWindow& w, BackgroundNuisance bg) {
  SphericalCompletionConfig cfg;
  cfg.reference = make_ref();
  cfg.background = bg;
  cfg.gain_e_per_adu = kGain;
  cfg.master_uncertainty_modelled = true;
  cfg.template_fixed = true;
  const auto r = complete_spherical_reference({w}, cfg);
  if (r.precision.controls.empty() ||
      !r.precision.controls[0].flux_variance.has_value())
    return -1.0;
  return *r.precision.controls[0].flux_variance;
}
}  // namespace

int main() {
  const Patch p = make_patch(100.37, 200.61);
  const int m = static_cast<int>(p.px.size());

  // 1. 常量 alpha = k ⇒ 与标量 plane_scale 口径逐位一致（接线兼容性）。
  const double k0 = 2.5;
  {
    std::vector<double> a_const(m, k0);
    PlanarPatchWindow w = make_window(p, a_const, BackgroundNuisance::kKnown);
    const double v_alpha = prod_vf(w, BackgroundNuisance::kKnown);
    // 同一窗口, alpha 走 plane_scale 常量路径（pixel_alpha 空）。
    PlanarPatchWindow w2 = make_window(p, {}, BackgroundNuisance::kKnown);
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref();
    cfg.background = BackgroundNuisance::kKnown;
    cfg.gain_e_per_adu = kGain;
    cfg.plane_scale = k0;
    cfg.master_uncertainty_modelled = true;
    cfg.template_fixed = true;
    const auto r2 = complete_spherical_reference({w2}, cfg);
    const double v_scalar = *r2.precision.controls[0].flux_variance;
    std::printf("  const-alpha V_F=%.10g  scalar-plane_scale V_F=%.10g rel=%.3e\n",
                v_alpha, v_scalar, rel(v_alpha, v_scalar));
    chk(rel(v_alpha, v_scalar) < 1e-12,
        "constant pixel_alpha == scalar plane_scale (bit-compatible)");
  }

  // 2. 线性梯度场 α_j = k·(1+g·x̃)（x̃ = (x−cx)/4 ∈ [−1,1]）的闭式对拍。
  const double kg = 2.0, ggrad = 0.15;
  std::vector<double> a_lin(m);
  for (int j = 0; j < m; ++j) {
    const double xt = (p.px[static_cast<std::size_t>(j)] - 100.37) / 4.0;
    a_lin[static_cast<std::size_t>(j)] = kg * (1.0 + ggrad * xt);
  }
  double a_ref = 0.0;
  for (int j = 0; j < m; ++j)
    a_ref += a_lin[static_cast<std::size_t>(j)] * p.P[static_cast<std::size_t>(j)];
  double i_f_closed = 0.0;
  for (int j = 0; j < m; ++j) {
    const double blank = kSky / kGain + kRN * kRN / (kGain * kGain);
    const double aj = a_lin[static_cast<std::size_t>(j)];
    const double vj = blank + kFRef * aj * aj * p.P[static_cast<std::size_t>(j)] /
                                   (kGain * a_ref);
    i_f_closed += p.P[static_cast<std::size_t>(j)] * p.P[static_cast<std::size_t>(j)] / vj;
  }
  {
    PlanarPatchWindow w = make_window(p, a_lin, BackgroundNuisance::kKnown);
    const double v_f = prod_vf(w, BackgroundNuisance::kKnown);
    const double v_closed = 1.0 / i_f_closed;
    std::printf("  spatial-alpha V_F=%.10g  closed-form=%.10g  rel=%.3e "
                "(a_ref=%.9f)\n", v_f, v_closed, rel(v_f, v_closed), a_ref);
    chk(rel(v_f, v_closed) < 1e-12,
        "spatial-alpha V_F equals closed form with alpha_j^2/a_ref pairing");
    // 与常量 α=k 的差别方向: 梯度场使 a_ref 偏离 k（Jensen 效应）
    std::vector<double> a_k(m, kg);
    const double v_k = prod_vf(make_window(p, a_k, BackgroundNuisance::kKnown),
                               BackgroundNuisance::kKnown);
    std::printf("  gradient-field V_F=%.6g vs constant-k V_F=%.6g (ratio %.5f)\n",
                v_f, v_k, v_f / v_k);
    chk(std::abs(v_f / v_k - 1.0) > 1e-6,
        "spatial alpha measurably changes V_F (field is actually consumed)");
  }

  // 3. 条件模型世界 MC: 噪声方差 = GLS 使用的 v_j（α_j²/a_ref 已含）。
  {
    PlanarPatchWindow w = make_window(p, a_lin, BackgroundNuisance::kConstantFit);
    const double v_f = prod_vf(w, BackgroundNuisance::kConstantFit);
    std::vector<double> v(m);
    const double blank = kSky / kGain + kRN * kRN / (kGain * kGain);
    for (int j = 0; j < m; ++j)
      v[static_cast<std::size_t>(j)] =
          blank + kFRef * a_lin[static_cast<std::size_t>(j)] *
                      a_lin[static_cast<std::size_t>(j)] *
                      p.P[static_cast<std::size_t>(j)] / (kGain * a_ref);
    // GLS（常量 nuisance, R=I）: 估计行向量 precompute
    // A = [P, 1]; M = AᵀD⁻¹A; W = [1,0]M⁻¹AᵀD⁻¹
    const int ncol = 2;
    std::vector<double> mm(ncol * ncol, 0.0);
    for (int a = 0; a < ncol; ++a)
      for (int b = 0; b < ncol; ++b) {
        double s = 0.0;
        for (int j = 0; j < m; ++j) {
          const double ac = (a == 0) ? p.P[static_cast<std::size_t>(j)] : 1.0;
          const double bc = (b == 0) ? p.P[static_cast<std::size_t>(j)] : 1.0;
          s += ac * bc / v[static_cast<std::size_t>(j)];
        }
        mm[static_cast<std::size_t>(a) * ncol + b] = s;
      }
    const double det = mm[0] * mm[3] - mm[1] * mm[2];
    const double inv00 = mm[3] / det, inv01 = -mm[1] / det;
    std::vector<double> wrow(m, 0.0);
    for (int j = 0; j < m; ++j)
      wrow[static_cast<std::size_t>(j)] =
          (inv00 * p.P[static_cast<std::size_t>(j)] + inv01) /
          v[static_cast<std::size_t>(j)];
    const double v_explicit = inv00;
    std::printf("  MC setup: V_F=%.6g explicit[M^-1]_FF=%.6g rel=%.3e\n",
                v_f, v_explicit, rel(v_f, v_explicit));
    chk(rel(v_f, v_explicit) < 1e-12,
        "spatial-alpha + nuisance V_F equals explicit nl-space GLS");
    std::mt19937_64 rng(20261017);
    std::normal_distribution<double> nz(0.0, 1.0);
    const int nrep = 200000;
    double s1 = 0.0, s2 = 0.0;
    int cover = 0;
    const double hw = 1.959963984540054 * std::sqrt(v_f);
    std::vector<double> x(m);
    for (int rep = 0; rep < nrep; ++rep) {
      for (int j = 0; j < m; ++j)
        x[static_cast<std::size_t>(j)] =
            kFRef * p.P[static_cast<std::size_t>(j)] + kSky +
            std::sqrt(v[static_cast<std::size_t>(j)]) * nz(rng);
      double fh = 0.0;
      for (int j = 0; j < m; ++j)
        fh += wrow[static_cast<std::size_t>(j)] * x[static_cast<std::size_t>(j)];
      s1 += fh;
      s2 += fh * fh;
      if (std::abs(fh - kFRef) <= hw) ++cover;
    }
    const double mean = s1 / nrep;
    const double sd = std::sqrt(s2 / nrep - mean * mean);
    const double ratio = sd / std::sqrt(v_f);
    const double mc_se = sd / std::sqrt(2.0 * nrep);
    std::printf("  MC n=%d: bias=%+.4f (%.3f sigma) sd/sqrt(V_F)=%.5f (se %.4f%%) "
                "coverage=%.4f\n", nrep, mean - kFRef, (mean - kFRef) / std::sqrt(v_f),
                ratio, 100 * mc_se / sd,
                static_cast<double>(cover) / nrep);
    chk(std::abs(mean - kFRef) <
            4.0 * std::sqrt(v_f) / std::sqrt(static_cast<double>(nrep)),
        "spatial-alpha estimator unbiased within 4 MC se");
    chk(std::abs(ratio - 1.0) < 4.0 * mc_se,
        "spatial-alpha MC scatter == sqrt(V_F) within 4 sigma");
    const double cov_se = std::sqrt(0.95 * 0.05 / nrep);
    chk(std::abs(static_cast<double>(cover) / nrep - 0.95) < 4.0 * cov_se,
        "spatial-alpha 95% coverage matches nominal");
  }

  // 4. 负例: alpha 非法（非正/非有限/长度错）⇒ 具名拒绝, 不造 V_F。
  {
    std::vector<double> bad(m, 0.0);
    bad[3] = -1.0;
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref();
    cfg.background = BackgroundNuisance::kKnown;
    cfg.gain_e_per_adu = kGain;
    const auto r = complete_spherical_reference(
        {make_window(p, bad, BackgroundNuisance::kKnown)}, cfg);
    chk(!r.precision.controls.empty() &&
            r.precision.controls[0].status.availability ==
                PrecisionAvailability::kUnavailable &&
            !r.precision.controls[0].flux_variance.has_value(),
        "non-positive alpha entry ⇒ named unavailable, no fabricated V_F");
    std::vector<double> shortv(m - 2, 1.0);
    const auto r2 = complete_spherical_reference(
        {make_window(p, shortv, BackgroundNuisance::kKnown)}, cfg);
    chk(!r2.precision.controls.empty() &&
            r2.precision.controls[0].status.availability ==
                PrecisionAvailability::kUnavailable,
        "alpha length mismatch ⇒ named unavailable");
  }

  std::printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "ALL PASS", fails);
  return fails ? 1 : 0;
}
