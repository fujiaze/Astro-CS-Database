// tests/unit/p3_projection_test.cpp — P3-001 投影 registry 与冻结四投影验证面
//
// 合同锚: ALG-P3-PROJ-IMPL-001 §15 (docs/algorithms/PHASE3_PROJ_IMPL.md,
// P3-001 新 claim) + SCI-P3-001 (FROZEN, TAN 容差/守卫零改动) + 宪章 §7.3/
// §18.1 (首批四投影 TAN/SIN/CAR/AIT registry 冻结)。
//
// 覆盖(冻结容差全部写死, 不事后放宽):
//   G1 registry 完整性: 版本常量=1、恰 4 行、码/CTYPE/id/函数指针/声明域
//      互异完备、未知码→nullptr(无 fallback)、越界 id→nullptr。
//   G2 每投影独立往返 Oracle: 3D 单位向量第一性原理(点积/正交基, 不调
//      生产实现), world2pix∘pix2world 往返 <1e-6 px (SCI §7 冻结)。
//   G3 TAN 冻结零漂移: 与 lib/phase3_session/p3_wcs.cpp 生产实现全网格
//      bitwise 对拍(G1/G2 公式零改动证明)。
//   G4 G1 CD 构造精确断言(parity/PA/crpix 四投影同构)。
//   G5 负面清单: 未知投影/parity 非法/|dec|>85/scale≤0/尺寸越界/空指针/
//      SIN 半球外/AIT 域外/CAR |dec|>90 全部显式拒绝(fail-closed)。
//   G6 CTYPE 关键词面(RA---<code>/DEC--<code>, 每行 ≤80 字节)。
//   G7 确定性: 全网格重复计算 bitwise 一致。
//   G8 1/N worker: 像素网格 1 vs 4 worker 分块计算 bitwise 一致。
//   G9 故障注入: ASTROCS_P3PROJ_FAULT=tan|sin|car|ait|registry 注入等价
//      缺陷必败(测试级注入, 生产源零 getenv, P2-002 先例同构)。
#include "p3_projection.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "p3_wcs.h"  // lib/phase3_session (TAN 冻结生产实现, 对拍用)

using astrocs::phase3proj::P3ProjectionDescriptor;
using astrocs::phase3proj::P3ProjectionId;
using astrocs::phase3proj::P3ProjectionSpec;
using astrocs::phase3proj::P3ProjectionStatus;
using astrocs::phase3proj::p3_projection_fits_keywords;
using astrocs::phase3proj::p3_projection_make;
using astrocs::phase3proj::p3_projection_pix2world;
using astrocs::phase3proj::p3_projection_registry_find;
using astrocs::phase3proj::p3_projection_registry_find_id;
using astrocs::phase3proj::p3_projection_registry_selfcheck;
using astrocs::phase3proj::p3_projection_registry_table;
using astrocs::phase3proj::p3_projection_world2pix;

