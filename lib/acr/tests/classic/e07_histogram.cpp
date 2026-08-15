// lib/acr/tests/classic/e07_histogram.cpp — E07 Histogram 256 bins
// 验证能力（17 §12）：原子竞争、局部副本、合并
//
// Phase H 扩展：
// - 分布：uniform、90%热点、ramp、随机
// - 尺寸：2^16 至 2^26
// - 实现：全局 atomic + block/local histogram + merge
// - 整数 exact
// - 记录冲突程度曲线（热点 bin 的原子竞争）
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// ===== 数据分布 =====
enum class Distribution { Uniform, Hotspot90, Ramp, Random };

void fill_u8_uniform(std::vector<std::uint8_t>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<std::uint8_t>(rng.next() & 0xFF);
}

// 90% 热点：90% 数据集中在 bin 0-25，10% 散布全范围
void fill_u8_hotspot90(std::vector<std::uint8_t>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) {
        if (rng.next_double() < 0.9) {
            x = static_cast<std::uint8_t>(rng.next() % 26);
        } else {
            x = static_cast<std::uint8_t>(rng.next() & 0xFF);
        }
    }
}

// ramp：bin i 的频率约正比于 i（值随索引线性增长）
void fill_u8_ramp(std::vector<std::uint8_t>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < v.size(); ++i) {
        // ramp 分布：用拒绝采样使高频值更可能
        std::uint8_t val;
        for (int tries = 0; tries < 8; ++tries) {
            val = static_cast<std::uint8_t>(rng.next() & 0xFF);
            // 接受概率 ∝ (val+1)/256
            if (rng.next_double() * 256.0 < static_cast<double>(val) + 1.0) break;
        }
        v[i] = val;
    }
}

void fill_u8_random(std::vector<std::uint8_t>& v, std::uint64_t seed) {
    LCG rng(seed ^ 0xABCDEFULL);
    for (auto& x : v) x = static_cast<std::uint8_t>(rng.next() & 0xFF);
}

void fill_distribution(std::vector<std::uint8_t>& v, Distribution dist, std::uint64_t seed) {
    switch (dist) {
        case Distribution::Uniform:   fill_u8_uniform(v, seed); break;
        case Distribution::Hotspot90: fill_u8_hotspot90(v, seed); break;
        case Distribution::Ramp:      fill_u8_ramp(v, seed); break;
        case Distribution::Random:    fill_u8_random(v, seed); break;
    }
}

const char* dist_name(Distribution d) {
    switch (d) {
        case Distribution::Uniform:   return "uniform";
        case Distribution::Hotspot90: return "hotspot90";
        case Distribution::Ramp:      return "ramp";
        case Distribution::Random:    return "random";
    }
    return "unknown";
}

// 串行 reference：256 bins 直方图
void ref_histogram_256(const std::vector<std::uint8_t>& data, std::vector<std::uint64_t>& hist) {
    hist.assign(256, 0);
    for (auto b : data) hist[b]++;
}

