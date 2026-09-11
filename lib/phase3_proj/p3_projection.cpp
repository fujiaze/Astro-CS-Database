// lib/phase3_proj/p3_projection.cpp — 版本化 projection registry + 冻结四投影
// 实现 — P3-001 (ASTROCS-CONSTITUTION-001 §7.3/§18.1, ALG-P3-PROJ-IMPL-001 §15)
//
// 数学冻结口径:
//   TAN(gnomonic): 逐式沿用 lib/phase3_session/p3_wcs.cpp 冻结生产事实
//     （Calabretta & Greisen (2002) 标准球面三角公式，RA wrap 经 atan2+fmod
//     归一；G1 CD 构造 east_left/east_right + PA 推广 + P0 修复 bughunt_p0_wcs）。
//     共享旋转核与其严格同构（sinθ=sinδ sinδ₀+cosδ cosδ₀ cosΔα 即 denom），
//     本文件 TAN 路径保持既有表达式顺序以支持 bitwise 对拍。
//   SIN(orthographic)/CAR(plate carrée)/AIT(Aitoff): 宪章 §18.1 负责人裁决
//     新增 claim，公式 = FITS WCS Paper II 标准定义（ALG §15 逐式冻结）:
//     共享 native↔celestial 旋转核 + 各投影 native 层:
//       TAN: X=−cotθ·sinφ, Y=cotθ·cosφ            (zenithal, θ₀=CRVAL2)
//       SIN: X=cosθ·(−sinφ), Y=cosθ·cosφ          (zenithal, θ₀=CRVAL2)
//       CAR: X=φ, Y=−θ                            (cylindrical, θ₀=+90° 恒等旋转)
//       AIT: X=2cosθ·sin(φ/2)/D, Y=sinθ/D, D=√(1+cosθ·cos(φ/2))
//                                                 (pseudo-cylindrical, θ₀=+90°)
//     旋转核（rad）: dec=asin(sinθ sinδ₀+cosθ cosδ₀ cosφ);
//       Δα=atan2(−cosθ sinφ, cosδ₀ sinθ−sinδ₀ cosθ cosφ);
//       逆: sinθ=sinδ sinδ₀+cosδ cosδ₀ cosΔα;
//       φ=atan2(−cosδ sinΔα, sinδ cosδ₀−cosδ sinδ₀ cosΔα)（zenithal 半球
//       denom=sinθ>0 与既有 TAN denom>0 背面判定数学等价）。
//   CAR/AIT 天球惯例: θ₀=+90°（native 北极=天球北极, LONPOLE=0 语义）⇒
//     native (φ,θ)=(α−α₀, δ) 恒等；CRVAL2 仅记录于 header 不进入映射
//     （ALG §15.4/§15.5 冻结声明）。
#include "p3_projection.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace astrocs::phase3proj {