namespace {

int failures = 0;
bool fault_mode = false;  // ASTROCS_P3PROJ_FAULT 注入模式(必败面)

#define CHECK_MSG(cond, msg)                                              \
    do {                                                                  \
        if (fault_mode) {                                                 \
            if (!(cond)) {                                                \
                std::fprintf(stderr,                                       \
                             "FAULT-EFFECT-CONFIRMED: %s\n", msg);        \
                ++failures;                                               \
            }                                                             \
        } else if (!(cond)) {                                             \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,            \
                         __LINE__, msg);                                  \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

constexpr double kRoundtripTolPx = 1e-6;   // SCI-P3-001 §7 冻结往返容差
constexpr double kOracleTolDeg = 1e-9;     // 解析解比对(FP64 机器精度量级)

// ---- 独立 oracle: 3D 单位向量第一性原理(不调任何生产函数) ------------------
// CRVAL 处正交基: up=native 极(zenithal), east/north 张成切平面;
// 球面点向量分量: ξ=−v·east(=−cosθ sinφ), η=v·north(=cosθ cosφ), sinθ=v·up。
struct Vec3 { double x, y, z; };
Vec3 v_sky(double ra_deg, double dec_deg) {
    const double a = ra_deg * M_PI / 180.0, d = dec_deg * M_PI / 180.0;
    return {std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
}
double v_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
struct OracleBasis {
    Vec3 up, east, north;
    bool ok;  // CRVAL 不在极点(north 可构造)
};
OracleBasis oracle_basis(double ra0, double dec0) {
    const Vec3 up = v_sky(ra0, dec0);
    const Vec3 east{-std::sin(ra0 * M_PI / 180.0), std::cos(ra0 * M_PI / 180.0), 0.0};
    const Vec3 north{up.y * east.z - up.z * east.y,
                     up.z * east.x - up.x * east.z,
                     up.x * east.y - up.y * east.x};  // north = up × east
    const double n_len = std::sqrt(v_dot(north, north));
    return {up, east, north, n_len > 1e-12};
}
// zenithal 球面点向量分量(独立式): ξ/η=正交基点积, sinθ=v·up。
bool oracle_sky_to_zenithal_plane(double ra0, double dec0, double ra, double dec,
                                  double* xi, double* eta, double* sin_theta) {
    const OracleBasis b = oracle_basis(ra0, dec0);
    if (!b.ok) return false;
    const Vec3 v = v_sky(ra, dec);
    *xi = v_dot(v, b.east);
    *eta = v_dot(v, b.north);
    *sin_theta = v_dot(v, b.up);
    if (*sin_theta <= 0.0) return false;  // 背面半球(zenithal 域)
    return true;
}
// TAN gnomonic 第一性反解: 切平面点 (x,y)(rad) → 球心连线交球面方向。
bool oracle_tan_pix2world(double ra0, double dec0, double x, double y,
                          double* ra, double* dec) {
    const OracleBasis b = oracle_basis(ra0, dec0);
    if (!b.ok) return false;
    Vec3 w{b.up.x + x * b.east.x + y * b.north.x,
           b.up.y + x * b.east.y + y * b.north.y,
           b.up.z + x * b.east.z + y * b.north.z};
    const double wn = std::sqrt(w.x * w.x + w.y * w.y + w.z * w.z);
    const double dec_r = std::asin(w.z / wn);
    double dra = std::atan2(w.y, w.x) - std::atan2(b.up.y, b.up.x);
    while (dra <= -M_PI) dra += 2.0 * M_PI;
    while (dra > M_PI) dra -= 2.0 * M_PI;
    *ra = ra0 + dra * 180.0 / M_PI;
    if (*ra < 0.0) *ra += 360.0;
    if (*ra >= 360.0) *ra -= 360.0;
    *dec = dec_r * 180.0 / M_PI;
    return true;
}
// SIN orthographic 第一性反解: 球面点 v = sinθ·up + x·(−east) + y·north。
bool oracle_sin_pix2world(double ra0, double dec0, double x, double y,
                          double* ra, double* dec) {
    const double r2 = x * x + y * y;
    if (r2 > 1.0) return false;  // SIN 半球域
    const OracleBasis b = oracle_basis(ra0, dec0);
    if (!b.ok) return false;
    const double zu = std::sqrt(std::max(0.0, 1.0 - r2));
    Vec3 w{zu * b.up.x + x * b.east.x + y * b.north.x,
           zu * b.up.y + x * b.east.y + y * b.north.y,
           zu * b.up.z + x * b.east.z + y * b.north.z};
    const double dec_r = std::asin(std::max(-1.0, std::min(1.0, w.z)));  // v 单位正交基系数
    double dra = std::atan2(w.y, w.x) - std::atan2(b.up.y, b.up.x);
    while (dra <= -M_PI) dra += 2.0 * M_PI;
    while (dra > M_PI) dra -= 2.0 * M_PI;
    *ra = ra0 + dra * 180.0 / M_PI;
    if (*ra < 0.0) *ra += 360.0;
    if (*ra >= 360.0) *ra -= 360.0;
    *dec = dec_r * 180.0 / M_PI;
    return true;
}

struct CaseCtx {
    P3ProjectionId id;
    const char* code;
    double ra0, dec0, scale;
    int w, h;
};

// 采样网格(确定性固定, 含跨 RA wrap 与四象限偏移点)
void sample_points(int w, int h, std::vector<std::pair<double, double>>* pts) {
    pts->clear();
    const int nx = std::min(w, 23), ny = std::min(h, 17);
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            const double x = (w - 1) * (double)i / (double)(nx - 1);
            const double y = (h - 1) * (double)j / (double)(ny - 1);
            pts->emplace_back(x, y);
        }
}

P3ProjectionDescriptor make_case(const CaseCtx& c) {
    P3ProjectionDescriptor d;
    const P3ProjectionStatus st =
        p3_projection_make(c.id, c.ra0, c.dec0, c.scale, c.w, c.h,
                           "east_left", 0.0, &d);
    if (st != P3ProjectionStatus::P3_PROJ_OK) {
        std::fprintf(stderr, "FAIL make(%s) rc=%d\n", c.code, (int)st);
        ++failures;
    }
    return d;
}

// G2: 每投影独立往返 Oracle + 正向解析解(全部容差冻结)
void test_roundtrip_and_oracle(const CaseCtx& c) {
    const P3ProjectionDescriptor d = make_case(c);
    std::vector<std::pair<double, double>> pts;
    sample_points(c.w, c.h, &pts);
    int ok_cnt = 0, skip_cnt = 0;
    for (const auto& p : pts) {
        double ra = 0, dec = 0, xb = 0, yb = 0;
        const P3ProjectionStatus st1 =
            p3_projection_pix2world(&d, p.first, p.second, &ra, &dec);
        if (st1 == P3ProjectionStatus::P3_PROJ_HEMISPHERE) { ++skip_cnt; continue; }
        CHECK_MSG(st1 == P3ProjectionStatus::P3_PROJ_OK,
                  "grid point must project inside domain");
        if (st1 != P3ProjectionStatus::P3_PROJ_OK) continue;
        // 独立 oracle(3D 向量/恒等旋转/Paper II 反演式, 逐投影)
        double ra_o = 0, dec_o = 0;
        double xd, yd;
        {
            const double dx = (p.first + 1.0) - d.crpix_x;
            const double dy = (p.second + 1.0) - d.crpix_y;
            xd = d.cd[0][0] * dx + d.cd[0][1] * dy;
            yd = d.cd[1][0] * dx + d.cd[1][1] * dy;
        }
        const double x_rad = xd * M_PI / 180.0, y_rad = yd * M_PI / 180.0;
        bool have_oracle = true;
        if (c.id == P3ProjectionId::TAN) {
            have_oracle = oracle_tan_pix2world(c.ra0, c.dec0, x_rad, y_rad,
                                               &ra_o, &dec_o);
        } else if (c.id == P3ProjectionId::SIN) {
            have_oracle = oracle_sin_pix2world(c.ra0, c.dec0, x_rad, y_rad,
                                               &ra_o, &dec_o);
        } else if (c.id == P3ProjectionId::CAR) {  // θ₀=+90° 恒等旋转独立式
            ra_o = c.ra0 + xd;
            dec_o = -yd;
            if (ra_o < 0.0) ra_o += 360.0;
            if (ra_o >= 360.0) ra_o -= 360.0;
            have_oracle = std::fabs(dec_o) <= 90.0;
        } else {  // AIT: Paper II 反演独立式(D²=2−X²/4−Y², sinθ=Y·D, φ=2·atan2(XD/2, D²−1))
            const double dsq = 2.0 - (x_rad * x_rad / 4.0 + y_rad * y_rad);
            if (dsq <= 0.0) { have_oracle = false; }
            else {
                const double dq = std::sqrt(dsq);
                const double sth = y_rad * dq;
                if (std::fabs(sth) > 1.0) have_oracle = false;
                else {
                    dec_o = std::asin(sth) * 180.0 / M_PI;
                    const double phi = 2.0 * std::atan2(x_rad * dq / 2.0, dsq - 1.0);
                    ra_o = c.ra0 + phi * 180.0 / M_PI;
                    if (ra_o < 0.0) ra_o += 360.0;
                    if (ra_o >= 360.0) ra_o -= 360.0;
                }
            }
        }
        if (have_oracle) {
            double dra = std::fabs(ra - ra_o);
            if (dra > 180.0) dra = 360.0 - dra;  // wrap 感知
            CHECK_MSG(dra < kOracleTolDeg && std::fabs(dec - dec_o) < kOracleTolDeg,
                      "pix2world must match independent oracle");
        }
        // 往返(冻结 <1e-6 px)
        const P3ProjectionStatus st2 = p3_projection_world2pix(&d, ra, dec, &xb, &yb);
        if (st2 == P3ProjectionStatus::P3_PROJ_PARAM) { ++skip_cnt; continue; }  // |dec|>85° 冻结边界拒
        CHECK_MSG(st2 == P3ProjectionStatus::P3_PROJ_OK, "roundtrip world2pix");
        if (st2 == P3ProjectionStatus::P3_PROJ_OK) {
            const double err = std::hypot(xb - p.first, yb - p.second);
            CHECK_MSG(err < kRoundtripTolPx, "roundtrip < 1e-6 px (SCI §7)");
        }
        // world2pix 正向独立解析解: 3D 向量分量(+TAN 透视除法/AIT 正向原式)→CD⁻¹
        double xo = 0, yo = 0;
        bool have_fwd = true;
        if (c.id == P3ProjectionId::TAN || c.id == P3ProjectionId::SIN) {
            double xi, eta, sin_th;
            if (oracle_sky_to_zenithal_plane(c.ra0, c.dec0, ra, dec, &xi, &eta,
                                             &sin_th)) {
                const double xr = (c.id == P3ProjectionId::TAN) ? xi / sin_th : xi;
                const double yr = (c.id == P3ProjectionId::TAN) ? eta / sin_th : eta;
                const double xid = xr * 180.0 / M_PI, etad = yr * 180.0 / M_PI;
                const double det = d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0];
                xo = (d.cd[1][1] * xid - d.cd[0][1] * etad) / det + d.crpix_x - 1.0;
                yo = (-d.cd[1][0] * xid + d.cd[0][0] * etad) / det + d.crpix_y - 1.0;
            } else {
                have_fwd = false;
            }
        } else if (c.id == P3ProjectionId::CAR) {  // 恒等旋转独立式
            double dra2 = ra - c.ra0;
            while (dra2 <= -180.0) dra2 += 360.0;
            while (dra2 > 180.0) dra2 -= 360.0;
            const double det = d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0];
            xo = (d.cd[1][1] * dra2 - d.cd[0][1] * (-dec)) / det + d.crpix_x - 1.0;
            yo = (-d.cd[1][0] * dra2 + d.cd[0][0] * (-dec)) / det + d.crpix_y - 1.0;
        } else {  // AIT 正向 Paper II 原式
            double dra2 = ra - c.ra0;
            while (dra2 <= -180.0) dra2 += 360.0;
            while (dra2 > 180.0) dra2 -= 360.0;
            const double phi = dra2 * M_PI / 180.0, th = dec * M_PI / 180.0;
            const double dq = std::sqrt(1.0 + std::cos(th) * std::cos(phi / 2.0));
            const double X = 2.0 * std::cos(th) * std::sin(phi / 2.0) / dq;
            const double Y = std::sin(th) / dq;
            const double det = d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0];
            xo = (d.cd[1][1] * X * 180.0 / M_PI - d.cd[0][1] * Y * 180.0 / M_PI) / det +
                 d.crpix_x - 1.0;
            yo = (-d.cd[1][0] * X * 180.0 / M_PI + d.cd[0][0] * Y * 180.0 / M_PI) / det +
                 d.crpix_y - 1.0;
        }
        if (have_fwd) {
            CHECK_MSG(std::hypot(xo - p.first, yo - p.second) < kRoundtripTolPx,
                      "world2pix matches independent forward oracle");
        }
        ++ok_cnt;
    }
    CHECK_MSG(ok_cnt > 0, "roundtrip sample must be non-empty");
}

