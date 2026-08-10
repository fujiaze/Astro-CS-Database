// lib/phase2/tests/sanitize_driver.cpp — Phase2 G10 sanitizer 驱动（WSL）
//
// 用法（Linux/WSL，聚焦无 AIO 依赖的核心数值模块）：
//   g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
//       -I <repo>/lib/phase2/include -I <nlohmann_include> \
//       sanitize_driver.cpp ../src/upm.cpp ../src/sha256.cpp \
//       ../src/rejection.cpp ../src/block.cpp ../src/integrate.cpp \
//       -o p2_san
//   ASAN_OPTIONS=detect_leaks=1 ./p2_san
#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/block.h"
#include "astro/phase2/integrate.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

int main() {
    // UPM build/calibrate/save/open/materialize/dense/stale
    std::vector<P2ControlObservation> obs;
    for (int f = 0; f < 4; ++f) {
        for (int k = 0; k < 8; ++k) {
            P2ControlObservation o{};
            o.frame_id = (std::uint64_t)f;
            o.control_id = (std::uint64_t)k;
            o.leaf_ipix = (std::uint64_t)k;
            o.ra_deg = (double)k;
            o.dec_deg = (double)f;
            o.value = 10.0 + 0.3 * f + 0.1 * k;
            o.snr = 5.0 + f;
            o.support = 1.0;
            obs.push_back(o);
        }
    }
    void* m = nullptr;
    if (p2_upm_build(obs.data(), obs.size(), nullptr, &m) != 0) {
        std::fprintf(stderr, "build failed\n");
        return 1;
    }
    std::uint64_t ip[4] = {0, 1, 2, 3};
    double in[4] = {10, 11, 12, 13};
    double out[4] = {0, 0, 0, 0};
    p2_upm_calibrate_block(m, 0, ip, in, out, 4);
    const char* upm_path = "/tmp/p2_upm.json";
    const char* dc = "/tmp/p2_dense.cache";
    if (p2_upm_save(m, upm_path) != 0) return 2;
    void* m2 = nullptr;
    if (p2_upm_open(upm_path, &m2) != 0) return 3;
    if (p2_upm_materialize_dense(m, 0, dc) != 0) return 4;
    if (p2_upm_dense_read_block(m2, dc, 1, ip, in, out, 4) != 0) return 5;
    p2_upm_dense_info(m2, dc, nullptr, nullptr, nullptr, 0);
    p2_upm_close(m2);
    p2_upm_close(m);

    // rejection：全部 7 种方法
    double vals[64];
    for (int i = 0; i < 64; ++i) vals[i] = 10.0 + 0.01 * i;
    vals[10] = 99.0;
    vals[20] = -88.0;
    for (int method = 0; method <= 6; ++method) {
        std::uint8_t acc[64] = {0};
        P2SampleStackView in2{};
        in2.values = vals;
        in2.count = 64;
        in2.data_type = 1;
        in2.method = method;
        in2.sigma_low = -4.0;
        in2.sigma_high = 3.0;
        in2.max_iterations = 8;
        in2.min_samples = 3;
        P2RejectionResult rr{};
        rr.accepted = acc;
        if (p2_reject_stack(&in2, &rr) != 0) return 6;
    }
    // NaN 输入
    double vals2[5] = {1, 2, std::nan(""), 4, 5};
    std::uint8_t acc2[5] = {0};
    P2SampleStackView in3{};
    in3.values = vals2;
    in3.count = 5;
    in3.method = P2_REJECT_SIGMA;
    in3.min_samples = 2;
    P2RejectionResult rr2{};
    rr2.accepted = acc2;
    if (p2_reject_stack(&in3, &rr2) != 0) return 7;

    // block plan
    P2BlockPlannerInput bp{};
    bp.output_pixels = 1000;
    bp.covering_frames = 3;
    bp.precision = 1;
    bp.memory_limit_bytes = 1u << 20;
    P2BlockPlan bpo{};
    if (p2_block_plan(&bp, &bpo) != 0) return 8;

    // integrate
    double w[3] = {0.5, 0.3, 0.2};
    double sup_arr[3] = {1, 1, 1};
    std::uint8_t acc3[3] = {1, 1, 1};
    P2PixelStack pi{};
    pi.values = vals;
    pi.weights = w;
    pi.support = sup_arr;
    pi.accepted = acc3;
    pi.count = 3;
    P2PixelResult pr{};
    if (p2_integrate_pixel(&pi, &pr) != 0) return 9;

    std::printf("sanitize driver ok\n");
    return 0;
}
