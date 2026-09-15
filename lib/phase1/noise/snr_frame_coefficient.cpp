// ============================================================================
// snr_frame_coefficient.cpp — P33-COEF 帧级单一 SNR 系数 (DATA-P1-SNR-COEF/1)
// 纯聚合: 输入 = 生产 compute_snr_frame_science 的逐源 SNR_F; 不含 SNR 公式副本。
// 确定性: 分位数/中位数在**已排序副本**上取 (与输入顺序无关); 无随机、无并行归约。
// ============================================================================
#include "snr_frame_coefficient.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

using nlohmann::json;

namespace astrocs {
namespace phase1 {
namespace {

// 线性插值分位数 (与 numpy 默认 'linear' 一致; 确定性)
double quantile_sorted(const std::vector<double>& s, double q) {
  const std::size_t n = s.size();
  if (n == 0) return 0.0;
  if (n == 1) return s[0];
  const double pos = q * static_cast<double>(n - 1);
  const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
  const std::size_t hi = std::min(n - 1, lo + 1);
  const double w = pos - static_cast<double>(lo);
  return s[lo] * (1.0 - w) + s[hi] * w;
}

double median_sorted(const std::vector<double>& s) { return quantile_sorted(s, 0.5); }

// 10% 截尾均值 (两端各去 10%, 至少留 1 个)
double trimmed10_sorted(const std::vector<double>& s) {
  const std::size_t n = s.size();
  if (n == 0) return 0.0;
  const std::size_t k = static_cast<std::size_t>(0.10 * static_cast<double>(n));
  if (n <= 2 * k) return median_sorted(s);
  double sum = 0.0;
  std::size_t cnt = 0;
  for (std::size_t i = k; i < n - k; ++i) { sum += s[i]; ++cnt; }
  return cnt ? sum / static_cast<double>(cnt) : 0.0;
}

}  // namespace

const char* snr_frame_coefficient_definition() {
  return "frame-level single SNR coefficient = median over the delivered sample of "
         "per-source SNR_F (SNR_F = F/sigma_F, Horne 1986 optimal extraction); "
         "identical to DATA-P1-SNR/2 snr_phot == median_snr (SCI-CW-001 2a frame "
         "science benchmark); NOT a local/regional SNR field";
}

const char* snr_frame_coefficient_sample_definition() {
  return "all photometrically valid sources from DATA-P1-SOURCES.sources "
         "(flux>0, fwhm_px>0); independent of psf.max_stars / psf_params";
}

SnrFrameCoefficient compute_snr_frame_coefficient(
    const SnrFrameScienceResult& sci, const std::vector<double>& flux_adu,
    const SnrFrameCoefficientConfig& cfg) {
  SnrFrameCoefficient c;
  if (!sci.valid || sci.n_used <= 0 || sci.snr_f.empty()) {
    c.valid = false;
    c.reason = sci.valid ? "no usable source for frame coefficient"
                         : ("upstream science result invalid: " + sci.reason);
    return c;
  }
  // 只取有限正 SNR_F (与 sci 同序的 flux 使用同一掩码)
  std::vector<double> s;
  std::vector<double> fx;
  const std::size_t n = sci.snr_f.size();
  s.reserve(n);
  fx.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double v = sci.snr_f[i];
    if (!(std::isfinite(v) && v > 0.0)) continue;
    s.push_back(v);
    fx.push_back(i < flux_adu.size() ? flux_adu[i] : 0.0);
  }
  if (s.empty()) {
    c.valid = false;
    c.reason = "no finite positive SNR_F in delivered sample";
    return c;
  }
  c.n_sources = static_cast<int64_t>(s.size());
  std::vector<double> sorted = s;
  std::sort(sorted.begin(), sorted.end());
  c.median_snr_f = median_sorted(sorted);
  c.value = c.median_snr_f;
  c.estimator = "median_snr_f";
  c.trimmed10_mean_snr_f = trimmed10_sorted(sorted);
  c.p16_snr_f = quantile_sorted(sorted, 0.16);
  c.p84_snr_f = quantile_sorted(sorted, 0.84);