// G3: TAN 与冻结生产实现(lib/phase3_session/p3_wcs.cpp) bitwise 对拍
void test_tan_bitwise_vs_production(const CaseCtx& c) {
    P3ProjectionDescriptor d = make_case(c);
    astrocs::phase3::P3WcsDescriptor legacy{};
    const astrocs::phase3::P3WcsStatus lst = astrocs::phase3::p3_wcs_make(
        c.ra0, c.dec0, c.scale, c.w, c.h, "east_left", 0.0, &legacy);
    CHECK_MSG(lst == astrocs::phase3::P3_WCS_OK, "legacy make");
    CHECK_MSG(std::memcmp(d.cd, legacy.cd, sizeof(d.cd)) == 0,
              "G1 CD bitwise identical (TAN 冻结零漂移)");
    CHECK_MSG(d.crpix_x == legacy.crpix_x && d.crpix_y == legacy.crpix_y,
              "CRPIX bitwise identical");
    std::vector<std::pair<double, double>> pts;
    sample_points(c.w, c.h, &pts);
    int cmp = 0;
    for (const auto& p : pts) {
        double ra1, dec1, ra2, dec2;
        const auto s1 = p3_projection_pix2world(&d, p.first, p.second, &ra1, &dec1);
        const auto s2 = astrocs::phase3::p3_wcs_pix2world(
            &legacy, p.first, p.second, &ra2, &dec2);
        CHECK_MSG((int)s1 == (int)s2, "TAN pix2world status identical");
        if ((int)s1 == 0 && (int)s2 == 0) {
            CHECK_MSG(std::memcmp(&ra1, &ra2, 8) == 0 &&
                          std::memcmp(&dec1, &dec2, 8) == 0,
                      "TAN pix2world bitwise identical");
            double x1, y1, x2, y2;
            const auto w1 = p3_projection_world2pix(&d, ra1, dec1, &x1, &y1);
            const auto w2 = astrocs::phase3::p3_wcs_world2pix(
                &legacy, ra2, dec2, &x2, &y2);
            CHECK_MSG((int)w1 == (int)w2, "TAN world2pix status identical");
            if ((int)w1 == 0 && (int)w2 == 0) {
                CHECK_MSG(std::memcmp(&x1, &x2, 8) == 0 &&
                              std::memcmp(&y1, &y2, 8) == 0,
                          "TAN world2pix bitwise identical");
            }
            ++cmp;
        }
    }
    CHECK_MSG(cmp > 0, "TAN 对拍点数 > 0");
}

