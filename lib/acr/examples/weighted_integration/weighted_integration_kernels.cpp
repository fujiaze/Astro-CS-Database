// lib/acr/examples/weighted_integration/weighted_integration_kernels.cpp
//
// ACR 架构冻结（07 号计划 C）：加权积分参考实现与 KernelRegistry launcher。
//   - SerialReference / OpenMPBaseline（OpenMP 只作为独立性能基线）
//   - ACR CPU launcher：只处理 invocation.domain [begin,end)，内部无嵌套 OpenMP
//   - ACR CUDA launcher：经桥接 DLL 真实 GPU kernel；输入 resident 时走
//     resident 提交（frames/weights 已整帧驻留，跳过逐块 H2D）
#include "weighted_integration_common.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "../../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace astro::compute::weighted_integration {

// ===== SerialReference（quick 小 case 最终数值参考）=====
void weighted_integration_serial(const WeightedIntegrationView& v,
                                 float* output) {
    integrate_range(v, 0, v.pixel_count, output);
}

// ===== OpenMPBaseline（独立基线；与 ACR CPU 共享同一逐像素核心）=====
void weighted_integration_openmp(const WeightedIntegrationView& v,
                                 float* output,
                                 int threads) {
#ifdef _OPENMP
    omp_set_dynamic(0);
    if (threads > 0) omp_set_num_threads(threads);
#pragma omp parallel for schedule(static)
    for (long long p = 0;
         p < static_cast<long long>(v.pixel_count); ++p) {
        output[static_cast<std::size_t>(p)] =
            integrate_one_pixel(v, static_cast<std::size_t>(p));
    }
#else
    (void)threads;
    weighted_integration_serial(v, output);
#endif
}

namespace {

// ===== LegacyParallelLauncher（Dispatcher Finalization 06/08 计划）=====
// 完整域一次执行现有 OpenMP 实现（不拆块）；BDR OpenMP 候选与安全 fallback
// 直接调用，业务侧无需手写三路 if/else。
void weighted_integration_legacy_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* out = inv.buffers.find(0);
    const BufferBinding* frames = inv.buffers.find(1);
    const BufferBinding* weights = inv.buffers.find(2);
    if (!out || !frames || !weights) {
        throw std::runtime_error(
            "weighted integration legacy: missing buffers");
    }
    const auto frame_count = read_scalar<std::size_t>(inv.scalars, 0);
    const auto pixel_count =
        read_scalar<std::size_t>(inv.scalars, sizeof(std::size_t));
    if (!frame_count || !pixel_count || *frame_count == 0 ||
        *pixel_count == 0) {
        throw std::runtime_error(
            "weighted integration legacy: missing scalars");
    }
    WeightedIntegrationView v{
        static_cast<const float*>(frames->data),
        static_cast<const float*>(weights->data),
        *frame_count, *pixel_count};
    weighted_integration_openmp(v, static_cast<float*>(out->data), 0);
}

// ===== ACR CPU launcher（无嵌套 OpenMP；独占输出范围）=====
void weighted_integration_cpu_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* out = inv.buffers.find(0);
    const BufferBinding* frames = inv.buffers.find(1);
    const BufferBinding* weights = inv.buffers.find(2);
    if (!out || !frames || !weights) {
        throw std::runtime_error("weighted integration: missing buffers");
    }
    const auto frame_count = read_scalar<std::size_t>(inv.scalars, 0);
    const auto pixel_count =
        read_scalar<std::size_t>(inv.scalars, sizeof(std::size_t));
    if (!frame_count || !pixel_count || *frame_count == 0 ||
        *pixel_count == 0) {
        throw std::runtime_error("weighted integration: missing scalars");
    }
    WeightedIntegrationView v{
        static_cast<const float*>(frames->data),
        static_cast<const float*>(weights->data),
        *frame_count, *pixel_count};
    integrate_range(v, inv.domain.begin, inv.domain.end,
                    static_cast<float*>(out->data));
}

// ===== ACR CUDA launcher（经桥接；resident 输入走 resident 提交）=====
void weighted_integration_cuda_launcher(const KernelInvocation& inv, void*) {
    using namespace astro::compute::cuda::bridge;
    auto& api = astro::compute::cuda::bridge::api();
    if (!api.loaded()) {
        throw std::runtime_error("cuda bridge not loaded");
    }
    void* h = get_tls_handle();
    if (!h) throw std::runtime_error("no cuda handle");
    const BufferBinding* out = inv.buffers.find(0);
    const BufferBinding* frames = inv.buffers.find(1);
    const BufferBinding* weights = inv.buffers.find(2);
    if (!out || !frames || !weights) {
        throw std::runtime_error("cuda weighted integration: missing buffers");
    }
    const auto frame_count = read_scalar<std::size_t>(inv.scalars, 0);
    const auto pixel_count =
        read_scalar<std::size_t>(inv.scalars, sizeof(std::size_t));
    if (!frame_count || !pixel_count || *frame_count == 0 ||
        *pixel_count == 0) {
        throw std::runtime_error("cuda weighted integration: missing scalars");
    }
    std::uint64_t elapsed = 0;
    const char* err = nullptr;
    const int rc = inv.input_resident
        ? api.submit_weighted_integration_resident(
              h, inv.domain.begin, inv.domain.end,
              static_cast<float*>(out->data),
              *frame_count, *pixel_count, &elapsed, &err)
        : api.submit_weighted_integration(
              h, inv.domain.begin, inv.domain.end,
              static_cast<float*>(out->data),
              static_cast<const float*>(frames->data),
              static_cast<const float*>(weights->data),
              *frame_count, *pixel_count, &elapsed, &err);
    if (rc != 0) {
        throw std::runtime_error(
            err ? err : "cuda weighted integration failed");
    }
    set_tls_elapsed(elapsed);
}

} // anonymous namespace

// ===== KernelRegistry 注册（幂等）=====
void register_weighted_integration_kernels() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        KernelRegistration r;
        r.id = kOperationId;
        r.args.buffer_count = 3;
        r.args.scalar_bytes = 2 * sizeof(std::size_t);
        r.cpu = &weighted_integration_cpu_launcher;
        r.cuda = &weighted_integration_cuda_launcher;
        r.legacy_parallel = &weighted_integration_legacy_launcher;
        r.numeric.compute = NumericPolicy::Compute::fp32;
        r.numeric.accumulator = NumericPolicy::Accumulator::fp64;
        global_kernel_registry().register_kernel(r);
    });
}

} // namespace astro::compute::weighted_integration
