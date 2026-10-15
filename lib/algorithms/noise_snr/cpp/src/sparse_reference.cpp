// ACSD Normalize — 球面参考精度 GLS 完成实现（数学在 noise 库, 单份实现）
//
// 依据: docs/science/noise_snr/NOISE_SNR.md §3/§11, NORMALIZE_DATA.md §6,
//       NORMALIZE_SCIENCE.md §7/§12。公式:
//   y = F h + Z β + n,  C 为参考源通量条件下冻结的条件协方差
//   V_F = [(A^T C^{-1} A)^{-1}]_{FF},  A = [h, Z]
//   h = R P, C = R C_planar R^T, Z = R Z_planar  (R = 同一次 Drizzle 的实际算子)
//   SNR_ref,k(u) = F_ref,k / sqrt(V_F,k(u))
// 秩亏按可估计子空间处理: 显式秩判定 (Jacobi 特征分解), 报告秩与条件数;
// 参数不可辨识 ⇒ unavailable, **不盲加 epsilon、不用伪逆制造信息量**。
#include "acsd/sparse_reference.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

namespace acsd::normalize {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

const char* kAccuracyTokens[] = {"conditional_complete", "approximate",
                                 "upper_bound", "not_applicable"};


// 稠密对称正定 Cholesky (A = L L^T, 下三角存入 a)。返回 true = 成功;
// 失败（非正定/秩亏）⇒ false（调用方按秩亏处理, 不猜）。
bool cholesky_spd(std::vector<double>& a, int n) {
  // 相对秩判定: 对角必须 > tol·max|a_ii|。纯 d>0 会把数值零（相近行/列造成
  // 的 ~1e-34 残差）当成"正定 ⇒ 满秩", 从而让秩亏系统给出伪造的高信息量。
  double anorm = 0.0;
  for (int j = 0; j < n; ++j)
    anorm = std::max(anorm, std::abs(a[static_cast<std::size_t>(j) * n + j]));
  const double tol = 1e-12 * (anorm > 0.0 ? anorm : 1.0);
  for (int j = 0; j < n; ++j) {
    double d = a[static_cast<std::size_t>(j) * n + j];
    for (int k = 0; k < j; ++k) {
      const double ljk = a[static_cast<std::size_t>(j) * n + k];
      d -= ljk * ljk;
    }
    if (!(d > tol) || !std::isfinite(d)) return false;
    const double ljj = std::sqrt(d);
    a[static_cast<std::size_t>(j) * n + j] = ljj;
    for (int i = j + 1; i < n; ++i) {
      double sum = a[static_cast<std::size_t>(i) * n + j];
      for (int k = 0; k < j; ++k) {
        sum -= a[static_cast<std::size_t>(i) * n + k] *
               a[static_cast<std::size_t>(j) * n + k];
      }
      a[static_cast<std::size_t>(i) * n + j] = sum / ljj;
    }
  }
  return true;
}

// 解 L y = b (前代) 与 L^T x = y (回代); L 下三角 n×n row-major, b/x 为 n 向量。
void cholesky_solve(const std::vector<double>& l, int n, const double* b,
                    double* x) {
  std::vector<double> y(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    double sum = b[i];
    for (int k = 0; k < i; ++k) sum -= l[static_cast<std::size_t>(i) * n + k] * y[static_cast<std::size_t>(k)];
    y[static_cast<std::size_t>(i)] = sum / l[static_cast<std::size_t>(i) * n + i];
  }
  for (int i = n - 1; i >= 0; --i) {
    double sum = y[static_cast<std::size_t>(i)];
    for (int k = i + 1; k < n; ++k) sum -= l[static_cast<std::size_t>(k) * n + i] * x[static_cast<std::size_t>(k)];
    x[static_cast<std::size_t>(i)] = sum / l[static_cast<std::size_t>(i) * n + i];
  }
}

// 对称矩阵 Jacobi 特征分解 (A = V diag(w) V^T), n 小 (<= 数百)。原地破坏 a。
void jacobi_eigh(std::vector<double> a, int n, std::vector<double>& w,
                 std::vector<double>& v) {
  w.assign(static_cast<std::size_t>(n), 0.0);
  v.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) v[static_cast<std::size_t>(i) * n + i] = 1.0;
  for (int sweep = 0; sweep < 64; ++sweep) {
    double off = 0.0;
    for (int p = 0; p < n; ++p) {
      for (int q = p + 1; q < n; ++q) {
        off += a[static_cast<std::size_t>(p) * n + q] *
               a[static_cast<std::size_t>(p) * n + q];
      }
    }
    if (off <= 1e-30) break;
    for (int p = 0; p < n; ++p) {
      for (int q = p + 1; q < n; ++q) {
        const std::size_t pq = static_cast<std::size_t>(p) * n + q;
        const double apq = a[pq];
        if (std::abs(apq) <= 1e-300) continue;
        const double app = a[static_cast<std::size_t>(p) * n + p];
        const double aqq = a[static_cast<std::size_t>(q) * n + q];
        const double theta = 0.5 * (aqq - app) / apq;
        const double t = (theta >= 0.0 ? 1.0 : -1.0) /
                         (std::abs(theta) + std::sqrt(theta * theta + 1.0));
        const double c = 1.0 / std::sqrt(t * t + 1.0);
        const double s = t * c;
        const double tau = s / (1.0 + c);
        a[static_cast<std::size_t>(p) * n + p] = app - t * apq;
        a[static_cast<std::size_t>(q) * n + q] = aqq + t * apq;
        a[pq] = 0.0;
        a[static_cast<std::size_t>(q) * n + p] = 0.0;   // 满矩阵: 两侧同时清零
        for (int r = 0; r < n; ++r) {
          if (r == p || r == q) continue;
          const std::size_t rp = static_cast<std::size_t>(r) * n + p;
          const std::size_t rq = static_cast<std::size_t>(r) * n + q;
          const double arp = a[rp];
          const double arq = a[rq];
          // 满对称矩阵: 列 p/q 与行 p/q 同时更新（保持对称; 只更新列会破坏
          // 对称性 ⇒ 特征向量错误）。
          const double nrp = arp - s * (arq + tau * arp);
          const double nrq = arq + s * (arp - tau * arq);
          a[rp] = nrp;
          a[rq] = nrq;
          a[static_cast<std::size_t>(p) * n + r] = nrp;
          a[static_cast<std::size_t>(q) * n + r] = nrq;
        }
        for (int r = 0; r < n; ++r) {
          const std::size_t rp = static_cast<std::size_t>(r) * n + p;
          const std::size_t rq = static_cast<std::size_t>(r) * n + q;
          const double vrp = v[rp];
          const double vrq = v[rq];
          v[rp] = vrp - s * (vrq + tau * vrp);
          v[rq] = vrq + s * (vrp - tau * vrq);
        }
      }
    }
  }
  for (int i = 0; i < n; ++i) w[static_cast<std::size_t>(i)] =
      a[static_cast<std::size_t>(i) * n + i];
  // 升序排列 (w, 对应列)
  std::vector<int> idx(static_cast<std::size_t>(n));
  std::iota(idx.begin(), idx.end(), 0);
  std::sort(idx.begin(), idx.end(),
            [&](int x, int y) { return w[static_cast<std::size_t>(x)] <
                                       w[static_cast<std::size_t>(y)]; });
  std::vector<double> w2(static_cast<std::size_t>(n));
  std::vector<double> v2 = v;
  for (int i = 0; i < n; ++i) {
    w2[static_cast<std::size_t>(i)] = w[static_cast<std::size_t>(idx[static_cast<std::size_t>(i)])];
    for (int r = 0; r < n; ++r) {
      v2[static_cast<std::size_t>(r) * n + i] =
          v[static_cast<std::size_t>(r) * n +
            idx[static_cast<std::size_t>(i)]];
    }
  }
  w = w2;
  v = v2;
}

