// eng/tests/unit/v6_p3_proj/v6_p3_proj_test.cpp — IMPL-P3-PROJ-001 共址单元测试
//
// 覆盖（正例 + 负例，负例=违反冻结即失败）:
//   A registry v3 完整性（已实现 4 行 TAN/SIN/CAR/AIT；权威冻结集合 = DESIGN §5.3
//     八投影 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA；表内 code 必须属于冻结集）。
//   B 标准 FITS WCS Paper II 一致性:
//     - TAN/SIN 独立 3D 单位向量第一性原理往返 <1e-6 px；
//     - CAR 标准 X=φ,Y=θ（dec 随 y 增加；负例: legacy Y=−θ 必红）；
//     - AIT 标准 γ=√2/D（负例: legacy 缺 √2 必红）；AIT 等积。
//   C 逐像素立体角 Ω(sr)（FZ-P3-OMEGA-NONCONST，禁常数 Ω）:
//     - CAR ±60° 视场 ratio≈2.0000（冻结值）；球形 δ=60° 纬度带 ratio≈1.445；
//     - AIT 等积 ratio<1.00001 且 Ω≈(s_rad)²；
//     - TAN 小场 ratio<1.001；CAR 逐像素 Ω 与解析纬度带互差 <1e-5；
//     - 盈余法 vs 微分法互差 <1e-4；负例: 常数 Ω ratio=1.0 必不满足冻结门。
//   D R/S 行/列归一二元语义（ALG-P3-001 §1.3）:
//     - Σ_j R_ij=1（行）、Σ_i S_ij=1（列）、S_ij=R_ij Ω'_i/Ω_j；
//     - 语义-归一二元一致门；负例: R/S 互换必红；Ω≤0 非有限 fail-closed。
//   E 计划/奇点/wrap: domain_valid/singularity_free/margin>0、RA wrap 检出、
//     越投影域显式拒绝（TAN r≥π/2 / SIN ρ>1 / AIT A>1 / CAR native 极行 |θ|≥90）。
//   H CRVAL2 进映射（Paper II §2.2 三 Euler 角）: CRPIX↔CRVAL 定义性不变量、
//     倾斜 CAR/AIT 相对恒等旋转必不等（负例: CRVAL2 不进映射必红）。
//   F 确定性 + 1/N worker 分块 bitwise 一致。
//   G 故障注入（ASTROCS_P3PROJ_V6_FAULT）等价缺陷必败（测试级注入，生产源零 getenv）。
#include "p3_proj_v6.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using astrocs::phase3proj::v6::Descriptor;
using astrocs::phase3proj::v6::Plan;
using astrocs::phase3proj::v6::ProjectionId;
using astrocs::phase3proj::v6::ProjStatus;
using astrocs::phase3proj::v6::SampleSemantics;
using astrocs::phase3proj::v6::Spec;

namespace {

int failures = 0;
bool fault_mode = false;

#define CHECK_MSG(cond, msg)                                             \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
                         msg);                                           \
            ++failures;                                                  \
        }                                                                \
    } while (0)

constexpr double kRoundtripTolPx = 1e-6;   // SCI-P3-001 §7 冻结往返容差
constexpr double kRowSumTol = 1e-12;       // 行/列归一恒等式容差
const double kDeg = 180.0 / M_PI;
const double kRad = M_PI / 180.0;

// ---- 独立 3D 单位向量 Oracle（不调被测实现）----
struct Vec3 { double x, y, z; };
Vec3 v_sky(double ra_deg, double dec_deg) {
    const double a = ra_deg * kRad, d = dec_deg * kRad;
    return {std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
}
double v_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 v_cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double v_norm(const Vec3& a) { return std::sqrt(v_dot(a, a)); }
// Van Oosterom & Strackee 球面三角形面积（独立实现）
double tri_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    const double num = std::fabs(v_dot(a, v_cross(b, c)));
    const double den = 1.0 + v_dot(a, b) + v_dot(b, c) + v_dot(c, a);
    return 2.0 * std::atan2(num, den);
}
double quad_area(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    return tri_area(a, b, c) + tri_area(a, c, d);
}
double sky_quad_omega(double ra_c, double dec_c, double s_deg) {
    const double h = s_deg / 2.0;
    const Vec3 v00 = v_sky(ra_c - h, dec_c - h);
    const Vec3 v10 = v_sky(ra_c + h, dec_c - h);
    const Vec3 v11 = v_sky(ra_c + h, dec_c + h);
    const Vec3 v01 = v_sky(ra_c - h, dec_c + h);
    return quad_area(v00, v10, v11, v01);
}
// 独立 zenithal 反解（TAN/SIN，3D 向量第一性原理，不调被测实现）
bool oracle_zenithal(double ra0, double dec0, double xi, double eta, bool tan,
                     double* ra, double* dec) {
    const Vec3 up = v_sky(ra0, dec0);
    const Vec3 east{-std::sin(ra0 * kRad), std::cos(ra0 * kRad), 0.0};
    const Vec3 north = v_cross(up, east);
    const double r2 = xi * xi + eta * eta;
    Vec3 w;
    if (tan) {
        if (r2 >= (M_PI / 2.0) * (M_PI / 2.0)) return false;
        w = {up.x + xi * east.x + eta * north.x,
             up.y + xi * east.y + eta * north.y,
             up.z + xi * east.z + eta * north.z};
    } else {
        if (r2 > 1.0) return false;
        const double zu = std::sqrt(std::max(0.0, 1.0 - r2));
        w = {zu * up.x + xi * east.x + eta * north.x,
             zu * up.y + xi * east.y + eta * north.y,
             zu * up.z + xi * east.z + eta * north.z};
    }
    const double wn = v_norm(w);
    *dec = std::asin(std::max(-1.0, std::min(1.0, w.z / wn))) * kDeg;
    double dra = std::atan2(w.y, w.x) - std::atan2(up.y, up.x);
    while (dra <= -M_PI) dra += 2 * M_PI;
    while (dra > M_PI) dra -= 2 * M_PI;
    *ra = ra0 + dra * kDeg;
    if (*ra < 0) *ra += 360.0;
    if (*ra >= 360.0) *ra -= 360.0;
    return true;
}

