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

// ── 列主元 Householder QR（秩揭示; NOISE_SNR §11.4 指定的数值 backend）─────
// 分解 B（m×n）。Q' 由反射子隐式表示（存于 B 的下三角部分）, 因此
//   * rank 由"最大残余列范数 ≤ rel_tol · 初始最大列范数"判定, 不用零计数;
//   * apply_qt() 只做 O(r·m·ncol) 即得 Q'ᵀW, 不需显式生成 Q'。
struct PivotedQr {
  int m = 0, n = 0, rank = 0;
  std::vector<double> a;       // m×n row-major: 上三角 R + 下三角反射子
  std::vector<double> beta;    // 每步 2/(vᵀv)
  std::vector<double> pivnorm; // 每步 pivot 列范数（秩/条件诊断）
  double max_col_norm = 0.0;   // 初始最大列范数（相对容差基准）
  double rel_tol = 1e-12;

  // B[j][l] = sqrt(d[j]) · R[l][j], 由 CSR 三元组 (op_pix/op_leaf/op_c) 组装。
  // d = 逐源像元条件方差（含参考源 Poisson）。
  bool build_from_csr(const std::vector<double>& d,
                      const std::vector<int>& op_pix,
                      const std::vector<int>& op_leaf,
                      const std::vector<double>& op_c, int m_, int n_,
                      double rel_tol_) {
    m = m_; n = n_; rank = 0; rel_tol = rel_tol_;
    if (m <= 0 || n <= 0) return false;
    a.assign(static_cast<std::size_t>(m) * n, 0.0);
    beta.assign(static_cast<std::size_t>(m), 0.0);
    pivnorm.assign(static_cast<std::size_t>(m), 0.0);
    for (std::size_t k = 0; k < op_c.size(); ++k) {
      const int j = op_pix[k];
      const int l = op_leaf[k];
      if (j < 0 || j >= m || l < 0 || l >= n) continue;
      if (!(d[static_cast<std::size_t>(j)] > 0.0)) return false;
      a[static_cast<std::size_t>(j) * n + l] =
          std::sqrt(d[static_cast<std::size_t>(j)]) * op_c[k];
    }
    // 残余列范数**每步重新显式计算**（不用递推降更新）: 递推式
    //   cnorm²[j] ← cnorm²[j] − a[k][j]²
    // 的舍入噪声底约 ε·‖B‖²（开方后 ~ε·‖B‖）, 会把结构性零残差读成 ~1e-9
    // （相对 1e-10 门限高两个量级）⇒ 秩被虚报为 m（反例: R 两列完全相同,
    // 真秩 1 却被报 2）。显式重算的噪声底是 ε·‖B‖ ≈ 1e-16·max, 低于门限。
    // 成本 O(m²n/2), 与反射子本身同阶。
    auto residual_norms = [&](int k) {
      std::vector<double> cn(static_cast<std::size_t>(n), 0.0);
      for (int l = k; l < n; ++l) {
        double s = 0.0;
        for (int j = k; j < m; ++j) {
          const double x = a[static_cast<std::size_t>(j) * n + l];
          s += x * x;
        }
        cn[static_cast<std::size_t>(l)] = std::sqrt(s);
      }
      return cn;
    };
    std::vector<double> cnorm = residual_norms(0);
    max_col_norm = 0.0;
    for (int l = 0; l < n; ++l)
      max_col_norm = std::max(max_col_norm, cnorm[static_cast<std::size_t>(l)]);
    if (!(max_col_norm > 0.0)) return false;
    const int kmax = std::min(m, n);
    for (int k = 0; k < kmax; ++k) {
      // 列主元: 在残余列里取范数最大者（秩揭示）
      int p = k;
      double best = cnorm[static_cast<std::size_t>(k)];
      for (int l = k + 1; l < n; ++l) {
        if (cnorm[static_cast<std::size_t>(l)] > best) {
          best = cnorm[static_cast<std::size_t>(l)];
          p = l;
        }
      }
      cnorm = residual_norms(k);                      // 显式重算（见上）
      best = cnorm[static_cast<std::size_t>(k)];
      p = k;
      for (int l = k + 1; l < n; ++l) {
        if (cnorm[static_cast<std::size_t>(l)] > best) {
          best = cnorm[static_cast<std::size_t>(l)];
          p = l;
        }
      }
      if (!(best > rel_tol * max_col_norm)) break;   // 数值秩耗尽
      if (p != k) {
        for (int j = 0; j < m; ++j)
          std::swap(a[static_cast<std::size_t>(j) * n + k],
                    a[static_cast<std::size_t>(j) * n + p]);
        std::swap(cnorm[static_cast<std::size_t>(k)],
                  cnorm[static_cast<std::size_t>(p)]);
      }
      const double xnorm = cnorm[static_cast<std::size_t>(k)];
      if (!(xnorm > 0.0)) break;
      const double x0 = a[static_cast<std::size_t>(k) * n + k];
      const double alpha = (x0 >= 0.0) ? -xnorm : xnorm;
      const double v0 = x0 - alpha;
      double vtv = v0 * v0;
      for (int i = k + 1; i < m; ++i) {
        const double vi = a[static_cast<std::size_t>(i) * n + k];
        vtv += vi * vi;
      }
      if (!(vtv > 0.0) || !std::isfinite(vtv)) break;
      const double b = 2.0 / vtv;
      beta[static_cast<std::size_t>(k)] = b;
      pivnorm[static_cast<std::size_t>(k)] = xnorm;
      a[static_cast<std::size_t>(k) * n + k] = v0;   // 存反射子首元
      for (int l = k + 1; l < n; ++l) {
        double dot = v0 * a[static_cast<std::size_t>(k) * n + l];
        for (int i = k + 1; i < m; ++i)
          dot += a[static_cast<std::size_t>(i) * n + k] *
                 a[static_cast<std::size_t>(i) * n + l];
        const double s = b * dot;
        a[static_cast<std::size_t>(k) * n + l] -= s * v0;
        for (int i = k + 1; i < m; ++i)
          a[static_cast<std::size_t>(i) * n + l] -=
              s * a[static_cast<std::size_t>(i) * n + k];
      }
      ++rank;
    }
    return rank > 0;
  }

