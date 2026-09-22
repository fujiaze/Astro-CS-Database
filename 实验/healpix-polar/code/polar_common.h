// ============================================================================
// polar_common.h — EXP-07-POLAR 共用底座
//
// 内容:
//   1. 球面向量 / TAN WCS / drop 构造
//   2. HEALPix 面坐标卡 (face chart) 的**数值稳定**正/逆映射
//      （对照 production 的 acos(z) 退化路径，用于根因定位）
//   3. 球面多边形面积 (旋转到质心为新极点的 VOS，避免 det 相消)
//   4. 独立高精度 oracle: 叶边界按**真实曲线**自适应采样 + 球面 S-H + VOS
//
// 所有真值口径与生产一致：drop 顶点为单位向量；叶 = HEALPix 面坐标卡单位方格
// 的像；面积元素 |J| = pi/3 (u,v in [0,1]) 常数（等面积）。
//
// 参考:
//   Gorski et al. 2005, ApJ 622, 759, DOI 10.1086/427976  (§4 坐标卡, §5.3 边界非大圆)
//   Calabretta & Roukema 2007, MNRAS 381, 865, DOI 10.1111/j.1365-2966.2007.12297.x
//   Van Oosterom & Strackee 1983, IEEE TBME 30(2), 125, DOI 10.1109/TBME.1983.325207
// ============================================================================
#ifndef POLAR_COMMON_H
#define POLAR_COMMON_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

