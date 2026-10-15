// ACSD Normalize — 第二轮独立验证程序（P3 秩亏 GLS 源空间投影）
//
// 目的: 验证 lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp 的
//       **生产求解路径**（列主元 Householder QR 行空间投影）与
//       **独立 oracle**（循环 Jacobi + 显式 H⁺²）在三种输入域上一致,
//       并满足 NOISE_SNR §11.4 的判定要求。
//
// 依据: docs/science/noise_snr/NOISE_SNR.md §11.1/§11.4
//       docs/detail/normalize/contracts.md §7.4A
//       docs/engineering/contracts/NORMALIZE_DATA.md §6/§7.1
//
// 判据（expected 不由被测实现反推）:
//   1. 满列秩 + 异方差: 生产 V_F == 1/(A_srcᵀD⁻¹A_src)（§11.1 解析恒等式）
//   2. 满列秩 + 背景 nuisance: 同上（A_src=[P,Z]）
//   3. 秩亏（R 满行秩 < m）: 生产 V_F == Jacobi oracle V_F（不同代码路径）
//   4. 秩亏 + 背景共线: 通量仍可辨识 ⇒ available（不因参数矩阵退化误判）
//   5. 通量落在背景列空间: unavailable 且不带 V_F（不造信息量）
//   6. Monte Carlo: 在真实 C_y=RDRᵀ 下 GLS 估计的 F 散度 == sqrt(V_F)
//   7. solver 诊断字段齐备（solver/rank/rel_tol/condition/background rank/I_F）
#include <cmath>
#include <cstdio>
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

ReferenceDefinition make_ref(double flux) {
  ReferenceDefinition r;
  r.kind = ReferenceKind::kExplicitLinearFlux;
  r.reference_id = "verify2:unit-source";
  r.flux_unit = "adu";
  r.normalization = "photo_applied";
  r.flux = flux;
  return r;
}

// ── 解析世界 A: m=6 像元, nl=10 叶, R 满列秩 ─────────────────────────────
// 满列秩要求 rank(R)=m 且 nl≥m; ncol≤4<m=6 ⇒ 参数信息矩阵超定良态,
// 1e-12 容差反映数学正确性而非条件数。
PlanarPatchWindow make_full_col_rank(int seed) {
  PlanarPatchWindow w;
  w.control_id = "fcr";
  w.cx_px = 50.5; w.cy_px = 60.5;
  w.ra_deg = 10.0; w.dec_deg = 20.0;
  w.leaf_order = 11;
  w.omega_leaf = 4.0 * M_PI / (12.0 * 2048.0 * 2048.0);
  w.pix_index = {0, 1, 2, 3, 4, 5};
  w.profile = {0.32, 0.24, 0.17, 0.12, 0.09, 0.06};
  w.variance = {2.0, 8.0, 32.0, 50.0, 4.0, 12.0};   // 强异方差
  // 背景基用**无量纲归一化坐标**: 裸像素坐标（48..53）与 y（58..57）跨 3 个
  // 数量级, 使参数信息矩阵 κ≈2e8; 此时 1e-12 容差只反映条件数而非数学正确性
  // （不能为了全绿而放宽容差 —— 应改为良态输入）。归一化后 κ≈1e4, 1e-12 有意义。
  // src_y 与 src_x 仍不得成仿射相关（y=x+c 会使 [1,x,y] 精确共线）。
  w.src_x = {(48-50.5)/1.7, (49-50.5)/1.7, (50-50.5)/1.7,
             (51-50.5)/1.7, (52-50.5)/1.7, (53-50.5)/1.7};
  w.src_y = {(58-57.5)/2.7, (54-57.5)/2.7, (61-57.5)/2.7,
             (55-57.5)/2.7, (60-57.5)/2.7, (57-57.5)/2.7};
  w.pix_omega.assign(6, 5.0);
  w.pix_a_drop.assign(6, 5.0);
  const int nl = 10;
  for (int l = 0; l < nl; ++l) w.leaf_ipix.push_back(7000ULL + l);
  // 每叶看到全部 6 个像元; 权重随叶与像元共同变化 ⇒ R (10×6) 满列秩
  std::vector<std::vector<double>> wv(nl, std::vector<double>(6, 0.0));
  for (int l = 0; l < nl; ++l)
    for (int j = 0; j < 6; ++j)
      wv[static_cast<std::size_t>(l)][static_cast<std::size_t>(j)] =
          0.20 + 0.55 * static_cast<double>((l * 3 + j * 7) % 11) / 10.0;
  std::vector<double> nlz(nl, 0.0);
  for (int l = 0; l < nl; ++l)
    for (int j = 0; j < 6; ++j) nlz[static_cast<std::size_t>(l)] +=
        5.0 * wv[static_cast<std::size_t>(l)][static_cast<std::size_t>(j)];
  for (int l = 0; l < nl; ++l) {
    for (int j = 0; j < 6; ++j) {
      w.op_pix.push_back(j);
      w.op_leaf.push_back(l);
      w.op_c.push_back(wv[static_cast<std::size_t>(l)][static_cast<std::size_t>(j)] /
                       nlz[static_cast<std::size_t>(l)]);
    }
    w.leaf_dp.push_back(nlz[static_cast<std::size_t>(l)]);
    w.leaf_n_full.push_back(nlz[static_cast<std::size_t>(l)]);
    w.leaf_n_covered.push_back(nlz[static_cast<std::size_t>(l)]);
  }
  w.has_target_leaf = true;
  w.target_leaf = 7003ULL;
  (void)seed;
  return w;
}