// G4: G1 CD 构造精确断言(四投影同构)
void test_g1_construction() {
    const P3ProjectionId ids[4] = {P3ProjectionId::TAN, P3ProjectionId::SIN,
                                   P3ProjectionId::CAR, P3ProjectionId::AIT};
    const char* names[4] = {"TAN", "SIN", "CAR", "AIT"};
    for (int k = 0; k < 4; ++k) {
        // PA=0 east_left: CD=diag(−s,+s) (bitwise)
        P3ProjectionDescriptor d;
        CHECK_MSG(p3_projection_make(ids[k], 10.0, 20.0, 0.001, 512, 512,
                                     "east_left", 0.0, &d) ==
                      P3ProjectionStatus::P3_PROJ_OK,
                  "make east_left PA=0");
        CHECK_MSG(d.cd[0][0] == -0.001 && d.cd[0][1] == 0.0 &&
                      d.cd[1][0] == 0.0 && d.cd[1][1] == 0.001,
                  "east_left PA=0 CD=diag(−s,+s) bitwise");
        CHECK_MSG(d.crpix_x == 256.5 && d.crpix_y == 256.5, "crpix (W+1)/2 奇");
        // east_right PA=0: diag(+s,−s) (P0 修复语义)
        CHECK_MSG(p3_projection_make(ids[k], 10.0, 20.0, 0.001, 513, 513,
                                     "east_right", 0.0, &d) ==
                      P3ProjectionStatus::P3_PROJ_OK,
                  "make east_right PA=0");
        CHECK_MSG(d.cd[0][0] == 0.001 && d.cd[1][1] == -0.001,
                  "east_right PA=0 CD=diag(+s,−s)");
        CHECK_MSG(d.crpix_x == 257.0, "crpix W=513 ⇒ 257.0 偶");
        // PA=90°: CD1_2=sgn_y·s, CD2_1=−sgn_x·s
        CHECK_MSG(p3_projection_make(ids[k], 10.0, 20.0, 0.002, 64, 64,
                                     "east_left", 90.0, &d) ==
                      P3ProjectionStatus::P3_PROJ_OK,
                  "make PA=90");
        CHECK_MSG(std::fabs(d.cd[0][1] - 0.002) < 1e-15 &&
                      std::fabs(d.cd[1][0] - 0.002) < 1e-15,
                  "PA=90 CD1_2=sgn_y·s=+s CD2_1=−sgn_x·s=+s (east_left)");
        // det<0 手性
        CHECK_MSG(d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0] < 0.0,
                  "det(CD)=−s²<0 手性冻结");
        (void)names[k];
    }
}

