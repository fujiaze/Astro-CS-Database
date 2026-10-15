// ACSD Normalize — 阶段一 typed 数据合同（NORMALIZE_DATA.md §1–§3 的 C++ 表达）
//
// 正本: docs/engineering/contracts/NORMALIZE_DATA.md
//       docs/engineering/architecture/NORMALIZE_PIPELINE.md §3
//       docs/detail/normalize/contracts.md §2–§3
// 科学定义: docs/science/normalize/NORMALIZE_SCIENCE.md
//
// 边界（本头只声明类型与语义，不做任何数值计算）:
//   * 阶段内对象带 ObjectHeader {run_id, frame_id, schema_version, revision,
//     producer, input_revisions}；生产者发布后对象只读，消费者核对帧/run/上游版本。
//   * 标量缺失用 std::optional，**不得用 0 表示未知 gain/方差/通量**（合同 §2）。
//   * 数值大面 FP32、几何/稀疏/标定元数据 FP64；稀疏控制点恒 FP64（合同 §1）。
//   * 坐标: 外部 ICRS 度（RA∈[0,360), Dec∈[-90,90]）；内部像元 0-based 数组中心；
//     CRPIX/CD 一律 FITS 1-based 数值（合同 §3）。
//   * 对象可用性（precision unavailable）与执行错误（fail）是两件事：缺 PSF 可使
//     reference precision unavailable，但不删除已有合法 signal。
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace acsd::normalize {

// ── 身份与版本（合同 §1）────────────────────────────────────────────────────
struct ObjectHeader {
  std::string run_id;
  std::string frame_id;
  std::string producer;                 // 生产节点/库的稳定身份（非代码行号）
  std::uint32_t schema_version = 1;
  std::uint64_t revision = 0;           // 本 run 内单调增
  std::map<std::string, std::uint64_t> input_revisions;  // 上游对象名 → revision
};

// ── 参考定义（合同 §5）──────────────────────────────────────────────────────
enum class ReferenceKind { kSedResponse = 0, kExplicitLinearFlux = 1, kUnavailable = 2 };

// 两种明确来源 + 显式不可用。未选参考时**不得**从实际恒星中位通量发明 canonical
// reference（组内 F0/source-median 路径只作具名 legacy diagnostic）。
struct ReferenceDefinition {
  ReferenceKind kind = ReferenceKind::kUnavailable;
  std::string reference_id;    // 独立参考身份（user:... 或目录内 SED/response ID）
  std::string flux_unit;       // reference_flux 的积分通量单位（与模板 h 配对）
  std::string normalization;   // native_calibrated | photo_applied（声明配对面）
  std::optional<double> flux;  // explicit_linear_flux 的正有限参考通量
  std::string sed_id;          // sed_response: 具名 SED（文件/hash 由调用方登记）
  std::string response_id;     // sed_response: 响应曲线身份
  std::string reason;          // unavailable 的原因（必填）
};

// ── 精度状态（合同 §5/§6）───────────────────────────────────────────────────
enum class PrecisionAvailability { kAvailable = 0, kUnavailable = 1 };
enum class PrecisionAccuracy {
  kConditionalComplete = 0,  // 参考源条件下完整（仍不宣称系统精度完整）
  kApproximate = 1,          // 有未定标增益/母版/模板误差等缺项
  kUpperBound = 2,           // 仅缺 PSD 随机源泊松项且模板/域固定 ⇒ 条件上界
  kNotApplicable = 3,
};

struct PrecisionStatus {
  PrecisionAvailability availability = PrecisionAvailability::kUnavailable;
  PrecisionAccuracy accuracy = PrecisionAccuracy::kNotApplicable;
  std::string reason;
  std::vector<std::string> omitted_terms;  // 被省略项的具名清单（不得静默）
};

// ── 点源 → 面亮度传递（合同 §6, NOISE_SNR §4）───────────────────────────────
enum class CovarianceCondition { kReferenceSource = 0, kActualScene = 1, kBackgroundOnly = 2 };

struct SurfaceTransfer {
  PrecisionAvailability availability = PrecisionAvailability::kUnavailable;
  std::optional<double> surface_variance;  // V_B（同源同位置同条件）
  std::optional<double> transfer_factor;   // T = V_B/V_F（有量纲）
  std::string surface_variance_unit;
  std::string transfer_unit;
  CovarianceCondition condition = CovarianceCondition::kReferenceSource;
  std::string reason;
};

// ── 参考控制点（合同 §2/§5/§6, detail contracts §3）─────────────────────────
struct ReferenceControl {
  std::string control_id;
  std::string support_id;
  std::string psf_model_id;
  std::string covariance_model_id;
  double ra_deg = 0.0;                   // ICRS
  double dec_deg = 0.0;
  double source_x0 = 0.0;                // detector 0-based 中心
  double source_y0 = 0.0;
  std::uint64_t leaf_ipix = 0;           // NESTED @ declared leaf_order
  bool leaf_ipix_valid = true;            // 十进制字符串解析失败 ⇒ false（不猜 0）
  int leaf_order = 0;                     // 实际叶阶 log2(nside)
  std::optional<double> reference_flux;  // 与其模板 h 配对的积分通量尺度
  std::optional<double> flux_variance;   // 同单位平方
  std::optional<double> reference_snr;   // F_ref/√V_F（非 legacy 代理量）
  PrecisionStatus status;
  std::optional<double> support_area_sr;  // 该点真实球面支持面积
  std::optional<std::uint64_t> covariance_rank;
  std::optional<double> condition_number;
  // known | constant_fit | plane_fit（合同 §5; 背景 nuisance 口径）
  std::string background_nuisance = "known";
  SurfaceTransfer transfer;
};

// ── 帧级摘要（合同 §5: area-weighted median of log reference_snr 后取 exp）───
struct FrameReferenceSummary {
  std::string method = "area_weighted_median_log_snr";
  std::optional<double> reference_snr;
  double supported_area_sr = 0.0;
  std::uint64_t n_valid = 0;
  PrecisionStatus status;
};

// ── 稀疏参考精度（合同 §2 SparseReferencePrecision）────────────────────────
struct SparseReferencePrecision {
  ObjectHeader header;
  std::string domain = "drizzled_sphere";
  PrecisionStatus status;
  std::string support_model_id;
  std::vector<ReferenceControl> controls;   // FP64 记录
  FrameReferenceSummary frame_summary;
};

// ── 局部模型文件身份（合同 §7.1: 每控制点局部数值窗口）──────────────────────
enum class LocalModelKind { kPsf = 0, kCovariance = 1, kDrizzleOperator = 2,
                            kSupport = 3, kScale = 4, kCalibrationUncertainty = 5 };
enum class LocalModelDomain { kCalibratedPlanar = 0, kPhotoAppliedPlanar = 1,
                              kDrizzledSphere = 2 };

const char* to_string(LocalModelKind kind);
const char* to_string(LocalModelDomain domain);
const char* to_string(ReferenceKind kind);
const char* to_string(PrecisionAvailability v);
const char* to_string(PrecisionAccuracy v);
const char* to_string(CovarianceCondition v);

}  // namespace acsd::normalize
