// lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp — CON-004 1T/2T 确定性
// 同一真实/合成输入分别以 cpu_workers=1 与 =2 运行 p2_sample_controls，
// 断言 accept/reject 计数、frame_id、control 顺序、obs 值在容差内一致。
// Linux 无 HiPS fixture => GTEST_SKIP（与既有 sampler 测试一致）；Fatduck 运行验证。
#include "astro/phase2/sampler.h"
#include "aio_hips.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
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

// =====================================================================
// SCI-FIX-WEIGHT / M8-C-002：合成 HiPS fixture ⇒ Linux 上也有**真实门**。
// 原 OneTvsTwoTDeterminism 依赖 Fatduck 真实 HiPS，在 Linux 恒 GTEST_SKIP，
// 使 sampler 层"并行=串行"实际无门（Skipped ≠ Passed）。
//
// 断言（位精确，非容差）：同输入 cpu_workers=1 与 =2 的 p2_sample_controls
// 输出必须逐字段**按位相等** —— sampler 并行按 union cell 连续切片、
// 每 cell 由单 worker 独占写 cells[idx]、计数用 std::atomic 整数累加，
// 无跨 worker 浮点归约（与 UPM compute_raw 的 per-control 求和不同：
// 后者的跨 worker 数是 1e-12 级容差，见 synthetic_gate OneTvsTwoTDetermine）。
// =====================================================================
namespace {

// 写一帧合成 HiPS（nside=512，12 个 order-0 tile，常量信号 + 全覆盖 support）。
bool make_synth_frame(const std::string& path, float flux) {
    constexpr std::uint32_t kW = 512;
    constexpr float kArea = 1.0e-8f;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
    if (ec) return false;
    AioHipsProductSet* ps = aio_hips_product_begin(
        path.c_str(), kW, kW, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/test", "SCI-FIX-WEIGHT sampler parallel", "R", 60.0,
        "2026-09-17T00:00:00Z", 0);
    if (ps == nullptr) {
        std::fprintf(stderr, "make_synth_frame begin failed: %s\n",
                     aio_hips_last_error());
        return false;
    }
    std::vector<float> sig((std::size_t)kW * kW, flux);
    std::vector<float> area((std::size_t)kW * kW, kArea);
    for (std::uint64_t ipix = 0; ipix < 12; ++ipix) {
        AstroSphereTileView v{};
        v.parent_ipix = ipix;
        v.leaf_order = 9;
        v.width = kW;
        v.data_type = AIO_HIPS_FLOAT32;
        v.flux_sum = sig.data();
        v.covered_area = area.data();
        v.valid_mask = nullptr;
        v.var_num_sum = nullptr;
        // 版本化 C ABI：跨边界结构必须用 *_abi_init 填 struct_size/abi_version
        aio_hips_tile_view_abi_init(&v);
        if (aio_hips_write_signal_support_tile(ps, &v) != 0) {
            std::fprintf(stderr, "make_synth_frame tile failed: %s\n",
                         aio_hips_last_error());
            aio_hips_abort(ps);
            return false;
        }
    }
    if (aio_hips_finalize(ps) != 0) {
        std::fprintf(stderr, "make_synth_frame finalize failed: %s\n",
                     aio_hips_last_error());
        return false;
    }
    return true;
}

void check_bitwise_same(const P2ControlObservation& a,
                        const P2ControlObservation& b, std::size_t i) {
    EXPECT_EQ(a.frame_id, b.frame_id) << "frame_id @i=" << i;
    EXPECT_EQ(a.control_id, b.control_id) << "control_id @i=" << i;
    EXPECT_EQ(a.leaf_ipix, b.leaf_ipix) << "leaf_ipix @i=" << i;
    EXPECT_EQ(a.snr_available, b.snr_available) << "snr_available @i=" << i;
    EXPECT_EQ(a.quality_flags, b.quality_flags) << "quality_flags @i=" << i;
    // 位精确（EXPECT_EQ on double）：不是容差比较。
    EXPECT_EQ(a.value, b.value) << "value 非位精确 @i=" << i;
    EXPECT_EQ(a.uncertainty, b.uncertainty) << "uncertainty 非位精确 @i=" << i;
    EXPECT_EQ(a.control_variance, b.control_variance)
        << "control_variance 非位精确 @i=" << i;
    EXPECT_EQ(a.control_ivar, b.control_ivar) << "control_ivar 非位精确 @i=" << i;
    EXPECT_EQ(a.support, b.support) << "support 非位精确 @i=" << i;
}

}  // namespace

