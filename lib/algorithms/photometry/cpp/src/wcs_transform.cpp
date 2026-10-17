// wcs_transform.cpp - TAN+SIP投影坐标转换（ACSD phase1 photometry 模块内部实现）
// 职责: 天球坐标(RA/Dec)与像素坐标(x/y)互转（gnomonic TAN + SIP 畸变修正）；输入块: Gaia 星表 + WCS 参数；
// 输出: 像素坐标；线程模型: 单线程调用（无内部并行）。数值对照: astropy.wcs 方法对照（非代码引用）。

#include "wcs_transform.h"

#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <algorithm>

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

    // P0(bug 狩猎 R5): SIP order 外部输入面硬校验 —— 对齐 f1cb487c 已入库
    // 口径 (healpix_drizzle wcs_sip/hp_drizzle_api): 正式支持 [0,5] (6x6
    // 系数数组, m_sip_*_buf[36] 按 i*6+j 索引)。此前 sip_order 无上界校验,
    // 外部 header A_ORDER>=6 时 evalSip 循环 coeffs[i*6+j] (i=order, j=0 →
    // 6*order > 35) 越界读 36 项成员缓冲 (order=8 → 读至 [48], 出对象)。
    // 越界一律硬失败抛 std::invalid_argument, 禁止静默截断; 异常由 pc_api
    // C 边界 try/catch 屏障转错误码 (见 pc_api.cpp)。
    if (sip_order < 0 || sip_order > 5) {
        std::fprintf(stderr, "[wcs_transform] 错误: SIP order 非法 (%d, 正式支持 [0,5]), 拒绝构造\n",
                     sip_order);
        throw std::invalid_argument("WcsTransform: SIP order 越界 (正式支持 [0,5])");
    }

    // CD矩阵行列式与逆（|det| < 1e-15 视为退化：逆矩阵置零，后续 skyToPixel/pixelToSky
    // 按退化输入处理，调用方按失败-置信度语义拒绝伪解；阈值为数值奇异守卫，见 ALG-WCS 落地约束）
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
// 前置条件: order ∈ [0,5] 且 coeffs 指向 36 项缓冲 —— 由构造函数硬校验
// 保证 (构造层唯一防线, 见上); 此处不做静默截断。
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

    // TAN/gnomonic 仅在切点可见半球 cos(c)>0 有唯一有限正投影；
    // 背半球不是另一组合法像素，必须返回逐星无效值。
    const double cosc = sdec0 * sdec + cdec0 * cdec * cdra;
    if (!std::isfinite(cosc) || cosc <= 1e-12) {
        xi = std::numeric_limits<double>::quiet_NaN();
        eta = xi;
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
// 1. 归一化: dx = x - (CRPIX1-1), dy = y - (CRPIX2-1) (CRPIX是1-based)
// 2. 前向SIP(A/B): dx' = dx + A(dx,dy), dy' = dy + B(dx,dy)
// 3. CD矩阵: xi = cd[0]*dx' + cd[1]*dy', eta = cd[2]*dx' + cd[3]*dy'
// 4. TAN反投影: (xi,eta) -> (RA,Dec)
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
// 1. TAN正投影: (RA,Dec) -> (xi,eta)（#57: 背半球/非法坐标 -> NaN）
// 2. CD逆矩阵: t = (cdInv·(xi,eta))（#61: 奇异CD -> NaN）
// 3. SIP 逆解: 目标求 F(u,v) = (u + A(u,v), v + B(u,v)) = t
// 4. 转回0-based: x = u + (CRPIX1-1), y = v + (CRPIX2-1)
//
// ── #92 A1-FIX: 唯一统一逆解算子 ──────────────────────────────────────────
// 缺陷（修复前）: 当显式 AP/BP 存在时只做**一次性**近似逆
// (u,v) = t + (AP(t), BP(t)) 就进入 1e-7 px 前向残差门。AP/BP 是求解器
// **拟合**出的近似逆多项式，不保证自身即满足该门；真实 M42 帧的
// AP_0_0 = -4.33e-5 px 使切点处残差 4.33e-5 > 1e-7，2309/2309 星被判 NaN。
//
// 修复: AP/BP 只作**初值**，有无 AP/BP 两条路径统一执行同一段有界不动点精化
// (最多 64 轮, 步长 <=1e-10 早停)，精化完成后仍以原有 1e-7 px 前向残差门
// 验收；非有限/不收敛一律 NaN（#56 的保守检查与 #57/#61 的拒绝条件全部保留）。
// 不放大 1e-7、不放宽匹配半径、不关闭 SIP、不删门、不用一次式 AP/BP 直接放行。
//
// 次级合同缺口（本批只登记，不改 API）: 生产者 p1_wcs.json 声明
// sip.ap_order=5，但消费者 WcsTransform 只有单一 m_sip_order(=sip_order=3)，
// AP/BP 以 3 阶求值。该缺口只影响**初值质量**（帧角落 ~0.5 px），精化后
// 对最终精度无影响；真正的 API 改造（ap_order 独立入参）需另批最小补丁 + 真实 A/B。
// ============================================================================
void WcsTransform::skyToPixel(double ra, double dec, double& x, double& y) const {
    double xi, eta;
    tanWorldToIntermediate(ra, dec, xi, eta);
    // 逆变换不得在背半球、非法天球坐标或奇异CD时产生虚假中心像素。
    if (!std::isfinite(xi) || !std::isfinite(eta) ||
        !std::isfinite(m_cdDet) || std::fabs(m_cdDet) < 1e-15) {
        x = std::numeric_limits<double>::quiet_NaN();
        y = x;
        return;
    }

    // 线性中间坐标 t（含 SIP 修正后的目标中间坐标）。
    const double t_x = m_cdInv[0] * xi + m_cdInv[1] * eta;
    const double t_y = m_cdInv[2] * xi + m_cdInv[3] * eta;

    double dx = t_x;
    double dy = t_y;

    if (m_has_sip) {
        // (1) 初值: 有 AP/BP 用 t + (AP(t),BP(t))；否则用 t。
        if (m_sip_ap != nullptr && m_sip_bp != nullptr) {
            const double ap = evalSip(m_sip_ap, t_x, t_y, m_sip_order);
            const double bp = evalSip(m_sip_bp, t_x, t_y, m_sip_order);
            const double u0 = t_x + ap;
            const double v0 = t_y + bp;
            // 畸变求值异常 => 退回受保护的线性初值 t（不产生"看似有效"的解）；
            // 结果是否成立仍由 (2) 的精化与最终前向残差门裁决。
            if (std::isfinite(u0) && std::isfinite(v0)) {
                dx = u0;
                dy = v0;
            }
        }
        // (2) 有界不动点精化: (u_{j+1},v_{j+1}) = t - (A(u_j,v_j), B(u_j,v_j))。
        // 两条路径同一实现；固定轮数不构成收敛证明，故 (3) 仍必须检验。
        for (int iter = 0; iter < 64; ++iter) {
            const double f = evalSip(m_sip_a, dx, dy, m_sip_order);
            const double g = evalSip(m_sip_b, dx, dy, m_sip_order);
            const double u_new = t_x - f;
            const double v_new = t_y - g;
            if (!std::isfinite(u_new) || !std::isfinite(v_new)) {
                dx = std::numeric_limits<double>::quiet_NaN();
                dy = dx;
                break;
            }
            const double step = std::max(std::fabs(u_new - dx), std::fabs(v_new - dy));
            dx = u_new;
            dy = v_new;
            if (step <= 1e-10) break;
        }
    }

    // (3) 精化完成后的唯一验收门: 逐分量前向残差 <= 1e-7 px 且全部有限。
    if (m_has_sip) {
        const double check_x = dx + evalSip(m_sip_a, dx, dy, m_sip_order);
        const double check_y = dy + evalSip(m_sip_b, dx, dy, m_sip_order);
        if (!std::isfinite(check_x) || !std::isfinite(check_y) ||
            std::fabs(check_x - t_x) > 1e-7 ||
            std::fabs(check_y - t_y) > 1e-7) {
            x = std::numeric_limits<double>::quiet_NaN();
            y = x;
            return;
        }
    }
    x = dx + (m_crpix1 - 1.0);
    y = dy + (m_crpix2 - 1.0);
}

} // namespace pc

// ---------------------------------------------------------------------------
// 公开 C ABI：天球 -> 像素（含 SIP），输出 **0-based 像素**。
// 实现复用本文件的 pc::WcsTransform::skyToPixel —— 单一事实来源，不在别处重写畸变数学。
// ---------------------------------------------------------------------------
extern "C" int pc_project_sky_to_pixel(
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order, const double* sip_a, const double* sip_b,
    const double* ra, const double* dec, int n,
    double* out_x, double* out_y) {
  if (ra == nullptr || dec == nullptr || out_x == nullptr || out_y == nullptr || n < 0)
    return -1;
  if (sip_order < 0 || sip_order > 5) return -2;
  try {
    const double* A = (sip_order == 0) ? nullptr : sip_a;
    const double* B = (sip_order == 0) ? nullptr : sip_b;
    pc::WcsTransform w(crval1, crval2, crpix1, crpix2, cd11, cd12, cd21, cd22,
                       sip_order, A, B, nullptr, nullptr);
    for (int i = 0; i < n; ++i)
      w.skyToPixel(ra[i], dec[i], out_x[i], out_y[i]);
    return 0;
  } catch (...) {
    return -3;
  }
}
