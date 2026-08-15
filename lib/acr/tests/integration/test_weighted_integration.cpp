// lib/acr/tests/integration/test_weighted_integration.cpp
//
// ACR 架构冻结（07 F）：加权积分合成 Mixed 样例四类 CTest。
// 1. CorrectnessQuickAllPaths：quick 小 case 全模式正确性 + 非整除尾块
// 2. ForcedMixedBothNonZero：ForcedMixed CPU/GPU 双方均非零
// 3. ResidentReuseFramesUploadOnce：同一帧栈 4 组权重，frames 上传保持 1
// 4. StreamConsistency：1/2 stream 结果一致
// 无 GPU 时 GPU/Mixed 测试准确 SKIPPED；CPU/OpenMP 仍通过。
#include <gtest/gtest.h>

#include "weighted_integration_kernels.hpp"
#include "scheduler/dispatcher.hpp"
#include "scheduler/device_executor.hpp"
#include "focused/operation_profile.hpp"
#include "../core/task_descriptor.hpp"
#include "../cost/cost_estimator.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace astro::compute;
using namespace astro::compute::scheduler;
using astro::compute::weighted_integration::WeightedIntegrationView;
using astro::compute::weighted_integration::generate_synthetic;
using astro::compute::weighted_integration::generate_weights;
using astro::compute::weighted_integration::integrate_one_pixel;
using astro::compute::weighted_integration::register_weighted_integration_kernels;
using astro::compute::weighted_integration::kOperationId;

namespace {

bool gpu_available() {
    astro::compute::cuda::bridge::ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    return api.loaded() && api.device_count() > 0 &&
           api.submit_weighted_integration &&
           api.submit_weighted_integration_resident &&
           api.upload_persistent_slot;
}

cost::CostEstimate make_estimate(std::size_t rec_cpu, std::size_t rec_gpu) {
    cost::CostEstimate e;
    cost::DeviceCost dc;
    dc.device_id = kHwCpuDeviceId;
    dc.backend = "cpu";
    dc.recommended_chunk = rec_cpu;
    dc.min_effective_chunk = 1024;
    dc.feasible = true;
    dc.profile_available = true;
    e.per_device.push_back(dc);
    cost::DeviceCost gdc;
    gdc.device_id = static_cast<DeviceId>(1);
    gdc.backend = "cuda:0";
    gdc.recommended_chunk = rec_gpu;
    gdc.min_effective_chunk = 1024;
    gdc.feasible = true;
    gdc.profile_available = true;
    e.per_device.push_back(gdc);
    e.preferred_device = kHwCpuDeviceId;
    e.profile_available = true;
    return e;
}

TaskDescriptor make_task(std::size_t n) {
    TaskDescriptor task;
    task.range = Range1D{0, n};
    task.item_count = n;
    return task;
}

KernelInvocation make_inv(const WeightedIntegrationView& view,
                          float* output,
                          const std::vector<float>& frames,
                          const std::vector<float>& weights) {
    KernelInvocation inv;
    inv.id = kOperationId;
    inv.domain = WorkDomain{0, view.pixel_count};
    inv.buffers.add(0, output, view.pixel_count, 1, BufferRole::Output);
    inv.buffers.add(1, const_cast<float*>(frames.data()),
                    frames.size(), 1, BufferRole::Input);
    inv.buffers.add(2, const_cast<float*>(weights.data()),
                    weights.size(), 1, BufferRole::Input);
    append_scalar(inv.scalars, view.frame_count);
    append_scalar(inv.scalars, view.pixel_count);
    inv.traits.bytes_read_per_item =
        view.frame_count * sizeof(float) + sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);
    // 与 KernelRegistration 声明一致：FP32 计算 + FP64 累加
    inv.traits.numeric.compute = NumericPolicy::Compute::fp32;
    inv.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;
    inv.partition = PartitionKind::IndependentOutputTiles;
    return inv;
}

scheduler::CostAwareResult run_once(Dispatcher& d,
                                    const WeightedIntegrationView& view,
                                    float* output,
                                    const std::vector<float>& frames,
                                    const std::vector<float>& weights,
                                    std::size_t rec_cpu,
                                    std::size_t rec_gpu) {
    KernelInvocation inv = make_inv(view, output, frames, weights);
    return d.dispatch_invocation(make_task(view.pixel_count),
                                 make_estimate(rec_cpu, rec_gpu), inv);
}

