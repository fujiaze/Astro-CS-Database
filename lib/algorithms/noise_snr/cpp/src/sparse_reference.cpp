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

// ── 对称半正定矩阵的保留子空间二次型（F01 修复的核心）───────────────────────
// 输入 H（m×m, 对称 PSD = V^{1/2} RᵀR V^{1/2}）与 B（m×ncol = V^{1/2} G A_src）。
// 输出 M = Bᵀ H⁺² B（**平方**的 Moore-Penrose 逆; 显式特征分解 + 保留子空间;
// rank 报告 H 的秩 = rank(C_sphere)）。
// 推导: C = R V Rᵀ = Q Qᵀ（Q = R V^{1/2}）⇒ C⁺ = Q H⁺² Qᵀ ⇒
//   RᵀC⁺R = G V^{1/2} H⁺² V^{1/2} G（G = RᵀR）
//   ⇒ M = A_srcᵀ RᵀC⁺R A_src = Bᵀ H⁺² B,  B = V^{1/2} G A_src
// H ≻ 0（R 满列秩）时严格有 M = A_srcᵀ V⁻¹ A_src（NOISE_SNR §11.1 恒等式;
// 旧实现除了矩阵次序错误还漏掉 H 的平方 ⇒ 反例 rel err -0.66）。
// H 半正定（R 秩亏）时只在保留子空间内报告; 被投掉的方向不当作零信息, 不加地板。
bool retained_subspace_quadratic(const std::vector<double>& h, int m,
                                 const std::vector<double>& b, int ncol,
                                 double rank_tol, std::vector<double>* m_out,
                                 int* rank_out, double* cond_out) {
  std::vector<double> w, v;
  jacobi_eigh(h, m, w, v);
  double wmax = 0.0;
  for (int i = 0; i < m; ++i) wmax = std::max(wmax, w[static_cast<std::size_t>(i)]);
  if (!(wmax > 0.0) || !std::isfinite(wmax)) return false;
  const double tol = (rank_tol > 0.0 ? rank_tol : 1e-12) * wmax;
  int rank = 0;
  double wmin_pos = 0.0;
  for (int i = 0; i < m; ++i) {
    if (w[static_cast<std::size_t>(i)] > tol) {
      ++rank;
      if (wmin_pos == 0.0) wmin_pos = w[static_cast<std::size_t>(i)];
    }
  }
  m_out->assign(static_cast<std::size_t>(ncol) * ncol, 0.0);
  for (int i = 0; i < m; ++i) {
    if (!(w[static_cast<std::size_t>(i)] > tol)) continue;  // 非保留方向不进 M
    std::vector<double> bi(ncol, 0.0);
    for (int j = 0; j < m; ++j) {
      const double vji = v[static_cast<std::size_t>(j) * m + i];
      if (vji == 0.0) continue;
      for (int c = 0; c < ncol; ++c) {
        bi[static_cast<std::size_t>(c)] +=
            vji * b[static_cast<std::size_t>(j) * ncol + c];
      }
    }
    const double inv = 1.0 / (w[static_cast<std::size_t>(i)] *
                               w[static_cast<std::size_t>(i)]);
    for (int a = 0; a < ncol; ++a) {
      const double ba = bi[static_cast<std::size_t>(a)];
      if (ba == 0.0) continue;
      for (int c = a; c < ncol; ++c) {
        const double val = ba * bi[static_cast<std::size_t>(c)] * inv;
        (*m_out)[static_cast<std::size_t>(a) * ncol + c] += val;
        if (c != a) (*m_out)[static_cast<std::size_t>(c) * ncol + a] += val;
      }
    }
  }
  if (rank_out) *rank_out = rank;
  if (cond_out) *cond_out = wmin_pos > 0.0 ? wmax / wmin_pos : kNaN;
  for (double x : *m_out) {
    if (!std::isfinite(x)) return false;
  }
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
  // 帧级缺项基线（合同 §5）。逐点可因窗口截断/模板缺项**进一步降级**;
  // 帧级状态在循环后由全部有效控制点归约（不提前宣告比任何点都严格的结论）。
  const bool gain_known = std::isfinite(cfg.gain_e_per_adu) && cfg.gain_e_per_adu > 0.0;
  std::vector<std::string> omitted_frame;
  if (!gain_known) omitted_frame.push_back("source_poisson");
  if (!cfg.master_uncertainty_modelled) omitted_frame.push_back("master_uncertainty");
  PrecisionAccuracy accuracy_frame = PrecisionAccuracy::kConditionalComplete;
  if (!gain_known) {
    accuracy_frame = cfg.template_fixed ? PrecisionAccuracy::kUpperBound
                                        : PrecisionAccuracy::kApproximate;
  }
  if (!cfg.master_uncertainty_modelled &&
      accuracy_frame == PrecisionAccuracy::kConditionalComplete) {
    accuracy_frame = PrecisionAccuracy::kApproximate;
  }
  if (!cfg.template_fixed &&
      accuracy_frame == PrecisionAccuracy::kConditionalComplete) {
    accuracy_frame = PrecisionAccuracy::kApproximate;
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
  out.precision.status.accuracy = accuracy_frame;
  out.precision.status.omitted_terms = omitted_frame;

  std::uint64_t n_available = 0;
  // 帧级归约用: 有效控制点中最严格的 accuracy 与 omitted_terms 并集。
  PrecisionAccuracy worst_accuracy = accuracy_frame;
  bool any_available = false;
  std::vector<std::string> union_omitted = omitted_frame;
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
        w.op_c.size() != w.op_leaf.size() || !w.has_target_leaf) {
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

    // 平面源泊松条件（NOISE_SNR §11.3 / PHOTOMETRY §4）:
    //   v_j 已是（调用方换算到 applied 数组标度的）blank-sky 方差 = α_j²·原生方差;
    //   参考源泊松期望在**原生** gain g 下为 F_native·P_j/g，施加确定性标度后
    //   为 α_j²·F_native·P_j/g。以 applied 面的 F_applied 表示时该分量写作
    //   α_j·F_applied·P_j/g（等价有效 gain g_eff = g·a_ref/α_j²）。
    //   原生 gain 的设备来源保留在 cfg.gain_provenance, 不被有效 gain 覆盖。
    const double alpha_const =
        std::isfinite(cfg.plane_scale) && cfg.plane_scale > 0.0 ? cfg.plane_scale
                                                                : 1.0;
    std::vector<double> alpha(m, alpha_const);
    if (!cfg.pixel_scale.empty()) {
      if (cfg.pixel_scale.size() != m) {
        fail_point("pixel_scale_length_mismatch");
        out.precision.controls.push_back(ctrl);
        continue;
      }
      for (std::size_t j = 0; j < m; ++j) {
        if (!std::isfinite(cfg.pixel_scale[j]) || !(cfg.pixel_scale[j] > 0.0)) {
          fail_point("nonfinite_pixel_scale");
          alpha.clear();
          break;
        }
        alpha[j] = cfg.pixel_scale[j];
      }
      if (alpha.empty()) {
        out.precision.controls.push_back(ctrl);
        continue;
      }
    }
    // a_ref(u) = Σ_j α_j·P_j（NORMALIZE_DATA §5 的空间标度配对归一）
    double a_ref = 0.0;
    for (std::size_t j = 0; j < m; ++j) a_ref += alpha[j] * std::max(0.0, w.profile[j]);
    if (!(a_ref > 0.0) || !std::isfinite(a_ref)) {
      fail_point("nonfinite_reference_scale");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    const bool gain_known =
        std::isfinite(cfg.gain_e_per_adu) && cfg.gain_e_per_adu > 0.0;
    // 窗口算子截断: 窗口像素对目标叶完整 N_p 的覆盖不足 ⇒ 随机贡献被省略,
    // 不得声称完整（NOISE_SNR §11.2「随机方差不能被省略后称完整」）。
    double n_p_full = 0.0;
    double n_p_cov = 0.0;
    {
      const int nlf = static_cast<int>(w.leaf_ipix.size());
      const double* nf = w.leaf_n_full.empty() ? nullptr : w.leaf_n_full.data();
      const double* nc = w.leaf_n_covered.empty() ? nullptr : w.leaf_n_covered.data();
      for (int l = 0; l < nlf; ++l) {
        if (w.leaf_ipix[static_cast<std::size_t>(l)] != w.target_leaf) continue;
        if (nf && l < static_cast<int>(w.leaf_n_full.size()))
          n_p_full = w.leaf_n_full[static_cast<std::size_t>(l)];
        if (nc && l < static_cast<int>(w.leaf_n_covered.size()))
          n_p_cov = w.leaf_n_covered[static_cast<std::size_t>(l)];
      }
    }
    const bool operator_complete =
        n_p_full > 0.0 && n_p_cov >= n_p_full * (1.0 - 1e-9);
    // 逐点 accuracy / omitted_terms（帧级基线 + 本点专属缺项）。
    PrecisionAccuracy accuracy_pw = accuracy_frame;
    std::vector<std::string> omitted_pw = omitted_frame;
    if (!gain_known) {
      // 只有省略项是 PSD 随机噪声且模板/域固定时才可称 upper_bound（合同 §5）;
      // 算子被截断 ⇒ 不满足「局部域完整」前提 ⇒ 至多 approximate。
      accuracy_pw = (cfg.template_fixed && operator_complete)
                        ? PrecisionAccuracy::kUpperBound
                        : PrecisionAccuracy::kApproximate;
    }
    if (!operator_complete) {
      omitted_pw.push_back("operator_window_truncation");
      if (accuracy_pw == PrecisionAccuracy::kConditionalComplete) {
        accuracy_pw = PrecisionAccuracy::kApproximate;
      }
    }
    if (!cfg.master_uncertainty_modelled &&
        accuracy_pw == PrecisionAccuracy::kConditionalComplete) {
      accuracy_pw = PrecisionAccuracy::kApproximate;
    }
    if (!cfg.template_fixed &&
        accuracy_pw == PrecisionAccuracy::kConditionalComplete) {
      accuracy_pw = PrecisionAccuracy::kApproximate;
    }

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
        // applied 面源泊松 = α_j²·F_native·P_j/g, F_native = F_applied/a_ref
        // ⇒ (α_j²/a_ref)·F_applied·P_j/g。α_j 恒为 α 时化简为 α·F_applied·P_j/g
        // （NOISE_SNR §11.3 逐字）; α=1 时逐位退化为 F_applied·P_j/g。
        vj += (alpha[j] * alpha[j] / a_ref) * (f_ref * pj / cfg.gain_e_per_adu);
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
    // ── 球面 GLS（精确; NORMALIZE_DATA §6 + NOISE_SNR §3/§11.1）────────────
    // y = F·(R P) + Z_s·β + n, C = R V Rᵀ, V = diag(v_j), A_s = [R P, R Z]（nl×ncol）。
    // 欲求 M = A_sᵀ C⁺ A_s。直接做需要 nl×nl 求逆（nl 可达数千）。用恒等式
    //   Rᵀ(RVRᵀ)⁺R = G V^{1/2} H⁺ V^{1/2} G,  G = RᵀR, H = V^{1/2} G V^{1/2}
    // （H 与 C 的非零特征值相同 ⇒ 秩与条件数同源）, 于是
    //   M = A_srcᵀ RᵀC⁺R A_src = Bᵀ H⁺² B,  B = V^{1/2} G A_src,  A_src = [P, Z]
    // **次序**: B = V^{1/2}·G·A_src（V^{1/2} 在左）。G 与 V^{1/2} 一般不交换;
    // 旧实现写成 G·V^{1/2}·A_src ⇒ 异方差下错误（F01 反例, NOISE_SNR §11.1）。
    // H ≻ 0（R 满列秩, 由特征分解显式验证）时严格有
    //   M = A_srcᵀ V⁻¹ A_src            （NOISE_SNR §11.1 条件信息恒等式）
    // H 半正定（R 秩亏）时用显式特征分解取保留子空间; 被投掉的方向不当作零信息,
    // 参数子空间内不可辨识 ⇒ unavailable（合同 §6: 不加 epsilon、不用伪逆造信息量）。
    // 与显式 nl 空间 GLS 的数值一致性由 eng/tests/unit/sparse_reference_gls_test.cpp
    // 的 dual 段对拍验证（不调用同一构造函数作 oracle）。
    std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
    int cov_rank = 0;
    double cov_cond = kNaN;
    bool gls_ok = false;
    {
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
      // H = V^{1/2} G V^{1/2}
      std::vector<double> hmat = g;
      for (int j = 0; j < static_cast<int>(m); ++j) {
        const double sj = std::sqrt(v[static_cast<std::size_t>(j)]);
        for (int j2 = 0; j2 < static_cast<int>(m); ++j2) {
          const double s2 = std::sqrt(v[static_cast<std::size_t>(j2)]);
          hmat[static_cast<std::size_t>(j) * m + j2] *= sj * s2;
        }
      }
      // B = V^{1/2} G A_src（m×ncol; 正确次序: V^{1/2} 在左）
      std::vector<double> bmat(static_cast<std::size_t>(m) * ncol, 0.0);
      for (int c = 0; c < ncol; ++c) {
        for (int j = 0; j < static_cast<int>(m); ++j) {
          auto src_col = [&](int j2) -> double {
            switch (c) {
              case 0:  return w.profile[static_cast<std::size_t>(j2)];
              case 1:  return 1.0;
              case 2:  return w.src_x[static_cast<std::size_t>(j2)];
              default: return w.src_y[static_cast<std::size_t>(j2)];
            }
          };
          // (V^{1/2} G A_src)[j][c] = sqrt(v_j) · Σ_{j2} G[j][j2]·A_src[j2][c]
          double sum = 0.0;
          for (int j2 = 0; j2 < static_cast<int>(m); ++j2) {
            sum += g[static_cast<std::size_t>(j) * m + j2] * src_col(j2);
          }
          bmat[static_cast<std::size_t>(j) * ncol + c] =
              std::sqrt(v[static_cast<std::size_t>(j)]) * sum;
        }
      }
      gls_ok = retained_subspace_quadratic(hmat, static_cast<int>(m), bmat, ncol,
                                           cfg.support_rank_tol, &mm, &cov_rank,
                                           &cov_cond);
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


    // 支持面积（合同 §5）: 叶之间互不相交 ⇒ Σ_l min(D_l, Ω_leaf) 本身是不重叠的
    // 球面面积分区（D_l 是真实 drop 命中面积, Ω_leaf 是其上界）。patch 之间可能
    // 共享叶 ⇒ **帧级**摘要的权重去重由调用方按 leaf 归属唯一 patch 完成
    // （detail §7.4「不按求解预算圆盘面积替换」; 求解预算不是物理支持定义）。
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
    ctrl.status.accuracy = accuracy_pw;
    ctrl.status.omitted_terms = omitted_pw;
    ctrl.reference_flux = f_ref;
    ctrl.flux_variance = v_f;
    ctrl.reference_snr = f_ref / std::sqrt(v_f);
    ctrl.support_area_sr = support_total;
    // covariance_rank / condition_number 指 **C_sphere 的保留子空间**
    // （detail §7.4: 与参数信息矩阵的秩分名, 不偷换意义）。
    ctrl.covariance_rank = static_cast<std::uint64_t>(cov_rank);
    ctrl.condition_number = std::isfinite(cov_cond) && cov_cond >= 1.0
                                ? std::optional<double>(cov_cond)
                                : std::nullopt;
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
    any_available = true;
    // 帧级归约: 最严格 accuracy（upper_bound > conditional_complete > approximate
    // 的严格度排序: approximate 最宽松 ⇒ 任一点 approximate ⇒ 摘要 approximate）。
    if (accuracy_pw == PrecisionAccuracy::kApproximate) {
      worst_accuracy = PrecisionAccuracy::kApproximate;
    } else if (accuracy_pw == PrecisionAccuracy::kConditionalComplete &&
               worst_accuracy == PrecisionAccuracy::kUpperBound) {
      worst_accuracy = PrecisionAccuracy::kConditionalComplete;
    }
    for (const auto& t : omitted_pw) {
      if (std::find(union_omitted.begin(), union_omitted.end(), t) ==
          union_omitted.end()) {
        union_omitted.push_back(t);
      }
    }

    // ── 局部窗口载荷（合同 §7.1 + detail §7.4 的真实 shape/单位/秩）──────────
    SphericalCompletionResult::LocalWindow lw;
    lw.window_index = window_index;
    lw.control_id = w.control_id;
    lw.pix_index = w.pix_index;
    lw.profile = w.profile;
    // h_sphere = R P（长度 nl; 单位 1/sr）。旧实现把平面 P（长度 m）标成球面 h。
    lw.h_sphere.assign(static_cast<std::size_t>(nl), 0.0);
    for (std::size_t k = 0; k < w.op_c.size(); ++k) {
      const int l = w.op_leaf[k];
      const int j = w.op_pix[k];
      if (l < 0 || l >= nl || j < 0 || static_cast<std::size_t>(j) >= m) continue;
      lw.h_sphere[static_cast<std::size_t>(l)] +=
          w.op_c[k] * w.profile[static_cast<std::size_t>(j)];
    }
    // v_j（C = R diag(v) Rᵀ 的精确源空间因子; 长度 m, linear_unit²/sr² 的因子）
    lw.source_variance = v;
    // Z_sphere = R Z_planar（nl × n_bg row-major; 与 h 同一叶序）
    lw.n_bg = n_bg;
    // Z_sphere = R Z_planar: nl × n_bg row-major（n_bg 列; 列索引 1..n_bg）
    for (int b = 1; b <= n_bg; ++b) {
      for (int l = 0; l < nl; ++l) lw.z_sphere.push_back(0.0);
    }
    for (std::size_t k = 0; k < w.op_c.size(); ++k) {
      const int l = w.op_leaf[k];
      const int j = w.op_pix[k];
      if (l < 0 || l >= nl || j < 0 || static_cast<std::size_t>(j) >= m) continue;
      const double c = w.op_c[k];
      for (int b = 1; b <= n_bg; ++b) {
        const double planar = (b == 1) ? 1.0
                            : (b == 2) ? w.src_x[static_cast<std::size_t>(j)]
                                       : w.src_y[static_cast<std::size_t>(j)];
        lw.z_sphere[static_cast<std::size_t>(b - 1) * nl +
                    static_cast<std::size_t>(l)] += c * planar;
      }
    }
    // C_sphere = R diag(v) Rᵀ（nl×nl row-major）。仅在 nl 不超过落盘预算时写显式
    // 矩阵; 超出时只保留精确因子（v_j + R 的 CSR, 由 drizzle_operator 模型承载）
    // 并如实登记 representation, **不把因子塞进显式矩阵标签**（合同 §7.1）。
    lw.covariance_explicit =
        static_cast<std::uint64_t>(nl) <= cfg.covariance_matrix_budget;
    if (lw.covariance_explicit) {
      std::vector<std::vector<std::pair<int, double>>> per_leaf(
          static_cast<std::size_t>(nl));
      for (std::size_t k = 0; k < w.op_c.size(); ++k) {
        const int l = w.op_leaf[k];
        const int j = w.op_pix[k];
        if (l < 0 || l >= nl || j < 0 || static_cast<std::size_t>(j) >= m) continue;
        per_leaf[static_cast<std::size_t>(l)].emplace_back(j, w.op_c[k]);
      }
      lw.covariance.assign(static_cast<std::size_t>(nl) * nl, 0.0);
      // 对角: C[l][l] = Σ_j c_lj² v_j
      for (int l = 0; l < nl; ++l) {
        for (const auto& e : per_leaf[static_cast<std::size_t>(l)]) {
          const double c = e.second;
          lw.covariance[static_cast<std::size_t>(l) * nl + l] +=
              c * v[static_cast<std::size_t>(e.first)] * c;
        }
      }
      // 跨叶非对角（共享源像元）: C[l][l'] = Σ_j c_lj v_j c_lj' 
      for (std::size_t j = 0; j < m; ++j) {
        if (!(v[j] > 0.0)) continue;
        std::vector<std::pair<int, double>> lj;
        for (std::size_t k = 0; k < w.op_c.size(); ++k) {
          if (w.op_pix[k] != static_cast<int>(j)) continue;
          const int l = w.op_leaf[k];
          if (l < 0 || l >= nl) continue;
          lj.emplace_back(l, w.op_c[k]);
        }
        for (std::size_t a = 0; a < lj.size(); ++a) {
          for (std::size_t b = a + 1; b < lj.size(); ++b) {
            const double val = lj[a].second * v[j] * lj[b].second;
            lw.covariance[static_cast<std::size_t>(lj[a].first) * nl +
                          static_cast<std::size_t>(lj[b].first)] += val;
            lw.covariance[static_cast<std::size_t>(lj[b].first) * nl +
                          static_cast<std::size_t>(lj[a].first)] += val;
          }
        }
      }
    }
    lw.random_components = {"background", "read_noise", "dark",
                            "reference_source_poisson"};
    if (!operator_complete) {
      lw.random_components.push_back("operator_window_truncation");
    }
    lw.omitted_terms = omitted_pw;
    lw.d_p = 0.0;
    for (double d : w.leaf_dp) lw.d_p += d;
    lw.n_p = n_p_full;
    lw.pix_omega = w.pix_omega;
    lw.pix_a_drop = w.pix_a_drop;
    lw.op_pix = w.op_pix;
    lw.op_leaf = w.op_leaf;
    lw.op_c = w.op_c;
    lw.leaf_dp = w.leaf_dp;
    lw.omega_sum = 0.0;
    for (double x : w.pix_omega) lw.omega_sum += x;
    lw.a_drop_sum = 0.0;
    for (double x : w.pix_a_drop) lw.a_drop_sum += x;
    lw.support_area_sr = support_total;
    lw.operator_coverage_fraction =
        n_p_full > 0.0 ? std::min(1.0, n_p_cov / n_p_full) : 0.0;
    lw.op_nnz = w.op_c.size();
    out.windows.push_back(std::move(lw));
    out.precision.controls.push_back(std::move(ctrl));
  }

  // 帧级状态由全部有效控制点归约（不提前宣告比任何点都严格的结论）。
  if (any_available) {
    out.precision.status.accuracy = worst_accuracy;
    out.precision.status.omitted_terms = union_omitted;
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
