#ifndef CALIBRATION_PHOTOMETRY_APPLY_H
#define CALIBRATION_PHOTOMETRY_APPLY_H

// ============================================================================
// photometry_apply.h - Gaia 测光比例应用模块
//
// 规范依据: 02_FROZEN_STAGE1_HISS_SPEC §7 / spec.md 步骤9
// Gaia 光谱积分校准是 Stage1 正式步骤。测光比例在 Drizzle 前应用:
// I_photo = k_photo * I_cal
// HISS signal 保存已应用 Gaia 光谱积分校准的统一相对测光累计通量。
//
// 公共契约: 00_COMMON_CONTRACTS.md §1.1
// 模块: lib/calibration/src/photometry_apply.h/.cpp
// 职责: Gaia 测光比例应用
// ============================================================================

namespace calibration {

// 应用 Gaia 测光比例到已校准图像
// 公式: I_photo = k_photo * I_cal (02_FROZEN §7)
//
// light: 输入已校准图像 (float32, W*H, 行主序)
// w, h: 图像宽高
// photscal: 测光比例 k_photo (必须为有限值)
// out: 输出图像 (float32, W*H, 调用方分配)
//
// 返回: 0=成功, <0=失败
// -1: light == nullptr
// -2: out == nullptr
// -3: w/h 非法 (<=0)
// -4: photscal 非有限值 (NaN/Inf)
//
// 说明:
// - NaN/Inf 像素透传 (后续 Drizzle 会跳过非有限像素)
// - 输入与输出可为同一缓冲区 (in-place 操作)
// - 内部使用 double 精度计算, 避免大动态范围下的精度损失
int apply_photometry(const float* light, int w, int h, double photscal, float* out);

} // namespace calibration

#endif // CALIBRATION_PHOTOMETRY_APPLY_H