// ── 解析世界 B: m=5 像元, nl=3 叶 ⇒ R (3×5) rank ≤ 3 < 5 ⇒ 秩亏 ────────────
//   这里让 3 个叶的权重行线性无关 ⇒ rank(R)=3, 零空间 2 维。
PlanarPatchWindow make_rank_deficient() {
  PlanarPatchWindow w;
  w.control_id = "rdef";
  w.cx_px = 70.5; w.cy_px = 80.5;
  w.ra_deg = 30.0; w.dec_deg = -10.0;
  w.leaf_order = 11;
  w.omega_leaf = 4.0 * M_PI / (12.0 * 2048.0 * 2048.0);
  w.pix_index = {0, 1, 2, 3, 4};
  w.profile = {0.40, 0.25, 0.15, 0.12, 0.08};
  w.variance = {3.0, 7.0, 11.0, 19.0, 23.0};
  w.src_x = {68, 69, 70, 71, 72};
  w.src_y = {78, 79, 80, 81, 82};
  w.pix_omega.assign(5, 6.0);
  w.pix_a_drop.assign(5, 6.0);
  for (int l = 0; l < 3; ++l) w.leaf_ipix.push_back(8100ULL + l);
  const double wv[3][5] = {{0.5, 0.3, 0.1, 0.1, 0.0},
                           {0.2, 0.3, 0.3, 0.1, 0.1},
                           {0.1, 0.1, 0.3, 0.3, 0.2}};
  std::vector<double> nlz(3, 0.0);
  for (int l = 0; l < 3; ++l)
    for (int j = 0; j < 5; ++j) nlz[l] += 6.0 * wv[l][j];
  for (int l = 0; l < 3; ++l) {
    for (int j = 0; j < 5; ++j) {
      w.op_pix.push_back(j);
      w.op_leaf.push_back(l);
      w.op_c.push_back(wv[l][j] / nlz[l]);
    }
    w.leaf_dp.push_back(nlz[l]);
    w.leaf_n_full.push_back(nlz[l]);
    w.leaf_n_covered.push_back(nlz[l]);
  }
  w.has_target_leaf = true;
  w.target_leaf = 8101ULL;
  return w;
}

