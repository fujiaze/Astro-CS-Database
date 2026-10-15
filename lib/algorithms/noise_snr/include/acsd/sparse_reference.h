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
// 数学要点（球面 GLS; NORMALIZE_DATA §6, NOISE_SNR §3/§11.1）:
//   观测 y = R w, w = F·P + Z·β + η, η ~ N(0, V=diag(v_j)), C = R V Rᵀ,
//   A_s = [R P, R Z]；V_F = [(A_sᵀ C⁺ A_s)⁻¹]_{FF}。
//   令 Q=R V^{1/2}, G=RᵀR, H=V^{1/2}G V^{1/2}=QᵀQ（H 与 C 的非零特征值相同）:
//     RᵀC⁺R = G V^{1/2} H⁺ V^{1/2} G
//     ⇒ M = A_srcᵀ RᵀC⁺R A_src = Bᵀ H⁺ B,  B = V^{1/2} G A_src
//   H ≻ 0（R 满列秩）时 H⁺=H⁻¹ ⇒ M = A_srcᵀ V⁻¹ A_src（NOISE_SNR §11.1 恒等式）。
//   H 半正定（R 秩亏）时用显式特征分解取保留子空间, 报告秩与条件数; 参数子空间
//   内不可辨识 ⇒ unavailable（不盲加 epsilon、不用伪逆制造信息量）。
//   V_B = Σ_j c_pj²·v_j（目标叶 p 的 C 对角; 同源同位置同条件）。
struct PlanarPatchWindow {
  std::string control_id;
  double cx_px = 0.0;      // patch 中心 (detector, 0-based)
  double cy_px = 0.0;
  double ra_deg = 0.0;     // patch 中心 ICRS（调用方经既有 pix2sky 回填）
  double dec_deg = 0.0;
  int leaf_order = 0;      // 实际叶阶 log2(nside)（非 tile 阶; detail §7.4）
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
  std::vector<double> op_c;              // c_lj = (a_lj/A_drop,j)/N_l (1/sr)
  std::vector<std::uint64_t> leaf_ipix;  // n_leaf 个 NESTED 叶
  std::vector<double> leaf_dp;           // 每叶 D_l = Σ_j a_lj (sr)
  // N_l = 同次生产该叶的**完整**面亮度归一分母 Σ_j w_lj·Ω_j（NOISE_SNR §11.2:
  // 必须来自生产累加器, 不在局部窗口重新归一）。空 ⇒ 调用方未提供（按局部口径
  // 计算并记 truncation, 不当成完整 R）。
  std::vector<double> leaf_n_full;
  // 窗口内已覆盖的 N_l 部分（Σ_{j∈window} w_lj·Ω_j）; 用于判定窗口外贡献被省略的份额。
  std::vector<double> leaf_n_covered;
  bool has_target_leaf = false;  // 显式状态: leaf 编号 0 是合法值, 不充当缺失
  std::uint64_t target_leaf = 0; // 控制点目标叶（由控制点 sky 的 NESTED ang2pix 求得）
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
  double gain_e_per_adu = 0.0;      // <=0 = 未知 ⇒ 源泊松项缺项（**原生** ADU 面 gain）
  bool master_uncertainty_modelled = false;  // false ⇒ omitted_terms 记 master_uncertainty
  bool template_fixed = true;       // 模板/有效域固定（upper_bound 的前提）
  double support_rank_tol = 1e-10;  // 秩判定相对容差
  // ── 标度面事实（NOISE_SNR §11.3 / PHOTOMETRY §4）────────────────────────
  // plane_scale = α = applied/native（确定性线性标度; =1 表示未施加测光）。
  // 源泊松项在 applied 面为 α_j²·F_native·P_j/g；以 F_applied 表示时
  // = α_j·F_applied·P_j/g，等价有效 gain g_eff = g·a_ref/α_j²。
  // 原生 gain 的设备来源保留在 gain_provenance, 不被有效 gain 覆盖。
  double plane_scale = 1.0;
  // 逐像元 α_j（空间标度 D_alpha）; 空 ⇒ 全窗取常量 plane_scale。
  std::vector<double> pixel_scale;
  std::string gain_provenance;      // 原生 gain 来源（config/header/unavailable）
  // 显式 nl×nl 协方差矩阵的落盘预算（合同 §7.1）。超出时该控制点的 covariance
  // 模型改记版本化因子表示（v_j + R 的 CSR）并如实登记, 不伪造 explicit 矩阵标签。
  std::uint64_t covariance_matrix_budget = 256;
};

struct SphericalCompletionResult {
  SparseReferencePrecision precision;
  // 逐点局部模型文件内容（serialize 由 writer 侧统一负责; 这里给出数值载荷）
  struct LocalWindow {
    std::size_t window_index = 0;   // 输入 windows[] 的下标（与 controls 对齐）
    std::string control_id;
    std::vector<std::int64_t> pix_index;   // 源像元下标
    std::vector<double> profile;           // P ( planar )
    std::vector<double> h_sphere;          // h = R P（长度 nl; 1/sr）
    std::vector<double> source_variance;   // v_j（长度 m; C=R diag(v) Rᵀ 的精确因子）
    std::vector<double> covariance;        // C = R diag(v) Rᵀ（nl×nl row-major; 仅预算内）
    std::vector<double> z_sphere;          // Z_sphere（nl×n_bg row-major）
    int n_bg = 0;
    std::vector<std::string> random_components;  // 本窗口已建模随机分量
    std::vector<std::string> omitted_terms;      // 被省略/截断项（不静默）
    bool covariance_explicit = false;     // true = covariance 是显式 nl×nl 矩阵
    double d_p = 0.0, n_p = 0.0;          // 目标叶 D_p 与完整 N_p
    double support_area_sr = 0.0;
    double a_drop_sum = 0.0;
    double omega_sum = 0.0;
    double operator_coverage_fraction = 0.0;  // 窗口像素对 N_p 的覆盖份额
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