  // 几何均值 (log 域, 已保证 > 0)。在**已排序**序上求和 => 与输入顺序逐位无关。
  {
    double sl = 0.0;
    for (double v : sorted) sl += std::log(v);
    c.geomean_snr_f = std::exp(sl / static_cast<double>(sorted.size()));
  }
  // 通量匹配窗 (|log10 F - median(log10 F)| <= half_dex) 内中位数: 对星等分布稳健
  {
    std::vector<double> lf;
    lf.reserve(fx.size());
    for (double f : fx) if (std::isfinite(f) && f > 0.0) lf.push_back(std::log10(f));
    if (!lf.empty()) {
      std::vector<double> lfs = lf;
      std::sort(lfs.begin(), lfs.end());
      const double m = median_sorted(lfs);
      std::vector<double> in;
      for (std::size_t i = 0; i < s.size(); ++i) {
        const double f = fx[i];
        if (!(std::isfinite(f) && f > 0.0)) continue;
        if (std::fabs(std::log10(f) - m) <= cfg.match_half_dex) in.push_back(s[i]);
      }
      if (!in.empty()) {
        std::sort(in.begin(), in.end());
        c.flux_matched_median_snr_f = median_sorted(in);
        c.n_flux_matched = static_cast<int64_t>(in.size());
      }
    }
  }
  // 样本敏感性: 暗四分位中位数 / 亮四分位中位数 (仅当两侧都有样本)
  {
    std::vector<double> idx_lf;
    idx_lf.reserve(fx.size());
    for (double f : fx) if (std::isfinite(f) && f > 0.0) idx_lf.push_back(std::log10(f));
    if (idx_lf.size() == s.size() && idx_lf.size() >= 8) {
      std::vector<double> lfs = idx_lf;
      std::sort(lfs.begin(), lfs.end());
      const double q25 = quantile_sorted(lfs, 0.25);
      const double q75 = quantile_sorted(lfs, 0.75);
      std::vector<double> faint, bright;
      for (std::size_t i = 0; i < s.size(); ++i) {
        if (idx_lf[i] <= q25) faint.push_back(s[i]);
        else if (idx_lf[i] >= q75) bright.push_back(s[i]);
      }
      if (!faint.empty() && !bright.empty()) {
        std::sort(faint.begin(), faint.end());
        std::sort(bright.begin(), bright.end());
        const double mb = median_sorted(bright);
        if (mb > 0.0) c.faint_over_bright = median_sorted(faint) / mb;
      }
    }
  }
  c.valid = std::isfinite(c.value) && c.value > 0.0;
  if (!c.valid) c.reason = "frame coefficient is not finite/positive";
  return c;
}

std::string snr_frame_coefficient_to_json(const SnrFrameCoefficient& c) {
  json j;
  j["schema"] = "DATA-P1-SNR-COEF/1";
  j["valid"] = c.valid;
  j["reason"] = c.reason;
  j["estimator"] = c.estimator;
  j["definition"] = snr_frame_coefficient_definition();
  j["sample"] = snr_frame_coefficient_sample_definition();
  j["units"] = "1";
  j["n_sources"] = c.n_sources;
  if (c.valid) {
    j["value"] = c.value;
    j["median_snr_f"] = c.median_snr_f;
    j["dispersion"] = json{{"p16", c.p16_snr_f}, {"p50", c.median_snr_f},
                           {"p84", c.p84_snr_f}};
    j["alternatives"] = json{{"trimmed10_mean_snr_f", c.trimmed10_mean_snr_f},
                             {"geometric_mean_snr_f", c.geomean_snr_f},
                             {"flux_matched_median_snr_f", c.flux_matched_median_snr_f},
                             {"flux_match_half_dex", 0.10},
                             {"n_flux_matched", c.n_flux_matched}};
    // 不可计算 (样本 < 8 或某四分位为空) 时落 null, 不得用 0 冒充
    j["sample_sensitivity"] = json{
        {"faint_over_bright",
         (c.faint_over_bright > 0.0) ? json(c.faint_over_bright) : json(nullptr)},
        {"definition", "median(SNR_F | faint quartile flux) "
                       "/ median(SNR_F | bright quartile flux); null = not computable"}};
  } else {
    j["value"] = nullptr;
    j["median_snr_f"] = nullptr;
  }
  return j.dump(2);
}

}  // namespace phase1
}  // namespace astrocs
