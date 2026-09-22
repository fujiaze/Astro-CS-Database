/* weight_chain.cpp — Phase2 SNR → 逆方差权重链实现（见 weight_chain.h 权威锚）
 *
 * 纯 FP64 + std；无第三方依赖。所有退化路径 fail-closed，无等权静默降级。
 */
#include "astrocs/v6/weight_chain.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace astrocs {
namespace v6 {
namespace p2weight {

namespace {

constexpr double kNodeReproductionTol = 1e-9;

bool positive_finite(double v) { return std::isfinite(v) && v > 0.0; }

WeightChainResult fail(WeightClosure closure, const std::string& err) {
  WeightChainResult r;
  r.ok = false;
  r.weight_chain_closed = false;
  r.production_allowed = false;
  r.closure = closure;
  r.error = err;
  r.weight_source = "none";
  return r;
}

/* 显式 legacy 请求：底层失败照实保留，但结果恒 fail-closed 并显式报「未闭合」。 */
WeightChainResult fail_with_policy(const WeightChainPolicy& policy,
                                   WeightClosure closure,
                                   const std::string& underlying,
                                   std::size_t n_frames) {
  if (!policy.legacy_allow_weight_fallback) return fail(closure, underlying);
  WeightChainResult r = fail(WeightClosure::kUnclosedLegacyFallbackRejected,
                             "weight chain NOT closed (权重链未闭合): " + underlying +
                                 "; legacy equal-weight fallback rejected "
                                 "(legacy_allow_weight_fallback=true does not produce a "
                                 "valid scientific weight chain)");
  r.legacy_equal_weight_used = true;
  r.diagnostic_equal_weights.assign(n_frames, 1.0);
  return r;
}

}  /* namespace */

const char* frame_snr_kind_token(FrameSnrKind k) {
  switch (k) {
    case FrameSnrKind::kFluxTypeUnweightedSnr: return "flux_type_unweighted_snr";
    case FrameSnrKind::kRelativeQualityWeight: return "relative_quality_weight";
    case FrameSnrKind::kDiagnosticMedianSnr: return "diagnostic_median_snr";
    case FrameSnrKind::kUnknown: return "unknown";
  }
  return "unknown";
}

const char* weight_closure_token(WeightClosure c) {
  switch (c) {
    case WeightClosure::kClosed: return "closed";
    case WeightClosure::kUnclosedMissingFrameSnr: return "unclosed_missing_frame_snr";
    case WeightClosure::kUnclosedInvalidFrameSnr: return "unclosed_invalid_frame_snr";
    case WeightClosure::kUnclosedInvalidReferenceFlux: return "unclosed_invalid_reference_flux";
    case WeightClosure::kUnclosedSparseLayerUnreconstructible:
      return "unclosed_sparse_layer_unreconstructible";
    case WeightClosure::kUnclosedInvalidIntraSnr: return "unclosed_invalid_intra_snr";
    case WeightClosure::kUnclosedNonFiniteWeight: return "unclosed_non_finite_weight";
    case WeightClosure::kUnclosedWrongSnrSemantics: return "unclosed_wrong_snr_semantics";
    case WeightClosure::kUnclosedLegacyFallbackRejected: return "unclosed_legacy_fallback_rejected";
    case WeightClosure::kUnclosedEmptyInput: return "unclosed_empty_input";
    case WeightClosure::kBaselineEqualWeight: return "baseline_equal_weight";
    case WeightClosure::kUnclosedMissingGain: return "unclosed_missing_gain";
    case WeightClosure::kUnclosedInvalidGain: return "unclosed_invalid_gain";
  }
  return "unknown";
}

bool weight_from_snr(double snr, double reference_flux, double* out_weight,
                     std::string* err) {
  auto set = [&](const char* m) { if (err) *err = m; };
  if (!positive_finite(reference_flux)) {
    set("reference_flux F_ref must be finite and > 0 (w = SNR^2 / F_ref^2 undefined)");
    return false;
  }
  if (!positive_finite(snr)) {
    set("SNR must be finite and > 0 (flux-type F_ref/sigma_F)");
    return false;
  }
  const double w = (snr / reference_flux) * (snr / reference_flux);
  if (!positive_finite(w)) {
    set("computed inverse-variance weight is non-finite/non-positive");
    return false;
  }
  if (out_weight) *out_weight = w;
  return true;
}

bool weight_from_corrected_variance(double variance, double* out_weight,
                                    std::string* err) {
  auto set = [&](const char* m) { if (err) *err = m; };
  if (!positive_finite(variance)) {
    set("weight_from_corrected_variance: Var(corrected) must be finite and > 0");
    return false;
  }
  const double w = 1.0 / variance;
  if (!positive_finite(w)) {
    set("weight_from_corrected_variance: 1/Var(corrected) non-finite/non-positive");
    return false;
  }
  if (out_weight) *out_weight = w;
  return true;
}

bool compose_actual_snr(double frame_snr, double intra_snr, double* out_snr,
                        std::string* err) {
  auto set = [&](const char* m) { if (err) *err = m; };
  if (!positive_finite(frame_snr)) {
    set("frame_snr must be finite and > 0");
    return false;
  }
  if (!positive_finite(intra_snr)) {
    set("intra-frame SNR factor must be finite and > 0");
    return false;
  }
  const double s = frame_snr * intra_snr;
  if (!positive_finite(s)) {
    set("composed actual SNR is non-finite/non-positive");
    return false;
  }
  if (out_snr) *out_snr = s;
  return true;
}

/* ------------------------------------------------------------------ */
/* 重建算子词表（冻结；算子标识 = 唯一配置面）                            */
/* ------------------------------------------------------------------ */
const char* sparse_recon_operator_token(SparseReconOperator op) {
  switch (op) {
    case SparseReconOperator::kNaturalBicubicSplineClip:
      return "natural_bicubic_spline_clip_v1";
    case SparseReconOperator::kNaturalBicubicSplineClipMeshMedian:
      return "natural_bicubic_spline_clip_mesh_median_v1";
    case SparseReconOperator::kBilinearRegularGrid:
      return "bilinear_regular_grid_v1";
    case SparseReconOperator::kNearestControlPoint:
      return "nearest_control_point_v1";
  }
  return "unknown";
}

const char* sparse_recon_operator_default_token() {
  return sparse_recon_operator_token(SparseReconOperator::kNaturalBicubicSplineClip);
}

bool parse_sparse_recon_operator(const std::string& token, SparseReconOperator* out) {
  const SparseReconOperator kAll[4] = {
      SparseReconOperator::kNaturalBicubicSplineClip,
      SparseReconOperator::kNaturalBicubicSplineClipMeshMedian,
      SparseReconOperator::kBilinearRegularGrid,
      SparseReconOperator::kNearestControlPoint,
  };
  for (const SparseReconOperator op : kAll) {
    if (token == sparse_recon_operator_token(op)) {
      if (out != nullptr) *out = op;
      return true;
    }
  }
  return false;
}

bool sparse_recon_operator_uses_mesh_median(SparseReconOperator op) {
  return op == SparseReconOperator::kNaturalBicubicSplineClipMeshMedian;
}

bool sparse_recon_operator_clips_to_ctrl_range(SparseReconOperator op) {
  return op == SparseReconOperator::kNaturalBicubicSplineClip ||
         op == SparseReconOperator::kNaturalBicubicSplineClipMeshMedian;
}

const char* sparse_recon_operator_for_source(bool high_contrast_unresolved_sources) {
  return sparse_recon_operator_token(
      high_contrast_unresolved_sources
          ? SparseReconOperator::kNaturalBicubicSplineClipMeshMedian
          : SparseReconOperator::kNaturalBicubicSplineClip);
}

bool reconstruct_sparse_snr(const SparseSnrLayer& layer, double x, double y,
                            double* out_snr, SparseReconstruction* info,
                            std::string* err) {
  SparseSnrReconstructor rec;
  if (!rec.prepare(layer, err)) return false;
  return rec.eval(x, y, out_snr, info, err);
}

/* ------------------------------------------------------------------ */
/* 重建预处理与求值（自然边界双三次样条 + 值域钳制 + 可选 mesh 中值）      */
/* ------------------------------------------------------------------ */
namespace {

/* 自然边界三次样条二阶导（节点等距 h=1，M[0]=M[n-1]=0）：
 *   M[i-1] + 4 M[i] + M[i+1] = 6 (y[i+1] - 2 y[i] + y[i-1])
 * 与实验 EXP-04 operators.py::_nat_second_deriv 及 SExtractor 2.28.2
 * makebackspline（natural BC）同式；n<3 退化为直线（M≡0）。 */
void natural_second_deriv_1d(const double* y, int n, double* M) {
  for (int i = 0; i < n; ++i) M[i] = 0.0;
  if (n < 3) return;
  const int m = n - 2;
  std::vector<double> cp(static_cast<std::size_t>(m), 0.0);
  std::vector<double> dp(static_cast<std::size_t>(m), 0.0);
  const double b0 = 4.0;
  cp[0] = 1.0 / b0;
  dp[0] = 6.0 * (y[2] - 2.0 * y[1] + y[0]) / b0;
  for (int i = 1; i < m; ++i) {
    const double denom = 4.0 - cp[static_cast<std::size_t>(i - 1)];
    cp[static_cast<std::size_t>(i)] = 1.0 / denom;
    const double rhs = 6.0 * (y[i + 2] - 2.0 * y[i + 1] + y[i]);
    dp[static_cast<std::size_t>(i)] =
        (rhs - dp[static_cast<std::size_t>(i - 1)]) / denom;
  }
  M[n - 2] = dp[static_cast<std::size_t>(m - 1)];
  for (int i = m - 2; i >= 0; --i) {
    M[i + 1] = dp[static_cast<std::size_t>(i)] -
               cp[static_cast<std::size_t>(i)] * M[i + 2];
  }
}

/* 自然边界三次样条求值（pos 须已钳到 [0, n-1]；与 EXP-04 _nat_eval 同式）。
 * stride = 相邻节点在内存中的步长（y 向求值时网格按行主序存储，列方向有步长）。 */
double natural_eval_1d_strided(const double* y, const double* M, int n,
                               std::size_t stride, double pos) {
  if (n == 1) return y[0];
  int i = static_cast<int>(std::floor(pos));
  if (i < 0) i = 0;
  if (i > n - 2) i = n - 2;
  double h = pos - static_cast<double>(i);
  if (h < 0.0) h = 0.0;
  if (h > 1.0) h = 1.0;
  const double y0 = y[static_cast<std::size_t>(i) * stride];
  const double y1 = y[static_cast<std::size_t>(i + 1) * stride];
  const double m0 = M[static_cast<std::size_t>(i) * stride];
  const double m1 = M[static_cast<std::size_t>(i + 1) * stride];
  const double b = (y1 - y0) - (2.0 * m0 + m1) / 6.0;
  return y0 + b * h + m0 * h * h / 2.0 + (m1 - m0) * h * h * h / 6.0;
}

double natural_eval_1d(const double* y, const double* M, int n, double pos) {
  return natural_eval_1d_strided(y, M, n, 1, pos);
}

/* 3×3 mesh 中值前置滤波（边界 replicate，无条件替换）。
 * 语义 = SExtractor 2.28.2 filterback（BACK_FILTTHRESH 默认 0.0 ⇒ 恒生效）
 * 与 photutils 2.2.0 filter_size=(3,3)。 */
std::vector<double> median3_mesh(const std::vector<double>& g, int nx, int ny) {
  std::vector<double> out(g.size(), 0.0);
  double win[9];
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      int k = 0;
      for (int dj = -1; dj <= 1; ++dj) {
        int jj = j + dj;
        if (jj < 0) jj = 0;
        if (jj > ny - 1) jj = ny - 1;
        for (int di = -1; di <= 1; ++di) {
          int ii = i + di;
          if (ii < 0) ii = 0;
          if (ii > nx - 1) ii = nx - 1;
          win[k++] = g[static_cast<std::size_t>(jj) * static_cast<std::size_t>(nx) +
                       static_cast<std::size_t>(ii)];
        }
      }
      std::sort(win, win + 9);
      out[static_cast<std::size_t>(j) * static_cast<std::size_t>(nx) +
          static_cast<std::size_t>(i)] = win[4];
    }
  }
  return out;
}

/* NaN（= schema 声明的 invalid 表示）控制点按「最近有效控制点、等距并列取平均」
 * 填充（SExtractor back.c bad-mesh 语义）。返回 false = 全部无效（fail-closed）。
 * 非 NaN 的非有限值 / <=0 是**非法值**（不是 invalid 表示），由调用方 fail-closed。 */
bool nearest_valid_fill(std::vector<double>& g, int nx, int ny, std::size_t* n_filled) {
  /* 填充基准 = **原始**有效控制点集合（不是被就地填充后的网格），
     否则结果依赖遍历顺序（先填的点会成为后填点的"有效邻居"）。 */
  std::vector<double> good_val;
  std::vector<int> good_i;
  std::vector<int> good_j;
  std::size_t n_bad = 0;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const double v = g[static_cast<std::size_t>(j) * static_cast<std::size_t>(nx) +
                         static_cast<std::size_t>(i)];
      if (std::isnan(v)) {
        ++n_bad;
      } else {
        good_val.push_back(v);
        good_i.push_back(i);
        good_j.push_back(j);
      }
    }
  }
  if (n_filled != nullptr) *n_filled = n_bad;
  if (n_bad == 0) return true;
  if (good_val.empty()) return false;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const std::size_t idx = static_cast<std::size_t>(j) * static_cast<std::size_t>(nx) +
                              static_cast<std::size_t>(i);
      if (!std::isnan(g[idx])) continue;
      double best = std::numeric_limits<double>::infinity();
      double sum = 0.0;
      std::size_t cnt = 0;
      for (std::size_t k = 0; k < good_val.size(); ++k) {
        const double dy = static_cast<double>(good_j[k] - j);
        const double dx = static_cast<double>(good_i[k] - i);
        const double d2 = dx * dx + dy * dy;
        if (d2 < best - 1e-12) {
          best = d2;
          sum = good_val[k];
          cnt = 1;
        } else if (d2 <= best + 1e-12) {
          sum += good_val[k];
          ++cnt;
        }
      }
      g[idx] = sum / static_cast<double>(cnt);
    }
  }
  return true;
}

}  /* namespace */

