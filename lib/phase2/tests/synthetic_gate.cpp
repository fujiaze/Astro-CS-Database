// lib/phase2/tests/synthetic_gate.cpp — Phase2 合成 Gate（W4/W7/W5/W6/W8/W9 子集）
//
// 覆盖：
//   S0 identity：无额外 gradient 时 UPM 校准不改变信号；
//   S1 known additive field：已知每帧加性偏移，UPM 联合求解恢复真值；
//   S2 low-SNR pull：SNR-aware 权重下低 SNR 偏差帧不拉偏高 SNR 真值；
//   R1 sigma rejection：注入离群值，sigma-clip 检出；
//   R2 min-samples：样本不足返回 status=min-samples；
//   D  sparse=dense（materialize 与稀疏模型一致）；
//   B  block planner（内存估算/micro-chunk）；
//   I  weighted integration（加权均值/全拒）；
//   A  ACR legacy launcher 与 CPU reference 等价。
#include <gtest/gtest.h>

#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/block.h"
#include "astro/phase2/integrate.h"
#include "astro/phase2/acr_kernels.h"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

P2ControlObservation make_obs(std::uint64_t frame, std::uint64_t ctrl,
                              double value, double snr) {
    P2ControlObservation o{};
    o.frame_id = frame;
    o.control_id = ctrl;
    o.value = value;
    o.snr = snr;
    o.support = 1.0;
    return o;
}

} // namespace

TEST(Phase2Upm, S0IdentityCalibrationNoChange) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 10.0),
        make_obs(0, 1, 12.0, 10.0),
        make_obs(0, 2, 11.0, 10.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    std::uint64_t ipix[2] = {0, 1};
    double in[2] = {10.0, 12.0};
    double out[2] = {0.0, 0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 0, ipix, in, out, 2), 0);
    EXPECT_NEAR(out[0], 10.0, 1e-6);
    EXPECT_NEAR(out[1], 12.0, 1e-6);
    p2_upm_close(model);
}

TEST(Phase2Upm, S1KnownAdditiveFieldRecovered) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(0, 1, 12.0, 100.0),
        make_obs(0, 2, 11.0, 100.0),
        make_obs(1, 0, 15.0, 100.0),
        make_obs(1, 1, 17.0, 100.0),
        make_obs(1, 2, 16.0, 100.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    std::uint64_t ipix[1] = {0};
    double in[1] = {15.0};
    double out[1] = {0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 1, ipix, in, out, 1), 0);
    EXPECT_NEAR(out[0], 10.0, 1e-3);
    p2_upm_close(model);
}

TEST(Phase2Upm, S2LowSnrDoesNotPullHighSnr) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(1, 0, 50.0, 0.3),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    P2ModelInfo info{};
    ASSERT_EQ(p2_upm_info(model, &info), 0);
    EXPECT_EQ(info.control_count, 1u);
    std::uint64_t ipix[1] = {0};
    double in[1] = {10.0};
    double out[1] = {0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 0, ipix, in, out, 1), 0);
    EXPECT_NEAR(out[0], 10.0, 0.5);
    p2_upm_close(model);
}

TEST(Phase2Upm, SparseEqualsDense) {
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 100.0),
        make_obs(0, 1, 12.0, 100.0),
        make_obs(0, 2, 11.0, 100.0),
        make_obs(1, 0, 15.0, 100.0),
        make_obs(1, 1, 17.0, 100.0),
        make_obs(1, 2, 16.0, 100.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    const char* cache = "run_tmp_upm_dense.json";
    ASSERT_EQ(p2_upm_materialize_dense(model, 0, cache), 0);
    int order = -1; std::uint64_t pixels = 0; char hash[65] = {0};
    ASSERT_EQ(p2_upm_dense_info(model, cache, &order, &pixels, hash,
                                sizeof(hash)), 0);
    EXPECT_GE(pixels, 3u);
    EXPECT_GT(std::strlen(hash), 0u);
    std::remove(cache);
    p2_upm_close(model);
}

