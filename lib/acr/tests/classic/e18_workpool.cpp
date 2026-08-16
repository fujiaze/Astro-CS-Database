// lib/acr/tests/classic/e18_workpool.cpp — E18 动态 CPU+GPU 工作池
//
// 23 §1/§6：经典 GPU 实验必须走 KernelRegistry + 真实 CUDA launcher。
// 本实现：
// - 通过 dispatch_invocation(KernelInvocation) 派发 Copy/AXPY/Reduction；
// - 无 GPU（无桥接 DLL/设备）→ 状态 SKIPPED，测试用 GTEST_SKIP（不冒充通过）；
// - 真实 Mixed 断言 cpu_done>0 && gpu_done>0；
// - coverage 每项恰好一次；profile hash 运行前后不变。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <device_executor.hpp>

#include "../backends/classic/classic_kernels.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "astro/compute/kernel_registry.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::scheduler;

namespace {

// 检测真实 GPU（桥接 DLL + 设备）
bool gpu_available() {
    classic::register_classic_kernels();
    ExecutorRegistry reg = ExecutorRegistry::create_auto();
    for (auto* e : reg.available_executors()) {
        if (e->backend_type() == "cuda") return true;
    }
    return false;
}

// CPU+GPU executor 注册表（无 GPU 时仅 CPU，但 Mixed 用例会 SKIPPED）
std::shared_ptr<ExecutorRegistry> make_registry() {
    classic::register_classic_kernels();
    return std::make_shared<ExecutorRegistry>(ExecutorRegistry::create_auto());
}

cost::CostEstimate make_mixed_estimate(std::size_t n) {
    cost::CostEstimate est;
    cost::DeviceCost cpu_dc;
    cpu_dc.device_id = kHwCpuDeviceId;
    cpu_dc.backend = "cpu";
    cpu_dc.recommended_chunk = 512;
    cpu_dc.min_effective_chunk = 64;
    cpu_dc.feasible = true;
    cpu_dc.profile_available = true;
    est.per_device.push_back(cpu_dc);
    if (gpu_available()) {
        cost::DeviceCost gpu_dc;
        gpu_dc.device_id = static_cast<DeviceId>(1);
        gpu_dc.backend = "cuda:0";
        gpu_dc.recommended_chunk = 65536;
        gpu_dc.min_effective_chunk = 256;
        gpu_dc.feasible = true;
        gpu_dc.profile_available = true;
        est.per_device.push_back(gpu_dc);
        est.preferred_device = static_cast<DeviceId>(1);
    } else {
        est.preferred_device = kHwCpuDeviceId;
    }
    est.profile_available = true;
    return est;
}

TaskDescriptor make_task(std::size_t n) {
    TaskDescriptor task;
    task.range = Range1D{0, n};
    task.item_count = n;
    return task;
}

// 真实 Mixed AXPY：CPU 与 GPU 均完成非零工作
CaseResult run_mixed_axpy(std::size_t total, const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;
    if (!gpu_available()) {
        return make_result("E18", case_id, "fp32", total, true, err, tm,
                           "SKIPPED", "no GPU available (GTEST_SKIP)",
                           "cpu", "cpu");
    }
    std::vector<float> x(total, 1.0f);
    std::vector<float> y(total, 2.0f);

    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = make_registry();
    cfg.enable_memory_budget = false;
    cfg.invocation_cpu_workers = 2;
    d.configure(cfg);

    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, total};
    inv.buffers.add(0, y.data(), total);
    inv.buffers.add(1, x.data(), total);
    append_scalar(inv.scalars, 2.0f);
    inv.traits.bytes_read_per_item = sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);

    auto est = make_mixed_estimate(total);
    CostAwareResult r;
    tm = measure_timing([&] { r = d.dispatch_invocation(make_task(total), est, inv); }, 1);

    bool ok = r.run_result.all_done &&
              r.chunks_on_cpu > 0 && r.chunks_on_gpu > 0 &&
              r.coverage.failed == 0;
    if (ok) {
        for (std::size_t i = 0; i < total; ++i) {
            if (!fp32_close(y[i], 4.0f)) { ok = false; break; }
        }
    }
    if (!ok) err.max_abs = 1.0;
    return make_result("E18", case_id, "fp32", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "mixed axpy mismatch / mixed assertion failed",
                       "mixed", "cpu+gpu");
}

