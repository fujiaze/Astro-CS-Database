// lib/acr/tests/classic/e07_histogram.cpp — E07 Histogram 256 bins
// 验证能力：原子竞争、局部副本
// Cases: hist_256 / hist_64_bins / hist_local_copy / hist_skewed
// 用 parallel_reduce 风格：每 worker 局部直方图 → 合并。
// 验证与串行 reference 完全一致（整数实验 exact）。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

void fill_u8(std::vector<std::uint8_t>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<std::uint8_t>(rng.next() & 0xFF);
}

// 串行 reference：256 bins 直方图
void ref_histogram_256(const std::vector<std::uint8_t>& data, std::vector<std::uint64_t>& hist) {
    hist.assign(256, 0);
    for (auto b : data) hist[b]++;
}

// 并行直方图：用 parallel_chunks 每 chunk 计算局部直方图，最后合并
CaseResult run_hist_256_chunks(std::size_t n, std::size_t chunk_size, const std::string& case_id) {
    std::vector<std::uint8_t> data(n);
    fill_u8(data, FIXED_SEED);
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

    // 整数 exact 比较
    ErrorStats err;
    bool ok = true;
    for (int i = 0; i < 256; ++i) {
        if (final_hist[i] != ref_hist[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(final_hist[i]) -
                                                         static_cast<long long>(ref_hist[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return make_result("E07", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "histogram count mismatch");
}

// 并行直方图：用 std::atomic 直接累加（验证原子竞争）
CaseResult run_hist_256_atomic(std::size_t n, const std::string& case_id) {
    std::vector<std::uint8_t> data(n);
    fill_u8(data, FIXED_SEED);
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
    bool ok = true;
    for (int i = 0; i < 256; ++i) {
        if (final_hist[i] != ref_hist[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(final_hist[i]) -
                                                         static_cast<long long>(ref_hist[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return make_result("E07", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "atomic histogram mismatch");
}

// 偏斜分布（80% 数据集中在 bin 0-31）— 验证热点 bin 不出错
CaseResult run_hist_skewed(std::size_t n, const std::string& case_id) {
    std::vector<std::uint8_t> data(n);
    LCG rng(FIXED_SEED);
    for (auto& x : data) {
        // 80% 概率落在 0-31，20% 概率落在 0-255
        if (rng.next_double() < 0.8) {
            x = static_cast<std::uint8_t>(rng.next() & 0x1F);
        } else {
            x = static_cast<std::uint8_t>(rng.next() & 0xFF);
        }
    }
    std::vector<std::uint64_t> ref_hist(256, 0);
    for (auto b : data) ref_hist[b]++;

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
    bool ok = true;
    for (int i = 0; i < 256; ++i) {
        if (final_hist[i] != ref_hist[i]) {
            ok = false;
            double diff = std::fabs(static_cast<double>(static_cast<long long>(final_hist[i]) -
                                                         static_cast<long long>(ref_hist[i])));
            if (diff > err.max_abs) err.max_abs = diff;
        }
    }
    return make_result("E07", case_id, "integer", n, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "skewed histogram mismatch");
}

} // anonymous namespace

TEST(E07Histogram, Chunks1K)        { auto r = run_hist_256_chunks(1<<10, 256, "chunks_1K");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Chunks64K)       { auto r = run_hist_256_chunks(1<<16, 1024, "chunks_64K");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Chunks1M)        { auto r = run_hist_256_chunks(1<<20, 4096, "chunks_1M");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Atomic1K)        { auto r = run_hist_256_atomic(1<<10, "atomic_1K");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Atomic64K)       { auto r = run_hist_256_atomic(1<<16, "atomic_64K");            ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Atomic1M)        { auto r = run_hist_256_atomic(1<<20, "atomic_1M");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Skewed1K)        { auto r = run_hist_skewed(1<<10, "skewed_1K");                 ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E07Histogram, Skewed64K)       { auto r = run_hist_skewed(1<<16, "skewed_64K");                ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e07() {
    return {
        run_hist_256_chunks(1<<10, 256, "chunks_1K"),
        run_hist_256_chunks(1<<16, 1024, "chunks_64K"),
        run_hist_256_chunks(1<<20, 4096, "chunks_1M"),
        run_hist_256_atomic(1<<10, "atomic_1K"),
        run_hist_256_atomic(1<<16, "atomic_64K"),
        run_hist_256_atomic(1<<20, "atomic_1M"),
        run_hist_skewed(1<<10, "skewed_1K"),
        run_hist_skewed(1<<16, "skewed_64K"),
    };
}
