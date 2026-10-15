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
// 窗口语义（合同 §4/§7.1）: patch 中心附近、对目标叶及其邻叶有贡献的源像元集合。
// R 以**稀疏三元组**给出（(j, l, c_lj)）, 不生成 nl×m 稠密矩阵（nl 可达数千）。
//
// 数学要点（球面 GLS; NORMALIZE_DATA §6, NOISE_SNR §3）:
//   观测 y = R w, w = F·P + Z·β + η, η ~ N(0, V=diag(v_j)), C = R V Rᵀ,
//   A_s = [R P, R Z]；V_F = [(A_sᵀ C⁺ A_s)^{-1}]_{FF}。
//   实现按 m ≤ nl / m > nl 选两条精确路径（各一次稠密 Cholesky）:
//     m ≤ nl: (RVRᵀ)⁺ = R̃(R̃ᵀR̃)⁻²R̃ᵀ, R̃ = R V^{1/2}
//              ⇒ M = BᵀH⁻²B, B = G V^{1/2}A_src, G = RᵀR, H = V^{1/2}GV^{1/2}
//     m > nl : C = RVRᵀ 直接 Cholesky, M = A_sᵀC⁻¹A_s
//   秩亏 ⇒ unavailable（不用伪逆制造信息量、不加任意地板）。
//   V_B = Σ_j c_pj²·v_j（目标叶 p 的 C 对角; 同源同位置同条件）。
//   两条路径互相一致 + 与显式定义式的一致性由 eng/tests/unit 的 dual 对拍验证。
struct PlanarPatchWindow {
  std::string control_id;
  double cx_px = 0.0;      // patch 中心 (detector, 0-based)
  double cy_px = 0.0;
  double ra_deg = 0.0;     // patch 中心 ICRS（调用方经既有 pix2sky 回填）
  double dec_deg = 0.0;
  int leaf_order = 0;
  double omega_leaf = 0.0;               // 4π/(12·nside²) (sr)
  std::vector<std::int64_t> pix_index;   // 源像元下标 (row-major, y*w+x), m 个
  std::vector<double> profile;           // P_j: 单位点源通量的像元积分响应, ΣP=1
  std::vector<double> variance;          // v_j: 条件方差（数组标度, 与输入面同标度）
  std::vector<double> src_x;             // 像元 x (0-based, 背景基用)
  std::vector<double> src_y;             // 像元 y
  std::vector<double> pix_omega;         // Ω_j: 未收缩源像元球面面积 (sr)
  std::vector<double> pix_a_drop;        // A_drop,j: 收缩 drop 面积 (sr)
  // 稀疏算子三元组（同一几何路径导出; 与生产 signal 的 R 逐项同源）
  std::vector<int> op_pix;               // 像元槽位 (0..m-1)
  std::vector<int> op_leaf;              // 叶槽位 (0..nl-1)
  std::vector<double> op_c;              // c_lj (1/sr)
  std::vector<std::uint64_t> leaf_ipix;  // n_leaf 个 NESTED 叶
  std::vector<double> leaf_dp;           // 每叶 D_l = Σ_j a_lj (sr)
  std::uint64_t target_leaf = 0;         // 控制点目标叶（支持面积最大的叶）
  // 汇总期暂存（由接线方在构造窗口时使用; 不参与科学与序列化）
  std::vector<double> op_raw_a, op_raw_adrop, op_raw_apixel;
  std::vector<int> op_raw_pix, op_raw_leaf;
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
    std::size_t window_index = 0;   // 输入 windows[] 的下标（与 controls 对齐）
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
    std::vector<double> pix_omega;    // 与 pix_index 对齐 (sr)
    std::vector<double> pix_a_drop;   // 与 pix_index 对齐 (sr)
    // 稀疏算子三元组（与输入窗口同源; 供 drizzle_operator 模型文件）
    std::vector<int> op_pix, op_leaf;
    std::vector<double> op_c;
    std::vector<double> leaf_dp;
    std::size_t op_nnz = 0;
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