  // W（m×ncol, row-major）就地变为 Q'ᵀW（只应用 rank 个反射子）。
  void apply_qt(std::vector<double>* w, int ncol) const {
    for (int k = 0; k < rank; ++k) {
      const double v0 = a[static_cast<std::size_t>(k) * n + k];
      const double b = beta[static_cast<std::size_t>(k)];
      for (int c = 0; c < ncol; ++c) {
        double dot = v0 * (*w)[static_cast<std::size_t>(k) * ncol + c];
        for (int i = k + 1; i < m; ++i)
          dot += a[static_cast<std::size_t>(i) * n + k] *
                 (*w)[static_cast<std::size_t>(i) * ncol + c];
        const double s = b * dot;
        (*w)[static_cast<std::size_t>(k) * ncol + c] -= s * v0;
        for (int i = k + 1; i < m; ++i)
          (*w)[static_cast<std::size_t>(i) * ncol + c] -=
              s * a[static_cast<std::size_t>(i) * n + k];
      }
    }
  }
};

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

    // ── 球面 GLS: 秩亏安全的源空间投影（NOISE_SNR §11.4 裁决）────────────────
    // 被估计量 y = F h + Z β + n, h = R P, C = R V Rᵀ, V = diag(v_j)（v_j 已含
    // 冻结参考源 Poisson 与 blank-sky 分量, 与输入面同标度）。
    //
    // §11.4 行/零空间投影定理（可独立复算的代数恒等式）:
    //   Q = R D^{1/2}（nl×m）, W = D^{-1/2} A_src, A_src = [P, Z_planar]
    //   M = A_srcᵀ Rᵀ(RDRᵀ)⁺R A_src = Wᵀ V_r V_rᵀ W
    // V_r = Q 的行空间正交基 ⇒ 等价 M = WᵀW − (NᵀW)ᵀ(NᵀW)（N = 零空间基）。
    // 满列秩 (r=m) 时 V_rV_rᵀ = I ⇒ M = A_srcᵀD⁻¹A_src，与 §11.1 恒等式逐位一致。
    //
    // 数值 backend: 列主元 Householder QR 分解 B = Qᵀ = D^{1/2}Rᵀ（m×nl, 由 CSR
    // 三元组直接稠密组装）。range(B) = range(Qᵀ); 反射子隐式存于 B, 只需把它们作用
    // 到 W 上即得 W̃ = Q'ᵀW, 其前 r 行就是 V_rᵀW。成本 O(m²nl), 不组成条件数被
    // 平方的 Gram H=QᵀQ, 也不对其做几十轮循环 Jacobi（生产 m≈645/nl≈798 时后者
    // 单控制点约 19 s ⇒ 64 控制点约 20 min）。Jacobi 保留子空间路径保留为独立
    // 数值 oracle（spherical_reference_jacobi_oracle）供 verify 程序对拍。
    //
    // 背景冗余 ≠ 通量不可辨识（§11.4 末段）: 设 t = W̃[0:r,0]（通量列）、
    // Z_w = W̃[0:r,1:ncol]，正交投影 I_{F|β} = ‖(I−Π_Z)t‖², V_F = 1/I_{F|β}。
    // 只有通量响应整体落在背景列空间内才判 unavailable, 不因参数矩阵退化误判。
    // I_F 以显式正交残差 e=t−Π_Zt 的两轮重投影计算（§11.6 审查的消去误差修正;
    // 不得退回 ‖t‖²−‖Π_Zt‖² 相减）。‖t‖、‖e‖、有效背景秩与 rank 容差一并落盘。
    double v_f_pre = 0.0;         // 由本块直接给出 V_F（取代 invert_ff_entry）
    bool have_v_f = false;
    int cov_rank = 0;             // rank(C_sphere), 与 solver 诊断同源
    double cov_cond = kNaN;
    std::string cov_cond_method;  // 条件数估计方法（§11.6: 估计 ≠ κ₂(C) 本身）
    bool gls_ok = false;
    std::string solver_name;      // solver 身份（随 LocalWindow 落盘供审计）
    double solver_rel_tol = 0.0;
    int z_rank_out = 0;           // Z_w 有效秩（共线背景列被识别并记录）
    double i_f_out = 0.0;         // I_{F|β}
    double t_norm_out = 0.0;      // ‖t‖
    double e_norm_out = 0.0;      // ‖t − Π_Z t‖
    {
      const double rel_tol = (std::isfinite(cfg.support_rank_tol) &&
                              cfg.support_rank_tol > 0.0)
                                 ? cfg.support_rank_tol
                                 : 1e-12;
      // 稠密工作集预算（元素个数）。超出则退回已验证的 Jacobi 保留子空间 oracle:
      // 正确性优先于速度, 并在 solver 名字里如实反映（不冒充已用 QR）。
      const double kDenseQrBudget = 6.0e7;
      if (static_cast<double>(m) * static_cast<double>(nl) > kDenseQrBudget) {
        const JacobiOracleResult jr = spherical_reference_jacobi_oracle(w, n_bg, v);
        if (!jr.ok || !(jr.flux_variance > 0.0) ||
            !std::isfinite(jr.flux_variance)) {
          fail_point("operator_rank_deficient");
          out.precision.controls.push_back(ctrl);
          continue;
        }
        v_f_pre = jr.flux_variance;
        have_v_f = true;
        cov_rank = jr.covariance_rank;
        cov_cond = jr.condition_number;
        cov_cond_method = "retained_eigenvalue_ratio";
        solver_name = "jacobi_retained_subspace";
        solver_rel_tol = rel_tol;
      } else {
        PivotedQr qr;
        if (!qr.build_from_csr(v, w.op_pix, w.op_leaf, w.op_c,
                               static_cast<int>(m), nl, rel_tol) ||
            qr.rank <= 0 || !(qr.max_col_norm > 0.0)) {
          fail_point("operator_rank_deficient");
          out.precision.controls.push_back(ctrl);
          continue;
        }
        // W = D^{-1/2} A_src（m × ncol, row-major）
        std::vector<double> w_mat(static_cast<std::size_t>(m) * ncol, 0.0);
        for (int j = 0; j < static_cast<int>(m); ++j) {
          const double inv_sqrt = 1.0 / std::sqrt(v[static_cast<std::size_t>(j)]);
          w_mat[static_cast<std::size_t>(j) * ncol + 0] =
              w.profile[static_cast<std::size_t>(j)] * inv_sqrt;
          for (int c = 1; c < ncol; ++c) {
            const double planar =
                (c == 1) ? 1.0 : (c == 2) ? w.src_x[static_cast<std::size_t>(j)]
                                          : w.src_y[static_cast<std::size_t>(j)];
            w_mat[static_cast<std::size_t>(j) * ncol + c] = planar * inv_sqrt;
          }
        }
        qr.apply_qt(&w_mat, ncol);   // W̃ = Q'ᵀW
        const int r = qr.rank;
        cov_rank = r;
        double min_piv = qr.max_col_norm;
        for (int k = 0; k < r; ++k)
          min_piv = std::min(min_piv, qr.pivnorm[static_cast<std::size_t>(k)]);
        const double ratio = qr.max_col_norm / min_piv;
        // 条件数估计: (QR pivot 比)² —— 与 C 的 σ²max/σ²min 同口径的保守估计。
        // NOISE_SNR §11.6 审查: 这是**估计**（pivot 范数比平方）, 不等于严格谱
        // 条件数 κ₂(C); 估计方法名随值落盘, 不用它单独特判病态。
        cov_cond = std::isfinite(ratio) ? ratio * ratio : kNaN;
        cov_cond_method = "pivot_norm_ratio_squared";
        double t_norm2 = 0.0;
        for (int i = 0; i < r; ++i) {
          const double x = w_mat[static_cast<std::size_t>(i) * ncol + 0];
          t_norm2 += x * x;
        }
        // 对 Z_w 做两次修正 Gram-Schmidt ⇒ 列空间正交基（记录有效背景秩）
        std::vector<double> basis(static_cast<std::size_t>(r) *
                                      (ncol > 1 ? ncol - 1 : 1),
                                  0.0);
        int z_rank = 0;
        for (int c = 1; c < ncol; ++c) {
          std::vector<double> z(static_cast<std::size_t>(r), 0.0);
          double cn = 0.0;
          for (int i = 0; i < r; ++i) {
            const double x = w_mat[static_cast<std::size_t>(i) * ncol + c];
            z[static_cast<std::size_t>(i)] = x;
            cn += x * x;
          }
          if (!(cn > 0.0)) continue;
          const double cn0 = std::sqrt(cn);
          for (int pass = 0; pass < 2; ++pass) {
            for (int q = 0; q < z_rank; ++q) {
              double d = 0.0;
              for (int i = 0; i < r; ++i)
                d += z[static_cast<std::size_t>(i)] *
                     basis[static_cast<std::size_t>(q) * r + i];
              for (int i = 0; i < r; ++i)
                z[static_cast<std::size_t>(i)] -=
                    d * basis[static_cast<std::size_t>(q) * r + i];
            }
          }
          double rn = 0.0;
          for (double x : z) rn += x * x;
          rn = std::sqrt(rn);
          if (rn <= rel_tol * cn0) continue;   // 与既有背景列共线 ⇒ 非新方向
          for (int i = 0; i < r; ++i)
            basis[static_cast<std::size_t>(z_rank) * r + i] =
                z[static_cast<std::size_t>(i)] / rn;
          ++z_rank;
        }
        // I_{F|β} 的数值稳定计算（NOISE_SNR §11.6 第二轮审查修正）:
        // 旧实现以 ‖t‖² − Σ_q(t·q)² 两个近似相等的量相减, 当通量方向接近背景
        // 列空间时发生灾难性消去（相对误差 ~ε·‖t‖²/I_F 放大）。改为**显式构造
        // 正交残差** e = t − Π_Z t（对正交基做两轮投影 = 重正交, 防单轮舍入
        // 残留）, 直接求 ‖e‖²。数学等价, 近共线时保持满相对精度。
        std::vector<double> e(static_cast<std::size_t>(r), 0.0);
        for (int i = 0; i < r; ++i)
          e[static_cast<std::size_t>(i)] =
              w_mat[static_cast<std::size_t>(i) * ncol + 0];
        for (int pass = 0; pass < 2; ++pass) {
          for (int q = 0; q < z_rank; ++q) {
            double d = 0.0;
            for (int i = 0; i < r; ++i)
              d += e[static_cast<std::size_t>(i)] *
                   basis[static_cast<std::size_t>(q) * r + i];
            for (int i = 0; i < r; ++i)
              e[static_cast<std::size_t>(i)] -=
                  d * basis[static_cast<std::size_t>(q) * r + i];
          }
        }
        double e_norm2 = 0.0;
        for (double x : e) e_norm2 += x * x;
        const double i_f = e_norm2;
        // 可辨识判据: 残差相对 ‖t‖ 超过 rank 容差（相对量纲, 不随模板缩放漂移）。
        if (!std::isfinite(i_f) || !(i_f > rel_tol * std::max(t_norm2, 0.0))) {
          fail_point(ncol > 1 ? "flux_direction_inside_background_span"
                              : "flux_not_identifiable");
          out.precision.controls.push_back(ctrl);
          continue;
        }
        v_f_pre = 1.0 / i_f;
        have_v_f = true;
        solver_name = "pivoted_householder_qr_rowspace";
        solver_rel_tol = rel_tol;
        z_rank_out = z_rank;
        i_f_out = i_f;
        t_norm_out = std::sqrt(t_norm2);
        e_norm_out = std::sqrt(e_norm2);
      }
    }
    gls_ok = have_v_f;

    if (!gls_ok) {
      fail_point("operator_rank_deficient");
      out.precision.controls.push_back(ctrl);
      continue;
    }
    // V_F 由上面的源空间投影直接给出（I_{F|β} 取代参数信息矩阵求逆）;
    // 秩/条件数来自同一 solver（cov_rank / cov_cond）, 不再混用参数矩阵的秩。
    const double v_f = v_f_pre;
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
    // （detail §7.4: 与参数信息矩阵的秩分名, 不偷换意义）。condition_number 是
    // solver_condition_estimate_method 标注的**估计**（§11.6）; QR 路径的秩容差
    // 作用于 Q 的奇异值（pivot 列范数）, 与 Jacobi 路径的特征值尺度不可比。
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
    lw.solver = solver_name;
    lw.solver_rank = cov_rank;
    lw.solver_relative_tol = solver_rel_tol;
    lw.solver_condition_estimate_method = cov_cond_method;
    lw.solver_condition_number = std::isfinite(cov_cond) ? cov_cond : 0.0;
    lw.background_basis_rank = z_rank_out;
    lw.flux_information = i_f_out;
    lw.flux_response_norm = t_norm_out;
    lw.flux_residual_norm = e_norm_out;
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

