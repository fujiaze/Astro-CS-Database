// ACSD Normalize — 唯一 frame_science.json sidecar 序列化/读取（NORMALIZE_DATA §7/§8）
//
// 正本: docs/engineering/contracts/NORMALIZE_DATA.md §7/§8
//       eng/contracts/schemas/normalize_frame_science.schema.json（机器形状）
// 边界: 一个 JSON serializer/reader（不各阶段手写一套解析）; writer 是唯一最终
//       发布者（本库只提供"写 staging + 原子发布"与"读回验证"原语, 发布时机由
//       writer 节点决定）。读取兼容（§8）: 无 sidecar 的旧 HiPS ⇒ precision
//       unavailable, 不得从 catalogue snr_psf / ACSD_FRAME_SNR 自动补 canonical。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "acsd/normalize_data.h"

namespace acsd::normalize {

// ── sidecar 字段（schema properties 的逐字映射; 未知数用 null/空 reason）──────
struct SignalSection {
  std::string hips_path = "signal";
  std::string bunit = "ADU/sr";
  std::string linear_unit = "adu";
  std::string scale_state = "native_calibrated";  // native_calibrated|relative_scaled|response_calibrated
  std::string ordering = "nested";
  int leaf_order = 0;
  int tile_width = 512;
  std::string invalid_policy = "nan";
};

struct WcsSection {
  std::string status = "solved";          // solved（unsolved 帧不发布 frame product）
  std::string source = "ipv";             // ipv|explicit_config
  std::string frame = "icrs";
  std::string projection = "TAN";
  std::string pixel_origin = "fits_1based";
  std::vector<double> crval_deg{0.0, 0.0};
  std::vector<double> crpix{0.0, 0.0};
  std::vector<double> cd_deg_per_pixel{0.0, 0.0, 0.0, 0.0};
  bool sip_present = false;
  std::vector<double> sip_a, sip_b, sip_ap, sip_bp;  // i*6+j 布局
  int sip_order = 0, sip_ap_order = 0;
  std::string catalogue_id;
  std::string catalogue_epoch;
  std::string epoch_propagation = "not_applicable";  // applied|not_applied|not_applicable
  std::optional<double> rms_arcsec;
  struct Attempt {
    std::string hint_source;
    std::string status;   // solved|unsolved
    std::string reason;
    std::optional<int> n_detections;
    std::optional<int> n_matches;
  };
  std::vector<Attempt> attempts;
};

struct PhotometrySection {
  std::string status = "native_unscaled";  // response_calibrated|relative_only|native_unscaled
  std::string response_id;
  std::string reference_id;
  std::string application_id;
  std::string scale_model_id;
  bool scale_applied = false;
  std::string parameter_uncertainty = "unmodelled";  // modelled|approximate|unmodelled
  std::string reason;
};

struct CalibrationSection {
  std::vector<std::string> master_ids;
  std::string uncertainty = "unmodelled";  // modelled|approximate|unmodelled
  std::vector<std::string> omitted_terms{"master_uncertainty"};
};

struct DrizzleSection {
  std::string operator_model_id;
  double pixfrac = 1.0;
  std::string normalization = "sum_overlap_over_drop_times_source_area";
  std::string geometry = "chord_approximation";  // curve_integrated|chord_approximation
  std::string correlation = "unmodelled";        // modelled|approximate|unmodelled
};

struct ModelFileRef {
  std::string model_id;
  LocalModelKind kind = LocalModelKind::kPsf;
  LocalModelDomain domain = LocalModelDomain::kDrizzledSphere;
  std::string representation;
  std::string path;      // 产品内相对路径
  std::string sha256;
  PrecisionStatus status;
};

struct FrameScienceDocument {
  // 身份（schema required）
  std::string run_id, frame_id;
  std::uint32_t input_occurrence = 0;
  std::string source_sha256, config_sha256, software_sha;
  SignalSection signal;
  WcsSection wcs;
  PhotometrySection photometry;
  ReferenceDefinition reference;
  std::vector<ModelFileRef> models;
  CalibrationSection calibration;
  DrizzleSection drizzle;
  SparseReferencePrecision precision;
  std::vector<int> source_dimensions{0, 0};
  double exposure_seconds = 0.0;
  std::string exposure_source;
};

// ── 每控制点局部模型文件载荷（§7.1: dtype=float64, 数组按声明顺序, 矩阵 row-major）
struct LocalModelPayload {
  LocalModelKind kind = LocalModelKind::kPsf;
  LocalModelDomain domain = LocalModelDomain::kDrizzledSphere;
  std::string unit;
  std::string control_id;
  std::vector<std::int64_t> source_pixel_index;   // 源像元下标
  std::vector<double> values;                     // 主数值数组 (P / h / C 行优先)
  std::vector<std::uint64_t> leaf_ipix;           // sphere 叶
  int matrix_rows = 0, matrix_cols = 0;           // values 为矩阵时的形状
  // covariance 专有
  std::optional<std::uint64_t> covariance_rank;
  std::optional<double> condition_number;
  std::vector<double> background_basis;           // Z (row-major n_leaf × n_bg)
  int n_bg = 0;
  std::vector<std::string> random_components;
  std::vector<std::string> omitted_terms;
  // drizzle_operator 专有
  std::vector<double> source_omega;               // Ω_j (sr)
  std::vector<double> source_a_drop;              // A_drop,j (sr)
  std::vector<double> source_n_leaf;              // 每叶完整 N_l (sr; 同次生产)
  std::vector<std::int64_t> csr_row_offsets;
  std::vector<std::int32_t> csr_column_indices;
  std::vector<double> csr_coefficients;
  // support 专有
  std::string support_note;
};

// 结果（落盘 + 哈希）
struct ModelWriteResult {
  bool ok = false;
  std::string error;
  std::string path;      // 相对产品根
  std::string sha256;
};

// ── 原子发布 frame_science.json（写私有临时 → 计算 hash → rename → 重开验证）──
// models_dir: 局部模型文件目录（相对 product_root; 不存在则创建）。
// 返回 false 时不留下可见半成品（不冒充完整产品）。
bool write_local_model_file(const std::string& product_root,
                            const LocalModelPayload& payload,
                            ModelWriteResult* out);

bool write_frame_science_document(const std::string& product_root,
                                  const FrameScienceDocument& doc,
                                  ModelWriteResult* out);

// ── 读取兼容（§8）───────────────────────────────────────────────────────────
struct FrameScienceReadResult {
  bool ok = false;                 // false = 无法读作 canonical precision 真源
  std::string error;               // 具体原因（损坏 = 数据错误, 不是 unavailable）
  bool sidecar_present = false;    // false = 无 sidecar ⇒ signal 可读, precision unavailable
  bool schema_supported = false;
  FrameScienceDocument doc;
  // 物理一致性核对（shape 之外的 §8 末段各项）
  std::vector<std::string> findings;
};

// path = <frame product root>/frame_science.json。文件不存在 ⇒
// ok=false 且 sidecar_present=false、error 说明 canonical precision unavailable
// （调用方不得因此否定 signal 产品本身）。文件损坏 ⇒ ok=false 且 error 为数据错误。
FrameScienceReadResult read_frame_science_document(const std::string& path);

// 旧产品（无 sidecar）判定辅助: 是否看起来像 Phase1 帧产品目录（有 signal/ 子树）。
bool looks_like_phase1_frame_product(const std::string& frame_product_root);

// ── 发布（writer 唯一最终发布者; NORMALIZE_DATA §7）──────────────────────────
// staging_dir: drizzle 节点的 run-private 暂存（含 models/ 与 frame_science.json）;
// product_root: <output_dir>/<frame_key>。步骤:
//   1. 读 staging sidecar 文本（不重新序列化 ⇒ 形状仍是唯一 serializer 的产物）;
//   2. 逐模型文件复制到 <product_root>/models/（原子写）+ 重算 sha256 与 sidecar
//      声明逐项比对（不一致 ⇒ 发布失败, 不落下悬空引用）;
//   3. 原子写 <product_root>/frame_science.json + read_frame_science_document 回读
//      验证（shape + §8 物理核对）。
// 返回 false 时不留被误识为完整的产物（模型文件复制失败即整体失败）。
bool publish_staged_frame_science(const std::string& staging_dir,
                                  const std::string& product_root,
                                  ModelWriteResult* out);

}  // namespace acsd::normalize
