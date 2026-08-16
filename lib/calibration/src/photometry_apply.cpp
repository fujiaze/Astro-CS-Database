// ============================================================================
// photometry_apply.cpp - Gaia 测光比例应用模块实现
//
// 规范依据: 02_FROZEN_STAGE1_HISS_SPEC §7 / spec.md 步骤9
// Gaia 光谱积分校准是 Stage1 正式步骤。测光比例在 Drizzle 前应用:
// I_photo = k_photo * I_cal
// HISS signal 保存已应用 Gaia 光谱积分校准的统一相对测光累计通量。
//
// 公共契约: 00_COMMON_CONTRACTS.md §1.1
// 模块: lib/calibration/src/photometry_apply.h/.cpp
// 职责: Gaia 测光比例应用
//
// 实现要点:
// - 内部使用 double 精度乘法, 避免大动态范围 (例如 k=1e-7) 下的 float 精度损失
// - NaN/Inf 输入像素透传 (NaN * 任何数 = NaN, 行为可预期, 下游 Drizzle 会跳过)
// - 支持 in-place 操作 (light == out 时直接逐元素覆盖, 无依赖)
// ============================================================================

#include "photometry_apply.h"

#include <cstdio>      // fprintf
#include <cmath>       // std::isfinite
#include <cstddef>     // size_t

namespace calibration {

// 应用 Gaia 测光比例到已校准图像
// I_photo = k_photo * I_cal (02_FROZEN §7)
int apply_photometry(const float* light, int w, int h, double photscal, float* out)
{
    // ---- 参数校验 ----
    if (light == nullptr) {
        fprintf(stderr, "[photometry_apply] 失败: 输入 light 为 nullptr\n");
        return -1;
    }
    if (out == nullptr) {
        fprintf(stderr, "[photometry_apply] 失败: 输出 out 为 nullptr\n");
        return -2;
    }
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "[photometry_apply] 失败: 图像尺寸非法 (w=%d, h=%d)\n", w, h);
        return -3;
    }
    if (!std::isfinite(photscal)) {
        fprintf(stderr, "[photometry_apply] 失败: photscal 非有限值 (%g)\n", photscal);
        return -4;
    }

    const size_t n = (size_t)w * (size_t)h;

    fprintf(stderr, "[photometry_apply] 开始: w=%d h=%d photscal=%.6f, 总像素=%zu\n",
            w, h, photscal, n);

    // ---- 应用测光比例: I_photo = k_photo * I_cal ----
    // 使用 double 精度计算, 避免 photscal 极大/极小时 float 乘法精度损失
    // NaN/Inf 透传: NaN * k = NaN, Inf * k = Inf (k>0) / -Inf (k<0) / NaN (k=0)
    // 下游 Drizzle 会用 std::isfinite 跳过非有限像素, 行为正确
    for (size_t i = 0; i < n; i++) {
        out[i] = (float)((double)light[i] * photscal);
    }

    fprintf(stderr, "[photometry_apply] 完成: 已处理 %zu 像素, photscal=%.6f\n",
            n, photscal);
    return 0;
}

} // namespace calibration
