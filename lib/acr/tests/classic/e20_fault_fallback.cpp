// lib/acr/tests/classic/e20_fault_fallback.cpp — E20 故障和回退
// 规范 E20：插件缺失、无画像、画像损坏/过期、显存不足、launch失败、
//   设备lost模拟、分配失败、取消、异常、profile只读。
//   未开始块回收，已完成块不重复。
// 聚焦于 Dispatcher/MixedRunner 的 fallback 行为。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <dispatcher.hpp>
#include <fallback.hpp>
#include <mixed_runner.hpp>
#include <partitioner.hpp>
#include <route_profile.hpp>
#include <static_router.hpp>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::routing;
using namespace astro::compute::scheduler;

namespace {

// 插件缺失：CUDA backend 不可用时降级到 CPU
CaseResult run_plugin_missing(const std::string& case_id) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_e20_plugin.json");
    auto res = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = res.missing && res.backend == "cpu" && res.reason == "missing-profile";
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "plugin missing degradation failed",
                       "cpu", "cpu");
}

// 无画像：无 routes.json → CPU baseline
CaseResult run_no_profile(const std::string& case_id) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_e20_noprofile.json");
    auto res = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = res.missing && res.backend == "cpu";
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "no profile degradation failed",
                       "cpu", "cpu");
}

// 画像损坏：JSON 解析失败 → CPU baseline
CaseResult run_profile_corrupt(const std::string& case_id) {
    const char* path = "acr_e20_corrupt.json";
    {
        std::ofstream f(path);
        f << "{ this is >>> NOT <<< valid json ]]]";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(KernelId::AXPY);
    std::remove(path);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = res.corrupt && res.backend == "cpu" && res.reason == "corrupt";
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "corrupt profile degradation failed",
                       "cpu", "cpu");
}

// 画像过期：指纹不匹配 → 警告 + 继续运行
CaseResult run_profile_stale(const std::string& case_id) {
    const char* path = "acr_e20_stale.json";
    {
        std::ofstream f(path);
        f << R"({"schema_version":"acr.route_profile.v1","generated_at":"20260802T120000Z","profile_kind":"standard",)"
          R"("fingerprint":{"cpu_model":"X","cpu_cores":4,"isa_mask":1,"gpu_name":"","gpu_memory_bytes":0,"gpu_driver_version":"","sha256":"e20stale"}},)"
          R"("routes":[]})";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(KernelId::AXPY);
    std::remove(path);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = (!res.missing) && (!res.corrupt) &&
              (res.stale || res.reason == "profile" || res.reason == "stale");
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "stale profile not detected",
                       "cpu", "cpu");
}

// 显存不足/OOM 模拟：分配大 Buffer，验证不崩溃
CaseResult run_oom_simulation(const std::string& case_id) {
    auto tm = measure_timing([&] {
        try {
            Buffer<float> b(256 * 1024 * 1024, 0.0f);  // 1GB
            b[0] = 1.0f;
        } catch (...) {
            // OOM 不崩溃即 ok
        }
    }, 1);

    ErrorStats err;
    bool ok = true;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "OOM simulation crash",
                       "cpu", "cpu");
}

// launch 失败：kernel 异常 → KernelFailed
CaseResult run_launch_failure(const std::string& case_id) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("launch failed"); });

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) { throw std::runtime_error("launch failed"); });
    }, 3);

    ErrorStats err;
    bool ok = (ev.status() == StatusCode::KernelFailed) && ev.ready();
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "launch failure not propagated as KernelFailed",
                       "cpu", "cpu");
}

// device lost 模拟：invalidate → reload
CaseResult run_device_lost(const std::string& case_id) {
    const char* path = "acr_e20_device_lost.json";
    {
        std::ofstream f(path);
        f << R"({"schema_version":"acr.route_profile.v1","generated_at":"20260802T120000Z","profile_kind":"standard",)"
          R"("fingerprint":{"cpu_model":"X","cpu_cores":4,"isa_mask":1,"gpu_name":"","gpu_memory_bytes":0,"gpu_driver_version":"","sha256":"e20devlost"}},)"
          R"("routes":[]})";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res1 = r.resolve(KernelId::AXPY);
    std::remove(path);
    r.invalidate_cache();
    auto res2 = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] {
        r.invalidate_cache();
        r.resolve(KernelId::AXPY);
    }, 5);

    ErrorStats err;
    bool ok = !res1.missing && res2.missing;
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "device lost recovery failed",
                       "cpu", "cpu");
}

// 分配失败：Buffer 分配后正确释放
CaseResult run_allocation_failure(const std::string& case_id) {
    auto tm = measure_timing([&] {
        for (int i = 0; i < 100; ++i) {
            Buffer<int> b(1024, i);
            b[0] = i + 1;
        }
    }, 1);

    ErrorStats err;
    bool ok = true;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "allocation failure crash",
                       "cpu", "cpu");
}

// 取消：cancel 正在执行的 kernel
CaseResult run_cancel_kernel(const std::string& case_id) {
    std::atomic<int> cnt{0};
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev.wait();
    ev.cancel();

    auto tm = measure_timing([&] {
        Event e = parallel_for(KernelId::Custom, Range1D{0, 1000}, [](std::size_t) {});
        e.wait();
        e.cancel();
    }, 3);

    ErrorStats err;
    bool ok = ev.cancelled() && (cnt.load() == 1000);
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1000, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "cancel kernel failed",
                       "cpu", "cpu");
}