// 在保留子空间内求 M 的逆并取 [0][0]; 不可逆 ⇒ false。
bool invert_ff_entry(const std::vector<double>& m, int n, double* out_vf,
                     int* out_rank, double* out_cond) {
  std::vector<double> w, v;
  jacobi_eigh(m, n, w, v);
  double wmax = 0.0;
  for (int i = 0; i < n; ++i) wmax = std::max(wmax, w[static_cast<std::size_t>(i)]);
  if (!(wmax > 0.0) || !std::isfinite(wmax)) return false;
  const double tol = 1e-12 * wmax;
  int rank = 0;
  double wmin_pos = 0.0;
  for (int i = 0; i < n; ++i) {
    if (w[static_cast<std::size_t>(i)] > tol) {
      ++rank;
      if (wmin_pos == 0.0) wmin_pos = w[static_cast<std::size_t>(i)];
    }
  }
  if (out_rank) *out_rank = rank;
  if (out_cond) *out_cond = wmin_pos > 0.0 ? wmax / wmin_pos : kNaN;
  if (rank != n) return false;  // 参数子空间内不可辨识 ⇒ 不用伪逆
  // M^{-1} = V diag(1/w) V^T; 取 [0][0]
  double s = 0.0;
  for (int i = 0; i < n; ++i) {
    const double vi0 = v[static_cast<std::size_t>(0) * n + i];
    s += vi0 * vi0 / w[static_cast<std::size_t>(i)];
  }
  if (!std::isfinite(s) || !(s > 0.0)) return false;
  *out_vf = s;
  return true;
}

}  // namespace