namespace {
constexpr double kDeg = 180.0 / M_PI;
constexpr double kRad = M_PI / 180.0;
constexpr double kMaxAbsDec = 85.0;   // 四投影统一中心守卫（保守冻结，TAN=SCI 单一条件）
// 最大尺寸与 lib/phase3_session/p3_wcs.cpp 冻结默认一致（PHASE3_API_V1 §2）。
#ifndef ASTROCS_P3_MAX_SIDE
constexpr int kMaxSide = 20000;
#else
constexpr int kMaxSide = ASTROCS_P3_MAX_SIDE;
#endif

void normalize_ra(double* ra) {
    *ra = std::fmod(*ra, 360.0);
    if (*ra < 0) *ra += 360.0;
}

// G1 CD 构造（四投影统一，逐式 p3_wcs.cpp:51-78 冻结式）:
//   east_left:  CD = diag(−s, +s)·R  ⇒ CD1_1=−s·cosPA …（PA 推广展开式）
//   east_right: CD = diag(+s, −s)·R
//   CD = R(−PA)·diag(sgn_x·s, sgn_y·s)，(sgn_x,sgn_y): east_left=(−1,+1),
//   east_right=(+1,−1)；det(CD)=−s²<0 手性冻结（P0 修复 sgn_y=−sgn_x 内含）。
void g1_build_cd(double scale_deg_per_px, const char* parity, double rotation_pa_deg,
                 double cd[2][2]) {
    const double pa = rotation_pa_deg * kRad;
    const double sgn_x = (std::strcmp(parity, "east_left") == 0) ? -1.0 : 1.0;
    const double sgn_y = -sgn_x;
    const double s = scale_deg_per_px;
    const double cp = std::cos(pa);
    const double sp = std::sin(pa);
    cd[0][0] = sgn_x * s * cp;
    cd[0][1] = sgn_y * s * sp;
    cd[1][0] = -sgn_x * s * sp;
    cd[1][1] = sgn_y * s * cp;
}

// 像素→中间坐标 (deg)→CD 平面 (deg)（四投影统一，逐式 p3_wcs.cpp:97-100）
void pix_to_plane(const P3ProjectionDescriptor* d, double x, double y,
                  double* xd, double* yd) {
    const double dx = (x + 1.0) - d->crpix_x;   // FITS 1-based
    const double dy = (y + 1.0) - d->crpix_y;
    *xd = d->cd[0][0] * dx + d->cd[0][1] * dy;  // deg
    *yd = d->cd[1][0] * dx + d->cd[1][1] * dy;  // deg
}

// 中间坐标 (deg)→CD⁻¹→像素（四投影统一，逐式 p3_wcs.cpp:136-141）
P3ProjectionStatus plane_to_pix(const P3ProjectionDescriptor* d,
                                double xd, double yd, double* x, double* y) {
    const double det = d->cd[0][0] * d->cd[1][1] - d->cd[0][1] * d->cd[1][0];
    if (std::fabs(det) < 1e-300) return P3ProjectionStatus::P3_PROJ_PARAM;
    const double dx = (d->cd[1][1] * xd - d->cd[0][1] * yd) / det;
    const double dy = (-d->cd[1][0] * xd + d->cd[0][0] * yd) / det;
    *x = dx + d->crpix_x - 1.0;   // 0-based 像素
    *y = dy + d->crpix_y - 1.0;
    return P3ProjectionStatus::P3_PROJ_OK;
}

// native(φ,θ) (rad)→天球 (RA 归一 deg, Dec deg)（zenithal 旋转核，θ₀=CRVAL2；
// TAN/SIN 共用；表达式顺序与 p3_wcs.cpp:105-114 一致）
void native_to_sky_zenithal(const P3ProjectionDescriptor* d,
                            double phi, double theta,
                            double* ra_deg, double* dec_deg) {
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double sd = std::sin(d0), cd_ = std::cos(d0);
    const double st = std::sin(theta), ct = std::cos(theta);
    const double sp = std::sin(phi), cp = std::cos(phi);
    const double dec = std::asin(st * sd + ct * cd_ * cp);
    const double dra = std::atan2(-ct * sp, cd_ * st - sd * ct * cp);
    double ra_out = (a0 + dra) * kDeg;
    normalize_ra(&ra_out);
    *ra_deg = ra_out;
    *dec_deg = dec * kDeg;
}

// 天球 (RA deg, Dec deg)→zenithal native 中间坐标 (ξ,η) (rad)（θ₀=CRVAL2；
// 表达式顺序与 p3_wcs.cpp:124-134 一致；denom≤0 → 半球外）
P3ProjectionStatus sky_to_plane_zenithal(const P3ProjectionDescriptor* d,
                                         double ra_deg, double dec_deg,
                                         double* xi_rad, double* eta_rad) {
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double a = ra_deg * kRad;
    const double dd = dec_deg * kRad;
    const double denom = std::sin(d0) * std::sin(dd) +
                         std::cos(d0) * std::cos(dd) * std::cos(a - a0);
    if (denom <= 0.0) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;
    *xi_rad = std::cos(dd) * std::sin(a - a0) / denom;
    *eta_rad = (std::sin(dd) * std::cos(d0) -
                std::cos(dd) * std::sin(d0) * std::cos(a - a0)) / denom;
    return P3ProjectionStatus::P3_PROJ_OK;
}

// ---------------- TAN（冻结逐式路径，p3_wcs.cpp:93-118/:120-143 同构）--------
P3ProjectionStatus tan_pix2world(const P3ProjectionDescriptor* d, double x, double y,
                                 double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3ProjectionStatus::P3_PROJ_PARAM;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double xi = xd * kRad;    // rad
    const double eta = yd * kRad;
    const double r = std::sqrt(xi * xi + eta * eta);
    if (r >= M_PI / 2.0) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;
    const double theta = std::atan2(1.0, r);
    const double phi = std::atan2(-xi, eta);
    native_to_sky_zenithal(d, phi, theta, ra_deg, dec_deg);
    return P3ProjectionStatus::P3_PROJ_OK;
}

P3ProjectionStatus tan_world2pix(const P3ProjectionDescriptor* d, double ra_deg,
                                 double dec_deg, double* x, double* y) {
    if (!d || !x || !y) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (std::fabs(dec_deg) > kMaxAbsDec) return P3ProjectionStatus::P3_PROJ_PARAM;
    double xi, eta;
    const P3ProjectionStatus st = sky_to_plane_zenithal(d, ra_deg, dec_deg, &xi, &eta);
    if (st != P3ProjectionStatus::P3_PROJ_OK) return st;
    return plane_to_pix(d, xi * kDeg, eta * kDeg, x, y);
}

// ---------------- SIN（orthographic，ALG §15.2 claim）-----------------------
// 正向: X=cosθ·(−sinφ), Y=cosθ·cosφ (rad)；R=cosθ=√(X²+Y²) ≤ 1。
P3ProjectionStatus sin_pix2world(const P3ProjectionDescriptor* d, double x, double y,
                                 double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3ProjectionStatus::P3_PROJ_PARAM;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double xi = xd * kRad;
    const double eta = yd * kRad;
    const double rho = std::sqrt(xi * xi + eta * eta);
    if (rho > 1.0) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;  // SIN 半球域
    const double theta = std::acos(rho);          // ρ=cosθ
    const double phi = std::atan2(-xi, eta);      // 与 TAN 同 native 约定
    native_to_sky_zenithal(d, phi, theta, ra_deg, dec_deg);
    return P3ProjectionStatus::P3_PROJ_OK;
}

P3ProjectionStatus sin_world2pix(const P3ProjectionDescriptor* d, double ra_deg,
                                 double dec_deg, double* x, double* y) {
    if (!d || !x || !y) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (std::fabs(dec_deg) > kMaxAbsDec) return P3ProjectionStatus::P3_PROJ_PARAM;
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double a = ra_deg * kRad;
    const double dd = dec_deg * kRad;
    const double stheta = std::sin(d0) * std::sin(dd) +
                          std::cos(d0) * std::cos(dd) * std::cos(a - a0);
    if (stheta <= 0.0) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;  // sinθ>0 半球
    const double ctheta = std::sqrt(std::max(0.0, 1.0 - stheta * stheta));
    const double phi = std::atan2(-std::cos(dd) * std::sin(a - a0),
                                  std::sin(dd) * std::cos(d0) -
                                      std::cos(dd) * std::sin(d0) * std::cos(a - a0));
    const double xi = -ctheta * std::sin(phi);    // X (rad)
    const double eta = ctheta * std::cos(phi);    // Y (rad)
    return plane_to_pix(d, xi * kDeg, eta * kDeg, x, y);
}

// ---------------- CAR（plate carrée，ALG §15.3 claim）-----------------------
// 正向: X=φ, Y=−θ (θ₀=+90° 恒等旋转 ⇒ φ=α−α₀, θ=δ)；冻结域 |δ|≤90°。
P3ProjectionStatus car_pix2world(const P3ProjectionDescriptor* d, double x, double y,
                                 double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3ProjectionStatus::P3_PROJ_PARAM;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double phi = xd * kRad;    // φ = X (rad)
    const double theta = -yd * kRad; // θ = −Y (rad)
    if (std::fabs(theta) > M_PI / 2.0) return P3ProjectionStatus::P3_PROJ_PARAM;
    // θ₀=+90°: δ=θ, α=α₀+φ（恒等旋转，RA wrap 归一）
    double ra_out = d->crval_ra_deg + phi * kDeg;
    normalize_ra(&ra_out);
    *ra_deg = ra_out;
    *dec_deg = theta * kDeg;
    return P3ProjectionStatus::P3_PROJ_OK;
}

P3ProjectionStatus car_world2pix(const P3ProjectionDescriptor* d, double ra_deg,
                                 double dec_deg, double* x, double* y) {
    if (!d || !x || !y) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (std::fabs(dec_deg) > 90.0) return P3ProjectionStatus::P3_PROJ_PARAM;
    double dra = std::fmod(ra_deg - d->crval_ra_deg, 360.0);
    if (dra <= -180.0) dra += 360.0;
    if (dra > 180.0) dra -= 360.0;     // 最短角差 ∈ (−180,180]
    const double phi = dra * kRad;
    const double theta = dec_deg * kRad;
    return plane_to_pix(d, phi * kDeg, -theta * kDeg, x, y);
}

// ---------------- AIT（Aitoff，ALG §15.4 claim）-----------------------------
// 正向: D=√(1+cosθ·cos(φ/2)), X=2cosθ·sin(φ/2)/D, Y=sinθ/D (θ₀=+90° 恒等旋转)
// 逆向: A=X²/4+Y², D²=2−A, sinθ=Y·D, φ=2·atan2(X·D/2, D²−1)（Paper II 反演，
//       由 X²/4+Y²=(1−cosθ·cos(φ/2)) 恒等式封闭推导，ALG §15.4）
P3ProjectionStatus ait_pix2world(const P3ProjectionDescriptor* d, double x, double y,
                                 double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3ProjectionStatus::P3_PROJ_PARAM;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double X = xd * kRad;
    const double Y = yd * kRad;
    const double dsq = 2.0 - (X * X / 4.0 + Y * Y);
    if (!(dsq > 0.0)) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;  // AIT 椭圆域
    const double dq = std::sqrt(dsq);
    const double sin_theta = Y * dq;
    if (std::fabs(sin_theta) > 1.0) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;
    const double theta = std::asin(sin_theta);
    const double half = std::atan2(X * dq / 2.0, dsq - 1.0);  // φ/2（dsq=1 ⇒ ±π/2 合法）
    const double phi = 2.0 * half;
    // θ₀=+90°: α=α₀+φ（wrap 归一）、δ=θ
    double ra_out = d->crval_ra_deg + phi * kDeg;
    normalize_ra(&ra_out);
    *ra_deg = ra_out;
    *dec_deg = theta * kDeg;
    return P3ProjectionStatus::P3_PROJ_OK;
}

P3ProjectionStatus ait_world2pix(const P3ProjectionDescriptor* d, double ra_deg,
                                 double dec_deg, double* x, double* y) {
    if (!d || !x || !y) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (std::fabs(dec_deg) > 90.0) return P3ProjectionStatus::P3_PROJ_PARAM;
    double dra = std::fmod(ra_deg - d->crval_ra_deg, 360.0);
    if (dra <= -180.0) dra += 360.0;
    if (dra > 180.0) dra -= 360.0;
    const double phi = dra * kRad;
    const double theta = dec_deg * kRad;
    const double dq = std::sqrt(1.0 + std::cos(theta) * std::cos(phi / 2.0));
    if (!(dq > 0.0)) return P3ProjectionStatus::P3_PROJ_HEMISPHERE;
    const double X = 2.0 * std::cos(theta) * std::sin(phi / 2.0) / dq;   // rad
    const double Y = std::sin(theta) / dq;
    return plane_to_pix(d, X * kDeg, Y * kDeg, x, y);
}

// ---------------- registry 冻结表（v1，四行，宪章 §18.1）--------------------
// max_fov_deg 为合法 FOV 声明（TAN=SCI §9a-12 alpha 冻结 20°；SIN/CAR/AIT 为
// ALG §15 claim 值——非 make 硬门，FOV 裁决属会话层合同）。
const P3ProjectionSpec kRegistry[4] = {
    {P3ProjectionId::TAN, "TAN", "RA---TAN", "DEC--TAN", 85.0, 20.0,
     &tan_pix2world, &tan_world2pix},
    {P3ProjectionId::SIN, "SIN", "RA---SIN", "DEC--SIN", 85.0, 60.0,
     &sin_pix2world, &sin_world2pix},
    {P3ProjectionId::CAR, "CAR", "RA---CAR", "DEC--CAR", 85.0, 180.0,
     &car_pix2world, &car_world2pix},
    {P3ProjectionId::AIT, "AIT", "RA---AIT", "DEC--AIT", 85.0, 360.0,
     &ait_pix2world, &ait_world2pix},
};

}  // namespace