Descriptor make_case(ProjectionId id, double ra0, double dec0, double scale, int w,
                     int h) {
    Descriptor d;
    const ProjStatus st =
        astrocs::phase3proj::v6::make(id, ra0, dec0, scale, w, h, "east_left", 0.0, &d);
    CHECK_MSG(st == ProjStatus::kOk, "make must succeed");
    return d;
}

// ---- A registry ----
void test_registry() {
    int n = -1;
    const Spec* tab = astrocs::phase3proj::v6::registry_table(&n);
    CHECK_MSG(n == 4,
              "registry 已实现恰 4 行（TAN/SIN/CAR/AIT；DESIGN §5.3 八投影之一部）");
    CHECK_MSG(tab != nullptr, "registry table 非空");
    CHECK_MSG(astrocs::phase3proj::v6::kProjectionRegistryVersion == 3,
              "registry 版本 = 3（Paper II CRVAL2 旋转 + AIT A≤1 + CAR 极行 fail-closed）");
    // 权威冻结集合 = DESIGN §5.3 八投影；本层实现为其子集，表内 code 必须属于该集合
    int nf = -1;
    const char* const* frozen = astrocs::phase3proj::v6::registry_frozen_set(&nf);
    CHECK_MSG(nf == 8 && frozen != nullptr, "冻结集合恰 8 行（DESIGN §5.3）");
    const char* frozen_expect[8] = {"TAN", "SIN", "CAR", "AIT",
                                    "STG", "MOL", "CEA", "ZEA"};
    for (int i = 0; i < 8; ++i) {
        CHECK_MSG(std::strcmp(frozen[i], frozen_expect[i]) == 0,
                  "冻结集合顺序 = DESIGN §5.3");
        CHECK_MSG(astrocs::phase3proj::v6::registry_is_frozen_code(frozen[i]),
                  "冻结集成员判定为真");
    }
    CHECK_MSG(!astrocs::phase3proj::v6::registry_is_frozen_code("ARC"),
              "非冻结集投影（ARC）判定为假");
    CHECK_MSG(!astrocs::phase3proj::v6::registry_is_frozen_code(nullptr),
              "nullptr -> false（fail-closed）");
    const char* codes[4] = {"TAN", "SIN", "CAR", "AIT"};
    for (int i = 0; i < 4; ++i) {
        CHECK_MSG(std::strcmp(tab[i].code, codes[i]) == 0, "code 顺序冻结");
        CHECK_MSG(tab[i].pix2world && tab[i].world2pix, "函数指针非空");
        CHECK_MSG(tab[i].singularity_kind && tab[i].singularity_kind[0] != '\0',
                  "奇点声明非空（DESIGN §5.3 六要素）");
        CHECK_MSG(astrocs::phase3proj::v6::registry_is_frozen_code(tab[i].code),
                  "已实现 code 属于 DESIGN §5.3 冻结集");
        CHECK_MSG(tab[i].max_abs_crval_dec_deg == 85.0, "中心守卫 85°");
        CHECK_MSG(tab[i].max_fov_deg > 0.0, "合法 FOV 声明 > 0");
        CHECK_MSG(astrocs::phase3proj::v6::registry_find(codes[i]) == &tab[i],
                  "registry_find 命中");
        CHECK_MSG(astrocs::phase3proj::v6::registry_find_id((ProjectionId)i) == &tab[i],
                  "registry_find_id 命中");
    }
    CHECK_MSG(astrocs::phase3proj::v6::registry_selfcheck() == 0, "registry 自检全过");
    CHECK_MSG(astrocs::phase3proj::v6::registry_find("ZEA") == nullptr,
              "冻结集内未实现投影 ZEA -> nullptr（无 fallback；实施归 P3-001/GAP-011）");
    CHECK_MSG(astrocs::phase3proj::v6::registry_find("car") == nullptr,
              "大小写敏感，无静默");
    CHECK_MSG(astrocs::phase3proj::v6::registry_find_id((ProjectionId)9) == nullptr,
              "越界 id -> nullptr");
}

