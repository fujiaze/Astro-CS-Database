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
  for (const auto& w : windows) {
    const std::size_t m = w.pix_index.size();
    const int nl = static_cast<int>(w.leaf_ipix.size());
    ReferenceControl ctrl;
    ctrl.control_id = w.control_id;
    ctrl.ra_deg = 0.0;   // 由调用方 (持 WCS) 回填; 此处只填检测器坐标
    ctrl.dec_deg = 0.0;
    ctrl.source_x0 = w.cx_px;
    ctrl.source_y0 = w.cy_px;
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

    if (m == 0 || nl == 0 || static_cast<int>(w.r_coeff.size()) != nl * static_cast<int>(m)) {
      fail_point("empty_local_window");
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

    // h = R P (nl)
    std::vector<double> h(static_cast<std::size_t>(nl), 0.0);
    for (int l = 0; l < nl; ++l) {
      double s = 0.0;
      for (std::size_t j = 0; j < m; ++j) {
        s += w.r_coeff[static_cast<std::size_t>(l) * m + j] * w.profile[j];
      }
      h[static_cast<std::size_t>(l)] = s;
    }
    double hnorm = 0.0;
    for (double x : h) hnorm += x * x;
    if (!(hnorm > 0.0) || !std::isfinite(hnorm)) {
      fail_point("template_outside_drizzle_support");
      out.precision.controls.push_back(ctrl);
      continue;
    }

    // Z_sphere = R Z_planar (nl × n_bg)
    std::vector<double> z(static_cast<std::size_t>(nl) * static_cast<std::size_t>(n_bg), 0.0);
    for (int l = 0; l < nl; ++l) {
      for (int b = 0; b < n_bg; ++b) {
        double s = 0.0;
        for (std::size_t j = 0; j < m; ++j) {
          const double r = w.r_coeff[static_cast<std::size_t>(l) * m + j];
          double basis = 1.0;
          if (b == 1) basis = w.src_x[j];
          else if (b == 2) basis = w.src_y[j];
          s += r * basis;
        }
        z[static_cast<std::size_t>(l) * n_bg + b] = s;
      }
    }

    // C = R diag(v) R^T (nl × nl)
    std::vector<double> cov(static_cast<std::size_t>(nl) * static_cast<std::size_t>(nl), 0.0);
    for (int a = 0; a < nl; ++a) {
      for (int b = a; b < nl; ++b) {
        double s = 0.0;
        for (std::size_t j = 0; j < m; ++j) {
          s += w.r_coeff[static_cast<std::size_t>(a) * m + j] * v[j] *
               w.r_coeff[static_cast<std::size_t>(b) * m + j];
        }
        cov[static_cast<std::size_t>(a) * nl + b] = s;
        cov[static_cast<std::size_t>(b) * nl + a] = s;
      }
    }
    std::vector<double> cw, cv;
    jacobi_eigh(cov, nl, cw, cv);
    double lambda_max = 0.0;
    for (int i = 0; i < nl; ++i) {
      lambda_max = std::max(lambda_max, cw[static_cast<std::size_t>(i)]);
    }
    if (!(lambda_max > 0.0) || !std::isfinite(lambda_max)) {
      fail_point("covariance_not_positive_definite");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    const double cov_tol = cfg.support_rank_tol * lambda_max;
    std::vector<int> retained;
    for (int i = 0; i < nl; ++i) {
      if (cw[static_cast<std::size_t>(i)] > cov_tol) retained.push_back(i);
    }
    const int crank = static_cast<int>(retained.size());
    const double cond = crank > 0 ? lambda_max / cw[static_cast<std::size_t>(retained.front())]
                                  : kNaN;

    // 设计矩阵列投影到保留子空间; 残差超阈 ⇒ 该列不可估计。
    auto project = [&](const double* col, std::vector<double>* out_col,
                       double* residual_ratio) {
      out_col->assign(static_cast<std::size_t>(nl), 0.0);
      for (int i : retained) {
        double d = 0.0;
        for (int l = 0; l < nl; ++l) {
          d += cv[static_cast<std::size_t>(l) * nl + i] * col[l];
        }
        for (int l = 0; l < nl; ++l) {
          (*out_col)[static_cast<std::size_t>(l)] +=
              cv[static_cast<std::size_t>(l) * nl + i] * d;
        }
      }
      double n0 = 0.0, nr = 0.0;
      for (int l = 0; l < nl; ++l) {
        n0 += col[l] * col[l];
        const double r = col[l] - (*out_col)[static_cast<std::size_t>(l)];
        nr += r * r;
      }
      *residual_ratio = n0 > 0.0 ? std::sqrt(nr / n0) : 1.0;
    };

    std::vector<std::vector<double>> acols(static_cast<std::size_t>(ncol));
    bool col_bad = false;
    for (int c = 0; c < ncol && !col_bad; ++c) {
      std::vector<double> col(static_cast<std::size_t>(nl), 0.0);
      if (c == 0) {
        for (int l = 0; l < nl; ++l) col[static_cast<std::size_t>(l)] = h[static_cast<std::size_t>(l)];
      } else {
        for (int l = 0; l < nl; ++l) {
          col[static_cast<std::size_t>(l)] = z[static_cast<std::size_t>(l) * n_bg + (c - 1)];
        }
      }
      double rr = 0.0;
      project(col.data(), &acols[static_cast<std::size_t>(c)], &rr);
      if (!(rr <= 1e-8)) {
        if (c == 0) {
          fail_point("template_outside_covariance_range");
        } else {
          fail_point("background_basis_outside_covariance_range");
        }
        col_bad = true;
      }
    }
    if (col_bad) {
      out.precision.controls.push_back(ctrl);
      continue;
    }

    // M = A_R^T C^+ A_R (ncol × ncol), C^+ = Σ_{i∈R} λ_i^{-1} u_i u_i^T
    std::vector<double> mm(static_cast<std::size_t>(ncol) * static_cast<std::size_t>(ncol), 0.0);
    for (int i : retained) {
      const double inv = 1.0 / cw[static_cast<std::size_t>(i)];
      for (int a = 0; a < ncol; ++a) {
        double da = 0.0;
        for (int l = 0; l < nl; ++l) {
          da += cv[static_cast<std::size_t>(l) * nl + i] *
                acols[static_cast<std::size_t>(a)][static_cast<std::size_t>(l)];
        }
        if (da == 0.0) continue;
        for (int b = a; b < ncol; ++b) {
          double db = 0.0;
          for (int l = 0; l < nl; ++l) {
            db += cv[static_cast<std::size_t>(l) * nl + i] *
                  acols[static_cast<std::size_t>(b)][static_cast<std::size_t>(l)];
          }
          const double val = inv * da * db;
          mm[static_cast<std::size_t>(a) * ncol + b] += val;
          if (b != a) mm[static_cast<std::size_t>(b) * ncol + a] += val;
        }
      }
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

    ctrl.status.availability = PrecisionAvailability::kAvailable;
    ctrl.status.accuracy = accuracy;
    ctrl.status.omitted_terms = omitted;
    ctrl.reference_flux = f_ref;
    ctrl.flux_variance = v_f;
    ctrl.reference_snr = f_ref / std::sqrt(v_f);
    ctrl.support_area_sr = 0.0;
    for (double s : w.leaf_support) ctrl.support_area_sr = *ctrl.support_area_sr + s;
    ctrl.covariance_rank = static_cast<std::uint64_t>(crank);
    ctrl.condition_number = cond;

    // 面亮度传递: V_B 取控制叶处 C_sphere 对角 (同源同位置同条件)。
    if (!w.leaf_support.empty()) {
      int target = -1;
      double best = -1.0;
      for (int l = 0; l < nl; ++l) {
        if (w.leaf_support[static_cast<std::size_t>(l)] > best) {
          best = w.leaf_support[static_cast<std::size_t>(l)];
          target = l;
        }
      }
      if (target >= 0) {
        const double v_b = cov[static_cast<std::size_t>(target) * nl + target];
        ctrl.leaf_ipix = w.leaf_ipix[static_cast<std::size_t>(target)];
        ctrl.leaf_order = w.leaf_order;
        if (std::isfinite(v_b) && v_b > 0.0) {
          ctrl.transfer.availability = PrecisionAvailability::kAvailable;
          ctrl.transfer.surface_variance = v_b;
          ctrl.transfer.transfer_factor = v_b / v_f;
          ctrl.transfer.surface_variance_unit =
              cfg.reference.flux_unit + "^2/sr^2";
          ctrl.transfer.transfer_unit = "sr^-2";
          ctrl.transfer.condition = CovarianceCondition::kReferenceSource;
        } else {
          ctrl.transfer.reason = "surface_variance_unavailable";
        }
      }
    }
    ++n_available;

    SphericalCompletionResult::LocalWindow lw;
    lw.control_id = w.control_id;
    lw.pix_index = w.pix_index;
    lw.profile = w.profile;
    lw.h_sphere = h;
    lw.variance.assign(cov.begin(), cov.end());  // C_sphere (nl×nl row-major)
    for (int b = 0; b < n_bg; ++b) {
      for (int l = 0; l < nl; ++l) {
        lw.z_sphere.push_back(z[static_cast<std::size_t>(l) * n_bg + b]);
      }
    }
    lw.n_bg = n_bg;
    lw.d_p = 0.0;
    for (double s : w.leaf_dp) lw.d_p += s;
    lw.n_p = 0.0;
    for (double s : w.leaf_np) lw.n_p += s;
    lw.support_area_sr = *ctrl.support_area_sr;
    lw.a_drop_sum = 0.0;
    lw.omega_sum = 0.0;
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
