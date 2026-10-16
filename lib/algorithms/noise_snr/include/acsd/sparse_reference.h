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
// 数学要点（球面 GLS; NORMALIZE_DATA §6, NOISE_SNR §3/§11.1/§11.4）:
//   观测 y = R w, w = F·P + Z·β + η, η ~ N(0, V=diag(v_j)), C = R V Rᵀ,
//   A_s = [R P, R Z]；V_F = [(A_sᵀ C⁺ A_s)⁻¹]_{FF}。
//   **生产求解路径（NOISE_SNR §11.4, 秩亏安全）** 不组成 Gram 矩阵:
//     Q = R V^{1/2}（nl×m）, W = V^{-1/2} A_src（m×ncol）
//     M = Wᵀ V_r V_rᵀ W,  V_r = Q 的行空间正交基（秩揭示 QR of Qᵀ）
//     V_F = 1 / I_{F|β},  I_{F|β} = ‖(I−Π_Z) W̃[:,0]‖²,  W̃ = Q'ᵀW, Z = W̃[:,1:]
//   **等价代数参考式**（仅用于理解/对拍, 不作为默认大规模算法）:
//     令 G=RᵀR, H=V^{1/2}G V^{1/2}=QᵀQ, B=V^{1/2}G A_src（**V^{1/2} 在左**;
//     G 与 V^{1/2} 一般不交换, 写成 G V^{1/2}A_src 即 F01 反例）:
//       RᵀC⁺R = G V^{1/2} H⁺ V^{1/2} G
//       ⇒ M = A_srcᵀ RᵀC⁺R A_src = Bᵀ H⁺² B      ← **H 的逆是平方（H⁺²）**
//     H ≻ 0（R 满列秩, r=m）时 V_rV_rᵀ=I ⇒ M = A_srcᵀ V⁻¹ A_src（§11.1 恒等式）。
//   H/Q 秩亏 ⇒ 走 §11.4 的保留子空间/零空间投影, 报告秩/条件/容差; 参数不可
//   辨识 ⇒ unavailable（不盲加 epsilon、不用伪逆制造信息量）。
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
  // 逐像元确定性标度 α_j = applied/native（空间 alpha; PHOT-MXY-01 语义
  // α_j = k_photo·m(x_j,y_j)）。**长度必须与 pix_index 对齐**; 空 ⇒ 全窗取
  // cfg.plane_scale 常量。帧级数组由调用方按窗口映射后填入。
  //
  // 空间 alpha 的两处作用（NOISE_SNR §2 总通量口径, 2026-10-16 订正后的
  // 自洽模型, 推导与验证见 实验/normalize-precision-p5/ROUND4_FLUX_CONVENTION.md）:
  //   1) 源泊松 Var_src,j = α_j²·F_native·P_j/g = F_ref·α_j²·P_j/(g·α_*) ;
  //   2) 信号模板 h_j = (α_j/α_*)·P_j（模板必须承载同一空间标度, 不能只改方差）;
  // α_* = reference_alpha_star 是**参考源位置 u=(cx_px,cy_px) 处声明的标度**
  // （α_*(u)=α(u)）, F_ref = α_*·F_native 是该位置参考源在 applied 面的总通量。
  // α_* 不随窗口截断重新归一; Σ_all P=1（analytic_total）保持, 有限窗 ΣP<1
  // 的真实截断由 f_enclosed/f_out 逐点登记, 不用窗 sum 重新定义总通量。
  // 有效 gain 语义: g_eff,j = g·α_*/α_j²（只表示该数组单位, 不覆盖设备原生 gain）。
  std::vector<double> pixel_alpha;
  // α_*(u): 参考源位置 u（= 本窗 cx_px/cy_px）处的确定性标度。0 ⇒ 未设置
  // （pixel_alpha 非空时必须 > 0, 否则该控制点具名拒绝; pixel_alpha 为空时
  // 退化为常量 cfg.plane_scale, 与 α_j ≡ α_* 的标量模型一致）。
  double reference_alpha_star = 0.0;
  // 本控制点的参考通量参数 F_ref(u)（与模板 h 配对的积分通量尺度）。0 ⇒ 未设置
  // ⇒ 用 cfg.reference.flux（帧级常量; 常量 α 或 photo_applied 声明的口径）。
  // 空间 α 场 + native_calibrated 声明时, 由接线方按 F_ref(u) = α_*·F_native
  // = f_ref·(α_*/k_photo) 逐点填写（NORMALIZE_DATA §5「每个控制点分别保存 F_ref」;
  // NOISE_SNR §2 总通量口径）。
  double reference_flux = 0.0;
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
  // ── 标度面事实（NOISE_SNR §2 总通量口径 / PHOTOMETRY §4）──────────────────
  // plane_scale = α = applied/native（确定性线性标度; =1 表示未施加测光）。
  // 源泊松项在 applied 面为 α_j²·F_native·P_j/g; 以参考位置 u 的 applied
  // 总通量 F_ref = α_*·F_native 表示时为 F_ref·α_j²·P_j/(g·α_*), 其中
  // α_* = 参考位置标度（窗口级 PlanarPatchWindow::reference_alpha_star）。
  // α_j ≡ α（常量）时 α_* = α, 泊松项退化为 α·F_ref·P_j/g —— 与
  // NOISE_SNR §2 标量公式 V' = α·F'·P/g 一致, 无窗口 sum 因子。
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
    // 平面模板 h_j = (α_j/α_*)·P_j（与 F_ref 配对; α_* = reference_alpha_star）。
    // 常量 α 时逐位退化为 P。仅当 applied 面确实施加了空间标度才与 profile 不同。
    std::vector<double> planar_template;
    std::vector<double> source_pixel_alpha;      // α_j（与 pix_index 对齐）
    double reference_alpha_star = 0.0;           // α_*(u)（参考源位置声明标度）
    double reference_window_scale = 0.0;         // Σ_j α_j·P_j（窗口诊断, 不参与归一）
    std::vector<double> h_sphere;          // h = R·(α/α_*)·P（长度 nl; 1/sr）
    std::vector<double> source_variance;   // v_j（长度 m; C=R diag(v) Rᵀ 的精确因子）
    std::vector<double> covariance;        // C = R diag(v) Rᵀ（nl×nl row-major; 仅预算内）
    std::vector<double> z_sphere;          // Z_sphere（nl×n_bg row-major）
    int n_bg = 0;
    std::vector<std::string> random_components;  // 本窗口已建模随机分量
    std::vector<std::string> omitted_terms;      // 被省略/截断项（不静默）
    bool covariance_explicit = false;     // true = covariance 是显式 nl×nl 矩阵
    // solver 诊断（NORMALIZE_DATA §7.1「rank/condition/solver/relative_tol」;
    // NOISE_SNR §11.4「同时报告 Q 的有效秩/奇异谱、背景基有效秩、源通量信息残差」）
    std::string solver;                    // pivoted_householder_qr_rowspace |
                                           // jacobi_retained_subspace
    int solver_rank = 0;                   // rank(C_sphere)
    double solver_relative_tol = 0.0;      // 秩判定相对容差
    // 条件数的**估计方法**（NOISE_SNR §11.6 第二轮审查: 估计方法必须随值落盘,
    // 不能把估计当作 κ₂(C) 的独立准确值）:
    //   pivot_norm_ratio_squared   — QR 路径: (max/min 保留 pivot 列范数比)²,
    //      是 C 的 σ²max/σ²min 同口径保守估计（pivot 范数 = B=Qᵀ 的列范数 =
    //      Q 的奇异值; rank_tol 作用于这些奇异值, 不是 H 的特征值）;
    //   retained_eigenvalue_ratio  — Jacobi 回退路径: 保留子空间内 H=QᵀQ 的
    //      特征值精确比 λmax/λmin = κ₂(C), 但其 rank_tol 作用于**特征值**
    //      （奇异值平方）尺度, 与 QR 路径同数值阈值的截断语义不同。
    std::string solver_condition_estimate_method;
    double solver_condition_number = 0.0;  // 估计/精确（见 SolverDiagnostics 注释）
    int background_basis_rank = 0;         // Z_w 的有效秩（共线列被识别并记录）
    double flux_information = 0.0;         // I_{F|β} = ‖(I−Π_Z)t‖²（显式残差平方范数）
    double flux_response_norm = 0.0;       // ‖t‖（通量响应在有效子空间的范数）
    double flux_residual_norm = 0.0;       // ‖t−Π_Zt‖（共线背景外可辨识残差范数;
                                           //  ≈0 即通量方向落入背景列空间）
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

