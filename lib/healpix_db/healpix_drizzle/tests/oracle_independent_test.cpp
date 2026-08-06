// ============================================================================
// oracle_independent_test.cpp - 候选零漏选独立 Oracle (R13 CAND-001)
//
// 独立性: 不调用生产 query_candidate_pixels_fast / compute_overlap_area_g。
//   真值判定 = 球面采样点归属 (radec2pix, HEALPix 定义) + 半平面点包含
//   (drop 边法向量, 与生产 overlap 独立)。
//   相交判定双方向:
//     A) 像素内采样点 (中心+4角+边4分点) 在 drop 内 → 相交
//     B) drop 内采样点 (中心+4角+边4分点) 归属该像素 → 相交
//   对凸四边形, 采样密度 = 像素尺度/4, 覆盖边交叉/顶点互含。
//
// 覆盖:
//   - 小 NSIDE (16/64) 全像素穷举: 111 位置 (12 face 中心/边/角 + RA0 + 极区)
//     x 尺度 {0.1,1,10,60,3600}" x pixfrac {0.1,0.5,1.0}
//   - 高 NSIDE (2^14/2^18/2^20/2^22) face 边/角 + 极区: 采样 Oracle
//   - 主域节点余量: 0.0503"/px (2^22) 与 12.883"/px (2^14)
// 硬门: false negative = 0 (fast 候选 ⊇ Oracle 真集)
// ============================================================================
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <set>
#include <algorithm>
#include <utility>
#include <string>

static const double PI_ = 3.14159265358979323846;
static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

using spherical::Vec3;

// ---- 独立几何原语 (不调用生产 overlap) ----
static Vec3 norm(const Vec3& v) {
    double l = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return {v.x/l, v.y/l, v.z/l};
}
static Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

// 点是否在 drop 凸多边形内 (半平面, drop 边法向量指向内部)
static bool point_in_drop(const std::vector<Vec3>& drop, const Vec3& p) {
    Vec3 c{0,0,0};
    for (const auto& v : drop) { c.x += v.x; c.y += v.y; c.z += v.z; }
    c = norm(c);
    int n = (int)drop.size();
    for (int i = 0; i < n; i++) {
        Vec3 e = cross3(drop[i], drop[(i + 1) % n]);
        e = norm(e);
        if (e.x*c.x + e.y*c.y + e.z*c.z < 0) e = {-e.x, -e.y, -e.z};
        if (e.x*p.x + e.y*p.y + e.z*p.z < -1e-12) return false;
    }
    return true;
}

// 像素内部采样点 (中心 + 4 角 + 4 边 4 分点)
static std::vector<Vec3> pixel_sample_points(const healpix::HealpixCore& hp,
                                             uint64_t ipix, int nside) {
    std::vector<Vec3> pts;
    double ra, dec;
    hp.pix2radec((int64_t)ipix, &ra, &dec);
    pts.push_back(spherical::radec_to_vec<double>(ra, dec));
    auto bnd = spherical::get_healpix_boundary<double>(hp, ipix, nside);
    for (const auto& v : bnd) pts.push_back(v);
    for (int i = 0; i < (int)bnd.size(); i++) {
        const Vec3& a = bnd[i];
        const Vec3& b = bnd[(i + 1) % bnd.size()];
        for (int t = 1; t <= 3; t++) {
            Vec3 m = {a.x + t * (b.x - a.x) / 4.0,
                      a.y + t * (b.y - a.y) / 4.0,
                      a.z + t * (b.z - a.z) / 4.0};
            pts.push_back(norm(m));
        }
    }
    return pts;
}

// drop 内部采样点 (中心 + 4 角 + 4 边 4 分点)
static std::vector<Vec3> drop_sample_points(const std::vector<Vec3>& drop) {
    std::vector<Vec3> pts;
    Vec3 c{0,0,0};
    for (const auto& v : drop) { c.x += v.x; c.y += v.y; c.z += v.z; }
    pts.push_back(norm(c));
    for (const auto& v : drop) pts.push_back(v);
    int n = (int)drop.size();
    for (int i = 0; i < n; i++) {
        const Vec3& a = drop[i];
        const Vec3& b = drop[(i + 1) % n];
        for (int t = 1; t <= 3; t++) {
            Vec3 m = {a.x + t * (b.x - a.x) / 4.0,
                      a.y + t * (b.y - a.y) / 4.0,
                      a.z + t * (b.z - a.z) / 4.0};
            pts.push_back(norm(m));
        }
    }
    return pts;
}

