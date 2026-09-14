// P15a exploration + regression probe: drizzle 线程预算不变式 (determinism)
//
// 目的: 在固定输入下, 以显式 config.threads 扫描线程预算, 输出
//   <prefix>.hiss    — writeHisTilesT 产物 (与生产 p1_stack.hiss 同路径)
//   <prefix>.canon   — 逐 leaf 规范化转储 (按 (parent,local) 排序, bit 级)
//   <prefix>.order   — tiles 向量的返回顺序 (tile directory 写盘顺序)
// 用于逐位比对不同线程预算/重复运行是否完全一致。
#include "drizzle_engine.h"

#include "p1drz_fixtures.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <string>
#include <vector>

using drizzle::DrizzleConfig;
using drizzle::DrizzleEngine;
using drizzle::DrizzleMeta;
using drizzle::DrizzleStats;
using drizzle::FitsImage;
using drizzle::TileAccumulatorT;

template <typename T>
static uint64_t bits_of(T v) {
    uint64_t u = 0;
    static_assert(sizeof(T) == sizeof(uint64_t) || sizeof(T) == sizeof(uint32_t), "size");
    std::memcpy(&u, &v, sizeof(T));
    return u;
}

template <typename Scalar>
static int run_one(const char* prefix, int threads, int nside, int W, int H,
                   double amp, double sigma, uint64_t seed, bool fp64) {
    FitsImage img = p1drz::fix_drz_b_gaussian(W, H, amp, sigma, seed, 0.0);
    DrizzleConfig cfg = p1drz::make_cfg(nside, 1.0, threads, fp64);
    DrizzleEngine eng;
    DrizzleStats st;
    std::string err;
    std::vector<TileAccumulatorT<Scalar>> tiles;
    bool ok;
    if constexpr (std::is_same_v<Scalar, double>) {
        img.use_f64 = true;
        ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr, tiles, st, err);
    } else {
        img.use_f64 = false;
        ok = eng.drizzleTiled(img, cfg, nullptr, nullptr, nullptr, tiles, st, err);
    }
    if (!ok) {
        std::fprintf(stderr, "drizzle failed: %s\n", err.c_str());
        return 2;
    }
    // 写 .hiss (与生产相同的写链)
    {
        std::string hiss = std::string(prefix) + ".hiss";
        DrizzleMeta meta;
        meta.filter = "TEST";
        meta.exposure_s = 1.0;
        meta.obs_time = "2026-01-01T00:00:00Z";
        std::string werr;
        bool wok = eng.writeHisTilesT<Scalar>(tiles, st, img.wcs, cfg, meta, "", hiss,
                                             nullptr, nullptr, werr);
        if (!wok) {
            std::fprintf(stderr, "write failed: %s\n", werr.c_str());
            return 3;
        }
        // 归一化副本: wall-clock 元数据 (history elapsed=%.3fs) 天然不可复现,
        // 不属于科学载荷。归零 elapsedSec 后再写一份, 用于跨线程预算的
        // 产物级 sha256 不变式 (科学载荷 = leaf 值 + tile 集合 + tile 目录序)。
        DrizzleStats st_norm = st;
        st_norm.elapsedSec = 0.0;
        std::string hiss_norm = std::string(prefix) + ".norm.hiss";
        std::string werr2;
        bool wok2 = eng.writeHisTilesT<Scalar>(tiles, st_norm, img.wcs, cfg, meta, "",
                                              hiss_norm, nullptr, nullptr, werr2);
        if (!wok2) {
            std::fprintf(stderr, "write(norm) failed: %s\n", werr2.c_str());
            return 3;
        }
    }
    // 规范化转储 + 顺序转储
    {
        std::string canon = std::string(prefix) + ".canon";
        FILE* f = std::fopen(canon.c_str(), "w");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", canon.c_str()); return 4; }
        std::vector<std::pair<uint64_t, const TileAccumulatorT<Scalar>*>> sorted;
        for (const auto& t : tiles) {
            if (t.touched.empty()) continue;
            sorted.push_back({t.parent_ipix, &t});
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [parent, t] : sorted) {
            std::vector<uint32_t> locals = t->touched;
            std::sort(locals.begin(), locals.end());
            for (uint32_t local : locals) {
                const auto& acc = t->pixels[local];
                std::fprintf(f, "%llu %u %016llx %016llx %016llx %u\n",
                             (unsigned long long)parent, local,
                             (unsigned long long)bits_of<Scalar>(acc.sumFlux),
                             (unsigned long long)bits_of<Scalar>(acc.sumArea),
                             (unsigned long long)bits_of<Scalar>(acc.sumVarNum),
                             acc.nContrib);
            }
        }
        std::fclose(f);
        std::string order = std::string(prefix) + ".order";
        FILE* g = std::fopen(order.c_str(), "w");
        if (!g) { std::fprintf(stderr, "cannot open %s\n", order.c_str()); return 4; }
        for (const auto& t : tiles) {
            if (t.touched.empty()) continue;
            std::fprintf(g, "%llu\n", (unsigned long long)t.parent_ipix);
        }
        std::fclose(g);
    }
    std::fprintf(stderr, "[probe] threads=%d tiles=%zu npix=%lld src=%lld\n", threads,
                 tiles.size(), (long long)st.nHealpixPixels, (long long)st.nSourcePixels);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 9) {
        std::fprintf(stderr,
                     "usage: %s <prefix> <threads> <nside> <W> <H> <amp> <sigma> <seed> [fp64]\n",
                     argv[0]);
        return 1;
    }
    const std::string prefix = argv[1];
    const int threads = std::atoi(argv[2]);
    const int nside = std::atoi(argv[3]);
    const int W = std::atoi(argv[4]);
    const int H = std::atoi(argv[5]);
    const double amp = std::atof(argv[6]);
    const double sigma = std::atof(argv[7]);
    const uint64_t seed = std::strtoull(argv[8], nullptr, 10);
    const bool fp64 = (argc > 9) && std::string(argv[9]) == "fp64";
    if (fp64) {
        return run_one<double>(prefix.c_str(), threads, nside, W, H, amp, sigma, seed, true);
    }
    return run_one<float>(prefix.c_str(), threads, nside, W, H, amp, sigma, seed, false);
}
