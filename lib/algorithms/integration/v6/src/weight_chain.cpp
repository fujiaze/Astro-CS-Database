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

bool reconstruct_sparse_snr(const SparseSnrLayer& layer, double x, double y,
                            double* out_snr, SparseReconstruction* info,
                            std::string* err) {
  auto set = [&](const std::string& m) { if (err) *err = m; };
  SparseReconstruction local;
  if (info) *info = local;

  if (!layer.present) {
    set("sparse SNR layer not present");
    return false;
  }
  if (layer.points.empty()) {
    set("sparse SNR layer present but empty (corrupt/unreconstructible)");
    return false;
  }
  local.n_control_points = layer.points.size();

  if (layer.regular_grid) {
    if (layer.nx < 2 || layer.ny < 2) {
      set("regular-grid sparse layer needs nx>=2 and ny>=2 for bilinear interpolation");
      return false;
    }
    if (!positive_finite(layer.dx) || !positive_finite(layer.dy) ||
        !std::isfinite(layer.x0) || !std::isfinite(layer.y0)) {
      set("regular-grid sparse layer geometry invalid (x0/y0 finite, dx/dy > 0 required)");
      return false;
    }
    const std::size_t expected =
        static_cast<std::size_t>(layer.nx) * static_cast<std::size_t>(layer.ny);
    if (layer.points.size() != expected) {
      set("regular-grid sparse layer point count != nx*ny (corrupt)");
      return false;
    }
    const double tol = std::max(0.0, layer.grid_tol);
    for (int j = 0; j < layer.ny; ++j) {
      for (int i = 0; i < layer.nx; ++i) {
        const SparseSnrPoint& p =
            layer.points[static_cast<std::size_t>(j) * static_cast<std::size_t>(layer.nx) +
                         static_cast<std::size_t>(i)];
        const double ex = layer.x0 + static_cast<double>(i) * layer.dx;
        const double ey = layer.y0 + static_cast<double>(j) * layer.dy;
        if (std::fabs(p.x - ex) > tol || std::fabs(p.y - ey) > tol) {
          set("regular-grid sparse layer control point does not match declared grid (corrupt)");
          return false;
        }
        if (!positive_finite(p.snr)) {
          set("sparse layer control point SNR non-finite/non-positive (corrupt)");
          return false;
        }
      }
    }
    const std::size_t nxl = static_cast<std::size_t>(layer.nx);
    const double hi_x = static_cast<double>(layer.nx - 1);
    const double hi_y = static_cast<double>(layer.ny - 1);
    /* 双线性求值（cx/cy 须在 [0, hi] 内；边界节点由索引夹取精确复现）。 */
    auto eval_bilinear = [&](double cx, double cy) -> double {
      int i0 = static_cast<int>(std::floor(cx));
      int j0 = static_cast<int>(std::floor(cy));
      i0 = std::min(std::max(i0, 0), layer.nx - 2);
      j0 = std::min(std::max(j0, 0), layer.ny - 2);
      const double fx = cx - static_cast<double>(i0);
      const double fy = cy - static_cast<double>(j0);
      const double a = layer.points[static_cast<std::size_t>(j0) * nxl +
                                    static_cast<std::size_t>(i0)].snr;
      const double b = layer.points[static_cast<std::size_t>(j0) * nxl +
                                    static_cast<std::size_t>(i0 + 1)].snr;
      const double c = layer.points[static_cast<std::size_t>(j0 + 1) * nxl +
                                    static_cast<std::size_t>(i0)].snr;
      const double d = layer.points[static_cast<std::size_t>(j0 + 1) * nxl +
                                    static_cast<std::size_t>(i0 + 1)].snr;
      return (1.0 - fx) * (1.0 - fy) * a + fx * (1.0 - fy) * b +
             (1.0 - fx) * fy * c + fx * fy * d;
    };
    /* 控制点自身复现自检：在节点坐标处经**同一求值路径**应精确复现节点值；
       索引/几何错位或 NaN 会在此放大为红灯（fail-closed）。 */
    double max_resid = 0.0;
    for (int j = 0; j < layer.ny; ++j) {
      for (int i = 0; i < layer.nx; ++i) {
        const double vv = layer.points[static_cast<std::size_t>(j) * nxl +
                                       static_cast<std::size_t>(i)].snr;
        const double rr = eval_bilinear(static_cast<double>(i), static_cast<double>(j));
        max_resid = std::max(max_resid, std::fabs(rr - vv));
      }
    }
    local.node_reproduction_max_abs = max_resid;
    local.operator_id = "bilinear_regular_grid_v1";
    if (!(max_resid <= kNodeReproductionTol)) {
      if (info) *info = local;
      set("sparse layer control-point reproduction residual exceeds tolerance "
          "(corrupt/unreconstructible)");
      return false;
    }

    const double gx = (x - layer.x0) / layer.dx;
    const double gy = (y - layer.y0) / layer.dy;
    if (!std::isfinite(gx) || !std::isfinite(gy) ||
        gx < -tol || gx > hi_x + tol || gy < -tol || gy > hi_y + tol) {
      local.out_of_domain = true;
      if (info) *info = local;
      set("query pixel outside sparse SNR layer domain (no extrapolation; "
          "frame-level fallback forbidden)");
      return false;
    }
    const double v = eval_bilinear(std::min(std::max(gx, 0.0), hi_x),
                                   std::min(std::max(gy, 0.0), hi_y));
    local.operator_id = "bilinear_regular_grid_v1";
    if (info) *info = local;
    if (!positive_finite(v)) {
      set("bilinear-reconstructed intra-frame SNR non-finite/non-positive");
      return false;
    }
    if (out_snr) *out_snr = v;
    return true;
  }

  /* 散点模式：最近控制点，必须显式声明覆盖半径（禁隐式外推）。 */
  if (!positive_finite(layer.max_radius_px)) {
    set("scattered sparse layer requires explicit max_radius_px > 0 "
        "(no implicit extrapolation / frame-level fallback)");
    return false;
  }
  double best_d2 = std::numeric_limits<double>::infinity();
  double best_v = 0.0;
  bool best_ok = false;
  for (const auto& p : layer.points) {
    if (!positive_finite(p.snr)) {
      set("sparse layer control point SNR non-finite/non-positive (corrupt)");
      return false;
    }
    const double ddx = x - p.x;
    const double ddy = y - p.y;
    const double d2 = ddx * ddx + ddy * ddy;
    if (d2 < best_d2) { best_d2 = d2; best_v = p.snr; best_ok = true; }
  }
  if (!best_ok) {
    set("scattered sparse layer has no usable control point");
    return false;
  }
  const double r = layer.max_radius_px;
  if (best_d2 > r * r * (1.0 + 1e-12)) {
    local.out_of_domain = true;
    local.operator_id = "nearest_control_point_v1";
    if (info) *info = local;
    set("query pixel farther than declared max_radius_px from any control point "
        "(no extrapolation; frame-level fallback forbidden)");
    return false;
  }
  local.operator_id = "nearest_control_point_v1";
  if (info) *info = local;
  if (out_snr) *out_snr = best_v;
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