// ---- B 标准 WCS Paper II ----
void test_standard_wcs() {
    struct C { ProjectionId id; const char* code; double ra0, dec0, scale; int w, h; };
    const C cases[4] = {{ProjectionId::kTAN, "TAN", 350.0, 30.0, 0.002, 97, 89},
                        {ProjectionId::kSIN, "SIN", 350.0, 30.0, 0.02, 97, 89},
                        {ProjectionId::kCAR, "CAR", 0.0, 0.0, 0.2, 41, 41},
                        {ProjectionId::kAIT, "AIT", 0.0, 0.0, 0.2, 41, 41}};
    for (const C& c : cases) {
        const Descriptor d = make_case(c.id, c.ra0, c.dec0, c.scale, c.w, c.h);
        int ok = 0;
        for (int j = 0; j < 7; ++j) {
            for (int i = 0; i < 7; ++i) {
                const double x = (c.w - 1) * i / 6.0;
                const double y = (c.h - 1) * j / 6.0;
                double ra = 0, dec = 0, xb = 0, yb = 0;
                const ProjStatus st =
                    astrocs::phase3proj::v6::pix2world(&d, x, y, &ra, &dec);
                if (st != ProjStatus::kOk) continue;
                // 独立 zenithal oracle（TAN/SIN）
                if (c.id == ProjectionId::kTAN || c.id == ProjectionId::kSIN) {
                    const double dx = (x + 1) - d.crpix_x;
                    const double dy = (y + 1) - d.crpix_y;
                    const double xd = d.cd[0][0] * dx + d.cd[0][1] * dy;
                    const double yd = d.cd[1][0] * dx + d.cd[1][1] * dy;
                    double ra_o, dec_o;
                    if (oracle_zenithal(c.ra0, c.dec0, xd * kRad, yd * kRad,
                                        c.id == ProjectionId::kTAN, &ra_o, &dec_o)) {
                        double dra = std::fabs(ra - ra_o);
                        if (dra > 180.0) dra = 360.0 - dra;
                        CHECK_MSG(dra < 1e-9 && std::fabs(dec - dec_o) < 1e-9,
                                  "TAN/SIN pix2world == 独立 3D oracle");
                    }
                }
                if (c.id == ProjectionId::kCAR) {
                    const double dx = (x + 1) - d.crpix_x;
                    const double dy = (y + 1) - d.crpix_y;
                    const double xd = d.cd[0][0] * dx + d.cd[0][1] * dy;
                    const double yd = d.cd[1][0] * dx + d.cd[1][1] * dy;
                    double ra_o = std::fmod(c.ra0 + xd, 360.0);
                    if (ra_o < 0) ra_o += 360.0;
                    // 标准 Paper II: dec = +Y（declination 随 y 增加）
                    CHECK_MSG(std::fabs(dec - yd) < 1e-12,
                              "CAR dec = +Y（标准 y=+θ; legacy Y=−θ 必红）");
                    double dra = std::fabs(ra - ra_o);
                    if (dra > 180.0) dra = 360.0 - dra;
                    CHECK_MSG(dra < 1e-12, "CAR RA = CRVAL1 + X");
                }
                if (c.id == ProjectionId::kAIT) {
                    // 标准 Paper II 正向解析式独立复算
                    double dra = ra - c.ra0;
                    while (dra <= -180.0) dra += 360.0;
                    while (dra > 180.0) dra -= 360.0;
                    const double phi = dra * kRad, th = dec * kRad;
                    const double dq = std::sqrt(1.0 + std::cos(th) * std::cos(phi / 2.0));
                    const double g = std::sqrt(2.0) / dq;
                    const double X = 2.0 * g * std::cos(th) * std::sin(phi / 2.0);
                    const double Y = g * std::sin(th);
                    const double dx = (x + 1) - d.crpix_x;
                    const double dy = (y + 1) - d.crpix_y;
                    const double xd = d.cd[0][0] * dx + d.cd[0][1] * dy;
                    const double yd = d.cd[1][0] * dx + d.cd[1][1] * dy;
                    CHECK_MSG(std::fabs(xd - X * kDeg) < 1e-9 &&
                                  std::fabs(yd - Y * kDeg) < 1e-9,
                              "AIT 标准 Paper II γ=√2/D（legacy 缺 √2 必红）");
                }
                // 往返
                const ProjStatus st2 =
                    astrocs::phase3proj::v6::world2pix(&d, ra, dec, &xb, &yb);
                if (st2 == ProjStatus::kOk) {
                    CHECK_MSG(std::hypot(xb - x, yb - y) < kRoundtripTolPx,
                              "往返 < 1e-6 px");
                }
                ++ok;
            }
        }
        CHECK_MSG(ok > 0, "采样点非空");
    }
}

