// wcs_transform.cpp - TAN+SIP投影坐标转换
// 功能: 实现天球坐标(RA/Dec)与像素坐标(x/y)之间的转换
// 算法: TAN gnomonic投影 + SIP多项式畸变修正
// 参考: lib/healpix_db/healpix_drizzle/wcs_sip.cpp, astropy.wcs.WCS

#include "wcs_transform.h"

#include <cstdio>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pc {

static constexpr double D2R = M_PI / 180.0;  // 度 -> 弧度
static constexpr double R2D = 180.0 / M_PI;  // 弧度 -> 度

// ============================================================================
// 构造函数: 复制WCS参数, 计算CD矩阵的逆和行列式
// ============================================================================
WcsTransform::WcsTransform(double crval1, double crval2,
                           double crpix1, double crpix2,
                           double cd11, double cd12,
                           double cd21, double cd22,
                           int sip_order,
                           const double* sip_a, const double* sip_b,
                           const double* sip_ap, const double* sip_bp)
    : m_crval1(crval1), m_crval2(crval2),
      m_crpix1(crpix1), m_crpix2(crpix2),
      m_cd{cd11, cd12, cd21, cd22},
      m_sip_order(sip_order),
      m_sip_a(nullptr), m_sip_b(nullptr),
      m_sip_ap(nullptr), m_sip_bp(nullptr),
      m_has_sip(sip_order > 0) {

    // CD矩阵行列式与逆
    m_cdDet = m_cd[0] * m_cd[3] - m_cd[1] * m_cd[2];
    if (std::fabs(m_cdDet) < 1e-15) {
        std::fprintf(stderr, "[wcs_transform] 警告: CD矩阵行列式接近0 (det=%.3e)\n", m_cdDet);
        m_cdInv[0] = 0.0; m_cdInv[1] = 0.0;
        m_cdInv[2] = 0.0; m_cdInv[3] = 0.0;
    } else {
        // 2x2逆: [a b; c d]^-1 = 1/det * [d -b; -c a]
        m_cdInv[0] =  m_cd[3] / m_cdDet;
        m_cdInv[1] = -m_cd[1] / m_cdDet;
        m_cdInv[2] = -m_cd[2] / m_cdDet;
        m_cdInv[3] =  m_cd[0] / m_cdDet;
    }

    // 复制SIP系数 (长度36, 按i*6+j索引)
    for (int i = 0; i < 36; ++i) {
        m_sip_a_buf[i] = (sip_a != nullptr) ? sip_a[i] : 0.0;
        m_sip_b_buf[i] = (sip_b != nullptr) ? sip_b[i] : 0.0;
        m_sip_ap_buf[i] = (sip_ap != nullptr) ? sip_ap[i] : 0.0;
        m_sip_bp_buf[i] = (sip_bp != nullptr) ? sip_bp[i] : 0.0;
    }
    if (m_has_sip) {
        m_sip_a = m_sip_a_buf;
        m_sip_b = m_sip_b_buf;
        // AP/BP可选, 无AP/BP时sky_to_pixel用迭代法
        m_sip_ap = (sip_ap != nullptr) ? m_sip_ap_buf : nullptr;
        m_sip_bp = (sip_bp != nullptr) ? m_sip_bp_buf : nullptr;
    }

    std::fprintf(stderr, "[wcs_transform] 初始化: CRVAL=(%.6f,%.6f), CRPIX=(%.3f,%.3f), "
                "CD=[%.3e,%.3e;%.3e,%.3e], det=%.3e, sip_order=%d\n",
                m_crval1, m_crval2, m_crpix1, m_crpix2,
                m_cd[0], m_cd[1], m_cd[2], m_cd[3], m_cdDet, m_sip_order);
}

// ============================================================================
// SIP多项式求值
// coeffs按coeffs[i*6+j]存储, 对应dx^i*dy^j, 下三角i+j<=order
// ============================================================================
double WcsTransform::evalSip(const double* coeffs, double dx, double dy, int order) {
    if (order <= 0 || coeffs == nullptr) return 0.0;
    double result = 0.0;
    for (int i = 0; i <= order; ++i) {
        for (int j = 0; j <= order - i; ++j) {
            result += coeffs[i * 6 + j] * std::pow(dx, i) * std::pow(dy, j);
        }
    }
    return result;
}

// ============================================================================
// TAN反投影: 中间坐标(xi,eta) -> 天球坐标(RA,Dec)
// 标准gnomonic反投影公式 (Calabretta & Greisen 2002)
// ============================================================================
void WcsTransform::tanIntermediateToWorld(double xi, double eta,
                                           double& ra, double& dec) const {
    const double xi_rad  = xi  * D2R;
    const double eta_rad = eta * D2R;
    const double rho = std::sqrt(xi_rad * xi_rad + eta_rad * eta_rad);

    // 切点本身, 直接返回投影中心
    if (rho < 1e-12) {
        ra  = m_crval1;
        dec = m_crval2;
        return;
    }

    const double dec0_rad = m_crval2 * D2R;
    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);
    const double c    = std::atan(rho);
    const double sinc = std::sin(c);
    const double cosc = std::cos(c);

    // dec = asin(cos(c)*sin(dec0) + eta*sin(c)*cos(dec0)/rho)
    double sin_dec = cosc * sdec0 + eta_rad * sinc * cdec0 / rho;
    if (sin_dec >  1.0) sin_dec =  1.0;
    if (sin_dec < -1.0) sin_dec = -1.0;
    const double dec_rad = std::asin(sin_dec);

    // ra = ra0 + atan2(xi*sin(c), rho*cos(dec0)*cos(c) - eta*sin(dec0)*sin(c))
    const double dra = std::atan2(xi_rad * sinc,
                                  rho * cdec0 * cosc - eta_rad * sdec0 * sinc);
    double ra_rad = m_crval1 * D2R + dra;

    // 归一化RA到[0, 2π)
    while (ra_rad < 0.0)         ra_rad += 2.0 * M_PI;
    while (ra_rad >= 2.0 * M_PI) ra_rad -= 2.0 * M_PI;

    ra  = ra_rad * R2D;
    dec = dec_rad * R2D;
}

