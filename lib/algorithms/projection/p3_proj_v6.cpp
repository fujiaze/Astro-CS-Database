// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:       V6 Phase3 投影内核层（registry v3：TAN/SIN/CAR/AIT 四内核 + 逐像素立体角 Ω'
//             + R/S 行/列归一二元语义 + 计划/奇点/wrap）。合同锚 = p3_proj_v6.h 头注。
// WHY-KEPT:   ① eng/contracts/schemas/projection_registry.schema.json:5,9 把本文件登记为
//             projection registry 的 implementation 导出锚（eng/contracts/** 不属本任务文件域）；
//             ② 任务书 CLEAN-401 步骤 7 明令「v6 SIN 内核容差问题转 FIX-406，不在本任务删」，
//             且 FIX-406 正在本文件上做往返 Oracle 复核（run/FIX-406/ 实时在写）；
//             ③ 本内核是 p3_projection_registry.h 冻结表 SIN/CAR/AIT = kKernelOnly 的
//             「实现未声明 = 隐藏能力（禁止）」判据的对照面。
// STATUS:     内核-only，非产品声明，未接入生产。生产 projection target
//             astrocs_p3_projection_wcs 只编 p3_wcs.cpp（TAN，lib/algorithms/projection/
//             CMakeLists.txt:19）；本文件仅被 eng/tests/unit/v6_p3_proj、eng/tests/integration/v6_p3、
//             lib/algorithms/projection/tests/p3wcs 三个测试 target 编译。
//             **已知缺陷（FIX-406 交接）**：p3_proj_v6.cpp:195 sin_world2pix 用
//             ctheta = sqrt(1 - stheta^2)，投影中心 theta→pi/2 时灾难性消去 ⇒ 往返误差
//             ∝ 1/离轴距离 × 1/像素角尺度、无上界：0.5"/px 实测 2.5e-5 px、0.05"/px 1.0e-4 px、
//             精确参考像素最坏 6.1e-3 px（0.5"/px）/ 1.7e-2 px（0.05"/px）；
//             独立 Oracle（astropy 7.0.1 / WCSLIB 8.4）与独立切基式同 WCS 可达 ~2.5e-10 px
//             ⇒ 属实现条件数缺陷，非双精度固有极限。证据 run/FIX-406/SIN_ROUNDTRIP_ORACLE.md。
//             复现门（修好即转红）：eng/tests/unit/v6_p3_proj/sin_roundtrip_gate.py
//             —— 偏差仍复现 = rc 0（绿）；内核被修好 = rc 1（红），强制同步本登记块与
//             p3_projection_registry.h 的已知偏差登记。
// EXIT:       ① FIX-406 裁决「修好」⇒ 本块改为在役说明、删除已知缺陷段、把 SIN 行从
//             kKernelOnly 提升为产品声明（需独立往返 Oracle + 适用域声明）；
//             ② FIX-406 裁决「退役」⇒ 随 v6 家族整体删除，删除需同批改
//             eng/contracts/schemas/projection_registry.schema.json:5,9、
//             lib/algorithms/projection/tests/p3wcs/CMakeLists.txt:16-19 与
//             p3_projection_registry_test.cpp、eng/tests/unit/v6_p3_proj/**、
//             eng/tests/integration/v6_p3/**，以及 eng/ci/checks.json 的 v6_p3_proj_* ctest_targets
//             （DOC-403 文件域）。
// AUTHORITY:  ENGINEERING_SPEC.md §2（历史实现处置：保留则注释）；ASTROCS_DESIGN.md §6.3
//             （未实现的投影被选择时显式报「不支持」，当前仅 TAN 可用）；
//             lib/algorithms/projection/p3_projection_registry.h「已知偏差登记（FIX-406）」；
//             RELEASE-04 GAP_AUDIT G3-6 / G2-1（包已出库）。
// ──────────────────────────────────────────────────────────────────────
// lib/algorithms/projection/p3_proj_v6.cpp — V6 Phase3 投影实现层（标准 FITS WCS Paper II）
// 任务 IMPL-P3-PROJ-001；合同锚见 p3_proj_v6.h 头注。
// registry v3（SCI-FIX-PROJ 2026-09-16）：CAR/AIT 按 Paper II §2.2 三 Euler 角把 CRVAL2
// （含 LONPOLE 标准默认）纳入映射；AIT 域界 A≤1；CAR native 极行 |θ|≥90° fail-closed。
#include "p3_proj_v6.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace astrocs::phase3proj::v6 {
namespace {

constexpr double kDeg = 180.0 / M_PI;
constexpr double kRad = M_PI / 180.0;
constexpr double kMaxAbsDec = 85.0;
constexpr double kHalfPi = M_PI / 2.0;
#ifndef ASTROCS_P3_MAX_SIDE
constexpr int kMaxSide = 20000;
#else
constexpr int kMaxSide = ASTROCS_P3_MAX_SIDE;
#endif
constexpr double kSqrt2 = 1.41421356237309504880168872420969808;

void normalize_ra(double* ra) {
    *ra = std::fmod(*ra, 360.0);
    if (*ra < 0) *ra += 360.0;
}

// G1 CD 构造（east_left: diag(−s,+s)·R；east_right: diag(+s,−s)·R；PA 推广）
void g1_build_cd(double scale_deg_per_px, const char* parity,
                 double rotation_pa_deg, double cd[2][2]) {
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

void pix_to_plane(const Descriptor* d, double x, double y, double* xd, double* yd) {
    const double dx = (x + 1.0) - d->crpix_x;   // FITS 1-based
    const double dy = (y + 1.0) - d->crpix_y;
    *xd = d->cd[0][0] * dx + d->cd[0][1] * dy;  // deg
    *yd = d->cd[1][0] * dx + d->cd[1][1] * dy;  // deg
}

ProjStatus plane_to_pix(const Descriptor* d, double xd, double yd,
                        double* x, double* y) {
    const double det = d->cd[0][0] * d->cd[1][1] - d->cd[0][1] * d->cd[1][0];
    if (std::fabs(det) < 1e-300) return ProjStatus::kParam;
    const double dx = (d->cd[1][1] * xd - d->cd[0][1] * yd) / det;
    const double dy = (-d->cd[1][0] * xd + d->cd[0][0] * yd) / det;
    *x = dx + d->crpix_x - 1.0;
    *y = dy + d->crpix_y - 1.0;
    return ProjStatus::kOk;
}

// zenithal 旋转核（TAN/SIN，θ0=CRVAL2；与 legacy 冻结逐式一致，astropy 一致）
void native_to_sky_zenithal(const Descriptor* d, double phi, double theta,
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

ProjStatus sky_to_plane_zenithal(const Descriptor* d, double ra_deg, double dec_deg,
                                 double* xi_rad, double* eta_rad) {
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double a = ra_deg * kRad;
    const double dd = dec_deg * kRad;
    const double denom = std::sin(d0) * std::sin(dd) +
                         std::cos(d0) * std::cos(dd) * std::cos(a - a0);
    if (denom <= 0.0) return ProjStatus::kHemisphere;
    *xi_rad = std::cos(dd) * std::sin(a - a0) / denom;
    *eta_rad = (std::sin(dd) * std::cos(d0) -
                std::cos(dd) * std::sin(d0) * std::cos(a - a0)) / denom;
    return ProjStatus::kOk;
}

// ---- Paper II §2.2 通用旋转: 由 CRVAL 与投影参考点 (phi0,theta0) 解三 Euler 角 ----
// 约束: sin(d0) = sin(th0) sin(dp) + cos(th0) cos(dp) cos(phi0 - phip)
//       ap = a0 - atan2(-cos(th0) sin(phi0-phip), sin(th0) cos(dp) - cos(th0) sin(dp) cos(phi0-phip))
// CAR/AIT 参考点 (phi0,theta0) = (0,0)；LONPOLE 标准默认 = 0 (d0>=th0) 否则 180。
// 逆向: th = asin(sin(d) sin(dp) + cos(d) cos(dp) cos(a-ap))
//       ph = phip + atan2(-cos(d) sin(a-ap), sin(d) cos(dp) - cos(d) sin(dp) cos(a-ap))
struct Tilt { double ra_p, dec_p, phi_p; };

Tilt tilt_params(double ra0_deg, double dec0_deg) {
    const double d0 = dec0_deg * kRad;
    const double phip = (dec0_deg >= 0.0) ? 0.0 : M_PI;
    double cos_dp = std::sin(d0) / std::cos(phip);          // th0=0, phi0=0
    if (cos_dp > 1.0) cos_dp = 1.0;
    if (cos_dp < -1.0) cos_dp = -1.0;
    const double dp = std::acos(cos_dp);                    // |dp| <= 90
    const double ap = ra0_deg * kRad -
        std::atan2(std::sin(phip), -std::sin(dp) * std::cos(phip));
    Tilt t; t.ra_p = ap; t.dec_p = dp; t.phi_p = phip; return t;
}

void native_to_sky_general(const Tilt& t, double phi, double theta,
                           double* ra_deg, double* dec_deg) {
    const double st = std::sin(theta), ct = std::cos(theta);
    const double dps = std::sin(t.dec_p), dpc = std::cos(t.dec_p);
    const double cp = std::cos(phi - t.phi_p), sp = std::sin(phi - t.phi_p);
    const double dec = std::asin(st * dps + ct * dpc * cp);
    const double dra = std::atan2(-ct * sp, st * dpc - ct * dps * cp);
    double ra_out = (t.ra_p + dra) * kDeg;
    normalize_ra(&ra_out);
    *ra_deg = ra_out; *dec_deg = dec * kDeg;
}

void sky_to_native_general(const Tilt& t, double ra_deg, double dec_deg,
                           double* phi, double* theta) {
    const double a = ra_deg * kRad, dd = dec_deg * kRad;
    const double dps = std::sin(t.dec_p), dpc = std::cos(t.dec_p);
    const double da = a - t.ra_p;
    double sth = std::sin(dd) * dps + std::cos(dd) * dpc * std::cos(da);
    if (sth > 1.0) sth = 1.0;
    if (sth < -1.0) sth = -1.0;
    *theta = std::asin(sth);
    double ph = t.phi_p + std::atan2(-std::cos(dd) * std::sin(da),
                                     std::sin(dd) * dpc - std::cos(dd) * dps * std::cos(da));
    ph = std::fmod(ph + M_PI, 2.0 * M_PI);        // 归一 (-pi, pi]
    if (ph <= 0.0) ph += 2.0 * M_PI;
    *phi = ph - M_PI;
}

// ---------------- TAN ----------------
ProjStatus tan_pix2world(const Descriptor* d, double x, double y,
                         double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return ProjStatus::kParam;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double xi = xd * kRad, eta = yd * kRad;
    const double r = std::sqrt(xi * xi + eta * eta);
    if (r >= kHalfPi) return ProjStatus::kHemisphere;
    const double theta = std::atan2(1.0, r);
    const double phi = std::atan2(-xi, eta);
    native_to_sky_zenithal(d, phi, theta, ra_deg, dec_deg);
    return ProjStatus::kOk;
}

ProjStatus tan_world2pix(const Descriptor* d, double ra_deg, double dec_deg,
                         double* x, double* y) {
    if (!d || !x || !y) return ProjStatus::kParam;
    if (std::fabs(dec_deg) > kMaxAbsDec) return ProjStatus::kParam;
    double xi, eta;
    const ProjStatus st = sky_to_plane_zenithal(d, ra_deg, dec_deg, &xi, &eta);
    if (st != ProjStatus::kOk) return st;
    return plane_to_pix(d, xi * kDeg, eta * kDeg, x, y);
}

// ---------------- SIN ----------------
ProjStatus sin_pix2world(const Descriptor* d, double x, double y,
                         double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return ProjStatus::kParam;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double xi = xd * kRad, eta = yd * kRad;
    const double rho = std::sqrt(xi * xi + eta * eta);
    if (rho > 1.0) return ProjStatus::kHemisphere;
    const double theta = std::acos(rho);
    const double phi = std::atan2(-xi, eta);
    native_to_sky_zenithal(d, phi, theta, ra_deg, dec_deg);
    return ProjStatus::kOk;
}

ProjStatus sin_world2pix(const Descriptor* d, double ra_deg, double dec_deg,
                         double* x, double* y) {
    if (!d || !x || !y) return ProjStatus::kParam;
    if (std::fabs(dec_deg) > kMaxAbsDec) return ProjStatus::kParam;
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double a = ra_deg * kRad;
    const double dd = dec_deg * kRad;
    const double stheta = std::sin(d0) * std::sin(dd) +
                          std::cos(d0) * std::cos(dd) * std::cos(a - a0);
    if (stheta <= 0.0) return ProjStatus::kHemisphere;
    const double ctheta = std::sqrt(std::max(0.0, 1.0 - stheta * stheta));
    const double phi = std::atan2(-std::cos(dd) * std::sin(a - a0),
                                  std::sin(dd) * std::cos(d0) -
                                      std::cos(dd) * std::sin(d0) * std::cos(a - a0));
    const double xi = -ctheta * std::sin(phi);
    const double eta = ctheta * std::cos(phi);
    return plane_to_pix(d, xi * kDeg, eta * kDeg, x, y);
}

// ---------------- CAR（标准 Paper II: X=φ, Y=θ; 参考点 (0,0) + CRVAL2 旋转）------
ProjStatus car_pix2world(const Descriptor* d, double x, double y,
                         double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return ProjStatus::kParam;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double phi_deg = xd;          // φ = X（deg）
    const double theta_deg = yd;        // θ = Y（deg），native 纬度
    // native 极行 θ=±90° 整行塌缩（Ω=0、RA 无定义）⇒ fail-closed（含 =90）
    if (std::fabs(theta_deg) >= 90.0) return ProjStatus::kParam;
    const Tilt t = tilt_params(d->crval_ra_deg, d->crval_dec_deg);
    native_to_sky_general(t, phi_deg * kRad, theta_deg * kRad, ra_deg, dec_deg);
    return ProjStatus::kOk;
}

ProjStatus car_world2pix(const Descriptor* d, double ra_deg, double dec_deg,
                         double* x, double* y) {
    if (!d || !x || !y) return ProjStatus::kParam;
    if (std::fabs(dec_deg) > 90.0) return ProjStatus::kParam;
    const Tilt t = tilt_params(d->crval_ra_deg, d->crval_dec_deg);
    double phi, theta;
    sky_to_native_general(t, ra_deg, dec_deg, &phi, &theta);
    // native 极点上逆映射 φ 不唯一（整行塌缩的同一点）⇒ fail-closed
    if (std::fabs(theta) >= kHalfPi) return ProjStatus::kParam;
    return plane_to_pix(d, phi * kDeg, theta * kDeg, x, y);
}

// -------- AIT（标准 Paper II: γ=√2/D, X=2γ cosθ sin(φ/2), Y=γ sinθ；CRVAL2 旋转）--
ProjStatus ait_pix2world(const Descriptor* d, double x, double y,
                         double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return ProjStatus::kParam;
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    const double xrad = xd * kRad, yrad = yd * kRad;
    // 反演: 令 X'=X/√2, Y'=Y/√2 ⇒ X'=2cosθ sin(φ/2)/D, Y'=sinθ/D
    const double xp = xrad / kSqrt2, yp = yrad / kSqrt2;
    // Paper II AIT 椭圆域: A = xp²/4 + yp² ≤ 1（A=1 即 φ=±180° 边界合法）
    const double a_ell = xp * xp / 4.0 + yp * yp;
    if (a_ell > 1.0) return ProjStatus::kHemisphere;
    const double dsq = 2.0 - a_ell;
    const double dq = std::sqrt(dsq);
    const double sin_theta = yp * dq;
    if (std::fabs(sin_theta) > 1.0) return ProjStatus::kHemisphere;
    const double theta = std::asin(sin_theta);
    const double half = std::atan2(xp * dq / 2.0, dsq - 1.0);
    const double phi = 2.0 * half;
    const Tilt t = tilt_params(d->crval_ra_deg, d->crval_dec_deg);
    native_to_sky_general(t, phi, theta, ra_deg, dec_deg);
    return ProjStatus::kOk;
}

ProjStatus ait_world2pix(const Descriptor* d, double ra_deg, double dec_deg,
                         double* x, double* y) {
    if (!d || !x || !y) return ProjStatus::kParam;
    if (std::fabs(dec_deg) > 90.0) return ProjStatus::kParam;
    const Tilt t = tilt_params(d->crval_ra_deg, d->crval_dec_deg);
    double phi, theta;
    sky_to_native_general(t, ra_deg, dec_deg, &phi, &theta);
    const double dq = std::sqrt(1.0 + std::cos(theta) * std::cos(phi / 2.0));
    if (!(dq > 0.0)) return ProjStatus::kHemisphere;
    const double gamma = kSqrt2 / dq;
    const double X = 2.0 * gamma * std::cos(theta) * std::sin(phi / 2.0);
    const double Y = gamma * std::sin(theta);
    return plane_to_pix(d, X * kDeg, Y * kDeg, x, y);
}

// DESIGN §5.3 八投影冻结集中已实现的 4 项（TAN/SIN/CAR/AIT）；STG/MOL/CEA/ZEA
// 归 P3-001（GAP-011）实施后再入表，registry_selfcheck 保证表内 code 恒属于冻结集。
const Spec kRegistry[4] = {
    {ProjectionId::kTAN, "TAN", "RA---TAN", "DEC--TAN", 85.0, 20.0,
     "tan_antipode_r_ge_halfpi", &tan_pix2world, &tan_world2pix},
    {ProjectionId::kSIN, "SIN", "RA---SIN", "DEC--SIN", 85.0, 60.0,
     "sin_limb_rho_gt_one", &sin_pix2world, &sin_world2pix},
    {ProjectionId::kCAR, "CAR", "RA---CAR", "DEC--CAR", 85.0, 180.0,
     "car_native_pole_row_abs_theta_ge_90", &car_pix2world, &car_world2pix},
    {ProjectionId::kAIT, "AIT", "RA---AIT", "DEC--AIT", 85.0, 360.0,
     "ait_ellipse_a_gt_one", &ait_pix2world, &ait_world2pix},
};

// ---------------- Ω 内部辅助 ----------------
struct Vec3 { double x, y, z; };

Vec3 sky_vec(double ra_deg, double dec_deg) {
    const double a = ra_deg * kRad, d = dec_deg * kRad;
    return {std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
}
double vdot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 vcross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double vnorm(const Vec3& a) { return std::sqrt(vdot(a, a)); }

// Van Oosterom & Strackee 球面三角形有向面积
double tri_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    const double num = std::fabs(vdot(a, vcross(b, c)));
    const double den = 1.0 + vdot(a, b) + vdot(b, c) + vdot(c, a);
    return 2.0 * std::atan2(num, den);
}

ProjStatus corner_vec(const Descriptor* d, double x, double y, Vec3* v) {
    double ra, dec;
    const ProjStatus st = pix2world(d, x, y, &ra, &dec);
    if (st != ProjStatus::kOk) return st;
    *v = sky_vec(ra, dec);
    return ProjStatus::kOk;
}

// 投影域 margin（>0 = 域内；单位随投影声明，仅作 bool 域判据，不作科学阈值）
double projection_margin(const Descriptor* d, double x, double y, ProjStatus* st) {
    double xd, yd;
    pix_to_plane(d, x, y, &xd, &yd);
    switch (d->id) {
        case ProjectionId::kTAN: {
            const double r = std::hypot(xd * kRad, yd * kRad);
            *st = (r >= kHalfPi) ? ProjStatus::kHemisphere : ProjStatus::kOk;
            return kHalfPi - r;
        }
        case ProjectionId::kSIN: {
            const double rho = std::hypot(xd * kRad, yd * kRad);
            *st = (rho > 1.0) ? ProjStatus::kHemisphere : ProjStatus::kOk;
            return 1.0 - rho;
        }
        case ProjectionId::kCAR: {
            const double th = yd * kRad;
            // native 极行 |θ|≥90° 整行塌缩 ⇒ margin 0 即域外（fail-closed）
            *st = (std::fabs(yd) >= 90.0) ? ProjStatus::kParam : ProjStatus::kOk;
            return kHalfPi - std::fabs(th);
        }
        default: {   // AIT: 椭圆域 A = xp²/4 + yp² ≤ 1
            const double xp = xd * kRad / kSqrt2, yp = yd * kRad / kSqrt2;
            const double a_ell = xp * xp / 4.0 + yp * yp;
            const double dsq = 2.0 - a_ell;
            const double sth = yp * std::sqrt(std::max(0.0, dsq));
            *st = (a_ell > 1.0 || std::fabs(sth) > 1.0) ? ProjStatus::kHemisphere
                                                        : ProjStatus::kOk;
            return 1.0 - a_ell;
        }
    }
}

}  // namespace

const char* const kFrozenSet[kFrozenProjectionCount] = {
    "TAN", "SIN", "CAR", "AIT", "STG", "MOL", "CEA", "ZEA"};

const char* const* registry_frozen_set(int* count) {
    if (count) *count = kFrozenProjectionCount;
    return kFrozenSet;
}

bool registry_is_frozen_code(const char* code) {
    if (!code) return false;
    for (int i = 0; i < kFrozenProjectionCount; ++i) {
        if (std::strcmp(code, kFrozenSet[i]) == 0) return true;
    }
    return false;
}

const Spec* registry_table(int* count) {
    if (count) *count = 4;
    return kRegistry;
}

const Spec* registry_find(const char* code) {
    if (!code) return nullptr;
    for (const auto& spec : kRegistry) {
        if (std::strcmp(spec.code, code) == 0) return &spec;
    }
    return nullptr;
}

const Spec* registry_find_id(ProjectionId id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= 4) return nullptr;
    return &kRegistry[i];
}

int registry_selfcheck() {
    if (kProjectionRegistryVersion != 3) return 1;
    if (kFrozenProjectionCount != 8) return 1;
    for (int i = 0; i < 4; ++i) {
        if (!kRegistry[i].code || !kRegistry[i].ctype1 || !kRegistry[i].ctype2)
            return i + 1;
        // 表内投影必须属于 DESIGN §5.3 冻结集合（防未注册投影混入）
        if (!registry_is_frozen_code(kRegistry[i].code)) return i + 1;
        if (!kRegistry[i].singularity_kind || kRegistry[i].code[0] == '\0')
            return i + 1;
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

ProjStatus make(ProjectionId id, double centre_ra_deg, double centre_dec_deg,
                double scale_deg_per_px, int width_px, int height_px,
                const char* parity, double rotation_pa_deg, Descriptor* out) {
    if (!out) return ProjStatus::kParam;
    *out = Descriptor{};
    const Spec* spec = registry_find_id(id);
    if (!spec) return ProjStatus::kUnsupported;
    if (parity == nullptr) parity = "east_left";
    if (std::strcmp(parity, "east_left") != 0 &&
        std::strcmp(parity, "east_right") != 0) return ProjStatus::kParam;
    if (!std::isfinite(centre_ra_deg) || !std::isfinite(centre_dec_deg) ||
        !std::isfinite(scale_deg_per_px) || !std::isfinite(rotation_pa_deg))
        return ProjStatus::kParam;
    if (std::fabs(centre_dec_deg) > kMaxAbsDec) return ProjStatus::kParam;
    if (!(scale_deg_per_px > 0.0)) return ProjStatus::kParam;
    if (width_px < 1 || width_px > kMaxSide || height_px < 1 || height_px > kMaxSide)
        return ProjStatus::kParam;

    out->id = id;
    out->crval_ra_deg = centre_ra_deg;
    out->crval_dec_deg = centre_dec_deg;
    out->crpix_x = (width_px + 1) / 2.0;
    out->crpix_y = (height_px + 1) / 2.0;
    out->width_px = width_px;
    out->height_px = height_px;
    g1_build_cd(scale_deg_per_px, parity, rotation_pa_deg, out->cd);

    const double corners[4][2] = {{0, 0},
                                  {double(width_px - 1), 0},
                                  {0, double(height_px - 1)},
                                  {double(width_px - 1), double(height_px - 1)}};
    for (const auto& c : corners) {
        double ra, dec;
        const ProjStatus st = spec->pix2world(out, c[0], c[1], &ra, &dec);
        if (st != ProjStatus::kOk) return st;
    }
    return ProjStatus::kOk;
}

ProjStatus pix2world(const Descriptor* d, double x, double y,
                     double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return ProjStatus::kParam;
    if (!std::isfinite(x) || !std::isfinite(y)) return ProjStatus::kParam;
    const Spec* spec = registry_find_id(d->id);
    if (!spec) return ProjStatus::kUnsupported;
    return spec->pix2world(d, x, y, ra_deg, dec_deg);
}

ProjStatus world2pix(const Descriptor* d, double ra_deg, double dec_deg,
                     double* x, double* y) {
    if (!d || !x || !y) return ProjStatus::kParam;
    if (!std::isfinite(ra_deg) || !std::isfinite(dec_deg)) return ProjStatus::kParam;
    const Spec* spec = registry_find_id(d->id);
    if (!spec) return ProjStatus::kUnsupported;
    return spec->world2pix(d, ra_deg, dec_deg, x, y);
}

std::string fits_keywords(const Descriptor* d) {
    if (!d) return {};
    const Spec* spec = registry_find_id(d->id);
    if (!spec) return {};
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

// ---------------- 逐像素 Ω ----------------
ProjStatus pixel_solid_angle(const Descriptor* d, double x, double y,
                             double* omega_sr) {
    if (!d || !omega_sr) return ProjStatus::kParam;
    Vec3 v00, v10, v11, v01;
    ProjStatus st = corner_vec(d, x - 0.5, y - 0.5, &v00);
    if (st != ProjStatus::kOk) return st;
    st = corner_vec(d, x + 0.5, y - 0.5, &v10);
    if (st != ProjStatus::kOk) return st;
    st = corner_vec(d, x + 0.5, y + 0.5, &v11);
    if (st != ProjStatus::kOk) return st;
    st = corner_vec(d, x - 0.5, y + 0.5, &v01);
    if (st != ProjStatus::kOk) return st;
    const double area = tri_area(v00, v10, v11) + tri_area(v00, v11, v01);
    if (!std::isfinite(area) || area <= 0.0) return ProjStatus::kParam;
    *omega_sr = area;
    return ProjStatus::kOk;
}

ProjStatus pixel_solid_angle_differential(const Descriptor* d, double x, double y,
                                          double* omega_sr) {
    if (!d || !omega_sr) return ProjStatus::kParam;
    const double h = 1e-3;   // px
    Vec3 xp, xm, yp, ym;
    ProjStatus st = corner_vec(d, x + h, y, &xp);
    if (st != ProjStatus::kOk) return st;
    st = corner_vec(d, x - h, y, &xm);
    if (st != ProjStatus::kOk) return st;
    st = corner_vec(d, x, y + h, &yp);
    if (st != ProjStatus::kOk) return st;
    st = corner_vec(d, x, y - h, &ym);
    if (st != ProjStatus::kOk) return st;
    const Vec3 dxv{xp.x - xm.x, xp.y - xm.y, xp.z - xm.z};
    const Vec3 dyv{yp.x - ym.x, yp.y - ym.y, yp.z - ym.z};
    const double area = vnorm(vcross(dxv, dyv)) / (4.0 * h * h);
    if (!std::isfinite(area) || area <= 0.0) return ProjStatus::kParam;
    *omega_sr = area;
    return ProjStatus::kOk;
}

ProjStatus solid_angle_grid(const Descriptor* d, double* omega_out,
                            ProjStatus* status_out) {
    if (!d || !omega_out) return ProjStatus::kParam;
    if (d->width_px < 1 || d->height_px < 1) return ProjStatus::kParam;
    ProjStatus first_bad = ProjStatus::kOk;
    for (int j = 0; j < d->height_px; ++j) {
        for (int i = 0; i < d->width_px; ++i) {
            double om = 0;
            const ProjStatus st = pixel_solid_angle(d, double(i), double(j), &om);
            const size_t k = static_cast<size_t>(j) * d->width_px + i;
            if (st == ProjStatus::kOk) {
                omega_out[k] = om;
            } else {
                omega_out[k] = std::numeric_limits<double>::quiet_NaN();
                if (first_bad == ProjStatus::kOk) first_bad = st;
            }
            if (status_out) status_out[k] = st;
        }
    }
    return first_bad;
}

// ---------------- 计划 ----------------
ProjStatus plan(ProjectionId id, double centre_ra_deg, double centre_dec_deg,
                double scale_deg_per_px, int width_px, int height_px,
                const char* parity, double rotation_pa_deg, Plan* out) {
    if (!out) return ProjStatus::kParam;
    *out = Plan{};
    const ProjStatus mk = make(id, centre_ra_deg, centre_dec_deg, scale_deg_per_px,
                               width_px, height_px, parity, rotation_pa_deg,
                               &out->descriptor);
    out->status = mk;
    if (mk != ProjStatus::kOk) return mk;

    const int nx = std::min(width_px, 9);
    const int ny = std::min(height_px, 7);
    double ra_samples[64];
    int nra = 0;
    double dec_min = 90.0, dec_max = -90.0;
    double margin_min = std::numeric_limits<double>::infinity();
    double om_min = std::numeric_limits<double>::infinity();
    double om_max = 0.0;
    int omega_n = 0;
    bool all_ok = true;
    ProjStatus first_bad = ProjStatus::kOk;
    for (int jj = 0; jj < ny; ++jj) {
        const double y = (height_px - 1) * (ny == 1 ? 0.0 : double(jj) / (ny - 1));
        for (int ii = 0; ii < nx; ++ii) {
            const double x = (width_px - 1) * (nx == 1 ? 0.0 : double(ii) / (nx - 1));
            double ra, dec;
            const ProjStatus st = pix2world(&out->descriptor, x, y, &ra, &dec);
            ProjStatus mst = ProjStatus::kOk;
            const double margin = projection_margin(&out->descriptor, x, y, &mst);
            if (nra < 64) ra_samples[nra++] = ra;
            if (st == ProjStatus::kOk) {
                dec_min = std::min(dec_min, dec);
                dec_max = std::max(dec_max, dec);
            } else if (all_ok) {
                all_ok = false;
                first_bad = st;
            }
            if (margin < margin_min) margin_min = margin;
            double om = 0;
            const ProjStatus ost = pixel_solid_angle(&out->descriptor, x, y, &om);
            if (ost == ProjStatus::kOk) {
                om_min = std::min(om_min, om);
                om_max = std::max(om_max, om);
                ++omega_n;
            }
        }
    }
    // 圆形 span（RA wrap）
    std::sort(ra_samples, ra_samples + nra);
    double raw_span = ra_samples[nra - 1] - ra_samples[0];
    double max_gap = ra_samples[0] + 360.0 - ra_samples[nra - 1];
    for (int i = 1; i < nra; ++i)
        max_gap = std::max(max_gap, ra_samples[i] - ra_samples[i - 1]);
    const double circ_span = 360.0 - max_gap;

    out->dec_min_deg = dec_min;
    out->dec_max_deg = dec_max;
    out->pole_margin_deg = 90.0 - std::max(std::fabs(dec_min), std::fabs(dec_max));
    out->fov_x_deg = circ_span;
    out->fov_y_deg = dec_max - dec_min;
    out->crosses_ra_wrap = (raw_span > 180.0) && (circ_span < raw_span - 1e-9);
    out->domain_valid = all_ok && (margin_min > 0.0);
    out->singularity_free = out->domain_valid;
    out->min_domain_margin = margin_min;
    out->n_omega_samples = omega_n;
    if (omega_n > 0) {
        out->omega_min_sr = om_min;
        out->omega_max_sr = om_max;
        out->omega_max_min_ratio = om_min > 0 ? om_max / om_min : 0.0;
    }
    if (!all_ok) out->status = first_bad;
    return out->status;
}

// ---------------- R/S 行/列归一 ----------------
ProjStatus semantics_required_normalization(SampleSemantics s, bool* row_required,
                                            bool* col_required) {
    if (!row_required || !col_required) return ProjStatus::kParam;
    *row_required = false;
    *col_required = false;
    switch (s) {
        case SampleSemantics::kSurfaceBrightnessRowNorm:
            *row_required = true;
            return ProjStatus::kOk;
        case SampleSemantics::kPointSourceFluxColNorm:
            *col_required = true;
            return ProjStatus::kOk;
        case SampleSemantics::kVisualizationNone:
            return ProjStatus::kOk;
        default:
            return ProjStatus::kParam;
    }
}

bool semantics_compatible(SampleSemantics s, bool row_normalized,
                          bool col_normalized) {
    bool rr = false, cr = false;
    if (semantics_required_normalization(s, &rr, &cr) != ProjStatus::kOk)
        return false;
    return row_normalized == rr && col_normalized == cr;
}

namespace {
bool omega_ok(const double* w, int n) {
    if (!w) return false;
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(w[i]) || !(w[i] > 0.0)) return false;
    }
    return true;
}
void zero_fill(double* p, size_t n) {
    for (size_t i = 0; i < n; ++i) p[i] = 0.0;
}
}  // namespace

ProjStatus row_normalise(const double* a_overlap_sr, int m, int n,
                         const double* omega_out_sr, double* r_out) {
    if (!a_overlap_sr || !r_out || m <= 0 || n <= 0) return ProjStatus::kParam;
    zero_fill(r_out, static_cast<size_t>(m) * n);
    if (!omega_ok(omega_out_sr, m)) return ProjStatus::kParam;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            const double a = a_overlap_sr[static_cast<size_t>(i) * n + j];
            if (!std::isfinite(a) || a < 0.0) return ProjStatus::kParam;
            r_out[static_cast<size_t>(i) * n + j] = a / omega_out_sr[i];
        }
    }
    return ProjStatus::kOk;
}

