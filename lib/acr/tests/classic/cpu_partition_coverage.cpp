// lib/acr/tests/classic/cpu_partition_coverage.cpp — CPU Partition Coverage
// 原 e13_mixed.cpp（按 20_PHASE_I_AUDIT_ACTION_PLAN.md §6 重命名）
// 重命名理由：CPU-only build 下 enable_gpu=false，测试内容是 CPU partition coverage
// 而非真实 CPU+GPU Mixed；真实 Mixed 需 GPU 可用，无 GPU 时 SKIPPED。
// 验证能力：coverage bitmap 完整不重复 + 真实 Mixed（无 GPU 则 SKIPPED）
// 重写（规范 §6 真实 Mixed）：
// 1. 删除 CPU 模拟 GPU 比例路径（run_cpu_gpu_ratio 已移除）
// 2. coverage bitmap 验证保留（CPU-only 合法，验证不重复不遗漏）
// 3. 真实 CPU+GPU 混合执行：无 GPU 则 SKIPPED（不再用 CPU 假装 GPU）
// 4. 动态工作池：无 GPU 则 SKIPPED
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

// GPU 可用性检测（编译时：CPU-only 构建无 GPU）
// ACR_BUILD_CUDA 未定义或为 0 时，GPU 不可用
#if !defined(ACR_BUILD_CUDA) || (ACR_BUILD_CUDA == 0)
constexpr bool kGpuAvailable = false;
#else
constexpr bool kGpuAvailable = true;
#endif

// 用 partition_range_into 拆分，验证 coverage 完整不重复（CPU-only 合法）
CaseResult run_partition_split(std::size_t total, std::size_t chunk_count,
                               const std::string& case_id) {
    auto chunks = partition_range_into(0, total, chunk_count);
    CoverageBitmap bm(chunks.size());
    std::vector<int> data(total, 0);

    auto fn = +[](std::size_t idx, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    MixedRunner runner;
    MixedRunnerConfig cfg;
    cfg.enable_gpu = false;
    runner.configure(cfg);

    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        auto r = runner.run_chunks(chunks, fn, &data);
        bm = runner.last_coverage();
    }, 1);

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

// 用 Dispatcher，验证不同 chunk_size 的 coverage（CPU-only 合法）
CaseResult run_dispatcher_split(std::size_t total, std::size_t chunk_size,
                                const std::string& case_id) {
    std::vector<int> data(total, 0);

    DispatcherConfig dcfg;
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
    }, 1);

    bool all_done = mr.all_done;
    bool no_fail = (mr.failed_chunks == 0);
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

// 真实 CPU+GPU 混合执行：无 GPU 则 SKIPPED（不再用 CPU 模拟 GPU）
CaseResult run_real_mixed(std::size_t total, std::size_t chunk_size,
                          const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;

    if (!kGpuAvailable) {
        return make_result("E13", case_id, "integer", total, true, err, tm,
                           "SKIPPED", "no GPU available (CPU-only build)",
                           "cpu", "cpu");
    }

    // GPU 可用时：真实 Mixed 执行
    std::vector<int> data(total, 0);
    MixedRunner runner;
    MixedRunnerConfig cfg;
    cfg.enable_gpu = true;
    cfg.gpu_backends = {"cuda:0"};
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    runner.configure(cfg);

    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    MixedRunResult mr;
    tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = runner.run_range(0, total, chunk_size, fn, &data);
    }, 1);

    bool all_done = mr.all_done;
    int sum = 0;
    for (auto v : data) sum += v;
    bool data_correct = (sum == static_cast<int>(total));
    bool ok = all_done && data_correct;
    if (!ok) err.max_abs = 1.0;
    return make_result("E13", case_id, "integer", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "real mixed execution failed",
                       "mixed", "cpu+gpu");
}

// 动态工作池：CPU 和真实 GPU 并发领取，无 GPU 则 SKIPPED
CaseResult run_workpool_dynamic(std::size_t total, std::size_t chunk_size,
                                const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;

    if (!kGpuAvailable) {
        return make_result("E13", case_id, "integer", total, true, err, tm,
                           "SKIPPED", "no GPU available for workpool (CPU-only build)",
                           "cpu", "cpu");
    }

    // GPU 可用时：创建大量带唯一 ID 的 chunk，CPU+GPU 并发领取
    std::vector<int> data(total, 0);
    DispatcherConfig dcfg;
    dcfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 0, 0, 80.0, true}};
    dcfg.fallback_strategy = FallbackStrategy::ToCpu;
    Dispatcher d;
    d.configure(dcfg);

    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* dd = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*dd)[i] = 1;
    };

    MixedRunResult mr;
    tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = d.dispatch_range(0, total, chunk_size, fn, &data);
    }, 1);

    bool all_done = mr.all_done;
    int sum = 0;
    for (auto v : data) sum += v;
    bool data_correct = (sum == static_cast<int>(total));
    bool ok = all_done && data_correct;
    if (!ok) err.max_abs = 1.0;
    return make_result("E13", case_id, "integer", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "workpool dynamic dispatch failed",
                       "mixed", "cpu+gpu");
}

} // anonymous namespace

// ===== coverage bitmap 验证（CPU-only 合法）=====
TEST(E13Mixed, Partition4Chunks)   { auto r = run_partition_split(1000, 4, "split_4_chunks");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Partition8Chunks)   { auto r = run_partition_split(1000, 8, "split_8_chunks");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Partition16Chunks)  { auto r = run_partition_split(4096, 16, "split_16_chunks");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, PartitionUneven)    { auto r = run_partition_split(1000, 3, "split_uneven_3");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, Dispatch10Chunks)   { auto r = run_dispatcher_split(1000, 100, "dispatch_10_chunks"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, DispatchSmallChunk) { auto r = run_dispatcher_split(1000, 25, "dispatch_small_chunk"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 真实 Mixed（无 GPU → SKIPPED）=====
TEST(E13Mixed, RealMixed_1K)       { auto r = run_real_mixed(1000, 100, "real_mixed_1k");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E13Mixed, RealMixed_64K)      { auto r = run_real_mixed(1<<16, 1024, "real_mixed_64k");     ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== 动态工作池（无 GPU → SKIPPED）=====
TEST(E13Mixed, WorkpoolDynamic)    { auto r = run_workpool_dynamic(1000, 100, "workpool_dynamic_1k"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e13() {
    return {
        run_partition_split(1000, 4, "split_4_chunks"),
        run_partition_split(1000, 8, "split_8_chunks"),
        run_partition_split(4096, 16, "split_16_chunks"),
        run_partition_split(1000, 3, "split_uneven_3"),
        run_dispatcher_split(1000, 100, "dispatch_10_chunks"),
        run_dispatcher_split(1000, 25, "dispatch_small_chunk"),
        // 真实 Mixed（无 GPU → SKIPPED）
        run_real_mixed(1000, 100, "real_mixed_1k"),
        run_real_mixed(1<<16, 1024, "real_mixed_64k"),
        // 动态工作池（无 GPU → SKIPPED）
        run_workpool_dynamic(1000, 100, "workpool_dynamic_1k"),
    };
}