const P3ProjectionSpec* p3_projection_registry_table(int* count) {
    if (count) *count = 4;
    return kRegistry;
}

const P3ProjectionSpec* p3_projection_registry_find(const char* code) {
    if (!code) return nullptr;
    for (const auto& spec : kRegistry) {
        if (std::strcmp(spec.code, code) == 0) return &spec;
    }
    return nullptr;   // 未注册 → 显式 nullptr（fail-closed，无 fallback）
}

const P3ProjectionSpec* p3_projection_registry_find_id(P3ProjectionId id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= 4) return nullptr;
    return &kRegistry[i];
}

int p3_projection_registry_selfcheck() {
    if (kP3ProjectionRegistryVersion != 1) return 1;
    for (int i = 0; i < 4; ++i) {
        if (!kRegistry[i].code || !kRegistry[i].ctype1 || !kRegistry[i].ctype2)
            return i + 1;
        if (kRegistry[i].code[0] == '\0') return i + 1;
        if (!kRegistry[i].pix2world || !kRegistry[i].world2pix) return i + 1;
        if (static_cast<int>(kRegistry[i].id) != i) return i + 1;
        if (!(kRegistry[i].max_abs_crval_dec_deg > 0.0) ||
            !(kRegistry[i].max_fov_deg > 0.0)) return i + 1;
        for (int j = 0; j < i; ++j) {
            if (std::strcmp(kRegistry[i].code, kRegistry[j].code) == 0) return i + 1;
        }
    }
    return 0;
}