const char* to_string(LocalModelKind kind) {
  switch (kind) {
    case LocalModelKind::kPsf: return "psf";
    case LocalModelKind::kCovariance: return "covariance";
    case LocalModelKind::kDrizzleOperator: return "drizzle_operator";
    case LocalModelKind::kSupport: return "support";
    case LocalModelKind::kScale: return "scale";
    case LocalModelKind::kCalibrationUncertainty: return "calibration_uncertainty";
  }
  return "unknown";
}

const char* to_string(LocalModelDomain domain) {
  switch (domain) {
    case LocalModelDomain::kCalibratedPlanar: return "calibrated_planar";
    case LocalModelDomain::kPhotoAppliedPlanar: return "photo_applied_planar";
    case LocalModelDomain::kDrizzledSphere: return "drizzled_sphere";
  }
  return "unknown";
}

const char* to_string(ReferenceKind kind) {
  switch (kind) {
    case ReferenceKind::kSedResponse: return "sed_response";
    case ReferenceKind::kExplicitLinearFlux: return "explicit_linear_flux";
    case ReferenceKind::kUnavailable: return "unavailable";
  }
  return "unavailable";
}

const char* to_string(PrecisionAvailability v) {
  return v == PrecisionAvailability::kAvailable ? "available" : "unavailable";
}

const char* to_string(PrecisionAccuracy v) {
  return kAccuracyTokens[static_cast<int>(v)];
}

const char* to_string(CovarianceCondition v) {
  switch (v) {
    case CovarianceCondition::kReferenceSource: return "reference_source";
    case CovarianceCondition::kActualScene: return "actual_scene";
    case CovarianceCondition::kBackgroundOnly: return "background_only";
  }
  return "reference_source";
}