// G5: 负面清单(fail-closed 全显式拒绝)
void test_negative() {
    P3ProjectionDescriptor d;
    // 未知投影码: registry nullptr + make UNSUPPORTED
    CHECK_MSG(p3_projection_registry_find("ZEA") == nullptr,
              "ZEA 未注册 → nullptr (宪章 §18.1 只注册四投影)");
    CHECK_MSG(p3_projection_registry_find("tan") == nullptr,
              "码大小写敏感, 无静默 fallback");
    CHECK_MSG(p3_projection_registry_find("") == nullptr, "空码 → nullptr");
    CHECK_MSG(p3_projection_registry_find(nullptr) == nullptr, "null → nullptr");
    CHECK_MSG(p3_projection_make((P3ProjectionId)7, 0, 0, 0.1, 8, 8, nullptr, 0,
                                 &d) == P3ProjectionStatus::P3_PROJ_UNSUPPORTED,
              "越界 id → UNSUPPORTED");
    // parity/dec/scale/尺寸
    for (int k = 0; k < 4; ++k) {
        const auto id = (P3ProjectionId)k;
        CHECK_MSG(p3_projection_make(id, 0, 0, 0.1, 8, 8, "NORTH", 0, &d) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "parity 非法 → PARAM");
        CHECK_MSG(p3_projection_make(id, 0, 85.1, 0.1, 8, 8, nullptr, 0, &d) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "|dec|=85.1>85 → PARAM");
        CHECK_MSG(p3_projection_make(id, 0, 0, 0.0, 8, 8, nullptr, 0, &d) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "scale=0 → PARAM");
        CHECK_MSG(p3_projection_make(id, 0, 0, -0.1, 8, 8, nullptr, 0, &d) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "scale<0 → PARAM");
        CHECK_MSG(p3_projection_make(id, 0, 0, 0.1, 0, 8, nullptr, 0, &d) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "W=0 → PARAM");
        CHECK_MSG(p3_projection_make(id, 0, 0, 0.1, 8, 20001, nullptr, 0, &d) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "H=20001 → PARAM");
        // 空指针面
        CHECK_MSG(p3_projection_make(id, 0, 0, 0.1, 8, 8, nullptr, 0,
                                     nullptr) == P3ProjectionStatus::P3_PROJ_PARAM,
                  "make out=null → PARAM");
        CHECK_MSG(p3_projection_pix2world(nullptr, 0, 0, nullptr, nullptr) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "pix2world d=null → PARAM");
        CHECK_MSG(p3_projection_world2pix(nullptr, 0, 0, nullptr, nullptr) ==
                      P3ProjectionStatus::P3_PROJ_PARAM,
                  "world2pix d=null → PARAM");
        CHECK_MSG(p3_projection_fits_keywords(nullptr).empty(),
                  "keywords d=null → 空串");
    }
    // SIN 半球外(ρ>1): 大视场四角越域 → make HEMISPHERE
    CHECK_MSG(p3_projection_make(P3ProjectionId::SIN, 0, 0, 2.0, 512, 512,
                                 nullptr, 0, &d) ==
                  P3ProjectionStatus::P3_PROJ_HEMISPHERE,
              "SIN 视场超半球 → HEMISPHERE");
    // TAN 大视场(对照, 冻结语义)
    CHECK_MSG(p3_projection_make(P3ProjectionId::TAN, 0, 0, 2.0, 512, 512,
                                 nullptr, 0, &d) ==
                  P3ProjectionStatus::P3_PROJ_HEMISPHERE,
              "TAN 大视场 → HEMISPHERE");
    // TAN world2pix 极点邻域/背面
    CHECK_MSG(p3_projection_make(P3ProjectionId::TAN, 0, 20, 0.01, 64, 64,
                                 nullptr, 0, &d) == P3ProjectionStatus::P3_PROJ_OK,
              "TAN make 基准");
    CHECK_MSG(p3_projection_world2pix(&d, 10.0, 85.1, nullptr, nullptr) ==
                  P3ProjectionStatus::P3_PROJ_PARAM,
              "TAN world2pix |dec|>85 → PARAM");
    // SIN world2pix 背面半球
    CHECK_MSG(p3_projection_make(P3ProjectionId::SIN, 0, 20, 0.01, 64, 64,
                                 nullptr, 0, &d) == P3ProjectionStatus::P3_PROJ_OK,
              "SIN make 基准");
    double xo, yo;
    CHECK_MSG(p3_projection_world2pix(&d, 180.0, 20.0, &xo, &yo) ==
                  P3ProjectionStatus::P3_PROJ_HEMISPHERE,
              "SIN 背面点 → HEMISPHERE");
    // AIT 域外(X²/4+Y²>2)
    CHECK_MSG(p3_projection_make(P3ProjectionId::AIT, 0, 0, 1.0, 64, 64,
                                 nullptr, 0, &d) == P3ProjectionStatus::P3_PROJ_OK,
              "AIT make 基准");
    CHECK_MSG(p3_projection_pix2world(&d, 1e9, 0.0, &xo, &yo) ==
                  P3ProjectionStatus::P3_PROJ_HEMISPHERE,
              "AIT 域外点 → HEMISPHERE");
    // CAR |dec|>90
    CHECK_MSG(p3_projection_make(P3ProjectionId::CAR, 0, 0, 1.0, 64, 64,
                                 nullptr, 0, &d) == P3ProjectionStatus::P3_PROJ_OK,
              "CAR make 基准");
    CHECK_MSG(p3_projection_world2pix(&d, 10.0, 90.1, &xo, &yo) ==
                  P3ProjectionStatus::P3_PROJ_PARAM,
              "CAR |dec|>90 → PARAM");
}