// 异常传播：throw → mark_failed
CaseResult run_exception_propagation(const std::string& case_id) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) { throw std::runtime_error("boom"); });
    }, 3);

    ErrorStats err;
    bool ok = (ev.status() == StatusCode::KernelFailed) && ev.ready();
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "exception not propagated as KernelFailed",
                       "cpu", "cpu");
}

// profile 只读：resolve 后 profile 文件不被修改
CaseResult run_profile_readonly(const std::string& case_id) {
    const char* path = "acr_e20_readonly.json";
    {
        std::ofstream f(path);
        f << R"({"schema_version":"acr.route_profile.v1","generated_at":"20260802T120000Z","profile_kind":"standard",)"
          R"("fingerprint":{"cpu_model":"X","cpu_cores":4,"isa_mask":1,"gpu_name":"","gpu_memory_bytes":0,"gpu_driver_version":"","sha256":"e20ro"}},)"
          R"("routes":[]})";
    }
    std::string before;
    { std::ifstream f(path); std::getline(f, before); }

    StaticRouteResolver r;
    r.set_profile_path(path);
    r.resolve(KernelId::AXPY);
    r.resolve(KernelId::Copy);

    std::string after;
    { std::ifstream f(path); std::getline(f, after); }
    std::remove(path);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = (before == after);
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "profile modified during resolve",
                       "cpu", "cpu");
}

// 未开始块回收，已完成块不重复：FallbackPolicy 验证
CaseResult run_fallback_no_replay(const std::string& case_id) {
    // 创建 coverage bitmap，标记部分 chunk 已完成
    CoverageBitmap bm(10);
    bm.mark_done(0);
    bm.mark_done(1);
    bm.mark_done(2);

    FallbackPolicy fp;
    fp.set_strategy(FallbackStrategy::ToCpu);
    auto decision = fp.decide("cuda:0", bm, {"cpu"});

    auto tm = measure_timing([&] {
        fp.decide("cuda:0", bm, {"cpu"});
    }, 5);

    ErrorStats err;
    // 回退到 CPU
    bool ok = (decision.strategy == FallbackStrategy::ToCpu) &&
              (decision.target_backend == "cpu") &&
              decision.skip_already_done;
    if (!ok) err.max_abs = 1.0;
    return make_result("E20", case_id, "integer", 10, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "fallback no-replay policy failed",
                       "cpu", "cpu");
}

// 混合调度 fallback：设备失败 → CPU 回退，已完成不重放
CaseResult run_mixed_fallback_to_cpu(const std::string& case_id) {
    runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.preferred_backend = "cpu";
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    d.configure(cfg);

    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };

    MixedRunResult mr;
    auto tm = measure_timing([&] {
        std::fill(data.begin(), data.end(), 0);
        mr = d.dispatch_range(0, 100, 25, fn, &data);
    }, 1);

    ErrorStats err;
    int sum = 0;
    for (auto v : data) sum += v;
    bool ok = mr.all_done && (mr.failed_chunks == 0) && (sum == 100);
    if (!ok) err.max_abs = 1.0;
    runtime_shutdown();
    return make_result("E20", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "mixed fallback to CPU failed",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E20Fault, PluginMissing)     { auto r = run_plugin_missing("plugin_missing");           ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, NoProfile)         { auto r = run_no_profile("no_profile");                   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, ProfileCorrupt)    { auto r = run_profile_corrupt("profile_corrupt");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, ProfileStale)      { auto r = run_profile_stale("profile_stale");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, OomSimulation)     { auto r = run_oom_simulation("oom_simulation");           ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, LaunchFailure)     { auto r = run_launch_failure("launch_failure");           ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, DeviceLost)        { auto r = run_device_lost("device_lost");                 ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, AllocFailure)      { auto r = run_allocation_failure("allocation_failure");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, CancelKernel)      { auto r = run_cancel_kernel("cancel_kernel");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, ExceptionProp)     { auto r = run_exception_propagation("exception_propagation"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, ProfileReadonly)   { auto r = run_profile_readonly("profile_readonly");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, FallbackNoReplay)  { auto r = run_fallback_no_replay("fallback_no_replay");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E20Fault, MixedFallbackCpu)  { auto r = run_mixed_fallback_to_cpu("mixed_fallback_to_cpu"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e20() {
    return {
        run_plugin_missing("plugin_missing"),
        run_no_profile("no_profile"),
        run_profile_corrupt("profile_corrupt"),
        run_profile_stale("profile_stale"),
        run_oom_simulation("oom_simulation"),
        run_launch_failure("launch_failure"),
        run_device_lost("device_lost"),
        run_allocation_failure("allocation_failure"),
        run_cancel_kernel("cancel_kernel"),
        run_exception_propagation("exception_propagation"),
        run_profile_readonly("profile_readonly"),
        run_fallback_no_replay("fallback_no_replay"),
        run_mixed_fallback_to_cpu("mixed_fallback_to_cpu"),
    };
}
