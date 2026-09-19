/* variance_propagation.cpp — 残差制造者 PΣPᵀ 方差传播实现（见头文件权威锚）
 *
 * 纯 FP64 + std；无第三方依赖。全部退化路径 fail-closed，无 NaN/clamp 静默。
 */
#include "astrocs/v6/variance_propagation.h"

#include <algorithm>
#include <cmath>

namespace astrocs {
namespace v6 {
namespace p2var {

namespace {

bool finite(double v) { return std::isfinite(v); }
bool positive_finite(double v) { return std::isfinite(v) && v > 0.0; }

void set_err(std::string* err, const std::string& m) {
  if (err) *err = m;
}

/* 行内取 H_ii（无则该列为 0）。 */
double row_diag(const HatRow& h, std::size_t i) {
  for (std::size_t k = 0; k < h.frame_index.size(); ++k)
    if (static_cast<std::size_t>(h.frame_index[k]) == i) return h.coeff[k];
  return 0.0;
}

}  /* namespace */

bool hat_row_valid(const HatRow& h, std::size_t n_frames, std::string* err) {
  if (h.frame_index.size() != h.coeff.size()) {
    set_err(err, "HatRow: frame_index/coeff length mismatch");
    return false;
  }
  std::uint32_t prev = 0;
  bool first = true;
  for (std::size_t k = 0; k < h.frame_index.size(); ++k) {
    const std::uint32_t j = h.frame_index[k];
    if (static_cast<std::size_t>(j) >= n_frames) {
      set_err(err, "HatRow: column index out of range");
      return false;
    }
    if (!first && j <= prev) {
      set_err(err, "HatRow: columns must be strictly ascending and unique");
      return false;
    }
    if (!finite(h.coeff[k])) {
      set_err(err, "HatRow: coefficient non-finite");
      return false;
    }
    prev = j;
    first = false;
  }
  return true;
}

bool residual_maker_variance(const HatRow& h, const double* sigma2,
                             std::size_t n_frames, std::size_t i,
                             double* out_var, std::string* err) {
  if (sigma2 == nullptr || n_frames == 0 || i >= n_frames) {
    set_err(err, "residual_maker_variance: bad sigma2/index");
    return false;
  }
  std::string herr;
  if (!hat_row_valid(h, n_frames, &herr)) {
    set_err(err, "residual_maker_variance: " + herr);
    return false;
  }
  for (std::size_t j = 0; j < n_frames; ++j) {
    if (!finite(sigma2[j]) || sigma2[j] < 0.0) {
      set_err(err, "residual_maker_variance: sigma2 non-finite/negative");
      return false;
    }
  }
  /* P = I − H ⇒ P_ii = 1 − H_ii，P_ij = −H_ij (j≠i)。 */
  const double p_ii = 1.0 - row_diag(h, i);
  double var = p_ii * p_ii * sigma2[i];
  for (std::size_t k = 0; k < h.frame_index.size(); ++k) {
    const std::size_t j = static_cast<std::size_t>(h.frame_index[k]);
    if (j == i) continue;
    var += h.coeff[k] * h.coeff[k] * sigma2[j];
  }
  if (!finite(var) || var < 0.0) {
    set_err(err, "residual_maker_variance: result non-finite/negative");
    return false;
  }
  if (out_var) *out_var = var;
  return true;
}

bool naive_variance(const HatRow& h, const double* sigma2, std::size_t n_frames,
                    std::size_t i, double* out_var, std::string* err) {
  if (sigma2 == nullptr || n_frames == 0 || i >= n_frames) {
    set_err(err, "naive_variance: bad sigma2/index");
    return false;
  }
  std::string herr;
  if (!hat_row_valid(h, n_frames, &herr)) {
    set_err(err, "naive_variance: " + herr);
    return false;
  }
  for (std::size_t j = 0; j < n_frames; ++j) {
    if (!finite(sigma2[j]) || sigma2[j] < 0.0) {
      set_err(err, "naive_variance: sigma2 non-finite/negative");
      return false;
    }
  }
  /* σ_i² + Var(ĝ_i)（错误：把 corrected 与 ĝ 当独立）。 */
  double var = sigma2[i];
  for (std::size_t k = 0; k < h.frame_index.size(); ++k) {
    const std::size_t j = static_cast<std::size_t>(h.frame_index[k]);
    var += h.coeff[k] * h.coeff[k] * sigma2[j];
  }
  if (!finite(var) || var < 0.0) {
    set_err(err, "naive_variance: result non-finite/negative");
    return false;
  }
  if (out_var) *out_var = var;
  return true;
}

bool normalized_weight_hat_row(const double* w, std::size_t n, std::size_t i,
                               bool include_self, HatRow* out,
                               std::string* err) {
  if (w == nullptr || n == 0 || i >= n) {
    set_err(err, "normalized_weight_hat_row: bad w/index");
    return false;
  }
  double wsum = 0.0;
  for (std::size_t j = 0; j < n; ++j) {
    if (!finite(w[j]) || w[j] < 0.0) {
      set_err(err, "normalized_weight_hat_row: weight non-finite/negative");
      return false;
    }
    wsum += w[j];
  }
  if (!positive_finite(wsum)) {
    set_err(err, "normalized_weight_hat_row: total weight non-positive");
    return false;
  }
  double denom = wsum;
  if (!include_self) {
    /* (c) 排除自身: 分母 W_{-i}；自身权重必须 >0（否则"排除自身"无定义）。 */
    if (!positive_finite(w[i])) {
      set_err(err, "normalized_weight_hat_row: exclude-self requires w_i > 0");
      return false;
    }
    denom = wsum - w[i];
    if (!positive_finite(denom)) {
      set_err(err, "normalized_weight_hat_row: W_-i non-positive (single-frame stack)");
      return false;
    }
  }
  HatRow row;
  row.frame_index.reserve(n);
  row.coeff.reserve(n);
  for (std::size_t j = 0; j < n; ++j) {
    if (!include_self && j == i) continue;   /* H_ii = 0 */
    const double c = w[j] / denom;
    if (c == 0.0) continue;                  /* 稀疏化零权重 */
    if (!finite(c)) {
      set_err(err, "normalized_weight_hat_row: coefficient non-finite");
      return false;
    }
    row.frame_index.push_back(static_cast<std::uint32_t>(j));
    row.coeff.push_back(c);
  }
  if (out) *out = std::move(row);
  return true;
}

bool corrected_pixel_variance(const HatRow& h, const double* sigma2,
                              std::size_t n_frames, std::size_t i,
                              double gain, double param_var, double* out_var,
                              std::string* err) {
  if (!positive_finite(gain)) {
    set_err(err, "corrected_pixel_variance: gain must be finite and > 0");
    return false;
  }
  if (!finite(param_var) || param_var < 0.0) {
    set_err(err, "corrected_pixel_variance: param_var must be finite and >= 0");
    return false;
  }
  double data_var = 0.0;
  if (!residual_maker_variance(h, sigma2, n_frames, i, &data_var, err))
    return false;
  const double var = (data_var + param_var) / (gain * gain);
  if (!positive_finite(var)) {
    set_err(err, "corrected_pixel_variance: result non-finite/non-positive");
    return false;
  }
  if (out_var) *out_var = var;
  return true;
}

bool weight_from_variance(double var, double* out_w, std::string* err) {
  if (!positive_finite(var)) {
    set_err(err, "weight_from_variance: variance must be finite and > 0");
    return false;
  }
  const double w = 1.0 / var;
  if (!positive_finite(w)) {
    set_err(err, "weight_from_variance: 1/variance non-finite/non-positive");
    return false;
  }
  if (out_w) *out_w = w;
  return true;
}

double mean_model_naive_over_correct(std::size_t n) {
  if (n < 2) return std::nan("");
  const double dn = static_cast<double>(n);
  const double correct = 1.0 - 1.0 / dn;
  const double naive = 1.0 + 1.0 / dn;
  return naive / correct;
}

}  /* namespace p2var */
}  /* namespace v6 */
}  /* namespace astrocs */
