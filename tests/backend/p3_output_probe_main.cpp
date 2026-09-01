// tests/backend/p3_output_probe_main.cpp — P3-004 输出 FITS 原子写探针
// 用法:
//   probe write <out> <W> <H> <cancel_row> <seed>
//       → "OK <sha256> <covered> <total>" | "CANCELLED" | "FAIL <code>"
//   probe verify <out> <W> <H> <seed>
//       → "OK <reopen> <covok> <covered> <total>" | "FAIL <code>"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <limits>
#include <vector>

#include "p3_output.h"
#include "p3_wcs.h"

using namespace astrocs::phase3;

static void fill(int W, int H, int seed, std::vector<float>& sig,
                 std::vector<float>& cov) {
    sig.assign((size_t)W * H, 0.0f);
    cov.assign((size_t)W * H, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int i = y * W + x;
            // 半幅矩形覆盖(0.25W..0.75W, 0.25H..0.75H): 非平凡 coverage
            const bool cov_pt = (x >= W / 4 && x < 3 * W / 4 && y >= H / 4 && y < 3 * H / 4);
            sig[i] = cov_pt ? (float)(seed * 0.001 + i) : std::numeric_limits<float>::quiet_NaN();
            cov[i] = cov_pt ? 1.0f : 0.0f;
        }
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string mode = argv[1];
    if (mode == "write") {
        if (argc < 7) return 2;
        const char* out = argv[2];
        const int W = atoi(argv[3]), H = atoi(argv[4]);
        const int cancel = atoi(argv[5]);
        const int seed = atoi(argv[6]);
        std::vector<float> sig, cov;
        fill(W, H, seed, sig, cov);
        P3WcsDescriptor w{};
        w.crpix_x = (W + 1) / 2.0; w.crpix_y = (H + 1) / 2.0;
        w.crval_ra_deg = 210.0; w.crval_dec_deg = 34.0;
        w.cd[0][0] = -0.001; w.cd[0][1] = 0; w.cd[1][0] = 0; w.cd[1][1] = 0.001;
        w.width_px = W; w.height_px = H;
        P3Provenance pv{"ivo://astrocs/test_p3", "deadbeef", nullptr, 0, "0.1.0",
                        "run-1", "0", "bilinear"};
        P3OutputResult r{};
        const P3OutputStatus st = p3_output_write_atomic(
            sig.data(), cov.data(), W, H, &w, "ADU", out, &pv, cancel, &r);  // P3-002: 面亮度 ADU, 非 Jy/beam
        if (st == P3_OUT_CANCELLED) { std::printf("CANCELLED\n"); return 0; }
        if (st != P3_OUT_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
        std::printf("OK %s %ld %ld\n", r.sha256, r.covered_px, r.total_px);
        return 0;
    }
    if (mode == "verify") {
        if (argc < 6) return 2;
        const char* out = argv[2];
        const int W = atoi(argv[3]), H = atoi(argv[4]);
        const int seed = atoi(argv[5]);
        std::vector<float> sig, cov;
        fill(W, H, seed, sig, cov);
        P3WcsDescriptor w{};
        w.crpix_x = (W + 1) / 2.0; w.crpix_y = (H + 1) / 2.0;
        w.crval_ra_deg = 210.0; w.crval_dec_deg = 34.0;
        w.cd[0][0] = -0.001; w.cd[0][1] = 0; w.cd[1][0] = 0; w.cd[1][1] = 0.001;
        w.width_px = W; w.height_px = H;
        P3OutputResult r{};
        const P3OutputStatus st = p3_output_verify(out, &w, sig.data(), cov.data(), W, H, &r);
        if (st != P3_OUT_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
        std::printf("OK %d %d %ld %ld\n", r.reopen_ok, r.coverage_ok, r.covered_px,
                    r.total_px);
        return 0;
    }
    return 2;
}