namespace pp {

constexpr double kPi       = 3.14159265358979323846264338327950288;
constexpr double kTwoPi    = 2.0 * kPi;
constexpr double kHalfPi   = 0.5 * kPi;
constexpr double kTwoThird = 2.0 / 3.0;
constexpr double kRoot3    = 1.73205080756887729352744634150587237;
constexpr double kRoot6    = 2.44948974278317809819728407470589139;
constexpr double kJacobian = kPi / 3.0;              // 面坐标卡单位方格 -> 球面度
constexpr double kDeg2Rad  = kPi / 180.0;
constexpr double kRad2Deg  = 180.0 / kPi;
constexpr double kArcsec2Rad = kPi / (180.0 * 3600.0);
constexpr double kRad2Arcsec = 180.0 * 3600.0 / kPi;

// ---------------------------------------------------------------------------
// 向量
// ---------------------------------------------------------------------------
struct V3 { double x = 0, y = 0, z = 0; };
inline double dot3(const V3& a, const V3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline V3 cross3(const V3& a, const V3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline double norm3(const V3& a) { return std::sqrt(dot3(a, a)); }
inline V3 normalize3(const V3& a) {
    double n = norm3(a);
    if (!(n > 0.0)) return {0, 0, 0};
    return {a.x/n, a.y/n, a.z/n};
}
// radec(度) -> 单位向量。极点附近 dec≈±90° 时 cos(dec) 的绝对误差 ~1e-16
// ⇒ 横向分量相对误差 ~1e-16/theta（theta=离极点角距）。这是输入层的固有下限，
// 与本仓算法无关，实验里作为"输入精度地板"单列。
inline V3 radec_to_vec(double ra_deg, double dec_deg) {
    double ra = ra_deg * kDeg2Rad, dec = dec_deg * kDeg2Rad;
    double cd = std::cos(dec);
    return {cd * std::cos(ra), cd * std::sin(ra), std::sin(dec)};
}
// long double 版 (ra,dec) -> 单位向量：用于判定"误差是否来自双精度表示"
inline V3 radec_to_vec_ld(double ra_deg, double dec_deg) {
    long double ra = (long double)ra_deg * 3.14159265358979323846264338327950288L / 180.0L;
    long double dec = (long double)dec_deg * 3.14159265358979323846264338327950288L / 180.0L;
    long double cd = cosl(dec);
    return {(double)(cd * cosl(ra)), (double)(cd * sinl(ra)), (double)sinl(dec)};
}
inline void vec_to_radec(const V3& v, double& ra_deg, double& dec_deg) {
    double ra = std::atan2(v.y, v.x);
    if (ra < 0.0) ra += kTwoPi;
    ra_deg = ra * kRad2Deg;
    dec_deg = std::asin(std::max(-1.0, std::min(1.0, v.z))) * kRad2Deg;
}
// 绕轴旋转（Rodrigues），把中心 c 精确旋到 +z。
// 旋转轴必须是 k = (c x z)/|c x z|：此时 k x c = (z - c*(c.z))/s，
//   R(c) = c*cos(th) + (k x c)*sin(th) = c*(c.z) + z - c*(c.z) = z。
// 用 k = (z x c)/|z x c|（反向轴）只会把 c 旋到 2*(c.z)^2 - 1 处，
// 起不到"把多边形搬到极点附近"的稳定作用（c.z=0 时甚至搬到南极点）。
inline V3 rotate_to_z(const V3& v, const V3& c) {
    V3 axis = cross3(c, {0, 0, 1});
    double s = norm3(axis);
    double ct = c.z;
    if (s < 1e-300) return (ct > 0) ? v : V3{v.x, -v.y, -v.z};
    axis = {axis.x/s, axis.y/s, axis.z/s};
    double st = s;
    double d = dot3(axis, v);
    V3 cr = cross3(axis, v);
    return {v.x*ct + cr.x*st + axis.x*d*(1.0-ct),
            v.y*ct + cr.y*st + axis.y*d*(1.0-ct),
            v.z*ct + cr.z*st + axis.z*d*(1.0-ct)};
}

// ---------------------------------------------------------------------------
// 球面多边形面积：Van Oosterom & Strackee 1983 扇形剖分，先把多边形旋到
// 以自身质心为新极点（横向坐标 ~theta 携带全相对精度），消除 det 相消。
// ---------------------------------------------------------------------------
inline double vos_area_rotated(const std::vector<V3>& poly_in) {
    if (poly_in.size() < 3) return 0.0;
    V3 c{0, 0, 0};
    for (const auto& p : poly_in) { c.x += p.x; c.y += p.y; c.z += p.z; }
    c = normalize3(c);
    if (!(norm3(c) > 0.0)) return 0.0;
    std::vector<V3> poly(poly_in.size());
    for (size_t i = 0; i < poly_in.size(); ++i) poly[i] = normalize3(rotate_to_z(poly_in[i], c));
    const V3& a = poly[0];
    double tot = 0.0;
    for (size_t i = 1; i + 1 < poly.size(); ++i) {
        const V3& b = poly[i];
        const V3& d = poly[i+1];
        double det = a.x*(b.y*d.z - b.z*d.y) + a.y*(b.z*d.x - b.x*d.z) + a.z*(b.x*d.y - b.y*d.x);
        double den = 1.0 + dot3(a,b) + dot3(b,d) + dot3(d,a);
        tot += 2.0 * std::atan2(det, den);
    }
    tot = std::fabs(tot);
    if (tot > 2.0 * kPi) tot = 4.0 * kPi - tot;
    return tot;
}

// ---------------------------------------------------------------------------
// HEALPix 面坐标卡（face chart）：(face, u, v) in [0,1]^2 -> 球面单位向量
//
// 分支与 Gorski et al. 2005 §4 一致（北面 0-3：u+v<=1 赤道三角、u+v>1 极冠三角，
// 极点 = (1,1)；赤道面 4-7；南面 8-11：极点 = (0,0)）。
// 等面积性: |dOmega/dudv| = pi/3 全 face 常数（解析可证，见报告 §3.1）。
//
// mode 0 = production 复刻路径：极冠分支 z = 1 - s^2/3 后 theta = acos(z)，
//          再由 sin(theta) 得横向分量 ⇒ 近极点 z≈1 相消，theta 相对误差 ~1e-16/theta。
// mode 1 = 数值稳定路径：theta = 2*asin(s/sqrt(6))，横向半径
//          r = s*sqrt(2/3)*sqrt(1 - s^2/6)（与 theta 的 sin 逐项等价但无相消）。
// ---------------------------------------------------------------------------
inline void chart_uv_to_ang(int face, double u, double v, double& theta, double& phi, int mode) {
    double z = 0.0;
    bool polar = false, south = false;
    if (face <= 3) {
        if (u + v <= 1.0) {
            z = kTwoThird * (u + v);
            phi = 0.25 * kPi * (u - v + 1.0 + 2.0 * face);
        } else {
            double s = 2.0 - u - v;
            z = 1.0 - s * s / 3.0;
            // s == 0 即极点本身：phi 无定义（生产 xyf2ang_replica / hp_to_xyz 同样置 0）
            phi = (s > 0.0) ? (kHalfPi * face + kPi * (1.0 - v) / (2.0 * s)) : (kHalfPi * face);
            polar = true;
        }
    } else if (face <= 7) {
        z = kTwoThird * (u + v - 1.0);
        phi = 0.25 * kPi * (u - v + 2.0 * (face - 4));
    } else {
        if (u + v >= 1.0) {
            z = kTwoThird * (u + v - 2.0);
            phi = 0.25 * kPi * (u - v + 1.0 + 2.0 * (face - 8));
        } else {
            double s = u + v;
            z = -(1.0 - s * s / 3.0);
            phi = (s > 0.0) ? (kHalfPi * (face - 8) + kPi * u / (2.0 * s)) : (kHalfPi * (face - 8));
            polar = true; south = true;
        }
    }
    if (polar) {
        double s = south ? (u + v) : (2.0 - u - v);
        if (mode == 1) {
            double a = std::asin(std::max(-1.0, std::min(1.0, s / kRoot6)));
            theta = south ? (kPi - 2.0 * a) : (2.0 * a);
        } else {
            theta = std::acos(std::max(-1.0, std::min(1.0, z)));   // production 复刻（近极点相消）
        }
    } else {
        theta = std::acos(std::max(-1.0, std::min(1.0, z)));
    }
    if (phi < 0.0) phi += kTwoPi;
    if (phi >= kTwoPi) phi -= kTwoPi;
}

inline V3 chart_uv_to_xyz(int face, double u, double v, int mode = 1) {
    double theta = 0.0, phi = 0.0;
    chart_uv_to_ang(face, u, v, theta, phi, mode);
    double st, ct;
    if (mode == 1 && face <= 3 && u + v > 1.0) {
        double s = 2.0 - u - v;
        st = s * std::sqrt(2.0/3.0) * std::sqrt(std::max(0.0, 1.0 - s*s/6.0));
        ct = 1.0 - s*s/3.0;
    } else if (mode == 1 && face >= 8 && u + v < 1.0) {
        double s = u + v;
        st = s * std::sqrt(2.0/3.0) * std::sqrt(std::max(0.0, 1.0 - s*s/6.0));
        ct = -(1.0 - s*s/3.0);
    } else {
        st = std::sin(theta);
        ct = std::cos(theta);
    }
    return {st * std::cos(phi), st * std::sin(phi), ct};
}

// 单位向量 -> 指定 face 的 chart 坐标（可落在 [0,1]^2 之外）
// mode 0 = production 复刻：极冠 s = sqrt(3(1-Z))（近极点相消）
// mode 1 = 稳定：theta = atan2(横向模, Z)，s = sqrt(6)*sin(theta/2)
inline bool xyz_to_chart_forced(int face, const V3& vin, double& u, double& v, int mode = 1) {
    V3 p = normalize3(vin);
    if (!(norm3(p) > 0.0)) return false;
    double phi = std::atan2(p.y, p.x);
    if (phi < 0.0) phi += kTwoPi;
    auto pick = [&](double sum, double diff_base) {
        double bu = 0, bv = 0, bd = std::numeric_limits<double>::infinity();
        for (int k = -2; k <= 2; ++k) {
            double diff = diff_base + 8.0 * k;
            double uu = 0.5 * (sum + diff), vv = 0.5 * (sum - diff);
            double du = std::max(0.0, std::max(-uu, uu - 1.0));
            double dv = std::max(0.0, std::max(-vv, vv - 1.0));
            double d = du*du + dv*dv;
            if (d < bd) { bd = d; bu = uu; bv = vv; }
        }
        u = bu; v = bv;
    };
    auto radial = [&](double zz) {
        if (mode == 0) return std::sqrt(std::max(0.0, 3.0 * (1.0 - zz)));
        double rho = std::sqrt(p.x*p.x + p.y*p.y);
        double th = std::atan2(rho, zz);
        return kRoot6 * std::sin(0.5 * th);
    };
    if (face <= 3) {
        if (p.z >= kTwoThird) {
            double s = radial(p.z);
            if (!(s > 0.0)) { u = 1.0; v = 1.0; return true; }   // 极点本身
            double pb = phi - kHalfPi * face;
            double bu = 0, bv = 0, bd = std::numeric_limits<double>::infinity();
            for (int k = -2; k <= 2; ++k) {
                double phit = pb + kTwoPi * k;
                double uu = 1.0 - s + 2.0 * s * phit / kPi;
                double vv = 1.0 - 2.0 * s * phit / kPi;
                double du = std::max(0.0, std::max(-uu, uu - 1.0));
                double dv = std::max(0.0, std::max(-vv, vv - 1.0));
                double d = du*du + dv*dv;
                if (d < bd) { bd = d; bu = uu; bv = vv; }
            }
            u = bu; v = bv; return true;
        }
        pick(1.5 * p.z, 4.0 * phi / kPi - 1.0 - 2.0 * face);
        return true;
    }
    if (face <= 7) { pick(1.5 * p.z + 1.0, 4.0 * phi / kPi - 2.0 * (face - 4)); return true; }
    if (p.z <= -kTwoThird) {
        double s = radial(-p.z);
        if (!(s > 0.0)) { u = 0.0; v = 0.0; return true; }   // 南极本身
        double pb = phi - kHalfPi * (face - 8);
        double bu = 0, bv = 0, bd = std::numeric_limits<double>::infinity();
        for (int k = -2; k <= 2; ++k) {
            double phit = pb + kTwoPi * k;
            double uu = 2.0 * s * phit / kPi;
            double vv = s - uu;
            double du = std::max(0.0, std::max(-uu, uu - 1.0));
            double dv = std::max(0.0, std::max(-vv, vv - 1.0));
            double d = du*du + dv*dv;
            if (d < bd) { bd = d; bu = uu; bv = vv; }
        }
        u = bu; v = bv; return true;
    }
    pick(1.5 * p.z + 2.0, 4.0 * phi / kPi - 1.0 - 2.0 * (face - 8));
    return true;
}

// 自然 face 归属（与 ang2pix_nest 的 xyz_to_hp 同分支）
inline int natural_face(const V3& vin) {
    V3 p = normalize3(vin);
    double phi = std::atan2(p.y, p.x);
    if (phi < 0.0) phi += kTwoPi;
    double phi_t = std::fmod(phi, kHalfPi);
    int sector = (int)std::lround((phi - phi_t) / kHalfPi);
    sector = ((sector % 4) + 4) % 4;
    if (p.z >= kTwoThird) return sector;
    if (p.z <= -kTwoThird) return 8 + sector;
    double zunits = (p.z + kTwoThird) / (4.0 / 3.0);
    double phiunits = phi_t / kHalfPi;
    double xx = zunits + phiunits, yy = zunits - phiunits + 1.0;
    if (xx >= 1.0) { if (yy >= 1.0) return sector; return ((sector + 1) % 4) + 4; }
    if (yy >= 1.0) return sector + 4;
    return 8 + sector;
}

// ---------------------------------------------------------------------------
// 叶：面 f 内 (i,j) 叶方格的 4 角（chart 坐标，u,v 以 face 为单位 [0,1]）
// 角序与生产一致: (i,j)->(i+1,j)->(i+1,j+1)->(i,j+1)
// ---------------------------------------------------------------------------
inline void leaf_corner_uv(uint32_t Ns, uint32_t i, uint32_t j, double uv[4][2]) {
    const double a = (double)i / (double)Ns, b = (double)j / (double)Ns;
    const double c = (double)(i + 1) / (double)Ns, d = (double)(j + 1) / (double)Ns;
    uv[0][0]=a; uv[0][1]=b; uv[1][0]=c; uv[1][1]=b;
    uv[2][0]=c; uv[2][1]=d; uv[3][0]=a; uv[3][1]=d;
}

// 叶边界真实曲线采样：每条边 K 段（取起点不含终点），点严格在真实曲线上
inline void leaf_boundary_curve_f(int face, uint32_t Ns, uint32_t i, uint32_t j, int K,
                                  std::vector<V3>& out, int mode = 1) {
    out.clear();
    out.reserve((size_t)4 * (size_t)K);
    double uv[4][2];
    leaf_corner_uv(Ns, i, j, uv);
    for (int e = 0; e < 4; ++e) {
        int e2 = (e + 1) % 4;
        for (int k = 0; k < K; ++k) {
            double t = (double)k / (double)K;
            double u = uv[e][0] + t * (uv[e2][0] - uv[e][0]);
            double v = uv[e][1] + t * (uv[e2][1] - uv[e][1]);
            out.push_back(chart_uv_to_xyz(face, u, v, mode));
        }
    }
}

// 叶中心（chart 坐标中点）
inline V3 leaf_center_xyz(int face, uint32_t Ns, uint32_t i, uint32_t j, int mode = 1) {
    double a = ((double)i + 0.5) / (double)Ns, b = ((double)j + 0.5) / (double)Ns;
    return chart_uv_to_xyz(face, a, b, mode);
}

// ---------------------------------------------------------------------------
// TAN WCS（与生产探针同口径）
// ---------------------------------------------------------------------------
struct TanWcs {
    double ra0 = 0, dec0 = 0, crpix[2] = {0, 0}, cd[2][2] = {{0,0},{0,0}};
    bool pixelToSky(double x, double y, double& ra, double& dec) const {
        double dx = x - crpix[0], dy = y - crpix[1];
        double xi  = (cd[0][0]*dx + cd[0][1]*dy) * kDeg2Rad;
        double eta = (cd[1][0]*dx + cd[1][1]*dy) * kDeg2Rad;
        double rho = std::sqrt(xi*xi + eta*eta);
        double d0 = dec0*kDeg2Rad, a0 = ra0*kDeg2Rad, dr, ar;
        if (rho < 1e-14) { dr = d0; ar = a0; }
        else {
            double c = std::atan(rho), sc = std::sin(c), cc = std::cos(c);
            dr = std::asin(std::max(-1.0, std::min(1.0, cc*std::sin(d0) + eta*sc*std::cos(d0)/rho)));
            ar = a0 + std::atan2(xi*sc, rho*std::cos(d0)*cc - eta*std::sin(d0)*sc);
        }
        ra = ar*kRad2Deg; dec = dr*kRad2Deg;
        if (ra < 0) ra += 360.0;
        if (ra >= 360.0) ra -= 360.0;
        return std::isfinite(ra) && std::isfinite(dec);
    }
};

inline TanWcs make_wcs(double ra, double dec, double arcsec_per_px, double rot_deg) {
    TanWcs w; w.ra0 = ra; w.dec0 = dec;
    double s = arcsec_per_px / 3600.0, th = rot_deg * kDeg2Rad;
    w.cd[0][0] =  s*std::cos(th); w.cd[0][1] = s*std::sin(th);
    w.cd[1][0] = -s*std::sin(th); w.cd[1][1] = s*std::cos(th);
    return w;
}

// drop 多边形（源像素中心 (px,py)，pixfrac 收缩，每边 m 段采样）
inline bool build_drop(const TanWcs& w, double px, double py, double pixfrac, int m,
                       std::vector<V3>& out) {
    out.clear();
    double half = 0.5 * pixfrac;
    double c[4][2] = {{px-half,py-half},{px+half,py-half},{px+half,py+half},{px-half,py+half}};
    for (int e = 0; e < 4; ++e) {
        double x0 = c[e][0], y0 = c[e][1], x1 = c[(e+1)%4][0], y1 = c[(e+1)%4][1];
        for (int k = 0; k < m; ++k) {
            double t = (double)k / (double)m, ra, dec;
            if (!w.pixelToSky(x0 + t*(x1-x0), y0 + t*(y1-y0), ra, dec)) return false;
            out.push_back(radec_to_vec(ra, dec));
        }
    }
    return out.size() >= 3;
}

// ---------------------------------------------------------------------------
// 直接 3D 构造（不经 (ra,dec) 度往返）：切点 T + 切平面基 (e1,e2)，
// p = normalize(T + xi*e1 + eta*e2)（gnomonic/TAN 的精确形式）。
// 极点附近 (ra,dec) 往返会把离极点偏移整体丢失（dec 舍入到 90 度），
// 本函数作为对照基准。
// ---------------------------------------------------------------------------
struct TanWcs3D {
    V3 T, e1, e2;
    double s_deg_per_px = 0, rot_rad = 0;
};
inline TanWcs3D make_wcs3d(double ra, double dec, double arcsec_per_px, double rot_deg) {
    TanWcs3D w;
    w.T = radec_to_vec(ra, dec);
    double a = ra * kDeg2Rad, d = dec * kDeg2Rad;
    w.e1 = {-std::sin(d) * std::cos(a), -std::sin(d) * std::sin(a), std::cos(d)};
    w.e2 = {-std::sin(a), std::cos(a), 0.0};
    w.s_deg_per_px = arcsec_per_px / 3600.0;
    w.rot_rad = rot_deg * kDeg2Rad;
    return w;
}
inline V3 wcs3d_sky(const TanWcs3D& w, double x, double y) {
    double ct = std::cos(w.rot_rad), st = std::sin(w.rot_rad);
    double dx = w.s_deg_per_px * ( ct * x + st * y);
    double dy = w.s_deg_per_px * (-st * x + ct * y);
    double xi = dx * kDeg2Rad, eta = dy * kDeg2Rad;
    // 标准 TAN 约定: eta 沿 e1(北), xi 沿 e2(东)。TAN 逆投影精确等于 gnomonic
    //   p ∝ T + eta*e1 + xi*e2
    // 该式不含 asin/acos，故在 dec≈±90 处不丢精度（对照 pixelToSky 的 asin 形式）。
    return normalize3({w.T.x + eta*w.e1.x + xi*w.e2.x,
                       w.T.y + eta*w.e1.y + xi*w.e2.y,
                       w.T.z + eta*w.e1.z + xi*w.e2.z});
}
inline bool build_drop3d(const TanWcs3D& w, double px, double py, double pixfrac, int m,
                         std::vector<V3>& out) {
    out.clear();
    double half = 0.5 * pixfrac;
    double c[4][2] = {{px-half,py-half},{px+half,py-half},{px+half,py+half},{px-half,py+half}};
    for (int e = 0; e < 4; ++e) {
        double x0 = c[e][0], y0 = c[e][1], x1 = c[(e+1)%4][0], y1 = c[(e+1)%4][1];
        for (int k = 0; k < m; ++k) {
            double t = (double)k / (double)m;
            out.push_back(wcs3d_sky(w, x0 + t*(x1-x0), y0 + t*(y1-y0)));
        }
    }
    return out.size() >= 3;
}

// drop 的 4 个内法向（凸多边形半空间）
inline void drop_clip_normals(const std::vector<V3>& drop, std::vector<V3>& nrm) {
    nrm.clear();
    V3 c{0,0,0};
    for (const auto& p : drop) { c.x += p.x; c.y += p.y; c.z += p.z; }
    c = normalize3(c);
    const size_t np = drop.size();
    for (size_t j = 0; j < np; ++j) {
        V3 nn = normalize3(cross3(drop[j], drop[(j+1)%np]));
        if (dot3(nn, c) < 0.0) nn = {-nn.x, -nn.y, -nn.z};
        nrm.push_back(nn);
    }
}

// ---------------------------------------------------------------------------
// 独立 oracle：叶边界真实曲线 K 段/边 -> 球面 S-H 裁剪 -> VOS(旋转) 面积
// 顶点严格落在真实曲线上，弦逼近误差随 K^-2 收敛；对外用 Richardson 外推。
// ---------------------------------------------------------------------------
inline std::vector<V3> sh_clip(const std::vector<V3>& subject,
                               const std::vector<V3>& clip_normals, double tol = 0.0) {
    std::vector<V3> cur = subject;
    for (const V3& n : clip_normals) {
        if (cur.size() < 3) return {};
        std::vector<V3> nxt;
        nxt.reserve(cur.size() + 2);
        size_t m = cur.size();
        for (size_t i = 0; i < m; ++i) {
            const V3& S = cur[(i + m - 1) % m];
            const V3& E = cur[i];
            double ds = dot3(n, S), de = dot3(n, E);
            bool si = ds >= -tol, ei = de >= -tol;
            if (ei) {
                if (!si) {
                    V3 ne = cross3(S, E);
                    V3 I = normalize3(cross3(ne, n));
                    if (dot3(I, S) + dot3(I, E) < 0.0) I = {-I.x, -I.y, -I.z};
                    nxt.push_back(I);
                }
                nxt.push_back(E);
            } else if (si) {
                V3 ne = cross3(S, E);
                V3 I = normalize3(cross3(ne, n));
                if (dot3(I, S) + dot3(I, E) < 0.0) I = {-I.x, -I.y, -I.z};
                nxt.push_back(I);
            }
        }
        cur.swap(nxt);
    }
    if (cur.size() < 3) return {};
    return cur;
}

// 单次 oracle 求值（给定 K）
inline double oracle_overlap_K(int face, uint32_t Ns, uint32_t i, uint32_t j,
                               const std::vector<V3>& drop, int K, int mode = 1) {
    std::vector<V3> bnd;
    leaf_boundary_curve_f(face, Ns, i, j, K, bnd, mode);
    std::vector<V3> nrm;
    drop_clip_normals(drop, nrm);
    std::vector<V3> res = sh_clip(bnd, nrm, 0.0);
    if (res.size() < 3) return 0.0;
    return vos_area_rotated(res);
}

// Richardson 外推（K 与 2K）——弦误差 ~ C/K^2 ⇒ A_inf ≈ (4 A(2K) - A(K))/3
inline double oracle_overlap(int face, uint32_t Ns, uint32_t i, uint32_t j,
                             const std::vector<V3>& drop, int K = 24, int mode = 1,
                             double* aK_out = nullptr, double* a2K_out = nullptr) {
    double a1 = oracle_overlap_K(face, Ns, i, j, drop, K, mode);
    double a2 = oracle_overlap_K(face, Ns, i, j, drop, 2*K, mode);
    if (aK_out) *aK_out = a1;
    if (a2K_out) *a2K_out = a2;
    return (4.0*a2 - a1) / 3.0;
}

// 叶自身面积（真实曲线 K 段）
inline double leaf_area_K(int face, uint32_t Ns, uint32_t i, uint32_t j, int K, int mode = 1) {
    std::vector<V3> bnd;
    leaf_boundary_curve_f(face, Ns, i, j, K, bnd, mode);
    return vos_area_rotated(bnd);
}

// 面内角点到 chart 坐标
inline bool face_corner_uv(int face, int which, double& u, double& v) {
    static const double cs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
    if (which < 0 || which > 3) return false;
    u = cs[which][0]; v = cs[which][1];
    (void)face;
    return true;
}

} // namespace pp
#endif // POLAR_COMMON_H
