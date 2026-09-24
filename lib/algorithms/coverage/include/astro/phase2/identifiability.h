// lib/algorithms/coverage/include/astro/phase2/identifiability.h
//
// Phase2 唯一的「可辨识性 / 病态」判据（天光面与 UPM/GLS 共用同一实现）。
//
// ---------------------------------------------------------------------------
// 规范依据
// ---------------------------------------------------------------------------
//   docs/science/PHASE2_UPM.md §7a:196-198
//     「κ 上限不得是两个不同的标定值……门控必须……判据按**矩阵谱自身**定
//      （相对 rank_rtol 口径），**不得**保留互相矛盾的绝对常数。」
//   docs/science/NOISE_MODEL.md §5d（范式）：「判据的零假设值由理论给定、拟合的
//      可行域由数据给定，均不引入可调标定常数。」
//
// ---------------------------------------------------------------------------
// 判据（唯一，尺度不变，不含按数据集标定的常数）
// ---------------------------------------------------------------------------
// 设 H 为**实际求解所用的信息矩阵**（H = AᵀWA，A 白化设计矩阵），先做列均衡
//     H_eq = D⁻¹ H D⁻¹,   D = diag(sqrt(H_ii))
// （列均衡消除**参数单位**的自由度：裸 κ 在参数列缩放下会变，见
//  Golub & Van Loan《Matrix Computations》§2.6.2；均衡后的相对谱不变。）
// 记 H_eq 的特征值 λ_1 ≥ … ≥ λ_n，唯一阈值 τ = rank_rtol，则定义
//
//     r_eff  = #{ i : λ_i > τ · λ_1 }        ← 有效秩（计数）
//     κ      = λ_1 / λ_n                     ← 严重度（秩亏时 = +inf，不发布伪值）
//     identifiable ⟺ r_eff == n ⟺ λ_n > τ·λ_1 ⟺ κ < 1/τ
//
// **「病态」与「欠定」是同一条不等式的两种读法**：λ_n 是最小特征值，最先跌破
// τ·λ_1，故 κ > 1/τ ⟺ r_eff < n（自证，两步；Hansen《Regularization Tools》
// v4.1 手册 §2.2 把「条件数大」与「数值秩不确定」并列为同一现象的两面，
// §2.3.1 给出 κ = σ_1/σ_n）。所以判决只有**一个位**，κ 与 r_eff 只是同一把尺
// 的两种读数，不是两条路径。
//
// ---------------------------------------------------------------------------
// 为什么不需要标定常数（τ 从精度来，不从数据来）
// ---------------------------------------------------------------------------
// τ 是**相对**阈值，其默认值由浮点算术本身给出（本实现取
// p2_identifiability_rank_rtol_floor = max(m,n)·eps，与 NumPy
// linalg.matrix_rank 的 tol = S.max()*max(M,N)*eps、MATLAB rank 的
// max(size(A))*eps(norm(A))、SciPy linalg.lstsq/pinv 的 cond 默认
// max(M,N)*eps 同一约定）：
//   计算得到的 λ_i 不可能比 eps·‖H‖ 更准，故低于 τ·λ_1 的方向在浮点意义下
//   **与零不可分辨**——这是关于算术的陈述，不是关于物理量的陈述。
// NumPy 文档 Notes 原文：「The thresholds above deal with floating point
// roundoff error in the calculation of the SVD. However, you may have more
// information about the sources of error in A that would make you consider
// other tolerance values」——即默认阈值只由精度与阶数决定，只有独立已知
// A 的误差模型时才允许改，而那时 τ 仍来自误差模型，不来自「让本数据集通过」。
//
// 本仓把 τ 登记为 FZ-AP2S-RANK-RTOL（冻结值 1e-10）。它**不是一个需要按数据集
// 标定的物理常数**：1e-10 是比精度地板 max(m,n)·eps（本产品量级 ≈6e-11）更保守
// 的数值常数；两个求解器从此共用**同一个符号、同一个值、同一个函数**，不再有
// 1e6 / 1e8 这类「一放一拦」的绝对常数。
//
// ---------------------------------------------------------------------------
// 判据必须落在**未正则化**的 H 上（否则是恒真门）
// ---------------------------------------------------------------------------
// 正则化后的矩阵条件数**是 λ 的函数**，因此它的门限值不是一个数据的性质：
//   · 岭正则（P = I，UPM/GLS 的情形）：κ(H + λI) = (λ_1+λ)/(λ_n+λ) 单调 ↓ 1，
//     于是**任何** H（包括精确秩亏的）只要 λ 够大就能过任何有限门。
//     可执行的见证：H = [[1,1],[1,1]]（rank 1）配 λ = 1e-9 ⇒
//     κ(H+λI) = 2e9 < 1/1e-10，旧天光面门判绿，而本判据判红——见
//     eng/tests/unit/v6_p2_sky_kappa/v6_p2_sky_kappa_test.cpp B9-B11。
//   · 奇异半正定 P（天光面的二阶差分模板 P = DᵀD，其零空间非空）：
//     κ(H + λP) 不再单调趋 1，而是随 λ 先降后升——但它**仍然随 λ 大幅摆动**
//     （实测量级见 run/UPM-KAPPA-UNIFY-01/REPORT.md），且沿 range(P) 的方向上
//     同样是「加 λ 买曲率」。门值仍是实现旋钮的函数。
// 两种情形下在 H_solve 上设门都对「原问题是否可辨识」零信息。Hansen《Regularization Tools》
// v4.1 手册 §1 把这一点列为离散不适定问题的三条主要困难之一：
//     「replacing A by a well-conditioned matrix derived from A does not
//       necessarily lead to a useful solution」
// §2.7.3 进一步把正则化刻画为引入一个「new problem」——它的条件数描述的是新
// 问题，与原问题的可辨识性无关。
// ⇒ 本模块只接受**数据信息矩阵**（H_red）；正则化矩阵的 κ 只能作为「求解稳定性」
//   诊断量单独记账（P2SkyPlaneInfo::kappa_solve），**不得**进入判决。
//
// ---------------------------------------------------------------------------
// 自由度（与残差口径的接合点）
// ---------------------------------------------------------------------------
// Andrae, Schulze-Hartung & Melchior (2010, arXiv:1012.3754) 式 (8)(9)：
//     P_eff = tr(Hat) = rank(X) = rank(A)，dof = N − P_eff ≥ N − P；
// 正文「The standard claim … K = N − P. Is this correct? No, not necessarily so.」；
// §5：「N − P **if and only if** the basis functions … are linearly independent」。
// 故 χ²_red 的分母必须是 n_obs − rank(X)，**不得**用 n_obs − n_params（秩亏时后者
// 会系统性低估 χ²_red，并完全掩盖未被约束的方向数）。本模块输出 r_eff 供调用方
// 计算 dof，χ² 本身不参与判决——文献中「用单一判据同时兼任拟合优度与可辨识性」
// 没有标准做法，不应声称有。
// 注意 rank(X) 是**完整设计矩阵**的秩，不等于本函数返回的 r_eff 当且仅当调用方
// 把辅助（被精确消去的）参数块排除在外时：天光面把 δ_k Schur 消元后只判 H_red，
// 故 dof 用的是 r_eff(H_red) + (n_frames−1)·m（见 P2SkyPlaneInfo::rank_full）；
// UPM 的 per-control 块判据则把每个块的 r_eff 累加成全局 rank_eff（块对角）。
//
// ---------------------------------------------------------------------------
// 边角语义（调用方必须知道的两种「不可评估」与两种「未约束」）
// ---------------------------------------------------------------------------
//   · H_ii == 0（或非有限）⇒ 第 i 个参数**完全没有信息**：它是一个未被约束的
//     方向，计入 n_unidentified（不是"评估失败"）。存在这种方向时 κ 记 +inf。
//   · H_ii < 0 ⇒ 输入不是法方程（法方程对角必为 Σ w j² ≥ 0）⇒ 返回 1 拒绝，
//     不取 |H_ii| 蒙过去。
//   · 判红不是错误：assess 返回 0 且 identifiable == 0。
//   · n_params == 0 / 空指针 / 非有限对角 ⇒ 返回 1（参数错误）。
//
// ---------------------------------------------------------------------------
// 怎么证伪它（本判据自带可红可绿的可执行反例）
// ---------------------------------------------------------------------------
//   eng/tests/unit/v6_p2_identifiability/  —— 正例/负例/尺度不变/独立 Jacobi Oracle 对拍
//   eng/tests/unit/v6_p2_sky_kappa/        —— 天光面侧接线 + 旧恒真门的可买绿见证
//   eng/tests/unit/v6_p2_sky/              —— 自适应回路单旋钮（放粗/细化）双向
//   eng/tests/unit/v6_p2_upm/              —— UPM/GLS 侧接线 + 旧绝对常数门的反例
#pragma once

