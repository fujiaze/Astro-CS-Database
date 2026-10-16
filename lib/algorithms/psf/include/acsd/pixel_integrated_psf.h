// ACSD PSF — 单位总通量像元积分响应（PSF.md §4 裁决的实现载体）
//
// 正本: docs/science/psf/PSF.md §4「连续单位总通量、像元求积与测光估计量的强制一致性」
//       docs/engineering/contracts/NORMALIZE_DATA.md §7.1（psf kind 的必需载荷）
//       docs/detail/normalize/contracts.md §7.4B
// 边界: **数学唯一实现在 PSF 库**; scheduler/normalize_precision 只消费本接口,
//       不得再自行维护一份 raw 中心值 Moffat 模板。
//
// 定义（圆对称 Moffat4, PSF.md §2.1/§3.1 的 σ 口径）:
//   g_σ(x,y) = (1 + (x²+y²)/(2σ²))^{-4},   ∫_{R²} g_σ = 2πσ²/3
// 推广到带旋转的 (sx, sy, θ): 连续未归一积分 = 2π·sx·sy/3（二次型行列式不变）。
// 单位总源通量的像元响应:
//   P_j(x0,y0) = (3/(2π sx sy)) ∫_{E_j} g(x-x0, y-y0) dx dy
// 其中 E_j 是第 j 个**实际源像元**足迹 [px-0.5, px+0.5]×[py-0.5, py+0.5],
// (x0,y0) 是亚像元参考中心相位。已入窗通量份额 f_enc = Σ_j P_j（**不重归一**）,
// 窗外份额 f_out = 1 - f_enc, 由实际像元集合求和得到, 不用理想圆半径闭式代替;
// 超过 1 只可能来自数值误差, 必须如实报告而不是 clamp 到 1。
#pragma once

#include <cstddef>
#include <vector>

namespace acsd {
namespace psf {

// 求积方法与误差估计（NORMALIZE_DATA §7.1 要求随模型落盘）。
enum class PixelIntegrationMethod { kGaussLegendreTensor = 0 };

struct PixelIntegrationDiagnostics {
  PixelIntegrationMethod method = PixelIntegrationMethod::kGaussLegendreTensor;
  int order = 0;               // 每维 Gauss-Legendre 节点数（阶 = order²/像元）
  double estimated_error = 0.0;  // 与阶 order+1 的差的最大绝对值（相对 f_enc）
  double f_enclosed = 0.0;     // Σ_j P_j
  double f_out = 0.0;          // 1 - f_enclosed
  int n_pixels = 0;            // 参与求积的像元数
};

struct PixelIntegrationRequest {
  double sx = 0.0;             // 主轴尺度（σ 口径; 各向同性时 sx=sy=σ）
  double sy = 0.0;             // 副轴尺度
  double theta = 0.0;          // 主轴位置角 (rad)
  double cx = 0.0;             // 亚像元参考中心 (0-based detector 坐标)
  double cy = 0.0;
  // 像元中心 (0-based); E_j = [px-0.5, px+0.5] × [py-0.5, py+0.5]。
  const double* pixel_x = nullptr;
  const double* pixel_y = nullptr;
  std::size_t n_pixels = 0;
  int order = 10;              // 每维 Gauss-Legendre 节点数（默认 10 → 100 点/像元）
};

// 计算单位总通量像元响应 P_j（与输入像元逐一对齐）。
//   * 返回 false ⇒ 参数非法（非正 sx/sy/order、空像元集、非有限输入）;
//     **不**回退到中心值，也不产生未归一模板。
//   * 内部同时用 order 与 order+1 各算一遍以给出数值误差估计, 并据此落盘
//     pixel_integration_method/order/estimated_error。
bool pixel_integrated_moffat4_profile(const PixelIntegrationRequest& req,
                                      std::vector<double>* out_p,
                                      PixelIntegrationDiagnostics* diag);

// 连续轮廓的精确总积分常量 3/(2π·sx·sy)（= 单位总通量归一因子）。
// 各向同性 (sx==sy==σ) 时即 3/(2πσ²)。仅用于诊断/校验, 不在求积路径上。
double moffat4_total_flux_normalization(double sx, double sy);

// 圆对称 Moffat4 的**解析**窗外份额参考值:
//   f_out(a) = (1 + a²/(2σ²))^{-3}   （半径 a 的圆盘内份额 = 1 - f_out）
// 这是"理想圆半径"参考求积, 不等于任意离散且非圆像元联盟的真实 f_out
// （PSF.md §4 明示）; 仅作独立校验/诊断用。
double moffat4_closed_form_f_out_disc(double sigma, double radius);

}  // namespace psf
}  // namespace acsd
