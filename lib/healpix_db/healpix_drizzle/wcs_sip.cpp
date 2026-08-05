#include "wcs_sip.h"

#include <cstdio>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace drizzle {

// ============================================================================
// 常量
// ============================================================================
static constexpr double D2R = M_PI / 180.0;  // 度 → 弧度
static constexpr double R2D = 180.0 / M_PI;  // 弧度 → 度

// ============================================================================
// 日志宏: 输出到 stderr, 格式 [wcs_sip] 消息
// ============================================================================
#define WCS_LOG(fmt, ...) \
    fprintf(stderr, "[wcs_sip] " fmt "\n", ##__VA_ARGS__)

// ============================================================================
// 1. 构造函数
//
// 复制 WCS 参数, 计算 CD 矩阵的逆和行列式
// CD 矩阵: [cd1_1, cd1_2, cd2_1, cd2_2] (行优先)
//   det = cd[0]*cd[3] - cd[1]*cd[2]
//   cdInv = [cd[3]/det, -cd[1]/det, -cd[2]/det, cd[0]/det]
// ============================================================================
WcsSip::WcsSip(const WcsParams& wcs) : m_wcs(wcs), m_hasWcs(wcs.has_wcs) {
    // 初始化 CD 逆为零
    m_cdInv[0] = 0.0; m_cdInv[1] = 0.0;
    m_cdInv[2] = 0.0; m_cdInv[3] = 0.0;
    m_cdDet = 0.0;

    if (!m_hasWcs) {
        WCS_LOG("警告: WCS 无效 (has_wcs=false), 坐标转换将返回零值");
        return;
    }

    // 计算 CD 矩阵行列式
    const double* cd = m_wcs.cd;
    m_cdDet = cd[0] * cd[3] - cd[1] * cd[2];

    if (std::fabs(m_cdDet) < 1e-15) {
        WCS_LOG("警告: CD 矩阵行列式接近 0 (det=%.3e), 坐标转换将不可靠", m_cdDet);
        m_cdInv[0] = 0.0; m_cdInv[1] = 0.0;
        m_cdInv[2] = 0.0; m_cdInv[3] = 0.0;
    } else {
        // 2x2 矩阵逆: [a b; c d]⁻¹ = 1/det * [d -b; -c a]
        m_cdInv[0] =  cd[3] / m_cdDet;
        m_cdInv[1] = -cd[1] / m_cdDet;
        m_cdInv[2] = -cd[2] / m_cdDet;
        m_cdInv[3] =  cd[0] / m_cdDet;
    }

    WCS_LOG("WCS 初始化: CRVAL=(%.6f, %.6f)°, CRPIX=(%.3f, %.3f), "
            "CD=[%.3e, %.3e; %.3e, %.3e], det=%.3e",
            m_wcs.crval[0], m_wcs.crval[1],
            m_wcs.crpix[0], m_wcs.crpix[1],
            cd[0], cd[1], cd[2], cd[3], m_cdDet);
    WCS_LOG("SIP: A/B order=%d, AP/BP order=%d",
            m_wcs.sip.order, m_wcs.sip.ap_order);
}

// ============================================================================
// 2. SIP 多项式求值
//
// 系数按 coeffs[i*6+j] 存储, 对应 dx^i * dy^j
// 下三角: i+j <= order
// order=0 时返回 0 (无 SIP 修正)
//
// 公式: result = Σ_{i+j<=order} coeffs[i*6+j] * dx^i * dy^j
// ============================================================================
template <typename T>
T WcsSip::evalSipT(const double* coeffs, T dx, T dy, int order) {
    if (order <= 0) return T(0);

    T result = T(0);
    // 整数幂递推 (与旧 evalSip 的 std::pow(dx, i) 整数指数一致, 避免
    // pow(double,double) 的 exp/log 舍入差异 — 模板化不得改变 FP64 数值)
    T pdx = T(1);
    for (int i = 0; i <= order; i++) {
        T pdy = T(1);
        for (int j = 0; j <= order - i; j++) {
            result += T(coeffs[i * 6 + j]) * pdx * pdy;
            pdy *= dy;
        }
        pdx *= dx;
    }
    return result;
}

// 兼容包装 (double 实例, skyToPixel 逆向 SIP 使用)
double WcsSip::evalSip(const double* coeffs, double dx, double dy, int order) {
    return evalSipT<double>(coeffs, dx, dy, order);
}

// ============================================================================
// 3. TAN 反投影 (中间坐标 → 天球坐标)
//
// 使用标准 gnomonic 反投影公式 (Calabretta & Greisen 2002, TAN 投影)
// 与 poly_clip.cpp 的 gnomonicReverse 一致
//
// 输入: xi, eta (中间世界坐标, 度, 来自 CD 矩阵)
// 输出: ra, dec (天球坐标, 度)
//
// 公式:
//   xi_rad = xi * D2R,  eta_rad = eta * D2R
//   rho = sqrt(xi_rad² + eta_rad²)
//   c   = atan(rho)
//   dec = asin(cos(c)*sin(dec0) + eta_rad*sin(c)*cos(dec0)/rho)
//   ra  = ra0 + atan2(xi_rad*sin(c), rho*cos(dec0)*cos(c) - eta_rad*sin(dec0)*sin(c))
//
// 保护:
//   - rho ≈ 0 (切点本身) → 返回 (ra0, dec0)
//   - asin 参数钳位到 [-1, 1] 避免浮点误差
//   - RA 归一化到 [0, 360)
// ============================================================================
template <typename T>
void WcsSip::tanIntermediateToWorldT(T xi, T eta, T& ra, T& dec) const {
    const T ra0_deg  = T(m_wcs.crval[0]);
    const T dec0_deg = T(m_wcs.crval[1]);

    // 中间坐标 → 弧度
    const T xi_rad  = xi  * T(D2R);
    const T eta_rad = eta * T(D2R);

    // rho = 投影平面上的径向距离 (弧度)
    const T rho = std::sqrt(xi_rad * xi_rad + eta_rad * eta_rad);

    // 切点本身, 直接返回投影中心
    if (rho < T(1e-12)) {
        ra  = ra0_deg;
        dec = dec0_deg;
        return;
    }

    const T dec0_rad = dec0_deg * T(D2R);
    const T sdec0 = std::sin(dec0_rad);
    const T cdec0 = std::cos(dec0_rad);

    const T c    = std::atan(rho);
    const T sinc = std::sin(c);
    const T cosc = std::cos(c);

    // dec = asin(cos(c)*sin(dec0) + eta*sin(c)*cos(dec0)/rho)
    T sin_dec = cosc * sdec0 + eta_rad * sinc * cdec0 / rho;
    // 数值保护: asin 参数可能因浮点误差略微超出 [-1, 1]
    if (sin_dec >  T(1)) sin_dec =  T(1);
    if (sin_dec < -T(1)) sin_dec = -T(1);
    const T dec_rad = std::asin(sin_dec);

    // ra = ra0 + atan2(xi*sin(c), rho*cos(dec0)*cos(c) - eta*sin(dec0)*sin(c))
    const T dra = std::atan2(xi_rad * sinc,
                             rho * cdec0 * cosc - eta_rad * sdec0 * sinc);
    T ra_rad = ra0_deg * T(D2R) + dra;

    // 归一化 RA 到 [0, 2π)
    while (ra_rad < T(0))              ra_rad += T(2.0 * M_PI);
    while (ra_rad >= T(2.0 * M_PI))    ra_rad -= T(2.0 * M_PI);

    ra  = ra_rad * T(R2D);
    dec = dec_rad * T(R2D);
}

// ============================================================================
// 4. TAN 正投影 (天球坐标 → 中间坐标)
//
// 使用标准 gnomonic 正投影公式 (Calabretta & Greisen 2002, TAN 投影)
// 与 poly_clip.cpp 的 gnomonicForward 一致
//
// 输入: ra, dec (天球坐标, 度)
// 输出: xi, eta (中间世界坐标, 度, 供 CD⁻¹ 使用)
//
// 公式:
//   cos(c) = sin(dec0)*sin(dec) + cos(dec0)*cos(dec)*cos(ra-ra0)
//   xi  = cos(dec)*sin(ra-ra0) / cos(c)
//   eta = (cos(dec0)*sin(dec) - sin(dec0)*cos(dec)*cos(ra-ra0)) / cos(c)
//
// 保护:
//   - cos(c) ≈ 0 (天极对面, 投影发散) → 返回极大值
// ============================================================================
void WcsSip::tanWorldToIntermediate(double ra, double dec,
                                     double& xi, double& eta) const {
    const double ra0_deg  = m_wcs.crval[0];
    const double dec0_deg = m_wcs.crval[1];

    const double ra_rad   = ra  * D2R;
    const double dec_rad  = dec * D2R;
    const double ra0_rad  = ra0_deg  * D2R;
    const double dec0_rad = dec0_deg * D2R;

    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);
    const double sdec  = std::sin(dec_rad);
    const double cdec  = std::cos(dec_rad);
    const double dra   = ra_rad - ra0_rad;
    const double cdra  = std::cos(dra);
    const double sdra  = std::sin(dra);

    // cos(c) = 点到切点的角距离余弦
    const double cosc = sdec0 * sdec + cdec0 * cdec * cdra;

    // 保护: cos(c) 接近 0 (距切点 ~90°, 投影背面) → 投影发散
    if (std::fabs(cosc) < 1e-12) {
        WCS_LOG("警告: TAN 正投影发散 (cos(c)=%.3e), 点可能在投影背面", cosc);
        xi  = 1e6;
        eta = 1e6;
        return;
    }

    // xi, eta (弧度) → 度
    double xi_rad  = cdec * sdra / cosc;
    double eta_rad = (cdec0 * sdec - sdec0 * cdec * cdra) / cosc;

    xi  = xi_rad * R2D;
    eta = eta_rad * R2D;
}

