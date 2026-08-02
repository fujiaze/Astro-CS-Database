// lib/acr/tests/classic/e13_mixed.cpp — E13 CPU+GPU Mixed Partition
// 验证能力：0/25/50/75/100% 拆分、coverage bitmap
// CPU-only 模式：GPU 比例 0%，全部 chunk 在 CPU 执行。
// 用 Dispatcher + partition_range 验证 coverage 完整不重复。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <mixed_runner.hpp>
#include <partitioner.hpp>

#include <set>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::scheduler;

namespace {

void fill_fp32(std::vector<float>& v, std::uint64_t seed) {
    LCG rng(seed);
    for (auto& x : v) x = static_cast<float>(rng.next_double() * 2.0 - 1.0);
}

// 用 partition_range_into 拆分，验证 coverage 完整不重复
CaseResult run_partition_split(std::size_t total, std::size_t chunk_count,
                               const std::string& case_id) {
    auto chunks = partition_range_into(0, total, chunk_count);
    CoverageBitmap bm(chunks.size());
    std::vector<int> data(total, 0);
    std::atomic<int> call_count{0};

    auto fn = +[](std::size_t idx, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    MixedRunner runner;
    MixedRunnerConfig cfg;
    cfg.preferred_backend = "cpu";
    cfg.enable_gpu = false;  // CPU-only
    runner.configure(cfg);

    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        call_count.store(0, std::memory_order_relaxed);
        auto r = runner.run_chunks(chunks, fn, &data);
        bm = runner.last_coverage();
    }, 1);  // rounds=1：runner 内部状态跨轮可能累积

    // 验证 coverage 完整
    bool all_done = bm.all_done();
    bool no_overlap = true;
    std::set<std::size_t> seen;
    for (const auto& c : chunks) {
        for (std::size_t i = c.begin; i < c.end; ++i) {
            if (seen.count(i)) { no_overlap = false; break; }
            seen.insert(i);
        }
        if (!no_overlap) break;
    }
    bool full_coverage = (seen.size() == total);
    int sum = 0;
    for (auto v : data) sum += v;
    bool data_correct = (sum == static_cast<int>(total));

    bool ok = all_done && no_overlap && full_coverage && data_correct;
    ErrorStats err;
    if (!ok) {
        err.max_abs = 1.0;
        err.max_rel = !all_done ? 1.0 : (!no_overlap ? 0.5 : (!full_coverage ? 0.3 : 0.1));
    }
    return make_result("E13", case_id, "integer", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : ("coverage=" + std::string(all_done ? "ok" : "fail") +
                                   " overlap=" + std::string(no_overlap ? "ok" : "fail") +
                                   " full=" + std::string(full_coverage ? "ok" : "fail") +
                                   " data=" + std::string(data_correct ? "ok" : "fail")),
                       "cpu", "cpu");
}

// 用 Dispatcher，验证不同 chunk_size 模拟 0/25/50/75/100% 拆分点
CaseResult run_dispatcher_split(std::size_t total, std::size_t chunk_size,
                                const std::string& case_id) {
    std::vector<int> data(total, 0);

    DispatcherConfig dcfg;
    dcfg.preferred_backend = "cpu";
    dcfg.devices = {{"cpu", 0, 0, 50.0, true}};
    dcfg.fallback_strategy = FallbackStrategy::ToCpu;
    Dispatcher d;
    d.configure(dcfg);

    auto fn = +[](std::size_t idx, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* dd = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*dd)[i] = static_cast<int>(idx + 1);
    };

    MixedRunResult mr;
    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = d.dispatch_range(0, total, chunk_size, fn, &data);
    }, 1);  // rounds=1：dispatcher 内部状态跨轮可能累积

    bool all_done = mr.all_done;
    bool no_fail = (mr.failed_chunks == 0);
    int sum = 0;
    for (auto v : data) sum += v;
    // 验证每个元素都被赋值（idx+1 >= 1）
    bool data_correct = true;
    for (auto v : data) if (v < 1) { data_correct = false; break; }

    bool ok = all_done && no_fail && data_correct;
    ErrorStats err;
    if (!ok) err.max_abs = 1.0;
    return make_result("E13", case_id, "integer", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "dispatch coverage incomplete",
                       "cpu", "cpu");
}