SphericalCompletionResult complete_spherical_reference(
    const std::vector<PlanarPatchWindow>& windows,
    const SphericalCompletionConfig& cfg) {
  SphericalCompletionResult out;
  out.precision.header.producer = "acsd.phase1.noise_snr.complete_spherical_reference";
  out.precision.domain = "drizzled_sphere";

  // 参考缺项 ⇒ 整个 precision unavailable（信号仍合法; 不发明中位 F0）。
  bool ref_ok = cfg.reference.kind == ReferenceKind::kExplicitLinearFlux &&
                cfg.reference.flux.has_value() &&
                std::isfinite(*cfg.reference.flux) && *cfg.reference.flux > 0.0 &&
                !cfg.reference.flux_unit.empty() && !cfg.reference.normalization.empty();
  if (cfg.reference.kind == ReferenceKind::kSedResponse) {
    // sed_response 需要可求值的响应积分; 未接线时显式 unavailable（不伪造绝对零点）。
    out.precision.status.availability = PrecisionAvailability::kUnavailable;
    out.precision.status.accuracy = PrecisionAccuracy::kNotApplicable;
    out.precision.status.reason =
        cfg.reference.reason.empty()
            ? "sed_response requires evaluable response integration; not wired"
            : cfg.reference.reason;
    for (const auto& w : windows) {
      ReferenceControl ctrl;
      ctrl.control_id = w.control_id;
      ctrl.source_x0 = w.cx_px;
      ctrl.source_y0 = w.cy_px;
      ctrl.status = out.precision.status;
      out.precision.controls.push_back(std::move(ctrl));
      out.failure_reasons.push_back(w.control_id + ": " +
                                    out.precision.status.reason);
    }
    return out;
  }
  if (!ref_ok) {
    out.precision.status.availability = PrecisionAvailability::kUnavailable;
    out.precision.status.accuracy = PrecisionAccuracy::kNotApplicable;
    out.precision.status.reason =
        cfg.reference.reason.empty() ? "no reference definition" : cfg.reference.reason;
    for (const auto& w : windows) {
      ReferenceControl ctrl;
      ctrl.control_id = w.control_id;
      ctrl.source_x0 = w.cx_px;
      ctrl.source_y0 = w.cy_px;
      ctrl.status = out.precision.status;
      out.precision.controls.push_back(std::move(ctrl));
      out.failure_reasons.push_back(w.control_id + ": " +
                                    out.precision.status.reason);
    }
    return out;
  }

  // 缺项登记（合同 §5: 只有确定被省略项是 PSD 随机噪声且模板/域固定时才可称
  // upper_bound; 有未定标增益/母版/模板误差 ⇒ approximate）。
  const bool gain_known = std::isfinite(cfg.gain_e_per_adu) && cfg.gain_e_per_adu > 0.0;
  std::vector<std::string> omitted;
  if (!gain_known) omitted.push_back("source_poisson");
  if (!cfg.master_uncertainty_modelled) omitted.push_back("master_uncertainty");
  PrecisionAccuracy accuracy = PrecisionAccuracy::kConditionalComplete;
  if (!gain_known) {
    accuracy = cfg.template_fixed ? PrecisionAccuracy::kUpperBound
                                  : PrecisionAccuracy::kApproximate;
  }
  if (!cfg.master_uncertainty_modelled &&
      accuracy == PrecisionAccuracy::kConditionalComplete) {
    accuracy = PrecisionAccuracy::kApproximate;
  }

  const double f_ref = *cfg.reference.flux;
  int n_bg = 0;
  switch (cfg.background) {
    case BackgroundNuisance::kKnown: n_bg = 0; break;
    case BackgroundNuisance::kConstantFit: n_bg = 1; break;
    case BackgroundNuisance::kPlaneFit: n_bg = 3; break;
  }
  const int ncol = 1 + n_bg;

  out.precision.status.availability = PrecisionAvailability::kAvailable;
  out.precision.status.accuracy = accuracy;
  out.precision.status.omitted_terms = omitted;

  std::uint64_t n_available = 0;
  for (std::size_t window_index = 0; window_index < windows.size();
       ++window_index) {
    const acsd::normalize::PlanarPatchWindow& w = windows[window_index];
    const std::size_t m = w.pix_index.size();
    const int nl = static_cast<int>(w.leaf_ipix.size());
    ReferenceControl ctrl;
    ctrl.control_id = w.control_id;
    ctrl.source_x0 = w.cx_px;
    ctrl.source_y0 = w.cy_px;
    ctrl.ra_deg = w.ra_deg;
    ctrl.dec_deg = w.dec_deg;
    ctrl.background_nuisance =
        cfg.background == BackgroundNuisance::kKnown
            ? "known"
            : (cfg.background == BackgroundNuisance::kConstantFit ? "constant_fit"
                                                                 : "plane_fit");

    bool failed = false;   // PrecisionStatus 默认即 unavailable; 用局部标志区分
                          // "尚未判定" 与 "已判定失败"（禁把默认态当初判失败）。
    auto fail_point = [&](const std::string& reason) {
      failed = true;
      ctrl.status.availability = PrecisionAvailability::kUnavailable;
      ctrl.status.accuracy = PrecisionAccuracy::kNotApplicable;
      ctrl.status.reason = reason;
      out.failure_reasons.push_back(w.control_id + ": " + reason);
    };

    if (m == 0 || nl == 0 || w.op_c.size() != w.op_pix.size() ||
        w.op_c.size() != w.op_leaf.size() || w.target_leaf == 0) {
      fail_point("empty_local_window");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    if (w.leaf_dp.size() != static_cast<std::size_t>(nl)) {
      fail_point("local_window_dimension_mismatch");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    if (w.profile.size() != m || w.variance.size() != m || w.src_x.size() != m ||
        w.src_y.size() != m) {
      fail_point("local_window_dimension_mismatch");
      out.precision.controls.push_back(ctrl);
      continue;
    }

    // 平面源泊松条件: v_j 已是 (调用方换算到数组标度的) blank-sky 方差;
    // 参考源泊松期望 F_ref·P_j/gain 在**原生** gain 下建模后随 D_alpha 变换进入
    // （调用方已按合同 §4 把 α² 乘入 v_j; 这里只加参考源项）。
    std::vector<double> v(m);
    double vmin = 0.0, vmax = 0.0;
    for (std::size_t j = 0; j < m; ++j) {
      double vj = w.variance[j];
      if (!std::isfinite(vj) || vj < 0.0) {
        fail_point("nonfinite_local_variance");
        vj = 0.0;
        break;
      }
      if (gain_known) {
        const double pj = std::max(0.0, w.profile[j]);
        vj += f_ref * pj / cfg.gain_e_per_adu;
      }
      if (!std::isfinite(vj)) {
        fail_point("nonfinite_local_variance");
        vj = 0.0;
        break;
      }
      v[j] = vj;
      if (j == 0) { vmin = vj; vmax = vj; }
      else { vmin = std::min(vmin, vj); vmax = std::max(vmax, vj); }
    }
    if (failed) {
      out.precision.controls.push_back(ctrl);
      continue;
    }
    if (!(vmin > 0.0)) {
      // 窗口内存在零方差像元 ⇒ 该方向信息无限/不可信; 不用地板制造信息量。
      fail_point("zero_variance_in_window");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    (void)vmax;

    // 模板在源空间的存在性（h = R P 的信息等价像; P=0 ⇒ 无模板）
    double pnorm = 0.0;
    for (double p : w.profile) pnorm += p * p;
    if (!(pnorm > 0.0) || !std::isfinite(pnorm)) {
      fail_point("template_outside_drizzle_support");
      out.precision.controls.push_back(ctrl);
      continue;
    }

    // ── 球面 GLS（精确; NORMALIZE_DATA §6 + NOISE_SNR §3）────────────────────
    // y = F·(R P) + Z_s·β + n, C = R V Rᵀ, V = diag(v_j)。欲求
    //   M = A_sᵀ C⁺ A_s, A_s = [R P, R Z]  (nl × ncol)
    // 直接做需要 nl×nl 求逆（nl 可达数千）。用满列秩恒等式（m ≤ nl 且 R 满列秩,
    // 几何上由捕获构造保证每像元/每叶都有贡献时成立）:
    //   (R V Rᵀ)⁺ = R̃ (R̃ᵀR̃)⁻² R̃ᵀ,  R̃ = R V^{1/2}
    //   ⇒ K := Rᵀ(RVRᵀ)⁺R = V^{1/2} G H⁻² G V^{1/2},  G = RᵀR, H = V^{1/2}GV^{1/2}
    //   ⇒ M = Bᵀ H⁻² B,  B = G V^{1/2} A_src,  A_src = [P, Z]
    // 其中 H ≻ 0 ⇒ Cholesky 解两次三角系统（O(m³/3); m 由 PSF 足迹窗口界定）。
    // 秩亏（Cholesky 失败）⇒ 参数不可辨识 ⇒ unavailable（合同 §6: 不用伪逆制造
    // 信息量, 不用任意地板）。该路径与显式 nl 空间 GLS 的数值一致性由
    // eng/tests/unit/sparse_reference_gls_test.cpp 的 dual 段对拍验证。
    // ── 球面 GLS（精确; NORMALIZE_DATA §6 + NOISE_SNR §3）────────────────────
    // y = F·(R P) + Z_s·β + n, C = R V Rᵀ, V = diag(v_j), A_s = [R P, R Z]。
    // 两条等价精确路径（按 m 与 nl 的大小选, 都只做一次稠密 Cholesky）:
    //   (a) m ≤ nl（R 满列秩）: (RVRᵀ)⁺ = R̃(R̃ᵀR̃)⁻²R̃ᵀ, R̃ = R V^{1/2}
    //       ⇒ M = Bᵀ H⁻² B, B = G V^{1/2} A_src, G = RᵀR, H = V^{1/2} G V^{1/2}
    //   (b) m > nl（R 满行秩）: C = R V Rᵀ (nl×nl) 直接 Cholesky,
    //       M = A_sᵀ C⁻¹ A_s
    // 秩亏（Cholesky 失败）⇒ 参数不可辨识 ⇒ unavailable（合同 §6: 不用伪逆制造
    // 信息量、不用任意地板）。与显式定义式 [(AᵀC⁻¹A)⁻¹]_FF 的数值一致性由
    // eng/tests/unit/sparse_reference_gls_test.cpp 的 dual 段对拍验证。
    std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
    bool gls_ok = false;
    if (m <= static_cast<std::size_t>(nl)) {
      // G = RᵀR (m×m): G[j][j'] = Σ_l c_lj·c_lj'（按叶聚合, O(nnz + Σk_l²)）
      std::vector<double> g(static_cast<std::size_t>(m) * m, 0.0);
      {
        std::vector<std::vector<std::pair<int, double>>> per_leaf(
            static_cast<std::size_t>(nl));
        for (std::size_t k = 0; k < w.op_c.size(); ++k) {
          const int l = w.op_leaf[k];
          const int j = w.op_pix[k];
          if (l < 0 || l >= nl || j < 0 || static_cast<std::size_t>(j) >= m) continue;
          per_leaf[static_cast<std::size_t>(l)].emplace_back(j, w.op_c[k]);
        }
        for (int l = 0; l < nl; ++l) {
          const auto& js = per_leaf[static_cast<std::size_t>(l)];
          for (std::size_t a = 0; a < js.size(); ++a) {
            const std::size_t ja = static_cast<std::size_t>(js[a].first);
            g[ja * m + ja] += js[a].second * js[a].second;
            for (std::size_t b = a + 1; b < js.size(); ++b) {
              const std::size_t jb = static_cast<std::size_t>(js[b].first);
              const double val = js[a].second * js[b].second;
              g[ja * m + jb] += val;
              g[jb * m + ja] += val;
            }
          }
        }
      }
      std::vector<double> hmat = g;   // H = V^{1/2} G V^{1/2}
      for (int j = 0; j < static_cast<int>(m); ++j) {
        const double sj = std::sqrt(v[static_cast<std::size_t>(j)]);
        for (int j2 = 0; j2 < static_cast<int>(m); ++j2) {
          const double s2 = std::sqrt(v[static_cast<std::size_t>(j2)]);
          hmat[static_cast<std::size_t>(j) * m + j2] *= sj * s2;
        }
      }
      if (cholesky_spd(hmat, static_cast<int>(m))) {
        // B = G V^{1/2} A_src, Y = H⁻¹B, M = YᵀY
        std::vector<double> ymat(static_cast<std::size_t>(m) * ncol, 0.0);
        for (int c = 0; c < ncol; ++c) {
          std::vector<double> bcol(static_cast<std::size_t>(m), 0.0);
          for (int j = 0; j < static_cast<int>(m); ++j) {
            // B[j][c] = Σ_{j2} G[j][j2]·V^{1/2}[j2]·A_src[j2][c]
            double sum = 0.0;
            for (int j2 = 0; j2 < static_cast<int>(m); ++j2) {
              const double s2 = std::sqrt(v[static_cast<std::size_t>(j2)]);
              const double colv = c == 0   ? w.profile[static_cast<std::size_t>(j2)]
                                 : c == 1 ? 1.0
                                 : c == 2 ? w.src_x[static_cast<std::size_t>(j2)]
                                          : w.src_y[static_cast<std::size_t>(j2)];
              sum += g[static_cast<std::size_t>(j) * m + j2] * s2 * colv;
            }
            bcol[static_cast<std::size_t>(j)] = sum;
          }
          std::vector<double> xcol(static_cast<std::size_t>(m), 0.0);
          cholesky_solve(hmat, static_cast<int>(m), bcol.data(), xcol.data());
          for (int j = 0; j < static_cast<int>(m); ++j) {
            ymat[static_cast<std::size_t>(j) * ncol + c] =
                xcol[static_cast<std::size_t>(j)];
          }
        }
        for (std::size_t j = 0; j < m; ++j) {
          for (int a = 0; a < ncol; ++a) {
            const double ya = ymat[j * ncol + a];
            if (ya == 0.0) continue;
            for (int b = a; b < ncol; ++b) {
              const double val = ya * ymat[j * ncol + b];
              mm[static_cast<std::size_t>(a) * ncol + b] += val;
              if (b != a) mm[static_cast<std::size_t>(b) * ncol + a] += val;
            }
          }
        }
        gls_ok = true;
      }
    } else {
      // C = R V Rᵀ (nl×nl) 直接路径
      std::vector<double> cov(static_cast<std::size_t>(nl) * nl, 0.0);
      {
        std::vector<std::vector<std::pair<int, double>>> per_leaf(
            static_cast<std::size_t>(nl));
        for (std::size_t k = 0; k < w.op_c.size(); ++k) {
          const int l = w.op_leaf[k];
          const int j = w.op_pix[k];
          if (l < 0 || l >= nl || j < 0 || static_cast<std::size_t>(j) >= m) continue;
          per_leaf[static_cast<std::size_t>(l)].emplace_back(j, w.op_c[k]);
        }
        for (int l = 0; l < nl; ++l) {
          const auto& js = per_leaf[static_cast<std::size_t>(l)];
          for (std::size_t a = 0; a < js.size(); ++a) {
            const std::size_t ja = static_cast<std::size_t>(js[a].first);
            cov[static_cast<std::size_t>(l) * nl + l] +=
                js[a].second * v[ja] * js[a].second;
            for (std::size_t b = a + 1; b < js.size(); ++b) {
              const std::size_t jb = static_cast<std::size_t>(js[b].first);
              const double val = js[a].second * v[ja] * js[b].second;
              cov[static_cast<std::size_t>(l) * nl + l] += 0.0;  // 对角已计
              (void)val;
            }
          }
        }
        // 非对角: C[l][l'] = Σ_j c_lj v_j c_lj'（共享像元对）
        for (std::size_t j = 0; j < m; ++j) {
          if (!(v[j] > 0.0)) continue;
          std::vector<std::pair<int, double>> lj;   // (leaf, c) for this pixel
          for (std::size_t k = 0; k < w.op_c.size(); ++k) {
            if (w.op_pix[k] != static_cast<int>(j)) continue;
            const int l = w.op_leaf[k];
            if (l < 0 || l >= nl) continue;
            lj.emplace_back(l, w.op_c[k]);
          }
          for (std::size_t a = 0; a < lj.size(); ++a) {
            for (std::size_t b = a + 1; b < lj.size(); ++b) {
              const double val = lj[a].second * v[j] * lj[b].second;
              cov[static_cast<std::size_t>(lj[a].first) * nl + lj[b].first] += val;
              cov[static_cast<std::size_t>(lj[b].first) * nl + lj[a].first] += val;
            }
          }
        }
      }
      if (cholesky_spd(cov, nl)) {
        // A_s = [R P, R Z]（nl × ncol）
        std::vector<double> as(static_cast<std::size_t>(nl) * ncol, 0.0);
        for (std::size_t k = 0; k < w.op_c.size(); ++k) {
          const int l = w.op_leaf[k];
          const int j = w.op_pix[k];
          if (l < 0 || l >= nl || j < 0 || static_cast<std::size_t>(j) >= m) continue;
          const double c = w.op_c[k];
          as[static_cast<std::size_t>(l) * ncol + 0] += c * w.profile[static_cast<std::size_t>(j)];
          if (ncol > 1)
            as[static_cast<std::size_t>(l) * ncol + 1] += c * 1.0;
          if (ncol > 2)
            as[static_cast<std::size_t>(l) * ncol + 2] += c * w.src_x[static_cast<std::size_t>(j)];
          if (ncol > 3)
            as[static_cast<std::size_t>(l) * ncol + 3] += c * w.src_y[static_cast<std::size_t>(j)];
        }
        for (int c = 0; c < ncol; ++c) {
          std::vector<double> bcol(static_cast<std::size_t>(nl), 0.0);
          for (int l = 0; l < nl; ++l)
            bcol[static_cast<std::size_t>(l)] = as[static_cast<std::size_t>(l) * ncol + c];
          std::vector<double> xcol(static_cast<std::size_t>(nl), 0.0);
          cholesky_solve(cov, nl, bcol.data(), xcol.data());
          for (int a = 0; a < ncol; ++a) {
            double sum = 0.0;
            for (int l = 0; l < nl; ++l)
              sum += as[static_cast<std::size_t>(l) * ncol + a] *
                    xcol[static_cast<std::size_t>(l)];
            mm[static_cast<std::size_t>(a) * ncol + c] = sum;
            mm[static_cast<std::size_t>(c) * ncol + a] = sum;
          }
        }
        gls_ok = true;
      }
    }
    if (!gls_ok) {
      fail_point("operator_rank_deficient");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    double v_f = 0.0;
    int mrank = 0;
    double mcond = kNaN;
    if (!invert_ff_entry(mm, ncol, &v_f, &mrank, &mcond)) {
      fail_point(ncol > 1 ? "flux_not_identifiable_with_background"
                          : "flux_not_identifiable");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    if (!std::isfinite(v_f) || !(v_f > 0.0)) {
      fail_point("nonfinite_flux_variance");
      out.precision.controls.push_back(ctrl);
      continue;
    }

    // 支持面积（合同 §5: 各有效 patch 的真实球面支持面积为权重; 不重复计面积）
    double support_total = 0.0;
    for (double d : w.leaf_dp) support_total += std::min(d, w.omega_leaf);

    // V_B（目标叶对角; 同源同位置同条件）: Σ_j c_pj²·v_j
    double v_b = 0.0;
    for (std::size_t k = 0; k < w.op_c.size(); ++k) {
      const int l = w.op_leaf[k];
      if (l < 0 || static_cast<std::size_t>(l) >= w.leaf_ipix.size()) continue;
      if (w.leaf_ipix[static_cast<std::size_t>(l)] != w.target_leaf) continue;
      const int j = w.op_pix[k];
      if (j < 0 || static_cast<std::size_t>(j) >= m) continue;
      v_b += w.op_c[k] * w.op_c[k] * v[j];
    }

    ctrl.status.availability = PrecisionAvailability::kAvailable;
    ctrl.status.accuracy = accuracy;
    ctrl.status.omitted_terms = omitted;
    ctrl.reference_flux = f_ref;
    ctrl.flux_variance = v_f;
    ctrl.reference_snr = f_ref / std::sqrt(v_f);
    ctrl.support_area_sr = support_total;
    ctrl.covariance_rank = static_cast<std::uint64_t>(mrank);
    ctrl.condition_number = mcond;
    ctrl.leaf_ipix = w.target_leaf;
    ctrl.leaf_order = w.leaf_order;
    if (std::isfinite(v_b) && v_b > 0.0) {
      ctrl.transfer.availability = PrecisionAvailability::kAvailable;
      ctrl.transfer.surface_variance = v_b;
      ctrl.transfer.transfer_factor = v_b / v_f;
      ctrl.transfer.surface_variance_unit = cfg.reference.flux_unit + "^2/sr^2";
      ctrl.transfer.transfer_unit = "sr^-2";
      ctrl.transfer.condition = CovarianceCondition::kReferenceSource;
    } else {
      ctrl.transfer.reason = "surface_variance_unavailable";
    }
    ++n_available;

    SphericalCompletionResult::LocalWindow lw;
    lw.window_index = window_index;
    lw.control_id = w.control_id;
    lw.pix_index = w.pix_index;
    lw.profile = w.profile;
    lw.h_sphere = w.profile;   // h = R P 的信息等价源空间像
    lw.variance = v;           // C_sphere = R diag(v) Rᵀ 的逐像元因子
    for (int b = 1; b < n_bg; ++b) {
      for (std::size_t j = 0; j < m; ++j) {
        lw.z_sphere.push_back(b == 1 ? 1.0
                                     : (b == 2 ? w.src_x[j] : w.src_y[j]));
      }
    }
    lw.n_bg = n_bg;
    lw.d_p = 0.0;
    for (double d : w.leaf_dp) lw.d_p += d;
    lw.pix_omega = w.pix_omega;
    lw.pix_a_drop = w.pix_a_drop;
    lw.op_pix = w.op_pix;
    lw.op_leaf = w.op_leaf;
    lw.op_c = w.op_c;
    lw.leaf_dp = w.leaf_dp;
    for (double x : w.pix_omega) lw.omega_sum += x;
    for (double x : w.pix_a_drop) lw.a_drop_sum += x;
    lw.support_area_sr = support_total;
    lw.op_nnz = w.op_c.size();
    out.windows.push_back(std::move(lw));
    out.precision.controls.push_back(std::move(ctrl));
  }

  if (n_available == 0) {
    out.precision.status.availability = PrecisionAvailability::kUnavailable;
    out.precision.status.accuracy = PrecisionAccuracy::kNotApplicable;
    if (out.precision.status.reason.empty()) {
      out.precision.status.reason = "no estimable control point";
    }
  }
  return out;
}

FrameReferenceSummary summarize_frame_reference(
    const std::vector<ReferenceControl>& controls) {
  FrameReferenceSummary s;
  std::vector<std::pair<double, double>> pairs;  // (log snr, weight)
  for (const auto& c : controls) {
    if (c.status.availability != PrecisionAvailability::kAvailable) continue;
    if (!c.reference_snr.has_value() || !(*c.reference_snr > 0.0)) continue;
    if (!c.support_area_sr.has_value() || !(*c.support_area_sr > 0.0)) continue;
    pairs.emplace_back(std::log(*c.reference_snr), *c.support_area_sr);
    s.supported_area_sr += *c.support_area_sr;
    ++s.n_valid;
  }
  if (pairs.empty()) {
    s.status.availability = PrecisionAvailability::kUnavailable;
    s.status.accuracy = PrecisionAccuracy::kNotApplicable;
    s.status.reason = "no valid reference control";
    return s;
  }
  std::stable_sort(pairs.begin(), pairs.end(),
                   [](const auto& a, const auto& b) { return a.first < b.first; });
  double total = 0.0;
  for (const auto& p : pairs) total += p.second;
  const double half = 0.5 * total;
  double acc = 0.0;
  double med = pairs.back().first;
  for (const auto& p : pairs) {
    acc += p.second;
    if (acc >= half) { med = p.first; break; }
  }
  s.reference_snr = std::exp(med);
  s.status.availability = PrecisionAvailability::kAvailable;
  // 摘要继承控制点最严格的 accuracy：任一点 approximate ⇒ 摘要 approximate。
  bool any_approx = false, all_upper = true;
  for (const auto& c : controls) {
    if (c.status.availability != PrecisionAvailability::kAvailable) continue;
    if (c.status.accuracy == PrecisionAccuracy::kApproximate) any_approx = true;
    if (c.status.accuracy != PrecisionAccuracy::kUpperBound) all_upper = false;
  }
  s.status.accuracy = any_approx ? PrecisionAccuracy::kApproximate
                                 : (all_upper ? PrecisionAccuracy::kUpperBound
                                              : PrecisionAccuracy::kConditionalComplete);
  std::vector<std::string> omitted;
  for (const auto& c : controls) {
    if (c.status.availability != PrecisionAvailability::kAvailable) continue;
    for (const auto& t : c.status.omitted_terms) {
      if (std::find(omitted.begin(), omitted.end(), t) == omitted.end()) {
        omitted.push_back(t);
      }
    }
  }
  s.status.omitted_terms = omitted;
  return s;
}

}  // namespace acsd::normalize