P3ProjectionStatus p3_projection_make(P3ProjectionId id,
                                      double centre_ra_deg, double centre_dec_deg,
                                      double scale_deg_per_px,
                                      int width_px, int height_px,
                                      const char* parity, double rotation_pa_deg,
                                      P3ProjectionDescriptor* out) {
    if (!out) return P3ProjectionStatus::P3_PROJ_PARAM;
    *out = P3ProjectionDescriptor{};
    const P3ProjectionSpec* spec = p3_projection_registry_find_id(id);
    if (!spec) return P3ProjectionStatus::P3_PROJ_UNSUPPORTED;
    if (parity == nullptr) parity = "east_left";
    if (std::strcmp(parity, "east_left") != 0 &&
        std::strcmp(parity, "east_right") != 0) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (std::fabs(centre_dec_deg) > kMaxAbsDec) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (!(scale_deg_per_px > 0.0)) return P3ProjectionStatus::P3_PROJ_PARAM;
    if (width_px < 1 || width_px > kMaxSide || height_px < 1 || height_px > kMaxSide)
        return P3ProjectionStatus::P3_PROJ_PARAM;

    out->crval_ra_deg = centre_ra_deg;
    out->crval_dec_deg = centre_dec_deg;
    out->crpix_x = (width_px + 1) / 2.0;
    out->crpix_y = (height_px + 1) / 2.0;
    out->width_px = width_px;
    out->height_px = height_px;
    out->projection = id;
    g1_build_cd(scale_deg_per_px, parity, rotation_pa_deg, out->cd);

    // 四角投影域守卫（与 TAN 冻结语义同构：任一角越域 → 整体失败不产半成品）
    const double corners[4][2] = {{0, 0}, {double(width_px - 1), 0},
                                  {0, double(height_px - 1)},
                                  {double(width_px - 1), double(height_px - 1)}};
    for (const auto& c : corners) {
        double ra, dec;
        const P3ProjectionStatus st = spec->pix2world(out, c[0], c[1], &ra, &dec);
        if (st != P3ProjectionStatus::P3_PROJ_OK) return st;
    }
    return P3ProjectionStatus::P3_PROJ_OK;
}