// ============================================================================
// 5. pixelToSky (像素 → 天球, 前向转换)
//
// 步骤:
//   1. 归一化坐标: dx = x - (CRPIX1-1), dy = y - (CRPIX2-1)
//      注意: CRPIX 是 1-based, 输入 x/y 是 0-based
//      内部统一用 0-based, 自动处理 CRPIX 的 1-based 偏移
//   2. 前向 SIP 修正 (A/B): dx' = dx + A(dx,dy), dy' = dy + B(dx,dy)
//      FITS 标准: A/B 是前向多项式 (像素 → 中间坐标), 用于 pixel→world
//   3. CD 矩阵: xi = cd[0]*dx' + cd[1]*dy', eta = cd[2]*dx' + cd[3]*dy'
//   4. TAN 反投影: (xi, eta) → (RA, Dec)
// ============================================================================
template <typename T>
void WcsSip::pixelToSkyT(T x, T y, T& ra, T& dec) const {
    if (!m_hasWcs) {
        WCS_LOG("错误: pixelToSky 调用时 WCS 无效, 返回零值");
        ra = T(0);
        dec = T(0);
        return;
    }

    // 1. 归一化像素坐标 (0-based)
    //    CRPIX 是 1-based, 输入 x/y 是 0-based
    //    dx = x - (CRPIX1 - 1) = x - CRPIX1 + 1
    T dx = x - T(m_wcs.crpix[0] - 1.0);
    T dy = y - T(m_wcs.crpix[1] - 1.0);

    // 2. 前向 SIP 修正 (A/B)
    //    FITS 标准: A/B 是前向多项式, 用于像素 → 中间坐标方向
    //    U = dx + A(dx, dy), V = dy + B(dx, dy)
    if (m_wcs.sip.order > 0) {
        T f = evalSipT<T>(m_wcs.sip.a, dx, dy, m_wcs.sip.order);
        T g = evalSipT<T>(m_wcs.sip.b, dx, dy, m_wcs.sip.order);
        dx += f;
        dy += g;
    }

    // 3. CD 矩阵: 像素 → 中间世界坐标 (度)
    T xi  = T(m_wcs.cd[0]) * dx + T(m_wcs.cd[1]) * dy;
    T eta = T(m_wcs.cd[2]) * dx + T(m_wcs.cd[3]) * dy;

    // 4. TAN 反投影: 中间坐标 → 天球
    tanIntermediateToWorldT<T>(xi, eta, ra, dec);
}

