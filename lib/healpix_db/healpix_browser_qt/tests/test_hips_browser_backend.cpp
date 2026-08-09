// ============================================================================
// test_hips_browser_backend.cpp - Browser HiPS 后端 headless 核心测试
//
// 硬门 (G3):
//   - open product (signal/support/snr)
//   - 1024 确定性随机天球位置查询 signal/support
//   - 与直接 AIO Reader 逐值比较, mismatch=0
//   - SNR catalogue 读取 (star_id 唯一)
//   - outside MOC 返回 NaN/无数据
//   - FP32/FP64 读取正确
//
// 用法: test_hips_browser_backend <hips_root> [n_queries]
// ============================================================================

#include "hips_browser_backend.h"

#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"
#include "logger.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int kTileDim = 512;
constexpr uint64_t kTileMask = (1ULL << 18) - 1;

struct RowResult {
    double sig_b = 0, sup_b = 0;
    double sig_d = 0, sup_d = 0;
    bool has_data = false;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: test_hips_browser_backend <hips_root> [n_queries]\n");
        return 2;
    }
    const std::string root = argv[1];
    const int n_queries = (argc >= 3) ? std::atoi(argv[2]) : 1024;

    HipsBrowserBackend bk;
    if (bk.open_product(root) != 0) {
        std::fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    std::printf("open: order=%d leaf_order=%d fp64=%d tiles=%llu\n",
                bk.get_hips_order(), bk.get_leaf_order(), bk.is_fp64() ? 1 : 0,
                (unsigned long long)bk.get_n_tiles());

    // 读取 tile 列表 (直接 AIO, 作为引用)
    AioHipsDataset* dsig = aio_hips_open(root.c_str(), AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* dsup = aio_hips_open(root.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!dsig || !dsup) {
        std::fprintf(stderr, "direct AIO open FAIL\n");
        return 4;
    }
    std::vector<uint64_t> tiles;
    const int nt = aio_hips_tile_count(dsig);
    for (int i = 0; i < nt; ++i) {
        uint64_t ip = 0;
        if (aio_hips_tile_ipix(dsig, i, &ip) == 0) tiles.push_back(ip);
    }

    // 确定性随机: 从 MOC tile 内随机叶像素采样天球位置 (保证在 MOC 内)
    std::mt19937_64 rng(20260809ULL);
    const uint32_t leaf_nside = uint32_t(1) << (uint32_t)bk.get_leaf_order();
    const int n_total = (int)tiles.size() * 512 * 512;
    (void)n_total;
    const bool fp64 = bk.is_fp64();

    // 逐 tile 缓存直接 AIO 引用数据 (避免单数组跨 tile 覆盖)
    std::map<uint64_t, std::vector<double>> ref_sig;
    std::map<uint64_t, std::vector<double>> ref_sup;
    std::vector<float> tmp((size_t)kTileDim * kTileDim);

    long long mismatch = 0;
    long long no_data = 0;
    long long inside = 0;

    for (int q = 0; q < n_queries; ++q) {
        const uint64_t tile_ipix = tiles[(size_t)(rng() % tiles.size())];
        const uint64_t z = rng() & kTileMask;
        const uint64_t leaf_ipix = (tile_ipix << 18) | z;
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(leaf_nside, leaf_ipix, ra, dec);

        // Browser 后端
        double sig_b = 0, sup_b = 0;
        const int rc = bk.query_pixel(ra, dec, sig_b, sup_b);
        if (rc != 0) {
            std::fprintf(stderr, "query_pixel rc=%d @ (%.6f,%.6f)\n", rc, ra, dec);
            ++mismatch;
            continue;
        }
        // 直接 AIO 引用 (同一映射, 独立实现路径; 按 tile 缓存)
        if (ref_sig.count(tile_ipix) == 0) {
            std::vector<double> sig0((size_t)kTileDim * kTileDim);
            std::vector<double> sup0((size_t)kTileDim * kTileDim);
            if (fp64) {
                aio_hips_read_tile_f64(dsig, tile_ipix, sig0.data());
                aio_hips_read_tile_f64(dsup, tile_ipix, sup0.data());
            } else {
                aio_hips_read_tile_f32(dsig, tile_ipix, tmp.data());
                for (size_t i = 0; i < tmp.size(); ++i) sig0[i] = (double)tmp[i];
                aio_hips_read_tile_f32(dsup, tile_ipix, tmp.data());
                for (size_t i = 0; i < tmp.size(); ++i) sup0[i] = (double)tmp[i];
            }
            ref_sig[tile_ipix] = std::move(sig0);
            ref_sup[tile_ipix] = std::move(sup0);
        }
        const uint64_t idx = astrocs::healpix::nested_local_to_fits_index(z, 9u, 512u);
        const double sig_d = ref_sig[tile_ipix][idx];
        const double sup_d = ref_sup[tile_ipix][idx];

        const bool d_ok = std::isfinite(sig_d);
        const bool b_ok = std::isfinite(sig_b);
        ++inside;
        if (!d_ok) ++no_data;
        if (b_ok != d_ok ||
            (b_ok && std::fabs(sig_b - sig_d) > 1e-6) ||
            std::fabs(sup_b - sup_d) > 1e-9) {
            ++mismatch;
            if (mismatch <= 5) {
                std::fprintf(stderr, "MISMATCH q=%d (%.6f,%.6f) sig_b=%g sig_d=%g sup_b=%g sup_d=%g\n",
                             q, ra, dec, sig_b, sig_d, sup_b, sup_d);
            }
        }
    }

    // outside MOC: 取一个远离产品的位置 (南天极方向, 产品在赤道附近时必在 MOC 外)
    int outside_ok = 0;
    for (int i = 0; i < 64; ++i) {
        const double ra = (double)(rng() % 3600) / 10.0;
        const double dec = -89.0 + (double)(rng() % 100) / 100.0;  // 靠近南天极
        double sig = 0, sup = 0;
        const int rc = bk.query_pixel(ra, dec, sig, sup);
        if (rc == -2 || (rc == 0 && !std::isfinite(sig))) ++outside_ok;
    }

    // SNR catalogue
    std::vector<double> cra, cdec, csnr;
    std::vector<int64_t> cid;
    std::vector<uint32_t> cqf, cps;
    const int n_snr = bk.read_snr_catalog(cra, cdec, csnr, cid, cqf, cps);
    std::set<int64_t> id_set;
    for (auto id : cid) id_set.insert(id);
    const bool id_unique = ((int)id_set.size() == n_snr);

    std::printf("RESULT: queries=%d inside=%lld no_data=%lld mismatch=%lld "
                "outside_ok=%d/64 snr_rows=%d snr_id_unique=%d\n",
                n_queries, inside, no_data, mismatch, outside_ok, n_snr, id_unique ? 1 : 0);

    aio_hips_close(dsig);
    aio_hips_close(dsup);

    const bool pass = (mismatch == 0 && outside_ok >= 1 && n_snr > 0 && id_unique);
    std::printf("RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
