#ifndef WCS_SIP_H
#define WCS_SIP_H

#include "fits_reader.h"  // WcsParams, SipCoeffs
#include "poly_clip.h"    // SkyCoord
#include <string>

namespace drizzle {

// WCS+SIP 坐标转换器
// 实现 TAN 投影 + SIP 畸变校正, 不依赖 astropy
class WcsSip {
public:
    // 从 WcsParams 构造
    explicit WcsSip(const WcsParams& wcs);

    // 像素坐标 → 天球坐标 (RA/Dec, 度)
    // 前向转换: A/B 前向 SIP → CD → TAN 反投影
    // x, y: 像素坐标 (0-based, FITS 习惯是 1-based, CRPIX 也是 1-based)
    // 阶段7: 模板双实例 (T=float → FP32 IEEE binary32 几何; T=double → FP64)
    template <typename T>
    void pixelToSkyT(T x, T y, T& ra, T& dec) const;

    // 兼容包装 (double 实例, 旧调用方)
    void pixelToSky(double x, double y, double& ra, double& dec) const;

    // 天球坐标 → 像素坐标
    // 逆向转换: TAN 投影 → CD⁻¹ → AP/BP 逆向 SIP
    void skyToPixel(double ra, double dec, double& x, double& y) const;

    // 批量像素 → 天球 (用于性能优化)
    void pixelToSkyBatch(const double* xy, int count, double* radec) const;

    // 是否有有效 WCS
    bool hasWcs() const { return m_hasWcs; }

    // 获取 WCS 参数 (用于元数据输出)
    const WcsParams& params() const { return m_wcs; }

private:
    WcsParams m_wcs;
    bool m_hasWcs;

    // CD 矩阵逆 (2x2)
    double m_cdInv[4];
    double m_cdDet;

    // SIP 多项式求值
    // coeffs: A[36] 或 B[36] 或 AP[36] 或 BP[36]
    // dx, dy: 归一化像素坐标 (相对于 CRPIX)
    // order: SIP 阶数
    // 返回: 多项式值
    // 阶段7: SIP 多项式求值模板 (T=float/double)
    template <typename T>
    static T evalSipT(const double* coeffs, T dx, T dy, int order);

    // 兼容包装 (double 实例, skyToPixel 逆向 SIP 使用)
    static double evalSip(const double* coeffs, double dx, double dy, int order);

    // TAN 投影反投影 (中间坐标 → 天球坐标)
    // intermediate: [xi, eta] (度)
    // 返回: (RA, Dec) 度
    // 阶段7: TAN 反投影模板 (T=float/double)
    template <typename T>
    void tanIntermediateToWorldT(T xi, T eta, T& ra, T& dec) const;

    // TAN 投影正投影 (天球坐标 → 中间坐标)
    void tanWorldToIntermediate(double ra, double dec, double& xi, double& eta) const;
};

} // namespace drizzle

#endif // WCS_SIP_H