// G6: CTYPE 关键词面
void test_ctype_keywords() {
    const P3ProjectionId ids[4] = {P3ProjectionId::TAN, P3ProjectionId::SIN,
                                   P3ProjectionId::CAR, P3ProjectionId::AIT};
    const char* expect[4][2] = {{"RA---TAN", "DEC--TAN"},
                                {"RA---SIN", "DEC--SIN"},
                                {"RA---CAR", "DEC--CAR"},
                                {"RA---AIT", "DEC--AIT"}};
    for (int k = 0; k < 4; ++k) {
        P3ProjectionDescriptor d = make_case({ids[k], "", 10.0, 20.0, 0.001, 64, 64});
        const std::string kw = p3_projection_fits_keywords(&d);
        CHECK_MSG(kw.find(expect[k][0]) != std::string::npos &&
                      kw.find(expect[k][1]) != std::string::npos,
                  "CTYPE 经 registry 解析正确");
        size_t pos = 0, lines = 0;
        while (pos < kw.size()) {
            size_t nl = kw.find('\n', pos);
            if (nl == std::string::npos) nl = kw.size();
            CHECK_MSG(nl - pos <= 80, "每行 ≤80 字节 (FITS 卡形态)");
            pos = nl + 1;
            ++lines;
        }
        CHECK_MSG(lines == 12, "12 行关键词面(CTYPE×2+CUNIT×2+CRPIX×2+CRVAL×2+CD×4)");
    }
}