void SparseSnrReconstructor::fill_info(SparseReconstruction* info) const {
  if (info == nullptr) return;
  info->operator_id = ready_ ? sparse_recon_operator_token(op_) : std::string();
  info->n_control_points = n_control_points_;
  info->node_reproduction_max_abs = node_residual_;
  info->out_of_domain = false;
  info->n_invalid_control_points_filled = n_filled_;
  info->mesh_median_applied = mesh_median_;
  info->value_range_clipped = clips_;
  info->clip_low = clips_ ? clip_low_ : 0.0;
  info->clip_high = clips_ ? clip_high_ : 0.0;
  info->cell_center_offset_max_abs = cell_center_offset_;
}

bool SparseSnrReconstructor::prepare(const SparseSnrLayer& layer, std::string* err) {
  auto set = [&](const std::string& m) { if (err) *err = m; };
  ready_ = false;
  regular_grid_ = false;
  nx_ = 0;
  ny_ = 0;
  grid_.clear();
  my_.clear();
  points_.clear();
  clips_ = false;
  mesh_median_ = false;
  clip_low_ = 0.0;
  clip_high_ = 0.0;
  node_residual_ = 0.0;
  cell_center_offset_ = 0.0;
  n_filled_ = 0;
  n_control_points_ = 0;

  if (!layer.present) {
    set("sparse SNR layer not present");
    return false;
  }
  if (layer.points.empty()) {
    set("sparse SNR layer present but empty (corrupt/unreconstructible)");
    return false;
  }
  n_control_points_ = layer.points.size();
  points_ = layer.points;
  regular_grid_ = layer.regular_grid;
  tol_ = std::max(0.0, layer.grid_tol);
  max_radius_px_ = layer.max_radius_px;

  /* 算子解析：未识别 token ⇒ fail-closed（不得静默回退默认）。 */
  SparseReconOperator declared = SparseReconOperator::kNaturalBicubicSplineClip;
  const bool has_decl = !layer.reconstruction_operator.empty();
  if (has_decl &&
      !parse_sparse_recon_operator(layer.reconstruction_operator, &declared)) {
    set("unknown reconstruction_operator token '" + layer.reconstruction_operator +
        "' (fail-closed; no silent fallback to the default operator)");
    return false;
  }
  if (regular_grid_) {
    op_ = has_decl ? declared : SparseReconOperator::kNaturalBicubicSplineClip;
    if (op_ == SparseReconOperator::kNearestControlPoint) {
      set("reconstruction_operator nearest_control_point_v1 is scattered-mode only "
          "(regular_grid=true)");
      return false;
    }
  } else {
    op_ = has_decl ? declared : SparseReconOperator::kNearestControlPoint;
    if (op_ != SparseReconOperator::kNearestControlPoint) {
      set(std::string("reconstruction_operator ") + sparse_recon_operator_token(op_) +
          " requires regular_grid=true");
      return false;
    }
  }
  mesh_median_ = sparse_recon_operator_uses_mesh_median(op_);
  clips_ = sparse_recon_operator_clips_to_ctrl_range(op_);

  if (!regular_grid_) {
    /* 散点模式：最近控制点，必须显式声明覆盖半径（禁隐式外推）。 */
    if (!positive_finite(max_radius_px_)) {
      set("scattered sparse layer requires explicit max_radius_px > 0 "
          "(no implicit extrapolation / frame-level fallback)");
      return false;
    }
    for (const auto& p : points_) {
      if (!positive_finite(p.snr)) {
        set("sparse layer control point SNR non-finite/non-positive (corrupt)");
        return false;
      }
    }
    ready_ = true;
    return true;
  }

  /* ---- 规则网格：几何 + 值语义校验 ---- */
  if (layer.nx < 2 || layer.ny < 2) {
    set("regular-grid sparse layer needs nx>=2 and ny>=2 for interpolation");
    return false;
  }
  if (!positive_finite(layer.dx) || !positive_finite(layer.dy) ||
      !std::isfinite(layer.x0) || !std::isfinite(layer.y0)) {
    set("regular-grid sparse layer geometry invalid (x0/y0 finite, dx/dy > 0 required)");
    return false;
  }
  if (!std::isfinite(layer.grid_origin_x) || !std::isfinite(layer.grid_origin_y)) {
    set("regular-grid sparse layer cell origin invalid (grid_origin_x/y must be finite)");
    return false;
  }
  const std::size_t nxl = static_cast<std::size_t>(layer.nx);
  const std::size_t nyl = static_cast<std::size_t>(layer.ny);
  if (points_.size() != nxl * nyl) {
    set("regular-grid sparse layer point count != nx*ny (corrupt)");
    return false;
  }
  nx_ = layer.nx;
  ny_ = layer.ny;
  x0_ = layer.x0;
  y0_ = layer.y0;
  dx_ = layer.dx;
  dy_ = layer.dy;
  grid_origin_x_ = layer.grid_origin_x;
  grid_origin_y_ = layer.grid_origin_y;
  grid_.assign(nxl * nyl, 0.0);
  double cc_off = 0.0;
  for (int j = 0; j < ny_; ++j) {
    for (int i = 0; i < nx_; ++i) {
      const std::size_t idx = static_cast<std::size_t>(j) * nxl + static_cast<std::size_t>(i);
      const SparseSnrPoint& p = points_[idx];
      const double ex = x0_ + static_cast<double>(i) * dx_;
      const double ey = y0_ + static_cast<double>(j) * dy_;
      if (std::fabs(p.x - ex) > tol_ || std::fabs(p.y - ey) > tol_) {
        set("regular-grid sparse layer control point does not match declared grid (corrupt)");
        return false;
      }
      /* 非法值：非 NaN 的非有限值 / <=0（0 与负值不是合法 SNR）。 */
      if (!std::isnan(p.snr) && !positive_finite(p.snr)) {
        set("sparse layer control point SNR non-finite/non-positive (corrupt)");
        return false;
      }
      grid_[idx] = p.snr;
      /* cell 中心门：节点必须落在所属 cell 的中心（见 weight_chain.h 几何约定）。 */
      const double cx = layer.grid_origin_x + static_cast<double>(i) * dx_ + (dx_ - 1.0) / 2.0;
      const double cy = layer.grid_origin_y + static_cast<double>(j) * dy_ + (dy_ - 1.0) / 2.0;
      cc_off = std::max(cc_off, std::max(std::fabs(p.x - cx), std::fabs(p.y - cy)));
    }
  }
  cell_center_offset_ = cc_off;
  const double tol_geom = std::max(tol_, 1e-9);
  if (!(cc_off <= tol_geom)) {
    set("regular-grid sparse layer control points are not at their cell centers "
        "(max offset " + std::to_string(cc_off) +
        " px; a corner-anchored grid shifts the whole field by half a cell; "
        "require x0 = grid_origin_x + (dx-1)/2)");
    return false;
  }

  /* NaN（schema 声明的 invalid 表示）按最近有效控制点填充；全无效 ⇒ fail-closed。 */
  if (!nearest_valid_fill(grid_, nx_, ny_, &n_filled_)) {
    set("all sparse layer control points are invalid (NaN); nothing to reconstruct");
    return false;
  }
  if (mesh_median_) grid_ = median3_mesh(grid_, nx_, ny_);
  {
    double lo = grid_[0];
    double hi = grid_[0];
    for (const double v : grid_) {
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    clip_low_ = lo;
    clip_high_ = hi;
  }
  if (!positive_finite(clip_low_)) {
    set("sparse layer value range non-positive (corrupt)");
    return false;
  }

  /* y 向自然样条二阶导预计算（列主序存储为 my_[j*nx+i]）。 */
  if (op_ != SparseReconOperator::kBilinearRegularGrid) {
    my_.assign(nxl * nyl, 0.0);
    std::vector<double> col(nyl, 0.0);
    std::vector<double> mcol(nyl, 0.0);
    for (int i = 0; i < nx_; ++i) {
      for (int j = 0; j < ny_; ++j) {
        col[static_cast<std::size_t>(j)] =
            grid_[static_cast<std::size_t>(j) * nxl + static_cast<std::size_t>(i)];
      }
      natural_second_deriv_1d(col.data(), ny_, mcol.data());
      for (int j = 0; j < ny_; ++j) {
        my_[static_cast<std::size_t>(j) * nxl + static_cast<std::size_t>(i)] =
            mcol[static_cast<std::size_t>(j)];
      }
    }
  }

  /* 控制点自身复现自检：在节点坐标处经**同一求值路径**应精确复现节点值；
     索引/几何错位或 NaN 会在此放大为红灯（fail-closed）。 */
  ready_ = true;
  double max_resid = 0.0;
  for (int j = 0; j < ny_; ++j) {
    for (int i = 0; i < nx_; ++i) {
      const std::size_t idx = static_cast<std::size_t>(j) * nxl + static_cast<std::size_t>(i);
      double vv = 0.0;
      if (!eval(x0_ + static_cast<double>(i) * dx_, y0_ + static_cast<double>(j) * dy_,
                &vv, nullptr, nullptr)) {
        ready_ = false;
        set("sparse layer control-point reproduction failed (corrupt/unreconstructible)");
        return false;
      }
      max_resid = std::max(max_resid, std::fabs(vv - grid_[idx]));
    }
  }
  node_residual_ = max_resid;
  if (!(max_resid <= kNodeReproductionTol)) {
    ready_ = false;
    set("sparse layer control-point reproduction residual exceeds tolerance "
        "(corrupt/unreconstructible)");
    return false;
  }
  return true;
}

bool SparseSnrReconstructor::eval(double x, double y, double* out_snr,
                                  SparseReconstruction* info, std::string* err) const {
  auto set = [&](const std::string& m) { if (err) *err = m; };
  if (!ready_) {
    set("sparse SNR reconstructor not prepared");
    return false;
  }
  SparseReconstruction local;
  fill_info(&local);

  if (!regular_grid_) {
    /* 散点模式：最近控制点 + 显式覆盖半径（禁隐式外推）。 */
    double best_d2 = std::numeric_limits<double>::infinity();
    double best_v = 0.0;
    bool best_ok = false;
    for (const auto& p : points_) {
      const double ddx = x - p.x;
      const double ddy = y - p.y;
      const double d2 = ddx * ddx + ddy * ddy;
      if (d2 < best_d2) { best_d2 = d2; best_v = p.snr; best_ok = true; }
    }
    if (!best_ok) {
      set("scattered sparse layer has no usable control point");
      return false;
    }
    const double r = max_radius_px_;
    if (best_d2 > r * r * (1.0 + 1e-12)) {
      local.out_of_domain = true;
      if (info) *info = local;
      set("query pixel farther than declared max_radius_px from any control point "
          "(no extrapolation; frame-level fallback forbidden)");
      return false;
    }
    if (info) *info = local;
    if (out_snr) *out_snr = best_v;
    return true;
  }

  const std::size_t nxl = static_cast<std::size_t>(nx_);
  const double hi_x = static_cast<double>(nx_ - 1);
  const double hi_y = static_cast<double>(ny_ - 1);
  /* 定义域 = 层覆盖的 cell 并集（不是节点张成的区间）：cell i 覆盖像素中心坐标
     [origin + i*dx - 0.5, origin + (i+1)*dx - 0.5]；节点在 cell 中心。cell 内
     非节点处由插值给出，最外半个 cell 由端点节点常数延拓（= EXP-04 的
     clip(pos, 0, n-1)，与 SExtractor/photutils 的 mesh 背景覆盖整帧同语义）。 */
  const double gx = (x - x0_) / dx_;
  const double gy = (y - y0_) / dy_;
  const double dom_lo_x = grid_origin_x_ - 0.5;
  const double dom_hi_x = grid_origin_x_ + static_cast<double>(nx_) * dx_ - 0.5;
  const double dom_lo_y = grid_origin_y_ - 0.5;
  const double dom_hi_y = grid_origin_y_ + static_cast<double>(ny_) * dy_ - 0.5;
  if (!std::isfinite(gx) || !std::isfinite(gy) ||
      x < dom_lo_x - tol_ || x > dom_hi_x + tol_ ||
      y < dom_lo_y - tol_ || y > dom_hi_y + tol_) {
    local.out_of_domain = true;
    if (info) *info = local;
    set("query pixel outside sparse SNR layer domain (no extrapolation; "
        "frame-level fallback forbidden)");
    return false;
  }
  const double cx = std::min(std::max(gx, 0.0), hi_x);
  const double cy = std::min(std::max(gy, 0.0), hi_y);

  double v = 0.0;
  if (op_ == SparseReconOperator::kBilinearRegularGrid) {
    int i0 = static_cast<int>(std::floor(cx));
    int j0 = static_cast<int>(std::floor(cy));
    i0 = std::min(std::max(i0, 0), nx_ - 2);
    j0 = std::min(std::max(j0, 0), ny_ - 2);
    const double fx = cx - static_cast<double>(i0);
    const double fy = cy - static_cast<double>(j0);
    const double a = grid_[static_cast<std::size_t>(j0) * nxl + static_cast<std::size_t>(i0)];
    const double b = grid_[static_cast<std::size_t>(j0) * nxl + static_cast<std::size_t>(i0 + 1)];
    const double c = grid_[static_cast<std::size_t>(j0 + 1) * nxl + static_cast<std::size_t>(i0)];
    const double d =
        grid_[static_cast<std::size_t>(j0 + 1) * nxl + static_cast<std::size_t>(i0 + 1)];
    v = (1.0 - fx) * (1.0 - fy) * a + fx * (1.0 - fy) * b +
        (1.0 - fx) * fy * c + fx * fy * d;
  } else {
    /* 可分离自然边界双三次样条：y 向（用预计算二阶导）→ x 向。 */
    constexpr int kStackCap = 512;
    double stack_tmp[kStackCap];
    double stack_mx[kStackCap];
    std::vector<double> heap_tmp;
    std::vector<double> heap_mx;
    double* tmp = stack_tmp;
    double* mx = stack_mx;
    if (nx_ > kStackCap) {
      heap_tmp.resize(nxl);
      heap_mx.resize(nxl);
      tmp = heap_tmp.data();
      mx = heap_mx.data();
    }
    for (int i = 0; i < nx_; ++i) {
      const std::size_t col0 = static_cast<std::size_t>(i);
      tmp[i] = natural_eval_1d_strided(&grid_[col0], &my_[col0], ny_, nxl, cy);
    }
    natural_second_deriv_1d(tmp, nx_, mx);
    v = natural_eval_1d(tmp, mx, nx_, cx);
  }
  if (clips_) {
    v = std::min(std::max(v, clip_low_), clip_high_);
  }
  if (info) *info = local;
  if (!positive_finite(v)) {
    set("reconstructed intra-frame SNR non-finite/non-positive");
    return false;
  }
  if (out_snr) *out_snr = v;
  return true;
}

WeightChainResult compute_inverse_variance_weights(
    const std::vector<FrameWeightInput>& frames, double reference_flux,
    const WeightChainPolicy& policy) {
  const std::size_t n = frames.size();
  if (n == 0) {
    return fail_with_policy(policy, WeightClosure::kUnclosedEmptyInput,
                            "no input frames", n);
  }
  if (!positive_finite(reference_flux)) {
    return fail_with_policy(policy, WeightClosure::kUnclosedInvalidReferenceFlux,
                            "reference_flux F_ref must be finite and > 0", n);
  }

  WeightChainResult r;
  r.reference_flux = reference_flux;
  r.reference_flux_k.assign(n, reference_flux);
  r.intra_snr.assign(n, 1.0);
  r.actual_snr.assign(n, 0.0);
  r.weights.assign(n, 0.0);
  r.sparse_operator_ids.assign(n, std::string());
  r.sparse_node_residual.assign(n, 0.0);
  r.frame_gain.assign(n, 1.0);

  bool any_sparse = false;
  for (std::size_t k = 0; k < n; ++k) {
    const FrameWeightInput& f = frames[k];
    const std::string tag =
        f.frame_id.empty() ? ("frame#" + std::to_string(k)) : f.frame_id;

    if (f.kind != FrameSnrKind::kFluxTypeUnweightedSnr) {
      return fail_with_policy(
          policy, WeightClosure::kUnclosedWrongSnrSemantics,
          tag + ": frame_snr semantics '" + frame_snr_kind_token(f.kind) +
              "' is not the flux-type unweighted raw SNR (F_ref/sigma_F); "
              "relative quality weights / median diagnostics cannot enter the weight chain",
          n);
    }
    if (!f.has_frame_snr) {
      return fail_with_policy(policy, WeightClosure::kUnclosedMissingFrameSnr,
                              tag + ": frame-level SNR missing (HiPS header has no "
                                    "flux-type frame_snr_value)",
                              n);
    }
    if (!positive_finite(f.frame_snr)) {
      return fail_with_policy(policy, WeightClosure::kUnclosedInvalidFrameSnr,
                              tag + ": frame-level SNR non-finite/non-positive", n);
    }

    double intra = 1.0;
    if (f.sparse != nullptr && f.sparse->present) {
      double v = 0.0;
      SparseReconstruction info;
      std::string serr;
      if (!reconstruct_sparse_snr(*f.sparse, f.x, f.y, &v, &info, &serr)) {
        return fail_with_policy(
            policy, WeightClosure::kUnclosedSparseLayerUnreconstructible,
            tag + ": sparse intra-frame SNR layer present but unreconstructible: " + serr,
            n);
      }
      if (!positive_finite(v)) {
        return fail_with_policy(policy, WeightClosure::kUnclosedInvalidIntraSnr,
                                tag + ": reconstructed intra-frame SNR non-finite/non-positive",
                                n);
      }
      intra = v;
      r.sparse_operator_ids[k] = info.operator_id;
      r.sparse_node_residual[k] = info.node_reproduction_max_abs;
      any_sparse = true;
    }
    r.intra_snr[k] = intra;

    double actual = 0.0;
    std::string cerr;
    if (!compose_actual_snr(f.frame_snr, intra, &actual, &cerr)) {
      return fail_with_policy(policy, WeightClosure::kUnclosedInvalidIntraSnr,
                              tag + ": " + cerr, n);
    }
    r.actual_snr[k] = actual;

    /* 乘性光度响应 g_k（可空）。归一化 corrected=(y−ĝ)/g_k ⇒ Var(corrected)=
       Var(y)/g_k² ⇒ 帧级权重乘 g_k²（q2-snr-smooth §2/§7）。声明要求而缺
       → fail-closed，不得静默按 g=1 冒充。 */
    double g = 1.0;
    if (f.gain != nullptr) {
      if (!positive_finite(*f.gain)) {
        return fail_with_policy(
            policy, WeightClosure::kUnclosedInvalidGain,
            tag + ": frame gain g_k must be finite and > 0 (multiplicative "
                  "normalization corrected=(y-ĝ)/g_k)", n);
      }
      g = *f.gain;
    } else if (policy.require_frame_gain) {
      return fail_with_policy(
          policy, WeightClosure::kUnclosedMissingGain,
          tag + ": frame gain g_k missing but policy.require_frame_gain=true "
                "(w = SNR²/F_ref²·g_k² undefined; no silent g=1 fallback)", n);
    }
    r.frame_gain[k] = g;

    /* 参考通量：**优先用本帧自己的 F_ref,k**（f.ref_flux>0），否则回退组标量。
     * 配对性定理只要求同一帧内 SNR 与 F_ref 同源；跨帧相等**不是**要求
     * （负责人 GAP_AUDIT §9.49 定案 2：帧间独立、不同光学系统混装不得报错）。
     * 旧实现强制组内公共 F_ref ⇒ 多指向/多光学系统拼接时 weight_mode=2
     * 完全不可用（实测 6/6 帧被拒）。 */
    const double f_ref_k = (f.ref_flux > 0.0) ? f.ref_flux : reference_flux;
    if (!positive_finite(f_ref_k)) {
      return fail_with_policy(
          policy, WeightClosure::kUnclosedInvalidReferenceFlux,
          tag + ": per-frame reference flux F_ref,k must be finite and > 0", n);
    }
    r.reference_flux_k[k] = f_ref_k;
    double w = 0.0;
    std::string werr;
    if (!weight_from_snr(actual, f_ref_k, &w, &werr)) {
      return fail_with_policy(policy, WeightClosure::kUnclosedNonFiniteWeight,
                              tag + ": " + werr, n);
    }
    w *= g * g;
    if (!positive_finite(w)) {
      return fail_with_policy(
          policy, WeightClosure::kUnclosedNonFiniteWeight,
          tag + ": SNR²/F_ref²·g_k² non-finite/non-positive after gain", n);
    }
    r.weights[k] = w;
  }

  r.ok = true;
  r.weight_chain_closed = true;
  r.production_allowed = true;
  r.closure = WeightClosure::kClosed;
  r.weight_source = any_sparse ? "frame_snr_x_sparse_snr" : "frame_snr";
  return r;
}

WeightChainResult make_equal_weight_baseline(std::size_t n_frames) {
  WeightChainResult r;
  r.ok = false; /* 显式基线，绝不冒充权重链闭合 */
  r.weight_chain_closed = false;
  r.production_allowed = false;
  r.closure = WeightClosure::kBaselineEqualWeight;
  r.error =
      "explicit non-production equal-weight baseline (not a closed inverse-variance "
      "SNR weight chain)";
  r.weight_source = "equal_weight_baseline";
  r.weights.assign(n_frames, 1.0);
  r.intra_snr.assign(n_frames, 1.0);
  r.actual_snr.assign(n_frames, 0.0);
  r.frame_gain.assign(n_frames, 1.0);
  r.sparse_operator_ids.assign(n_frames, std::string());
  r.sparse_node_residual.assign(n_frames, 0.0);
  return r;
}

}  /* namespace p2weight */
}  /* namespace v6 */
}  /* namespace astrocs */