// 兼容包装 (double 实例, 旧调用方)
void WcsSip::pixelToSky(double x, double y, double& ra, double& dec) const {
    pixelToSkyT<double>(x, y, ra, dec);
}

// ============================================================================
// 6. skyToPixel (天球 → 像素, 逆向转换)
//
// 步骤:
//   1. TAN 正投影: (RA, Dec) → (xi, eta) (度)
//   2. CD 逆矩阵: dx = cdInv[0]*xi + cdInv[1]*eta, dy = cdInv[2]*xi + cdInv[3]*eta
//   3. 逆向 SIP 修正 (AP/BP): dx' = dx + AP(dx,dy), dy' = dy + BP(dx,dy)
//      FITS 标准: AP/BP 是逆向多项式 (中间坐标 → 像素), 用于 world→pixel
//      一次迭代通常足够 (SIP 修正量很小)
//   4. 转回 0-based 像素坐标: x = dx' + (CRPIX1-1), y = dy' + (CRPIX2-1)
// ============================================================================
void WcsSip::skyToPixel(double ra, double dec, double& x, double& y) const {
    if (!m_hasWcs) {
        WCS_LOG("错误: skyToPixel 调用时 WCS 无效, 返回零值");
        x = 0.0;
        y = 0.0;
        return;
    }

    // 1. TAN 正投影: 天球 → 中间世界坐标 (度)
    double xi, eta;
    tanWorldToIntermediate(ra, dec, xi, eta);

    // 2. CD 逆矩阵: 中间坐标 → 像素偏移
    double dx = m_cdInv[0] * xi + m_cdInv[1] * eta;
    double dy = m_cdInv[2] * xi + m_cdInv[3] * eta;

    // 3. 逆向 SIP 修正 (AP/BP)
    //    FITS 标准: AP/BP 是逆向多项式, 用于中间坐标 → 像素方向
    //    x = dx + AP(dx, dy), y = dy + BP(dx, dy)
    //    一次迭代即可 (SIP 修正量通常 << 1 像素)
    if (m_wcs.sip.ap_order > 0) {
        double fp = evalSip(m_wcs.sip.ap, dx, dy, m_wcs.sip.ap_order);
        double gp = evalSip(m_wcs.sip.bp, dx, dy, m_wcs.sip.ap_order);
        dx += fp;
        dy += gp;
    }

    // 4. 转回 0-based 像素坐标
    x = dx + (m_wcs.crpix[0] - 1.0);
    y = dy + (m_wcs.crpix[1] - 1.0);
}

