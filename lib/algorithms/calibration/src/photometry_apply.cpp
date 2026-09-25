// ============================================================================
// photometry_apply.cpp - Gaia 测光比例应用模块实现
//
// 规范依据: 02_FROZEN_STAGE1_HISS_SPEC §7 / spec.md 步骤9
// Gaia 光谱积分校准是 Stage1 正式步骤。测光比例在 Drizzle 前应用:
// I_photo = k_photo * I_cal
// HISS signal 保存已应用 Gaia 光谱积分校准的统一相对测光累计通量。
//
// 公共契约: 00_COMMON_CONTRACTS.md §1.1
// 模块: lib/algorithms/calibration/src/photometry_apply.h/.cpp
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

#ifdef _OPENMP
#include <omp.h>
#endif

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
    // photscal 必须 > 0 且有限: k=0 产生全零输出 (静默数据毁损), 负值翻转极性无物理意义
    if (photscal <= 0.0) {
        fprintf(stderr, "[photometry_apply] 失败: photscal=%.6f 非正 (要求 > 0)\n", photscal);
        return -5;
    }

    const size_t n = (size_t)w * (size_t)h;

    fprintf(stderr, "[photometry_apply] 开始: w=%d h=%d photscal=%.6f, 总像素=%zu\n",
            w, h, photscal, n);

    // ---- 应用测光比例: I_photo = k_photo * I_cal ----
    // 使用 double 精度计算, 避免 photscal 极大/极小时 float 乘法精度损失
    // NaN/Inf 透传: NaN * k = NaN, Inf * k = Inf (k>0)
    // 下游 Drizzle 会用 std::isfinite 跳过非有限像素, 行为正确
    for (size_t i = 0; i < n; i++) {
        out[i] = (float)((double)light[i] * photscal);
    }

    fprintf(stderr, "[photometry_apply] 完成: 已处理 %zu 像素, photscal=%.6f\n",
            n, photscal);
    return 0;
}

// ============================================================================
// PHOT-MXY-01: 低阶乘性空间增益施加  I_photo = k_photo·m(x,y)·I_cal
//
// 规范依据: docs/science/PHOTOMETRY.md §16.1 ⑤（施加到**整帧像素**，不只星点）。
// 场的定义/基函数/规范自由度: 见本文件头（photometry_apply.h）与本函数上方注释。
// 与冻结的 apply_photometry 的关系:
//   · order == 0（m ≡ 1）⇒ **直接委托** apply_photometry，逐位一致（可回归对照）；
//   · order >= 1 ⇒ 逐像素乘因子 (k_photo · m(x,y))，double 中间精度。
// 确定性: 逐像素独立（无跨像素归约）⇒ 与 OpenMP 线程数无关（逐位一致）。
// ============================================================================
int apply_photometry_spatial(const float* light, int w, int h, double photoscale,
                             const PhotoSpatialGain& field, float* out)
{
    // ---- m ≡ 1：严格退化为冻结路径（逐位一致，见 ctest p1phot_spatial S4）----
    const int nterm = photo_spatial_nterm(field.order);
    if (nterm == 0) {
        return apply_photometry(light, w, h, photoscale, out);
    }

    // ---- 参数校验（与 apply_photometry 同判据，逐条同码）----
    if (light == nullptr) {
        fprintf(stderr, "[photometry_apply_spatial] 失败: 输入 light 为 nullptr\n");
        return -1;
    }
    if (out == nullptr) {
        fprintf(stderr, "[photometry_apply_spatial] 失败: 输出 out 为 nullptr\n");
        return -2;
    }
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "[photometry_apply_spatial] 失败: 图像尺寸非法 (w=%d, h=%d)\n", w, h);
        return -3;
    }
    if (!std::isfinite(photoscale)) {
        fprintf(stderr, "[photometry_apply_spatial] 失败: photscal 非有限值 (%g)\n", photoscale);
        return -4;
    }
    if (photoscale <= 0.0) {
        fprintf(stderr, "[photometry_apply_spatial] 失败: photscal=%.6f 非正 (要求 > 0)\n",
                photoscale);
        return -5;
    }
    // -6: 场的定标/系数非法（非有限；归一化尺度非正）⇒ fail-closed，不产出伪像素
    if (!(field.x_scale > 0.0) || !(field.y_scale > 0.0) ||
        !std::isfinite(field.x_ref) || !std::isfinite(field.y_ref) ||
        !std::isfinite(field.x_scale) || !std::isfinite(field.y_scale)) {
        fprintf(stderr, "[photometry_apply_spatial] 失败: 空间场归一化参数非法\n");
        return -6;
    }
    for (int j = 0; j < nterm; ++j) {
        if (!std::isfinite(field.coef[j])) {
            fprintf(stderr, "[photometry_apply_spatial] 失败: 系数[%d] 非有限\n", j);
            return -6;
        }
    }

    fprintf(stderr, "[photometry_apply_spatial] 开始: w=%d h=%d photscal=%.6f order=%d\n",
            w, h, photoscale, field.order);

    const std::size_t npix = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    for (std::size_t i = 0; i < npix; ++i) {
        const int x = static_cast<int>(i % static_cast<std::size_t>(w));
        const int y = static_cast<int>(i / static_cast<std::size_t>(w));
        const double m = photo_spatial_m(field, static_cast<double>(x), static_cast<double>(y));
        out[i] = static_cast<float>(static_cast<double>(light[i]) * photoscale * m);
    }

    fprintf(stderr, "[photometry_apply_spatial] 完成: 已处理 %zu 像素 (order=%d)\n",
            npix, field.order);
    return 0;
}

} // namespace calibration
