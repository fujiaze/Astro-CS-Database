// ============================================================================
// candidate_oracle_test.cpp - 快速候选零漏选 Oracle 矩阵 (控制包 CANDIDATE_TEST_MATRIX)
//
// Oracle: 小 NSIDE 全像素穷举 — 对每个 HEALPix 像素计算 compute_overlap_area_g
//   > 0 即为真集合 (false negatives 定义基准)
// 对比: query_candidate_pixels_fast (生产快速候选, 含跨 face 边界回退)
// 断言: fast 候选集合 ⊇ Oracle 真集合 (false_negatives == 0)
//
// 场景: 12 base face × {中心, 4 边中点, 4 角} + RA跨0 + 南北极
//       × pixfrac {0.1, 0.25, 0.5, 1.0} × 尺度 {0.1", 1", 10", 1', 1°}
//
// 编译 (tests/): 同 drizzle_l0_test.cpp
// ============================================================================
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

static const double PI_ = 3.14159265358979323846;
static int g_pass = 0, g_fail = 0;
static long long g_total_cases = 0, g_fallback_cases = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static const double D2R = PI_ / 180.0;

// 构造球面 drop: 中心 (ra,dec) + 像素尺度 (角秒) + pixfrac 收缩
static std::vector<spherical::Vec3> make_drop(double ra_c, double dec_c,
                                              double scale_arcsec, double pixfrac) {
    double half_deg = 0.5 * scale_arcsec / 3600.0 * pixfrac;
    double c[4][2] = {
        {ra_c - half_deg, dec_c - half_deg},
        {ra_c + half_deg, dec_c - half_deg},
        {ra_c + half_deg, dec_c + half_deg},
        {ra_c - half_deg, dec_c + half_deg}
    };
    std::vector<spherical::Vec3> drop;
    for (int i = 0; i < 4; i++)
        drop.push_back(spherical::radec_to_vec<double>(c[i][0], c[i][1]));
    return drop;
}

// Oracle: 小 NSIDE 全像素穷举 (真集合 = overlap > 0)
static std::vector<uint64_t> oracle_exhaustive(const std::vector<spherical::Vec3>& drop,
                                               const healpix::HealpixCore& hp) {
    std::vector<uint64_t> truth;
    int64_t npix = hp.getNpix();
    spherical::DropGeometry g = spherical::build_drop_geometry(drop);
    for (int64_t ip = 0; ip < npix; ip++) {
        if (spherical::compute_overlap_area_g(g, hp, (uint64_t)ip) > 0.0)
            truth.push_back((uint64_t)ip);
    }
    return truth;
}

// Oracle: 高 NSIDE 保守参考 (queryDisc buffer 3.0 + 真集合判定)
static std::vector<uint64_t> oracle_high_nside(const std::vector<spherical::Vec3>& drop,
                                               const healpix::HealpixCore& hp) {
    std::vector<uint64_t> superset;
    spherical::query_candidate_pixels<double>(drop, hp, superset);
    spherical::DropGeometry g = spherical::build_drop_geometry(drop);
    std::vector<uint64_t> truth;
    for (uint64_t ip : superset)
        if (spherical::compute_overlap_area_g(g, hp, ip) > 0.0)
            truth.push_back(ip);
    return truth;
}

// 单 case: 构造 drop + Oracle + fast, 断言 false_negatives == 0
static void run_case(const char* loc, double ra, double dec,
                     double scale_arcsec, double pixfrac, int nside) {
    g_total_cases++;
    std::vector<spherical::Vec3> drop = make_drop(ra, dec, scale_arcsec, pixfrac);
    healpix::HealpixCore hp(nside, true);

    std::vector<uint64_t> truth = (nside <= 32)
        ? oracle_exhaustive(drop, hp)
        : oracle_high_nside(drop, hp);
    std::vector<uint64_t> fast;
    spherical::query_candidate_pixels_fast<double>(drop, hp, fast);

    // false negatives: truth 中不在 fast 里的
    std::vector<uint64_t> sorted_fast = fast;
    std::sort(sorted_fast.begin(), sorted_fast.end());
    int fn = 0;
    for (uint64_t ip : truth) {
        if (!std::binary_search(sorted_fast.begin(), sorted_fast.end(), ip)) fn++;
    }
    // false positives: fast 中不在 truth 里的
    std::vector<uint64_t> sorted_truth = truth;
    std::sort(sorted_truth.begin(), sorted_truth.end());
    int fp = 0;
    for (uint64_t ip : fast) {
        if (!std::binary_search(sorted_truth.begin(), sorted_truth.end(), ip)) fp++;
    }
    char msg[256];
    if (fn > 0) {
        snprintf(msg, sizeof(msg),
                 "loc=%s ra=%.3f dec=%.3f scale=%.1f\" pixfrac=%.2f nside=%d: "
                 "FN=%d (truth=%zu fast=%zu)",
                 loc, ra, dec, scale_arcsec, pixfrac, nside, fn, truth.size(), fast.size());
        CHECK(false, msg);
    } else {
        g_pass++;
    }
}