bool compare_hist_exact(const std::vector<std::uint64_t>& a,
                        const std::vector<std::uint64_t>& ref, ErrorStats& err) {
    bool ok = true;
    err = ErrorStats{};
    for (int i = 0; i < 256; ++i) {
        if (a[i] != ref[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(a[i]) -
                                                         static_cast<long long>(ref[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return ok;
}

// ===== 实现 1：全局 atomic（验证原子竞争，热点 bin 冲突严重）=====
CaseResult run_hist_global_atomic(std::size_t n, Distribution dist, const std::string& case_id) {
    std::vector<std::uint8_t> data(n);
    fill_distribution(data, dist, FIXED_SEED);
    std::vector<std::uint64_t> ref_hist;
    ref_histogram_256(data, ref_hist);

    std::vector<std::atomic<std::uint64_t>> atomic_hist(256);
    for (auto& a : atomic_hist) a.store(0, std::memory_order_relaxed);

    auto tm = measure_timing([&] {
        for (auto& a : atomic_hist) a.store(0, std::memory_order_relaxed);
        parallel_for(KernelId::Histogram256, Range1D{0, n}, [&](std::size_t i) {
            atomic_hist[data[i]].fetch_add(1, std::memory_order_relaxed);
        });
    });

    std::vector<std::uint64_t> final_hist(256);
    for (int i = 0; i < 256; ++i) final_hist[i] = atomic_hist[i].load(std::memory_order_relaxed);

    ErrorStats err;
    bool ok = compare_hist_exact(final_hist, ref_hist, err);
    return make_result("E07", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "global atomic histogram mismatch",
                       "cpu", "cpu");
}

// ===== 实现 2：block/local histogram + merge（每 chunk 局部副本，最后合并）=====
CaseResult run_hist_local_merge(std::size_t n, std::size_t chunk_size,
                                Distribution dist, const std::string& case_id) {
    std::vector<std::uint8_t> data(n);
    fill_distribution(data, dist, FIXED_SEED);
    std::vector<std::uint64_t> ref_hist;
    ref_histogram_256(data, ref_hist);

    std::vector<std::uint64_t> final_hist(256, 0);
    std::mutex merge_mu;

    auto tm = measure_timing([&] {
        std::fill(final_hist.begin(), final_hist.end(), 0);
        parallel_chunks(KernelId::Histogram256, Range1D{0, n}, chunk_size,
            [&](std::size_t b, std::size_t e) {
                std::vector<std::uint64_t> local(256, 0);
                for (std::size_t i = b; i < e; ++i) local[data[i]]++;
                std::lock_guard<std::mutex> lk(merge_mu);
                for (int i = 0; i < 256; ++i) final_hist[i] += local[i];
            });
    });

    ErrorStats err;
    bool ok = compare_hist_exact(final_hist, ref_hist, err);
    return make_result("E07", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "local merge histogram mismatch",
                       "cpu", "cpu");
}

// ===== 实现 3：local merge 无锁（每线程独立 local，atomic 合并）=====
CaseResult run_hist_local_atomic_merge(std::size_t n, Distribution dist,
                                       const std::string& case_id) {
    std::vector<std::uint8_t> data(n);
    fill_distribution(data, dist, FIXED_SEED);
    std::vector<std::uint64_t> ref_hist;
    ref_histogram_256(data, ref_hist);

    std::vector<std::atomic<std::uint64_t>> atomic_hist(256);
    for (auto& a : atomic_hist) a.store(0, std::memory_order_relaxed);

    auto tm = measure_timing([&] {
        for (auto& a : atomic_hist) a.store(0, std::memory_order_relaxed);
        parallel_chunks(KernelId::Histogram256, Range1D{0, n}, 4096,
            [&](std::size_t b, std::size_t e) {
                // 每 chunk 局部直方图（无竞争），再 atomic 合并（每 bin 一次 CAS，远少于全局）
                std::uint64_t local[256] = {0};
                for (std::size_t i = b; i < e; ++i) local[data[i]]++;
                for (int i = 0; i < 256; ++i) {
                    if (local[i] > 0)
                        atomic_hist[i].fetch_add(local[i], std::memory_order_relaxed);
                }
            });
    });

    std::vector<std::uint64_t> final_hist(256);
    for (int i = 0; i < 256; ++i) final_hist[i] = atomic_hist[i].load(std::memory_order_relaxed);

    ErrorStats err;
    bool ok = compare_hist_exact(final_hist, ref_hist, err);
    return make_result("E07", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "local atomic merge mismatch",
                       "cpu", "cpu");
}

// 辅助：构造 case_id
std::string make_case(const char* impl, Distribution d, std::size_t n) {
    std::string s = impl;
    s += "_";
    s += dist_name(d);
    s += "_";
    s += std::to_string(n);
    return s;
}

} // anonymous namespace

// ===== 向后兼容 TEST =====
TEST(E07Histogram, Chunks1K)    { auto r = run_hist_local_merge(1<<10, 256, Distribution::Random, "chunks_1K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Atomic1K)    { auto r = run_hist_global_atomic(1<<10, Distribution::Random, "atomic_1K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Skewed1K)    { auto r = run_hist_global_atomic(1<<10, Distribution::Hotspot90, "skewed_1K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：17 §12 规范 =====
// uniform 分布，全局 atomic，2^16 / 2^20 / 2^24
TEST(E07Histogram, GlobalAtomic_Uniform_64K)  { auto r = run_hist_global_atomic(1<<16, Distribution::Uniform, "global_atomic_uniform_64K");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Uniform_1M)   { auto r = run_hist_global_atomic(1<<20, Distribution::Uniform, "global_atomic_uniform_1M");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Uniform_16M)  { auto r = run_hist_global_atomic(1<<24, Distribution::Uniform, "global_atomic_uniform_16M");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// hotspot90 分布（冲突严重），全局 atomic
TEST(E07Histogram, GlobalAtomic_Hotspot90_64K){ auto r = run_hist_global_atomic(1<<16, Distribution::Hotspot90, "global_atomic_hotspot90_64K"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Hotspot90_1M) { auto r = run_hist_global_atomic(1<<20, Distribution::Hotspot90, "global_atomic_hotspot90_1M");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Hotspot90_16M){ auto r = run_hist_global_atomic(1<<24, Distribution::Hotspot90, "global_atomic_hotspot90_16M"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ramp 分布
TEST(E07Histogram, GlobalAtomic_Ramp_64K)     { auto r = run_hist_global_atomic(1<<16, Distribution::Ramp, "global_atomic_ramp_64K");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Ramp_1M)      { auto r = run_hist_global_atomic(1<<20, Distribution::Ramp, "global_atomic_ramp_1M");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// random 分布
TEST(E07Histogram, GlobalAtomic_Random_64K)   { auto r = run_hist_global_atomic(1<<16, Distribution::Random, "global_atomic_random_64K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Random_1M)    { auto r = run_hist_global_atomic(1<<20, Distribution::Random, "global_atomic_random_1M");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// local merge 实现（chunk_size=4096）
TEST(E07Histogram, LocalMerge_Uniform_64K)    { auto r = run_hist_local_merge(1<<16, 4096, Distribution::Uniform, "local_merge_uniform_64K");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, LocalMerge_Hotspot90_1M)   { auto r = run_hist_local_merge(1<<20, 4096, Distribution::Hotspot90, "local_merge_hotspot90_1M"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, LocalMerge_Ramp_16M)       { auto r = run_hist_local_merge(1<<24, 4096, Distribution::Ramp, "local_merge_ramp_16M");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, LocalMerge_Random_64M)     { auto r = run_hist_local_merge(1<<26, 4096, Distribution::Random, "local_merge_random_64M");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// local + atomic merge（混合策略）
TEST(E07Histogram, LocalAtomicMerge_Uniform_1M)  { auto r = run_hist_local_atomic_merge(1<<20, Distribution::Uniform, "local_atomic_merge_uniform_1M");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, LocalAtomicMerge_Hotspot90_1M){ auto r = run_hist_local_atomic_merge(1<<20, Distribution::Hotspot90, "local_atomic_merge_hotspot90_1M"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// 大尺寸 2^26
TEST(E07Histogram, GlobalAtomic_Uniform_64M)  { auto r = run_hist_global_atomic(1<<26, Distribution::Uniform, "global_atomic_uniform_64M");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, GlobalAtomic_Hotspot90_64M){ auto r = run_hist_global_atomic(1<<26, Distribution::Hotspot90, "global_atomic_hotspot90_64M"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e07() {
    return {
        // 向后兼容
        run_hist_local_merge(1<<10, 256, Distribution::Random, "chunks_1K"),
        run_hist_global_atomic(1<<10, Distribution::Random, "atomic_1K"),
        run_hist_global_atomic(1<<10, Distribution::Hotspot90, "skewed_1K"),
        // Phase H 扩展：global atomic × 分布 × 尺寸
        run_hist_global_atomic(1<<16, Distribution::Uniform, "global_atomic_uniform_64K"),
        run_hist_global_atomic(1<<20, Distribution::Uniform, "global_atomic_uniform_1M"),
        run_hist_global_atomic(1<<24, Distribution::Uniform, "global_atomic_uniform_16M"),
        run_hist_global_atomic(1<<16, Distribution::Hotspot90, "global_atomic_hotspot90_64K"),
        run_hist_global_atomic(1<<20, Distribution::Hotspot90, "global_atomic_hotspot90_1M"),
        run_hist_global_atomic(1<<24, Distribution::Hotspot90, "global_atomic_hotspot90_16M"),
        run_hist_global_atomic(1<<16, Distribution::Ramp, "global_atomic_ramp_64K"),
        run_hist_global_atomic(1<<20, Distribution::Ramp, "global_atomic_ramp_1M"),
        run_hist_global_atomic(1<<16, Distribution::Random, "global_atomic_random_64K"),
        run_hist_global_atomic(1<<20, Distribution::Random, "global_atomic_random_1M"),
        // local merge
        run_hist_local_merge(1<<16, 4096, Distribution::Uniform, "local_merge_uniform_64K"),
        run_hist_local_merge(1<<20, 4096, Distribution::Hotspot90, "local_merge_hotspot90_1M"),
        run_hist_local_merge(1<<24, 4096, Distribution::Ramp, "local_merge_ramp_16M"),
        run_hist_local_merge(1<<26, 4096, Distribution::Random, "local_merge_random_64M"),
        // local + atomic merge
        run_hist_local_atomic_merge(1<<20, Distribution::Uniform, "local_atomic_merge_uniform_1M"),
        run_hist_local_atomic_merge(1<<20, Distribution::Hotspot90, "local_atomic_merge_hotspot90_1M"),
        // 大尺寸 2^26
        run_hist_global_atomic(1<<26, Distribution::Uniform, "global_atomic_uniform_64M"),
        run_hist_global_atomic(1<<26, Distribution::Hotspot90, "global_atomic_hotspot90_64M"),
    };
}
