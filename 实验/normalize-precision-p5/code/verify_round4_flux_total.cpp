// ACSD Normalize — 参考通量总通量口径（NOISE_SNR §2 总通量语义订正）独立验证。
//
// 背景（ROUND3_FIX_REPORT §5.2 登记的可测偏差, 本程序给订正后的判据）:
//   旧实现把 a_ref = Σ_window α_j·P_j 同时用作源泊松归一（v_src =
//   (α_j²/a_ref)·F_ref·P_j/g）却让模板保持 h = P, 造成 analytic_total 与窗口
//   通量混称; 空间 α 下模板根本不承载 α 结构 ⇒ 估计量对物理世界有偏。
//
// 订正后的自洽模型（2026-10-16; 公式见 sparse_reference.h 文件头）:
//   F_native: 原生总通量; Σ_all P = 1（analytic_total, 不随窗口重归一）
//   E[y_j]    = α_j·F_native·P_j = F_ref·h_j,  h_j = (α_j/α_*)·P_j
//   F_ref(u)  = α_*·F_native（α_* = 参考源位置 u 处声明标度）
//   Var_src,j = α_j²·F_native·P_j/g = F_ref·α_j²·P_j/(g·α_*)
//   有限窗 Σ_window P < 1 是真实截断, 由 f_enclosed/f_out 逐点登记,
//   **不用窗 sum 重新定义总通量**。
//
// 判据（expected 全部由独立闭式/独立模拟给出, 不由被测实现反推; 不放宽容差）:
//  1. 恒等退化: α_j ≡ k、α_* = k、无 per-point flux ⇒ V_F 等于闭式
//     I_F = Σ P_j²/v_j, v_j = blank + k·F_ref·P_j/g; 且与标量 plane_scale
//     常量路径逐位一致;
//  2. 窗口截断阶梯: 5×5/9×9/13×13/21×21, α≡1: 新 V_F 逐窗等于无 1/ΣP 闭式;
//     旧（1/ΣP）口径与新口径的比随 ΣP 偏离 1 增长（数学解释: 泊松份额 × 1/ΣP）;
//  3. 空间梯度+曲率场 α_j = k·(1+g1x̃+g2x̃²+h1ỹ), α_* = k·m(u):
//     (a) 已知背景 V_F == 闭式（h 承载 α）;
//     (b) 常量 nuisance V_F/SNR == 显式 2×2 GLS;
//     (c) per-point reference_flux = α_*·F_native 被正确消费;
//  4. 算子秩亏三态: R=I; 像元 0.5 配对到叶（满行秩稀疏）; 重复行（cov 秩 <
//     叶数）。三态用**行空间投影闭式**独立重算 I_F 与生产 QR 对拍;
//  5. 载荷一致性: lw.h_sphere == R·planar_template（逐项）,
//     planar_template == (α_j/α_*)·P_j（逐项）, α_j/α_* 与 photometry m(x,y)
//     场一致（独立多项式求值）;
//  6. 物理世界 MC（Poisson + 读噪, 真值 F_native, n=200000）:
//     (a) α≡1: 新条件模型 sd == √V_F、95% 覆盖（旧口径给出可闭式预测的
//         1/ΣP·泊松份额偏差, 定量对照）;
//     (b) 空间梯度场: 新估计量无偏、散度 == √V_F; 旧模板（h=P）估计量的解析
//         偏差 E[F̂_old] ≠ F_native 被显式复算（旧模型在空间 α 下有偏的证明）;
//  7. 负例: 空间 α 场存在但 reference_alpha_star 缺失/非正 ⇒ 具名 unavailable。
#include <cmath>
#include <cstdio>
#include <cstdint>
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

constexpr double kSigma = 2.211089;   // 生产 M42 帧 PSF σ（ROUND3 §6.1 实核）
constexpr double kGain = 1.0;         // e-/ADU（run/norm-precision-test.json）
constexpr double kRN = 9.0;           // e-
constexpr double kSky = 58.0;         // ADU/px
constexpr double kFRef = 1000.0;      // ADU（配置 explicit_linear_flux）

double blank_var() { return kSky / kGain + kRN * kRN / (kGain * kGain); }

