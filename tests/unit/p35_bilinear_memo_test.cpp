// tests/unit/p35_bilinear_memo_test.cpp — P35 回归: phase3 bilinear 保核记忆化逐位等值
//
// 冻结核 = 切平面**四象限**最近中心双线性 (docs/algorithms/PHASE3_RSMP_IMPL.md:188-199,275,
// DISP-P3RSMP-001)。P35 不改核, 只对权威 neighbors/pix2ang_nest 做 per-sampler 记忆化。
// 断言:
//   A. 记忆化开/关, 同一批采样点输出 **逐位相同** (value 位型/weights/leaf_ipix/coverage);
//   B. 覆盖 tile 中心 / 跨 tile 边界 / 极区 / face 缝网格;
//   C. 阴性对照: 记忆化开着时必须真的发生命中 (leaf_hits>0), 关闭时 leaf_hits==0;
//      且逐位比较器对人为翻 1 bit 必红 (等值断言非恒真)。
#include "p3_resample.h"
#include "aio_hips.h"
#include "healpix_core.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_fail; } } while (0)

using astrocs::phase3::P3Sampler;
using astrocs::phase3::P3MemoStats;

static bool build_mini_hips(const std::string& hips, uint32_t n_tiles) {
    std::error_code ec;
    std::filesystem::remove_all(hips, ec);
    std::filesystem::create_directories(hips, ec);
    if (ec) return false;
    const uint32_t W = 512;
    AioHipsProductSet* ps = aio_hips_product_begin(
        hips.c_str(), 512, W, AIO_HIPS_FLOAT32, AIO_HIPS_PRODUCT_SIGNAL,
        "did:test:p35memo", "P35 bilinear memo regression", "NONE", 0.0,
        "2026-09-15T00:00:00Z", 0);
    if (!ps) return false;
    for (uint32_t k = 0; k < n_tiles; ++k) {
        std::vector<float> flux((size_t)W * W, 10.0f * (float)(k + 1));
        std::vector<float> area((size_t)W * W, 1.0f);
        AstroSphereTileView view{};
        view.parent_ipix = k;
        view.leaf_order = 9;
        view.width = W;
        view.data_type = AIO_HIPS_FLOAT32;
        view.flux_sum = flux.data();
        view.covered_area = area.data();
        if (aio_hips_write_signal_support_tile(ps, &view) != 0) { aio_hips_finalize(ps); return false; }
    }
    return aio_hips_finalize(ps) == 0;
}

int main() {
    const char* d = std::getenv("TMPDIR");
    if (!d || !*d) d = "/tmp";
    const std::string hips = std::string(d) + "/astrocs_p35_bilinear_memo";
    CHECK(build_mini_hips(hips, 12));

    P3Sampler s_on{}, s_off{};
    std::string err;
    int order = -1;
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s_on, &order, nullptr, &err) == astrocs::phase3::P3_RS_OK);
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s_off, nullptr, nullptr, &err) == astrocs::phase3::P3_RS_OK);
    astrocs::phase3::p3_sampler_set_leaf_memo(&s_off, 0);   // 阴性对照: 关记忆化

    long long n = 0, n_diff = 0, n_cov1 = 0;
    auto cmp_point = [&](double ra, double dec) {
        float v_on = 0, v_off = 0;
        int c_on = 0, c_off = 0;
        double w_on[4] = {0,0,0,0}, w_off[4] = {0,0,0,0};
        uint64_t i_on[4] = {0,0,0,0}, i_off[4] = {0,0,0,0};
        astrocs::phase3::p3_sample_bilinear_ex(&s_on, ra, dec, &v_on, &c_on, w_on, i_on);
        astrocs::phase3::p3_sample_bilinear_ex(&s_off, ra, dec, &v_off, &c_off, w_off, i_off);
        bool same = (std::memcmp(&v_on, &v_off, sizeof(float)) == 0) && (c_on == c_off);
        for (int k = 0; k < 4; k++) {
            if (std::memcmp(&w_on[k], &w_off[k], sizeof(double)) != 0) same = false;
            if (i_on[k] != i_off[k]) same = false;
        }
        if (!same) ++n_diff;
        if (c_on == 1) ++n_cov1;
        ++n;
    };
    // 1) 12 个 base tile 中心 (nside=512 ⇒ K=0)
    for (uint32_t k = 0; k < 12; ++k) {
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(1, k, ra, dec);
        cmp_point(ra, dec);
    }
    // 2) M42 场跨 tile 网格 + 极区 + RA=0/face 缝
    for (int i = 0; i <= 60; ++i)
        for (int j = 0; j <= 60; ++j) {
            cmp_point(80.0 + i * 0.2, -10.0 + j * 0.2);
        }
    for (int i = 0; i < 40; ++i) {
        cmp_point(10.0 + i * 0.5, 89.5);
        cmp_point(10.0 + i * 0.5, -89.5);
        cmp_point(0.0 + i * 0.5, 0.0);
    }
    CHECK(n > 3000);
    CHECK(n_diff == 0);      // A/B: 逐位相同
    CHECK(n_cov1 > 0);       // 覆盖有值区

    P3MemoStats st_on{}, st_off{};
    astrocs::phase3::p3_sampler_memo_stats(&s_on, &st_on);
    astrocs::phase3::p3_sampler_memo_stats(&s_off, &st_off);
    std::printf("P35 memo: calls=%lld diff=%lld cov1=%lld on(hits=%llu miss=%llu) off(hits=%llu)\n",
                n, n_diff, n_cov1, st_on.leaf_hits, st_on.leaf_misses, st_off.leaf_hits);
    CHECK(st_on.leaf_hits > 0);       // C: 记忆化真的命中 (非空跑)
    CHECK(st_on.leaf_misses > 0);
    CHECK(st_off.leaf_hits == 0);     // 关闭后无命中
    // 阴性对照: 逐位比较器对人为翻 bit 必红
    {
        float a = 1.0f, b = 1.0f;
        CHECK(std::memcmp(&a, &b, sizeof(float)) == 0);
        b = std::nextafterf(b, 2.0f);
        CHECK(std::memcmp(&a, &b, sizeof(float)) != 0);
    }
    astrocs::phase3::p3_sampler_close(&s_on);
    astrocs::phase3::p3_sampler_close(&s_off);
    if (g_fail) { std::fprintf(stderr, "p35_bilinear_memo: %d FAIL\n", g_fail); return 1; }
    std::printf("p35_bilinear_memo: PASS\n");
    return 0;
}
