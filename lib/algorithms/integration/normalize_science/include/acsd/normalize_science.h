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
  std::vector<double> covariance;                 // 显式 nl×nl（仅作对照/小窗）
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
  // ── PSF 空间标度合同（NOISE_SNR §2 总通量口径, 2026-10-16 接线）──────────
  // h_j = (α_j/α_*)·P_j; 源泊松 Var_src,j = F_ref·α_j²·P_j/(g·α_*)。逐像元 α_j、
  // 参考位置标度 α_* 与平面模板随 psf 模型落盘, 供独立读侧重算
  // （h_sphere == R·planar_template; α_j/α_* 与 photometry spatial_gain 一致）。
  std::vector<double> source_pixel_alpha;   // α_j（与 source_pixel_index 对齐）
  double reference_alpha_star = 0.0;        // α_*(u)（参考源位置声明标度）
  std::vector<double> planar_template;      // h_j = (α_j/α_*)·P_j（长度 m）
  // ── local-factor/v2（NORMALIZE_DATA §7.1 2026-10-15 裁决）────────────────
  // v1 的错位: 把 factor payload values=v_j 的 unit 写成 adu^2/sr^2、
  // matrix_shape 写成 [m,1], 与派生 C 的真实 shape/单位混同。v2 用不可混淆的
  // 字段分别声明逻辑 C shape/单位、因子 shape/单位、算子 shape/单位与 basis。
  std::string factor_variance_unit;   // v_j 的单位（[linear_integrated_signal]^2）
  std::string derived_covariance_unit;// 派生 C = R diag(v) Rᵀ 的单位
  std::string covariance_condition;   // "reference_source" | ...
  std::string operator_unit;          // R 的单位 (sr^-1)
  std::string background_basis_unit;  // Z 与被估计 β 的单位配对
  std::string background_basis_definition;
  std::vector<std::string> assumptions;   // ["diagonal_planar_condition"]
  int operator_rows = 0;              // nl
  int operator_cols = 0;              // m
  int logical_covariance_rows = 0;     // nl
  int logical_covariance_cols = 0;     // nl
  bool exact_factorization_of_model = false;  // 仅当 C=R diag(v)Rᵀ 精确重建成立
  std::string solver;                 // NOISE_SNR §11.4 solver 身份
  double solver_relative_tol = 0.0;
  int solver_rank = 0;
  // 条件数估计方法（NOISE_SNR §11.6: 估计必须带方法名落盘, 不得当作 κ₂(C)）:
  //   pivot_norm_ratio_squared | retained_eigenvalue_ratio
  std::string condition_estimate_method;
  double solver_condition_number = 0.0;
  int background_basis_rank = 0;
  double flux_information = 0.0;
  // 通量方向诊断（NOISE_SNR §11.6: 显式正交残差及其范数; flux_residual_norm ≈ 0
  // 即通量落入背景列空间 ⇒ 不可辨识, 不是"信息量小"）
  double flux_response_norm = 0.0;    // ‖t‖（有效子空间内通量响应范数）
  double flux_residual_norm = 0.0;    // ‖t−Π_Z t‖（背景列空间外可辨识残差范数）
  std::vector<std::int64_t> csr_row_offsets;
  std::vector<std::int32_t> csr_column_indices;
  std::vector<double> csr_coefficients;
  // support 专有
  std::string support_note;
  // PSF 像元求积的阶自检误差（PSF.md §4「integration method/order/估计误差」;
  // 负值 = 未能估计, 不得记为 0 误差）
  double pixel_integration_error = 0.0;
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
// ── local-factor/v2 的读侧重算（NORMALIZE_DATA §7.1/§8 末段）────────────────
// 目的: reader 不只要"能解码 JSON", 还要能从同一控制点的 factor(v_j) 与算子
// CSR(R) **精确重建** C_sphere = R diag(v) Rᵀ, 并核验 shape/单位/hash 三者一致;
// 这正是 §8 末段「C factor 与派生单位」要求的执行者校验。
struct FactorReconstruction {
  bool ok = false;                 // 重建成功且单位/shape 自洽
  std::string error;               // 具体数据错误（不是 unavailable）
  std::vector<double> c_row_major; // nl×nl row-major（仅在允许大小时返回）
  int rows = 0, cols = 0;
  double derived_unit_sr_power = 0.0;  // 派生 C 的 sr 幂次（须为 -2）
  double factor_unit_sr_power = 0.0;   // v_j 的 sr 幂次（须为 0）
  double operator_unit_sr_power = 0.0; // R 的 sr 幂次（须为 -1）
  // NORMALIZE_DATA §7.1B: 除 sr 幂次外还核对**线性单位自身**（L 是该 frame 真正
  // 施加 photometric scale 后的线性 signal 单位; "adu^2" 与 "electron^2/sr^2"
  // 即使 sr 幂次正确也不可组合）。checked=true 表示已按 expected 核对通过。
  std::string factor_linear_unit;    // 解析出的因子线性单位（如 "adu^2"）
  std::string derived_linear_unit;   // 解析出的派生线性单位
  bool linear_unit_checked = false;
  double max_abs_diff_vs_payload = 0.0;  // 与 payload.values 的逐项最大差
};

// 从已解析的 payload + 同控制点算子 payload 重建 C 并核验单位/shape。
// dense_limit 为安全阀: nl×nl 超过该元素个数时只核验单位/shape 与 CSR 结构,
// 不返回稠密矩阵（避免为了校验而分配数百 MB）。
// expected_linear_unit: 该 frame 的 signal 线性单位基名（如 "adu"）。因子单位
// 必须是 <expected>^2、派生单位必须是 <expected>^2/sr^2; 空 / 不匹配 ⇒ 具名
// 拒绝（§7.1B: 未知转换不许仅因 sr 指数正确就接纳）。
FactorReconstruction reconstruct_factor_covariance(const LocalModelPayload& covariance,
                                                   const LocalModelPayload& operator_model,
                                                   std::size_t dense_limit,
                                                   const std::string& expected_linear_unit);

// 从产品根读入单份局部模型文件的数值载荷（用于读侧重算）。
// 文件不存在/不可解析 ⇒ ok=false, error 为具体数据错误（不是 unavailable）。
struct LocalModelLoadResult {
  bool ok = false;
  std::string error;
  LocalModelPayload payload;
};
LocalModelLoadResult load_local_model_payload(const std::string& product_root,
                                             const std::string& rel_path);

// 对整个帧产品执行 §8 末段的 factor/单位重算校验; findings 追加到 out。
// 只做结构/单位/shape/（小窗）逐项对拍, 不重算 V_F。
void verify_frame_science_factor_models(
    const std::string& product_root, const FrameScienceDocument& doc,
    std::vector<std::string>* findings);

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