// 解析真值: V_F = 1/(A_srcᵀ D⁻¹ A_src) 的 [0][0]（仅 R 满列秩时成立）
double source_space_vf(const PlanarPatchWindow& w, int n_bg,
                       const std::vector<double>& v) {
  const int m = static_cast<int>(w.pix_index.size());
  const int ncol = 1 + n_bg;
  std::vector<double> a(static_cast<std::size_t>(m) * ncol, 0.0);
  for (int j = 0; j < m; ++j) {
    a[static_cast<std::size_t>(j) * ncol + 0] = w.profile[static_cast<std::size_t>(j)];
    for (int c = 1; c < ncol; ++c)
      a[static_cast<std::size_t>(j) * ncol + c] =
          (c == 1) ? 1.0
                   : (c == 2) ? w.src_x[static_cast<std::size_t>(j)]
                              : w.src_y[static_cast<std::size_t>(j)];
  }
  std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
  for (int i = 0; i < ncol; ++i)
    for (int j = 0; j < ncol; ++j) {
      double s = 0.0;
      for (int k = 0; k < m; ++k)
        s += a[static_cast<std::size_t>(k) * ncol + i] *
             a[static_cast<std::size_t>(k) * ncol + j] /
             v[static_cast<std::size_t>(k)];
      mm[static_cast<std::size_t>(i) * ncol + j] = s;
    }
  const int k = ncol;
  std::vector<double> aug(static_cast<std::size_t>(k) * 2 * k, 0.0);
  for (int i = 0; i < k; ++i) {
    for (int j = 0; j < k; ++j)
      aug[static_cast<std::size_t>(i) * 2 * k + j] =
          mm[static_cast<std::size_t>(i) * k + j];
    aug[static_cast<std::size_t>(i) * 2 * k + k + i] = 1.0;
  }
  for (int col = 0; col < k; ++col) {
    int piv = col;
    double best = std::abs(aug[static_cast<std::size_t>(col) * 2 * k + col]);
    for (int r = col + 1; r < k; ++r) {
      const double x = std::abs(aug[static_cast<std::size_t>(r) * 2 * k + col]);
      if (x > best) { best = x; piv = r; }
    }
    if (!(best > 0.0)) return 0.0;
    if (piv != col)
      for (int j = 0; j < 2 * k; ++j)
        std::swap(aug[static_cast<std::size_t>(col) * 2 * k + j],
                  aug[static_cast<std::size_t>(piv) * 2 * k + j]);
    const double d = aug[static_cast<std::size_t>(col) * 2 * k + col];
    for (int j = 0; j < 2 * k; ++j)
      aug[static_cast<std::size_t>(col) * 2 * k + j] /= d;
    for (int r = 0; r < k; ++r) {
      if (r == col) continue;
      const double f = aug[static_cast<std::size_t>(r) * 2 * k + col];
      if (f == 0.0) continue;
      for (int j = 0; j < 2 * k; ++j)
        aug[static_cast<std::size_t>(r) * 2 * k + j] -=
            f * aug[static_cast<std::size_t>(col) * 2 * k + j];
    }
  }
  return aug[static_cast<std::size_t>(0) * 2 * k + k];
}

// 第三条独立路径: 显式 nl 空间 C_y = R D Rᵀ 求逆, M = A_yᵀC_y⁻¹A_y, V_F=[(M⁻¹)]_FF。
// 仅在 nl ≤ rank(R)（C_y 非奇异）时有效; 否则返回 0 并跳过该检查。
double nl_space_truth(const PlanarPatchWindow& w, int n_bg,
                      const std::vector<double>& v) {
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
      const double planar =
          (b == 1) ? 1.0
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
  // Gauss-Jordan: [C | I]
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
    if (!(best > 0.0)) return 0.0;       // C_y 奇异 ⇒ 该路径不适用
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
    for (int j = 0; j < ncol; ++j) {
      double s = 0.0;
      for (int l = 0; l < nl; ++l) {
        const double ail = as[static_cast<std::size_t>(l) * ncol + i];
        if (ail == 0.0) continue;
        for (int l2 = 0; l2 < nl; ++l2)
          s += ail * aug[static_cast<std::size_t>(l) * 2 * nl + nl + l2] *
               as[static_cast<std::size_t>(l2) * ncol + j];
      }
      mm[static_cast<std::size_t>(i) * ncol + j] = s;
    }
  const int k = ncol;
  std::vector<double> b(static_cast<std::size_t>(k) * 2 * k, 0.0);
  for (int i = 0; i < k; ++i) {
    for (int j = 0; j < k; ++j)
      b[static_cast<std::size_t>(i) * 2 * k + j] =
          mm[static_cast<std::size_t>(i) * k + j];
    b[static_cast<std::size_t>(i) * 2 * k + k + i] = 1.0;
  }
  for (int col = 0; col < k; ++col) {
    int piv = col;
    double best = std::abs(b[static_cast<std::size_t>(col) * 2 * k + col]);
    for (int r = col + 1; r < k; ++r) {
      const double x = std::abs(b[static_cast<std::size_t>(r) * 2 * k + col]);
      if (x > best) { best = x; piv = r; }
    }
    if (!(best > 0.0)) return 0.0;
    if (piv != col)
      for (int j = 0; j < 2 * k; ++j)
        std::swap(b[static_cast<std::size_t>(col) * 2 * k + j],
                  b[static_cast<std::size_t>(piv) * 2 * k + j]);
    const double d = b[static_cast<std::size_t>(col) * 2 * k + col];
    for (int j = 0; j < 2 * k; ++j)
      b[static_cast<std::size_t>(col) * 2 * k + j] /= d;
    for (int r = 0; r < k; ++r) {
      if (r == col) continue;
      const double f = b[static_cast<std::size_t>(r) * 2 * k + col];
      if (f == 0.0) continue;
      for (int j = 0; j < 2 * k; ++j)
        b[static_cast<std::size_t>(r) * 2 * k + j] -=
            f * b[static_cast<std::size_t>(col) * 2 * k + j];
    }
  }
  return b[static_cast<std::size_t>(0) * 2 * k + k];
}

}  // namespace