TEST(Phase2SamplerParallel, SyntheticFixtureBitwiseDeterminism) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / "astrocs_p2_sampler_par").string();
    const std::string p0 = dir + "/F1.hips";
    const std::string p1 = dir + "/F2.hips";
    ASSERT_TRUE(make_synth_frame(p0, 100.0f));
    ASSERT_TRUE(make_synth_frame(p1, 125.0f));

    const char* paths[2] = {p0.c_str(), p1.c_str()};
    P2CoverageResult cov{};
    P2HipsInputInfo infos[2]{};
    cov.n_inputs = 2;
    cov.inputs = infos;
    ASSERT_EQ(p2_coverage_build(paths, 2, &cov), 0);
    ASSERT_GT(cov.n_union_cells, 0u);
    std::vector<P2MocCell> cells(cov.n_union_cells);
    cov.union_cells = cells.data();
    ASSERT_EQ(p2_coverage_build(paths, 2, &cov), 0);

    char err[512] = {0};
    auto sample = [&](int workers, std::uint64_t* n_obs, std::uint64_t* n_ctrl,
                      std::vector<P2ControlObservation>* obs, P2SampleStats* st) {
        P2SamplerConfig cfg{};
        cfg.control_grid_per_tile = 8;
        cfg.patch_radius_leaf = 2;
        cfg.min_samples = 5;
        cfg.snr_search_radius_deg = 0.05;
        cfg.cpu_workers = workers;
        std::uint64_t n = 0, c = 0;
        if (p2_sample_controls(&cov, paths, &cfg, nullptr, 0, &n, &c, nullptr,
                               nullptr, 0, err, sizeof(err)) != 0)
            return 1;
        obs->assign(n, P2ControlObservation{});
        if (p2_sample_controls(&cov, paths, &cfg, obs->data(), n, &n, &c, st,
                               nullptr, 0, err, sizeof(err)) != 0)
            return 1;
        *n_obs = n;
        *n_ctrl = c;
        return 0;
    };

    std::uint64_t n1 = 0, c1 = 0, n2 = 0, c2 = 0;
    std::vector<P2ControlObservation> o1, o2;
    P2SampleStats s1{}, s2{};
    ASSERT_EQ(sample(1, &n1, &c1, &o1, &s1), 0) << err;
    ASSERT_EQ(sample(2, &n2, &c2, &o2, &s2), 0) << err;
    // 门前提自检：fixture 必须真的产出观测，否则门是空门。
    ASSERT_GT(n1, 0u) << "合成 fixture 未产出观测（门失效）";
    ASSERT_EQ(n1, n2);
    ASSERT_EQ(c1, c2);
    ASSERT_EQ(o1.size(), o2.size());
    for (std::size_t i = 0; i < o1.size(); ++i) check_bitwise_same(o1[i], o2[i], i);
    EXPECT_EQ(s1.accepted_observations, s2.accepted_observations);
    EXPECT_EQ(s1.accepted_controls, s2.accepted_controls);
    EXPECT_EQ(s1.overlap_controls, s2.overlap_controls);
    EXPECT_EQ(s1.rejected_catalog_veto, s2.rejected_catalog_veto);
    EXPECT_EQ(s1.rejected_insufficient_support, s2.rejected_insufficient_support);
    p2_coverage_free(&cov);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}