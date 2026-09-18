// AstroCS Phase1 — 帧级 SNR 科学聚合实现 (P8-SNR-LINUX)
//
// 科学公式零本地副本: 本 TU 的全部数值都来自 P5-SNR 唯一权威实现
// lib/algorithms/noise_snr/cpp/src/snr_science.cpp:
//   snr_source_snr_f64              — 逐源 Horne 1986 最优提取 (+ 孔径 CCD 方程)
//   snr_moffat4_profile_f64         — 离散归一化 Moffat4 beta=4 轮廓统计
//   snr_frame_depth_f64             — 帧级 5sigma 点源深度 (显式参考轮廓)
//   snr_calib_zero_point_standard_error — 零点标准误 1.253*sigma/sqrt(N)
//
// verifier 独立性: 同一组公式另由 NumPy oracle
// (run/perf-fix/P5-snr/harness/snr_oracle.py, 不调用被测代码) 独立复算;
// 本 TU 不参与 oracle 的"参考侧"。

#include "snr_frame_science.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "../cpp/include/snr_estimator.h"

namespace astrocs::phase1 {
namespace {

inline double nan_value() { return std::numeric_limits<double>::quiet_NaN(); }

// 确定性中位数: 复制后排序 (不依赖输入顺序之外的任何状态; 与线程数无关)。
// 空输入 -> NaN。
double median_of(const std::vector<double>& v) {
  if (v.empty()) return nan_value();
  std::vector<double> s = v;
  std::sort(s.begin(), s.end());
  const std::size_t n = s.size();
  if (n % 2 == 1) return s[n / 2];
  return 0.5 * (s[n / 2 - 1] + s[n / 2]);
}

// 单源 Horne 最优提取 (全部参数显式; 退化时返回 false)
bool source_snr(const SnrSourceRow& row, const SnrFrameScienceConfig& cfg,
                SnrSourceResult* out) {
  if (!std::isfinite(row.flux_adu) || !(row.flux_adu > 0.0)) return false;
  if (!std::isfinite(row.fwhm_px) || !(row.fwhm_px > 0.0)) return false;

  SnrSourceParams p;
  std::memset(&p, 0, sizeof(p));
  p.flux_adu = row.flux_adu;
  p.fwhm_px = row.fwhm_px;
  p.sigma_px = 0.0;
  p.sigma_sky_adu = cfg.sigma_sky_adu;
  p.gain_e_per_adu = cfg.gain_e_per_adu;
  p.read_noise_e = cfg.read_noise_e;
  p.aperture_radius_px = cfg.aperture_radius_px;
  p.n_sky = cfg.n_sky;
  p.zero_point_mag = cfg.zero_point_mag;
  p.profile_half_px = cfg.profile_half_px;

  if (snr_source_snr_f64(&p, out) != 0) return false;
  if (out->status != 0) return false;
  if (!std::isfinite(out->snr_optimal) || !(out->snr_optimal > 0.0)) return false;
  return true;
}

}  // namespace

SnrFrameScienceResult compute_snr_frame_science(
    const std::vector<SnrSourceRow>& sources,
    const SnrFrameScienceConfig& cfg) {
  SnrFrameScienceResult out;
  out.n_input = static_cast<int>(sources.size());
  out.median_snr = nan_value();
  out.snr_phot = nan_value();
  out.median_source_snr = nan_value();
  out.frame_depth_flux5_adu = nan_value();
  out.frame_depth_m5_mag = nan_value();
  out.reference_flux_adu = nan_value();
  out.reference_fwhm_px = nan_value();
  out.reference_snr_f = nan_value();
  out.reference_sigma_f_adu = nan_value();
  out.snr_f.assign(sources.size(), nan_value());
  out.sigma_f_adu.assign(sources.size(), nan_value());
  out.local_snr.assign(sources.size(), nan_value());

  out.sigma_location_se_dex =
      snr_calib_zero_point_standard_error(cfg.sigma_logflux_dex, cfg.n_matches);
  out.sigma_location_se_mag = 2.5 * out.sigma_location_se_dex;

  if (!std::isfinite(cfg.sigma_sky_adu) || !(cfg.sigma_sky_adu > 0.0)) {
    out.reason = "invalid sigma_sky_adu (<=0 or non-finite)";
    return out;
  }
  if (sources.empty()) {
    out.reason = "empty source catalogue";
    return out;
  }

  // --- 逐源科学 SNR (无共享可变状态; 逐行独立 -> 与并行度无关) ---
  //
  // P10-UTIL2-001 (2026-09-14): 逐源并行化。
  //   并行轴 = 源 (work unit = catalogue 行); 每行只读输入、只写自己的定长槽位
  //   (rn_snr/rn_sigmaf/rok 的第 i 项) —— 无共享可变状态、无归约、无锁。
  //   确定性论证: 并行区是逐行独立的标量计算, 结果写回**按下标固定**的数组;
  //   串行 compact 阶段严格按 i 升序复现原 for 的取值与先后 (原 skip-continue
  //   语义由 rok[] 标记 + 升序扫描 1:1 复刻), 故 used_* 序列与串行逐位一致
  //   -> 后续 median/参考轮廓/产物与线程数、调度顺序均无关。
  //   调度 dynamic(小 chunk): work unit 数 = catalogue 行数, 与线程数无关。
  const std::size_t n_src = sources.size();
  std::vector<unsigned char> rok(n_src, 0);
  std::vector<double> rn_snr(n_src, nan_value());
  std::vector<double> rn_sigmaf(n_src, nan_value());
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 1)
#endif
  for (long long i = 0; i < static_cast<long long>(n_src); ++i) {
    SnrSourceResult r;
    if (!source_snr(sources[static_cast<std::size_t>(i)], cfg, &r)) continue;
    rn_snr[static_cast<std::size_t>(i)] = r.snr_optimal;
    rn_sigmaf[static_cast<std::size_t>(i)] = r.sigma_f_optimal_adu;
    rok[static_cast<std::size_t>(i)] = 1;
  }
  std::vector<double> used_snr;
  std::vector<double> used_fwhm;
  used_snr.reserve(n_src);
  used_fwhm.reserve(n_src);
  for (std::size_t i = 0; i < n_src; ++i) {
    if (!rok[i]) continue;
    out.snr_f[i] = rn_snr[i];
    out.sigma_f_adu[i] = rn_sigmaf[i];
    used_snr.push_back(rn_snr[i]);
    used_fwhm.push_back(sources[i].fwhm_px);
  }
  out.n_used = static_cast<int>(used_snr.size());
  if (used_snr.empty()) {
    out.reason = "no valid source (flux/fwhm degenerate in every row)";
    return out;
  }

