// lib/phase2/tests/synthetic_gate.cpp — Phase2 合成 Gate（W4/W7 子集）
//
// 覆盖：
//   S0 identity：无额外 gradient 时 UPM 校准不改变信号；
//   S1 known additive field：已知每帧加性偏移，UPM 联合求解恢复真值；
//   S2 low-SNR pull：SNR-aware 权重下低 SNR 偏差帧不拉偏高 SNR 真值；
//   R1 sigma rejection：注入离群值，sigma-clip 检出；
//   R2 min-samples：样本不足返回 status=min-samples。
#include <gtest/gtest.h>

#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"

#include <cmath>
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
    // 单帧、无梯度：offset≈0，calibrate 不改变信号
    std::vector<P2ControlObservation> obs{
        make_obs(0, 0, 10.0, 10.0),
        make_obs(0, 1, 12.0, 10.0),
        make_obs(0, 2, 11.0, 10.0),
    };
    P2UpmBuildConfig cfg{};
    void* model = nullptr;
    ASSERT_EQ(p2_upm_build(obs.data(), obs.size(), &cfg, &model), 0);
    ASSERT_NE(model, nullptr);
    std::uint64_t ipix[2] = {0, 1};
    double in[2] = {10.0, 12.0};
    double out[2] = {0.0, 0.0};
    ASSERT_EQ(p2_upm_calibrate_block(model, 0, ipix, in, out, 2), 0);
    EXPECT_NEAR(out[0], 10.0, 1e-6);
    EXPECT_NEAR(out[1], 12.0, 1e-6);
    p2_upm_close(model);
}

TEST(Phase2Upm, S1KnownAdditiveFieldRecovered) {
    // 两帧共享 3 个控制点；帧1 有 +5 加性偏移。联合求解应恢复真值并校准。
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
    // 帧1 校准后应回到 ~10（减去联合 offset +5）
    ASSERT_EQ(p2_upm_calibrate_block(model, 1, ipix, in, out, 1), 0);
    EXPECT_NEAR(out[0], 10.0, 1e-3);
    p2_upm_close(model);
}

TEST(Phase2Upm, S2LowSnrDoesNotPullHighSnr) {
    // 控制点 0：真值 10；帧0 高 SNR=100 给 10；帧1 低 SNR=0.3 给 50（偏差）。
    // SNR-aware 权重应显著接近高 SNR 真值。
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
    // 帧0 校准（offset≈0.117）后仍在 10 附近；低 SNR 帧贡献被压制
    EXPECT_NEAR(out[0], 10.0, 0.5);
    p2_upm_close(model);
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
    EXPECT_EQ(accepted.back(), 0u);      // 50.0 被拒绝
    EXPECT_GT(out.accepted_count, 3u);
    EXPECT_GE(out.accepted_count + out.rejected_low + out.rejected_high,
              vals.size());
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
    EXPECT_EQ(out.status, 1);  // min-samples
}