ProjStatus col_normalise(const double* a_overlap_sr, int m, int n,
                         const double* omega_in_sr, double* s_out) {
    if (!a_overlap_sr || !s_out || m <= 0 || n <= 0) return ProjStatus::kParam;
    zero_fill(s_out, static_cast<size_t>(m) * n);
    if (!omega_ok(omega_in_sr, n)) return ProjStatus::kParam;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            const double a = a_overlap_sr[static_cast<size_t>(i) * n + j];
            if (!std::isfinite(a) || a < 0.0) return ProjStatus::kParam;
            s_out[static_cast<size_t>(i) * n + j] = a / omega_in_sr[j];
        }
    }
    return ProjStatus::kOk;
}

ProjStatus row_to_col(const double* r, int m, int n, const double* omega_out_sr,
                      const double* omega_in_sr, double* s_out) {
    if (!r || !s_out || m <= 0 || n <= 0) return ProjStatus::kParam;
    zero_fill(s_out, static_cast<size_t>(m) * n);
    if (!omega_ok(omega_out_sr, m) || !omega_ok(omega_in_sr, n))
        return ProjStatus::kParam;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            const double rij = r[static_cast<size_t>(i) * n + j];
            if (!std::isfinite(rij) || rij < 0.0) return ProjStatus::kParam;
            s_out[static_cast<size_t>(i) * n + j] =
                rij * omega_out_sr[i] / omega_in_sr[j];
        }
    }
    return ProjStatus::kOk;
}

ProjStatus matrix_row_sums(const double* r, int m, int n, double* sums) {
    if (!r || !sums || m <= 0 || n <= 0) return ProjStatus::kParam;
    for (int i = 0; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += r[static_cast<size_t>(i) * n + j];
        sums[i] = s;
    }
    return ProjStatus::kOk;
}

ProjStatus matrix_col_sums(const double* s, int m, int n, double* sums) {
    if (!s || !sums || m <= 0 || n <= 0) return ProjStatus::kParam;
    for (int j = 0; j < n; ++j) sums[j] = 0.0;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j)
            sums[j] += s[static_cast<size_t>(i) * n + j];
    }
    return ProjStatus::kOk;
}

}  // namespace astrocs::phase3proj::v6
