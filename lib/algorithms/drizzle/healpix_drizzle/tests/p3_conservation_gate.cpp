// ============================================================================
// p3_conservation_gate.cpp — P3 守恒闭合回归门（P3-01 / P3-06 订正闭环）
// ----------------------------------------------------------------------------
// 被测面: 生产球面交叠核 spherical::compute_overlap_area_g_ctx_cached<double>
//   （drop 几何 → 候选叶 → 面积累加，与 drizzle_engine 生产路径同一入口）。
//
// 判据（唯一权威: docs/science/algorithms/DRIZZLE_GEOMETRY.md §9「面积守恒闭合」）:
//   逐 drop:  |Σ_j a_jp − A_drop,p| ≤ max(τ_rel·A_drop,p, ε_abs)
//             τ_rel = 1e-6   （相对项: 几何/表示误差域，赤道与极冠内部可达）
//             ε_abs = 1e-15 sr（绝对项: 叶边界弦表示 + 近退化大圆求交的
//                               双精度绝对地板；实测最坏 1.48e-16 sr，
//                               取 6.8× 裕量；适用域与证据见 §9 与订正报告）
//   帧级:     |Σ_p (Σ_j a_jp − A_drop,p)| / Σ_p A_drop,p ≤ 1e-4
//   参考面积 A_drop 由本门自算（double-double 精确 VOS 扇形），不复用被测路径。
//
// 组划分（判据按组分别判定并打印，禁止"全局汇总通过即通过"）:
//   POLE      极点邻域（P3-01 快路径假阳性位置集）
//   POLAR     极冠内部 / face 角点另侧 / 同纬度三角点
//   EQUATOR   赤道对照
//   SEAM      u+v=1 face 接缝（P3-06 浮点绝对地板位置集）
//   HST       HST 真实尺度 0.04″/px nside=2^23（接缝 + 赤道对照）
//
// 负例（同一可执行、同一判据）: ASTROCS_DRZ_P3_FAULT=legacy_corner_fast
//   注入订正前行为（叶多边形顶点全含判定 + 返回解析叶面积 π/(3N²)）：
//   ① POLE/POLAR 组必须出现破门（否则判 INVALID，证明门对被测对象失明）；
//   ② EQUATOR 对照组必须仍全绿（证明判据不是恒假）。
//   ctest 以 WILL_FAIL 登记 drizzle_p3_conservation_legacy_injection。
//
// --self-test: 判据自检（正例/负例/边界）+ 逐组统计输出。
// ============================================================================
#include "spherical_overlap.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using spherical::Vec3;

// ---- 冻结判据常数（须与 DRIZZLE_GEOMETRY.md §9 逐字一致）------------------
const double P3_REL_BUDGET    = 1e-6;    // 相对项
const double P3_ABS_BUDGET_SR = 1e-15;   // 绝对项 [sr]
const double P3_FRAME_BUDGET  = 1e-4;    // 帧级通量记账误差

const double PI_     = 3.14159265358979323846;
const double ARCSEC  = PI_ / (180.0 * 3600.0);
const double DEG2RAD = PI_ / 180.0;

// ---- TAN (gnomonic) 足迹构造：p = normalize(T + ξ·e1 + η·e2) --------------
// 与生产 WCS 的 TAN 前向同构（切平面坐标 = 像元偏移 × 尺度）；不经 (ra,dec)
// 度往返，避免极区 dec 舍入（RC3）混入本门判据。
struct TanFrame {
    Vec3 T, e1, e2;
    double scale_rad = 0.0;
};