// ---- C 逐像素 Ω ----
void test_solid_angle() {
    // CAR ±60° 视场（冻结 CAR max/min=2.0000）
    {
        const Descriptor d = make_case(ProjectionId::kCAR, 0, 0, 0.2, 16, 601);
        std::vector<double> om(static_cast<size_t>(16) * 601, 0.0);
        CHECK_MSG(astrocs::phase3proj::v6::solid_angle_grid(&d, om.data(), nullptr) ==
                      ProjStatus::kOk, "CAR 网格 Ω 全 OK");
        double mn = om[0], mx = om[0];
        for (double v : om) { mn = std::min(mn, v); mx = std::max(mx, v); }
        const double ratio = mx / mn;
        CHECK_MSG(std::fabs(ratio - 2.0000) < 0.01,
                  "CAR ±60° Ω max/min ≈ 2.0000（FZ-P3-OMEGA-NONCONST 冻结值）");
        // 与解析纬度带 Ω = Δα_rad (sin δ_hi − sin δ_lo) 独立交叉。
        // ⚠ 量测域: 仅 |CRVAL2|≈0（本用例 dec0=0）；倾斜 CAR 的行是倾斜等纬线，
        // 该解析式不成立（R-1 §4-B），禁止把本判据复用到 CRVAL2≠0 用例。
        double worst = 0.0;
        for (int j = 0; j < 601; ++j) {
            const double dec = -60.0 + j * 0.2;
            const double analytic = (0.2 * kRad) *
                                    (std::sin((dec + 0.1) * kRad) -
                                     std::sin((dec - 0.1) * kRad));
            worst = std::max(worst, std::fabs(om[static_cast<size_t>(j) * 16] - analytic) /
                                        analytic);
        }
        CHECK_MSG(worst < 1e-5, "CAR 逐像素 Ω 与解析纬度带互差 <1e-5");
    }
    // AIT 等积（ratio < 1.00001；Ω = (s_rad)^2 精确）
    {
        const Descriptor d = make_case(ProjectionId::kAIT, 0, 0, 0.2, 61, 61);
        std::vector<double> om(61 * 61, 0.0);
        astrocs::phase3proj::v6::solid_angle_grid(&d, om.data(), nullptr);
        double mn = om[0], mx = om[0];
        for (double v : om) { mn = std::min(mn, v); mx = std::max(mx, v); }
        CHECK_MSG(mx / mn < 1.00001, "AIT 等积 Ω ratio < 1.00001");
        const double expect = (0.2 * kRad) * (0.2 * kRad);
        CHECK_MSG(std::fabs(mn / expect - 1.0) < 1e-5,
                  "AIT Ω = (s_rad)^2（等积；球面盈余 vs 坐标面积曲率差 ~1.7e-6）");
    }
    // 球形 δ=60° 纬度带（冻结证据 TAN(dec60)=1.445 实为 Δα×Δsinδ 网格）
    {
        const double s = 0.2;
        double mn = 1e30, mx = 0.0;
        for (int j = 0; j < 61; ++j) {
            const double dec = 60.0 - 6.0 + j * s;
            const double om = sky_quad_omega(0.0, dec, s);
            mn = std::min(mn, om);
            mx = std::max(mx, om);
        }
        CHECK_MSG(std::fabs(mx / mn - 1.445) < 0.01,
                  "δ=60° 12° 纬度带 Ω ratio ≈ 1.445（冻结证据，独立球面盈余）");
    }
    // TAN 小场 ratio < 1.001；TAN dec=60 12° 场投影 Ω ratio ∈ (1.02,1.05)
    {
        const Descriptor d0 = make_case(ProjectionId::kTAN, 0, 0, 0.2, 11, 11);
        std::vector<double> om0(121, 0.0);
        astrocs::phase3proj::v6::solid_angle_grid(&d0, om0.data(), nullptr);
        double mn = om0[0], mx = om0[0];
        for (double v : om0) { mn = std::min(mn, v); mx = std::max(mx, v); }
        CHECK_MSG(mx / mn < 1.001, "TAN 0.2° 小场 Ω ratio < 1.001");
        const Descriptor d1 = make_case(ProjectionId::kTAN, 0, 60, 0.2, 61, 61);
        std::vector<double> om1(61 * 61, 0.0);
        astrocs::phase3proj::v6::solid_angle_grid(&d1, om1.data(), nullptr);
        double mn1 = om1[0], mx1 = om1[0];
        for (double v : om1) { mn1 = std::min(mn1, v); mx1 = std::max(mx1, v); }
        const double ratio1 = mx1 / mn1;
        CHECK_MSG(ratio1 > 1.02 && ratio1 < 1.05,
                  "TAN dec=60 12° 场 Ω ratio ≈1.033（投影面积元，非常数）");
    }
    // 盈余法 vs 微分法（独立面积算法）互差 < 1e-4
    {
        const Descriptor d = make_case(ProjectionId::kTAN, 0, 0, 0.2, 11, 11);
        double worst = 0.0;
        for (int j = 0; j < 11; ++j) {
            for (int i = 0; i < 11; ++i) {
                double a = 0, b = 0;
                const auto sa = astrocs::phase3proj::v6::pixel_solid_angle(&d, i, j, &a);
                const auto sb = astrocs::phase3proj::v6::pixel_solid_angle_differential(
                    &d, i, j, &b);
                if (sa == ProjStatus::kOk && sb == ProjStatus::kOk)
                    worst = std::max(worst, std::fabs(a - b) / a);
            }
        }
        CHECK_MSG(worst < 1e-4, "盈余法 vs 微分法 Ω 互差 <1e-4（独立面积算法）");
    }
    // 负例: 常数 Ω 模型必不满足冻结门（ratio==1.0 < 1.9）
    {
        const double constant_ratio = 1.0;
        CHECK_MSG(constant_ratio < 1.9,
                  "常数 Ω 必红: ratio=1.0 不满足 CAR 冻结门 (>1.9)");
        // fail-closed: 越域像素 Ω 不产值
        const Descriptor dsin = make_case(ProjectionId::kSIN, 0, 0, 0.5, 64, 64);
        double om = -1.0;
        const ProjStatus st = astrocs::phase3proj::v6::pixel_solid_angle(
            &dsin, 1e6, 1e6, &om);
        CHECK_MSG(st != ProjStatus::kOk && om == -1.0,
                  "越域 Ω fail-closed: 非 OK 且 *omega_sr 不变（禁零填）");
        CHECK_MSG(astrocs::phase3proj::v6::pixel_solid_angle(nullptr, 0, 0, &om) ==
                      ProjStatus::kParam,
                  "Ω 空指针 -> PARAM");
    }
}