// ============================================================================
// TAN正投影: 天球坐标(RA,Dec) -> 中间坐标(xi,eta)
// 标准gnomonic正投影公式
// ============================================================================
void WcsTransform::tanWorldToIntermediate(double ra, double dec,
                                           double& xi, double& eta) const {
    const double ra_rad   = ra  * D2R;
    const double dec_rad  = dec * D2R;
    const double ra0_rad  = m_crval1 * D2R;
    const double dec0_rad = m_crval2 * D2R;

    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);
    const double sdec  = std::sin(dec_rad);
    const double cdec  = std::cos(dec_rad);
    const double dra   = ra_rad - ra0_rad;
    const double cdra  = std::cos(dra);
    const double sdra  = std::sin(dra);

    // cos(c) = 点到切点的角距离余弦
    const double cosc = sdec0 * sdec + cdec0 * cdec * cdra;
    if (std::fabs(cosc) < 1e-12) {
        // 投影发散 (点在投影背面)
        xi  = 1e6;
        eta = 1e6;
        return;
    }

    double xi_rad  = cdec * sdra / cosc;
    double eta_rad = (cdec0 * sdec - sdec0 * cdec * cdra) / cosc;
    xi  = xi_rad * R2D;
    eta = eta_rad * R2D;
}

// ============================================================================
// 像素坐标 -> 天球坐标 (前向转换)
// 步骤:
//   1. 归一化: dx = x - (CRPIX1-1), dy = y - (CRPIX2-1) (CRPIX是1-based)
//   2. 前向SIP(A/B): dx' = dx + A(dx,dy), dy' = dy + B(dx,dy)
//   3. CD矩阵: xi = cd[0]*dx' + cd[1]*dy', eta = cd[2]*dx' + cd[3]*dy'
//   4. TAN反投影: (xi,eta) -> (RA,Dec)
// ============================================================================
void WcsTransform::pixelToSky(double x, double y, double& ra, double& dec) const {
    double dx = x - (m_crpix1 - 1.0);
    double dy = y - (m_crpix2 - 1.0);

    if (m_has_sip) {
        double f = evalSip(m_sip_a, dx, dy, m_sip_order);
        double g = evalSip(m_sip_b, dx, dy, m_sip_order);
        dx += f;
        dy += g;
    }

    double xi  = m_cd[0] * dx + m_cd[1] * dy;
    double eta = m_cd[2] * dx + m_cd[3] * dy;

    tanIntermediateToWorld(xi, eta, ra, dec);
}

// ============================================================================
// 天球坐标 -> 像素坐标 (逆向转换)
// 步骤:
//   1. TAN正投影: (RA,Dec) -> (xi,eta)
//   2. CD逆矩阵: dx = cdInv[0]*xi + cdInv[1]*eta, dy = cdInv[2]*xi + cdInv[3]*eta
//   3. 逆向SIP(AP/BP): 如果有AP/BP, dx' = dx + AP(dx,dy); 无则迭代一次
//   4. 转回0-based: x = dx' + (CRPIX1-1), y = dy' + (CRPIX2-1)
// ============================================================================
void WcsTransform::skyToPixel(double ra, double dec, double& x, double& y) const {
    double xi, eta;
    tanWorldToIntermediate(ra, dec, xi, eta);

    double dx = m_cdInv[0] * xi + m_cdInv[1] * eta;
    double dy = m_cdInv[2] * xi + m_cdInv[3] * eta;

    if (m_has_sip) {
        if (m_sip_ap != nullptr && m_sip_bp != nullptr) {
            // 有逆向SIP多项式, 直接应用
            double fp = evalSip(m_sip_ap, dx, dy, m_sip_order);
            double gp = evalSip(m_sip_bp, dx, dy, m_sip_order);
            dx += fp;
            dy += gp;
        } else {
            // 无逆向SIP, 用一次迭代近似:
            // 前向SIP: U = dx + A(U,V), V = dy + B(U,V)
            // 一次迭代: U ≈ dx + A(dx,dy), V ≈ dy + B(dx,dy)
            // 然后反解: dx_orig = U - A(U,V), dy_orig = V - B(U,V)
            // 这里我们需要从dx,dy(含SIP修正后的)反推原始像素坐标
            // 实际上CD逆矩阵给出的是含SIP修正的中间坐标对应的像素偏移
            // 对于无AP/BP的情况, 用牛顿迭代一次
            double u = dx, v = dy;
            for (int iter = 0; iter < 3; ++iter) {
                double f = evalSip(m_sip_a, u, v, m_sip_order);
                double g = evalSip(m_sip_b, u, v, m_sip_order);
                // U = u_orig + A(u_orig, v_orig), 求 u_orig
                // 近似: u_orig = U - A(u, v)
                double u_new = dx - f;
                double v_new = dy - g;
                if (std::fabs(u_new - u) < 1e-10 && std::fabs(v_new - v) < 1e-10) {
                    u = u_new;
                    v = v_new;
                    break;
                }
                u = u_new;
                v = v_new;
            }
            dx = u;
            dy = v;
        }
    }

    x = dx + (m_crpix1 - 1.0);
    y = dy + (m_crpix2 - 1.0);
}

} // namespace pc