// 独立 Oracle: drop 与像素是否相交
static bool oracle_intersects(const std::vector<Vec3>& drop,
                              const healpix::HealpixCore& hp, uint64_t ipix,
                              int nside) {
    // A) 像素内采样点在 drop 内
    for (const auto& p : pixel_sample_points(hp, ipix, nside))
        if (point_in_drop(drop, p)) return true;
    // B) drop 内采样点归属该像素
    for (const auto& p : drop_sample_points(drop)) {
        double ra, dec;
        spherical::vec_to_radec<double>(p, ra, dec);
        if ((uint64_t)hp.radec2pix(ra, dec) == ipix) return true;
    }
    return false;
}

// 构造 drop (RA/Dec 盒子 + pixfrac 收缩)
static std::vector<Vec3> make_drop(double ra_c, double dec_c,
                                   double scale_arcsec, double pixfrac) {
    double half_deg = 0.5 * scale_arcsec / 3600.0 * pixfrac;
    double c[4][2] = {
        {ra_c - half_deg, dec_c - half_deg},
        {ra_c + half_deg, dec_c - half_deg},
        {ra_c + half_deg, dec_c + half_deg},
        {ra_c - half_deg, dec_c + half_deg}
    };
    std::vector<Vec3> drop;
    for (int i = 0; i < 4; i++)
        drop.push_back(spherical::radec_to_vec<double>(c[i][0], c[i][1]));
    return drop;
}

// 单 case: Oracle 穷举/采样真集 vs 生产 fast 候选
static void run_case(const char* loc, double ra, double dec,
                     double scale, double pf, int nside, bool exhaustive) {
    std::vector<Vec3> drop = make_drop(ra, dec, scale, pf);
    healpix::HealpixCore hp(nside, true);
    std::vector<uint64_t> fast;
    spherical::query_candidate_pixels_fast<double>(drop, hp, fast);
    std::set<uint64_t> fast_set(fast.begin(), fast.end());
    int fn = 0;
    if (exhaustive) {
        int64_t npix = hp.getNpix();
        for (int64_t ip = 0; ip < npix; ip++)
            if (oracle_intersects(drop, hp, (uint64_t)ip, nside))
                if (!fast_set.count((uint64_t)ip)) fn++;
    } else {
        // 高 NSIDE: 只测候选邻近 + 采样真集 (fast 集合必须 ⊇ 采样真集)
        // 采样真集: drop 采样点归属像素 + 像素中心在 drop 内的像素
        for (const auto& p : drop_sample_points(drop)) {
            double ra2, dec2;
            spherical::vec_to_radec<double>(p, ra2, dec2);
            uint64_t ip = (uint64_t)hp.radec2pix(ra2, dec2);
            if (!fast_set.count(ip)) fn++;
        }
        // 像素中心在 drop 内的像素 (通过包围盒邻居枚举)
        double ra_c2 = 0, dec_c2 = 0;
        Vec3 c{0,0,0};
        for (const auto& v : drop) { c.x += v.x; c.y += v.y; c.z += v.z; }
        c = norm(c);
        spherical::vec_to_radec<double>(c, ra_c2, dec_c2);
        uint64_t cip = (uint64_t)hp.radec2pix(ra_c2, dec_c2);
        // 邻居扩展检查 (中心像素 + 邻近, 防止高 NSIDE 采样漏)
        std::vector<int64_t> nbr = hp.neighbors((int64_t)cip);
        nbr.push_back((int64_t)cip);
        for (int64_t ip : nbr) {
            if (ip < 0) continue;
            double ra3, dec3;
            hp.pix2radec(ip, &ra3, &dec3);
            if (point_in_drop(drop, spherical::radec_to_vec<double>(ra3, dec3)))
                if (!fast_set.count((uint64_t)ip)) fn++;
        }
    }
    char msg[256];
    if (fn > 0) {
        snprintf(msg, sizeof(msg),
                 "loc=%s ra=%.3f dec=%.3f scale=%.4g\" pf=%.2f nside=%d FN=%d",
                 loc, ra, dec, scale, pf, nside, fn);
        CHECK(false, msg);
    } else {
        g_pass++;
    }
}

