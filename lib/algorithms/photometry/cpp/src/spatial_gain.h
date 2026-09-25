#ifndef ASTROCS_PHOTOMETRY_SPATIAL_GAIN_H
#define ASTROCS_PHOTOMETRY_SPATIAL_GAIN_H
// ============================================================================
// spatial_gain.h - 单帧低阶乘性空间增益场 m(x,y) 的拟合（SCI-PHOT-001 §16.1 ④）
//
// ---------------------------------------------------------------------------
// 规范依据（逐条，不自行发明）
// ---------------------------------------------------------------------------
//   · docs/science/PHOTOMETRY.md §16.1 ④（FROZEN）：「逐星 r_i = log10(F_instr/F_syn)
//     → 星等一致性预过滤 → IRLS/Tukey 稳健位置（§5）；同时用星点残差在帧内估计
//     **低阶乘性空间增益** m(x,y)（平场/光学大尺度响应的低阶残余），与 k_photo
//     一并作为标定面」；
//   · 同文件 §16.1 ⑤：「I_photo = k_photo·m(x,y)·I_cal 施加到**整帧像素**」；
//   · 同文件 §16.2：可辨识量只有**乘性标定面** k_photo·m(x,y)。
//
//   规范只规定「低阶」，**未**规定基函数族/阶数/规范自由度。本实现取仓内**既有**
//   逆向验收设计（实验/photometric-magnitude/docs/p1-spatial-gain.md §2.1/§2.2/
//   §2.5；其上游是负责人指令「phase1 的校准应该对能拟合出来的低阶乘法增益校准。
//   也就是把主要残差去掉。不管高阶。」）：
//     · 基函数 = **二维多项式**（像素坐标归一化到 [−1,1]，避免病态）；阶 ∈ {1,2}：
//       order 1 = {x̃, ỹ}，order 2 = {x̃, ỹ, x̃², x̃ỹ, ỹ²}；**order ≥ 3 不启用**
//       （该文件 §3.4 实测 order 3 的负例噪声底是真值信号的 1.3×，明确有害）；
//     · **规范（gauge）**：参与拟合的星集合上 log10 m 的（Tukey 权重加权）均值为 0
//       ⇔ 该集合上 m 的加权几何均值为 1（该文件 §2.1）。因此 k_photo 仍严格是
//       全局项、语义不变；order=0（m≡1）时与既有实现逐位一致，可做回归对照；
//     · **降级（§2.5）**：星数不足 / 覆盖不足 / 可辨识性判红 ⇒ **降阶**，最低到
//       order=0（= 现行全局常数），具名写入 degraded_reason 并落盘；
//       **绝不发布未被数据约束的场**。
//
// ---------------------------------------------------------------------------
// 可辨识性判据 = 仓内**唯一**实现（不另立阈值、不另写秩/条件数判据）
// ---------------------------------------------------------------------------
//   lib/algorithms/coverage/src/identifiability.cpp 的 p2_identifiability_assess
//   （接口语义见 astro/phase2/identifiability.h 文件头）：在**未正则化**的信息
//   矩阵 H = AᵀWA 上判 r_eff == n_params。本模块传 H = Σ w·B̃ B̃ᵀ（实际求解用的
//   那个矩阵，已按列均衡处理在判据内部完成）。
//   两级检查：
//     (a) **预检**（无权 w≡1 的设计几何）→ 决定请求阶能否成立、决定降阶到哪一阶；
//     (b) **后检**（最终 Tukey 权重的 H，即真正被求解的那个矩阵）→ 决定**发布的**
//         场是否可辨识。后检判红 ⇒ 继续降阶；降到 order 1 仍判红 ⇒ m≡1。
//   这样「发布的场」永远来自一个可辨识的加权信息矩阵。
//
// ---------------------------------------------------------------------------
// 为什么规范自由度必须固定（§16.2 的退化）
// ---------------------------------------------------------------------------
//   k_photo·m 的尺度可互换：任意常数 λ 下 (k·λ, m/λ) 给出同一标定面。本模块把
//   λ 固定为「星集合加权几何均值 1」，于是 k_photo 保持为既有定义的全局项，
//   空间项承担且仅承担**零均值**的部分。
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {
namespace photometry {

// 拟合状态（具名；落盘到 provenance 的 spatial_gain.status）
enum class SpatialGainStatus : int {
  kDisabled = 0,            // 开关关闭（order_requested==0）：m ≡ 1，未做任何拟合
  kOk = 1,                  // 拟合成功且发布阶可辨识（order >= 1）
  kDegradedCoverage = 2,    // 星位置覆盖不足 ⇒ 只做标量（m ≡ 1）
  kDegradedStars = 3,       // 星数低于任何阶的 N_min ⇒ 只做标量（m ≡ 1）
  kDegradedRank = 4,        // 可辨识性判红（预检或后检）⇒ 降阶；降到 0 即 m ≡ 1
  kDegradedZeroScatter = 5, // 逐星残差散度为 0（无信息）⇒ m ≡ 1
  kDegradedSolve = 6,       // 数值求解失败（H 非正定/非有限）⇒ 降阶/失败
};

const char* spatial_gain_status_name(SpatialGainStatus s);

// 单颗星的空间拟合样本：像素位置 + r = log10(F_instr/F_syn)（dex）
struct SpatialGainSample {
  double x = 0.0;
  double y = 0.0;
  double r = 0.0;
};

// 拟合参数。默认阈值 = 实验/photometric-magnitude/docs/p1-spatial-gain.md §2.5 登记值。
struct SpatialGainParams {
  int order_requested = 0;      // 0=关闭（逐位退化）/1/2；>2 被钳到 2
  double location_dex = 0.0;    // 阶段 2 的全局稳健位置 L（k_photo = 10^(−L)）
  int width = 0;                // 帧宽（像素；坐标归一化基准）
  int height = 0;               // 帧高
  // §2.5 的降级门
  int min_stars_order1 = 50;             // N_min(1)
  int min_stars_order2 = 200;            // N_min(2)
  int coverage_block_grid = 3;           // 3×3 分块
  int coverage_min_stars_per_block = 5;  // 每块 >= 5 星才计为「有效块」
  int coverage_min_blocks = 6;           // 有效块数下限
  double coverage_min_bbox_frac = 0.5;   // 星位置包围盒**逐轴**最小展幅 / 帧幅
  bool coverage_check = true;            // 关闭仅用于单元测试的可控构造
};

// 拟合结果。poly 的语义与求值见 lib/algorithms/calibration/src/photometry_apply.h
// （基函数与 m(x,y) 求值的**唯一实现**在那里，本模块只做拟合）。
struct SpatialGainField {
  int order = 0;              // 发布阶：0 = m≡1
  int order_requested = 0;    // 请求阶
  SpatialGainStatus status = SpatialGainStatus::kDisabled;
  // 规范（机器可读，必须落盘）
  std::string gauge;
  // 基函数清单（与 poly.coef 同序、同长度）
  std::vector<std::string> basis_names;
  // 归一化：x̃ = (x − x_ref)/x_scale，ỹ = (y − y_ref)/y_scale
  double x_ref = 0.0, y_ref = 0.0, x_scale = 1.0, y_scale = 1.0;
  // log10 m(x,y) = −Σ_j coef[j]·B̃_j(x̃,ỹ)，B̃ = B − center（样本加权中心化）
  double coef[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
  // center[j] = 拟合样本（最终 Tukey 权重加权）上 B_j 的均值；**必须**与 coef 一起
  // 送进施加端（否则施加的是未中心化的场，会引入整体乘性常数偏移）。
  double center[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
  // ── 诊断/审计读数 ────────────────────────────────────────────────────────
  int n_stars = 0;            // 有效（有限）样本数
  int n_used = 0;             // 最终 Tukey 权重 > 0 的样本数
  int n_outliers = 0;         // 阶段 3 剔除数
  int tukey_iterations = 0;
  // 可辨识性（后检：**最终 Tukey 权重**下的 H）。order>0 时这些读数属于发布阶；
  // order==0（降级到 m≡1）时它们属于**被拒的那个阶**（即判红的原因读数）。
  int identifiable = 0;
  double rank_rtol_eff = 0.0;
  double kappa = 0.0;         // 秩亏 ⇒ +inf（不发布伪值）
  std::uint64_t rank_eff = 0;
  std::uint64_t n_unidentified = 0;
  // 覆盖读数
  double coverage_bbox_frac = 0.0;
  int coverage_blocks = 0;
  // 残差
  double sigma_global_dex = 0.0;   // 阶段 2 的逐星散度（输入回填，dex）
  double tukey_scale_dex = 0.0;    // 阶段 3 固定尺度 S（MAD(r−L)/0.6745）
  double sigma_spatial_dex = 0.0;  // 空间拟合后逐星残差 MAD/0.6745
  double field_ptp_dex = 0.0;      // 拟合场 log10 m 在视场上的峰峰值
  double field_rms_dex = 0.0;      // 拟合场 log10 m 在视场上的 RMS（含规范均值）
  // 解析噪声底（加权最小二乘系数协方差投影到视场）:
  //   noise_floor_dex     = S·sqrt(mean_pix(B̃ᵀ H⁻¹ B̃))  ← 与 field_rms 同量纲可比
  //   noise_floor_max_dex = S·sqrt(max_pix(B̃ᵀ H⁻¹ B̃))   ← 与"最大偏差"同量纲可比
  double noise_floor_dex = 0.0;
  double noise_floor_max_dex = 0.0;
  // 逐系数解析标准差 S·sqrt((H⁻¹)_jj)（诊断: 该系数是否真被数据约束）
  double coef_sigma_dex[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
  // ── 逐星拟合样本（审计证据）─────────────────────────────────────────────
  // 仅 order_requested > 0 时填充（关闭开关时不调用拟合 ⇒ 与改动前逐位一致）。
  // 用途: 独立复算「加 / 不加 m(x,y) 时的逐星残差与其空间结构」，以及核查
  // m(x,y) 吸收的是否为真实乘性响应（而不是天光/背景梯度）。落盘由调用方决定。
  std::vector<double> sample_x, sample_y, sample_r;
  std::vector<int> sample_used;   // 1 = 最终 Tukey 权重 > 0（进入拟合的星）
  // 降级/失败（具名，机器可读）
  std::string degraded_reason;
  // true ⇒ 调用方**必须把该帧判 fail**（几何充分但数值求解失败 = 缺陷, fail-closed；
  // 见 docs/design/LOG_AND_ERROR_SYSTEM.md §10 帧级失败作用域）。分布/几何不足的
  // 降级**不**置此位（那是声明过的降级，不是缺陷）。
  bool frame_fail = false;
};

// 拟合入口。n = 样本数（s 可为 nullptr 当 n<=0）。
// 确定性：固定样本序、固定基函数序、无并行归约、无随机数。
SpatialGainField fit_spatial_gain(const SpatialGainSample* s, int n,
                                  const SpatialGainParams& p);

// 对称正定线性方程组 H·c = g 的 Cholesky 解（H 为 n×n row-major，对称）。
// 返回 false ⇔ H 非正定/非有限（调用方按 §2.5 的 fail-closed 处理）。
// 单独暴露以便对「奇异 H」这一分支做可执行单元测试。
bool spatial_gain_solve_spd(const double* H, const double* g, int n, double* c);

// 帧上均匀采样的确定性网格（噪声底与峰峰值诊断用；stride 保证采样数 <= max_samples）
struct SpatialGainGrid {
  double dx = 1.0, dy = 1.0;
  int nx = 0, ny = 0;
};
SpatialGainGrid spatial_gain_frame_grid(int width, int height, int max_samples);

}  // namespace photometry
}  // namespace astrocs

#endif  // ASTROCS_PHOTOMETRY_SPATIAL_GAIN_H
