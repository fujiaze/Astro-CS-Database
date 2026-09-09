// P1-WCS-TEST · 独立 oracle
//
// 合同锚: docs/algorithms/PLATESOLVE.md §8 (参考实现/Oracle) + §9 (容差来源)
// + §11.4 TEST-WCS-DESIGN-001 (P1-WCS-DOC 冻结)。
//
// 独立性规则 (模板 <prefix>-TEST §3): oracle 不调用被测函数、不复制同一实现。
// 本 oracle 推导路径全部独立于被测代码路径:
//   - gnomonic 平面坐标: 球面三角闭式解正算 (§8 "合成投影图已知 WCS 恢复
//     残差 <0.1"" 的独立实现; 对齐 astrometry.net TAN 语义但不抄其代码)。
//   - 线性 6 参数: 由 identity 对应直接解析求解 (平移由前两对差分, 尺度/旋转
//     由一对正交方向向量), 与被测 iter_trans 的迭代重加权路径完全不同源。
//   - SIP 系数期望: 由注入的二次畸变系数经 inv(M)·(qx,qy) 闭式映射独立计算
//     (与被测 extract_wcs_sip 的正规方程/AP 网格反变换路径不同源)。
//   - TAN 投影数学: 标准球面公式 (Calabretta & Greisen 2002 语义),
//     独立重写, 不调用 WcsTan/iter_trans/extract_wcs_sip 任何函数。
#ifndef P1WCS_ORACLE_HPP
#define P1WCS_ORACLE_HPP

#include <cmath>
#include <cstddef>
#include <vector>