// ── 独立数值 oracle（仅测试/诊断; 不在生产调用路径）────────────────────────
// 用循环 Jacobi 特征分解 + 显式 H⁺² 重算 M = A_srcᵀ Rᵀ(RDRᵀ)⁺R A_src, 再取
// [(M⁻¹)]_FF。与生产 QR 行空间投影是**不同代码路径**; 满列秩时两者同为
// A_srcᵀD⁻¹A_src（NOISE_SNR §11.1/§11.4）。verify 程序对拍二者。
JacobiOracleResult spherical_reference_jacobi_oracle(
    const PlanarPatchWindow& w, int n_bg, const std::vector<double>& v_override) {
  JacobiOracleResult out;
  const int m = static_cast<int>(w.pix_index.size());
  const int nl = static_cast<int>(w.leaf_ipix.size());
  const int ncol = 1 + n_bg;
  if (m <= 0 || nl <= 0 || w.op_c.size() != w.op_pix.size() ||
      w.op_c.size() != w.op_leaf.size()) {
    return out;
  }
  const std::vector<double>& v = v_override.empty() ? w.variance : v_override;
  if (v.size() != w.pix_index.size()) return out;
  for (double x : v) {
    if (!std::isfinite(x) || x <= 0.0) return out;
  }
  // H = V^{1/2} RᵀR V^{1/2}
  std::vector<double> g(static_cast<std::size_t>(m) * m, 0.0);
  {
    std::vector<std::vector<std::pair<int, double>>> per_leaf(
        static_cast<std::size_t>(nl));
    for (std::size_t k = 0; k < w.op_c.size(); ++k) {
      const int l = w.op_leaf[k];
      const int j = w.op_pix[k];
      if (l < 0 || l >= nl || j < 0 || j >= m) continue;
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
  std::vector<double> hmat = g;
  for (int j = 0; j < m; ++j) {
    const double sj = std::sqrt(v[static_cast<std::size_t>(j)]);
    for (int j2 = 0; j2 < m; ++j2)
      hmat[static_cast<std::size_t>(j) * m + j2] *=
          sj * std::sqrt(v[static_cast<std::size_t>(j2)]);
  }
  // A_src = [P, Z_planar]（m × ncol）
  std::vector<double> a_src(static_cast<std::size_t>(m) * ncol, 0.0);
  for (int j = 0; j < m; ++j) {
    a_src[static_cast<std::size_t>(j) * ncol + 0] =
        w.profile[static_cast<std::size_t>(j)];
    for (int c = 1; c < ncol; ++c) {
      a_src[static_cast<std::size_t>(j) * ncol + c] =
          (c == 1) ? 1.0
                   : (c == 2) ? w.src_x[static_cast<std::size_t>(j)]
                              : w.src_y[static_cast<std::size_t>(j)];
    }
  }
  // B = V^{1/2} G A_src —— retained_subspace_quadratic 的**第二参数必须是 B**,
  // 不是 A_src（NOISE_SNR §11.1: M = BᵀH⁺²B, H=V^{1/2}GV^{1/2}）。
  std::vector<double> bmat(static_cast<std::size_t>(m) * ncol, 0.0);
  for (int j = 0; j < m; ++j) {
    const double sj = std::sqrt(v[static_cast<std::size_t>(j)]);
    for (int c = 0; c < ncol; ++c) {
      double sum = 0.0;
      for (int j2 = 0; j2 < m; ++j2)
        sum += g[static_cast<std::size_t>(j) * m + j2] *
               a_src[static_cast<std::size_t>(j2) * ncol + c];
      bmat[static_cast<std::size_t>(j) * ncol + c] = sj * sum;
    }
  }
  std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
  int rank = 0;
  double cond = kNaN;
  if (!retained_subspace_quadratic(hmat, m, bmat, ncol, 1e-10, &mm, &rank,
                                   &cond)) {
    return out;
  }
  // Gauss-Jordan 反转 M 取 [0][0]（ncol ≤ 4; 与生产 QR 路径独立实现）
  double vf = 0.0;
  {
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
      if (!(best > 0.0)) return out;
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
    vf = aug[static_cast<std::size_t>(0) * 2 * k + k];
  }
  if (!std::isfinite(vf) || !(vf > 0.0)) return out;
  out.ok = true;
  out.flux_variance = vf;
  out.covariance_rank = rank;
  out.condition_number = std::isfinite(cond) ? cond : 0.0;
  return out;
}

}  // namespace acsd::normalize