#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#define P2_IDENT_API __declspec(dllexport)
#else
#define P2_IDENT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 判据结果。identifiable 是**唯一的判决位**；其余都是让判决可被解读的读数。
typedef struct {
    std::uint64_t n_params;        // 参数数 n
    std::uint64_t n_obs;           // 观测数 m（仅用于推导精度地板；可为 0）
    double rank_rtol;              // 请求的 τ
    double rank_rtol_effective;    // 实际生效的 τ = max(请求值, max(m,n)·eps)
    double lambda_max;             // 列均衡矩阵的最大特征值 λ_1
    double lambda_min;             // 最小特征值 λ_n（秩亏/非正定 ⇒ 0）
    double kappa;                  // λ_1/λ_n；秩亏 ⇒ +inf（**不发布伪值**）
    std::uint64_t rank_eff;        // r_eff = #{λ_i > τ_eff·λ_1}
    std::uint64_t n_unidentified;  // n − r_eff：未被数据约束的方向数
    double equilibrate_min;        // D 的最小对角（诊断：列均衡强度）
    double equilibrate_max;        // D 的最大对角
    int identifiable;              // 1 = r_eff == n；0 = 判红
} P2Identifiability;

// 判据。H 为 n×n 对称矩阵（row-major），**必须是未正则化的数据信息矩阵**
// （传 H_solve = H + λP 进来等于把判据退化成恒真门，见文件头）。
// rank_rtol <= 0 / 非有限 ⇒ 取精度地板 max(n_obs,n_params)·eps。
// 返回 0 = ok（**含判红**，判红不是错误）；1 = 参数错误（H/out 为空、n==0、
// 任一对角为负或非有限 ⇒ 不是合法法方程）。H_ii == 0 不算错误：该参数被记为
// 未约束方向（n_unidentified 计入它）。
P2_IDENT_API int p2_identifiability_assess(const double* H, std::uint64_t n_params,
                                           std::uint64_t n_obs, double rank_rtol,
                                           P2Identifiability* out);

// 精度地板：τ_floor = max(m, n) · eps。这是**由算术推出**的数值阈值，不是标定常数。
P2_IDENT_API double p2_identifiability_rank_rtol_floor(std::uint64_t n_obs,
                                                       std::uint64_t n_params);

// 有效自由度（Andrae et al. 2010 式 (9)）：dof = n_obs − rank(X)。
// rank_eff 参数应传**完整设计矩阵**的秩（不是 n_params）。秩亏时它严格大于
// n_obs − n_params；返回值可为非正（n_obs ≤ rank，此时 χ²_red 无定义，
// 调用方必须按「无自由度」处理而不是取 0）。
P2_IDENT_API double p2_identifiability_dof(std::uint64_t n_obs,
                                           std::uint64_t rank_eff);

#ifdef __cplusplus
}
#endif