// ── 秩亏 GLS 的 solver 诊断（NOISE_SNR §11.4 / NORMALIZE_DATA §7.1）──────────
// 同一字段在两条路径下口径不同, 由 solver 名字标识, 不由字段名暗示唯一含义:
//   * pivoted_householder_qr_rowspace: 生产路径。对 B = D^{1/2}Rᵀ（=Qᵀ, Q=RD^{1/2}）
//     做列主元 Householder QR 求 range(Qᵀ) 正交基, M = WᵀV_rV_rᵀW,
//     W = D^{-1/2}[P,Z_planar]; V_F 由 I_{F|β}=‖(I−Π_Z)t‖² 反解得（残差显式
//     构造, 不用 ‖t‖²−‖Π_Zt‖² 相减——后者在近共线时灾难性消去, §11.6 审查修正）。
//     covariance_rank = rank(Q) = rank(C_sphere); rank_tol 作用于 Q 的奇异值
//     （pivot 列范数）, 不是 H=QᵀQ 特征值的平方口径; condition_number 为
//     (max/min 保留 pivot 比)² 的**保守估计**（估计方法见
//     solver_condition_estimate_method）, 与 rel_tol、rank 一并落盘供审计。
//   * jacobi_retained_subspace: 仅在稠密工作集 m·nl 超过预算时的正确优先回退,
//     由 spherical_reference_jacobi_oracle 求得; 不冒充已用 QR。其秩判定作用于
//     H 的特征值尺度, condition_number 是保留特征值精确比。
struct SolverDiagnostics {
  std::string solver;                 // 见上
  int rank = 0;                       // rank(C_sphere)
  double relative_tol = 0.0;          // 秩判定相对容差
  double condition_number = 0.0;      // 估计/精确（见 solver 注释）
};

