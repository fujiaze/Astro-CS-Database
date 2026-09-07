// lib/phase3_session/p3_wcs.cpp — TAN(gnomonic) WCS 实现 (ALG-P3-002/003) — P3-002
// 数学: Calabretta & Greisen (2002) 标准球面三角公式(RA wrap 经 atan2+fmod 归一)。
#include "p3_wcs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace astrocs::phase3 {

namespace {
constexpr double kDeg = 180.0 / M_PI;
constexpr double kRad = M_PI / 180.0;
constexpr double kMaxAbsDec = 85.0;       // SCI/API/session 单一条件: abs(dec)<=85°
// 最大尺寸来自配置/资源合同 (PHASE3_API_V1 §2 默认 20000), 可被
// 编译期配置覆盖; 不硬编码业务值 (最大尺寸来自资源/配置合同)。
#ifndef ASTROCS_P3_MAX_SIDE
constexpr int kMaxSide = 20000;
#else
constexpr int kMaxSide = ASTROCS_P3_MAX_SIDE;
#endif

void normalize_ra(double* ra) {
    *ra = std::fmod(*ra, 360.0);
    if (*ra < 0) *ra += 360.0;
}
}  // namespace

P3WcsStatus p3_wcs_make(double centre_ra_deg, double centre_dec_deg,
                        double scale_deg_per_px, int width_px, int height_px,
                        const char* parity, double rotation_pa_deg,
                        P3WcsDescriptor* out) {
    if (!out) return P3_WCS_PARAM;
    *out = P3WcsDescriptor{};
    const std::string proj = "TAN";
    if (parity == nullptr) parity = "east_left";
    const std::string par = parity;
    if (par != "east_left" && par != "east_right") return P3_WCS_PARAM;
    if (std::fabs(centre_dec_deg) > kMaxAbsDec) return P3_WCS_PARAM;
    if (!(scale_deg_per_px > 0.0)) return P3_WCS_PARAM;
    if (width_px < 1 || width_px > kMaxSide || height_px < 1 || height_px > kMaxSide)
        return P3_WCS_PARAM;

    out->crval_ra_deg = centre_ra_deg;
    out->crval_dec_deg = centre_dec_deg;
    out->crpix_x = (width_px + 1) / 2.0;    // pixel-center(FITS 1-based): (W+1)/2
    out->crpix_y = (height_px + 1) / 2.0;
    out->width_px = width_px;
    out->height_px = height_px;
    // G1 (ALG-P3-002, docs/algorithms/PHASE3_RESAMPLE.md §2) 冻结输出 WCS 构造
    // (FITS 1-based, CD-only, 对角, PA=0 精确形式):
    //   east_left:  CD = diag(−s, +s)   (x 增 → RA 减, 北朝上)
    //   east_right: CD = diag(+s, −s)   (x 增 → RA 增, y 增 → Dec 减)
    // PA≠0 推广 = G1 对角形式与图像平面旋转的一致复合:
    //   CD = R(−PA)·diag(sgn_x·s, sgn_y·s),
    //   R(−PA) = [[cos PA, sin PA], [−sin PA, cos PA]]
    //   (sgn_x, sgn_y): east_left=(−1,+1), east_right=(+1,−1)。
    // 推导: TAN 切平面中间坐标 (ξ,η) 沿 (东,北) 为右手系 (det>0); 输出图像
    // 两种 parity 均要求 det(CD) = sgn_x·sgn_y·s² = −s² <0 (平面映像镜像一次,
    // 保持天球手性, 与 SCI-P3-001 §9a-4 收紧 CD1_1 符号后的 G1 一致)。
    // 展开式:
    //   CD1_1 = sgn_x·s·cosPA;  CD1_2 = sgn_y·s·sinPA
    //   CD2_1 = −sgn_x·s·sinPA; CD2_2 = sgn_y·s·cosPA
    // P0 修复 (bughunt_p0_wcs): 旧实现 east_right 分支误用 sgn_y=+1,
    // 使 PA=0 时 CD=diag(+s,+s), 违反 G1 冻结的 diag(+s,−s) (y 镜像错误)。
    // PA=0 时 cos=1/sin=0 精确退化到 G1 对角形式; PA 语义不变 (天北相对
    // +y 的位置角, 逆时针为正), east_left 分支与旧实现逐元素 bitwise 一致。
    const double pa = rotation_pa_deg * kRad;
    const double sgn_x = (par == "east_left") ? -1.0 : 1.0;
    const double sgn_y = -sgn_x;
    const double s = scale_deg_per_px;
    const double cp = std::cos(pa);
    const double sp = std::sin(pa);
    out->cd[0][0] = sgn_x * s * cp;
    out->cd[0][1] = sgn_y * s * sp;
    out->cd[1][0] = -sgn_x * s * sp;
    out->cd[1][1] = sgn_y * s * cp;

    // 输出四角同半球守卫(四角 world 变换全部成功)
    const double corners[4][2] = {{0, 0}, {double(width_px - 1), 0},
                                  {0, double(height_px - 1)},
                                  {double(width_px - 1), double(height_px - 1)}};
    for (const auto& c : corners) {
        double ra, dec;
        const P3WcsStatus st = p3_wcs_pix2world(out, c[0], c[1], &ra, &dec);
        if (st != P3_WCS_OK) return st;
    }
    (void)proj;
    return P3_WCS_OK;
}

