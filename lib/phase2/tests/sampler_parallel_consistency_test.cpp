// lib/phase2/tests/sampler_parallel_consistency_test.cpp — CON-004 1T/2T 确定性
// 同一真实/合成输入分别以 cpu_workers=1 与 =2 运行 p2_sample_controls，
// 断言 accept/reject 计数、frame_id、control 顺序、obs 值在容差内一致。
// Linux 无 HiPS fixture => GTEST_SKIP（与既有 sampler 测试一致）；Fatduck 运行验证。
#include "astro/phase2/sampler.h"

#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <cmath>
#include <cinttypes>

namespace {
struct P2ObsHash {
    std::uint64_t n_obs = 0, n_ctrl = 0;
    std::uint64_t acc = 0;
    static std::uint64_t mix(std::uint64_t h, std::uint64_t v) {
        h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
    void add(const P2ControlObservation& o) {
        acc = mix(acc, o.control_id);
        acc = mix(acc, o.frame_id);
        // 只对结构化 ID/计数做 exact；浮点值以逐项差分单独校验
    }
};
} // namespace

TEST(Phase2SamplerParallel, OneTvsTwoTDeterminism) {
    const char* base = "F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze";
    const std::string p0 = std::string(base) + "/t4_crop_v3.hips";
    const std::string p1 = std::string(base) + "/t4_full_v3_final.hips";
    if (!std::ifstream(std::string(base) + "/t4_crop_v3.hips/signal/properties").good())
        GTEST_SKIP() << "真实 HiPS 输入不存在";

    const char* paths[2] = {p0.c_str(), p1.c_str()};
    P2CoverageResult cov{};
    P2HipsInputInfo infos[2]{};
    cov.n_inputs = 2; cov.inputs = infos;
    ASSERT_EQ(p2_coverage_build(paths, 2, &cov), 0);
    std::vector<P2MocCell> cells(cov.n_union_cells);
    cov.union_cells = cells.data();
    ASSERT_EQ(p2_coverage_build(paths, 2, &cov), 0);

    std::uint64_t n1 = 0, n2 = 0, c1 = 0, c2 = 0;
    char err[512] = {0};
    P2SamplerConfig cfg{};
    cfg.control_grid_per_tile = 8; cfg.patch_radius_leaf = 2;
    cfg.min_samples = 5; cfg.snr_search_radius_deg = 0.05;
    cfg.cpu_workers = 1;
    ASSERT_EQ(p2_sample_controls(&cov, paths, &cfg, nullptr, 0, &n1, &c1, nullptr, nullptr, 0, err, sizeof(err)), 0);
    ASSERT_GT(n1, 0u);
    std::vector<P2ControlObservation> o1(n1);
    ASSERT_EQ(p2_sample_controls(&cov, paths, &cfg, o1.data(), n1, &n1, &c1, nullptr, nullptr, 0, err, sizeof(err)), 0);

    cfg.cpu_workers = 2;
    ASSERT_EQ(p2_sample_controls(&cov, paths, &cfg, nullptr, 0, &n2, &c2, nullptr, nullptr, 0, err, sizeof(err)), 0);
    ASSERT_EQ(n2, n1);
    std::vector<P2ControlObservation> o2(n2);
    ASSERT_EQ(p2_sample_controls(&cov, paths, &cfg, o2.data(), n2, &n2, &c2, nullptr, nullptr, 0, err, sizeof(err)), 0);

    ASSERT_EQ(c1, c2) << "control 节点数必须一致";
    ASSERT_EQ(n1, n2) << "观测数必须一致";
    P2ObsHash h1, h2;
    for (std::size_t i = 0; i < o1.size(); ++i) {
        const auto& a = o1[i]; const auto& b = o2[i];
        ASSERT_EQ(a.frame_id, b.frame_id) << "frame_id 顺序必须一致 @i=" << i;
        ASSERT_EQ(a.control_id, b.control_id) << "control_id 顺序必须一致 @i=" << i;
        ASSERT_EQ(a.snr_available, b.snr_available) << "snr_available @i=" << i;
        EXPECT_NEAR(a.value, b.value, 1e-9) << "value @i=" << i;
        EXPECT_NEAR(a.uncertainty, b.uncertainty, 1e-9) << "uncertainty @i=" << i;
        EXPECT_NEAR(a.snr, b.snr, 1e-9) << "snr @i=" << i;
        EXPECT_NEAR(a.control_variance, b.control_variance, 1e-9) << "control_variance @i=" << i;
        EXPECT_NEAR(a.support, b.support, 1e-9) << "support @i=" << i;
        h1.add(a); h2.add(b);
    }
    EXPECT_EQ(h1.acc, h2.acc) << "1T/2T 结构化 ID/顺序哈希必须一致";
    p2_coverage_free(&cov);
}