// ACSD Normalize — 球面参考精度生产接线（调度层 → noise 库数学 + AIO 落盘）
//
// 正本: docs/engineering/contracts/NORMALIZE_DATA.md §4/§5/§6/§7,
//       docs/engineering/architecture/NORMALIZE_PIPELINE.md §2/§6,
//       docs/detail/normalize/contracts.md §2/§5。
// 边界:
//   * 本单元只做「config → typed 输入 → 调算法库 → 注册 outputs」, **不含科学公式**
//     （GLS/秩判定在 acsd::normalize::complete_spherical_reference; 序列化在
//      acsd::normalize::write_frame_science_document）;
//   * 实际算子 R 由 drizzle 生产几何路径导出（hp_drizzle_run_phase1_hips 的
//     overlap capture）, 不在本单元重算几何（NOISE_SNR §5.1）;
//   * 诊断快照不供下游读取; 本单元输出只进 run-private staging, 由 writer 发布。
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "acsd/normalize_data.h"      // ReferenceDefinition / ReferenceKind / ...
#include "acsd/sparse_reference.h"    // PlanarPatchWindow（noise 库的窗口类型）
#include "acsd/normalize_science.h"   // LocalModelPayload / FrameScienceDocument
#include "drizzle_engine.h"   // DrizzleEngine::OverlapTarget / SourceLeafOverlap

namespace acsd::phase1::precision {

using Json = nlohmann::json;
using acsd::normalize::ReferenceDefinition;
using acsd::normalize::PlanarPatchWindow;

// ── 参考定义配置（合同 §5; 两种明确来源 + 显式不可用）────────────────────────
// 未选参考 ⇒ kind=unavailable（不得从实际恒星中位通量发明 canonical reference）;
// 旧 snr.reference_flux_adu / fixed_magnitude 路径只作具名 legacy diagnostic。
ReferenceDefinition parse_reference_config(const Json& snr_cfg, std::string* why);

// ── 运行身份/输入哈希（sidecar 身份字段; 缺项显式空串, 不编造）────────────────
bool load_run_identity(const std::string& output_dir, std::string* run_id,
                       std::string* software_sha, std::string* source_sha);
bool file_sha256_hex(const std::string& path, std::string* out);

// ── 帧 PSF（Moffat4 σ 口径, PSF.md §2）──────────────────────────────────────
// 来源 = p1_sources.json frames[frame_key].psf_params[] 的 sx/sy（PSF 块 Moffat4
// sigma; 禁把检测块 Gaussian FWHM 当 Moffat 参数, DISP-STAR-007）。
// 无可用 PSF ⇒ false（调用方按缺项处理, 不用虚构值）。
// frame_key: p1_frame_key(light)（字符白名单净化后的基名）。p1_sources.json 的
// frames[].file = <节点前缀>_<原始基名>.<ext>; 匹配前两侧都用同一净化归一
// （原始基名可能含 '@' 等被 frame_key 净化的字符）。
bool frame_psf_sigma_px(const Json& sources_doc, const std::string& frame_key,
                        double* out_sigma_px, std::string* why);

// ── 每帧精度请求（调度层从既有产物汇集的 typed 输入）────────────────────────
struct PrecisionFrameRequest {
  int nside = 0;
  int width = 0;
  int height = 0;
  double pixfrac = 1.0;
  // noise model 合格 patch 中心（NoiseWeightModelV1.ctrl_x_px/ctrl_y_px）
  std::vector<double> ctrl_x;
  std::vector<double> ctrl_y;
  double gain_e_per_adu = 0.0;
  ReferenceDefinition reference;
  bool background_constant_fit = true;  // false = known 背景（nuisance 口径）
  double psf_sigma_px = 0.0;
  bool have_psf = false;
  int patch_grid_x = 8;
  int patch_grid_y = 8;
  bool master_uncertainty_modelled = false;
  // 本帧 drizzle 输入面的标度事实（决定 reference_flux 的单位配对）:
  //   photometry_applied=true ⇒ 输入面 = photoapplied（像元值 = α·原生 ADU）;
  //   applied_scale = α。var_plane 以**同一数组标度**给出（= α²·原生 ADU²）。
  bool photometry_applied = false;
  double applied_scale = 1.0;
  double window_radius_px = 0.0;   // 实际使用的窗口半径（support 模型记录用）
  double pixel_scale_arcsec = 1.0; // 输入像元角尺度（query_disc 半径估计用）
};

// 控制点窗口半径（px）的唯一推导（build_capture_targets 与 build_patch_windows
// 共用; 三者取小、下限 4 px）: patch 半径 / PSF 足迹 6σ / 像元预算半径
// sqrt(1024/π)。预算依据: GLS 稠密 Cholesky 的 O(m³/3), m ≤ 1024 时单控制点
// 约 3.6e8 flops。
double precision_window_radius_px(const PrecisionFrameRequest& req);

// 构造捕获目标（叶 ipix → 中心/半径）。pixel_to_sky_fits1 必须施加 FITS 1-based
// 桥接并含 SIP（复用 p1_product_forward_sip 单一实现, 不在本单元重写投影）。
// 半径 = patch 尺寸/2（相邻 patch 窗口不重叠; patch 中心网格来自 noise model）。
std::unordered_map<uint64_t, drizzle::DrizzleEngine::OverlapTarget>
build_capture_targets(
    const PrecisionFrameRequest& req,
    const std::function<void(double, double, double*, double*)>& pixel_to_sky_fits1);

// 由捕获结果构造每控制点平面局部窗口（R 的 CSR→稠密小矩阵 + P + v）。
// 控制点与 req.ctrl_x/ctrl_y 逐一对齐（同序）; 无捕获贡献的点给空窗口（由 GLS
// 侧记 unavailable, 不虚构）。
std::vector<acsd::normalize::PlanarPatchWindow> build_patch_windows(
    const PrecisionFrameRequest& req,
    const std::vector<drizzle::DrizzleEngine::SourceLeafOverlap>& overlaps,
    const std::function<void(double, double, double*, double*)>& pixel_to_sky_fits1,
    double omega_leaf);

// ── staging（run-private; writer 唯一发布者）────────────────────────────────
struct StagedPrecision {
  bool ok = false;
  std::string error;
  std::vector<std::string> model_rel_paths;  // 产品内相对路径（models/...）
  std::uint64_t n_available_controls = 0;
  acsd::normalize::FrameScienceDocument document;  // 备发布的 sidecar 内容
};

// 全链: windows → complete_spherical_reference → 局部模型文件文本 + sidecar 文本。
// identity 字段（run_id/sha/…）由调用方从既有运行上下文填入。
// var_plane: blank-sky 方差平面, **与 drizzle 输入面同一数组标度**（frame_applied
// 时已由 p1_noise_model_for_frame 按 α² 换算; 未施加时即原生 ADU²）。可空 ⇒ 该帧
// 无方差面（控制点由 GLS 侧具名 unavailable, 不用虚构方差）。
StagedPrecision stage_frame_precision(
    const PrecisionFrameRequest& req,
    const std::vector<acsd::normalize::PlanarPatchWindow>& windows,
    acsd::normalize::FrameScienceDocument doc_template,
    const std::string& frame_key,
    const std::vector<float>* var_plane,
    std::vector<acsd::normalize::LocalModelPayload>* model_payloads = nullptr);

}  // namespace acsd::phase1::precision
