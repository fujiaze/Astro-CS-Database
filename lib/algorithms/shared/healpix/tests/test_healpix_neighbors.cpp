// ============================================================================
// test_healpix_neighbors.cpp - HEALPix NESTED neighbors/query_disc 科学面修复
//                             验证 (bug 狩猎 R9-B, P1 batchJ)
//
// 修复内容:
//   1. neighbors(): corner 分支错向+缺失 -> 官方 Healpix_3.83 表驱动算法
//      (nb_xoffset/nb_yoffset + kNbFaceArray + kNbSwapArray), 输出槽序与
//      astrometry get_neighbours 逐槽对齐 (+0,++,0+,-+,-0,--,0-,+-)。
//   2. query_disc(): face 边界 BFS 丢像素 -> 官方 NEST scheme 四叉树下钻
//      (zone 判定 + max_pixrad 安全余量), 语义 = 像素中心落在盘内。
//   3. 加固: 非 2 次幂 nside (含 0) 抛 invalid_argument;
//      child_nest/tile_to_leaf_nest 移位溢出抛 overflow_error。
//
// 验证锚:
//   - Gorski 2005 paper / astrometry test_healpix.c 钉值 18 例 (nside=4 槽序)
//   - nside=1 ipix=0 必含 face4 三面角邻居 (修复前缺失分支)
//   - 邻居关系对称性 (全像素, nside 1/2/4/8)
//   - query_disc vs 逐像素暴力 ground truth (极点/面交界/随机中心)
//   - 非法 nside / 移位溢出拒绝
//
// 用法: test_healpix_neighbors.exe
// ============================================================================

#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

using namespace astrocs::healpix;

namespace {

std::uint64_t g_fail = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        ++g_fail;
        std::printf("FAIL: %s\n", what);
    }
}

// 官方钉值 (Gorski 2005 / astrometry): nside=4, NESTED, astrometry 槽序
struct Pin { std::uint64_t ipix; std::vector<std::uint64_t> nb; };
const std::vector<Pin>& paper_pins() {
    static const std::vector<Pin> pins = {
        {0,   {1, 3, 2, 71, 69, 143, 90, 91}},
        {5,   {26, 27, 7, 6, 4, 94, 95}},
        {13,  {30, 31, 15, 14, 12, 6, 7, 27}},
        {15,  {31, 47, 63, 61, 14, 12, 13, 30}},
        {30,  {31, 15, 13, 7, 27, 25, 28, 29}},
        {101, {32, 34, 103, 102, 100, 174, 175, 122}},
        {127, {58, 37, 36, 126, 124, 125, 56}},
        {64,  {65, 67, 66, 183, 181, 138, 139}},
        {133, {80, 82, 135, 134, 132, 152, 154}},
        {148, {149, 151, 150, 147, 145, 162, 168, 170}},
        {160, {161, 163, 162, 145, 144, 128, 176, 178}},
        {24,  {25, 27, 26, 95, 93, 87, 18, 19}},
        {42,  {43, 23, 21, 111, 109, 40, 41}},
        {59,  {62, 45, 39, 37, 58, 56, 57, 60}},
        {191, {74, 48, 117, 116, 190, 188, 189, 72}},
        {190, {191, 117, 116, 113, 187, 185, 188, 189}},
        {186, {187, 113, 112, 165, 164, 184, 185}},
        {184, {185, 187, 186, 165, 164, 161, 178, 179}},
    };
    return pins;
}

// query_disc 暴力 ground truth: 像素中心球面角距 <= radius
std::set<std::uint64_t> brute_disc(std::uint32_t nside, double ra_deg,
                                   double dec_deg, double radius_arcsec) {
    std::set<std::uint64_t> want;
    const double rad = radius_arcsec * M_PI / (180.0 * 3600.0);
    const double cr = std::cos(rad);
    const double sin_dec0 = std::sin(dec_deg * M_PI / 180.0);
    const double cos_dec0 = std::cos(dec_deg * M_PI / 180.0);
    const double ra0 = ra_deg * M_PI / 180.0;
    const std::uint64_t np = npix(nside);
    for (std::uint64_t p = 0; p < np; ++p) {
        double ra = 0.0, dec = 0.0;
        pix2ang_nest(nside, p, ra, dec);
        const double sin_dec = std::sin(dec * M_PI / 180.0);
        const double cos_dec = std::cos(dec * M_PI / 180.0);
        const double cosd = sin_dec * sin_dec0 +
                            cos_dec * cos_dec0 * std::cos(ra * M_PI / 180.0 - ra0);
        if (cosd >= cr) want.insert(p);
    }
    return want;
}