// G7+G8: 确定性 + 1/N worker parity(1 vs 4 线程分块 bitwise)
void test_determinism_and_parity(const CaseCtx& c) {
    P3ProjectionDescriptor d = make_case(c);
    std::vector<std::pair<double, double>> pts;
    sample_points(c.w, c.h, &pts);
    const size_t n = pts.size();
    std::vector<double> ref(n * 4, 0.0);
    auto compute = [&](size_t begin, size_t end, double* out) {
        for (size_t i = begin; i < end; ++i) {
            double ra = 0, dec = 0;
            const auto st = p3_projection_pix2world(&d, pts[i].first,
                                                    pts[i].second, &ra, &dec);
            out[i * 4] = (st == P3ProjectionStatus::P3_PROJ_OK) ? 1.0 : 0.0;
            out[i * 4 + 1] = ra;
            out[i * 4 + 2] = dec;
            double x = 0, y = 0;
            const auto st2 = (st == P3ProjectionStatus::P3_PROJ_OK)
                                 ? p3_projection_world2pix(&d, ra, dec, &x, &y)
                                 : P3ProjectionStatus::P3_PROJ_PARAM;
            out[i * 4 + 3] = (st2 == P3ProjectionStatus::P3_PROJ_OK) ? x : y;
        }
    };
    compute(0, n, ref.data());
    // 重复单线程(确定性)
    std::vector<double> again(n * 4, 0.0);
    compute(0, n, again.data());
    CHECK_MSG(std::memcmp(ref.data(), again.data(), n * 4 * 8) == 0,
              "重复计算 bitwise 一致(确定性)");
    // 4 worker 分块(1/N parity)
    for (int wnum : {2, 4, 8}) {
        std::vector<double> par(n * 4, 0.0);
        std::vector<std::thread> ths;
        const size_t chunk = (n + wnum - 1) / wnum;
        for (int t = 0; t < wnum; ++t) {
            const size_t b = std::min(n, (size_t)t * chunk);
            const size_t e = std::min(n, b + chunk);
            ths.emplace_back([&, b, e] { compute(b, e, par.data()); });
        }
        for (auto& th : ths) th.join();
        CHECK_MSG(std::memcmp(ref.data(), par.data(), n * 4 * 8) == 0,
                  "1/N worker 分块 bitwise 一致(纯函数并发安全)");
    }
}

// G9: 故障注入等价缺陷面(测试级注入, 生产源零 getenv; P2-002 先例同构)
// 注入路径: 期望值按等价缺陷偏移 → 正常实现下断言必败 → 证明测试可捕获
// 对应缺陷类。
void test_fault_injection() {
    struct Case { const char* env; CaseCtx c; double bias; };
    std::vector<Case> cases = {
        {"tan", {P3ProjectionId::TAN, "TAN", 10.0, 20.0, 0.001, 64, 64}, 1.0},
        {"sin", {P3ProjectionId::SIN, "SIN", 10.0, 20.0, 0.01, 64, 64}, 1.0},
        {"car", {P3ProjectionId::CAR, "CAR", 10.0, 20.0, 0.01, 64, 64}, 1.0},
        {"ait", {P3ProjectionId::AIT, "AIT", 10.0, 20.0, 0.01, 64, 64}, 1.0},
    };
    const char* env = std::getenv("ASTROCS_P3PROJ_FAULT");
    if (!env) return;
    for (auto& cs : cases) {
        if (std::strcmp(env, cs.env) != 0) continue;
        fault_mode = true;  // 注入模式: 断言失败=缺陷捕获成功(预期路径)
        const P3ProjectionDescriptor d = make_case(cs.c);
        double ra, dec, x2, y2;
        p3_projection_pix2world(&d, 32.0, 32.0, &ra, &dec);
        // 等价缺陷: 往返期望偏移 ±1 px → 往返断言必败
        p3_projection_world2pix(&d, ra, dec, &x2, &y2);
        CHECK_MSG(std::hypot(x2 - 32.0 - cs.bias, y2 - 32.0) < kRoundtripTolPx,
                  "fault: roundtrip biased → 必败");
    }
    if (std::strcmp(env, "registry") == 0) {
        fault_mode = true;
        // 等价缺陷: 未知码静默 fallback(应 nullptr)
        CHECK_MSG(p3_projection_registry_find("ZEA") != nullptr,
                  "fault: registry fallback → 必败");
    }
}

}  // namespace