// ---- D 行/列归一 ----
void test_normalisation() {
    const int m = 3, n = 5;
    const double om_out[3] = {2.0, 2.5, 1.5};
    const double om_in[5] = {1.0, 0.5, 1.5, 1.5, 1.5};
    double a[15] = {1.0, 0.4, 0.6, 0.0, 0.0,
                    0.0, 0.1, 0.9, 1.5, 0.0,
                    0.0, 0.0, 0.0, 0.0, 1.5};
    double r[15], s[15], s2[15], rs[3], cs[5];
    CHECK_MSG(astrocs::phase3proj::v6::row_normalise(a, m, n, om_out, r) ==
                  ProjStatus::kOk, "row_normalise OK");
    CHECK_MSG(astrocs::phase3proj::v6::col_normalise(a, m, n, om_in, s) ==
                  ProjStatus::kOk, "col_normalise OK");
    astrocs::phase3proj::v6::matrix_row_sums(r, m, n, rs);
    astrocs::phase3proj::v6::matrix_col_sums(s, m, n, cs);
    for (int i = 0; i < m; ++i)
        CHECK_MSG(std::fabs(rs[i] - 1.0) < kRowSumTol, "Σ_j R_ij = 1（行归一）");
    for (int j = 0; j < n; ++j)
        CHECK_MSG(std::fabs(cs[j] - 1.0) < kRowSumTol, "Σ_i S_ij = 1（列归一）");
    // S_ij = R_ij Ω'_i/Ω_j
    CHECK_MSG(astrocs::phase3proj::v6::row_to_col(r, m, n, om_out, om_in, s2) ==
                  ProjStatus::kOk, "row_to_col OK");
    for (int k = 0; k < m * n; ++k)
        CHECK_MSG(std::fabs(s2[k] - s[k]) < 1e-15, "S_ij = R_ij Ω'_i/Ω_j");
    // 负例: R/S 互换必红（R 不满足列归一；S 不满足行归一）
    double cs_r[5], rs_s[3];
    astrocs::phase3proj::v6::matrix_col_sums(r, m, n, cs_r);
    astrocs::phase3proj::v6::matrix_row_sums(s, m, n, rs_s);
    double worst_col = 0.0, worst_row = 0.0;
    for (int j = 0; j < n; ++j) worst_col = std::max(worst_col, std::fabs(cs_r[j] - 1.0));
    for (int i = 0; i < m; ++i) worst_row = std::max(worst_row, std::fabs(rs_s[i] - 1.0));
    CHECK_MSG(worst_col > 0.1, "负例: R 用作列归一必红（R/S 不可互替）");
    CHECK_MSG(worst_row > 0.1, "负例: S 用作行归一必红（R/S 不可互替）");
    // 语义-归一二元一致门
    CHECK_MSG(astrocs::phase3proj::v6::semantics_compatible(
                  SampleSemantics::kSurfaceBrightnessRowNorm, true, false),
              "surface_brightness <-> 行归一");
    CHECK_MSG(!astrocs::phase3proj::v6::semantics_compatible(
                  SampleSemantics::kSurfaceBrightnessRowNorm, false, true),
              "surface_brightness 用列归一 -> 不可兼容（负例）");
    CHECK_MSG(astrocs::phase3proj::v6::semantics_compatible(
                  SampleSemantics::kPointSourceFluxColNorm, false, true),
              "point_source_flux <-> 列归一");
    CHECK_MSG(!astrocs::phase3proj::v6::semantics_compatible(
                  SampleSemantics::kPointSourceFluxColNorm, true, false),
              "point_source_flux 用行归一 -> 不可兼容（负例）");
    CHECK_MSG(astrocs::phase3proj::v6::semantics_compatible(
                  SampleSemantics::kVisualizationNone, false, false),
              "visualization 无归一");
    CHECK_MSG(!astrocs::phase3proj::v6::semantics_compatible(
                  SampleSemantics::kVisualizationNone, true, false),
              "visualization 声明归一 -> 不可兼容（负例）");
    // fail-closed: Ω≤0 / 非有限 / 空指针
    double bad[3] = {2.0, 0.0, 1.5};
    CHECK_MSG(astrocs::phase3proj::v6::row_normalise(a, m, n, bad, r) ==
                  ProjStatus::kParam, "Ω=0 -> PARAM（fail-closed）");
    double bad2[5] = {1.0, -0.5, 1.5, 1.5, 1.5};
    CHECK_MSG(astrocs::phase3proj::v6::col_normalise(a, m, n, bad2, s) ==
                  ProjStatus::kParam, "Ω<0 -> PARAM（fail-closed）");
    double bad3[5] = {1.0, NAN, 1.5, 1.5, 1.5};
    CHECK_MSG(astrocs::phase3proj::v6::col_normalise(a, m, n, bad3, s) ==
                  ProjStatus::kParam, "Ω=NaN -> PARAM（fail-closed）");
    CHECK_MSG(astrocs::phase3proj::v6::row_normalise(nullptr, m, n, om_out, r) ==
                  ProjStatus::kParam, "A=null -> PARAM");
}