bool disc_ok(std::uint32_t nside, double ra, double dec, double radius_arcsec) {
    const auto got = query_disc(nside, ra, dec, radius_arcsec);
    const std::set<std::uint64_t> gs(got.begin(), got.end());
    return gs == brute_disc(nside, ra, dec, radius_arcsec);
}

} // namespace

int main() {
    // ---- 1. 官方钉值 18 例 (nside=4, 槽序敏感) ----
    for (const Pin& pin : paper_pins()) {
        const auto nb = neighbors(4, pin.ipix);
        bool ok = (nb == pin.nb);
        if (!ok) {
            ++g_fail;
            std::printf("FAIL[pin] ipix=%llu got n=%zu:",
                        (unsigned long long)pin.ipix, nb.size());
            for (std::uint64_t v : nb) std::printf(" %llu", (unsigned long long)v);
            std::printf("  want:");
            for (std::uint64_t v : pin.nb) std::printf(" %llu", (unsigned long long)v);
            std::printf("\n");
        }
    }

    // ---- 2. nside=1 ipix=0: face4 三面角邻居必须在 (修复前缺失分支) ----
    {
        const auto nb = neighbors(1, 0);
        const std::set<std::uint64_t> s(nb.begin(), nb.end());
        expect(s == std::set<std::uint64_t>({1, 2, 3, 4, 5, 8}),
               "nside=1 ipix=0 neighbor set == {1,2,3,4,5,8} (face4 included)");
        // astrometry 槽序输出 (A 序, 与 oracle compare 逐序列一致): {1,2,3,4,8,5}
        expect(nb == std::vector<std::uint64_t>({1, 2, 3, 4, 8, 5}),
               "nside=1 ipix=0 slot order == {1,2,3,4,8,5}");
    }

    // ---- 3. 邻居对称性: q in nb(p) <=> p in nb(q) (全像素) ----
    for (std::uint32_t nside : {1u, 2u, 4u, 8u}) {
        const std::uint64_t np = npix(nside);
        std::vector<std::set<std::uint64_t>> nbsets(np);
        for (std::uint64_t p = 0; p < np; ++p) {
            const auto nb = neighbors(nside, p);
            expect(!nb.empty(), "neighbors non-empty");
            for (std::uint64_t q : nb) {
                expect(q < np, "neighbor in range");
                nbsets[p].insert(q);
            }
        }
        for (std::uint64_t p = 0; p < np; ++p) {
            for (std::uint64_t q : nbsets[p]) {
                if (nbsets[q].count(p) == 0) {
                    ++g_fail;
                    std::printf("FAIL[sym] nside=%u p=%llu -> q=%llu missing reverse\n",
                                nside, (unsigned long long)p, (unsigned long long)q);
                }
            }
        }
    }

    // ---- 4. query_disc vs 暴力 ground truth ----
    {
        const double centers[][2] = {
            {0, 0}, {90, 0}, {180, 0}, {270, 0},        // 赤道面心 (面交界)
            {22.5, 41.8103}, {22.5, -41.8103},          // 赤道带面心
            {0, 90}, {0, -90},                          // 南北极
            {0, 66.1603}, {90, -66.1603},               // 帽/带环交界
            {33.7, 12.4}, {201.3, -47.9}, {77.7, 65.2}, // 随机
            {313.1, -8.8}, {150.2, 29.3}, {260.4, 78.1},
        };
        const double radii[] = {60.0, 600.0, 3600.0, 18000.0};
        for (std::uint32_t nside : {1u, 2u, 4u, 8u, 16u}) {
            for (const auto& c : centers) {
                for (double r : radii) {
                    if (!disc_ok(nside, c[0], c[1], r)) {
                        ++g_fail;
                        std::printf("FAIL[qd] nside=%u ra=%.4f dec=%.4f r=%.1f\n",
                                    nside, c[0], c[1], r);
                    }
                }
            }
        }
        // exact 模式语义 = 像素中心在盘内 (官方 query_disc): 粗 nside 下盘心
        // 所在像素的中心可能在盘外, 故此处验证与暴力逐像素一致而非自包含
        expect(disc_ok(1, 10.0, 20.0, 600.0) && disc_ok(4, 10.0, 20.0, 600.0) &&
                   disc_ok(16, 10.0, 20.0, 600.0),
               "query_disc equals brute force around (10,20) r=600");
        // 全球: 半径 >= 180 度 -> 全像素
        {
            const auto got = query_disc(4, 0.0, 0.0, 180.0 * 3600.0 + 1.0);
            expect(got.size() == npix(4), "query_disc global == npix");
        }
        // 零半径 -> 仅中心像素
        {
            const auto got = query_disc(4, 10.0, 20.0, 0.0);
            expect(got.size() == 1 && got[0] == ang2pix_nest(4, 10.0, 20.0),
                   "query_disc zero radius == center pixel only");
        }
    }

    // ---- 5. 非 2 次幂 nside 拒绝 (R9-B 加固, 废弃静默取整) ----
    {
        auto throws = [](auto&& fn) -> bool {
            try { fn(); } catch (const std::invalid_argument&) { return true; }
            return false;
        };
        expect(throws([] { ang2pix_nest(3, 0.0, 0.0); }), "ang2pix_nest nside=3 throws");
        expect(throws([] { double x = 0.0, y = 0.0;
                          pix2ang_nest(3, 0, x, y); }), "pix2ang_nest nside=3 throws");
        expect(throws([] { neighbors(3, 0); }), "neighbors nside=3 throws");
        expect(throws([] { query_disc(3, 0.0, 0.0, 600.0); }), "query_disc nside=3 throws");
        expect(throws([] { ang2pix_nest(0, 0.0, 0.0); }), "ang2pix_nest nside=0 throws");
        // 合法 2 次幂不抛
        bool ok = true;
        try { (void)ang2pix_nest(8, 0.0, 0.0); (void)neighbors(8, 0); } catch (...) { ok = false; }
        expect(ok, "legal power-of-two nside does not throw");
    }

    // ---- 6. child_nest / tile_to_leaf_nest 移位溢出拒绝 ----
    {
        auto throws = [](auto&& fn) -> bool {
            try { fn(); } catch (const std::overflow_error&) { return true; }
            return false;
        };
        expect(throws([] { (void)child_nest(1ULL << 62, 1); }),
               "child_nest 62-bit payload shift=1 overflows");
        expect(throws([] { (void)child_nest(~0ULL, 1); }),
               "child_nest full payload shift=1 overflows");
        expect(throws([] { (void)tile_to_leaf_nest(~0ULL, 0, 31); }),
               "tile_to_leaf_nest overflow throws");
        // 边界内合法: 2 位 tile @order0 -> 首叶 @order31, roundtrip 还原
        // child_nest 是裸位左移 (ipix<<2*shift), roundtrip 用 leaf_to_tile 还原
        bool ok = true;
        try {
            const std::uint64_t leaf = tile_to_leaf_nest(2ULL, 0, 31);
            if (leaf != (2ULL << 62)) ok = false;
            if (leaf_to_tile_nest(leaf, 31, 0) != 2ULL) ok = false;
            const std::uint64_t v = child_nest(0x0FFFFFFFFFFFFFFFULL, 2); // bits=4 安全
            if (v != (0x0FFFFFFFFFFFFFFFULL << 4)) ok = false;
            if ((v >> 4) != 0x0FFFFFFFFFFFFFFFULL) ok = false;
        } catch (...) { ok = false; }
        expect(ok, "child_nest/tile_to_leaf_nest legal roundtrip");
    }

    std::printf("healpix neighbors/query_disc: pins=18, sym nsides=4, brute centers=16x4 radii x5 nsides, guards\n");
    if (g_fail == 0) {
        std::printf("RESULT: PASS (R9-B neighbors corner + query_disc disc completeness + guards)\n");
        return 0;
    }
    std::printf("RESULT: FAIL (fail=%llu)\n", (unsigned long long)g_fail);
    return 1;
}
