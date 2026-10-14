// ACSD Normalize — 球面参考精度的 GLS 完成（数学唯一实现在 noise 库）
//
// 正本: docs/science/noise_snr/NOISE_SNR.md §3/§11/§12
//       docs/engineering/contracts/NORMALIZE_DATA.md §5/§6/§7.1
//       docs/science/normalize/NORMALIZE_SCIENCE.md §7/§12
//
// 被估计量（三个不同估计量之一，禁止互相冒充）:
//   sparse_snr_controls(u) = 指定天球位置、局部 PSF/噪声/面积/背景条件下，对
//   **固定参考点源**的条件信息量。SNR_ref,k(u) = F_ref,k / sqrt(V_F,k(u))。
//
// 公式（合同 §6）: y = F h + Z β + n, C 为参考源通量条件下冻结的条件协方差
//   V_F = [(A^T C^{-1} A)^{-1}]_{FF}, A = [h, Z]
// 球面完成（合同 §4/NOISE_SNR §11）: 用**同一次 Drizzle 的实际 R**,
//   h_sphere = R h_planar, C_sphere = R C_planar R^T, Z_sphere = R Z_planar。
// 局部 R 可能秩亏：不得给 C_sphere 盲加 epsilon；用显式秩判定/分解，报告秩与
// 条件数；参数不可辨识 ⇒ precision unavailable（不用伪逆制造信息量）。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "acsd/normalize_data.h"

namespace acsd::normalize {

// 单个控制点的平面局部窗口输入（由 Drizzle 节点按 patch 中心汇集; FP64）。
//
// 窗口语义（合同 §4/§7.1）: 源像元集合 (patch 内对目标叶及其邻叶有贡献的像元)
// 与它们实际落入的球面叶集合。R 为 (n_leaf × m) 实系数矩阵:
//   S_l = Σ_j c_lj x_j,  c_lj = (a_lj/A_drop,j)·(D_l/N_l)
// 其中 D_l = Σ_j a_lj, N_l = Σ_j (a_lj/A_drop,j)·A_pixel,j —— 与生产 signal
// 的**同一几何系数**（信号算子与协方差算子共享几何, NOISE_SNR §5.1）。
struct PlanarPatchWindow {
  std::string control_id;
  double cx_px = 0.0;      // patch 中心 (detector, 0-based)
  double cy_px = 0.0;
  std::vector<std::int64_t> pix_index;   // 源像元下标 (row-major, y*w+x), m 个
  std::vector<double> profile;           // P_j: 单位点源通量的像元积分响应, ΣP=1
  std::vector<double> variance;          // C_planar 对角 (blank-sky, 数组标度 ADU^2)
  std::vector<double> src_x;             // 像元 x (0-based, 背景基用)
  std::vector<double> src_y;             // 像元 y
  std::vector<std::uint64_t> leaf_ipix;  // NESTED 叶, n_leaf 个
  int leaf_order = 0;
  double omega_leaf = 0.0;               // 4π/(12·nside²) (sr)
  std::vector<double> r_coeff;           // row-major n_leaf × m 系数 (1/sr)
  std::vector<double> leaf_dp;           // 每叶 D_l = Σ_j a_lj (sr)
  std::vector<double> leaf_np;           // 每叶 N_l (sr)
  std::vector<double> leaf_support;      // 每叶 min(D_l, Ω_leaf) (sr, 不重复计面积)
};

// 背景 nuisance 口径（合同 §5）
enum class BackgroundNuisance { kKnown = 0, kConstantFit = 1, kPlaneFit = 2 };

struct SphericalCompletionConfig {
  ReferenceDefinition reference;
  BackgroundNuisance background = BackgroundNuisance::kKnown;
  // 精度判定输入（调用方如实登记，不得伪造）:
  double gain_e_per_adu = 0.0;      // <=0 = 未知 ⇒ 源泊松项缺项
  bool master_uncertainty_modelled = false;  // false ⇒ omitted_terms 记 master_uncertainty
  bool template_fixed = true;       // 模板/有效域固定（upper_bound 的前提）
  double support_rank_tol = 1e-10;  // 秩判定相对容差
};

struct SphericalCompletionResult {
  SparseReferencePrecision precision;
  // 逐点局部模型文件内容（serialize 由 writer 侧统一负责; 这里给出数值载荷）
  struct LocalWindow {
    std::string control_id;
    std::vector<std::int64_t> pix_index;   // 源像元下标
    std::vector<double> profile;           // P ( planar )
    std::vector<double> h_sphere;          // h = R P (1/sr)
    std::vector<double> variance;          // C_sphere 对角 (h 同标度平方)
    std::vector<double> z_sphere;          // Z_sphere 列 (benign nuisance 基)
    int n_bg = 0;
    double d_p = 0.0, n_p = 0.0;
    double support_area_sr = 0.0;
    double a_drop_sum = 0.0;
    double omega_sum = 0.0;
  };
  std::vector<LocalWindow> windows;
  std::vector<std::string> failure_reasons;  // 逐点 unavailable 的具名原因
};

// 球面完成: 对每个平面 patch 窗口, 用实际 R 构造 h/C/Z 并求 GLS 的 V_F。
// 返回的 precision.controls 与输入窗口**逐一对齐**（同序）; 某点不可辨识时该点
// status.availability=unavailable 且带原因（不回填 0/NaN 冒充可估计）。
SphericalCompletionResult complete_spherical_reference(
    const std::vector<PlanarPatchWindow>& windows,
    const SphericalCompletionConfig& cfg);

// 帧级摘要: area-weighted median of log(SNR) 后取 exp（合同 §5 唯一定义）。
// 权重 = 各有效 patch 的真实球面支持面积; 无有效点 ⇒ status unavailable。
FrameReferenceSummary summarize_frame_reference(
    const std::vector<ReferenceControl>& controls);

}  // namespace acsd::normalize
