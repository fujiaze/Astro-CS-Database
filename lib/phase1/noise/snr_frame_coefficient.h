// ============================================================================
// snr_frame_coefficient.h — P33-COEF: 帧级单一 SNR 系数 (DATA-P1-SNR-COEF/1)
// ----------------------------------------------------------------------------
// 负责人 2026-09-15 设计变更: 不再交付稀疏控制点/IDW 重建层; 改为**一帧一个
// SNR 系数**写入产品元数据 (帧级基准度量), 并删除逐源数组。
//
// 选型依据 (run/perf-fix/P33-snr-model/report REPORT.md 3 节, 6 真实帧):
//   以**图像侧独立参考** R = median(孔径测光 SNR, 通量匹配) 为基准:
//     - median(SNR_F)              : R 比值跨帧 gm=1.12 (排除参考失效的 M42_Ha),
//                                    逐帧 0.66-2.71; 增量成本 0 (节点已算 median)
//     - trimmed-10% / 几何均值      : 同类, 无系统优势
//     - flux-matched median        : 对样本选择更稳健, 成本 O(N)
//     - 1/sigma_sky (像素 SNR)      : 尺度不同 (逐像素 vs 逐源), 不可作源 SNR 系数
//     - 孔径测光 SNR                : 背景主导帧与 PSF 族一致; 强星云帧失效
//     - 测光不确定度 1/(ln10 sigma) : 本 6 帧无定标残差, 不可评估; 且语义为定标散度
//   结论: **value = median(SNR_F) = sci.median_snr** (与 DATA-P1-SNR/2 的
//   snr_phot/median_snr 同一量), 并随帧记录样本定义/离散度/噪声尺度 provenance。
//
// 边界 (诚实声明, 见 REPORT): 「帧内 SNR 近似一致」只在帧级成立——固定通量下
//   分区局部系数相对帧系数的偏离 p50 = 8%-26% (强星云帧更大), 分区观测中位数
//   相对帧系数偏离 p50 = 13%-78%。故本系数是**帧级度量**, 不得当作局部 SNR 场。
//
// 公式零副本: 本库不实现任何 SNR 公式; 输入是生产
//   astrocs::phase1::compute_snr_frame_science 已算出的逐源 SNR_F/中位数。
// ============================================================================
#ifndef ASTROCS_PHASE1_NOISE_SNR_FRAME_COEFFICIENT_H
#define ASTROCS_PHASE1_NOISE_SNR_FRAME_COEFFICIENT_H

#include "snr_frame_science.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {
namespace phase1 {

// 帧级 SNR 系数 (DATA-P1-SNR-COEF/1) —— 值语义, 可序列化。
struct SnrFrameCoefficient {
  bool valid = false;             // 无有效样本/非有限中位数 -> false (fail-closed)
  std::string reason;             // invalid 原因 (空 = ok)
  double value = 0.0;             // 交付系数 = median(SNR_F) (推荐估计量)
  std::string estimator = "median_snr_f";
  int64_t n_sources = 0;          // 参与样本数
  double median_snr_f = 0.0;      // == value (同源, 显式留存)
  double trimmed10_mean_snr_f = 0.0;   // 备选估计量 (provenance)
  double geomean_snr_f = 0.0;          // 备选估计量 (provenance)
  double flux_matched_median_snr_f = 0.0;  // 备选估计量 (通量匹配窗内中位数)
  double p16_snr_f = 0.0;         // 帧内离散度 (逐源分位, 非局部场)
  double p84_snr_f = 0.0;
  double faint_over_bright = 0.0; // 暗四分位中位数 / 亮四分位中位数 (样本敏感性)
  int64_t n_flux_matched = 0;     // 通量匹配窗内样本数
};

struct SnrFrameCoefficientConfig {
  // 通量匹配窗: |log10(F) - median(log10 F)| <= match_half_dex
  double match_half_dex = 0.10;
};

// 计算帧级系数。snr_f/flux_adu 与 sci 同序 (逐源, 已剔除不可计算行)。
// 确定性: 分位数用稳定排序 + 线性插值; 与输入顺序无关。
SnrFrameCoefficient compute_snr_frame_coefficient(
    const SnrFrameScienceResult& sci, const std::vector<double>& flux_adu,
    const SnrFrameCoefficientConfig& cfg = SnrFrameCoefficientConfig());

// 产品 JSON (DATA-P1-SNR-COEF/1)。调用方负责附加帧级 provenance。
std::string snr_frame_coefficient_to_json(const SnrFrameCoefficient& c);

// 估计量定义明文 (唯一文案; 消费者/文档引用此函数, 不复制字符串)。
const char* snr_frame_coefficient_definition();
const char* snr_frame_coefficient_sample_definition();

}  // namespace phase1
}  // namespace astrocs

#endif  // ASTROCS_PHASE1_NOISE_SNR_FRAME_COEFFICIENT_H
