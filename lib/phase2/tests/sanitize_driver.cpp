// lib/phase2/tests/sanitize_driver.cpp — Phase2 G10 sanitizer 驱动（WSL）
//
// 用法（Linux/WSL，聚焦无 AIO 依赖的核心数值模块）：
// g++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
// -I <repo>/lib/phase2/include -I <nlohmann_include> \
// sanitize_driver.cpp ../src/upm.cpp \
// <repo>/lib/common/crypto/sha256.cpp \
// ../src/rejection.cpp ../src/block.cpp ../src/integrate.cpp \
// -o p2_san
// ASAN_OPTIONS=detect_leaks=1 ./p2_san
#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/block.h"
#include "astro/phase2/integrate.h"
#include "astro/phase2/acr_kernels.h"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "cuda_bridge_api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <random>
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
            //production 权重要求 control_ivar（显式填充）
            o.control_variance = 1.0;
            o.control_ivar = 1.0;
            obs.push_back(o);
        }
    }
    P2UpmBuildConfig bcfg{};
    bcfg.target_order = 7;
    bcfg.smoothing_lambda = 0.1;
    bcfg.max_iterations = 20;
    void* m = nullptr;
    if (p2_upm_build(obs.data(), obs.size(), &bcfg, &m) != 0) {
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

    // ---- 随机 fuzz 压力（确定性 seed，覆盖边界/损坏输入） ----
    std::mt19937_64 rng(20260810);
    std::uniform_int_distribution<int> method_dist(0, 6);
    std::uniform_int_distribution<int> n_dist(0, 40);
    std::uniform_real_distribution<double> v_dist(-1e3, 1e3);
    std::uniform_real_distribution<double> s_dist(-10.0, 10.0);
    for (int iter = 0; iter < 2000; ++iter) {
        const int n = n_dist(rng);
        std::vector<double> vals(n);
        for (auto& x : vals) {
            x = v_dist(rng);
            if ((rng() % 20) == 0) x = std::nan("");
            if ((rng() % 25) == 0) x = std::numeric_limits<double>::infinity();
        }
        std::vector<std::uint8_t> acc(n);
        P2SampleStackView fin{};
        fin.values = vals.data();
        fin.count = (std::uint32_t)n;
        fin.method = method_dist(rng);
        fin.sigma_low = s_dist(rng);
        fin.sigma_high = s_dist(rng);
        fin.max_iterations = (int)(rng() % 16);
        fin.min_samples = (int)(rng() % 8);
        P2RejectionResult fr{};
        fr.accepted = acc.data();
        if (p2_reject_stack(&fin, &fr) != 0) return 10;
        P2PixelStack fpi{};
        fpi.values = vals.data();
        fpi.accepted = acc.data();
        fpi.count = (std::uint32_t)n;
        P2PixelResult fpr{};
        if (p2_integrate_pixel(&fpi, &fpr) != 0) return 11;
    }
    // 随机 UPM 观测
    for (int iter = 0; iter < 100; ++iter) {
        std::vector<P2ControlObservation> fobs;
        const int n_obs = (int)(rng() % 60);
        for (int i = 0; i < n_obs; ++i) {
            P2ControlObservation o{};
            o.frame_id = rng() % 8;
            o.control_id = rng() % 12;
            o.leaf_ipix = rng();
            o.ra_deg = v_dist(rng);
            o.dec_deg = v_dist(rng);
            o.value = v_dist(rng);
            o.uncertainty = std::fabs(v_dist(rng));
            o.snr = std::fabs(v_dist(rng));
            o.support = std::fabs(v_dist(rng));
            o.quality_flags = (std::uint32_t)rng();
            fobs.push_back(o);
        }
        void* fm = nullptr;
        if (n_obs == 0) {
        if (p2_upm_build(fobs.data(), 0, &bcfg, &fm) == 0) return 12;
            continue;
        }
        if (p2_upm_build(fobs.data(), (std::uint64_t)fobs.size(), &bcfg,
                         &fm) != 0) {
            continue;  // 空/退化输入可拒绝
        }
        P2UpmBuildConfig fcfg{};
        fcfg.zero_anchor_weight = s_dist(rng);
        fcfg.huber_delta = std::fabs(v_dist(rng));
        fcfg.max_iterations = (int)(rng() % 10);
        void* fm2 = nullptr;
        if (p2_upm_build(fobs.data(), (std::uint64_t)fobs.size(), &fcfg,
                         &fm2) == 0) {
            p2_upm_close(fm2);
        }
        p2_upm_close(fm);
    }

    //integration 零权重合同（ZERO_VALID_WEIGHT 可达路径）
    {
        double vals[2] = {10.0, 12.0};
        double w[2] = {1.0, 0.0};
        double sup[2] = {1.0, 1.0};
        P2PixelStack pi{};
        pi.values = vals;
        pi.weights = w;
        pi.support = sup;
        pi.count = 2;
        P2PixelResult pr{};
        if (p2_integrate_pixel(&pi, &pr) != 0) return 21;
        if (pr.status != P2_INTEGRATE_OK) return 22;
        if (pr.n_positive_weight != 1u) return 23;
        if (p2_validate_candidate_weights(w, 2) != 0) return 24;
    }

    //ACR CPU/reference（synthetic mosaic_reject，CPU launcher）
    {
        astro::compute::phase2::register_phase2_acr_kernels();
        const astro::compute::KernelRegistration* reg =
            astro::compute::global_kernel_registry().find(
                astro::compute::phase2::kOpMosaicReject);
        if (reg == nullptr || reg->legacy_parallel == nullptr) return 31;
        const std::size_t px = 2, depth = 2;
        float vals[4] = {10.0f, 20.0f, 11.0f, 19.0f};
        float out[2] = {0, 0};
        astro::compute::KernelInvocation inv;
        inv.id = astro::compute::phase2::kOpMosaicReject;
        inv.domain = astro::compute::WorkDomain{0, px};
        inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
        inv.buffers.add(1, vals, px * depth, 1,
                        astro::compute::BufferRole::Input);
        astro::compute::append_scalar(inv.scalars, std::size_t{px});
        astro::compute::append_scalar(inv.scalars, std::size_t{depth});
        astro::compute::append_scalar(inv.scalars, int{P2_REJECT_SIGMA});
        astro::compute::append_scalar(inv.scalars, int{2});
        astro::compute::append_scalar(inv.scalars, double{4.0});
        astro::compute::append_scalar(inv.scalars, double{3.0});
        astro::compute::append_scalar(inv.scalars, int{8});
        astro::compute::append_scalar(inv.scalars, std::size_t{0});
        astro::compute::append_scalar(inv.scalars, int{0});
        reg->legacy_parallel(inv, nullptr);
    }

    std::printf("sanitize driver ok\n");
    return 0;
}