void check_against_ref(const std::vector<float>& ref,
                       const std::vector<float>& got,
                       std::size_t pixels,
                       const char* label) {
    ASSERT_EQ(ref.size(), got.size()) << label;
    double max_abs = 0.0, diff2 = 0.0, ref2 = 0.0;
    bool finite = true;
    for (std::size_t i = 0; i < ref.size(); ++i) {
        finite = finite && std::isfinite(got[i]) && std::isfinite(ref[i]);
        const double d = static_cast<double>(got[i]) - ref[i];
        max_abs = std::max(max_abs, std::abs(d));
        diff2 += d * d;
        ref2 += static_cast<double>(ref[i]) * ref[i];
    }
    ASSERT_TRUE(finite) << label;
    ASSERT_LE(max_abs, 2e-5) << label;
    const double rel_l2 =
        std::sqrt(diff2 / std::max(ref2, 1e-300));
    ASSERT_LE(rel_l2, 2e-6) << label;
    ASSERT_EQ(ref.size(), pixels) << label;
}

std::vector<float> serial_ref(const WeightedIntegrationView& view) {
    std::vector<float> out(view.pixel_count);
    for (std::size_t p = 0; p < view.pixel_count; ++p) {
        out[p] = integrate_one_pixel(view, p);
    }
    return out;
}

} // anonymous namespace

// ============================================================================
// 1. quick 小 case 全模式正确性 + 非整除尾块
// ============================================================================
TEST(WeightedIntegration, CorrectnessQuickAllPaths) {
    astro::compute::runtime_init();
    register_weighted_integration_kernels();

    const std::size_t pixels = 512u * 512u;   // quick 第一档
    const std::size_t frames = 8;
    std::vector<float> fdata(frames * pixels), w(frames);
    generate_synthetic(20260806, frames, pixels, fdata, w);
    WeightedIntegrationView view{fdata.data(), w.data(), frames, pixels};
    const std::vector<float> ref = serial_ref(view);

    // ---- ACR CpuOnly ----
    {
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_cpu_only());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::CpuOnly;
        d.configure(cfg);
        std::vector<float> out(pixels, 0.0f);
        auto r = run_once(d, view, out.data(), fdata, w, 1u << 16, 1u << 18);
        EXPECT_TRUE(r.run_result.all_done)
            << "cpu err=" << r.run_result.error_message;
        check_against_ref(ref, out, pixels, "acr_cpu");
        EXPECT_EQ(r.chunks_on_cpu + r.chunks_on_gpu, r.total_chunks);
        EXPECT_EQ(r.coverage.done, r.coverage.total);
    }

    if (!gpu_available()) {
        astro::compute::runtime_shutdown();
        GTEST_SKIP() << "no CUDA bridge/device; gpu/mixed paths skipped";
    }

    // ---- ACR GpuOnly（resident 路径经 Dispatcher prefetch）----
    {
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::GpuOnly;
        d.configure(cfg);
        std::vector<float> out(pixels, 0.0f);
        auto r = run_once(d, view, out.data(), fdata, w, 1u << 16, 1u << 18);
        EXPECT_TRUE(r.run_result.all_done)
            << "gpu err=" << r.run_result.error_message;
        check_against_ref(ref, out, pixels, "acr_gpu");
        EXPECT_GT(r.chunks_on_gpu, 0u);
        EXPECT_EQ(r.chunks_on_cpu, 0u);
    }

    // ---- ForcedMixed（机制正确性：双方非零、结果正确）----
    {
        // 独立 1M 像素工作域：池内块数充足（16 块 @64K），保证首轮公平门下
        // CPU 与 GPU 都能领到非零工作（小 case 池过小会被单方抢空，只能证明
        // 机制而非混合；1M 是 ForcedMixed 机制验证的稳定规模）。
        const std::size_t mx_pixels = 1u << 20;
        const std::size_t mx_frames = 8;
        std::vector<float> fdata(mx_frames * mx_pixels), w(mx_frames);
        generate_synthetic(20260807, mx_frames, mx_pixels, fdata, w);
        WeightedIntegrationView mx_view{
            fdata.data(), w.data(), mx_frames, mx_pixels};
        const std::vector<float> mx_ref = serial_ref(mx_view);
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::AutoMixed;
        cfg.force_all_supported_executors = true;
        d.configure(cfg);
        std::vector<float> out(mx_pixels, 0.0f);
        auto r = run_once(d, mx_view, out.data(), fdata, w,
                          1u << 16, 1u << 16);
        EXPECT_TRUE(r.run_result.all_done)
            << "mixed err=" << r.run_result.error_message;
        EXPECT_GT(r.chunks_on_cpu, 0u);
        EXPECT_GT(r.chunks_on_gpu, 0u);
        check_against_ref(mx_ref, out, mx_pixels, "forced_mixed");
    }

    // ---- 非整除尾块（pixels 不整除推荐块）----
    {
        const std::size_t odd_pixels = 1000000u - 1;  // 非 2 的幂，尾块非整除
        const std::size_t odd_frames = 5;
        std::vector<float> fd(odd_frames * odd_pixels), wd(odd_frames);
        generate_synthetic(7, odd_frames, odd_pixels, fd, wd);
        WeightedIntegrationView v2{fd.data(), wd.data(), odd_frames,
                                   odd_pixels};
        const std::vector<float> ref2 = serial_ref(v2);
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::AutoMixed;
        cfg.force_all_supported_executors = true;
        d.configure(cfg);
        std::vector<float> out(odd_pixels, 0.0f);
        auto r = run_once(d, v2, out.data(), fd, wd, 1u << 16, 1u << 16);
        EXPECT_TRUE(r.run_result.all_done);
        EXPECT_EQ(r.coverage.done, r.coverage.total);
        check_against_ref(ref2, out, odd_pixels, "odd_tail");
    }

    astro::compute::runtime_shutdown();
}