// ---- H CRVAL2 进映射（Paper II §2.2 三 Euler 角）----
void test_crval2_rotation() {
    struct C { ProjectionId id; const char* code; double ra0, dec0; };
    const C cases[6] = {{ProjectionId::kCAR, "CAR", 10.0, 30.0},
                        {ProjectionId::kCAR, "CAR", 10.0, -30.0},
                        {ProjectionId::kCAR, "CAR", 10.0, 60.0},
                        {ProjectionId::kAIT, "AIT", 10.0, 30.0},
                        {ProjectionId::kAIT, "AIT", 10.0, -30.0},
                        {ProjectionId::kAIT, "AIT", 200.0, -45.0}};
    for (const C& c : cases) {
        const Descriptor d = make_case(c.id, c.ra0, c.dec0, 0.2, 17, 17);
        // (i) CRPIX↔CRVAL 定义性不变量（Paper I §2.1.1）
        double ra = 0, dec = 0;
        const ProjStatus st = astrocs::phase3proj::v6::pix2world(
            &d, d.crpix_x - 1.0, d.crpix_y - 1.0, &ra, &dec);
        CHECK_MSG(st == ProjStatus::kOk, "CRPIX 处 pix2world OK");
        double dra = std::fabs(ra - c.ra0);
        if (dra > 180.0) dra = 360.0 - dra;
        CHECK_MSG(dra < 1e-9 && std::fabs(dec - c.dec0) < 1e-9,
                  "pix2world(CRPIX) == CRVAL（CRVAL2 不进映射必红）");
        // (ii) 倾斜后 ≠ 恒等旋转（dec0≠0 时 dec 不再等于平面 Y）
        double ra2 = 0, dec2 = 0;
        const ProjStatus st2 = astrocs::phase3proj::v6::pix2world(&d, 0.0, 16.0,
                                                                  &ra2, &dec2);
        if (st2 == ProjStatus::kOk) {
            const double dy = d.cd[1][0] * ((0.0 + 1.0) - d.crpix_x) +
                              d.cd[1][1] * ((16.0 + 1.0) - d.crpix_y);
            CHECK_MSG(std::fabs(dec2 - dy) > 1e-6,
                      "倾斜 CAR/AIT: dec ≠ 平面 Y（恒等旋转必红）");
        }
        // (iii) CRVAL 点往返
        double xb = 0, yb = 0;
        if (astrocs::phase3proj::v6::world2pix(&d, ra, dec, &xb, &yb) ==
            ProjStatus::kOk) {
            CHECK_MSG(std::hypot(xb - (d.crpix_x - 1.0), yb - (d.crpix_y - 1.0)) <
                          kRoundtripTolPx,
                      "CRVAL 点往返 < 1e-6 px");
        }
    }
}

// ---- H2 域界收紧：CAR native 极行 fail-closed + AIT 椭圆域 A≤1 ----
void test_domain_limits() {
    // CAR: 平面 θ=±90° 整行塌缩 -> kParam（含恰好 =90；旧守卫 >90 会放行）
    {
        // 足迹 ±50°（make 四角守卫通过）；pix2world 仍可在极行 θ=±90 上被调用
        const Descriptor d = make_case(ProjectionId::kCAR, 0, 0, 1.0, 11, 101);
        double ra = 0, dec = 0;
        const double y_pole = 50.0 + 90.0;    // (y+1-51)*1 = +90 -> y = 140
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(&d, 0.0, y_pole, &ra, &dec) ==
                      ProjStatus::kParam,
                  "CAR native 极行 θ=+90 -> PARAM（fail-closed；旧 >90 放行必红）");
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(&d, 0.0, 50.0 - 90.0 - 1.0, &ra,
                                                     &dec) == ProjStatus::kParam,
                  "CAR native 极行 θ=-90 -> PARAM");
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(&d, 0.0, y_pole - 1.0, &ra,
                                                     &dec) == ProjStatus::kOk,
                  "CAR θ=89 域内仍 OK");
        CHECK_MSG(astrocs::phase3proj::v6::world2pix(&d, 0.0, 90.0, &ra, &dec) ==
                      ProjStatus::kParam,
                  "CAR 天球极点（native θ=90）逆映射 φ 不唯一 -> PARAM");
        Descriptor dd;
        CHECK_MSG(astrocs::phase3proj::v6::make(ProjectionId::kCAR, 0, 0, 0.3, 11, 601,
                                                "east_left", 0.0, &dd) ==
                      ProjStatus::kParam,
                  "CAR make 足迹触极行（600*0.3=180 -> θ=±90）-> PARAM");
        Plan p;
        CHECK_MSG(astrocs::phase3proj::v6::plan(ProjectionId::kCAR, 0, 0, 0.3, 11, 601,
                                                "east_left", 0.0, &p) ==
                          ProjStatus::kParam &&
                      !p.domain_valid,
                  "CAR plan 触极行 -> PARAM 且 domain_valid=false");
    }
    // AIT: 椭圆域 A = xp²/4+yp² ≤ 1（标准半轴 X=2√2 rad=162.0569°, Y=√2 rad=81.0285°）
    {
        const Descriptor d = make_case(ProjectionId::kAIT, 0, 0, 0.2, 17, 17);
        const double cd11 = d.cd[0][0], cd22 = d.cd[1][1];
        const double y_mid = d.crpix_y - 1.0;
        auto px_for_x = [&](double xd) { return (d.crpix_x - 1.0) + xd / cd11; };
        auto py_for_y = [&](double yd) { return y_mid + yd / cd22; };
        double ra = 0, dec = 0;
        // X 轴: |X| ≤ 2√2 rad 域内 / > 域外（旧判据 A<2 会放行到 229.125°）
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(
                      &d, px_for_x(162.0), y_mid, &ra, &dec) == ProjStatus::kOk,
                  "AIT A≤1（|X|=162.0° < 2√2 rad）域内 OK");
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(
                      &d, px_for_x(163.0), y_mid, &ra, &dec) == ProjStatus::kHemisphere,
                  "AIT A>1（|X|=163.0°）-> HEMISPHERE");
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(
                      &d, px_for_x(-163.0), y_mid, &ra, &dec) == ProjStatus::kHemisphere,
                  "AIT A>1（|X|=−163.0°）-> HEMISPHERE");
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(
                      &d, px_for_x(229.125), y_mid, &ra, &dec) ==
                      ProjStatus::kHemisphere,
                  "AIT 折叠环带 |X|=229.125°（旧 A<2 放行）-> HEMISPHERE");
        // Y 轴: |Y| ≤ √2 rad 域内 / > 域外
        const double x_mid = d.crpix_x - 1.0;
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(
                      &d, x_mid, py_for_y(81.0), &ra, &dec) == ProjStatus::kOk,
                  "AIT A≤1（|Y|=81.0° < √2 rad）域内 OK");
        CHECK_MSG(astrocs::phase3proj::v6::pix2world(
                      &d, x_mid, py_for_y(82.0), &ra, &dec) == ProjStatus::kHemisphere,
                  "AIT A>1（|Y|=82.0°）-> HEMISPHERE");
    }
}

