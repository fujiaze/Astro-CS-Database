// lib/acr/tests/classic/e15_failure.cpp — E15 Failure and Fallback
// 验证能力：backend 缺失 / profile corrupt / device lost / 取消 / 异常传播
// 用 routing StaticRouteResolver 验证 profile 三态；用 parallel_for 异常 kernel 验证 KernelFailed。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <route_profile.hpp>
#include <static_router.hpp>

#include <cstdio>
#include <fstream>
#include <string>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;
using namespace astro::compute::routing;

namespace {

// backend 缺失：无 GPU 时 CUDA backend 不可用 → StatusCode::BackendUnavailable
CaseResult run_backend_unavailable(const std::string& case_id) {
    // ACR_BUILD_CUDA=OFF 时，CUDA backend 不可用
    // 通过 routing missing profile → CPU baseline 验证降级路径
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_e15_backend.json");
    auto res = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = res.missing && res.backend == "cpu" && res.reason == "missing-profile";
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "backend unavailable degradation failed",
                       "cpu", "cpu");
}

// profile corrupt：JSON 解析失败 → CPU baseline + 警告
CaseResult run_profile_corrupt(const std::string& case_id) {
    const char* path = "acr_e15_corrupt_profile.json";
    {
        std::ofstream f(path);
        f << "{ this is >>> NOT <<< valid json ]]]";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);
    std::remove(path);

    ErrorStats err;
    bool ok = res.corrupt && res.backend == "cpu" && res.reason == "corrupt";
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "corrupt profile degradation failed",
                       "cpu", "cpu");
}

// profile missing：无 routes.json → CPU baseline + 警告
CaseResult run_profile_missing(const std::string& case_id) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_e15_missing.json");
    auto res = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] { r.resolve(KernelId::AXPY); }, 5);

    ErrorStats err;
    bool ok = res.missing && res.backend == "cpu";
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "missing profile degradation failed",
                       "cpu", "cpu");
}

// 取消正在执行的 kernel（同步模式下 kernel 已完成，cancel 设置标志）
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
    }, 5);

    ErrorStats err;
    bool ok = ev.cancelled() && (cnt.load() == 1000);
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 1000, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "cancel kernel failed",
                       "cpu", "cpu");
}

// kernel 异常传播：throw → mark_failed(KernelFailed)
CaseResult run_kernel_exception(const std::string& case_id) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });

    auto tm = measure_timing([&] {
        parallel_for(KernelId::Custom, Range1D{0, 100},
            [](std::size_t) { throw std::runtime_error("boom"); });
    }, 5);

    ErrorStats err;
    bool ok = (ev.status() == StatusCode::KernelFailed) && ev.ready();
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 100, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "kernel exception not propagated as KernelFailed",
                       "cpu", "cpu");
}

// device lost 模拟：通过 invalidation 触发 profile reload
CaseResult run_device_lost(const std::string& case_id) {
    const char* path = "acr_e15_device_lost.json";
    {
        std::ofstream f(path);
        f << R"({"schema_version":"acr.route_profile.v1","generated_at":"20260802T120000Z","profile_kind":"standard",)"
          R"("fingerprint":{"cpu_model":"X","cpu_cores":4,"isa_mask":1,"gpu_name":"","gpu_memory_bytes":0,"gpu_driver_version":"","sha256":"devlost"}},)"
          R"("routes":[]})";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res1 = r.resolve(KernelId::AXPY);
    // 模拟 device lost：删除 profile + invalidate
    std::remove(path);
    r.invalidate_cache();
    auto res2 = r.resolve(KernelId::AXPY);

    auto tm = measure_timing([&] {
        r.invalidate_cache();
        r.resolve(KernelId::AXPY);
    }, 5);

    ErrorStats err;
    // res1 应该非 missing（profile 存在），res2 应该 missing（profile 已删除）
    bool ok = !res1.missing && res2.missing;
    if (!ok) err.max_abs = 1.0;
    return make_result("E15", case_id, "integer", 1, ok, err, tm,
                       ok ? "PASS" : "FAIL",
                       ok ? "" : "device lost recovery failed",
                       "cpu", "cpu");
}

} // anonymous namespace

TEST(E15Failure, BackendUnavailable) { auto r = run_backend_unavailable("backend_unavailable"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, ProfileCorrupt)     { auto r = run_profile_corrupt("profile_corrupt");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, ProfileMissing)     { auto r = run_profile_missing("profile_missing");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, CancelKernel)       { auto r = run_cancel_kernel("cancel_kernel");             ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, KernelException)    { auto r = run_kernel_exception("kernel_exception");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E15Failure, DeviceLost)         { auto r = run_device_lost("device_lost");                 ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e15() {
    return {
        run_backend_unavailable("backend_unavailable"),
        run_profile_corrupt("profile_corrupt"),
        run_profile_missing("profile_missing"),
        run_cancel_kernel("cancel_kernel"),
        run_kernel_exception("kernel_exception"),
        run_device_lost("device_lost"),
    };
}
