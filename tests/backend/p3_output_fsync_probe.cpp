// tests/backend/p3_output_fsync_probe.cpp — R10-C fsync/sha256 失败注入探针
// 与 p3_output_probe_main.cpp 同源数据语义, 专供失败分支断言:
//   write <out> <W> <H> <seed>
//       正常写 → "OK <sha256> <covered> <total>" | "FAIL <code>"
//   verify_missing <path>
//       对不存在/不可读路径 verify → "IO sha_len=<n>" | "BADOK sha_len=<n>"
//       断言点: 必须报 P3_OUT_IO 且 result.sha256 不携带 64 位假哈希。
// 失败注入方式:
//   1) 读失败: 构建时 -Dastrocs_hash_fail_inject + 运行时 ASTROCS_HASH_FAIL_INJECT=1
//      (p3_output.cpp 的测试钩子: sha256_file_checked 在 final 前注入一次错误)
//   2) 不存在文件: verify_missing 直接以不存在路径调用 verify。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
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
            const bool cov_pt = (x >= W / 4 && x < 3 * W / 4 && y >= H / 4 && y < 3 * H / 4);
            sig[i] = cov_pt ? (float)(seed * 0.001 + i) : std::numeric_limits<float>::quiet_NaN();
            cov[i] = cov_pt ? 1.0f : 0.0f;
        }
}

static int do_write(const char* out, int W, int H, int seed) {
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
        sig.data(), cov.data(), W, H, &w, "ADU", out, &pv, -32, -1, &r);
    if (st != P3_OUT_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
    std::printf("OK %s %ld %ld\n", r.sha256, r.covered_px, r.total_px);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string mode = argv[1];
    if (mode == "write" && argc >= 6) {
        return do_write(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    }
    if (mode == "verify_missing" && argc >= 3) {
        P3WcsDescriptor w{};
        P3OutputResult r{};
        std::vector<float> sig(64, 1.0f), cov(64, 1.0f);
        w.crpix_x = 4.5; w.crpix_y = 4.5;
        w.crval_ra_deg = 210.0; w.crval_dec_deg = 34.0;
        w.cd[0][0] = -0.001; w.cd[0][1] = 0; w.cd[1][0] = 0; w.cd[1][1] = 0.001;
        w.width_px = 8; w.height_px = 8;
        const P3OutputStatus st = p3_output_verify(argv[2], &w, sig.data(), cov.data(),
                                                   8, 8, &r);
        if (st == P3_OUT_OK) {
            std::printf("BADOK sha_len=%zu\n", std::strlen(r.sha256));
            return 1;
        }
        std::printf("IO sha_len=%zu\n", std::strlen(r.sha256));
        return 0;
    }
    return 2;
}
