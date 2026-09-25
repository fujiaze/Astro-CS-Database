#ifndef CALIBRATION_PHOTOMETRY_APPLY_H
#define CALIBRATION_PHOTOMETRY_APPLY_H

#include <cmath>   // std::pow（空间增益 m(x,y) 的内联求值）

// ============================================================================
// photometry_apply.h - Gaia 测光比例应用模块
//
// 规范依据: 02_FROZEN_STAGE1_HISS_SPEC §7 / spec.md 步骤9
// Gaia 光谱积分校准是 Stage1 正式步骤。测光比例在 Drizzle 前应用:
// I_photo = k_photo * I_cal
// HISS signal 保存已应用 Gaia 光谱积分校准的统一相对测光累计通量。
//
// 公共契约: 00_COMMON_CONTRACTS.md §1.1
// 模块: lib/algorithms/calibration/src/photometry_apply.h/.cpp
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
// -5: photscal <= 0 (k=0 会静默产生全零, 负值翻转极性)
//
// 说明:
// - NaN/Inf 像素透传 (后续 Drizzle 会跳过非有限像素)
// - 输入与输出可为同一缓冲区 (in-place 操作)
// - 内部使用 double 精度计算, 避免大动态范围下的精度损失
int apply_photometry(const float* light, int w, int h, double photscal, float* out);

// ============================================================================
// PHOT-MXY-01: 低阶乘性空间增益 m(x,y)（SCI-PHOT-001 §16.1 ④⑤）
// ----------------------------------------------------------------------------
// 规范: I_photo = k_photo·m(x,y)·I_cal，施加到**整帧像素**（不只星点）。
// 形式与规范自由度: 见 lib/algorithms/photometry/cpp/src/spatial_gain.h 文件头
//   （基函数 = 二维多项式, 阶 ∈ {1,2}; 规范 = 参与拟合的星集合上 log10 m 的
//    加权均值为 0 ⇔ 该集合上 m 的加权几何均值为 1）。
//
// 本头是 m(x,y) **求值**的唯一实现（基函数定义 + 归一化 + 10^(−Σc·B̃)），
// 拟合侧（spatial_gain.cpp）与像素施加侧（photometry_apply.cpp）共用同一份内联
// 实现，避免公式副本。
//
// order=0（m ≡ 1）时 apply_photometry_spatial 与 apply_photometry **逐位一致**
// （乘法因子恒为 1.0; 由 ctest p1phot_spatial 的 S4 负例锁定）。
// ============================================================================

// 场定义（纯数据契约）。order=0 ⇒ m ≡ 1（关闭或降级到全局常数）。
// center[j] = 拟合样本（Tukey 权重加权）上基函数 B_j 的均值 —— **必须**一起携带:
// 拟合解出的是**中心化**基函数 B̃_j = B_j − center_j 的系数, 施加端若用未中心化的
// B_j 会引入一个整体乘性常数偏移（使 k_photo 的全局语义被暗改）。
struct PhotoSpatialGain {
    int order = 0;
    double coef[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double center[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double x_ref = 0.0, y_ref = 0.0;
    double x_scale = 1.0, y_scale = 1.0;
};

// 基函数（阶 1: {x̃, ỹ}; 阶 2: {x̃, ỹ, x̃², x̃ỹ, ỹ²}）。阶 >=3 不启用（见 spatial_gain.h）。
inline int photo_spatial_nterm(int order) {
    return (order <= 0) ? 0 : (order == 1 ? 2 : 5);
}

// 归一化坐标: x̃ = (x − x_ref)/x_scale（拟合侧取帧几何中心与半宽 ⇒ x̃ ∈ [−1,1]）
inline void photo_spatial_basis(int order, double xt, double yt, double* B) {
    B[0] = xt;
    B[1] = yt;
    if (order >= 2) {
        B[2] = xt * xt;
        B[3] = xt * yt;
        B[4] = yt * yt;
    }
}

// log10 m(x,y) = −Σ_j coef[j]·(B_j(x̃,ỹ) − center[j])   ← 中心化在施加端同样生效
inline double photo_spatial_log10_m(const PhotoSpatialGain& f, double x, double y) {
    const int n = photo_spatial_nterm(f.order);
    if (n == 0) return 0.0;   // m ≡ 1（严格：不进入多项式路径）
    const double xt = (x - f.x_ref) / f.x_scale;
    const double yt = (y - f.y_ref) / f.y_scale;
    double B[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    photo_spatial_basis(f.order, xt, yt, B);
    double s = 0.0;
    for (int j = 0; j < n; ++j) s += f.coef[j] * (B[j] - f.center[j]);
    return -s;
}

// m(x,y) = 10^(−Σ c_j B_j)（order=0 ⇒ 恒等 1.0）
inline double photo_spatial_m(const PhotoSpatialGain& f, double x, double y) {
    if (photo_spatial_nterm(f.order) == 0) return 1.0;
    return std::pow(10.0, photo_spatial_log10_m(f, x, y));
}

// 施加空间增益: out[i] = in[i] · k_photo · m(x, y)，(x,y) 为 0-based 像素坐标
// （与 PSF 星坐标同一约定；施加面 = 整帧像素, SCI-PHOT-001 §16.1 ⑤）。
// 返回: 0=成功; -1/-2/-3/-4/-5 同 apply_photometry; -6: 场的定标量非有限或 <=0。
// 说明: NaN/Inf 像素透传; 支持 in-place; double 中间精度; order=0 时与
//       apply_photometry 逐位一致; 逐像素独立 ⇒ 与 OpenMP/线程数无关。
int apply_photometry_spatial(const float* light, int w, int h, double photoscale,
                             const PhotoSpatialGain& field, float* out);

} // namespace calibration

#endif // CALIBRATION_PHOTOMETRY_APPLY_H
