/* variance_propagation.h — Phase2b 归一化方差传播（残差制造者 PΣPᵀ）
 *
 * 权威依据（只读，不改）:
 *   - reports/RELEASE-02/unc-prop-audit.md §3（正确传播公式，M2 合同项）
 *   - reports/RELEASE-02/q2-snr-smooth.md §2/§5/§7（÷g² 必进方差；权重同源）
 *   - lib/algorithms/coverage/include/astro/phase2/upm.h:225-228
 *       C_theta=(JᵀWJ)^-1；C_out=C_stat+J_out C_theta J_outᵀ；禁止权重反推 variance
 *   - docs/plugins/algorithms_phase2/11_upm.md §4.1（校准参数不确定度必须传播）
 *
 * 科学形式（本模块唯一实现，不得旁路）:
 *   归一化把观测 y 映射到被扣除的校正场  ĝ = H y；输出
 *       corrected = y − ĝ = (I − H) y = P y      （P 为"残差制造者"）
 *   线性化下（Σ = Cov(y)，对角 = diag(σ²)）:
 *       Var(corrected) = P Σ Pᵀ + J_out C_theta^ind J_outᵀ
 *       Var(corrected_i) = Σ_j P_ij² σ_j²  +  param_i
 *   其中 H 行可来自任意估计子（(c) 排除自身 / W2 含自身 / 参考帧 gauge），
 *   **本模块不假设参考帧**；参考帧只是 H 的一种特例。
 *
 * 错误形式（禁止用于生产权重）:
 *       σ_i² + Var(ĝ_i) = σ_i² + Σ_j H_ij² σ_j²
 *   它漏掉 −HΣ − ΣHᵀ 交叉项；对 ĝ=ȳ（N 帧均值）给出 σ²(1+1/N)，而正确是
 *   σ²(1−1/N)；N=8 时高估 (1+1/8)/(1−1/8) = 1.2857×。
 *
 * 乘性归一化: corrected=(y−ĝ)/g ⇒ Var /= g²（÷g² 是硬要求，缺它高响应高 SNR
 *   帧被压低 ≈1/g²、权重序可翻转；q2-snr-smooth §2）。
 *
 * fail-closed: 非有限/负 σ²、非法 gain、非法 H 行、长度不符 → 返回 false 并写 err；
 *   不产生 NaN 权重、不静默 clamp。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p2var {

/* H 的稀疏行：一个输出像素/一个 frame i 的 H_i·（ĝ_i = Σ_j H_ij y_j）。
 * frame_index 必须严格升序且唯一；coeff 有限。 */
struct HatRow {
  std::vector<std::uint32_t> frame_index;  /* 列下标 j */
  std::vector<double> coeff;               /* H_ij */
};

/* H 行合法性（升序唯一、有限、下标 < n_frames）。非法 → false + err。 */
bool hat_row_valid(const HatRow& h, std::size_t n_frames, std::string* err);

/* Var(corrected_i) = Σ_j (δ_ij − H_ij)² σ_j²（Σ=diag(sigma2)，残差制造者）。
 * sigma2 长度 = n_frames；i < n_frames。任一 σ² 非有限/负、行非法 → fail-closed。 */
bool residual_maker_variance(const HatRow& h, const double* sigma2,
                             std::size_t n_frames, std::size_t i,
                             double* out_var, std::string* err);

/* 朴素（错误）形式 σ_i² + Var(ĝ_i) = σ_i² + Σ_j H_ij² σ_j²。
 * **仅红例/对照/审计**；生产权重禁用。 */
bool naive_variance(const HatRow& h, const double* sigma2, std::size_t n_frames,
                    std::size_t i, double* out_var, std::string* err);

/* 由帧权重 w_j（>0 有限）构造 H 行：ĝ = Σ_j w_j y_j / W。
 *   include_self=true  → H_ij = w_j/W        （含自身的加权均值；W2 口径）
 *   include_self=false → H_ij = w_j/W_{-i}   （(c) 排除自身；H_ii=0）
 * include_self=false 且 w_i<=0/非有限 → fail-closed（"排除自身"要求自身存在）。 */
bool normalized_weight_hat_row(const double* w, std::size_t n, std::size_t i,
                               bool include_self, HatRow* out,
                               std::string* err);

/* 生产逐像素公式（唯一对外总入口）:
 *   Var(corrected_i) = [ PΣPᵀ_ii + param_var ] / g²
 * gain<=0/非有限、param_var<0/非有限 → fail-closed。gain=1、param_var=0 为
 * 加性-only 方案 B 基线。 */
bool corrected_pixel_variance(const HatRow& h, const double* sigma2,
                              std::size_t n_frames, std::size_t i,
                              double gain, double param_var, double* out_var,
                              std::string* err);

/* w = 1/Var(corrected)（Var>0 有限）[ADU^-2]。Var<=0/非有限 → fail-closed。 */
bool weight_from_variance(double var, double* out_w, std::string* err);

/* 诊断：ĝ=ȳ（N 帧等权均值）的 朴素/正确 方差比 = (1+1/N)/(1−1/N)。
 * N<2 → NaN。仅自证/审计用。 */
double mean_model_naive_over_correct(std::size_t n);

}  /* namespace p2var */
}  /* namespace v6 */
}  /* namespace astrocs */