  // --- median(SNR_F): snr_phot / median_snr / median_source_snr 三者同一值 ---
  const double med = median_of(used_snr);
  if (!std::isfinite(med) || !(med > 0.0)) {
    out.reason = "median(SNR_F) not positive/finite";
    return out;
  }
  out.median_snr = med;
  out.snr_phot = med;
  out.median_source_snr = med;

  // --- 相对质量权重场 (SCI-CW §4 quality_weight; 非校准 SNR) ---
  int indices_used = 0;
  for (std::size_t i = 0; i < sources.size(); ++i) {
    if (std::isfinite(out.snr_f[i])) {
      out.local_snr[i] = out.snr_f[i] / med;
      ++indices_used;
    }
  }
  if (indices_used != out.n_used) {
    out.reason = "internal catalogue count mismatch";
    out.valid = false;
    return out;
  }

  // --- 帧级科学基准: 显式参考轮廓 = (median FWHM, sigma_sky, 参考通量) ---
  //
  // WEIGHT-SCI-001（2026-09-18 科学裁决，reports/RELEASE-02/weight-sci-ruling.md）:
  // F_ref 必须是**组内公共参考通量**，且与存入 HiPS 头的 ASTROCS_FRAME_SNR
  // **配对**（同一定义参考）。配对性定理:
  //   SNR_f = a_f·F_ref/σ_f  ⇒  SNR_f²/F_ref² = a_f²/σ_f² = w_f
  // 成立当且仅当分母 F_ref 与定义 SNR 时所用参考通量是同一个。
  // 逐帧检出通量中位数回退会丢掉帧间标度因子 a_f²，并使存头 SNR 混入本帧检出
  // 亮度（帧间不可比较），与 Phase2 闸门/权重链的单 F_ref 约定不配对
  // ⇒ 必然 unclosed_invalid_reference_flux。该回退**已删除**（不得恢复）。
  // reference_flux_adu 缺失/非有限/≤0 ⇒ fail-closed；调用方（Phase1 节点）必须为
  // 整个帧组选定一个公共 F0 并对所有帧传入同一值。
  if (!std::isfinite(cfg.reference_flux_adu) || !(cfg.reference_flux_adu > 0.0)) {
    out.reason =
        "reference_flux_adu required (group-common F_ref; per-frame median "
        "fallback removed)";
    out.valid = false;
    return out;
  }
  const double med_fwhm = median_of(used_fwhm);
  const double ref_flux = cfg.reference_flux_adu;
  if (!std::isfinite(med_fwhm) || !(med_fwhm > 0.0) || !(ref_flux > 0.0)) {
    out.reason = "reference profile degenerate";
    return out;
  }
  SnrSourceRow ref_row;
  ref_row.id = "__frame_reference_median_fwhm__";
  ref_row.flux_adu = ref_flux;
  ref_row.fwhm_px = med_fwhm;
  SnrSourceResult ref_res;
  if (!source_snr(ref_row, cfg, &ref_res)) {
    out.reason = "reference profile SNR failed";
    return out;
  }
  out.reference_flux_adu = ref_flux;
  out.reference_fwhm_px = med_fwhm;
  out.reference_snr_f = ref_res.snr_optimal;
  out.reference_sigma_f_adu = ref_res.sigma_f_optimal_adu;
  out.frame_depth_flux5_adu = nan_value();
  out.frame_depth_m5_mag = nan_value();
  if (snr_frame_depth_f64(&ref_res, cfg.zero_point_mag,
                          &out.frame_depth_flux5_adu,
                          &out.frame_depth_m5_mag) != 0) {
    out.reason = "snr_frame_depth_f64 failed";
    return out;
  }

  out.valid = true;
  return out;
}

}  // namespace astrocs::phase1
