// AstroCS Phase1 — 帧级 SNR 科学聚合 (P8-SNR-LINUX)
//
// 职责: 把 Linux 生产路径 (LIBS 节点 astrocs.phase1.noise-snr → NoiseModel::estimate)
//       的 SNR 输出从"整帧标量"改为**逐源科学 SNR** + 帧级 5sigma 深度。
//
// 唯一权威实现: lib/algorithms/noise_snr/cpp/src/snr_science.cpp (P5-SNR, 负责人授权
// 2026-09-14)。本文件**只做聚合**, 不含任何本地科学公式副本; 逐源 SNR 一律调用
// snr_source_snr_f64, 帧级深度一律 snr_frame_depth_f64, 零点标准误一律
// snr_calib_zero_point_standard_error。
//
// 依据与定义式 (见 docs/science/CONTROL_WEIGHT_SNR.md §2a / S4):
//   逐源 SNR_F = F / sigma_F,  sigma_F^-2 = sum_i P_i^2/sigma_i^2   (Horne 1986)
//   帧级科学基准 = 5sigma 点源深度 F_5 = 5*sigma_F(ref) [ADU];
//                  m_5 = ZP - 2.5*log10(F_5) [mag] (无 ZP 时 NaN)
//   相对质量权重 (S4 quality_weight) = SNR_F_i / median(SNR_F)
//   零点标准误 sigma_se = 1.253 * sigma_logflux_dex / sqrt(n_matches) [dex]
//
// 单位 (强制):
//   flux_adu [ADU]; fwhm_px [pixel] = **检测块椭圆高斯** FWHM
//   (DATA-P1-SOURCES.fwhm_px, FWHM = 2.3548200450309493*sigma; SCI-P1-STAR-001 §2);
//   sigma_sky_adu [ADU]; gain [e-/ADU];
//   read_noise_e [e-]; zero_point_mag [mag]; snr_f/local_snr [1];
//   sigma_f_adu [ADU]; flux5_adu [ADU]; m5_mag [mag]; se_dex [dex]
//
// 明确禁止: 本路径**不产出**"整帧 SNR 标量"。帧级量只有 5sigma 深度 (+ 目录中位数
//   median(SNR_F), 其语义是逐源 SNR 的分布摘要, 不是新的帧级 SNR 定义)。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::phase1 {

// 逐源输入行 (来自 p1_sources.json 的测光有效星; 调用方负责提供)
struct SnrSourceRow {
  std::string id;        // star_id (可空; 仅用于追溯)
  double flux_adu = 0.0; // F 总通量 [ADU] (>0 才可计算)
  // **检测块**母函数宽度 = 椭圆高斯 FWHM [pixel] (>0 才可计算)。
  // 来源列固定为 DATA-P1-SOURCES.sources[].fwhm_px (与 psf.max_stars 解耦,
  // DATA_SEMANTICS §13.4); 该列不得与 PSF 块 Moffat4 FWHM 列互换/比较
  // (SCI-P1-STAR-001 §2 :31-34, DISP-STAR-007): 同 sigma 下相差 1.914005x。
  double fwhm_px = 0.0;
};

// 帧级配置 (全部显式; 无隐式帧级 SNR 标量)
struct SnrFrameScienceConfig {
  double sigma_sky_adu = 0.0;      // 逐像素空背景 rms [ADU]; <=0 或非有限 -> 无效
  double gain_e_per_adu = 0.0;     // [e-/ADU]; <=0 = 未知 (不加源泊松项, 天空受限)
  double read_noise_e = 0.0;       // [e-]; 仅 gain>0 时进入逐像素方差
  double zero_point_mag = 0.0;     // [mag]; 0 或非有限 = 未知 -> frame_depth_m5_mag = NaN
  double aperture_radius_px = 0.0; // [pixel]; <=0 -> 1.5*FWHM
  double n_sky = 0.0;              // 天空环像素数; <=0 -> n_pix
  int profile_half_px = 0;         // 轮廓网格半边长; 0 -> 自动 (max(30, ceil(12*FWHM)))
  double sigma_logflux_dex = 0.0;  // 定标残差散度 [dex] (逐星散度, 非零点误差)
  int n_matches = 0;               // 定标匹配星数 N
  // 组内公共参考通量 F_ref [ADU]（WEIGHT-SCI-001）。缺失/非有限/<=0 ->
  // fail-closed（逐帧检出通量中位数回退已删除；调用方必须为整个帧组传入同一 F0）。
  double reference_flux_adu = 0.0;
  // sigma_sky 语义 (SCI-B D1; 07_noise_snr.md 4.2a): SNR_SIGMA_SKY_* 常量, 见 snr_estimator.h
  int sigma_sky_source = 0;
};

// 帧级聚合结果。snr_f/sigma_f_adu/local_snr 与输入 sources 逐行对齐;
// NaN 表示该行未参与 (输入退化)。
struct SnrFrameScienceResult {
  bool valid = false;
  std::string reason;              // 空 = 成功
  int n_input = 0;
  int n_used = 0;
  std::vector<double> snr_f;        // 逐源 SNR_F [1]  = F/sigma_F
  std::vector<double> sigma_f_adu;  // 逐源 sigma_F [ADU]
  std::vector<double> local_snr;    // 逐源相对质量权重 [1] = SNR_F/median(SNR_F) (S4)
  double median_snr = 0.0;          // == median(SNR_F) [1]
  double snr_phot = 0.0;            // == median(SNR_F) [1] (字段名冻结, 语义已重定义)
  double median_source_snr = 0.0;   // == median(SNR_F) [1]
  double frame_depth_flux5_adu = 0.0;  // F_5 = 5*sigma_F(ref) [ADU]
  double frame_depth_m5_mag = 0.0;     // m_5 [mag]; 无 ZP -> NaN
  double sigma_location_se_dex = 0.0;  // 1.253*sigma/sqrt(N) [dex]
  double sigma_location_se_mag = 0.0;  // 2.5*sigma_location_se_dex [mag]
  // 显式参考轮廓 (帧级深度的唯一绑定对象; 禁止用整帧标量替代)
  int reference_index = -1;         // 参考轮廓对应的输入行 (-1 = 合成中位轮廓)
  double reference_flux_adu = 0.0;  // 组内公共参考通量 F_ref [ADU]（= 定义 reference_snr_f 的同一通量）
  double reference_fwhm_px = 0.0;   // 参考 FWHM (有效源 FWHM 中位数) [pixel]
  double reference_snr_f = 0.0;     // 参考轮廓 SNR_F [1]
  double reference_sigma_f_adu = 0.0;  // 参考 sigma_F [ADU]
};

// 逐源科学 SNR + 帧级 5sigma 深度 + 零点标准误 的唯一聚合入口。
// 返回结果在输入/配置退化时 valid=false 且 reason 非空 (fail-closed,
// 不产"帧级 SNR 标量"、不静默回填 1.0)。
SnrFrameScienceResult compute_snr_frame_science(
    const std::vector<SnrSourceRow>& sources,
    const SnrFrameScienceConfig& cfg);

}  // namespace astrocs::phase1