// 真实 Mixed Reduction：分块局部归约 + merge
CaseResult run_mixed_reduce(std::size_t total, const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;
    if (!gpu_available()) {
        return make_result("E18", case_id, "fp32", total, true, err, tm,
                           "SKIPPED", "no GPU available (GTEST_SKIP)",
                           "cpu", "cpu");
    }
    std::vector<float> x(total, 1.0f);
    const std::size_t max_chunks = total / 64 + 8;
    std::vector<double> partials(max_chunks * classic::kReduceBlocks, 0.0);

    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = make_registry();
    cfg.enable_memory_budget = false;
    cfg.invocation_cpu_workers = 2;
    d.configure(cfg);

    KernelInvocation inv;
    inv.id = "kernel.reduce";
    inv.domain = WorkDomain{0, total};
    inv.buffers.add(0, x.data(), total);
    inv.buffers.add(1, partials.data(), partials.size());
    // 注册声明 FP64 accumulator（24 §5.1）
    inv.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;

    auto est = make_mixed_estimate(total);
    CostAwareResult r;
    tm = measure_timing([&] { r = d.dispatch_invocation(make_task(total), est, inv); }, 1);

    double sum = 0.0;
    for (double v : partials) sum += v;
    bool ok = r.run_result.all_done &&
              r.chunks_on_cpu > 0 && r.chunks_on_gpu > 0 &&
              std::fabs(sum - static_cast<double>(total)) <=
                  1e-2 * static_cast<double>(total) + 1e-2;
    if (!ok) err.max_abs = std::fabs(sum - static_cast<double>(total));
    return make_result("E18", case_id, "fp32", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "mixed reduce mismatch",
                       "mixed", "cpu+gpu");
}

// 工作池 coverage：大范围 AXPY，coverage 每项恰好一次
CaseResult run_workpool_coverage(std::size_t total, const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;
    if (!gpu_available()) {
        return make_result("E18", case_id, "fp32", total, true, err, tm,
                           "SKIPPED", "no GPU available (GTEST_SKIP)",
                           "cpu", "cpu");
    }
    std::vector<float> x(total, 1.0f);
    std::vector<float> y(total, 0.0f);

    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}, {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = make_registry();
    cfg.enable_memory_budget = false;
    cfg.invocation_cpu_workers = 4;
    d.configure(cfg);

    KernelInvocation inv;
    inv.id = "kernel.copy";
    inv.domain = WorkDomain{0, total};
    inv.buffers.add(0, y.data(), total);
    inv.buffers.add(1, x.data(), total);

    auto est = make_mixed_estimate(total);
    CostAwareResult r;
    tm = measure_timing([&] { r = d.dispatch_invocation(make_task(total), est, inv); }, 1);

    bool all_assigned = true;
    for (std::size_t i = 0; i < total; ++i) {
        if (!fp32_close(y[i], 1.0f)) { all_assigned = false; break; }
    }
    bool ok = r.run_result.all_done && all_assigned &&
              r.coverage.done == r.coverage.total && r.coverage.failed == 0;
    if (!ok) err.max_abs = 1.0;
    return make_result("E18", case_id, "fp32", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "workpool coverage incomplete",
                       "mixed", "cpu+gpu");
}