TanFrame make_tan(double ra_deg, double dec_deg, double arcsec_per_px) {
    const double a = ra_deg * DEG2RAD, d = dec_deg * DEG2RAD;
    TanFrame f;
    f.T  = {std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
    f.e1 = {-std::sin(a), std::cos(a), 0.0};                       // 东
    f.e2 = {-std::sin(d) * std::cos(a), -std::sin(d) * std::sin(a), std::cos(d)};  // 北
    f.scale_rad = arcsec_per_px * ARCSEC;
    return f;
}

Vec3 sky_of(const TanFrame& f, double dx_px, double dy_px) {
    const double xi = dx_px * f.scale_rad, eta = dy_px * f.scale_rad;
    Vec3 v{f.T.x + xi * f.e1.x + eta * f.e2.x,
           f.T.y + xi * f.e1.y + eta * f.e2.y,
           f.T.z + xi * f.e1.z + eta * f.e2.z};
    const double l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return {v.x / l, v.y / l, v.z / l};
}

// drop 四角（pixfrac 收缩），角序与生产 processPixelTiled Step1-2 同规
std::vector<Vec3> make_drop(const TanFrame& f, double px, double py, double pixfrac) {
    const double h = 0.5 * pixfrac;
    const double c[4][2] = {{px - h, py - h}, {px + h, py - h},
                            {px + h, py + h}, {px - h, py + h}};
    std::vector<Vec3> out;
    out.reserve(4);
    for (int i = 0; i < 4; ++i) out.push_back(sky_of(f, c[i][0], c[i][1]));
    return out;
}

// ---- 参考面积：double-double（FMA two_prod/two_sum）Van Oosterom 扇形 ------
// 精度 ~1e-30，使参考误差 ≪ 被测量的 1e-16 sr 绝对地板（严格分离测量与参考）。
struct DD { double hi, lo; };
inline DD two_sum(double a, double b) { double s = a + b, bb = s - a; return {s, (a - (s - bb)) + (b - bb)}; }
inline DD two_prod(double a, double b) { double p = a * b; return {p, std::fma(a, b, -p)}; }
inline DD dd_add(DD x, DD y) { DD s = two_sum(x.hi, y.hi); DD t = two_sum(x.lo, y.lo); s.lo += t.hi; DD r = two_sum(s.hi, s.lo); r.lo += t.lo; return r; }
inline DD dd_mul(DD x, DD y) { DD p = two_prod(x.hi, y.hi); p.lo += x.hi * y.lo + x.lo * y.hi; DD r = two_sum(p.hi, p.lo); return r; }
inline DD dd_sub(DD x, DD y) { return dd_add(x, {-y.hi, -y.lo}); }
inline DD D(double v) { return {v, 0.0}; }
inline double dv(DD x) { return x.hi + x.lo; }

DD ref_area_sr(const std::vector<Vec3>& P) {
    DD tot = D(0.0);
    for (size_t i = 1; i + 1 < P.size(); ++i) {
        const Vec3& A = P[0]; const Vec3& B = P[i]; const Vec3& C = P[i + 1];
        DD bx = dd_sub(dd_mul(D(B.y), D(C.z)), dd_mul(D(B.z), D(C.y)));
        DD by = dd_sub(dd_mul(D(B.z), D(C.x)), dd_mul(D(B.x), D(C.z)));
        DD bz = dd_sub(dd_mul(D(B.x), D(C.y)), dd_mul(D(B.y), D(C.x)));
        DD det = dd_add(dd_add(dd_mul(D(A.x), bx), dd_mul(D(A.y), by)), dd_mul(D(A.z), bz));
        DD dab = dd_add(dd_add(dd_mul(D(A.x), D(B.x)), dd_mul(D(A.y), D(B.y))), dd_mul(D(A.z), D(B.z)));
        DD dbc = dd_add(dd_add(dd_mul(D(B.x), D(C.x)), dd_mul(D(B.y), D(C.y))), dd_mul(D(B.z), D(C.z)));
        DD dca = dd_add(dd_add(dd_mul(D(C.x), D(A.x)), dd_mul(D(C.y), D(A.y))), dd_mul(D(C.z), D(A.z)));
        DD den = dd_add(D(1.0), dd_add(dab, dd_add(dbc, dca)));
        tot = dd_add(tot, D(2.0 * std::atan2(dv(det), dv(den))));
    }
    return tot;
}

// ---- long double 同算法复算（方法稳定性锁）--------------------------------
// 与生产逐位相同的 double 输入顶点（叶 4 角），在 long double 下重做
// S-H + 切平面面积。若"算术精度"是地板来源，该量级应显著下降；实测
// 与生产同量级 ⇒ 地板来自叶边界的弦表示与近退化大圆求交（几何），
// 不是加法/投影的算术精度（证据: run/FINAL-07/logs/p3_diag_precision_2e21.txt）。
struct LD3 { long double x, y, z; };
inline LD3 L(const Vec3& v) { return {(long double)v.x, (long double)v.y, (long double)v.z}; }
inline LD3 xc(LD3 a, LD3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline long double dt(LD3 a, LD3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline LD3 nm(LD3 a) { long double l = sqrtl(dt(a, a)); return {a.x / l, a.y / l, a.z / l}; }

// 同一组 double 顶点（= 生产裁剪输出顶点）下的两种算术:
//   double  : 有向面积元直接累加（生产 planar_polygon_area_n 的算术）
//   long double: 同一式子在 80 位扩展精度下累加
// 二者之差 = **纯算术**误差；它与 ε_abs 的关系决定绝对项预算是否为"算术地板"。
// 注: 把整个裁剪算法（含大圆求交）换到 long double 不能让地板下降
//     （实测同量级，见 run/FINAL-07/logs/p3_diag_precision_2e21.txt）
//     ⇒ 地板来自叶边界弦表示 + 近退化大圆求交的**几何**，不是加法精度。
double area_double_arith(const Vec3* pts, int n, const Vec3& center) {
    Vec3 c = center;
    const double cl = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
    c.x /= cl; c.y /= cl; c.z /= cl;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const Vec3& p = pts[i]; const Vec3& q = pts[(i + 1) % n];
        const double dp = p.x * c.x + p.y * c.y + p.z * c.z;
        const double dq = q.x * c.x + q.y * c.y + q.z * c.z;
        const double ux = p.x - dp * c.x, uy = p.y - dp * c.y, uz = p.z - dp * c.z;
        const double vx = q.x - dq * c.x, vy = q.y - dq * c.y, vz = q.z - dq * c.z;
        sum += (uy * vz - uz * vy) * c.x + (uz * vx - ux * vz) * c.y + (ux * vy - uy * vx) * c.z;
    }
    return 0.5 * std::fabs(sum);
}

long double area_ld_arith(const Vec3* pts, int n, const Vec3& center) {
    LD3 c = L(center);
    const long double cl = sqrtl(dt(c, c));
    c = {c.x / cl, c.y / cl, c.z / cl};
    long double sum = 0.0L;
    for (int i = 0; i < n; ++i) {
        const LD3 p = L(pts[i]); const LD3 q = L(pts[(i + 1) % n]);
        const long double dp = dt(p, c), dq = dt(q, c);
        const LD3 u{p.x - dp * c.x, p.y - dp * c.y, p.z - dp * c.z};
        const LD3 v{q.x - dq * c.x, q.y - dq * c.y, q.z - dq * c.z};
        sum += dt(xc(u, v), c);
    }
    return 0.5L * fabsl(sum);
}

// ---- 判据 ------------------------------------------------------------------
double judge_limit(double a_ref) {
    return std::max(P3_REL_BUDGET * a_ref, P3_ABS_BUDGET_SR);
}
bool judge_drop(double da, double a_ref) { return std::fabs(da) <= judge_limit(a_ref); }
// 旧口径（订正前）: 仅相对项 —— 仅用于自检中证明"接缝域结构性不可达"。
bool judge_drop_rel_only(double da, double a_ref) { return std::fabs(da) <= P3_REL_BUDGET * a_ref; }

const char* const kGroupNames[] = {"POLE", "POLAR", "EQUATOR", "SEAM", "HST"};
enum GroupId { G_POLE = 0, G_POLAR, G_EQUATOR, G_SEAM, G_HST, G_COUNT };

struct Tally {
    int n_drop = 0, n_fail = 0, n_fail_rel_only = 0;
    double worst_abs = 0.0, worst_rel = 0.0, worst_ratio = 0.0;
    double sum_da = 0.0, sum_a = 0.0, lr = 0.0;   // lr = 与 long double 复算的最大相对差
    void add(double da, double a_ref, double ld_rel) {
        ++n_drop;
        const double lim = judge_limit(a_ref);
        if (!judge_drop(da, a_ref)) ++n_fail;
        if (!judge_drop_rel_only(da, a_ref)) ++n_fail_rel_only;
        worst_abs = std::max(worst_abs, std::fabs(da));
        worst_rel = std::max(worst_rel, std::fabs(da) / a_ref);
        worst_ratio = std::max(worst_ratio, std::fabs(da) / lim);
        lr = std::max(lr, ld_rel);
        sum_da += da; sum_a += a_ref;
    }
    double frame_rel() const { return std::fabs(sum_da) / sum_a; }
};

struct Position { const char* tag; double ra, dec; };

// 位置集（P3-01/P3-06 要求的极点 / 接缝 / face 角点 / 对照）
const Position kPole[] = {
    {"极点(0,90)",      0.0,  90.0},
    {"极点邻域(30,89.9999)", 30.0, 89.9999},
};
const Position kPolar[] = {
    {"face 角点另侧(0,-41.810315)", 0.0, -41.810315},
    {"同纬度三角点(0,41.810315)",   0.0,  41.810315},
    {"极冠内部(0,60)",              0.0,  60.0},
};
const Position kEquator[] = {
    {"赤道(45,0)", 45.0, 0.0},
};
const Position kSeam[] = {
    {"u+v=1 接缝中点(45,41.810315)", 45.0, 41.810315},
};

// 单个位置 × 尺度：构造 drop 网格并累加生产面积
void run_block(const Position& pos, uint32_t nside, double scale_arcsec, int half_grid,
               double pixfrac, Tally& t, bool verbose) {
    healpix::HealpixCore hp((int)nside);
    const double hp_res = hp.pixelResolutionArcsec() * ARCSEC;
    const TanFrame f = make_tan(pos.ra, pos.dec, scale_arcsec);
    spherical::TargetGeomCache cache(8192);
    double worst = 0.0, worst_rel = 0.0, worst_ld = 0.0;
    for (int ix = -half_grid; ix <= half_grid; ++ix)
        for (int iy = -half_grid; iy <= half_grid; ++iy) {
            const std::vector<Vec3> drop = make_drop(f, (double)ix, (double)iy, pixfrac);
            const double a_ref = dv(ref_area_sr(drop));
            std::vector<Vec3> d = drop, dd = drop;
            spherical::DropGeometryT<double> pg;
            spherical::build_drop_geometry_into<double>(pg, d, &dd);
            std::vector<uint64_t> cands;
            spherical::query_candidate_pixels<double>(dd, hp, cands);
            double arith = 0.0;
            double sum = 0.0, sum_ld = 0.0;
            for (uint64_t ipix : cands) {
                sum += spherical::compute_overlap_area_g_ctx_cached<double>(pg, hp, ipix,
                                                                            hp_res, cache);
                // 生产同一路径的裁剪输出顶点（同一 double 输入、同一 S-H 实现）
                std::array<Vec3, 4> b4;
                spherical::get_healpix_boundary4<double>(hp, ipix, nside, b4);
                Vec3 inter[16];
                const int ni = spherical::sutherland_hodgman_spherical_fixed(
                    b4.data(), 4, pg.clip_normals_d, inter, 16);
                if (ni >= 3) {
                    const double ad = area_double_arith(inter, ni, pg.center_d);
                    const long double al = area_ld_arith(inter, ni, pg.center_d);
                    sum_ld += (double)al;
                    // 纯算术差（同一顶点集合，两种累加精度）
                    arith = std::max(arith, std::fabs((double)((long double)ad - al)) / a_ref);
                }
            }
            const double da = sum - a_ref;
            const double ld_rel = arith;
            t.add(da, a_ref, ld_rel);
            worst = std::max(worst, std::fabs(da));
            worst_rel = std::max(worst_rel, std::fabs(da) / a_ref);
            worst_ld = std::max(worst_ld, ld_rel);
        }
    if (verbose)
        printf("      %-38s drop=%.4f\"/px (%.2f x hp_res)  n=%d  最坏|dA|=%.3e sr  "
               "最坏相对=%.3e  算术地板=dbl/ld 同顶点 %.2e\n",
               pos.tag, scale_arcsec, scale_arcsec / hp.pixelResolutionArcsec(), t.n_drop,
               worst, worst_rel, worst_ld);
}

void run_group(GroupId g, const Position* pos, int npos, uint32_t nside,
               const double* scales, int nscales, int half_grid, double pixfrac,
               Tally& out, bool verbose) {
    for (int p = 0; p < npos; ++p) {
        if (verbose) printf("   [%s] %s\n", kGroupNames[g], pos[p].tag);
        for (int s = 0; s < nscales; ++s) {
            Tally t;
            run_block(pos[p], nside, scales[s], half_grid, pixfrac, t, verbose);
            out.n_drop += t.n_drop; out.n_fail += t.n_fail;
            out.n_fail_rel_only += t.n_fail_rel_only;
            out.worst_abs = std::max(out.worst_abs, t.worst_abs);
            out.worst_rel = std::max(out.worst_rel, t.worst_rel);
            out.worst_ratio = std::max(out.worst_ratio, t.worst_ratio);
            out.lr = std::max(out.lr, t.lr);
            out.sum_da += t.sum_da; out.sum_a += t.sum_a;
        }
    }
}

// 判据自检（不依赖被测路径）: 正例 / 负例 / 边界
int self_test_judge() {
    int fail = 0;
    struct C { double da, a; bool want; const char* tag; };
    const C cases[] = {
        {0.0,        1e-12, true,  "零残差 ⇒ 绿（判据非恒假）"},
        {5e-16,      1e-12, true,  "0.5·ε_abs ⇒ 绿"},
        {1.01e-15,   1e-12, false, "1.01·ε_abs ⇒ 红（绝对项有判别力）"},
        {2e-9,       1e-3,  false, "2·τ_rel·A ⇒ 红（相对项有判别力）"},
        {5e-10,      1e-3,  true,  "0.5·τ_rel·A ⇒ 绿"},
    };
    for (const auto& c : cases) {
        const bool got = judge_drop(c.da, c.a);
        if (got != c.want) {
            printf("  [FAIL] self-test 判据: %s (da=%.3e A=%.3e got=%d want=%d)\n",
                   c.tag, c.da, c.a, (int)got, (int)c.want);
            ++fail;
        }
    }
    if (fail == 0) printf("  [PASS] self-test 判据正例/负例/边界（5 例）\n");
    return fail;
}

}  // namespace

int main(int argc, char** argv) {
    bool self_test_only = false, verbose = true;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--self-test")) self_test_only = true;
        else if (!std::strcmp(argv[i], "--quiet")) verbose = false;

    }
    const char* fault = std::getenv("ASTROCS_DRZ_P3_FAULT");
    const bool injected = fault && std::strcmp(fault, "legacy_corner_fast") == 0;

    printf("=== P3 守恒闭合门 (P3-01/P3-06) ===\n");
    printf("  判据: 逐 drop |dA| <= max(%.1e * A_drop, %.1e sr) | 帧级 <= %.1e\n",
           P3_REL_BUDGET, P3_ABS_BUDGET_SR, P3_FRAME_BUDGET);
    printf("  注入: ASTROCS_DRZ_P3_FAULT=%s\n", fault ? fault : "(未设=生产行为)");

    int fail = 0;
    fail += self_test_judge();

    Tally T[G_COUNT];
    const double scales21[3] = {0.3 * 0.100649, 1.0 * 0.100649, 3.0 * 0.100649};  // 0.3/1/3 × hp_res(2^21)
    {  // 极点强扫描（T12/T27 实验侧同口径）: 常开，日志随门一起落盘
        // 与实验侧 T12 同口径的极点强扫描（追问"极点 6.6e-4 是否出现在生产路径"）:
        // 同一 drop 尺度（1x hp_res）、9x9 栅格（±4 px, 步 1 px）、位置 = 极点
        // drop 尺度对齐实验侧 T12/T27: 0.2"/px 像元（= 2.0 x hp_res(2^21)），
        // 9x9 栅格（±4 px, 步 1 px）覆盖 T12 的 14 角点扫描窗口。
        const double two = 2.0 * 0.100649;
        Tally g1, g3;
        run_group(G_POLE, kPole, 1, 2097152u, &two, 1, 4, 1.0, g1, true);
        const double six = 6.0 * 0.100649;
        run_group(G_POLE, kPole, 1, 2097152u, &six, 1, 4, 1.0, g3, true);
        printf("  [实验侧 T12/T27 口径] 极点 9x9: drop=2x hp_res 生产最坏|dA|=%.3e sr "
               "相对=%.3e 破门=%d/%d\n", g1.worst_abs, g1.worst_rel, g1.n_fail, g1.n_drop);
        printf("  [实验侧 T12/T27 口径] 极点 9x9: drop=6x hp_res 生产最坏|dA|=%.3e sr "
               "相对=%.3e 破门=%d/%d\n", g3.worst_abs, g3.worst_rel, g3.n_fail, g3.n_drop);
    }
    printf("  [矩阵] nside=2^21 drop=0.3/1/3 x hp_res(0.1006\"), 5x5 栅格\n");
    run_group(G_POLE,    kPole,    2, 2097152u, scales21, 3, 2, 1.0, T[G_POLE],    verbose);
    run_group(G_POLAR,   kPolar,   3, 2097152u, scales21, 3, 2, 1.0, T[G_POLAR],   verbose);
    run_group(G_EQUATOR, kEquator, 1, 2097152u, scales21, 3, 2, 1.0, T[G_EQUATOR], verbose);
    run_group(G_SEAM,    kSeam,    1, 2097152u, scales21, 3, 2, 1.0, T[G_SEAM],    verbose);

    printf("  [矩阵] HST 真实尺度 0.04\"/px nside=2^23, drop = 0.005/0.04/0.2\" 3x3 栅格\n");
    {
        const double sc[3] = {0.005, 0.04, 0.2};
        run_group(G_HST, kSeam,    1, 8388608u, sc, 3, 1, 1.0, T[G_HST], verbose);
        run_group(G_HST, kEquator, 1, 8388608u, sc, 3, 1, 1.0, T[G_HST], verbose);
    }

    // ---- 逐组判定 ----------------------------------------------------------
    printf("  ---- 逐组结果（每组独立判定）----\n");
    for (int g = 0; g < G_COUNT; ++g) {
        const Tally& t = T[g];
        printf("   [%s] n=%d 破门(新口径)=%d 破门(旧相对口径)=%d 最坏|dA|=%.3e sr "
               "最坏相对=%.3e |dA|/判据限=%.4f 帧级=%.3e 算术地板=%.2e\n",
               kGroupNames[g], t.n_drop, t.n_fail, t.n_fail_rel_only, t.worst_abs,
               t.worst_rel, t.worst_ratio, t.frame_rel(), t.lr);
    }

    int fail_pole = T[G_POLE].n_fail + T[G_POLAR].n_fail;
    int fail_ctrl = T[G_EQUATOR].n_fail;
    if (injected) {
        // 负例: 注入订正前行为后，极点/极冠位置集必须判红，赤道对照必须仍绿
        if (fail_pole == 0) {
            printf("  [INVALID] 故障注入 %s 未在 POLE/POLAR 组产生破门 ⇒ 门对被测对象失明\n", fault);
            ++fail;
        } else {
            printf("  [OK] 故障注入: POLE/POLAR 组破门 %d 例（快路径假阳性必须被抓到）\n", fail_pole);
        }
        if (fail_ctrl != 0) {
            printf("  [INVALID] 注入后 EQUATOR 对照组也破门 %d 例 ⇒ 判据恒假，无判别力\n", fail_ctrl);
            ++fail;
        } else {
            printf("  [OK] 故障注入: EQUATOR 对照组仍全绿（判据非恒假）\n");
        }
        // 注入模式 = 负例：无论上面是否 INVALID，都必须以非零退出（ctest WILL_FAIL
        // 依赖非零退出码；正常模式的红/绿判定与注入模式完全独立）。
        printf("  [EXPECT-RED] 注入 legacy_corner_fast ⇒ 负例判红（退出码 1）\n");
        ++fail;
    } else {
        for (int g = 0; g < G_COUNT; ++g) {
            if (T[g].n_fail != 0) {
                printf("  [FAIL] %s 组逐 drop 守恒闭合破门 %d/%d（最坏相对 %.3e）\n",
                       kGroupNames[g], T[g].n_fail, T[g].n_drop, T[g].worst_rel);
                ++fail;
            }
        }
        if (fail == 0) printf("  [PASS] 全组逐 drop 守恒闭合: 0 破门\n");
        // 帧级（全位置集合计）与"方法稳定性"锁
        double sum_da = 0.0, sum_a = 0.0, lr = 0.0;
        for (int g = 0; g < G_COUNT; ++g) {
            sum_da += T[g].sum_da; sum_a += T[g].sum_a; lr = std::max(lr, T[g].lr);
        }
        const double frame_rel = std::fabs(sum_da) / sum_a;
        if (frame_rel > P3_FRAME_BUDGET) {
            printf("  [FAIL] 帧级通量记账误差 %.4e > %.1e\n", frame_rel, P3_FRAME_BUDGET);
            ++fail;
        } else {
            printf("  [PASS] 帧级通量记账误差 %.4e <= %.1e\n", frame_rel, P3_FRAME_BUDGET);
        }
        // 算术稳定性锁: 同一组裁剪输出顶点下 double 与 long double 累加之差。
        // 它必须显著低于判据的绝对项（ε_abs/A_drop），否则"1e-16 sr 地板"不可
        // 归因于几何/表示，而应归因于算术精度（届时须改实现或预算）。
        if (lr > 1e-8) {
            printf("  [FAIL] 算术稳定性锁: dbl/ld 同顶点累加相对差 = %.3e > 1e-8 "
                   "⇒ 算术精度已进入被判量级（需复评 ε_abs 或改实现）\n", lr);
            ++fail;
        } else {
            printf("  [PASS] 算术稳定性锁: dbl/ld 同顶点累加相对差最坏 %.2e <= 1e-8 "
                   "（ε_abs 不来自算术精度；限值取实测最坏 6.2e-10 的约 16 倍裕量）\n", lr);
        }
    }

    if (self_test_only) {
        if (T[G_SEAM].n_fail_rel_only == 0) {
            printf("  [FAIL] self-test 对照: 旧相对口径竟然全绿（位置集未覆盖小 drop 域）\n");
            ++fail;
        } else {
            printf("  [PASS] self-test 对照: 旧相对口径破门 %d/%d（小 drop 域结构性不可达，"
                   "新口径以绝对项覆盖）\n", T[G_SEAM].n_fail_rel_only, T[G_SEAM].n_drop);
        }
        printf("== self-test: %s ==\n", fail == 0 ? "PASS" : "FAIL");
        return fail == 0 ? 0 : 1;
    }
    printf("== 守恒闭合门: %s ==\n", fail == 0 ? "PASS" : "FAIL");
    return fail == 0 ? 0 : 1;
}