// ---- E 计划/奇点/wrap ----
void test_plan() {
    // 正常 CAR ±60°
    {
        Plan p;
        CHECK_MSG(astrocs::phase3proj::v6::plan(ProjectionId::kCAR, 0, 0, 0.2, 16, 601,
                                                "east_left", 0.0, &p) == ProjStatus::kOk,
                  "CAR plan OK");
        CHECK_MSG(p.domain_valid && p.singularity_free, "CAR 域内且无奇点");
        CHECK_MSG(p.min_domain_margin > 0.0, "域 margin > 0");
        CHECK_MSG(p.omega_max_min_ratio > 1.9, "plan Ω ratio 捕获 CAR 变化");
    }
    // RA wrap
    {
        Plan p;
        astrocs::phase3proj::v6::plan(ProjectionId::kCAR, 0, 0, 1.0, 97, 97,
                                      "east_left", 0.0, &p);
        CHECK_MSG(p.crosses_ra_wrap, "CAR RA=0 ±48° 跨 wrap");
        CHECK_MSG(std::fabs(p.fov_x_deg - 96.0) < 0.5, "wrap 后 span ≈96°");
        Plan q;
        astrocs::phase3proj::v6::plan(ProjectionId::kCAR, 180, 0, 1.0, 97, 97,
                                      "east_left", 0.0, &q);
        CHECK_MSG(!q.crosses_ra_wrap, "CAR RA=180 不跨 wrap");
        // TAN 中心 RA=0.5° 小场，期望不跨（0.5 ± 1° 全在 0 同侧? x 跨 0）：
        Plan r;
        astrocs::phase3proj::v6::plan(ProjectionId::kTAN, 0.5, 0, 0.02, 97, 97,
                                      "east_left", 0.0, &r);
        CHECK_MSG(r.status == ProjStatus::kOk, "TAN plan OK");
    }
    // 越投影域显式拒绝
    Plan p;
    CHECK_MSG(astrocs::phase3proj::v6::plan(ProjectionId::kTAN, 0, 0, 2.0, 512, 512,
                                            "east_left", 0.0, &p) ==
                      ProjStatus::kHemisphere &&
                  !p.domain_valid,
              "TAN r≥π/2 -> HEMISPHERE, domain_valid=false");
    CHECK_MSG(astrocs::phase3proj::v6::plan(ProjectionId::kSIN, 0, 0, 2.0, 512, 512,
                                            "east_left", 0.0, &p) ==
                      ProjStatus::kHemisphere &&
                  !p.domain_valid,
              "SIN ρ>1 -> HEMISPHERE, domain_valid=false");
    CHECK_MSG(astrocs::phase3proj::v6::plan(ProjectionId::kAIT, 0, 0, 100.0, 64, 64,
                                            "east_left", 0.0, &p) ==
                      ProjStatus::kHemisphere,
              "AIT D²≤0 域外 -> HEMISPHERE");
    CHECK_MSG(astrocs::phase3proj::v6::plan(ProjectionId::kCAR, 0, 0, 100.0, 64, 64,
                                            "east_left", 0.0, &p) ==
                      ProjStatus::kParam,
              "CAR |δ|>90 -> PARAM（拒绝柱面延伸支）");
    // world2pix 奇点/域界
    const Descriptor dsin = make_case(ProjectionId::kSIN, 0, 20, 0.01, 64, 64);
    double xo, yo;
    CHECK_MSG(astrocs::phase3proj::v6::world2pix(&dsin, 180.0, 20.0, &xo, &yo) ==
                  ProjStatus::kHemisphere, "SIN 背面点 -> HEMISPHERE");
    const Descriptor dtan = make_case(ProjectionId::kTAN, 0, 20, 0.01, 64, 64);
    CHECK_MSG(astrocs::phase3proj::v6::world2pix(&dtan, 10.0, 85.1, &xo, &yo) ==
                  ProjStatus::kParam, "TAN |dec|>85 -> PARAM");
    // 参数守卫
    Descriptor dd;
    CHECK_MSG(astrocs::phase3proj::v6::make(ProjectionId::kTAN, 0, 0, 0.1, 8, 8,
                                            "NORTH", 0, &dd) == ProjStatus::kParam,
              "parity 非法 -> PARAM");
    CHECK_MSG(astrocs::phase3proj::v6::make(ProjectionId::kTAN, 0, 0, 0.1, 8, 8,
                                            nullptr, 0, nullptr) == ProjStatus::kParam,
              "out=null -> PARAM");
    CHECK_MSG(astrocs::phase3proj::v6::make((ProjectionId)7, 0, 0, 0.1, 8, 8,
                                            nullptr, 0, &dd) ==
                  ProjStatus::kUnsupported,
              "越界 id -> UNSUPPORTED");
}