// ── 独立数值 oracle（仅测试/诊断, 不在生产调用路径）────────────────────────
// 用循环 Jacobi 特征分解 + 显式 H⁺²（H = V^{1/2}RᵀRV^{1/2}）重算
//   M = A_srcᵀ Rᵀ(RDRᵀ)⁺R A_src, 再取 [(M⁻¹)]_FF。
// 与生产 QR 行空间投影是**不同代码路径**, 用于 verify 程序对拍; 其数学等价性见
// NOISE_SNR §11.4（满列秩时同为 A_srcᵀD⁻¹A_src）。
struct JacobiOracleResult {
  bool ok = false;
  double flux_variance = 0.0;   // V_F = [(A_srcᵀ Rᵀ(RDRᵀ)⁺R A_src)⁻¹]_FF
  int covariance_rank = 0;      // rank(C_sphere) = rank(H)
  double condition_number = 0.0;// 保留特征值精确比 λmax/λmin
};

// v_override 为空时使用 w.variance（即调用方已把参考源 Poisson 等并入的方差）。
// h_override 非空时以其作为平面通量模板列 A_src[:,0]（生产路径传入
// (α_j/α_*)·P_j, 与 QR 路径同一模板; 空 ⇒ 用 w.profile）。
JacobiOracleResult spherical_reference_jacobi_oracle(
    const PlanarPatchWindow& w, int n_bg, const std::vector<double>& v_override,
    const std::vector<double>* h_override = nullptr);

}  // namespace acsd::normalize