int main() {
    printf("=== 候选零漏选独立 Oracle (R13 CAND-001) ===\n");
    // 111 位置 (与候选矩阵一致): 12 face 中心 + 边/角 + RA0 + 极区
    std::vector<std::pair<std::string, std::pair<double,double>>> pos;
    healpix::HealpixCore hp1(1, true);
    for (int64_t ip = 0; ip < 12; ip++) {
        double th, ph;
        hp1.pix2ang(ip, &th, &ph);
        char name[32];
        snprintf(name, sizeof(name), "face%d_c", (int)ip);
        pos.push_back({name, {ph * 180.0 / PI_, 90.0 - th * 180.0 / PI_}});
    }
    {
        healpix::HealpixCore hp8(8, true);
        auto spread3 = [](int v) -> int64_t {
            int64_t r = 0;
            for (int i = 0; i < 3; i++) r |= ((int64_t)((v >> i) & 1)) << (2 * i);
            return r;
        };
        for (int f = 0; f < 12; f++) {
            int corners[4][2] = {{0,0},{0,7},{7,0},{7,7}};
            for (int c = 0; c < 4; c++) {
                int64_t rip = (int64_t)f * 64 + (spread3(corners[c][0]) | (spread3(corners[c][1]) << 1));
                double th, ph;
                hp8.pix2ang(rip, &th, &ph);
                char name[32];
                snprintf(name, sizeof(name), "face%d_c%d", f, c);
                pos.push_back({name, {ph * 180.0 / PI_, 90.0 - th * 180.0 / PI_}});
            }
            int edges[4][2] = {{0,3},{7,4},{3,0},{4,7}};
            for (int e = 0; e < 4; e++) {
                int64_t rip = (int64_t)f * 64 + (spread3(edges[e][0]) | (spread3(edges[e][1]) << 1));
                double th, ph;
                hp8.pix2ang(rip, &th, &ph);
                char name[32];
                snprintf(name, sizeof(name), "face%d_e%d", f, e);
                pos.push_back({name, {ph * 180.0 / PI_, 90.0 - th * 180.0 / PI_}});
            }
        }
        pos.push_back({"ra0", {0.0, 0.0}});
        pos.push_back({"np", {0.0, 89.9}});
        pos.push_back({"sp", {0.0, -89.9}});
    }

    // 1) 小 NSIDE 穷举: 16/64 全像素, 尺度 {0.1,1,10,60,3600}, pf {0.1,0.5,1.0}
    const double scales[] = {0.1, 1.0, 10.0, 60.0, 3600.0};
    const double pfs[] = {0.1, 0.5, 1.0};
    int case_no = 0;
    for (int nside : {16, 64}) {
        for (const auto& [loc, p] : pos) {
            for (double sc : scales)
                for (double pf : pfs) {
                    run_case(loc.c_str(), p.first, p.second, sc, pf, nside, true);
                    case_no++;
                }
        }
        printf("  nside=%d 穷举完成 (%d cases, pass=%d fail=%d)\n",
               nside, case_no, g_pass, g_fail);
    }

    // 2) 高 NSIDE face 边/角 + 极区 (采样 Oracle) + 节点余量端点
    const int hi_nsides[] = {16384, 262144, 1048576, 4194304};  // 2^14~2^22
    const double hi_scales[] = {0.0503, 0.1, 1.0, 10.0, 12.883};
    for (int nside : hi_nsides) {
        // face 边/角位置 (nside=8 构造的 pos 中 face*_c*/face*_e*)
        int n_hi = 0;
        for (const auto& [loc, p] : pos) {
            if (loc.rfind("face", 0) != 0) continue;  // 只测 face 边/角
            for (double sc : hi_scales) {
                run_case(loc.c_str(), p.first, p.second, sc, 1.0, nside, false);
                n_hi++;
            }
        }
        // 极区 + RA0
        run_case("np", 0.0, 89.9, 1.0, 1.0, nside, false);
        run_case("sp", 0.0, -89.9, 1.0, 1.0, nside, false);
        run_case("ra0", 0.0, 0.0, 1.0, 1.0, nside, false);
        printf("  nside=%d 高 NSIDE face 边/角 (%d cases, pass=%d fail=%d)\n",
               nside, n_hi + 3, g_pass, g_fail);
    }
    printf("== 独立 Oracle 结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