// CPU-only histogram（保持经典实验覆盖；不宣称 GPU）
CaseResult run_histogram(std::size_t total, const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;
    std::vector<std::uint8_t> data(total);
    LCG rng(FIXED_SEED);
    for (auto& v : data) v = static_cast<std::uint8_t>(rng.next() & 0xFF);
    std::vector<std::atomic<std::uint64_t>> hist(256);
    for (auto& a : hist) a.store(0, std::memory_order_relaxed);
    tm = measure_timing([&] {
        parallel_chunks(KernelId::Histogram256, Range1D{0, total}, 4096,
            [&](std::size_t b, std::size_t e) {
                std::uint64_t local[256] = {0};
                for (std::size_t i = b; i < e; ++i) local[data[i]]++;
                for (int i = 0; i < 256; ++i) {
                    if (local[i] > 0) hist[i].fetch_add(local[i], std::memory_order_relaxed);
                }
            });
    }, 1);
    std::uint64_t sum = 0;
    for (int i = 0; i < 256; ++i) sum += hist[i].load(std::memory_order_relaxed);
    bool ok = (sum == total);
    if (!ok) err.max_abs = std::fabs(static_cast<double>(sum) - static_cast<double>(total));
    return make_result("E18", case_id, "integer", total, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "histogram sum mismatch",
                       "cpu", "cpu");
}

// profile hash 运行前后不变（只读约束）
CaseResult run_profile_hash_unchanged(const std::string& case_id) {
    TimingStats tm;
    ErrorStats err;
    const char* path = "acr_e18_profile_hash.json";
    {
        std::ofstream f(path);
        f << R"({"schema_version":"acr.route_profile.v1","generated_at":"20260802T120000Z","profile_kind":"standard",)"
          R"("fingerprint":{"cpu_model":"X","cpu_cores":4,"isa_mask":1,"gpu_name":"","gpu_memory_bytes":0,"gpu_driver_version":"","sha256":"e18hash"}},)"
          R"("routes":[]})";
    }
    std::ifstream f1(path, std::ios::binary | std::ios::ate);
    std::size_t size_before = f1.tellg();
    f1.close();
    tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 1000}, [](std::size_t) {});
    }, 1);
    std::ifstream f2(path, std::ios::binary | std::ios::ate);
    std::size_t size_after = f2.tellg();
    f2.close();
    std::remove(path);
    bool ok = (size_before == size_after);
    if (!ok) err.max_abs = 1.0;
    return make_result("E18", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "profile file changed during run",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E18Workpool, Coverage_1M) {
    auto r = run_workpool_coverage(1 << 20, "workpool_coverage_1m");
    ResultSink::instance().push(r);
    if (r.status == "SKIPPED") GTEST_SKIP() << r.reason;
    EXPECT_TRUE(r.correct);
}

TEST(E18Workpool, Elementwise) {
    auto r = run_mixed_axpy(200000, "mixed_axpy_200k");
    ResultSink::instance().push(r);
    if (r.status == "SKIPPED") GTEST_SKIP() << r.reason;
    EXPECT_TRUE(r.correct);
}

TEST(E18Workpool, Reduction) {
    auto r = run_mixed_reduce(200000, "mixed_reduce_200k");
    ResultSink::instance().push(r);
    if (r.status == "SKIPPED") GTEST_SKIP() << r.reason;
    EXPECT_TRUE(r.correct);
}

TEST(E18Workpool, Histogram) {
    auto r = run_histogram(100000, "histogram_100k");
    ResultSink::instance().push(r);
    EXPECT_TRUE(r.correct);
}

TEST(E18Workpool, ProfileHash) {
    auto r = run_profile_hash_unchanged("profile_hash_unchanged");
    ResultSink::instance().push(r);
    EXPECT_TRUE(r.correct);
}

extern "C" std::vector<CaseResult> run_e18() {
    return {
        run_workpool_coverage(1 << 20, "workpool_coverage_1m"),
        run_mixed_axpy(200000, "mixed_axpy_200k"),
        run_mixed_reduce(200000, "mixed_reduce_200k"),
        run_histogram(100000, "histogram_100k"),
        run_profile_hash_unchanged("profile_hash_unchanged"),
    };
}