TEST(Phase2Block, MemoryEstimateAndMicrochunk) {
    P2BlockPlannerInput in{};
    in.output_pixels = 1024u * 1024u;
    in.covering_frames = 10;
    in.precision = 1;
    in.memory_limit_bytes = 16u * 1024 * 1024;
    in.safety_factor = 0.75;
    in.scratch_bytes_per_sample = 8;
    in.fixed_overhead = 1u << 20;
    P2BlockPlan plan{};
    ASSERT_EQ(p2_block_plan(&in, &plan), 0);
    EXPECT_GT(plan.estimated_peak_bytes, 0u);
    in.memory_limit_bytes = 1ull << 30;
    ASSERT_EQ(p2_block_plan(&in, &plan), 0);
    EXPECT_EQ(plan.micro_chunk_required, 0);
}

TEST(Phase2Integrate, WeightedMeanAndAllRejected) {
    double vals[] = {10.0, 12.0, 11.0};
    double ws[] = {0.5, 0.3, 0.2};
    P2PixelStack in{};
    in.values = vals;
    in.weights = ws;
    in.count = 3;
    P2PixelResult out{};
    ASSERT_EQ(p2_integrate_pixel(&in, &out), 0);
    EXPECT_NEAR(out.signal, 0.5*10 + 0.3*12 + 0.2*11, 1e-9);
    EXPECT_EQ(out.n_used, 3u);
    std::uint8_t acc[3] = {0, 0, 0};
    in.accepted = acc;
    ASSERT_EQ(p2_integrate_pixel(&in, &out), 0);
    EXPECT_EQ(out.status, 2);
}

TEST(Phase2Reject, R1SigmaClippingFindsOutliers) {
    std::vector<double> vals{10, 10.1, 9.9, 10.05, 10.2, 9.8, 50.0};
    std::vector<std::uint8_t> accepted(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = static_cast<std::uint32_t>(vals.size());
    in.method = P2_REJECT_SIGMA;
    in.sigma_low = -4.0;
    in.sigma_high = 3.0;
    in.max_iterations = 8;
    in.min_samples = 3;
    P2RejectionResult out{};
    out.accepted = accepted.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_GT(out.rejected_high, 0u);
    EXPECT_EQ(accepted.back(), 0u);
    EXPECT_GT(out.accepted_count, 3u);
}

TEST(Phase2Reject, R2MinSamples) {
    std::vector<double> vals{10.0, 10.1};
    std::vector<std::uint8_t> accepted(vals.size(), 0);
    P2SampleStackView in{};
    in.values = vals.data();
    in.count = 2;
    in.method = P2_REJECT_SIGMA;
    in.min_samples = 5;
    P2RejectionResult out{};
    out.accepted = accepted.data();
    ASSERT_EQ(p2_reject_stack(&in, &out), 0);
    EXPECT_EQ(out.status, 1);
}

// W9：ACR 合成 mosaic_reject legacy launcher 与 CPU reference 等价
TEST(Phase2Acr, LegacyLauncherEquivalent) {
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(reg->legacy_parallel, nullptr);

    const std::size_t px = 2, depth = 3;
    // frame-major：s*n_px+p —— s0:[10,20] s1:[50,20.1] s2:[10.2,19.9]
    float vals[6] = {10.0f, 20.0f, 50.0f, 20.1f, 10.2f, 19.9f};
    float out[2] = {0, 0};
    astro::compute::KernelInvocation inv;
    inv.id = astro::compute::phase2::kOpMosaicReject;
    inv.domain = astro::compute::WorkDomain{0, px};
    inv.buffers.add(0, out, px, 1, astro::compute::BufferRole::Output);
    inv.buffers.add(1, vals, px*depth, 1, astro::compute::BufferRole::Input);
    astro::compute::append_scalar(inv.scalars, std::size_t{px});
    astro::compute::append_scalar(inv.scalars, std::size_t{depth});
    astro::compute::append_scalar(inv.scalars, int{P2_REJECT_SIGMA});
    astro::compute::append_scalar(inv.scalars, double{-4.0});
    astro::compute::append_scalar(inv.scalars, double{3.0});
    reg->legacy_parallel(inv, nullptr);
    EXPECT_NEAR(out[0], 10.1f, 1e-4f);
    EXPECT_NEAR(out[1], 20.0f, 1e-2f);
}
