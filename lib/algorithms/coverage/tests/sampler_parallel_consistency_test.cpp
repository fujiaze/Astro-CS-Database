// lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp — CON-004 1T/2T 确定性
// 同一真实/合成输入分别以 cpu_workers=1 与 =2 运行 p2_sample_controls，
// 断言 accept/reject 计数、frame_id、control 顺序、obs 值在容差内一致。
// Linux 无 HiPS fixture => GTEST_SKIP（与既有 sampler 测试一致）；Fatduck 运行验证。
#include "astro/phase2/sampler.h"
#include "aio_hips.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cinttypes>
#include <cstdlib>

namespace {
// 真实 HiPS fixture 根目录（禁止写死机器绝对路径 —— AGENTS §3）：
// ASTROCS_PHASE1_FREEZE_DIR 未设置时用仓库相对默认 run/temp/phase1_freeze。
std::string phase1_fixture_root() {
    const char* e = std::getenv("ASTROCS_PHASE1_FREEZE_DIR");
    return (e && *e) ? std::string(e) : std::string("run/temp/phase1_freeze");
}

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
    const std::string base = phase1_fixture_root();
    const std::string p0 = base + "/t4_crop_v3.hips";
    const std::string p1 = base + "/t4_full_v3_final.hips";
    if (!std::ifstream(base + "/t4_crop_v3.hips/signal/properties").good())
        GTEST_SKIP() << "真实 HiPS fixture 不存在：设 ASTROCS_PHASE1_FREEZE_DIR 指向 phase1_freeze 根";

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

// 测试用临时目录（合成 fixture 根）与清理：两个用例共用同一对 I/O 原语，
// 避免重复文件系统原语命中（AIO 边界台账对本文件的命中数**只减不增**：
// eng/ci/ledgers/aio_io_boundary_inventory.json#entries[5]）。
std::string sampler_tmp_dir(const char* leaf) {
    return (std::filesystem::temp_directory_path() / leaf).string();
}

void remove_test_dir(const std::string& dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// 写一帧合成 HiPS（nside=512，12 个 order-0 tile，常量信号 + 全覆盖 support）。
// support_tiles：前 N 个 order-0 tile 带 support（kArea），其余 tile 的 support 全 0
// —— tile 仍在文件里（coverage 的 tile 集合不变），但该帧在这些 tile 上
// patch 有效样本 = 0 < min_samples ⇒ pass1 产生 insufficient_support 拒绝对
// （FIX-210 D2 门的非退化输入：全覆盖 fixture 的 insuff 恒 0，门会退化成空门）。
// spike_grid：>0 时在 x%64==32 ∧ y%64==32 的像素写亮异常值（每 patch 恰 1 个）。
// 用途 = FIX-405 / DISP-P2SMP-002 门：亮端 clipping 必剔除该像素 ⇒
// n_retained = n_total-1 < 1.0·n_total（background_min_retained_fraction=1.0）
// ⇒ 第二遍置 reason=2；另一帧保持 clean ⇒ nclean=1 ⇒ 第三遍可达。
// spike_period: 亮斑注入步长（像素）。默认 64 = 稀疏亮点（旧调用方不变）；
// FIX-405 的 retained 拒绝用例需要"每个 5×5 patch 内必有亮点" ⇒ 传 8。
bool make_synth_frame(const std::string& path, float flux,
                      std::uint64_t support_tiles = 12,
                      float spike_value = 0.0f,
                      std::uint32_t spike_period = 64) {
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
    if (spike_value > 0.0f && spike_period > 0) {
        for (std::uint32_t y = spike_period / 2; y < kW; y += spike_period)
            for (std::uint32_t x = spike_period / 2; x < kW; x += spike_period)
                sig[(std::size_t)y * kW + x] = spike_value;
    }
    std::vector<float> area((std::size_t)kW * kW, kArea);
    for (std::uint64_t ipix = 0; ipix < 12; ++ipix) {
        const float a = (ipix < support_tiles) ? kArea : 0.0f;
        std::fill(area.begin(), area.end(), a);
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
    const std::string dir = sampler_tmp_dir("astrocs_p2_sampler_par");
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
    remove_test_dir(dir);
}

// =====================================================================
// FIX-210 D2：诊断计数在 1 worker 与 N worker 下必须**逐位一致**
// （ASTROCS_DESIGN §8「并行开关不得改变科学数值；输出不得依赖线程调度」）。
//
// 根因（本任务定位）：pass1_cell() 入口 "cv = 0; ci = 0;" 把调用方传入的
// 计数器清零；串行路径每 cell 用新局部量接收后立即累加（正确），而并行 worker
// 把**同一个** cv/ci 复用为跨 cell 累加器 ⇒ 每个 worker 只剩最后一个 cell 的
// 计数（真实 testdata 实测 insuff 4214 vs 322；证据见
// run/RELEASE-03/fix/FIX-210/RECEIPT.md）。全覆盖 fixture 的 insuff 恒 0，
// 故既有 SyntheticFixtureBitwiseDeterminism 门对该缺陷是空门。
//
// 断言：
//   ① 非退化自证：fixture 必须真的产生 insufficient_support 拒绝（否则门失效）；
//   ② 1 worker 与 N worker 的 P2SampleStats **全部字段**逐位一致；
//   ③ 计数恒等式 candidate == accepted + Σrejected（两侧都要成立）；
//   ④ 观测序列逐位一致（既有 check_bitwise_same）。
// =====================================================================
TEST(Phase2SamplerParallel, SparseSupportStatsBitwiseIdenticalAcrossWorkers) {
    const std::string dir = sampler_tmp_dir("astrocs_p2_sampler_sparse");
    const std::string p0 = dir + "/F1.hips";
    const std::string p1 = dir + "/F2.hips";
    ASSERT_TRUE(make_synth_frame(p0, 100.0f));
    // 后 6 个 order-0 tile 的 support 全 0 ⇒ 该帧在这些 tile 的每个 union cell
    // 上都是 insufficient_support（coverage 的 tile 集合不变，故 cov_frames 仍含它）。
    ASSERT_TRUE(make_synth_frame(p1, 125.0f, /*support_tiles=*/6));

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
    auto sample = [&](int workers, P2SampleStats* st,
                      std::vector<P2ControlObservation>* obs) {
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
        return 0;
    };

    P2SampleStats s1{}, sN{};
    std::vector<P2ControlObservation> o1, oN;
    ASSERT_EQ(sample(1, &s1, &o1), 0) << err;
    ASSERT_EQ(sample(4, &sN, &oN), 0) << err;   // N>1 走并行分支

    // ① 非退化自证：门必须踩到 insufficient_support 计数路径
    ASSERT_GT(s1.rejected_insufficient_support, 0u)
        << "fixture 未触发 insufficient_support ⇒ 该门为空门（判据无效）";

    // ② 全部 stats 字段逐位一致（不止 insuff：veto 走同一累加器）
    EXPECT_EQ(s1.candidate_observations, sN.candidate_observations);
    EXPECT_EQ(s1.accepted_observations, sN.accepted_observations);
    EXPECT_EQ(s1.rejected_insufficient_support, sN.rejected_insufficient_support)
        << "insufficient_support 计数随 worker 数变化（FIX-210 D2 回归）";
    EXPECT_EQ(s1.rejected_insufficient_retained, sN.rejected_insufficient_retained);
    EXPECT_EQ(s1.rejected_bright_tolerance, sN.rejected_bright_tolerance);
    EXPECT_EQ(s1.rejected_high_contamination, sN.rejected_high_contamination);
    EXPECT_EQ(s1.rejected_catalog_veto, sN.rejected_catalog_veto);
    EXPECT_EQ(s1.rejected_lt_two_clean_frames, sN.rejected_lt_two_clean_frames);
    EXPECT_EQ(s1.accepted_controls, sN.accepted_controls);
    EXPECT_EQ(s1.overlap_controls, sN.overlap_controls);

    // ③ 计数恒等式（1 与 N 都必须精确成立）。
    // 注：本 fixture 的 retained 计数为 0 ⇒ 不受已登记缺陷 DISP-P2SMP-002
    // （第三遍对 reason==2 重复 ++rejected_insufficient_retained，
    // docs/algorithms/PHASE2_SAMPLER.md §11.2）影响；该缺陷整改归 P2-SAMP-IMPL，
    // 不在 FIX-210 范围。
    ASSERT_EQ(0u, s1.rejected_insufficient_retained)
        << "fixture 触发了 DISP-P2SMP-002 双计数面 ⇒ 恒等式断言需先处置该登记缺陷";
    auto identity_gap = [](const P2SampleStats& s) -> long long {
        const std::uint64_t rej = s.rejected_insufficient_support +
                                  s.rejected_insufficient_retained +
                                  s.rejected_bright_tolerance +
                                  s.rejected_high_contamination +
                                  s.rejected_catalog_veto +
                                  s.rejected_lt_two_clean_frames;
        return static_cast<long long>(s.candidate_observations) -
               static_cast<long long>(s.accepted_observations + rej);
    };
    EXPECT_EQ(0, identity_gap(s1)) << "1 worker：candidate != accepted + Σrejected";
    EXPECT_EQ(0, identity_gap(sN)) << "N worker：candidate != accepted + Σrejected";

    // ④ 观测序列逐位一致
    ASSERT_EQ(o1.size(), oN.size());
    for (std::size_t i = 0; i < o1.size(); ++i) check_bitwise_same(o1[i], oN[i], i);

    p2_coverage_free(&cov);
    remove_test_dir(dir);
}

// =====================================================================
// FIX-405 / DISP-P2SMP-002：rejected_insufficient_retained **恰好计一次**
//
// 缺陷（登记项 DISP-P2SMP-002，本轮修复）：第二遍在置 reason=2 的同一分支内
// 已 ++rejected_insufficient_retained（sampler.cpp 第二遍），第三遍又对
// reason==2 帧重复 ++（原第三遍 :1071）⇒ 同一帧计两次，统计面
// candidate = accepted + Σrejected 恒等式被破坏（obs 输出不受影响）。
//
// 门（非退化）：F2 每个背景 patch 中心有 1 个亮异常像素 ⇒ clipping 必剔除
// ⇒ n_retained = n_total-1；令 background_min_retained_fraction = 1.0
// ⇒ F2 每 cell 置 reason=2；F1 保持 clean ⇒ nclean=1 ⇒ 第三遍可达（旧码
// 必双计）。修复后：retained 恰 = 每个 union cell 一帧，且恒等式缺口 = 0。
// 负例注入自证：把第三遍的 ++ 加回 ⇒ identity_gap != 0 ⇒ 本门判红。
// =====================================================================
TEST(Phase2SamplerParallel, RetainedRejectionCountedExactlyOnce) {
    const std::string dir = sampler_tmp_dir("astrocs_p2_sampler_retained");
    const std::string p0 = dir + "/F1.hips";
    const std::string p1 = dir + "/F2.hips";
    ASSERT_TRUE(make_synth_frame(p0, 100.0f));
    // 亮斑步长 8 ⇒ 任意 5×5 patch 内必含亮斑 ⇒ 第一遍 clipping 必然剔除样本
    // （n_retained < n_total）⇒ reason=2 分支被确定性触发（非空门）。
    ASSERT_TRUE(make_synth_frame(p1, 100.0f, /*support_tiles=*/12,
                                 /*spike_value=*/1000.0f, /*spike_period=*/8));

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
    auto sample = [&](int workers, P2SampleStats* st) {
        P2SamplerConfig cfg{};
        cfg.control_grid_per_tile = 8;
        cfg.patch_radius_leaf = 2;
        cfg.min_samples = 5;
        cfg.snr_search_radius_deg = 0.05;
        cfg.cpu_workers = workers;
        // 保留比例下限 = 1.0 ⇒ 任何 clipping 剔除都触发 reason=2。
        // 同时把前两级门（亮端 tolerance / 污染占比）放宽到不可达，使被 spike
        // 触发的 clipping **只能**落在 reason=2 分支 —— 否则 1000 vs 100 的
        // 亮点会先被 tolerance 门（reason=3）吃掉，本门退化为空门。
        cfg.background_min_retained_fraction = 1.0;
        cfg.background_tolerance = 1.0e9;
        cfg.background_max_contamination = 1.0;
        std::uint64_t n = 0, c = 0;
        if (p2_sample_controls(&cov, paths, &cfg, nullptr, 0, &n, &c, nullptr,
                               nullptr, 0, err, sizeof(err)) != 0)
            return 1;
        std::vector<P2ControlObservation> obs(n);
        if (p2_sample_controls(&cov, paths, &cfg, obs.data(), n, &n, &c, st,
                               nullptr, 0, err, sizeof(err)) != 0)
            return 1;
        return 0;
    };

    auto identity_gap = [](const P2SampleStats& s) -> long long {
        const std::uint64_t rej = s.rejected_insufficient_support +
                                  s.rejected_insufficient_retained +
                                  s.rejected_bright_tolerance +
                                  s.rejected_high_contamination +
                                  s.rejected_catalog_veto +
                                  s.rejected_lt_two_clean_frames;
        return static_cast<long long>(s.candidate_observations) -
               static_cast<long long>(s.accepted_observations + rej);
    };

    for (int workers : {1, 2}) {
        P2SampleStats st{};
        ASSERT_EQ(sample(workers, &st), 0) << err << " (workers=" << workers << ")";
        // ① 非退化自证：fixture 必须真的踩到 retained 拒绝路径
        ASSERT_GT(st.rejected_insufficient_retained, 0u)
            << "fixture 未触发 retained 拒绝 ⇒ 门为空门（workers=" << workers << "）";
        // ② 精确计数：union tile 内每个 control cell（grid²=64）恰有 1 帧
        //    （F2，带亮斑）被 retained 拒绝 ⇒ 恰好 1×；第三遍再计一次即 2×。
        //    （cov.n_union_cells 是 **union tile** 数；cell = tile × grid²。）
        const std::uint64_t grid = 8;   // cfg.control_grid_per_tile
        EXPECT_EQ(st.rejected_insufficient_retained,
                  static_cast<std::uint64_t>(cov.n_union_cells) * grid * grid)
            << "retained 计数 != union cell 数（DISP-P2SMP-002 双计数回归）"
            << " workers=" << workers;
        // ③ 计数恒等式：双计数必然破坏（缺口 = -reason2 帧数）
        EXPECT_EQ(0, identity_gap(st))
            << "candidate != accepted + Σrejected（DISP-P2SMP-002 双计数）"
            << " workers=" << workers;
    }

    p2_coverage_free(&cov);
    remove_test_dir(dir);
}