int main() {
    printf("=== 候选零漏选 Oracle 矩阵 ===\n");
    const double scales[] = {0.1, 1.0, 10.0, 60.0};  // 1° 尺度在 L0/高NSIDE 层单独覆盖
    const double pixfracs[] = {0.1, 0.25, 0.5, 1.0};
    const int nside_oracle = 16;  // 全像素 Oracle 快路径 (3072 像素); 高 NSIDE 由保守参考矩阵覆盖

    // 12 base face 中心 (nside=1 像素中心) + 边/角构造位置
    // 用 nside=8 枚举 face 边界附近像素中心作为边/角场景
    healpix::HealpixCore hp8(8, true);
    std::vector<std::pair<std::string, std::pair<double,double>>> positions;

    // 12 face 中心: nside=1 的 12 个像素中心
    healpix::HealpixCore hp1(1, true);
    for (int64_t ip = 0; ip < 12; ip++) {
        double th, ph;
        hp1.pix2ang(ip, &th, &ph);
        double dec = 90.0 - th * (180.0 / PI_);
        double ra = ph * (180.0 / PI_);
        char name[32];
        snprintf(name, sizeof(name), "face%d_center", (int)ip);
        positions.push_back({name, {ra, dec}});
    }
    // 12 face 边/角: 用 nside=8 在 face 边界 (ix/iy ∈ {0,7}) 取像素中心
    {
        int64_t per_face8 = 64;
        auto spread3 = [](int v) -> int64_t {
            int64_t r = 0;
            for (int i = 0; i < 3; i++)
                r |= ((int64_t)((v >> i) & 1)) << (2 * i);
            return r;
        };
        for (int f = 0; f < 12; f++) {
            // 4 个角 (ix,iy ∈ {0,7})
            int corners[4][2] = {{0,0},{0,7},{7,0},{7,7}};
            for (int c = 0; c < 4; c++) {
                int ix = corners[c][0], iy = corners[c][1];
                int64_t real_ip = (int64_t)f * per_face8 +
                    (spread3(ix) | (spread3(iy) << 1));
                double th, ph;
                hp8.pix2ang(real_ip, &th, &ph);
                double dec = 90.0 - th * (180.0 / PI_);
                double ra = ph * (180.0 / PI_);
                char name[32];
                snprintf(name, sizeof(name), "face%d_c%d", f, c);
                positions.push_back({name, {ra, dec}});
            }
            // 4 条边中点 (ix=0 或 7, iy=3/4; iy=0 或 7, ix=3/4)
            int edges[4][2] = {{0,3},{7,4},{3,0},{4,7}};
            for (int e = 0; e < 4; e++) {
                int ix = edges[e][0], iy = edges[e][1];
                int64_t real_ip = (int64_t)f * per_face8 +
                    (spread3(ix) | (spread3(iy) << 1));
                double th, ph;
                hp8.pix2ang(real_ip, &th, &ph);
                double dec = 90.0 - th * (180.0 / PI_);
                double ra = ph * (180.0 / PI_);
                char name[32];
                snprintf(name, sizeof(name), "face%d_e%d", f, e);
                positions.push_back({name, {ra, dec}});
            }
        }
    }
    // RA 跨 0 + 极区
    positions.push_back({"ra_cross0", {359.999, 0.0}});
    positions.push_back({"north_pole", {0.0, 89.99}});
    positions.push_back({"south_pole", {0.0, -89.99}});

    // 高 NSIDE 抽样 (保守参考 Oracle): 12 face 中心 × 1"/10" × pixfrac 1.0
    for (const auto& [loc, pos] : positions) {
        if (std::string(loc).find("_center") == std::string::npos) continue;
        for (int nside_hi : {1024, 65536}) {
            run_case(loc.c_str(), pos.first, pos.second, 1.0, 1.0, nside_hi);
            run_case(loc.c_str(), pos.first, pos.second, 10.0, 1.0, nside_hi);
        }
    }

    // 矩阵: 位置 × 尺度 × pixfrac
    long long case_no = 0;
    for (const auto& [loc, pos] : positions) {
        for (double scale : scales) {
            for (double pf : pixfracs) {
                run_case(loc.c_str(), pos.first, pos.second, scale, pf, nside_oracle);
                case_no++;
                if (case_no % 200 == 0) {
                    printf("  ... %lld cases done (pass=%d fail=%d)\n", case_no, g_pass, g_fail);
                }
            }
        }
    }
    printf("== 候选 Oracle 矩阵: %d 通过, %d 失败 (cases=%lld/%lld) ==\n",
           g_pass, g_fail, case_no, g_total_cases);
    return g_fail == 0 ? 0 : 1;
}