// ============================================================================
// 7. pixelToSkyBatch (批量像素 → 天球)
//
// xy:    [x0, y0, x1, y1, ...] 长度 count*2 (0-based 像素坐标)
// radec: [ra0, dec0, ra1, dec1, ...] 长度 count*2 (度)
// ============================================================================
void WcsSip::pixelToSkyBatch(const double* xy, int count, double* radec) const {
    if (!m_hasWcs || xy == nullptr || radec == nullptr || count <= 0) {
        WCS_LOG("错误: pixelToSkyBatch 参数无效 (hasWcs=%d, xy=%p, radec=%p, count=%d)",
                (int)m_hasWcs, (const void*)xy, (void*)radec, count);
        return;
    }

    for (int i = 0; i < count; ++i) {
        pixelToSky(xy[i * 2], xy[i * 2 + 1],
                   radec[i * 2], radec[i * 2 + 1]);
    }
}

// ============================================================================
// R11 阶段7: 显式实例化 FP32/FP64 双实例 (真 Scalar WCS 几何)
// ============================================================================
template void WcsSip::pixelToSkyT<float>(float, float, float&, float&) const;
template void WcsSip::pixelToSkyT<double>(double, double, double&, double&) const;
template float WcsSip::evalSipT<float>(const double*, float, float, int);
template double WcsSip::evalSipT<double>(const double*, double, double, int);
template void WcsSip::tanIntermediateToWorldT<float>(float, float, float&, float&) const;
template void WcsSip::tanIntermediateToWorldT<double>(double, double, double&, double&) const;

} // namespace drizzle