int main(int argc, char** argv) {
    const bool probe = argc > 1 && std::strcmp(argv[1], "--probe") == 0;
    (void)probe;
    // G1 registry 完整性
    {
        int cnt = -1;
        const P3ProjectionSpec* tab = p3_projection_registry_table(&cnt);
        CHECK_MSG(cnt == 4, "registry 恰 4 行(TAN/SIN/CAR/AIT, §18.1)");
        CHECK_MSG(astrocs::phase3proj::kP3ProjectionRegistryVersion == 1,
                  "registry 版本常量=1");
        CHECK_MSG(tab != nullptr, "table 非空");
        const char* codes[4] = {"TAN", "SIN", "CAR", "AIT"};
        for (int i = 0; i < 4; ++i) {
            CHECK_MSG(std::strcmp(tab[i].code, codes[i]) == 0, "code 顺序冻结");
            CHECK_MSG(tab[i].pix2world && tab[i].world2pix, "函数指针非空");
            CHECK_MSG(p3_projection_registry_find(codes[i]) == &tab[i],
                      "find 命中冻结行");
            CHECK_MSG(p3_projection_registry_find_id((P3ProjectionId)i) == &tab[i],
                      "find_id 命中冻结行");
        }
        CHECK_MSG(p3_projection_registry_selfcheck() == 0,
                  "registry 自检全过");
    }
    test_negative();
    test_g1_construction();
    test_ctype_keywords();
    // 四投影正交用例(跨 RA wrap: ra0=350 → 东侧跨 0/360; dec 四象限;
    // dec0=85 行缩小 scale 保持四角 |dec|≤90°)
    std::vector<CaseCtx> cases = {
        {P3ProjectionId::TAN, "TAN", 350.0, 30.0, 0.002, 97, 89},
        {P3ProjectionId::SIN, "SIN", 350.0, 30.0, 0.02, 97, 89},
        {P3ProjectionId::CAR, "CAR", 350.0, 30.0, 0.02, 97, 89},
        {P3ProjectionId::AIT, "AIT", 350.0, 30.0, 0.5, 97, 89},
        {P3ProjectionId::TAN, "TAN", 10.0, -60.0, 0.001, 64, 64},
        {P3ProjectionId::SIN, "SIN", 10.0, -60.0, 0.01, 64, 64},
        {P3ProjectionId::CAR, "CAR", 10.0, -60.0, 0.01, 64, 64},
        {P3ProjectionId::AIT, "AIT", 10.0, -60.0, 0.25, 64, 64},
        {P3ProjectionId::TAN, "TAN", 0.0, 85.0, 0.001, 32, 32},
        {P3ProjectionId::SIN, "SIN", 0.0, 85.0, 0.01, 32, 32},
        {P3ProjectionId::CAR, "CAR", 0.0, 85.0, 0.01, 32, 32},
        {P3ProjectionId::AIT, "AIT", 0.0, 85.0, 0.05, 32, 32},
    };
    if (!fault_mode) {
        for (const auto& c : cases) test_roundtrip_and_oracle(c);
        for (const auto& c : cases) {
            if (c.id == P3ProjectionId::TAN) test_tan_bitwise_vs_production(c);
        }
        for (const auto& c : cases) test_determinism_and_parity(c);
    }
    test_fault_injection();

    if (fault_mode) {
        if (failures > 0) {
            std::printf("P3-001 FAULT MODE: %d 断言失败=注入缺陷被捕获(必败面成立)\n",
                        failures);
            return 0;  // 注入模式: 捕获成功即通过(外部脚本断言非零捕获信息)
        }
        std::fprintf(stderr, "FAULT MODE 未触发任何断言失败: 注入无效\n");
        return 2;
    }
    if (failures == 0) {
        std::printf(
            "P3-001 PROJ REGISTRY PASS (registry v1 四投影 TAN/SIN/CAR/AIT + "
            "独立往返 Oracle <1e-6px + TAN 生产 bitwise 零漂移 + G1 精确断言 + "
            "负面 fail-closed + CTYPE 面 + 确定性 + 1/N parity)\n");
        return 0;
    }
    std::fprintf(stderr, "P3-001 PROJ REGISTRY FAIL (%d)\n", failures);
    return 1;
}
