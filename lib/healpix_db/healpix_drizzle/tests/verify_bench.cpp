// verify_bench.cpp - HISS Verify 单句柄基准 (session 遍历全部 Tile)
#include "aio_healpix_io.h"
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cmath>

static double now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "run/temp/bench_write.hiss";
    uint32_t nside = 0, tile_nside = 0;
    uint64_t n_tiles = 0;
    double t0 = now_s();
    void* sess = aio_hiss_open_session(path, &nside, &tile_nside, &n_tiles);
    double t1 = now_s();
    if (!sess) { printf("open session 失败\n"); return 1; }
    printf("open session: %.3fs (nside=%u tile_nside=%u tiles=%llu)\n",
           t1 - t0, nside, tile_nside, (unsigned long long)n_tiles);
    // 模拟 verify: 遍历全部 tile 读 signal/support/snr + 完整性检查
    // 需要 tile 列表: 从 HissReader 拿 (session 未暴露 tiles) — 用 inspect 先拿
    uint32_t depth = 0, n_leaf = 0;
    uint64_t n_pix = 0;
    char* meta = nullptr;
    uint64_t* tiles = nullptr;
    if (aio_hiss_inspect(path, &nside, &tile_nside, &depth, &n_leaf,
                         &n_tiles, &n_pix, &meta, &tiles) != 0) {
        printf("inspect 失败\n"); return 1;
    }
    double t2 = now_s();
    uint64_t passed = 0, snr_total = 0;
    bool naninf = false, allzero = true;
    for (uint64_t i = 0; i < n_tiles; i++) {
        float* sig = nullptr; uint32_t ns = 0;
        uint8_t* sup = nullptr; uint32_t nc = 0;
        uint8_t* snr = nullptr; uint32_t nsnr = 0;
        if (aio_hiss_read_tile_signal_session(sess, tiles[i], &sig, &ns) == 0 && sig && ns > 0) {
            for (uint32_t j = 0; j < ns; j++) {
                if (!std::isfinite(sig[j])) naninf = true;
                if (sig[j] != 0.0f) allzero = false;
            }
        }
        if (aio_hiss_read_tile_support_session(sess, tiles[i], &sup, &nc) != 0) {
            printf("tile %llu support 失败\n", (unsigned long long)i);
        }
        if (aio_hiss_read_tile_snr_session(sess, tiles[i], &snr, &nsnr) == 0)
            snr_total += nsnr;
        aio_hio_free(sig); aio_hio_free(sup); aio_hio_free(snr);
        passed++;
    }
    double t3 = now_s();
    printf("verify 遍历: %.3fs (%.1f ms/tile, tiles=%llu) snr_points=%llu naninf=%d allzero=%d\n",
           t3 - t2, (t3 - t2) / (double)n_tiles * 1000.0,
           (unsigned long long)n_tiles, (unsigned long long)snr_total,
           naninf ? 1 : 0, allzero ? 1 : 0);
    aio_hio_free(meta); aio_hio_free(tiles);
    aio_hiss_close_session(sess);
    return 0;
}