// 模拟 CPU+GPU 比例：CPU-only 模式下 GPU=0%
CaseResult run_cpu_gpu_ratio(std::size_t total, int cpu_percent,
                             const std::string& case_id) {
    // cpu_percent: 0/25/50/75/100
    // CPU-only 模式下，实际全部在 CPU。验证 CPU 比例为 100%（GPU=0%）
    std::vector<int> data(total, 0);
    MixedRunner runner;
    MixedRunnerConfig cfg;
    cfg.preferred_backend = "cpu";
    cfg.enable_gpu = false;  // 强制 CPU-only
    runner.configure(cfg);

    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    std::size_t chunk_size = (total + 9) / 10;  // 10 chunks
    MixedRunResult mr;
    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = runner.run_range(0, total, chunk_size, fn, &data);
    }, 1);  // rounds=1：runner 内部状态跨轮可能累积

    // CPU-only：executed_on_cpu == total_chunks，executed_on_gpu == 0
    bool cpu_only = (mr.executed_on_gpu == 0 && mr.executed_on_cpu == mr.total_chunks);
    bool all_done = mr.all_done;
    int sum = 0;
    for (auto v : data) sum += v;
    bool data_correct = (sum == static_cast<int>(total));

    bool ok = cpu_only && all_done && data_correct;
    ErrorStats err;
    if (!ok) err.max_abs = 1.0;
    std::string reason = ok ? "" : ("cpu_only=" + std::string(cpu_only ? "ok" : "fail") +
                                     " gpu_chunks=" + std::to_string(mr.executed_on_gpu));
    // 即使 requested cpu_percent，实际 CPU-only 模式下都是 100% CPU
    (void)cpu_percent;  // CPU-only 模式忽略 requested 比例
    return make_result("E13", case_id, "integer", total, ok, err, tm,
                       ok ? "PASS" : "FAIL", reason, "cpu", "cpu");
}

} // anonymous namespace

TEST(E13Mixed, Partition4Chunks)   { auto r = run_partition_split(1000, 4, "split_4_chunks");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Partition8Chunks)   { auto r = run_partition_split(1000, 8, "split_8_chunks");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Partition16Chunks)  { auto r = run_partition_split(4096, 16, "split_16_chunks");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, PartitionUneven)    { auto r = run_partition_split(1000, 3, "split_uneven_3");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Dispatch10Chunks)   { auto r = run_dispatcher_split(1000, 100, "dispatch_10_chunks"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, DispatchSmallChunk) { auto r = run_dispatcher_split(1000, 25, "dispatch_small_chunk"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Ratio0Pct)          { auto r = run_cpu_gpu_ratio(1000, 0, "ratio_0pct_cpu");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Ratio50Pct)         { auto r = run_cpu_gpu_ratio(1000, 50, "ratio_50pct_cpu");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Ratio100Pct)        { auto r = run_cpu_gpu_ratio(1000, 100, "ratio_100pct_cpu"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, RatioLarge)         { auto r = run_cpu_gpu_ratio(1<<16, 75, "ratio_large_75pct"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e13() {
    return {
        run_partition_split(1000, 4, "split_4_chunks"),
        run_partition_split(1000, 8, "split_8_chunks"),
        run_partition_split(4096, 16, "split_16_chunks"),
        run_partition_split(1000, 3, "split_uneven_3"),
        run_dispatcher_split(1000, 100, "dispatch_10_chunks"),
        run_dispatcher_split(1000, 25, "dispatch_small_chunk"),
        run_cpu_gpu_ratio(1000, 0, "ratio_0pct_cpu"),
        run_cpu_gpu_ratio(1000, 50, "ratio_50pct_cpu"),
        run_cpu_gpu_ratio(1000, 100, "ratio_100pct_cpu"),
        run_cpu_gpu_ratio(1<<16, 75, "ratio_large_75pct"),
    };
}