P3WcsStatus p3_wcs_pix2world(const P3WcsDescriptor* d, double x, double y,
                             double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3_WCS_PARAM;
    // 中间坐标 ξ,η(deg): CD·(pix − crpix)
    const double dx = (x + 1.0) - d->crpix_x;   // FITS 1-based
    const double dy = (y + 1.0) - d->crpix_y;
    const double xi = (d->cd[0][0] * dx + d->cd[0][1] * dy) * kRad;   // rad
    const double eta = (d->cd[1][0] * dx + d->cd[1][1] * dy) * kRad;
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double r = std::sqrt(xi * xi + eta * eta);
    if (r >= M_PI / 2.0) return P3_WCS_HEMISPHERE;    // 跨 TAN 半球
    const double theta = std::atan2(1.0, r);          // = atan(1/r)
    const double phi = std::atan2(-xi, eta);          // 自 +dec 轴向 −RA
    const double sd = std::sin(d0), cd = std::cos(d0);
    const double st = std::sin(theta), ct = std::cos(theta);
    const double sp = std::sin(phi), cp = std::cos(phi);
    const double dec = std::asin(st * sd + ct * cd * cp);
    // Calabretta & Greisen: Δα = arg(−cosθ·sinφ, sinθ·cosθ0 + cosθ·sinθ0·cosφ)
    const double dra = std::atan2(-ct * sp, cd * st - sd * ct * cp);
    double ra_deg_out = (a0 + dra) * kDeg;
    normalize_ra(&ra_deg_out);
    *ra_deg = ra_deg_out;
    *dec_deg = dec * kDeg;
    return P3_WCS_OK;
}

P3WcsStatus p3_wcs_world2pix(const P3WcsDescriptor* d, double ra_deg, double dec_deg,
                             double* x, double* y) {
    if (!d || !x || !y) return P3_WCS_PARAM;
    if (std::fabs(dec_deg) > kMaxAbsDec) return P3_WCS_PARAM;   // 极点邻域拒
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double a = ra_deg * kRad;
    const double dd = dec_deg * kRad;
    const double denom = std::sin(d0) * std::sin(dd) + std::cos(d0) * std::cos(dd) *
                                                          std::cos(a - a0);
    if (denom <= 0.0) return P3_WCS_HEMISPHERE;       // 背面(跨半球)
    const double xi = std::cos(dd) * std::sin(a - a0) / denom;            // rad
    const double eta = (std::sin(dd) * std::cos(d0) -
                        std::cos(dd) * std::sin(d0) * std::cos(a - a0)) / denom;
    const double xid = xi * kDeg, etad = eta * kDeg;
    // 线性解 CD·δ = (ξ,η): δ = CD⁻¹·(ξ,η)
    const double det = d->cd[0][0] * d->cd[1][1] - d->cd[0][1] * d->cd[1][0];
    if (std::fabs(det) < 1e-300) return P3_WCS_PARAM;
    const double dx = (d->cd[1][1] * xid - d->cd[0][1] * etad) / det;
    const double dy = (-d->cd[1][0] * xid + d->cd[0][0] * etad) / det;
    *x = dx + d->crpix_x - 1.0;   // 0-based 像素
    *y = dy + d->crpix_y - 1.0;
    return P3_WCS_OK;
}

std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d) {
    if (!d) return {};
    char buf[128];
    std::string out;
    auto add = [&](const std::string& line) { out += line + "\n"; };
    add("CTYPE1= 'RA---TAN'");
    add("CTYPE2= 'DEC--TAN'");
    add("CUNIT1 = 'deg'");
    add("CUNIT2 = 'deg'");
    std::snprintf(buf, sizeof(buf), "CRPIX1 = %.10f", d->crpix_x); add(buf);
    std::snprintf(buf, sizeof(buf), "CRPIX2 = %.10f", d->crpix_y); add(buf);
    std::snprintf(buf, sizeof(buf), "CRVAL1 = %.10f", d->crval_ra_deg); add(buf);
    std::snprintf(buf, sizeof(buf), "CRVAL2 = %.10f", d->crval_dec_deg); add(buf);
    std::snprintf(buf, sizeof(buf), "CD1_1 = %.12e", d->cd[0][0]); add(buf);
    std::snprintf(buf, sizeof(buf), "CD1_2 = %.12e", d->cd[0][1]); add(buf);
    std::snprintf(buf, sizeof(buf), "CD2_1 = %.12e", d->cd[1][0]); add(buf);
    std::snprintf(buf, sizeof(buf), "CD2_2 = %.12e", d->cd[1][1]); add(buf);
    return out;
}

}  // namespace astrocs::phase3