// ============================================================================
// 2. ForcedMixed：CPU/GPU 双方均非零（1M 像素）
// ============================================================================
TEST(WeightedIntegration, ForcedMixedBothNonZero) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; mixed skipped";
    }
    astro::compute::runtime_init();
    register_weighted_integration_kernels();
    const std::size_t pixels = 1u << 20;
    const std::size_t frames = 8;
    std::vector<float> fdata(frames * pixels), w(frames);
    generate_synthetic(20260806, frames, pixels, fdata, w);
    WeightedIntegrationView view{fdata.data(), w.data(), frames, pixels};

    auto regs = std::make_shared<ExecutorRegistry>(
        ExecutorRegistry::create_auto());
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.devices = {{"cpu", 0, 0, 50.0, true},
                   {"cuda:0", 1, 0, 500.0, true}};
    cfg.executors = regs;
    cfg.route_mode = RouteMode::AutoMixed;
    cfg.force_all_supported_executors = true;
    d.configure(cfg);
    std::vector<float> out(pixels, 0.0f);
    auto r = run_once(d, view, out.data(), fdata, w, 1u << 16, 1u << 18);
    EXPECT_TRUE(r.run_result.all_done);
    EXPECT_GT(r.chunks_on_cpu, 0u);
    EXPECT_GT(r.chunks_on_gpu, 0u);
    EXPECT_EQ(r.coverage.done, r.coverage.total);
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 3. resident-reuse：同一帧栈 4 组权重，frames 上传保持 1
// ============================================================================
TEST(WeightedIntegration, ResidentReuseFramesUploadOnce) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; reuse skipped";
    }
    astro::compute::runtime_init();
    register_weighted_integration_kernels();

    // ---- 桥接层真实上传计数（slot 0 = frames 必须保持 1）----
    {
        using namespace astro::compute::cuda::bridge;
        ensure_bridge_loaded();
        auto& api = astro::compute::cuda::bridge::api();
        ASSERT_TRUE(api.upload_count);
        const char* err = nullptr;
        ASSERT_GT(api.init(&err), 0);
        void* h = api.executor_create(0, 65536, 256, &err);
        ASSERT_NE(h, nullptr);
        const std::size_t pixels = 1u << 18;
        const std::size_t frames = 8;
        std::vector<float> fdata(frames * pixels), w(frames);
        generate_synthetic(20260806, frames, pixels, fdata, w);
        std::vector<float> out(pixels, 0.0f);
        std::uint64_t el = 0;
        ASSERT_EQ(api.upload_persistent_slot(h, 0, 0, frames * pixels,
                                             fdata.data(), &el, &err), 0);
        for (int g = 0; g < 4; ++g) {
            generate_weights(20260806 + g, frames, w);
            ASSERT_EQ(api.upload_persistent_slot(h, 1, 0, frames,
                                                 w.data(), &el, &err), 0);
            ASSERT_EQ(api.submit_weighted_integration_resident(
                          h, 0, pixels, out.data(), frames, pixels,
                          &el, &err), 0);
        }
        EXPECT_EQ(api.upload_count(h, 0), 1) << "frames must upload once";
        EXPECT_EQ(api.upload_count(h, 1), 4) << "weights update per call";
        api.executor_destroy(h);
    }

    // ---- Dispatcher 端到端：同一 ExecutorRegistry 连续 4 次调用 ----
    {
        const std::size_t pixels = 1u << 18;
        const std::size_t frames = 8;
        std::vector<float> fdata(frames * pixels), w0(frames),
            w1(frames), w2(frames), w3(frames);
        generate_synthetic(20260806, frames, pixels, fdata, w0);
        generate_weights(20260806 + 1, frames, w1);
        generate_weights(20260806 + 2, frames, w2);
        generate_weights(20260806 + 3, frames, w3);

        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        DeviceExecutor* cuda = nullptr;
        for (auto* e : regs->available_executors()) {
            if (e->backend_type().rfind("cuda", 0) == 0) cuda = e;
        }
        ASSERT_NE(cuda, nullptr);
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::AutoMixed;
        cfg.force_all_supported_executors = true;
        d.configure(cfg);

        const std::vector<float>* ws[] = {&w0, &w1, &w2, &w3};
        for (std::size_t gi = 0; gi < 4; ++gi) {
            const auto* w = ws[gi];
            WeightedIntegrationView view{
                fdata.data(), w->data(), frames, pixels};
            const std::vector<float> ref = serial_ref(view);
            std::vector<float> out(pixels, 0.0f);
            auto r = run_once(d, view, out.data(), fdata, *w,
                              1u << 16, 1u << 18);
            ASSERT_TRUE(r.run_result.all_done);
            check_against_ref(ref, out, pixels, "reuse weights");
        }
        // frames（slot 0）只上传一次；weights（slot 1）4 组各更新一次
        EXPECT_EQ(cuda->slot_upload_count(0), 1u)
            << "dispatcher reuse must not re-upload frames";
        EXPECT_EQ(cuda->slot_upload_count(1), 4u);
    }

    astro::compute::runtime_shutdown();
}

