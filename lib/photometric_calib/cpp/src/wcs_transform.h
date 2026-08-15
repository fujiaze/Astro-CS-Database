#ifndef PC_WCS_TRANSFORM_H
#define PC_WCS_TRANSFORM_H

#include <cmath>

namespace pc {

// WCS坐标转换器: TAN投影 + SIP多项式畸变修正
// 坐标约定:
// CRPIX: 1-based (FITS约定)
// 像素坐标: 0-based
// 天球坐标: 度 (degrees)
class WcsTransform {
public:
    // 构造WCS转换器
    // sip_a/b/ap/bp: SIP系数数组(长度36, 按i*6+j索引), 可为nullptr
    WcsTransform(double crval1, double crval2,
                 double crpix1, double crpix2,
                 double cd11, double cd12,
                 double cd21, double cd22,
                 int sip_order,
                 const double* sip_a, const double* sip_b,
                 const double* sip_ap, const double* sip_bp);

    // 像素坐标(0-based) -> 天球坐标(度)
    void pixelToSky(double x, double y, double& ra, double& dec) const;

    // 天球坐标(度) -> 像素坐标(0-based)
    void skyToPixel(double ra, double dec, double& x, double& y) const;

    bool hasSip() const { return m_has_sip; }

private:
    double m_crval1, m_crval2;       // 参考天球坐标 (度)
    double m_crpix1, m_crpix2;       // 参考像素 (1-based)
    double m_cd[4];                  // CD矩阵 [cd11, cd12, cd21, cd22]
    double m_cdInv[4];               // CD逆矩阵
    double m_cdDet;                  // CD行列式
    int m_sip_order;                 // SIP阶数
    double m_sip_a_buf[36];          // SIP A系数缓冲
    double m_sip_b_buf[36];          // SIP B系数缓冲
    double m_sip_ap_buf[36];         // SIP AP系数缓冲
    double m_sip_bp_buf[36];         // SIP BP系数缓冲
    const double* m_sip_a;           // 前向SIP A
    const double* m_sip_b;           // 前向SIP B
    const double* m_sip_ap;          // 逆向SIP AP (可为nullptr)
    const double* m_sip_bp;          // 逆向SIP BP (可为nullptr)
    bool m_has_sip;

    // SIP多项式求值
    static double evalSip(const double* coeffs, double dx, double dy, int order);

    // TAN反投影: 中间坐标 -> 天球坐标
    void tanIntermediateToWorld(double xi, double eta, double& ra, double& dec) const;

    // TAN正投影: 天球坐标 -> 中间坐标
    void tanWorldToIntermediate(double ra, double dec, double& xi, double& eta) const;
};

} // namespace pc

#endif // PC_WCS_TRANSFORM_H