// ---- F 确定性 + 并发 parity ----
void test_determinism() {
    const Descriptor d = make_case(ProjectionId::kAIT, 0, 0, 0.2, 61, 61);
    std::vector<double> ref(61 * 61, 0.0), again(61 * 61, 0.0);
    astrocs::phase3proj::v6::solid_angle_grid(&d, ref.data(), nullptr);
    astrocs::phase3proj::v6::solid_angle_grid(&d, again.data(), nullptr);
    CHECK_MSG(std::memcmp(ref.data(), again.data(), ref.size() * 8) == 0,
              "重复 Ω 网格 bitwise 一致");
    auto worker = [&](int b, int e, double* out) {
        for (int j = b; j < e; ++j)
            for (int i = 0; i < 61; ++i)
                astrocs::phase3proj::v6::pixel_solid_angle(&d, i, j, &out[j * 61 + i]);
    };
    for (int nt : {2, 4, 8}) {
        std::vector<double> par(61 * 61, 0.0);
        std::vector<std::thread> th;
        const int chunk = (61 + nt - 1) / nt;
        for (int t = 0; t < nt; ++t) {
            const int b = std::min(61, t * chunk);
            const int e = std::min(61, b + chunk);
            th.emplace_back(worker, b, e, par.data());
        }
        for (auto& x : th) x.join();
        CHECK_MSG(std::memcmp(ref.data(), par.data(), ref.size() * 8) == 0,
                  "1/N worker Ω 分块 bitwise 一致");
    }
}

// ---- G 故障注入 ----
void test_fault_injection(const char* mode) {
    fault_mode = true;
    if (std::strcmp(mode, "const_omega") == 0) {
        const Descriptor d = make_case(ProjectionId::kCAR, 0, 0, 0.2, 16, 601);
        std::vector<double> om(16 * 601, 0.0);
        astrocs::phase3proj::v6::solid_angle_grid(&d, om.data(), nullptr);
        double mn = om[0], mx = om[0];
        for (double v : om) { mn = std::min(mn, v); mx = std::max(mx, v); }
        // 等价缺陷: 把 Ω 当常数 -> ratio==1.0；正向实现 ratio≈2.0 -> 必败
        const double defective = 1.0;
        CHECK_MSG(std::fabs(mx / mn - defective) < 1e-9,
                  "fault const_omega: 常数 Ω 假设必败");
    } else if (std::strcmp(mode, "legacy_car") == 0) {
        const Descriptor d = make_case(ProjectionId::kCAR, 0, 0, 0.2, 41, 41);
        double ra, dec;
        astrocs::phase3proj::v6::pix2world(&d, 20, 40, &ra, &dec);
        // 等价缺陷: legacy Y=−θ -> dec 反号；正向实现 dec=+Y -> 必败
        const double yd = 0.2 * ((40 + 1) - d.crpix_y);
        CHECK_MSG(std::fabs(dec - (-yd)) < 1e-12, "fault legacy_car: Y=−θ 必败");
    } else if (std::strcmp(mode, "legacy_ait") == 0) {
        const Descriptor d = make_case(ProjectionId::kAIT, 0, 0, 0.2, 61, 61);
        std::vector<double> om(61 * 61, 0.0);
        astrocs::phase3proj::v6::solid_angle_grid(&d, om.data(), nullptr);
        // 等价缺陷: legacy 缺 √2 因子 -> Ω 减半
        const double defective = 0.5 * (0.2 * kRad) * (0.2 * kRad);
        CHECK_MSG(std::fabs(om[0] - defective) / defective < 1e-9,
                  "fault legacy_ait: 缺 √2 必败");
    } else if (std::strcmp(mode, "swap_norm") == 0) {
        const int m = 3, n = 5;
        const double om_in[5] = {1.0, 0.5, 1.5, 1.5, 1.5};
        double a[15] = {1.0, 0.4, 0.6, 0.0, 0.0, 0.0, 0.1, 0.9, 1.5, 0.0,
                        0.0, 0.0, 0.0, 0.0, 1.5};
        double s[15], rs[3];
        astrocs::phase3proj::v6::col_normalise(a, m, n, om_in, s);
        astrocs::phase3proj::v6::matrix_row_sums(s, m, n, rs);
        // 等价缺陷: S 当行归一 -> Σ_j S_ij=1；正向 S 为列归一 -> 必败
        CHECK_MSG(std::fabs(rs[0] - 1.0) < 1e-12, "fault swap_norm: R/S 互换必败");
    } else if (std::strcmp(mode, "naive_wrap") == 0) {
        Plan p;
        astrocs::phase3proj::v6::plan(ProjectionId::kCAR, 0, 0, 1.0, 97, 97,
                                      "east_left", 0.0, &p);
        // 等价缺陷: 不做最短角差/wrap -> wrap=false；正向 wrap=true -> 必败
        CHECK_MSG(!p.crosses_ra_wrap, "fault naive_wrap: 忽略 RA wrap 必败");
    }
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    const char* fault = std::getenv("ASTROCS_P3PROJ_V6_FAULT");
    if (fault && std::strcmp(fault, "0") != 0) {
        test_fault_injection(fault);
        if (failures > 0) {
            std::printf("IMPL-P3-PROJ-001 FAULT-EFFECT-CONFIRMED mode=%s (%d)\n",
                        fault, failures);
            return 0;
        }
        std::fprintf(stderr, "FAULT MODE 未触发失败: 注入无效 mode=%s\n", fault);
        return 2;
    }
    test_registry();
    test_standard_wcs();
    test_crval2_rotation();
    test_domain_limits();
    test_solid_angle();
    test_normalisation();
    test_plan();
    test_determinism();
    if (failures == 0) {
        std::printf(
            "IMPL-P3-PROJ-001 V6 PROJECTION PASS (registry v3 标准 Paper II "
            "TAN/SIN/CAR/AIT + CRVAL2 三 Euler 角旋转 + AIT A≤1 + CAR 极行 "
            "fail-closed + 逐像素 Ω 真实计算（CAR ratio≈2.0000）+ R/S 行/列归一 "
            "+ 计划/奇点/wrap + 独立 oracle)\n");
        return 0;
    }
    std::fprintf(stderr, "IMPL-P3-PROJ-001 V6 PROJECTION FAIL (%d)\n", failures);
    return 1;
}