// ============================================================================
// 4. stream 一致性：1 与 2 stream 结果一致
// ============================================================================
TEST(WeightedIntegration, StreamConsistency) {
    if (!gpu_available()) {
        GTEST_SKIP() << "no CUDA bridge/device; stream skipped";
    }
    astro::compute::runtime_init();
    register_weighted_integration_kernels();
    const std::size_t pixels = 1u << 18;
    const std::size_t frames = 8;
    std::vector<float> fdata(frames * pixels), w(frames);
    generate_synthetic(20260806, frames, pixels, fdata, w);
    WeightedIntegrationView view{fdata.data(), w.data(), frames, pixels};
    const std::vector<float> ref = serial_ref(view);

    for (std::size_t streams : {std::size_t{1}, std::size_t{2}}) {
        auto regs = std::make_shared<ExecutorRegistry>(
            ExecutorRegistry::create_auto());
        for (auto* e : regs->available_executors()) {
            if (e->backend_type().rfind("cuda", 0) == 0) {
                ASSERT_TRUE(e->set_streams(streams));
                EXPECT_EQ(e->max_in_flight(), streams);
            }
        }
        Dispatcher d;
        DispatcherConfig cfg;
        cfg.devices = {{"cpu", 0, 0, 50.0, true},
                       {"cuda:0", 1, 0, 500.0, true}};
        cfg.executors = regs;
        cfg.route_mode = RouteMode::AutoMixed;
        cfg.force_all_supported_executors = true;
        d.configure(cfg);
        std::vector<float> out(pixels, 0.0f);
        auto r = run_once(d, view, out.data(), fdata, w, 1u << 16, 1u << 18);
        EXPECT_TRUE(r.run_result.all_done);
        check_against_ref(ref, out, pixels,
                          streams == 1 ? "stream1" : "stream2");
    }
    astro::compute::runtime_shutdown();
}
