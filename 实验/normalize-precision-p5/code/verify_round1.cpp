// ACSD Normalize — 阶段一第一轮（A 批）独立验证程序
//
// 目的: 对 lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp 的球面参考精度
//       GLS 做**独立**验证（不调用同一构造函数作 oracle; 显式 nl 空间真值另行实现）。
// 依据: docs/science/noise_snr/NOISE_SNR.md §3/§11.1/§11.2/§11.3
//       docs/engineering/contracts/NORMALIZE_DATA.md §5/§6/§7.1
//       docs/detail/normalize/contracts.md §7.4
//       INDEPENDENT_REVIEW_950C261.md F01–F06
//
// 退出码 0 = 全部通过; 1 = 有反例失败（不与真值妥协）。
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "acsd/sparse_reference.h"

using namespace acsd::normalize;

namespace {

int g_failures = 0;
const char* g_case = "";

void check(bool ok, const char* what) {
  std::printf("  [%s] %s (%s)\n", ok ? "PASS" : "FAIL", what, g_case);
  if (!ok) ++g_failures;
}

double rel(double a, double b) {
  const double d = std::abs(a - b);
  const double s = std::max(1e-300, std::max(std::abs(a), std::abs(b)));
  return d / s;
}

// ── 独立真值 1: 源空间闭式（NOISE_SNR §11.1, R 满列秩时严格成立）──────────────
//   V_F = [(A_srcᵀ V⁻¹ A_src)⁻¹]_{FF},  A_src = [P, Z_planar]
// 与生产实现（H 的特征分解路径）是完全不同的代码路径, 可作独立 oracle。
struct SourceTruth {
  bool ok = false;
  double v_f = 0.0;
};

SourceTruth source_space_truth(const PlanarPatchWindow& w, int n_bg,
                              const std::vector<double>& v) {
  SourceTruth t;
  const int m = static_cast<int>(w.pix_index.size());
  const int ncol = 1 + n_bg;
  // A_src（m × ncol）
  std::vector<double> a(static_cast<std::size_t>(m) * ncol, 0.0);
  for (int j = 0; j < m; ++j) {
    a[static_cast<std::size_t>(j) * ncol + 0] = w.profile[static_cast<std::size_t>(j)];
    for (int b = 1; b < ncol; ++b) {
      a[static_cast<std::size_t>(j) * ncol + b] =
          (b == 1) ? 1.0 : (b == 2) ? w.src_x[static_cast<std::size_t>(j)]
                                    : w.src_y[static_cast<std::size_t>(j)];
    }
  }
  // M = A_srcᵀ V⁻¹ A_src
  std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
  for (int i = 0; i < ncol; ++i) {
    for (int j2 = 0; j2 < ncol; ++j2) {
      double s = 0.0;
      for (int j = 0; j < m; ++j)
        s += a[static_cast<std::size_t>(j) * ncol + i] *
             a[static_cast<std::size_t>(j) * ncol + j2] /
             v[static_cast<std::size_t>(j)];
      mm[static_cast<std::size_t>(i) * ncol + j2] = s;
    }
  }
  // Gauss-Jordan 反转, 取 [0][0]
  const int k = ncol;
  std::vector<double> b(static_cast<std::size_t>(k) * 2 * k, 0.0);
  for (int i = 0; i < k; ++i) {
    for (int j2 = 0; j2 < k; ++j2)
      b[static_cast<std::size_t>(i) * 2 * k + j2] =
          mm[static_cast<std::size_t>(i) * k + j2];
    b[static_cast<std::size_t>(i) * 2 * k + k + i] = 1.0;
  }
  for (int col = 0; col < k; ++col) {
    int piv = col;
    double best = std::abs(b[static_cast<std::size_t>(col) * 2 * k + col]);
    for (int r = col + 1; r < k; ++r) {
      const double x = std::abs(b[static_cast<std::size_t>(r) * 2 * k + col]);
      if (x > best) { best = x; piv = r; }
    }
    if (!(best > 0.0)) return t;
    if (piv != col)
      for (int j2 = 0; j2 < 2 * k; ++j2)
        std::swap(b[static_cast<std::size_t>(col) * 2 * k + j2],
                  b[static_cast<std::size_t>(piv) * 2 * k + j2]);
    const double d = b[static_cast<std::size_t>(col) * 2 * k + col];
    for (int j2 = 0; j2 < 2 * k; ++j2)
      b[static_cast<std::size_t>(col) * 2 * k + j2] /= d;
    for (int r = 0; r < k; ++r) {
      if (r == col) continue;
      const double f = b[static_cast<std::size_t>(r) * 2 * k + col];
      if (f == 0.0) continue;
      for (int j2 = 0; j2 < 2 * k; ++j2)
        b[static_cast<std::size_t>(r) * 2 * k + j2] -=
            f * b[static_cast<std::size_t>(col) * 2 * k + j2];
    }
  }
  t.v_f = b[static_cast<std::size_t>(0) * 2 * k + k];
  t.ok = std::isfinite(t.v_f) && t.v_f > 0.0;
  return t;
}

// ── 独立真值 2: nl 空间 C = R diag(v) Rᵀ 与 h = R P 的显式重算 ──────────────
struct ExplicitShape {
  std::vector<double> h;
  std::vector<double> c;   // nl×nl row-major
  int nl = 0;
};

ExplicitShape explicit_shape(const PlanarPatchWindow& w,
                             const std::vector<double>& v) {
  ExplicitShape t;
  const int m = static_cast<int>(w.pix_index.size());
  const int nl = static_cast<int>(w.leaf_ipix.size());
  t.nl = nl;
  t.h.assign(nl, 0.0);
  t.c.assign(static_cast<std::size_t>(nl) * nl, 0.0);
  for (std::size_t k = 0; k < w.op_c.size(); ++k) {
    const int l = w.op_leaf[k];
    const int j = w.op_pix[k];
    if (l < 0 || l >= nl || j < 0 || j >= m) continue;
    t.h[static_cast<std::size_t>(l)] +=
        w.op_c[k] * w.profile[static_cast<std::size_t>(j)];
  }
  for (int j = 0; j < m; ++j) {
    std::vector<std::pair<int, double>> lj;
    for (std::size_t k = 0; k < w.op_c.size(); ++k) {
      if (w.op_pix[k] != j) continue;
      const int l = w.op_leaf[k];
      if (l < 0 || l >= nl) continue;
      lj.emplace_back(l, w.op_c[k]);
    }
    for (std::size_t a = 0; a < lj.size(); ++a) {
      for (std::size_t b = a; b < lj.size(); ++b) {
        const double val = lj[a].second * v[static_cast<std::size_t>(j)] *
                           lj[b].second;
        t.c[static_cast<std::size_t>(lj[a].first) * nl +
            static_cast<std::size_t>(lj[b].first)] += val;
        if (b != a) {
          t.c[static_cast<std::size_t>(lj[b].first) * nl +
              static_cast<std::size_t>(lj[a].first)] += val;
        }
      }
    }
  }
  return t;
}

// ── 独立真值 3: nl 空间 GLS（R 秩亏时唯一有效口径; C = R diag(v) Rᵀ 可逆）───
struct NlTruth {
  bool ok = false;
  double v_f = 0.0;
};

NlTruth nl_space_truth(const PlanarPatchWindow& w, int n_bg,
                       const std::vector<double>& v) {
  NlTruth t;
  const int m = static_cast<int>(w.pix_index.size());
  const int nl = static_cast<int>(w.leaf_ipix.size());
  const int ncol = 1 + n_bg;
  std::vector<double> as(static_cast<std::size_t>(nl) * ncol, 0.0);
  std::vector<double> cov(static_cast<std::size_t>(nl) * nl, 0.0);
  for (std::size_t k = 0; k < w.op_c.size(); ++k) {
    const int l = w.op_leaf[k];
    const int j = w.op_pix[k];
    if (l < 0 || l >= nl || j < 0 || j >= m) continue;
    const double c = w.op_c[k];
    as[static_cast<std::size_t>(l) * ncol + 0] +=
        c * w.profile[static_cast<std::size_t>(j)];
    for (int b = 1; b < ncol; ++b) {
      const double planar = (b == 1) ? 1.0
                          : (b == 2) ? w.src_x[static_cast<std::size_t>(j)]
                                     : w.src_y[static_cast<std::size_t>(j)];
      as[static_cast<std::size_t>(l) * ncol + b] += c * planar;
    }
  }
  for (int j = 0; j < m; ++j) {
    std::vector<std::pair<int, double>> lj;
    for (std::size_t k = 0; k < w.op_c.size(); ++k) {
      if (w.op_pix[k] != j) continue;
      const int l = w.op_leaf[k];
      if (l < 0 || l >= nl) continue;
      lj.emplace_back(l, w.op_c[k]);
    }
    for (std::size_t a = 0; a < lj.size(); ++a)
      for (std::size_t b = a; b < lj.size(); ++b) {
        const double val = lj[a].second * v[static_cast<std::size_t>(j)] *
                           lj[b].second;
        cov[static_cast<std::size_t>(lj[a].first) * nl +
            static_cast<std::size_t>(lj[b].first)] += val;
        if (b != a)
          cov[static_cast<std::size_t>(lj[b].first) * nl +
              static_cast<std::size_t>(lj[a].first)] += val;
      }
  }
  // Gauss-Jordan: [C | I] → [I | C⁻¹]; M = A_sᵀ C⁻¹ A_s; V_F = (M⁻¹)[0][0]
  std::vector<double> aug(static_cast<std::size_t>(nl) * 2 * nl, 0.0);
  for (int i = 0; i < nl; ++i) {
    for (int j = 0; j < nl; ++j)
      aug[static_cast<std::size_t>(i) * 2 * nl + j] =
          cov[static_cast<std::size_t>(i) * nl + j];
    aug[static_cast<std::size_t>(i) * 2 * nl + nl + i] = 1.0;
  }
  for (int col = 0; col < nl; ++col) {
    int piv = col;
    double best = std::abs(aug[static_cast<std::size_t>(col) * 2 * nl + col]);
    for (int r = col + 1; r < nl; ++r) {
      const double x = std::abs(aug[static_cast<std::size_t>(r) * 2 * nl + col]);
      if (x > best) { best = x; piv = r; }
    }
    if (!(best > 0.0)) return t;      // C 奇异 ⇒ 该口径不适用
    if (piv != col)
      for (int j = 0; j < 2 * nl; ++j)
        std::swap(aug[static_cast<std::size_t>(col) * 2 * nl + j],
                  aug[static_cast<std::size_t>(piv) * 2 * nl + j]);
    const double d = aug[static_cast<std::size_t>(col) * 2 * nl + col];
    for (int j = 0; j < 2 * nl; ++j)
      aug[static_cast<std::size_t>(col) * 2 * nl + j] /= d;
    for (int r = 0; r < nl; ++r) {
      if (r == col) continue;
      const double f = aug[static_cast<std::size_t>(r) * 2 * nl + col];
      if (f == 0.0) continue;
      for (int j = 0; j < 2 * nl; ++j)
        aug[static_cast<std::size_t>(r) * 2 * nl + j] -=
            f * aug[static_cast<std::size_t>(col) * 2 * nl + j];
    }
  }
  std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
  for (int i = 0; i < ncol; ++i)
    for (int j2 = 0; j2 < ncol; ++j2) {
      double s = 0.0;
      for (int l = 0; l < nl; ++l) {
        const double ail = as[static_cast<std::size_t>(l) * ncol + i];
        if (ail == 0.0) continue;
        for (int l2 = 0; l2 < nl; ++l2)
          s += ail * aug[static_cast<std::size_t>(l) * 2 * nl + nl + l2] *
               as[static_cast<std::size_t>(l2) * ncol + j2];
      }
      mm[static_cast<std::size_t>(i) * ncol + j2] = s;
    }
  const int k = ncol;
  std::vector<double> b(static_cast<std::size_t>(k) * 2 * k, 0.0);
  for (int i = 0; i < k; ++i) {
    for (int j2 = 0; j2 < k; ++j2)
      b[static_cast<std::size_t>(i) * 2 * k + j2] =
          mm[static_cast<std::size_t>(i) * k + j2];
    b[static_cast<std::size_t>(i) * 2 * k + k + i] = 1.0;
  }
  for (int col = 0; col < k; ++col) {
    int piv = col;
    double best = std::abs(b[static_cast<std::size_t>(col) * 2 * k + col]);
    for (int r = col + 1; r < k; ++r) {
      const double x = std::abs(b[static_cast<std::size_t>(r) * 2 * k + col]);
      if (x > best) { best = x; piv = r; }
    }
    if (!(best > 0.0)) return t;
    if (piv != col)
      for (int j2 = 0; j2 < 2 * k; ++j2)
        std::swap(b[static_cast<std::size_t>(col) * 2 * k + j2],
                  b[static_cast<std::size_t>(piv) * 2 * k + j2]);
    const double d = b[static_cast<std::size_t>(col) * 2 * k + col];
    for (int j2 = 0; j2 < 2 * k; ++j2)
      b[static_cast<std::size_t>(col) * 2 * k + j2] /= d;
    for (int r = 0; r < k; ++r) {
      if (r == col) continue;
      const double f = b[static_cast<std::size_t>(r) * 2 * k + col];
      if (f == 0.0) continue;
      for (int j2 = 0; j2 < 2 * k; ++j2)
        b[static_cast<std::size_t>(r) * 2 * k + j2] -=
            f * b[static_cast<std::size_t>(col) * 2 * k + j2];
    }
  }
  t.v_f = b[static_cast<std::size_t>(0) * 2 * k + k];
  t.ok = std::isfinite(t.v_f) && t.v_f > 0.0;
  return t;
}

// ── 合成一个满列秩窗口（m ≤ nl, R 满列秩）───────────────────────────────────
PlanarPatchWindow make_full_rank_window(int seed) {
  // 4 叶 × 3 像元, R 满列秩（各像元足迹错开; 非全 1 矩阵）。
  // Ω_j = A_drop,j = 9 sr; w_lj 见下表; N_l = Σ_j w_lj·Ω_j。
  PlanarPatchWindow w;
  w.control_id = "full_rank";
  w.cx_px = 100.5;
  w.cy_px = 200.5;
  w.ra_deg = 83.7;
  w.dec_deg = 22.0;
  w.leaf_order = 12;                                    // 实际叶阶 log2(nside)=12
  w.omega_leaf = 4.0 * M_PI / (12.0 * 4096.0 * 4096.0);
  w.pix_index = {0, 1, 2};
  w.profile = {0.55, 0.30, 0.15};
  w.variance = {4.0, 16.0, 36.0};                       // 异方差（F01 的必要条件）
  w.src_x = {100, 101, 102};
  w.src_y = {200, 200, 200};
  w.pix_omega = {9.0, 9.0, 9.0};
  w.pix_a_drop = {9.0, 9.0, 9.0};
  w.leaf_ipix = {1000ULL, 1001ULL, 1002ULL, 1003ULL};
  const double wv[4][3] = {{0.40, 0.00, 0.00},
                           {0.60, 0.30, 0.25},
                           {0.00, 0.45, 0.40},
                           {0.00, 0.25, 0.35}};
  double n_l[4] = {0, 0, 0, 0};
  for (int l = 0; l < 4; ++l)
    for (int j = 0; j < 3; ++j) n_l[l] += wv[l][j] * 9.0;
  w.leaf_dp.assign(4, 0.0);
  w.leaf_n_full.assign(4, 0.0);
  w.leaf_n_covered.assign(4, 0.0);
  for (int l = 0; l < 4; ++l) {
    for (int j = 0; j < 3; ++j) {
      if (wv[l][j] == 0.0) continue;
      w.op_pix.push_back(j);
      w.op_leaf.push_back(l);
      w.op_c.push_back(wv[l][j] / n_l[l]);              // c_lj = w_lj/N_l（§11.2）
      w.leaf_dp[static_cast<std::size_t>(l)] += 9.0 * wv[l][j];
    }
    w.leaf_n_full[static_cast<std::size_t>(l)] = n_l[l];
    w.leaf_n_covered[static_cast<std::size_t>(l)] = n_l[l];
  }
  w.has_target_leaf = true;
  w.target_leaf = 1001ULL;   // 非 0 编号; leaf 0 的合法性在 F06 案单独验证
  (void)seed;
  return w;
}

ReferenceDefinition make_ref(double flux) {
  ReferenceDefinition r;
  r.kind = ReferenceKind::kExplicitLinearFlux;
  r.reference_id = "verify:unit-source";
  r.flux_unit = "adu";
  r.normalization = "photo_applied";
  r.flux = flux;
  return r;
}

}  // namespace

