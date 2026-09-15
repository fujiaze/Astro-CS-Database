// tests/unit/p35_candidate_incremental_test.cpp — P35 回归: drizzle 增量候选枚举等值
//
// 记录守护的真问题 (P35/K2): query_candidate_pixels_incremental 复用上一源像素候选盒
// 交集的格中心向量, 只对"新盒 − 旧盒"做 morton+pix2ang。这里断言它对每个源像素输出
// 与 query_candidate_pixels_fast **逐元素全等** (含排序), 覆盖:
//   赤道带同 face 渐进扫描 (复用段) + face 边界 + 极冠回退 (queryDisc) + 大 drop。
//
// 阴性对照 (证明等值断言非恒真 / 非空跑):
//   N1: 比较器对"删一个候选"必红;
//   N2: 把状态记录盒伪装成"覆盖所有未来盒"以模拟 P34 首版"只复用上一轮保留格"的
//       已知漏选坑, 必与 fast 不等 ⇒ 用例能捕获该机制失效。
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_fail; } } while (0)

static std::vector<spherical::Vec3> make_drop(double ra_c, double dec_c,
                                              double scale_arcsec, double pixfrac) {
    double half_deg = 0.5 * scale_arcsec / 3600.0 * pixfrac;
    double c[4][2] = {{ra_c - half_deg, dec_c - half_deg},
                      {ra_c + half_deg, dec_c - half_deg},
                      {ra_c + half_deg, dec_c + half_deg},
                      {ra_c - half_deg, dec_c + half_deg}};
    std::vector<spherical::Vec3> d;
    for (int i = 0; i < 4; i++)
        d.push_back(spherical::radec_to_vec<double>(c[i][0], c[i][1]));
    return d;
}

int main() {
    const int nside = 512;
    healpix::HealpixCore hp(nside, true);
    const double scale = hp.pixelResolutionArcsec();
    const double step_deg = scale * 0.3 / 3600.0;   // 相邻源像素盒平移 ~2 格

    long long n_pix = 0, n_fallback = 0, n_reuse = 0;
    bool saw_nonvacuous_n1 = false, saw_n2_mismatch = false;

    struct Scan { double ra0, dec0; int n; const char* tag; };
    const Scan scans[] = {
        {83.8221, -5.3911, 41, "M42 equatorial"},   // 赤道带 face5, 同 face 渐进
        {10.0, 89.90, 25, "north polar"},           // 极冠回退
        {0.0, 0.0, 25, "RA=0 face boundary"},       // face 角/RA 跨界
    };
    for (const Scan& sc : scans) {
        spherical::CandidateBoxState state;
        spherical::CandidateBoxState state_bad;
        for (int iy = 0; iy < sc.n; ++iy)
            for (int ix = 0; ix < sc.n; ++ix) {
                double ra = sc.ra0 + (ix - sc.n / 2) * step_deg;
                double dec = sc.dec0 + (iy - sc.n / 2) * step_deg;
                std::vector<spherical::Vec3> drop = make_drop(ra, dec, scale, 1.0);
                std::vector<uint64_t> fast, inc, inc_bad;
                bool fb = false, fb2 = false;
                spherical::query_candidate_pixels_fast<double>(drop, hp, fast, &fb);
                spherical::query_candidate_pixels_incremental<double>(
                    drop, hp, inc, state, &fb2);
                CHECK(fb == fb2);
                CHECK(fast == inc);
                if (fb) ++n_fallback;
                if (!fast.empty()) {
                    ++n_pix;
                    // N1: 删一个候选必被比较器检出 (等值断言非恒真)
                    std::vector<uint64_t> corrupt = fast;
                    corrupt.erase(corrupt.begin());
                    if (corrupt != inc) saw_nonvacuous_n1 = true;
                }
                // N2: 模拟"只复用上一轮保留格"漏选坑 —— 记录盒伪装成全空间,
                //     使新盒新增格被当作"已在旧盒"跳过。
                spherical::query_candidate_pixels_incremental<double>(
                    drop, hp, inc_bad, state_bad, &fb2);
                state_bad.x0 = -1000000; state_bad.y0 = -1000000;
                state_bad.x1 =  1000000; state_bad.y1 =  1000000;
                if (inc_bad != fast) saw_n2_mismatch = true;
                if (state.ok) ++n_reuse;   // 复用段持续存在
            }
    }
    CHECK(n_pix > 1000);
    CHECK(n_fallback > 0);            // 回退路径被覆盖
    CHECK(n_reuse > 1000);            // 增量复用段被覆盖
    CHECK(saw_nonvacuous_n1);         // 阴性对照 N1
    CHECK(saw_n2_mismatch);           // 阴性对照 N2 (漏选坑可被检出)

    if (g_fail) { std::fprintf(stderr, "p35_candidate_incremental: %d FAIL\n", g_fail); return 1; }
    std::printf("p35_candidate_incremental: PASS (pix=%lld fallback=%lld reuse=%lld)\n",
                n_pix, n_fallback, n_reuse);
    return 0;
}