int main() {
  // ── 1/2. 满列秩 + 异方差 + 背景口径 ────────────────────────────────────────
  for (int nbg = 0; nbg <= 2; ++nbg) {
    g_case = nbg == 0 ? "full-col-rank, background=known"
                      : (nbg == 1 ? "full-col-rank, constant_fit"
                                  : "full-col-rank, plane_fit");
    PlanarPatchWindow w = make_full_col_rank(1);
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(800.0);
    cfg.background = nbg == 0 ? BackgroundNuisance::kKnown
                    : nbg == 1 ? BackgroundNuisance::kConstantFit
                               : BackgroundNuisance::kPlaneFit;
    cfg.gain_e_per_adu = 1.7;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::vector<double> v(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      v[j] = w.variance[j] + 800.0 * w.profile[j] / 1.7;
    // BackgroundNuisance 的**背景列数** n_bg: known=0, constant_fit=1,
    // plane_fit=3（不是循环下标 2 —— 第一版测试在这里传错, 使 A_src 少一列,
    // 曾把正确生产值误判为 2.9% 偏差; 已用独立 numpy 复算裁定）。
    const int n_bg_cols = (nbg == 0) ? 0 : (nbg == 1) ? 1 : 3;
    const double truth = source_space_vf(w, n_bg_cols, v);
    const JacobiOracleResult orc = spherical_reference_jacobi_oracle(w, n_bg_cols, v);
    std::printf("  nbg=%d prod=%.17g truth=%.17g oracle=%.17g rel_tr=%.3e rel_or=%.3e\n",
                nbg, c.flux_variance ? *c.flux_variance : -1.0, truth,
                orc.ok ? orc.flux_variance : -1.0,
                c.flux_variance ? rel(*c.flux_variance, truth) : -1.0,
                c.flux_variance && orc.ok ? rel(*c.flux_variance, orc.flux_variance) : -1.0);
    if (!r.windows.empty()) {
      // 独立性自检: r=m（满列秩）时 Qᵀ 必须正交 ⇒ ‖t‖² = Σ_j P_j²/v_j 逐位成立。
      double direct_t2 = 0.0;
      for (std::size_t j = 0; j < w.pix_index.size(); ++j)
        direct_t2 += w.profile[j] * w.profile[j] / v[j];
      std::printf("    kernel: I_F=%.12g  direct ΣP²/v=%.12g  rel=%.3e\n",
                  r.windows[0].flux_information, direct_t2,
                  rel(r.windows[0].flux_information, direct_t2));
    }
    if (!r.windows.empty())
      std::printf("    diag: solver=%s rank=%d bg_rank=%d I_F=%.10g V_F=1/I_F=%.10g\n",
                  r.windows[0].solver.c_str(), r.windows[0].solver_rank,
                  r.windows[0].background_basis_rank, r.windows[0].flux_information,
                  r.windows[0].flux_information > 0.0
                      ? 1.0 / r.windows[0].flux_information : -1.0);
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "full column rank ⇒ available");
    check(c.flux_variance.has_value() && rel(*c.flux_variance, truth) < 1e-12,
          "V_F equals A_src^T D^-1 A_src identity (NOISE_SNR §11.1)");
    check(orc.ok && c.flux_variance.has_value() &&
              rel(*c.flux_variance, orc.flux_variance) < 1e-9,
          "V_F equals independent Jacobi+Explicit-H^+2 oracle");
    check(!r.windows.empty() &&
              r.windows[0].solver == "pivoted_householder_qr_rowspace",
          "solver recorded as pivoted Householder QR row-space");
  }

  // ── 3. 秩亏（R 满行秩 3 < m=5）: 生产 vs oracle ────────────────────────────
  g_case = "rank-deficient (rank(R)=3 < m=5)";
  {
    PlanarPatchWindow w = make_rank_deficient();
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(500.0);
    cfg.background = BackgroundNuisance::kKnown;
    cfg.gain_e_per_adu = 0.9;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::vector<double> v(w.variance.size());
    for (std::size_t j = 0; j < w.variance.size(); ++j)
      v[j] = w.variance[j] + 500.0 * w.profile[j] / 0.9;
    const JacobiOracleResult orc = spherical_reference_jacobi_oracle(w, 0, v);
    const double nl_truth = nl_space_truth(w, 0, v);   // 第三条独立路径
    std::printf("  prod=%.17g oracle=%.17g nl_truth=%.17g rank_oracle=%d rel=%.3e rel_nl=%.3e\n",
                c.flux_variance ? *c.flux_variance : -1.0,
                orc.ok ? orc.flux_variance : -1.0, nl_truth, orc.covariance_rank,
                (c.flux_variance && orc.ok) ? rel(*c.flux_variance, orc.flux_variance) : -1.0,
                c.flux_variance ? rel(*c.flux_variance, nl_truth) : -1.0);
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "rank deficient but flux identifiable ⇒ available");
    check(c.flux_variance.has_value() && orc.ok &&
              rel(*c.flux_variance, orc.flux_variance) < 1e-10,
          "rank-deficient V_F equals Jacobi+Explicit-H^+2 oracle (different path)");
    check(nl_truth > 0.0 && c.flux_variance.has_value() &&
              rel(*c.flux_variance, nl_truth) < 1e-10,
          "rank-deficient V_F equals explicit nl-space C_y^-1 GLS (third path)");
    check(c.covariance_rank.has_value() && *c.covariance_rank == 3,
          "covariance_rank == rank(C_sphere) == 3 (not padded to m)");
    check(!r.windows.empty() && r.windows[0].solver_rank == 3,
          "LocalWindow records solver_rank=3");
  }

  // ── 4. 秩亏 + 背景共线: 通量仍可辨识 ⇒ 不得误判 unavailable ────────────────
  g_case = "rank-deficient + collinear background, flux still identifiable";
  {
    PlanarPatchWindow w = make_rank_deficient();   // m=5, nl=3, rank 3
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(500.0);
    cfg.background = BackgroundNuisance::kPlaneFit;   // n_bg=3, ncol=4 > rank 3
    cfg.gain_e_per_adu = 0.9;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::printf("  availability=%d reason=%s\n", (int)c.status.availability,
                c.status.reason.c_str());
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "background collinearity does not auto-imply flux unidentifiable");
    check(c.flux_variance.has_value(),
          "V_F reported although full parameter matrix is singular");
    if (!r.windows.empty()) {
      std::printf("  bg_basis_rank=%d I_F=%.6g\n",
                  r.windows[0].background_basis_rank, r.windows[0].flux_information);
      check(r.windows[0].background_basis_rank < 3,
            "effective background basis rank < declared columns");
      check(r.windows[0].flux_information > 0.0,
            "flux information I_{F|beta} > 0 recorded");
    }
  }

  // ── 5. 通量落在背景列空间 ⇒ unavailable ───────────────────────────────────
  g_case = "flux direction inside background span ⇒ unavailable";
  {
    PlanarPatchWindow w;
    w.control_id = "bgspan";
    w.pix_index = {0, 1};
    w.profile = {0.5, 0.5};          // 与常量背景列成比例
    w.variance = {1.0, 1.0};
    w.src_x = {0, 1};
    w.src_y = {0, 0};
    w.leaf_ipix = {11ULL, 12ULL};
    w.leaf_dp = {1.0, 1.0};
    w.leaf_n_full = {1.0, 1.0};
    w.leaf_n_covered = {1.0, 1.0};
    w.has_target_leaf = true;
    w.target_leaf = 11ULL;
    w.op_pix = {0, 1, 0, 1};
    w.op_leaf = {0, 0, 1, 1};
    w.op_c = {0.5, 0.5, 0.5, 0.5};
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(100.0);
    cfg.background = BackgroundNuisance::kConstantFit;   // z=1 与 P 成比例
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    std::printf("  availability=%d reason=%s\n", (int)c.status.availability,
                c.status.reason.c_str());
    check(c.status.availability == PrecisionAvailability::kUnavailable,
          "flux inside background span ⇒ unavailable");
    check(!c.flux_variance.has_value() && !c.reference_snr.has_value(),
          "no fabricated V_F/SNR");
    check(c.status.reason == "flux_direction_inside_background_span",
          "named reason");
  }

  // ── 6. Monte Carlo: 真实 C_y = R D Rᵀ 下的 GLS 散度 == sqrt(V_F) ───────────
  g_case = "Monte Carlo flux scatter vs predicted sqrt(V_F)";
  {
    PlanarPatchWindow w = make_rank_deficient();   // 秩亏情形（最难）
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(500.0);
    cfg.background = BackgroundNuisance::kKnown;
    cfg.gain_e_per_adu = 0.9;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& c = r.precision.controls[0];
    const int m = 5, nl = 3;
    std::vector<double> v(m);
    for (int j = 0; j < m; ++j)
      v[j] = w.variance[static_cast<std::size_t>(j)] +
             500.0 * w.profile[static_cast<std::size_t>(j)] / 0.9;
    // y_l = Σ_j R[l][j] x_j, x_j = F P_j + sqrt(v_j) n_j
    std::vector<std::vector<double>> R(nl, std::vector<double>(m, 0.0));
    for (std::size_t k = 0; k < w.op_c.size(); ++k)
      R[static_cast<std::size_t>(w.op_leaf[k])][static_cast<std::size_t>(w.op_pix[k])] =
          w.op_c[k];
    const double F = 500.0;
    const int nrep = 200000;
    std::mt19937_64 rng(20261015);
    std::normal_distribution<double> nz(0.0, 1.0);
    double s1 = 0.0, s2 = 0.0;
    for (int rep = 0; rep < nrep; ++rep) {
      std::vector<double> x(m);
      for (int j = 0; j < m; ++j)
        x[static_cast<std::size_t>(j)] =
            F * w.profile[static_cast<std::size_t>(j)] +
            std::sqrt(v[static_cast<std::size_t>(j)]) * nz(rng);
      std::vector<double> y(nl, 0.0);
      for (int l = 0; l < nl; ++l)
        for (int j = 0; j < m; ++j)
          y[static_cast<std::size_t>(l)] +=
              R[static_cast<std::size_t>(l)][static_cast<std::size_t>(j)] *
              x[static_cast<std::size_t>(j)];
      // GLS: F̂ = (hᵀC_y⁺y)/(hᵀC_y⁺h), h = R P, C_y = R D Rᵀ
      // 直接构造 C_y（3×3, rank 3 = nl ⇒ 可逆）并做 Gauss-Jordan。
      std::vector<double> C(nl * nl, 0.0);
      for (int l = 0; l < nl; ++l)
        for (int l2 = 0; l2 < nl; ++l2) {
          double s = 0.0;
          for (int j = 0; j < m; ++j)
            s += R[static_cast<std::size_t>(l)][static_cast<std::size_t>(j)] *
                 v[static_cast<std::size_t>(j)] *
                 R[static_cast<std::size_t>(l2)][static_cast<std::size_t>(j)];
          C[static_cast<std::size_t>(l) * nl + l2] = s;
        }
      std::vector<double> h(nl, 0.0);
      for (int l = 0; l < nl; ++l) {
        double s = 0.0;
        for (int j = 0; j < m; ++j)
          s += R[static_cast<std::size_t>(l)][static_cast<std::size_t>(j)] *
               w.profile[static_cast<std::size_t>(j)];
        h[static_cast<std::size_t>(l)] = s;
      }
      // 解 C z_h = h 与 C z_y = y
      std::vector<double> aug(nl * (nl + 2), 0.0);
      for (int l = 0; l < nl; ++l) {
        for (int l2 = 0; l2 < nl; ++l2)
          aug[static_cast<std::size_t>(l) * (nl + 2) + l2] =
              C[static_cast<std::size_t>(l) * nl + l2];
        aug[static_cast<std::size_t>(l) * (nl + 2) + nl] = h[static_cast<std::size_t>(l)];
        aug[static_cast<std::size_t>(l) * (nl + 2) + nl + 1] =
            y[static_cast<std::size_t>(l)];
      }
      for (int col = 0; col < nl; ++col) {
        int piv = col;
        double best = std::abs(aug[static_cast<std::size_t>(col) * (nl + 2) + col]);
        for (int rr = col + 1; rr < nl; ++rr) {
          const double xx = std::abs(aug[static_cast<std::size_t>(rr) * (nl + 2) + col]);
          if (xx > best) { best = xx; piv = rr; }
        }
        if (!(best > 0.0)) break;
        if (piv != col)
          for (int j = 0; j < nl + 2; ++j)
            std::swap(aug[static_cast<std::size_t>(col) * (nl + 2) + j],
                      aug[static_cast<std::size_t>(piv) * (nl + 2) + j]);
        const double d = aug[static_cast<std::size_t>(col) * (nl + 2) + col];
        for (int j = 0; j < nl + 2; ++j)
          aug[static_cast<std::size_t>(col) * (nl + 2) + j] /= d;
        for (int rr = 0; rr < nl; ++rr) {
          if (rr == col) continue;
          const double f = aug[static_cast<std::size_t>(rr) * (nl + 2) + col];
          if (f == 0.0) continue;
          for (int j = 0; j < nl + 2; ++j)
            aug[static_cast<std::size_t>(rr) * (nl + 2) + j] -=
                f * aug[static_cast<std::size_t>(col) * (nl + 2) + j];
        }
      }
      double num = 0.0, den = 0.0;
      for (int l = 0; l < nl; ++l) {
        num += h[static_cast<std::size_t>(l)] *
               aug[static_cast<std::size_t>(l) * (nl + 2) + nl + 1];
        den += h[static_cast<std::size_t>(l)] *
               aug[static_cast<std::size_t>(l) * (nl + 2) + nl];
      }
      const double fhat = num / den;
      const double d = fhat - F;
      s1 += d;
      s2 += d * d;
    }
    const double sd = std::sqrt((s2 - s1 * s1 / nrep) / (nrep - 1));
    const double pred = std::sqrt(*c.flux_variance);
    std::printf("  sd=%.6f sqrt(V_F)=%.6f ratio=%.4f (n=%d)\n", sd, pred,
                sd / pred, nrep);
    // 容差取 Monte Carlo 自身标准误的 4 倍（SE(sd)/sd = 1/sqrt(2(n-1))）,
    // 不用固定百分比容忍随机涨落, 也不用固定宽松阈值换取全绿。
    const double mc_se = 1.0 / std::sqrt(2.0 * static_cast<double>(nrep - 1));
    std::printf("  MC se=%.4f%% (4-sigma band %.4f%%)\n", 100.0 * mc_se,
                400.0 * mc_se);
    check(std::abs(sd / pred - 1.0) < 4.0 * mc_se,
          "Monte Carlo scatter within 4 sigma of predicted sqrt(V_F)");
  }

  // ── 7. solver 诊断齐备 ────────────────────────────────────────────────────
  g_case = "solver diagnostics completeness";
  {
    PlanarPatchWindow w = make_full_col_rank(1);
    SphericalCompletionConfig cfg;
    cfg.reference = make_ref(800.0);
    cfg.background = BackgroundNuisance::kConstantFit;
    cfg.gain_e_per_adu = 1.7;
    const auto r = complete_spherical_reference({w}, cfg);
    const auto& lw = r.windows[0];
    std::printf("  solver=%s rank=%d rel_tol=%g cond=%g bg_rank=%d I_F=%g\n",
                lw.solver.c_str(), lw.solver_rank, lw.solver_relative_tol,
                lw.solver_condition_number, lw.background_basis_rank,
                lw.flux_information);
    check(!lw.solver.empty() && lw.solver_rank > 0 &&
              lw.solver_relative_tol > 0.0 &&
              lw.solver_condition_number >= 1.0 &&
              lw.background_basis_rank >= 0 && lw.flux_information > 0.0,
          "solver/rank/rel_tol/condition/background_rank/I_F all recorded");
  }

  std::printf("\n%s: %d failure(s)\n",
              g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