// ── 窗口构造（n×n 方形 patch + 生产 PSF 库单位总通量模板）──────────────────
struct Patch {
  std::vector<double> px, py, P;
  double cx = 0.0, cy = 0.0;
};
Patch make_patch(double cx, double cy, int n) {
  Patch p;
  p.cx = cx; p.cy = cy;
  const int h = (n - 1) / 2;
  for (int y = -h; y <= h; ++y)
    for (int x = -h; x <= h; ++x) {
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
double sum_P(const Patch& p) {
  double s = 0.0;
  for (double x : p.P) s += x;
  return s;
}

enum class OpMode { kIdentity, kPairedHalf, kDuplicateRow };

PlanarPatchWindow make_window(const Patch& p, const std::vector<double>& alpha,
                              double alpha_star, double reference_flux,
                              BackgroundNuisance bg, OpMode op,
                              bool applied_blank = false) {
  PlanarPatchWindow w;
  w.control_id = "flux-total";
  w.leaf_order = 11;
  w.omega_leaf = 4.0 * M_PI / (12.0 * 2048.0 * 2048.0);
  const int m = static_cast<int>(p.px.size());
  w.pix_index.resize(m);
  for (int j = 0; j < m; ++j)
    w.pix_index[static_cast<std::size_t>(j)] =
        static_cast<std::int64_t>(p.py[static_cast<std::size_t>(j)]) * 100000 +
        static_cast<std::int64_t>(p.px[static_cast<std::size_t>(j)]);
  w.profile = p.P;
  // 生产 var_plane 与输入面同标度: frame_applied 时是 α²·原生 blank。
  // applied_blank=false 用于纯代数对拍（与闭式同口径）, true 复现生产约定。
  w.variance.assign(m, blank_var());
  if (applied_blank && !alpha.empty())
    for (int j = 0; j < m; ++j)
      w.variance[static_cast<std::size_t>(j)] =
          alpha[static_cast<std::size_t>(j)] * alpha[static_cast<std::size_t>(j)] *
          blank_var();
  w.src_x = p.px;
  w.src_y = p.py;
  w.pix_omega.assign(m, 1.0);
  w.pix_a_drop.assign(m, 1.0);
  if (!alpha.empty()) w.pixel_alpha = alpha;
  w.reference_alpha_star = alpha_star;
  w.reference_flux = reference_flux;
  int nl = 0;
  if (op == OpMode::kIdentity) {
    nl = m;
    for (int j = 0; j < m; ++j) {
      w.op_pix.push_back(j); w.op_leaf.push_back(j); w.op_c.push_back(1.0);
    }
  } else if (op == OpMode::kPairedHalf) {
    nl = (m + 1) / 2;
    for (int l = 0; l < nl; ++l)
      for (int k = 0; k < 2; ++k) {
        const int j = 2 * l + k;
        if (j >= m) break;
        w.op_pix.push_back(j); w.op_leaf.push_back(l); w.op_c.push_back(0.5);
      }
  } else {
    const int half = m / 2;
    nl = half + 1;
    for (int l = 0; l < half; ++l) {
      w.op_pix.push_back(l); w.op_leaf.push_back(l); w.op_c.push_back(1.0);
    }
    w.op_pix.push_back(half - 1); w.op_leaf.push_back(half); w.op_c.push_back(1.0);
  }
  w.leaf_ipix.resize(nl);
  w.leaf_dp.assign(nl, 1.0);
  w.leaf_n_full.assign(nl, 1.0);
  w.leaf_n_covered.assign(nl, 1.0);
  for (int l = 0; l < nl; ++l) w.leaf_ipix[static_cast<std::size_t>(l)] =
      900000ULL + static_cast<std::uint64_t>(l);
  w.has_target_leaf = true;
  w.target_leaf = 900000ULL;
  (void)bg;
  return w;
}

ReferenceDefinition make_ref() {
  ReferenceDefinition r;
  r.kind = ReferenceKind::kExplicitLinearFlux;
  r.reference_id = "flux-total:unit";
  r.flux_unit = "adu";
  r.normalization = "photo_applied";
  r.flux = kFRef;
  return r;
}

struct ProdOut {
  bool ok = false;
  double v_f = -1.0, snr = -1.0, f_ref_used = -1.0;
  std::string reason;
  std::vector<double> h_sphere, planar, alpha;
  double alpha_star = 0.0, window_scale = 0.0;
  double i_f = 0.0, t_norm = 0.0, e_norm = 0.0;
  std::uint64_t cov_rank = 0;
};

ProdOut prod_vf(const PlanarPatchWindow& w, BackgroundNuisance bg,
                double plane_scale = 1.0) {
  ProdOut o;
  SphericalCompletionConfig cfg;
  cfg.reference = make_ref();
  cfg.background = bg;
  cfg.gain_e_per_adu = kGain;
  cfg.master_uncertainty_modelled = true;
  cfg.template_fixed = true;
  cfg.plane_scale = plane_scale;
  const auto r = complete_spherical_reference({w}, cfg);
  if (r.precision.controls.empty()) { o.reason = "no controls"; return o; }
  const auto& c = r.precision.controls[0];
  if (c.status.availability != PrecisionAvailability::kAvailable) {
    o.reason = c.status.reason;
    return o;
  }
  o.ok = true;
  o.v_f = *c.flux_variance;
  o.snr = *c.reference_snr;
  o.f_ref_used = *c.reference_flux;
  o.cov_rank = c.covariance_rank.value_or(0);
  if (!r.windows.empty()) {
    const auto& lw = r.windows[0];
    o.h_sphere = lw.h_sphere;
    o.planar = lw.planar_template;
    o.alpha = lw.source_pixel_alpha;
    o.alpha_star = lw.reference_alpha_star;
    o.window_scale = lw.reference_window_scale;
    o.i_f = lw.flux_information;
    o.t_norm = lw.flux_response_norm;
    o.e_norm = lw.flux_residual_norm;
  }
  return o;
}

// 通用窗口量: h_j、v_j（新口径; alpha 空 ⇒ α_* = alpha_star 常量）。
// applied_blank=true 时 blank 用 α_j²·原生（生产 var_plane 约定）。
void h_and_v(const Patch& p, const std::vector<double>& alpha, double alpha_star,
             double f_ref, std::vector<double>& h, std::vector<double>& v,
             bool applied_blank = false) {
  const int m = static_cast<int>(p.px.size());
  h.assign(m, 0.0); v.assign(m, 0.0);
  for (int j = 0; j < m; ++j) {
    const double P = p.P[static_cast<std::size_t>(j)];
    const double aj = alpha.empty() ? alpha_star : alpha[static_cast<std::size_t>(j)];
    h[static_cast<std::size_t>(j)] = (aj / alpha_star) * P;
    const double blank = applied_blank ? aj * aj * blank_var() : blank_var();
    v[static_cast<std::size_t>(j)] =
        blank + f_ref * aj * aj * P / (kGain * alpha_star);
  }
}

// ── 独立闭式: 已知背景, 任意算子（行空间投影闭式, 与生产 QR 不同代码路径）──
// NOISE_SNR「数值误差怎样传到参考信息」: J 是 Q = R·D^{1/2} 的**行空间**投影,
// 即 span{D^{1/2}·row_l(R)}（D^{1/2} 非标量时 ≠ rowspan(R)）;
// I_F = ‖J·D^{-1/2}·h‖²。实现: 把算子行按 √v_j 加权后正交化再投影。
double closed_if_known_bg(const Patch& p, const std::vector<double>& alpha,
                          double alpha_star, double f_ref, OpMode op) {
  const int m = static_cast<int>(p.px.size());
  std::vector<double> h, v;
  h_and_v(p, alpha, alpha_star, f_ref, h, v);
  // 算子行（sparse triplets, 与 make_window 同一构造 —— 但投影/正交化是
  // 独立代码, 不由被测实现反推 expected）。
  std::vector<std::vector<std::pair<int, double>>> rows;
  if (op == OpMode::kIdentity) {
    rows.assign(m, {});
    for (int j = 0; j < m; ++j) rows[static_cast<std::size_t>(j)].push_back({j, 1.0});
  } else if (op == OpMode::kPairedHalf) {
    rows.assign((m + 1) / 2, {});
    for (int l = 0; l < static_cast<int>(rows.size()); ++l)
      for (int k = 0; k < 2; ++k) {
        const int j = 2 * l + k;
        if (j >= m) break;
        rows[static_cast<std::size_t>(l)].push_back({j, 0.5});
      }
  } else {
    const int half = m / 2;
    rows.assign(half + 1, {});
    for (int l = 0; l < half; ++l)
      rows[static_cast<std::size_t>(l)].push_back({l, 1.0});
    rows[static_cast<std::size_t>(half)].push_back({half - 1, 1.0});
  }
  // D^{1/2} 加权后的算子行正交基（两轮修正 Gram-Schmidt; 相关行按 rel 1e-12 判弃）。
  std::vector<std::vector<double>> basis;
  for (const auto& rw : rows) {
    std::vector<double> z(m, 0.0);
    for (const auto& e : rw)
      z[static_cast<std::size_t>(e.first)] +=
          e.second * std::sqrt(v[static_cast<std::size_t>(e.first)]);
    double n0 = 0.0;
    for (double x : z) n0 += x * x;
    n0 = std::sqrt(n0);
    if (n0 <= 0.0) continue;
    for (int pass = 0; pass < 2; ++pass)
      for (auto& b : basis) {
        double d = 0.0;
        for (int j = 0; j < m; ++j)
          d += z[static_cast<std::size_t>(j)] * b[static_cast<std::size_t>(j)];
        for (int j = 0; j < m; ++j)
          z[static_cast<std::size_t>(j)] -= d * b[static_cast<std::size_t>(j)];
      }
    double rn = 0.0;
    for (double x : z) rn += x * x;
    rn = std::sqrt(rn);
    if (rn <= 1e-12 * n0) continue;   // 相关行 ⇒ 非新方向（精确重复行被丢弃）
    for (int j = 0; j < m; ++j) z[static_cast<std::size_t>(j)] /= rn;
    basis.push_back(z);
  }
  const int r = static_cast<int>(basis.size());
  std::vector<double> t(r, 0.0);
  for (int q = 0; q < r; ++q) {
    double s = 0.0;
    for (int j = 0; j < m; ++j)
      s += basis[static_cast<std::size_t>(q)][static_cast<std::size_t>(j)] *
           h[static_cast<std::size_t>(j)] /
           std::sqrt(v[static_cast<std::size_t>(j)]);
    t[static_cast<std::size_t>(q)] = s;
  }
  double i_f = 0.0;
  for (double x : t) i_f += x * x;
  return i_f;
}

// 旧口径闭式（模板 h=P, 泊松 1/ΣP 因子; 用于旧/新差异的数学解释与一致性判定）。
// applied_blank=true 时 blank 用 α_j²·原生（生产 var_plane 约定）。
double old_if_known_bg(const Patch& p, const std::vector<double>& alpha,
                       double alpha_star, double f_ref,
                       bool applied_blank = false) {
  const int m = static_cast<int>(p.px.size());
  double a_ref = 0.0;
  for (int j = 0; j < m; ++j) {
    const double P = p.P[static_cast<std::size_t>(j)];
    const double aj = alpha.empty() ? alpha_star : alpha[static_cast<std::size_t>(j)];
    a_ref += aj * P;
  }
  double i_f = 0.0;
  for (int j = 0; j < m; ++j) {
    const double P = p.P[static_cast<std::size_t>(j)];
    const double aj = alpha.empty() ? alpha_star : alpha[static_cast<std::size_t>(j)];
    const double blank = applied_blank ? aj * aj * blank_var() : blank_var();
    const double vj = blank + (aj * aj / a_ref) * f_ref * P / kGain;
    i_f += P * P / vj;   // 旧模板列 = P（不承载 α）
  }
  return i_f;
}

}  // namespace

int main() {
  std::printf("== 参考通量总通量口径（NOISE_SNR §2）独立验证 ==\n");

  // ── 1. 常量 α=k 恒等退化 + 标量路径逐位一致 ──────────────────────────────
  {
    const Patch p = make_patch(100.37, 200.61, 9);
    const int m = static_cast<int>(p.px.size());
    const double k0 = 2.5;
    std::vector<double> a_const(m, k0);
    const auto o = prod_vf(make_window(p, a_const, k0, 0.0,
                                       BackgroundNuisance::kKnown, OpMode::kIdentity),
                           BackgroundNuisance::kKnown);
    double i_new = 0.0;
    for (int j = 0; j < m; ++j) {
      const double P = p.P[static_cast<std::size_t>(j)];
      i_new += P * P / (blank_var() + k0 * kFRef * P / kGain);
    }
    std::printf("  const-alpha V_F=%.10g closed(new)=%.10g rel=%.3e\n",
                o.v_f, 1.0 / i_new, rel(o.v_f, 1.0 / i_new));
    chk(o.ok && rel(o.v_f, 1.0 / i_new) < 1e-12,
        "constant alpha: V_F == closed form without window-sum factor");
    const auto o2 = prod_vf(make_window(p, {}, k0, 0.0,
                                        BackgroundNuisance::kKnown, OpMode::kIdentity),
                            BackgroundNuisance::kKnown, k0);
    std::printf("  pixel_alpha const vs scalar plane_scale: rel=%.3e\n",
                rel(o.v_f, o2.v_f));
    chk(o.ok && o2.ok && rel(o.v_f, o2.v_f) == 0.0,
        "constant pixel_alpha == scalar plane_scale path (bit-identical)");
  }

  // ── 2. 窗口截断阶梯（α≡1）: 新 V_F 无 1/ΣP; 旧/新比 = 泊松份额 × 1/ΣP ────
  {
    std::printf("  window ladder (alpha==1):\n");
    for (int n : {5, 9, 13, 21}) {
      const Patch p = make_patch(100.37, 200.61, n);
      const int m = static_cast<int>(p.px.size());
      std::vector<double> one(m, 1.0);
      const auto o = prod_vf(make_window(p, one, 1.0, 0.0,
                                         BackgroundNuisance::kKnown, OpMode::kIdentity),
                             BackgroundNuisance::kKnown);
      double i_new = 0.0, i_old = 0.0;
      const double sp = sum_P(p);
      for (int j = 0; j < m; ++j) {
        const double P = p.P[static_cast<std::size_t>(j)];
        i_new += P * P / (blank_var() + kFRef * P / kGain);
        i_old += P * P / (blank_var() + kFRef * P / (kGain * sp));
      }
      const double v_new = 1.0 / i_new, v_old = 1.0 / i_old;
      std::printf("    %2dx%-2d SumP=%.6f  V_F(new)=%.8g closed=%.8g rel=%.2e | "
                  "V_F(old)/V_F(new)=%.6f (SumP=%.6f)\n",
                  n, n, sp, o.v_f, v_new, rel(o.v_f, v_new), v_old / v_new, sp);
      chk(o.ok && rel(o.v_f, v_new) < 1e-12,
          "truncated window: V_F == total-flux closed form (no 1/SigmaP)");
      // 纯泊松极限下旧/新比恰为 ΣP; 含 blank 时该比被稀释 ⇒ 一定有
      // 1 ≤ old/new ≤ 1/ΣP 且单调趋近。这里核对区间（不使用容许带凑绿）。
      chk(v_old / v_new >= 1.0 - 1e-12 && v_old / v_new <= 1.0 / sp + 1e-9,
          "old/new ratio within [1, 1/SigmaP] (Poisson share diluted by blank)");
    }
  }

  // ── 3. 空间梯度+曲率场 ───────────────────────────────────────────────────
  {
    const Patch p = make_patch(150.25, 80.75, 9);
    const int m = static_cast<int>(p.px.size());
    const double k = 2.0, g1 = 0.15, g2 = 0.08, h1 = -0.06;
    auto mfun = [&](double x, double y) {
      const double xt = (x - p.cx) / 4.0, yt = (y - p.cy) / 4.0;
      return 1.0 + g1 * xt + g2 * xt * xt + h1 * yt;
    };
    std::vector<double> alpha(m);
    for (int j = 0; j < m; ++j)
      alpha[static_cast<std::size_t>(j)] =
          k * mfun(p.px[static_cast<std::size_t>(j)], p.py[static_cast<std::size_t>(j)]);
    const double alpha_star = k * mfun(p.cx, p.cy);

    // (a) 已知背景
    const auto oa = prod_vf(make_window(p, alpha, alpha_star, 0.0,
                                        BackgroundNuisance::kKnown, OpMode::kIdentity),
                            BackgroundNuisance::kKnown);
    double i_new = 0.0;
    for (int j = 0; j < m; ++j) {
      const double P = p.P[static_cast<std::size_t>(j)];
      const double aj = alpha[static_cast<std::size_t>(j)];
      const double hj = (aj / alpha_star) * P;
      i_new += hj * hj / (blank_var() + kFRef * aj * aj * P / (kGain * alpha_star));
    }
    std::printf("  spatial gradient+curvature: V_F=%.10g closed=%.10g rel=%.3e "
                "(alpha_*=%.9f)\n", oa.v_f, 1.0 / i_new, rel(oa.v_f, 1.0 / i_new),
                alpha_star);
    chk(oa.ok && rel(oa.v_f, 1.0 / i_new) < 1e-12,
        "spatial gradient+curvature: V_F == closed form (h carries alpha)");

    // (b) 常量 nuisance: 显式 2×2 GLS, V_F = [(M⁻¹)]_FF = M11/det
    const auto ob = prod_vf(make_window(p, alpha, alpha_star, 0.0,
                                        BackgroundNuisance::kConstantFit, OpMode::kIdentity),
                            BackgroundNuisance::kConstantFit);
    double m00 = 0.0, m01 = 0.0, m11 = 0.0;
    for (int j = 0; j < m; ++j) {
      const double P = p.P[static_cast<std::size_t>(j)];
      const double aj = alpha[static_cast<std::size_t>(j)];
      const double hj = (aj / alpha_star) * P;
      const double vj = blank_var() + kFRef * aj * aj * P / (kGain * alpha_star);
      m00 += hj * hj / vj; m01 += hj / vj; m11 += 1.0 / vj;
    }
    const double det = m00 * m11 - m01 * m01;
    const double v_closed = m11 / det;
    std::printf("  +constant-fit nuisance: V_F=%.10g closed=%.10g rel=%.3e\n",
                ob.v_f, v_closed, rel(ob.v_f, v_closed));
    chk(ob.ok && rel(ob.v_f, v_closed) < 1e-12 &&
            rel(ob.snr, kFRef / std::sqrt(v_closed)) < 1e-12,
        "spatial + constant-fit: V_F/SNR == explicit 2x2 GLS");

    // (c) per-point reference_flux = α_*·F_native
    const double f_native = kFRef / k;
    const double f_ref_pt = alpha_star * f_native;
    const auto oc = prod_vf(make_window(p, alpha, alpha_star, f_ref_pt,
                                        BackgroundNuisance::kKnown, OpMode::kIdentity),
                            BackgroundNuisance::kKnown);
    double i_pt = 0.0;
    for (int j = 0; j < m; ++j) {
      const double P = p.P[static_cast<std::size_t>(j)];
      const double aj = alpha[static_cast<std::size_t>(j)];
      const double hj = (aj / alpha_star) * P;
      i_pt += hj * hj / (blank_var() + f_ref_pt * aj * aj * P / (kGain * alpha_star));
    }
    std::printf("  per-point F_ref=%.6g: V_F=%.10g closed=%.10g rel=%.3e SNR=%.9g\n",
                f_ref_pt, oc.v_f, 1.0 / i_pt, rel(oc.v_f, 1.0 / i_pt), oc.snr);
    chk(oc.ok && oc.f_ref_used == f_ref_pt && rel(oc.v_f, 1.0 / i_pt) < 1e-12 &&
            rel(oc.snr, f_ref_pt * std::sqrt(i_pt)) < 1e-12,
        "per-point F_ref(u)=alpha_*·F_native consumed in V_F/SNR");
  }

  // ── 4. 算子秩亏三态: 行空间投影闭式对拍 ──────────────────────────────────
  {
    const Patch p = make_patch(120.5, 300.5, 9);
    const int m = static_cast<int>(p.px.size());
    std::vector<double> one(m, 1.0);
    struct Case { const char* name; OpMode op; };
    const Case cases[] = {{"R=I", OpMode::kIdentity},
                          {"paired-half rows (full row rank)", OpMode::kPairedHalf},
                          {"duplicate row (cov rank < n_leaf)", OpMode::kDuplicateRow}};
    for (const auto& c : cases) {
      const auto o = prod_vf(make_window(p, one, 1.0, 0.0,
                                         BackgroundNuisance::kKnown, c.op),
                             BackgroundNuisance::kKnown);
      const double i_closed = closed_if_known_bg(p, one, 1.0, kFRef, c.op);
      std::printf("  %-38s rank=%llu V_F=%.10g closed=%.10g rel=%.3e\n", c.name,
                  static_cast<unsigned long long>(o.cov_rank), o.v_f,
                  1.0 / i_closed, rel(o.v_f, 1.0 / i_closed));
      chk(o.ok && rel(o.v_f, 1.0 / i_closed) < 1e-10,
          "rank-deficient operator: V_F == independent row-space projection");
    }
  }

  // ── 5. 载荷一致性: h_sphere == R·planar_template; planar == (α/α_*)P ──────
  {
    const Patch p = make_patch(150.25, 80.75, 9);
    const int m = static_cast<int>(p.px.size());
    const double k = 2.0;
    std::vector<double> alpha(m);
    for (int j = 0; j < m; ++j) {
      const double xt = (p.px[static_cast<std::size_t>(j)] - p.cx) / 4.0;
      alpha[static_cast<std::size_t>(j)] = k * (1.0 + 0.15 * xt);
    }
    const double alpha_star = k;   // 控制点处 m(u)=1
    const auto o = prod_vf(make_window(p, alpha, alpha_star, 0.0,
                                       BackgroundNuisance::kKnown, OpMode::kIdentity),
                           BackgroundNuisance::kKnown);
    bool planar_ok = o.planar.size() == p.P.size();
    bool hsphere_ok = o.h_sphere.size() == p.P.size();
    for (int j = 0; j < m && planar_ok; ++j)
      planar_ok = rel(o.planar[static_cast<std::size_t>(j)],
                      (alpha[static_cast<std::size_t>(j)] / alpha_star) *
                          p.P[static_cast<std::size_t>(j)]) < 1e-15;
    for (int j = 0; j < m && hsphere_ok; ++j)   // R=I ⇒ h_sphere == planar
      hsphere_ok = o.h_sphere[static_cast<std::size_t>(j)] ==
                   o.planar[static_cast<std::size_t>(j)];
    std::printf("  planar_template==(alpha/alpha_*)P: %s; h_sphere==R·planar: %s; "
                "alpha_star=%.12g window_scale=%.12g\n",
                planar_ok ? "yes" : "NO", hsphere_ok ? "yes" : "NO",
                o.alpha_star, o.window_scale);
    chk(planar_ok && hsphere_ok && o.alpha_star == alpha_star,
        "payload: planar_template == (alpha/alpha_*)·P and h_sphere == R·planar");
    // 窗口诊断 a_ref = Σα_jP_j 与独立和一致（只登记, 不归一）
    double a_ref = 0.0;
    for (int j = 0; j < m; ++j)
      a_ref += alpha[static_cast<std::size_t>(j)] * p.P[static_cast<std::size_t>(j)];
    chk(rel(o.window_scale, a_ref) < 1e-14,
        "diagnostic window scale == sum(alpha_j·P_j) (registered, not normalizing)");
  }

  // ── 6a. 物理世界 MC: α≡1, 真值 F_native = F_ref ─────────────────────────
  {
    const Patch p = make_patch(100.37, 200.61, 9);
    const int m = static_cast<int>(p.px.size());
    const auto o = prod_vf(make_window(p, std::vector<double>(m, 1.0), 1.0, 0.0,
                                       BackgroundNuisance::kKnown, OpMode::kIdentity),
                           BackgroundNuisance::kKnown);
    // 已知背景 GLS 权重（独立构造）: w_j = h_j/(v_j·I_F)
    std::vector<double> h, v;
    h_and_v(p, std::vector<double>(), 1.0, kFRef, h, v);
    double i_f = 0.0;
    for (int j = 0; j < m; ++j) i_f += h[static_cast<std::size_t>(j)] * h[static_cast<std::size_t>(j)] /
                                           v[static_cast<std::size_t>(j)];
    std::vector<double> wrow(m);
    for (int j = 0; j < m; ++j)
      wrow[static_cast<std::size_t>(j)] =
          h[static_cast<std::size_t>(j)] / v[static_cast<std::size_t>(j)] / i_f;
    // 真值世界: 电子 = Poisson(g·(S_j + sky)) + RN; y = e/g; S_j = F_native·P_j
    std::mt19937_64 rng(20261016);
    std::normal_distribution<double> nz(0.0, 1.0);
    std::poisson_distribution<int> pois;
    const int nrep = 200000;
    const double f_true = kFRef;   // α_*=1 ⇒ F_ref = F_native
    double s1 = 0.0, s2 = 0.0;
    int cover = 0;
    const double hw = 1.959963984540054 * std::sqrt(o.v_f);
    for (int rep = 0; rep < nrep; ++rep) {
      double fh = 0.0;
      for (int j = 0; j < m; ++j) {
        const double s_adu = f_true * p.P[static_cast<std::size_t>(j)];
        const double e_sky = kGain * kSky;
        const double e_src = kGain * s_adu;
        const double ne = static_cast<double>(pois(
                              rng, std::poisson_distribution<int>::param_type(
                                       e_sky + e_src))) + kRN * nz(rng);
        const double y = ne / kGain - kSky;   // 减已知天空
        fh += wrow[static_cast<std::size_t>(j)] * y;
      }
      s1 += fh; s2 += fh * fh;
      if (std::abs(fh - f_true) <= hw) ++cover;
    }
    const double mean = s1 / nrep;
    const double sd = std::sqrt(s2 / nrep - mean * mean);
    const double mc_se = sd / std::sqrt(2.0 * nrep);
    // 旧口径预测: I_F_old（1/ΣP）⇒ V_F_old > V_F_new; 物理世界真方差 == V_F_new
    const double i_old = old_if_known_bg(p, std::vector<double>(), 1.0, kFRef);
    const double v_old = 1.0 / i_old;
    std::printf("  MC(alpha==1) n=%d: bias=%+.4f (%.3f sigma) sd/sqrt(V_F)=%.5f "
                "(se %.4f%%) cover=%.4f | V_F_new=%.6g V_F_old=%.6g ratio=%.6f "
                "SumP=%.6f\n",
                nrep, mean - f_true, (mean - f_true) / std::sqrt(o.v_f),
                sd / std::sqrt(o.v_f), 100 * mc_se / sd,
                static_cast<double>(cover) / nrep, o.v_f, v_old, v_old / o.v_f,
                sum_P(p));
    chk(std::abs(mean - f_true) < 4.0 * std::sqrt(o.v_f) / std::sqrt(static_cast<double>(nrep)),
        "physical-world MC: new estimator unbiased (alpha==1)");
    chk(std::abs(sd / std::sqrt(o.v_f) - 1.0) < 4.0 * mc_se / std::sqrt(o.v_f),
        "physical-world MC: sd == sqrt(V_F) of frozen conditional model");
    const double cov_se = std::sqrt(0.95 * 0.05 / nrep);
    chk(std::abs(static_cast<double>(cover) / nrep - 0.95) < 4.0 * cov_se,
        "physical-world MC: 95% coverage matches nominal");
    chk(v_old / o.v_f > 1.0 + 1e-9,
        "old convention overstates V_F (1/SigmaP Poisson share) — fix direction");
  }

  // ── 6b. 物理世界 MC: 空间梯度场（新估计量无偏; 旧模板解析有偏）────────────
  // 物理世界（生产 var_plane 约定: applied 面）:
  //   native_j = (Poisson(g·(F_native·P_j + sky)) + RN·N(0,1))/g
  //   y_j      = α_j·native_j          ⇒ E[y_j] = α_j(F_native P_j + sky)
  //   Var[y_j] = α_j²(F_native P_j/g + sky/g + RN²/g²)
  //   已知背景 = α_j·sky（applied 面真实背景; 不是常量）。
  // 冻结条件模型: h_j = (α_j/α_*)P_j, v_j = α_j²blank + F_ref·α_j²P_j/(gα_*),
  // F_ref(u) = α_*·F_native ⇒ v_j 恰等于 Var[y_j] ⇒ sd == √V_F 应成立。
  {
    const Patch p = make_patch(150.25, 80.75, 9);
    const int m = static_cast<int>(p.px.size());
    const double k = 2.0, g1 = 0.35;   // 较强梯度使旧口径偏差可测
    std::vector<double> alpha(m);
    for (int j = 0; j < m; ++j) {
      const double xt = (p.px[static_cast<std::size_t>(j)] - p.cx) / 4.0;
      alpha[static_cast<std::size_t>(j)] = k * (1.0 + g1 * xt);
    }
    const double alpha_star = k;              // 控制点 m(u)=1
    const double f_native = kFRef / k;        // native 声明 ⇒ F_ref(u)=α_*·F_native
    const double f_ref_pt = alpha_star * f_native;
    const auto o = prod_vf(make_window(p, alpha, alpha_star, f_ref_pt,
                                       BackgroundNuisance::kKnown, OpMode::kIdentity,
                                       /*applied_blank=*/true),
                           BackgroundNuisance::kKnown);
    std::vector<double> h, v;
    h_and_v(p, alpha, alpha_star, f_ref_pt, h, v, /*applied_blank=*/true);
    double i_f = 0.0;
    for (int j = 0; j < m; ++j)
      i_f += h[static_cast<std::size_t>(j)] * h[static_cast<std::size_t>(j)] /
             v[static_cast<std::size_t>(j)];
    std::vector<double> wrow(m);
    for (int j = 0; j < m; ++j)
      wrow[static_cast<std::size_t>(j)] =
          h[static_cast<std::size_t>(j)] / v[static_cast<std::size_t>(j)] / i_f;
    std::mt19937_64 rng(20261017);
    std::normal_distribution<double> nz(0.0, 1.0);
    std::poisson_distribution<int> pois;
    const int nrep = 200000;
    double s1 = 0.0, s2 = 0.0;
    int cover = 0;
    const double hw = 1.959963984540054 * std::sqrt(o.v_f);
    for (int rep = 0; rep < nrep; ++rep) {
      double fh = 0.0;
      for (int j = 0; j < m; ++j) {
        const double aj = alpha[static_cast<std::size_t>(j)];
        const double P = p.P[static_cast<std::size_t>(j)];
        const double native = static_cast<double>(pois(
                                  rng, std::poisson_distribution<int>::param_type(
                                           kGain * (f_native * P + kSky)))) /
                                  kGain +
                              kRN * nz(rng) / kGain;
        const double y = aj * native;                  // applied 面
        fh += wrow[static_cast<std::size_t>(j)] * (y - aj * kSky);  // 减 α_j·sky
      }
      s1 += fh; s2 += fh * fh;
      if (std::abs(fh - f_ref_pt) <= hw) ++cover;
    }
    const double mean = s1 / nrep;
    const double sd = std::sqrt(s2 / nrep - mean * mean);
    const double mc_se = sd / std::sqrt(2.0 * nrep);
    std::printf("  MC(spatial) n=%d: bias=%+.4f (%.3f sigma) sd/sqrt(V_F)=%.5f "
                "(se %.4f%%) cover=%.4f | F_ref(u)=%.6g alpha_*=%.6g\n",
                nrep, mean - f_ref_pt, (mean - f_ref_pt) / std::sqrt(o.v_f),
                sd / std::sqrt(o.v_f), 100 * mc_se / sd,
                static_cast<double>(cover) / nrep, f_ref_pt, alpha_star);
    chk(std::abs(mean - f_ref_pt) < 4.0 * std::sqrt(o.v_f) / std::sqrt(static_cast<double>(nrep)),
        "physical-world MC: new (alpha-carrying) estimator unbiased (spatial field)");
    chk(std::abs(sd / std::sqrt(o.v_f) - 1.0) < 4.0 * mc_se / std::sqrt(o.v_f),
        "physical-world MC: sd == sqrt(V_F) (spatial field)");
    const double cov_se = std::sqrt(0.95 * 0.05 / nrep);
    chk(std::abs(static_cast<double>(cover) / nrep - 0.95) < 4.0 * cov_se,
        "physical-world MC: 95% coverage (spatial field)");

    // 旧口径对照（模板 h=P, 泊松 1/a_ref, applied blank）。判据不是"均值差多少"
    // 而是**模型自洽性**: 一个正确配对的模型必须满足
    //   (i)  E[F̂] = F_ref（声明的参考通量）,
    //   (ii) V_F = Σ_j w_j²·Var_true(y_j)（冻结条件方差 == 真采样方差）。
    // 对每个像元 Var_true(y_j) = α_j²(blank + F_native·P_j/g)（applied 面泊松+
    // 读噪真值, 不引用任何被测模型）。
    auto true_var = [&](int j) {
      const double aj = alpha[static_cast<std::size_t>(j)];
      return aj * aj * (blank_var() + f_native * p.P[static_cast<std::size_t>(j)] / kGain);
    };
    // 新模型（已由生产 V_F 给出）
    double sd_new2 = 0.0;
    for (int j = 0; j < m; ++j)
      sd_new2 += wrow[static_cast<std::size_t>(j)] * wrow[static_cast<std::size_t>(j)] *
                 true_var(j);
    // 旧模型
    const double i_old = old_if_known_bg(p, alpha, alpha_star, f_ref_pt, true);
    double a_ref = 0.0;
    for (int j = 0; j < m; ++j)
      a_ref += alpha[static_cast<std::size_t>(j)] * p.P[static_cast<std::size_t>(j)];
    std::vector<double> w_old(m, 0.0);
    double e_old = 0.0, sd_old2 = 0.0;
    for (int j = 0; j < m; ++j) {
      const double P = p.P[static_cast<std::size_t>(j)];
      const double aj = alpha[static_cast<std::size_t>(j)];
      const double vj_old = aj * aj * blank_var() +
                            (aj * aj / a_ref) * f_ref_pt * P / kGain;
      w_old[static_cast<std::size_t>(j)] = P / vj_old / i_old;
      e_old += w_old[static_cast<std::size_t>(j)] * aj * f_native * P;
      sd_old2 += w_old[static_cast<std::size_t>(j)] * w_old[static_cast<std::size_t>(j)] *
                 true_var(j);
    }
    const double v_old = 1.0 / i_old;
    std::printf("  self-consistency: new V_F=%g vs true sd_new^2=%g (rel %.2e); "
                "old V_F=%g vs true sd_old^2=%g (rel %.4f) | old E[F]=%g vs its "
                "F_ref=%g (rel %+.4f%%)\n",
                o.v_f, sd_new2, rel(o.v_f, sd_new2), v_old, sd_old2,
                rel(v_old, sd_old2), e_old, f_ref_pt,
                100 * (e_old - f_ref_pt) / f_ref_pt);
    chk(rel(o.v_f, sd_new2) < 1e-12,
        "new pairing: V_F == true sampling variance exactly (self-consistent)");
    chk(rel(v_old, sd_old2) > 1e-5,
        "old pairing: V_F != true sampling variance (not self-consistent)");
    // 对称梯度场下旧估计量均值偏差: 非零但较小（旧 v 用 α² 加权而信号只带 α）;
    // 这是被旧模型**误报**为可忽略的偏离, 如实报告其量级。
    std::printf("  (symmetric field: old mean bias %+.4f%%; old v weights alpha^2 "
                "while the signal carries alpha)\n",
                100 * (e_old - f_ref_pt) / f_ref_pt);

    // 非对称 α 场: 旧模板的一阶偏差。控制点仍在 u, 但场在足迹内非对称
    // （α(x) = k·(1 + g1·x̃ + g2·x̃²) 的二次项使加权均值 ≠ α_*）。
    {
      const double kg2 = 0.9;   // 强曲率 ⇒ 足迹内明显非对称
      std::vector<double> alpha2(m);
      for (int j = 0; j < m; ++j) {
        const double xt = (p.px[static_cast<std::size_t>(j)] - p.cx) / 4.0;
        alpha2[static_cast<std::size_t>(j)] = k * (1.0 + kg2 * xt * xt);
      }
      const double astar2 = k;   // u 处 m=1
      const double f_ref2 = astar2 * f_native;
      const double i_old2 = old_if_known_bg(p, alpha2, astar2, f_ref2, true);
      double a_ref2 = 0.0;
      for (int j = 0; j < m; ++j)
        a_ref2 += alpha2[static_cast<std::size_t>(j)] * p.P[static_cast<std::size_t>(j)];
      double e_old2 = 0.0, sd_old2b = 0.0;
      for (int j = 0; j < m; ++j) {
        const double P = p.P[static_cast<std::size_t>(j)];
        const double aj = alpha2[static_cast<std::size_t>(j)];
        const double vj = aj * aj * blank_var() +
                          (aj * aj / a_ref2) * f_ref2 * P / kGain;
        const double wj = P / vj / i_old2;
        e_old2 += wj * aj * f_native * P;
        sd_old2b += wj * wj * aj * aj *
                    (blank_var() + f_native * P / kGain);
      }
      const double v_old2 = 1.0 / i_old2;
      std::printf("  asymmetric field: old E[F]=%.6g vs F_ref=%.6g (rel bias "
                  "%+.4f%%); old V_F vs true sd^2 rel %.4f\n",
                  e_old2, f_ref2, 100 * (e_old2 - f_ref2) / f_ref2,
                  rel(v_old2, sd_old2b));
      chk(std::abs(e_old2 - f_ref2) / f_ref2 > 1e-3,
          "old h=P estimator has first-order bias under asymmetric alpha");
    }
  }

  // ── 7. 负例: 空间 α 场存在但 α_* 缺失/非正 ⇒ 具名 unavailable ────────────
  {
    const Patch p = make_patch(100.37, 200.61, 9);
    const int m = static_cast<int>(p.px.size());
    std::vector<double> alpha(m, 2.0);
    for (double astar : {0.0, -1.0}) {
      const auto o = prod_vf(make_window(p, alpha, astar, 0.0,
                                         BackgroundNuisance::kKnown, OpMode::kIdentity),
                             BackgroundNuisance::kKnown);
      std::printf("  alpha_star=%g ⇒ reason=\"%s\"\n", astar, o.reason.c_str());
      chk(!o.ok && o.reason == "reference_alpha_star_missing",
          "spatial alpha without declared alpha_* ⇒ named unavailable");
    }
  }

  std::printf("\n%s: %d failure(s)\n", fails ? "FAILED" : "ALL PASS", fails);
  return fails ? 1 : 0;
}