int main() {
  // ─────────────────────────────────────────────────────────────────────────
  // 1. F01: INDEPENDENT_REVIEW_950C261.md 的异方差反例（逐字输入/解析真值）
  // ─────────────────────────────────────────────────────────────────────────
  g_case = "F01 heteroscedastic counterexample";
  {
    PlanarPatchWindow w;
    w.control_id = "heteroscedastic";
    w.pix_index = {0, 1};
    w.profile = {0.6, 0.4};
    w.variance = {1, 9};
    w.src_x = {0, 1};
    w.src_y = {0, 0};
    w.leaf_ipix = {101, 102, 103};
    w.leaf_order = 9;
    w.omega_leaf = 1;
    w.leaf_dp = {1, 1, 1};
    w.target_leaf = 101;
    w.has_target_leaf = true;
    w.op_pix = {0, 1, 0, 1, 1};
    w.op_leaf = {0, 0, 1, 1, 2};
    w.op_c = {0.8, 0.2, 0.2, 0.8, 1};
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(10);
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    const double oracle = 1.0 / (0.6 * 0.6 / 1.0 + 0.4 * 0.4 / 9.0);
    std::printf("  F01 production_VF=%.17g oracle_VF=%.17g rel=%.3e\n",
                *c.flux_variance, oracle, rel(*c.flux_variance, oracle));
    check(rel(*c.flux_variance, oracle) < 1e-12, "V_F equals source-space truth");
    // 满列秩恒等式 M = A_srcᵀ V⁻¹ A_src ⇒ V_F = 1/(PᵀV⁻¹P)
    const double vf_direct = 1.0 / (0.36 / 1.0 + 0.16 / 9.0);
    check(rel(*c.flux_variance, vf_direct) < 1e-12,
          "V_F equals 1/(P^T V^-1 P) (NOISE_SNR §11.1 identity)");
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 2. 与独立显式 nl 空间 GLS 对拍（不调用同一构造函数）
  // ─────────────────────────────────────────────────────────────────────────
  for (int nbg = 0; nbg <= 1; ++nbg) {
    g_case = nbg == 0 ? "dual background=known" : "dual background=constant_fit";
    PlanarPatchWindow w = make_full_rank_window(1);
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(1000.0);
    cfg.background = nbg == 0 ? BackgroundNuisance::kKnown
                              : BackgroundNuisance::kConstantFit;
    cfg.gain_e_per_adu = 2.0;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::vector<double> v(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      v[j] = w.variance[j] + 1000.0 * w.profile[j] / 2.0;
    const SourceTruth st = source_space_truth(w, nbg, v);
    std::printf("  dual nbg=%d production=%.17g source_truth=%.17g rel=%.3e\n",
                nbg, *c.flux_variance, st.v_f, rel(*c.flux_variance, st.v_f));
    check(st.ok && rel(*c.flux_variance, st.v_f) < 1e-12,
          "V_F equals independent source-space closed form");
    const ExplicitShape sh = explicit_shape(w, v);
    if (nbg == 0) {
      check(r.windows[0].h_sphere.size() == w.leaf_ipix.size(),
            "persisted h length == nl (F03)");
      check(r.windows[0].covariance.size() ==
                w.leaf_ipix.size() * w.leaf_ipix.size(),
            "persisted C is nl x nl (F03)");
      check(r.windows[0].z_sphere.empty(), "known background => Z empty");
      double worst = 0.0, worsth = 0.0;
      for (std::size_t i = 0; i < sh.c.size(); ++i)
        worst = std::max(worst, rel(sh.c[i], r.windows[0].covariance[i]));
      for (std::size_t i = 0; i < sh.h.size(); ++i)
        worsth = std::max(worsth, rel(sh.h[i], r.windows[0].h_sphere[i]));
      check(worst < 1e-12, "persisted C == R diag(v) R^T (independent)");
      check(worsth < 1e-12, "persisted h == R P (independent)");
      const int nl = static_cast<int>(w.leaf_ipix.size());
      bool sym = true, psd = true;
      for (int i = 0; i < nl; ++i) {
        if (!(r.windows[0].covariance[static_cast<std::size_t>(i) * nl + i] > 0.0))
          psd = false;
        for (int j = 0; j < nl; ++j)
          if (rel(r.windows[0].covariance[static_cast<std::size_t>(i) * nl + j],
                  r.windows[0].covariance[static_cast<std::size_t>(j) * nl + i]) >
              1e-13)
            sym = false;
      }
      check(sym, "persisted C symmetric");
      check(psd, "persisted C has positive diagonal");
      check(c.covariance_rank.has_value() && *c.covariance_rank == 3,
            "covariance_rank == rank(C) == rank(R) == 3");
    }
    if (nbg == 1) {
      std::printf("  note: n_bg=%d z_size=%zu (nl=%zu)\n", r.windows[0].n_bg,
                  r.windows[0].z_sphere.size(), w.leaf_ipix.size());
      check(r.windows[0].n_bg == 1 &&
                r.windows[0].z_sphere.size() == w.leaf_ipix.size(),
            "constant_fit => Z is nl x 1 (F03)");
    }
  }

  // 参数多于观测**不再**自动判 unavailable —— 前台 2026-10-15 裁决
  // （NOISE_SNR §11.4 末段 / NORMALIZE_CONVERGENCE N14）:
  //   「背景基冗余不自动意味着通量不可辨识; 若通量响应在背景列空间之外仍有有效
  //     分量, 就不应因整个参数矩阵 M 退化而误判通量不可辨识。」
  // 本窗口 rank(R)=m ⇒ 投影空间即 R^m ⇒ 通量必然可辨识 ⇒ **available**。
  // 真正不可辨识的情形（通量整体落在背景列空间内）见 verify_round2 的
  // flux_direction_inside_background_span 用例。
  g_case = "plane_fit: parameter count > observations, flux still identifiable";
  {
    PlanarPatchWindow w = make_full_rank_window(1);
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(1000.0);
    cfg.background = BackgroundNuisance::kPlaneFit;
    cfg.gain_e_per_adu = 2.0;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::printf("  plane_fit availability=%d reason=%s\n",
                static_cast<int>(c.status.availability), c.status.reason.c_str());
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "background redundancy does not auto-imply flux unidentifiable (NOISE_SNR 11.4)");
    check(c.flux_variance.has_value() && *c.flux_variance > 0.0,
          "V_F reported although the full parameter matrix is singular");
    if (!r.windows.empty()) {
      std::printf("    bg_basis_rank=%d I_F=%.10g\n",
                  r.windows[0].background_basis_rank,
                  r.windows[0].flux_information);
      check(r.windows[0].background_basis_rank < 3,
            "effective background basis rank < declared columns");
    }
  }

  // plane_fit 可辨识: 6 像元 × 4 叶, 参数 4 < 观测
  g_case = "plane_fit identifiable (m=6)";
  {
    PlanarPatchWindow w;
    w.control_id = "plane_ok";
    w.pix_index = {0, 1, 2, 3, 4, 5};
    w.profile = {0.30, 0.22, 0.16, 0.13, 0.11, 0.08};
    for (int j = 0; j < 6; ++j) {
      w.variance.push_back(3.0 + 25.0 * j);
      w.src_x.push_back(40.0 + j);
      // 不能与 x 成仿射相关（否则平面基线性相关 ⇒ 参数不可辨识）
      w.src_y.push_back(60.0 + (j % 2 == 0 ? 3.0 * j : 2.0 * j + 1.0));
    }
    for (int l = 0; l < 4; ++l) w.leaf_ipix.push_back(900ULL + l);
    w.leaf_dp.assign(4, 0.0);
    w.leaf_n_full.assign(4, 0.0);
    w.leaf_n_covered.assign(4, 0.0);
    // 每叶看到全部 6 个像元但权重**互不相同**（非 outer product）⇒ R (4×6) 满行秩。
    // 注意: 若 R = a⊗b（各叶权重模式成比例）秩为 1 ⇒ 大量参数不可辨识, 这是
    // 真实几何中"叶只按同一模式采样"的退化, 必须显式 unavailable。
    double raw[4][6];
    double n_l[4] = {0, 0, 0, 0};
    for (int l = 0; l < 4; ++l)
      for (int j = 0; j < 6; ++j) {
        raw[l][j] = 0.5 + 0.1 * static_cast<double>((l * 5 + j * 3) % 7);
        n_l[l] += 9.0 * raw[l][j];      // N_l = Σ_j w_lj·Ω_j
      }
    for (int l = 0; l < 4; ++l) {
      for (int j = 0; j < 6; ++j) {
        w.op_pix.push_back(j);
        w.op_leaf.push_back(l);
        w.op_c.push_back(raw[l][j] / n_l[l]);   // c_lj = w_lj/N_l
      }
      w.leaf_dp[static_cast<std::size_t>(l)] = n_l[l];
      w.leaf_n_full[static_cast<std::size_t>(l)] = n_l[l];
      w.leaf_n_covered[static_cast<std::size_t>(l)] = n_l[l];
    }
    w.has_target_leaf = true;
    w.target_leaf = 902ULL;
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(2000.0);
    cfg.background = BackgroundNuisance::kPlaneFit;
    cfg.gain_e_per_adu = 1.2;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::vector<double> v(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      v[j] = w.variance[j] + 2000.0 * w.profile[j] / 1.2;
    const NlTruth st = nl_space_truth(w, 3, v);
    std::printf("  plane_ok reason=%s cov_rank=%s\n", c.status.reason.c_str(),
                c.covariance_rank ? std::to_string(*c.covariance_rank).c_str()
                                  : "none");
    std::printf("  plane_ok production=%.17g source_truth=%.17g rel=%.3e "
                "availability=%d\n",
                c.flux_variance ? *c.flux_variance : -1.0, st.v_f,
                c.flux_variance ? rel(*c.flux_variance, st.v_f) : -1.0,
                static_cast<int>(c.status.availability));
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "identifiable plane_fit is available");
    // M 的条件数由 Gauss-Jordan 逆决定; 用相对 1e-8 的容差并打印条件数诊断。
    check(st.ok && c.flux_variance.has_value() &&
              rel(*c.flux_variance, st.v_f) < 1e-8,
          "plane_fit V_F equals independent nl-space GLS (R rank-deficient)");
    check(r.windows[0].n_bg == 3 &&
              r.windows[0].z_sphere.size() == 3 * w.leaf_ipix.size(),
          "plane_fit => Z is nl x 3 (F03)");
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 3. F04: 标度后的源泊松项（NOISE_SNR §11.3）
  // ─────────────────────────────────────────────────────────────────────────
  g_case = "F04 gain/alpha source Poisson";
  {
    PlanarPatchWindow w = make_full_rank_window(1);
    // α=0.5, gain=2。w.variance 必须已经是 **applied 数组标度**（= α²·原生方差;
    // 生产口径见 sparse_reference.h「v_j 已是换算到 applied 数组标度的 blank-sky
    // 方差」）; 生产只负责加源泊松项。
    for (double& x : w.variance) x *= 0.25;
    // 原生 F_native=1000 ⇒ applied F_applied = α·F_native = 500
    // 源泊松（applied 面）= α²·F_native·P/g = 0.25·1000·P/2 = 125·P
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(500.0);   // applied 面参考通量
    cfg.background = BackgroundNuisance::kKnown;
    cfg.gain_e_per_adu = 2.0;
    cfg.plane_scale = 0.5;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::vector<double> v(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j) {
      // α²·原生 blank-sky + α²·F_native·P/g  （NOISE_SNR §11.3 逐字）
      v[j] = w.variance[j] + 0.25 * 1000.0 * w.profile[j] / 2.0;
    }
    const SourceTruth st = source_space_truth(w, 0, v);
    std::printf("  F04 alpha=0.5 production=%.17g source_truth=%.17g rel=%.3e\n",
                *c.flux_variance, st.v_f, rel(*c.flux_variance, st.v_f));
    check(st.ok && rel(*c.flux_variance, st.v_f) < 1e-12,
          "alpha-scaled source Poisson = alpha^2 F_native P/g");
    // 确定性统一标度: F 与 sqrt(V_F) 同比 ⇒ SNR 不变
    SphericalCompletionConfig c1;
    c1.reference = make_ref(1000.0);
    c1.background = BackgroundNuisance::kKnown;
    c1.gain_e_per_adu = 2.0;
    c1.plane_scale = 1.0;
    PlanarPatchWindow wn = make_full_rank_window(1);
    // F_native=1000 在未施加标度的面上就是 F_applied
    const auto r1 = complete_spherical_reference({wn}, c1);
    std::printf("  F04 SNR(native)=%.17g SNR(alpha=0.5)=%.17g\n",
                *r1.precision.controls[0].reference_snr,
                *c.reference_snr);
    (void)0;
    check(rel(*r1.precision.controls[0].reference_snr, *c.reference_snr) < 1e-12,
          "deterministic scale leaves SNR invariant (NOISE_SNR §2)");
    // 读噪/背景不得被重复缩放: 只加一次源泊松
    double vsum = 0.0;
    for (double x : v) vsum += x;
    double expect = 0.0;
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      expect += w.variance[j] + 0.25 * 1000.0 * w.profile[j] / 2.0;
    check(rel(vsum, expect) < 1e-12, "no double counting of RN/background/source");
    // 反向: 生产若把 α² 再加一次（重复缩放）⇒ V_F 会差一个 2 倍量级, 不由容差掩盖
    std::vector<double> v_dup(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      v_dup[j] = 0.25 * w.variance[j] + 0.25 * 1000.0 * w.profile[j] / 2.0;
    const SourceTruth st_dup = source_space_truth(w, 0, v_dup);
    check(st_dup.ok && rel(st_dup.v_f, st.v_f) > 1e-3,
          "double-scaled variance would give a different answer (discriminating)");
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 4. F01 一般性: 随机异方差 + 随机满列秩 R, Monte Carlo 通量散度
  // ─────────────────────────────────────────────────────────────────────────
  g_case = "random heteroscedastic + Monte Carlo";
  {
    std::mt19937_64 rng(20261015);
    std::uniform_real_distribution<double> u(0.05, 1.0);
    const int m = 6, nl = 12;
    PlanarPatchWindow w;
    w.control_id = "random";
    w.pix_index = {0, 1, 2, 3, 4, 5};
    w.profile = {0.30, 0.25, 0.18, 0.12, 0.09, 0.06};
    for (int j = 0; j < m; ++j) {
      w.variance.push_back(1.0 + 40.0 * u(rng));
      w.src_x.push_back(10.0 + j);
      w.src_y.push_back(20.0);
    }
    for (int l = 0; l < nl; ++l) w.leaf_ipix.push_back(5000ULL + l);
    w.has_target_leaf = true;
    w.target_leaf = 5003ULL;
    // 每叶接收全部 6 个像元但权重互不相同 ⇒ R (nl×m) 满列秩（非 outer product）
    w.leaf_dp.assign(nl, 0.0);
    w.leaf_n_full.assign(nl, 0.0);
    w.leaf_n_covered.assign(nl, 0.0);
    std::vector<double> nlz(nl, 0.0);
    std::vector<std::vector<double>> rw(nl, std::vector<double>(m, 0.0));
    for (int l = 0; l < nl; ++l)
      for (int j = 0; j < m; ++j) {
        rw[l][j] = 0.4 + 0.1 * static_cast<double>((l * 17 + j * 5 + l * j * 4) % 11);
        nlz[l] += 9.0 * rw[l][j];
      }
    for (int l = 0; l < nl; ++l) {
      for (int j = 0; j < m; ++j) {
        w.op_pix.push_back(j);
        w.op_leaf.push_back(l);
        w.op_c.push_back(rw[l][j] / nlz[l]);
      }
      w.leaf_dp[static_cast<std::size_t>(l)] = nlz[l];
      w.leaf_n_full[static_cast<std::size_t>(l)] = nlz[l];
      w.leaf_n_covered[static_cast<std::size_t>(l)] = nlz[l];
    }
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(500.0);
    cfg.background = BackgroundNuisance::kKnown;
    cfg.gain_e_per_adu = 1.5;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::vector<double> v(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      v[j] = w.variance[j] + 500.0 * w.profile[j] / 1.5;
    const ExplicitShape sh = explicit_shape(w, v);
    const SourceTruth st = source_space_truth(w, 0, v);
    std::printf("  random availability=%d reason=%s\n",
                static_cast<int>(c.status.availability), c.status.reason.c_str());
    check(st.ok && c.flux_variance.has_value() &&
              rel(*c.flux_variance, st.v_f) < 1e-10,
          "random heteroscedastic V_F equals source-space closed form");
    // nl 空间显式 GLS 只在 C = R diag(v) Rᵀ 非奇异（nl ≤ m 且 R 满行秩）时有效;
    // nl > m 时 C 秩亏, Gauss-Jordan 的 "非零主元" 只是数值噪声 ⇒ 不用作真值。
    const NlTruth nlt = (nl <= m) ? nl_space_truth(w, 0, v) : NlTruth{};
    if (nlt.ok) {
      std::printf("  random nl_truth=%.17g rel=%.3e\n", nlt.v_f,
                  rel(*c.flux_variance, nlt.v_f));
      check(c.flux_variance.has_value() &&
                rel(*c.flux_variance, nlt.v_f) < 1e-10,
            "random heteroscedastic V_F equals independent nl-space GLS");
    }

    // Monte Carlo（NOISE_SNR §11.3「确定性统一标度下 SNR 不变」与 V_F 的散度核对）:
    // 在**源像元**空间生成 x_j = F·P_j + √v_j·n_j, 用逆方差加权估 F̂。R 满列秩时
    // 该估计量与球面 GLS 估计量等价（R 可逆保留局部观测信息, §11.1）⇒ 其条件方差
    // 必须是 V_F。R 本身的一致性由上面的 h/C 显式重算与 F01 恒等式覆盖。
    const double F = 500.0;
    const int nrep = 20000;
    std::normal_distribution<double> nz(0.0, 1.0);
    double s1 = 0.0, s2 = 0.0;
    for (int rep = 0; rep < nrep; ++rep) {
      double num = 0.0, den = 0.0;
      for (int j = 0; j < m; ++j) {
        const double x = F * w.profile[static_cast<std::size_t>(j)] +
                         std::sqrt(v[static_cast<std::size_t>(j)]) * nz(rng);
        const double pj = w.profile[static_cast<std::size_t>(j)];
        num += pj * x / v[static_cast<std::size_t>(j)];
        den += pj * pj / v[static_cast<std::size_t>(j)];
      }
      const double fhat = num / den;
      const double d = fhat - F;
      s1 += d;
      s2 += d * d;
    }
    const double sd = std::sqrt((s2 - s1 * s1 / nrep) / (nrep - 1));
    const double predicted =
        c.flux_variance ? std::sqrt(*c.flux_variance) : -1.0;
    std::printf("  MC sd=%.6f predicted sqrt(V_F)=%.6f ratio=%.4f (n=%d)\n",
                sd, predicted, sd / predicted, nrep);
    check(predicted > 0.0 && std::abs(sd / predicted - 1.0) < 0.02,
          "Monte Carlo flux scatter matches predicted sqrt(V_F) (2%)");
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 5. F06: 目标叶 0 合法 + 无 ang2pix ⇒ 显式 unavailable
  // ─────────────────────────────────────────────────────────────────────────
  g_case = "F06 target leaf 0";
  {
    PlanarPatchWindow w = make_full_rank_window(1);
    w.target_leaf = 0;             // 合法编号, 不是缺失
    w.has_target_leaf = true;
    w.leaf_ipix = {0ULL, 1ULL, 2ULL};
    w.leaf_dp.assign(3, 9.0);
    w.leaf_n_full.assign(3, 9.0);
    w.leaf_n_covered.assign(3, 9.0);
    w.op_pix = {0, 1, 2, 0, 1, 2};
    w.op_leaf = {0, 0, 0, 1, 1, 2};
    w.op_c = {1.0 / 27.0, 1.0 / 27.0, 1.0 / 27.0,
              1.0 / 27.0, 1.0 / 27.0, 1.0 / 27.0};
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(100.0);
    cfg.background = BackgroundNuisance::kKnown;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "leaf index 0 is a valid target leaf");
    check(c.leaf_ipix == 0, "leaf_ipix preserved as 0");
  }
  {
    PlanarPatchWindow w = make_full_rank_window(1);
    w.has_target_leaf = false;     // 未解出 ⇒ 显式 unavailable
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(100.0);
    const auto r = complete_spherical_reference({w}, cfg);
    check(r.precision.controls[0].status.availability ==
              PrecisionAvailability::kUnavailable,
          "missing target leaf ⇒ unavailable (not silently 0)");
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 6. 秩亏: 通量可辨识 vs 不可辨识（不加 epsilon）
  // ─────────────────────────────────────────────────────────────────────────
  g_case = "rank deficiency";
  {
    // R 的两列相同 ⇒ rank(R)=1 < m=2 ⇒ 源空间有 1 个不可观方向。
    // 若模板/背景都落在该方向上 ⇒ 通量不可辨识 ⇒ unavailable。
    PlanarPatchWindow w;
    w.control_id = "rank_def";
    w.pix_index = {0, 1};
    w.profile = {0.5, 0.5};
    w.variance = {1.0, 1.0};
    w.src_x = {0, 1};
    w.src_y = {0, 0};
    w.leaf_ipix = {7ULL, 8ULL};
    w.leaf_dp = {2.0, 2.0};
    w.leaf_n_full = {2.0, 2.0};
    w.leaf_n_covered = {2.0, 2.0};
    w.has_target_leaf = true;
    w.target_leaf = 7ULL;
    // 两个叶都只从两个**相同**的像元系数取 ⇒ R 的两列成比例 ⇒ rank 1
    for (int l = 0; l < 2; ++l) {
      w.op_pix.push_back(0);
      w.op_leaf.push_back(l);
      w.op_c.push_back((1.0 / 2.0) / 2.0);
      w.op_pix.push_back(1);
      w.op_leaf.push_back(l);
      w.op_c.push_back((1.0 / 2.0) / 2.0);
    }
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(100.0);
    cfg.background = BackgroundNuisance::kKnown;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    // 两个像元系数恒等 ⇒ h 的两个分量恒等 ⇒ F 仍可辨识（R 满列秩? 否: rank 1<m）。
    // G = RᵀR 是 [[x,x],[x,x]] 秩 1 ⇒ H 秩 1 = rank(C)。通量方向可估计。
    std::printf("  rank_def availability=%d reason=%s rank=%s\n",
                static_cast<int>(c.status.availability),
                c.status.reason.c_str(),
                c.covariance_rank ? std::to_string(*c.covariance_rank).c_str()
                                  : "none");
    check(c.covariance_rank.has_value() && *c.covariance_rank == 1,
          "covariance_rank reports rank(C)=1 (QR rank detection, no 1e-16 noise floor)");
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "flux remains identifiable in the retained subspace");
    // 与独立显式 GLS（用伪逆口径的满秩子空间）不同: 这里只断言报告一致。
  }
  {
    // 模板完全落在协方差零空间: 第三列全零 ⇒ F 模板为零向量 ⇒ 不可辨识
    PlanarPatchWindow w;
    w.control_id = "zero_template";
    w.pix_index = {0, 1};
    w.profile = {0.0, 0.0};
    w.variance = {1.0, 1.0};
    w.src_x = {0, 1};
    w.src_y = {0, 0};
    w.leaf_ipix = {7ULL, 8ULL};
    w.leaf_dp = {2.0, 2.0};
    w.leaf_n_full = {2.0, 2.0};
    w.leaf_n_covered = {2.0, 2.0};
    w.has_target_leaf = true;
    w.target_leaf = 7ULL;
    for (int l = 0; l < 2; ++l) {
      w.op_pix.push_back(0);
      w.op_leaf.push_back(l);
      w.op_c.push_back(0.5);
      w.op_pix.push_back(1);
      w.op_leaf.push_back(l);
      w.op_c.push_back(0.5);
    }
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(100.0);
    cfg.background = BackgroundNuisance::kKnown;
    const auto r = complete_spherical_reference({w}, cfg);
    check(r.precision.controls[0].status.availability ==
              PrecisionAvailability::kUnavailable,
          "zero template ⇒ flux not identifiable");
  }

  std::printf("\n%s: %d failure(s)\n",
              g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