P3ProjectionStatus p3_projection_pix2world(const P3ProjectionDescriptor* d,
                                           double x, double y,
                                           double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3ProjectionStatus::P3_PROJ_PARAM;
    const P3ProjectionSpec* spec = p3_projection_registry_find_id(d->projection);
    if (!spec) return P3ProjectionStatus::P3_PROJ_UNSUPPORTED;
    return spec->pix2world(d, x, y, ra_deg, dec_deg);
}

P3ProjectionStatus p3_projection_world2pix(const P3ProjectionDescriptor* d,
                                           double ra_deg, double dec_deg,
                                           double* x, double* y) {
    if (!d || !x || !y) return P3ProjectionStatus::P3_PROJ_PARAM;
    const P3ProjectionSpec* spec = p3_projection_registry_find_id(d->projection);
    if (!spec) return P3ProjectionStatus::P3_PROJ_UNSUPPORTED;
    return spec->world2pix(d, ra_deg, dec_deg, x, y);
}

std::string p3_projection_fits_keywords(const P3ProjectionDescriptor* d) {
    if (!d) return {};
    const P3ProjectionSpec* spec = p3_projection_registry_find_id(d->projection);
    if (!spec) return {};   // 未注册 id → 空关键词面（fail-closed 不产 CTYPE）
    char buf[128];
    std::string out;
    auto add = [&](const std::string& line) { out += line + "\n"; };
    char c1[64], c2[64];
    std::snprintf(c1, sizeof(c1), "CTYPE1= '%s'", spec->ctype1);
    std::snprintf(c2, sizeof(c2), "CTYPE2= '%s'", spec->ctype2);
    add(c1);
    add(c2);
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

}  // namespace astrocs::phase3proj