namespace p1wcs {

constexpr double kDegToRad = 0.01745329251994329577;  // π/180
constexpr double kRadToDeg = 57.29577951308232087680;  // 180/π

// ---------------------------------------------------------------------------
// oracle-1: gnomonic 平面坐标正算 (球面三角闭式解, 独立于一切被测路径)
//   输入: 天球坐标 (ra, dec) 度 + 切点 (ra0, dec0) 度
//   输出: (xi, eta) **度** (标准 TAN 中间世界坐标, 与 CD 矩阵 deg/px 域
//   直接衔接; 角秒域调用方自行 ×3600)
//   推导: 单位球旋转 + gnomonic 投影闭式解:
//     xi  = cos(dec)·sin(Δra) / (sin(dec0)·sin(dec) + cos(dec0)·cos(dec)·cos(Δra))
//     eta = (cos(dec0)·sin(dec) - sin(dec0)·cos(dec)·cos(Δra)) / 同分母
//   (分子分母同为弧度商, 输出按度回乘)
// ---------------------------------------------------------------------------
inline void oracle_gnomonic(double ra, double dec, double ra0, double dec0,
                            double* xi, double* eta) {
    const double d_ra = (ra - ra0) * kDegToRad;
    const double d = dec * kDegToRad;
    const double d0 = dec0 * kDegToRad;
    const double cd = std::cos(d), sd = std::sin(d);
    const double c0 = std::cos(d0), s0 = std::sin(d0);
    const double den = s0 * sd + c0 * cd * std::cos(d_ra);
    *xi = (cd * std::sin(d_ra)) / den * kRadToDeg;
    *eta = (c0 * sd - s0 * cd * std::cos(d_ra)) / den * kRadToDeg;
}

// ---------------------------------------------------------------------------
// oracle-1b: gnomonic 正算第二推导路径 (WCS-001 增补, 2026-09-09)
//   独立性: 不共享 oracle-1 的球面三角闭式公式 — 三维单位向量 + 切平面
//   正交基法 (第三路径 / "第三方语义"参考实现):
//     r  = (cos δ cos α, cos δ sin α, sin δ)          (天球单位向量, 弧度域)
//     east  = normalize(ẑ × r0) = (−sin α0, cos α0, 0)
//     north = r0 × east
//     切平面交点 p = r / (r·r0)  (gnomonic: 球心过 r 直线交切平面 r·x=1)
//     ξ_rad = p·east,  η_rad = p·north  → ×kRadToDeg 回度域
//   与 oracle-1 只共享输入约定与 kDegToRad 常数, 推导路径不同源; 度/弧度
//   单位错在两路径成对抵消概率≈0 (oracle 初版两处量纲缺陷均逃过自洽、
//   仅被交叉/对拍捕获的教训, 见 oracle_gnomonic_inv 修正记录)。
//   用途: units/oracle 组 o1_gnomonic_cross 双路径交叉断言。
// ---------------------------------------------------------------------------
inline void oracle_gnomonic_vec(double ra, double dec, double ra0, double dec0,
                                double* xi, double* eta) {
    const double a = ra * kDegToRad, d = dec * kDegToRad;
    const double a0 = ra0 * kDegToRad, d0 = dec0 * kDegToRad;
    const double rx = std::cos(d) * std::cos(a);
    const double ry = std::cos(d) * std::sin(a);
    const double rz = std::sin(d);
    const double r0x = std::cos(d0) * std::cos(a0);
    const double r0y = std::cos(d0) * std::sin(a0);
    const double r0z = std::sin(d0);
    // east = normalize(ẑ × r0) = (−sin α0, cos α0, 0) (|ẑ × r0| = cos δ0 > 0,
    // 切点不在极点 — fixture 域保证)
    const double ex = -r0y / std::cos(d0);
    const double ey = r0x / std::cos(d0);
    // north = r0 × east
    const double nx = r0y * 0.0 - r0z * ey;
    const double ny = r0z * ex - r0x * 0.0;
    const double nz = r0x * ey - r0y * ex;
    const double denom = rx * r0x + ry * r0y + rz * r0z;  // r·r0 = cos c
    const double px = rx / denom, py = ry / denom, pz = rz / denom;
    *xi = (px * ex + py * ey) * kRadToDeg;
    *eta = (px * nx + py * ny + pz * nz) * kRadToDeg;
}

// 逆变换 (度 xi/eta → 度 ra/dec): 标准 TAN (gnomonic) 逆投影闭式解
//   输入单位: 度 (与 CD 矩阵 deg/px 域一致)
//   S = sqrt(1 + xi² + eta²)  (gnomonic 性质 cosc = 1/S, 全弧度域)
//   dec = asin((sin(dec0) + η·cos(dec0)) / S)
//   ra  = ra0 + atan2(ξ, cos(dec0) − η·sin(dec0))   ← 分母无 S (tanΔα =
//   ξ/(S·X), X = (c0−η·s0)/S ⇒ S 相消; 与 WcsTan 的 atan(R)/asin 形式
//   同构: 分子分母同除 R 后恒等)
//   (修正记录: 初版误用角度空间旋转式 (漏 S 归一), 二版 Δα 分母多乘 S —
//   均由 units F6 交叉断言 / oracle 往返断言捕获, 2026-09-09)
//   (与 WcsTan 的 atan(R)/asin 形式同构不同源; 修正记录: 初版误用角度
//   空间旋转式 (漏 S 归一) 且单位约定混用, units F6 交叉断言捕获,
//   2026-09-09)
inline void oracle_gnomonic_inv(double xi, double eta, double ra0, double dec0,
                                double* ra, double* dec) {
    const double xi_r = xi * kDegToRad;
    const double eta_r = eta * kDegToRad;
    const double d0 = dec0 * kDegToRad;
    const double c0 = std::cos(d0), s0 = std::sin(d0);
    const double S = std::sqrt(1.0 + xi_r * xi_r + eta_r * eta_r);
    const double sd = (s0 + eta_r * c0) / S;
    const double d = std::asin(sd);
    const double r = ra0 * kDegToRad + std::atan2(xi_r, c0 - eta_r * s0);
    *dec = d * kRadToDeg;
    *ra = r * kRadToDeg;
    if (*ra >= 360.0) *ra -= 360.0;
    if (*ra < 0.0) *ra += 360.0;
}

// ---------------------------------------------------------------------------
// oracle-2: 线性 6 参数解析求解 (identity 对应, 弧秒域)
//   W = M·U + t, M 2×2。解析路径: 平移 = W[0] - M·U[0]; M 由 U[1]-U[0] 与
//   U[2]-U[0] 两基向量映射闭式解 (2×2 Cramer 法则)。与被测迭代路径不同源。
// ---------------------------------------------------------------------------
struct OracleLinear6 {
    double m00, m01, m10, m11;  // 弧秒/像素
    double t0, t1;              // 弧秒
};

inline OracleLinear6 oracle_solve_linear6(const double u_xy[][2],
                                          const double w_xy[][2], int n) {
    // 要求 n>=3 且三点不共线 (fixture 保证)
    const double ux1 = u_xy[1][0] - u_xy[0][0], uy1 = u_xy[1][1] - u_xy[0][1];
    const double ux2 = u_xy[2][0] - u_xy[0][0], uy2 = u_xy[2][1] - u_xy[0][1];
    const double wx1 = w_xy[1][0] - w_xy[0][0], wy1 = w_xy[1][1] - w_xy[0][1];
    const double wx2 = w_xy[2][0] - w_xy[0][0], wy2 = w_xy[2][1] - w_xy[0][1];
    const double det = ux1 * uy2 - ux2 * uy1;
    OracleLinear6 o{};
    o.m00 = (wx1 * uy2 - wx2 * uy1) / det;
    o.m01 = (wx2 * ux1 - wx1 * ux2) / det;
    o.m10 = (wy1 * uy2 - wy2 * uy1) / det;
    o.m11 = (wy2 * ux1 - wy1 * ux2) / det;
    o.t0 = w_xy[0][0] - (o.m00 * u_xy[0][0] + o.m01 * u_xy[0][1]);
    o.t1 = w_xy[0][1] - (o.m10 * u_xy[0][0] + o.m11 * u_xy[0][1]);
    (void)n;
    return o;
}

inline void oracle_apply6(const OracleLinear6& o, double ux, double uy,
                          double* wx, double* wy) {
    *wx = o.m00 * ux + o.m01 * uy + o.t0;
    *wy = o.m10 * ux + o.m11 * uy + o.t1;
}

// 2×2 矩阵求逆 (Cramer; oracle 内部工具)
inline void oracle_invert2(double a, double b, double c, double d,
                           double* ia, double* ib, double* ic, double* id) {
    const double det = a * d - b * c;
    *ia = d / det;
    *ib = -b / det;
    *ic = -c / det;
    *id = a / det;
}

// ---------------------------------------------------------------------------
// oracle-3: Y-up 真值 → Y-down FITS 期望 (独立按几何推导, 非 ipv_wcs.cpp 路径)
//   U.Y-up → Y-down: u_d = u, v_d = -v ⇒ W = M_up·diag(1,-1)·(u,v)_down + t
//   CD_down = M_up·diag(1,-1)/3600 ⇒ cd11=m00/3600, cd12=-m01/3600,
//                                    cd21=m10/3600, cd22=-m11/3600
//   SIP (二次注入): Y-up A_up = inv(M_up)·Q (Q=(qx,qy) 列);
//   Y-down 变换: A[i][j]·=(-1)^j, B[i][j]·=-(-1)^j (输入/输出 y 翻转的
//   多项式符号推演, 独立推导见 §11.4 F2 注记)。
// ---------------------------------------------------------------------------
inline void oracle_cd_from_truth(const OracleLinear6& o,
                                 double* cd11, double* cd12,
                                 double* cd21, double* cd22) {
    *cd11 = o.m00 / 3600.0;
    *cd12 = -o.m01 / 3600.0;
    *cd21 = o.m10 / 3600.0;
    *cd22 = -o.m11 / 3600.0;
}

// SIP A/B 期望 (Y-down, 二次三项): 注入系数 → inv(M_up)·q → 符号变换
struct OracleSipExpect {
    double A_up[3];  // [20, 11, 02] Y-up x 多项式
    double B_up[3];  // Y-up y 多项式
    double A_dn[3];  // Y-down (FITS 输出语义)
    double B_dn[3];
};

inline OracleSipExpect oracle_sip_expect(const OracleLinear6& o,
                                         double qx20, double qx11, double qx02,
                                         double qy20, double qy11, double qy02) {
    OracleSipExpect e{};
    double i00, i01, i10, i11;
    oracle_invert2(o.m00, o.m01, o.m10, o.m11, &i00, &i01, &i10, &i11);
    // A_up = inv(M_up)·(qx, qy)
    e.A_up[0] = i00 * qx20 + i01 * qy20;
    e.A_up[1] = i00 * qx11 + i01 * qy11;
    e.A_up[2] = i00 * qx02 + i01 * qy02;
    e.B_up[0] = i10 * qx20 + i11 * qy20;
    e.B_up[1] = i10 * qx11 + i11 * qy11;
    e.B_up[2] = i10 * qx02 + i11 * qy02;
    // Y-down: A' = A·(-1)^j, B' = -B·(-1)^j (j = y 幂)
    // 索引 [0]=20 (j=0), [1]=11 (j=1), [2]=02 (j=2)
    const double sgn[3] = {1.0, -1.0, 1.0};  // (-1)^j
    for (int k = 0; k < 3; ++k) {
        e.A_dn[k] = e.A_up[k] * sgn[k];
        e.B_dn[k] = -e.B_up[k] * sgn[k];
    }
    return e;
}

// ---------------------------------------------------------------------------
// oracle-4: FITS WCS 前向像素→天球 (含 SIP 前向多项式; 独立实现, 验 F2/F6)
//   u,v = 提交像素 − crpix (0-based FITS 域); SIP 前向畸变作用于像素域:
//   u' = u + A_poly(u,v), v' = v + B_poly(u,v) (FITS SIP 标准语义);
//   (ξ, η) = CD·(u', v') (度); (ra, dec) = TAN 逆投影 (oracle-1)。
// ---------------------------------------------------------------------------
inline void oracle_wcs_forward(double cd11, double cd12, double cd21, double cd22,
                               double crval1, double crval2,
                               double crpix1, double crpix2,
                               const double* A, const double* B, int sip_order,
                               double x, double y,
                               double* ra, double* dec) {
    const double u = x - crpix1;
    const double v = y - crpix2;
    double uc = u, vc = v;
    if (sip_order >= 2 && A != nullptr && B != nullptr) {
        // SIP 前向畸变作用于**像素域**: u' = u + f(u,v) (f 单位 px, A 系数
        // 单位 1/px), (ξ,η) = CD·(u',v') — FITS SIP 标准语义; 修正记录:
        // 初版误将 f (px) 直接加到 CD·(u,v) (度) 上 (量纲错), F2 对拍捕获。
        double ax = 0.0, by = 0.0;
        for (int i = 0; i <= sip_order; ++i) {
            for (int j = 0; j <= sip_order - i; ++j) {
                const int idx = i * 6 + j;
                const double uv = std::pow(u, i) * std::pow(v, j);
                ax += A[idx] * uv;
                by += B[idx] * uv;
            }
        }
        uc = u + ax;
        vc = v + by;
    }
    const double fx = cd11 * uc + cd12 * vc;
    const double fy = cd21 * uc + cd22 * vc;
    oracle_gnomonic_inv(fx, fy, crval1, crval2, ra, dec);
}

// ---------------------------------------------------------------------------
// oracle-5: FITS WCS 逆向 (ra/dec → 像素; 无 SIP 精确 / 有 SIP 固定点迭代;
// 独立实现, 验 F2 逆向 roundtrip)
//   逆向: (xi, eta) = gnomonic(ra, dec) → (u,v)₀ = CD^-1·(xi,eta); 有 SIP:
//   解 u 满足 u = (u,v)₀ - SIPfwd(u,v), 固定点迭代 u ← u₀ - SIPfwd(u,v)。
//   收敛性: FIX-WCS-B 畸变 |∇SIP| ≈ 2·A20·u_max ≈ 0.43 < 1 (压缩映射),
//   60 次迭代 (角点收敛率 ~0.66 实测校准, 30 次残 1.5e-4 px 不足, 60 次 → <1e-9 px), 远深于 1e-4 px 冻结容差。
// ---------------------------------------------------------------------------
inline void oracle_wcs_reverse(double cd11, double cd12, double cd21, double cd22,
                               double crval1, double crval2,
                               double crpix1, double crpix2,
                               const double* A, const double* B, int sip_order,
                               double ra, double dec,
                               double* x, double* y) {
    double xi, eta;
    oracle_gnomonic(ra, dec, crval1, crval2, &xi, &eta);
    double i00, i01, i10, i11;
    oracle_invert2(cd11, cd12, cd21, cd22, &i00, &i01, &i10, &i11);
    const double u0 = i00 * xi + i01 * eta;
    const double v0 = i10 * xi + i11 * eta;
    double u = u0, v = v0;
    if (sip_order >= 2 && A != nullptr && B != nullptr) {
        for (int it = 0; it < 60; ++it) {
            double ax = 0.0, by = 0.0;
            for (int i = 0; i <= sip_order; ++i) {
                for (int j = 0; j <= sip_order - i; ++j) {
                    const int idx = i * 6 + j;
                    const double uv = std::pow(u, i) * std::pow(v, j);
                    ax += A[idx] * uv;
                    by += B[idx] * uv;
                }
            }
            u = u0 - ax;
            v = v0 - by;
        }
    }
    *x = u + crpix1;
    *y = v + crpix2;
}

// ---------------------------------------------------------------------------
// oracle-6: AP/BP 逆向固定点解 (F2 AP/BP 逆向一致性; 独立实现)
//   标准 SIP 逆: 给定 (xi,eta) → u₀ = CD⁻¹·(xi,eta); 解 u 满足
//   u = u₀ - AP_full(u,v) (AP_full 含被测写入的 -u/-v 线性归一项,
//   ipv_wcs.cpp:463-464 AP[1,0]-=1, BP[0,1]-=1)。固定点 60 次迭代 (角点收敛率实测校准, 同 oracle-5 压缩映射 |∇AP|<1)。
// ---------------------------------------------------------------------------
inline void oracle_wcs_reverse_apbp(double cd11, double cd12, double cd21,
                                    double cd22, double crval1, double crval2,
                                    double crpix1, double crpix2,
                                    const double* AP, const double* BP,
                                    int ap_order, double ra, double dec,
                                    double* x, double* y) {
    double xi, eta;
    oracle_gnomonic(ra, dec, crval1, crval2, &xi, &eta);
    double i00, i01, i10, i11;
    oracle_invert2(cd11, cd12, cd21, cd22, &i00, &i01, &i10, &i11);
    const double u0 = i00 * xi + i01 * eta;
    const double v0 = i10 * xi + i11 * eta;
    double u = u0, v = v0;
    if (ap_order >= 2 && AP != nullptr && BP != nullptr) {
        for (int it = 0; it < 60; ++it) {
            double ax = 0.0, by = 0.0;
            for (int i = 0; i <= ap_order; ++i) {
                for (int j = 0; j <= ap_order - i; ++j) {
                    const int idx = i * 6 + j;
                    const double uv = std::pow(u, i) * std::pow(v, j);
                    ax += AP[idx] * uv;
                    by += BP[idx] * uv;
                }
            }
            u = u0 + ax;
            v = v0 + by;
        }
    }
    *x = u + crpix1;
    *y = v + crpix2;
}

}  // namespace p1wcs

#endif  // P1WCS_ORACLE_HPP
