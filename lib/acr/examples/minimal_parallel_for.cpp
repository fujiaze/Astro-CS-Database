// lib/acr/examples/minimal_parallel_for.cpp
// ACR 最小示例：parallel_for 初始化 + parallel_reduce 求和 + run_for 串行验证
//
// 演示 ACR 公共 API 的典型用法：
// runtime_init → Buffer 分配 → parallel_for 初始化 → parallel_reduce 求和
// → run_for 串行参考 → 打印结果 → runtime_shutdown
//
// 注意：FP32 默认允许末位差异（见 acr.hpp 头注），并行与串行累加顺序不同，
// sum 可能有微小相对误差，示例仅打印不硬性断言。

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <functional>

#include "astro/compute/acr.hpp"

int main() {
    using namespace astro::compute;
    using clock_type = std::chrono::steady_clock;

    // 1. 初始化 runtime（幂等；首次配置生效）
    runtime_init();

    constexpr std::size_t N = 1u << 20;  // 1M 元素
    Buffer<float> buf(N);

    // 2. parallel_for：每个元素赋值 i*0.5f
    parallel_for(KernelId::Custom, Range1D{0, N},
        [&](std::size_t i) { buf[i] = static_cast<float>(i) * 0.5f; });

    // 3. parallel_reduce：并行求和
    // identity=0.0f，map=[i]返回 buf[i]，reduce=std::plus<float>
    auto t0 = clock_type::now();
    float sum = parallel_reduce<float>(
        KernelId::Custom, Range1D{0, N}, 0.0f,
        [&](std::size_t i) { return buf[i]; },
        std::plus<float>{});
    auto t1 = clock_type::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 4. run_for：串行参考求和，验证并行结果一致
    float ref_sum = 0.0f;
    run_for(KernelId::Custom, Range1D{0, N},
        [&](std::size_t i) { ref_sum += buf[i]; });

    // 5. 打印结果（浮点累加顺序不同，允许末位差异）
    float rel_err = (ref_sum != 0.0f)
        ? std::fabs((sum - ref_sum) / ref_sum)
        : std::fabs(sum - ref_sum);

    std::printf("=== ACR minimal_parallel_for ===\n");
    std::printf("N                       = %zu\n", N);
    std::printf("parallel_reduce sum     = %.6f\n", sum);
    std::printf("serial run_for  sum     = %.6f\n", ref_sum);
    std::printf("relative error          = %.3e\n", rel_err);
    std::printf("parallel_reduce elapsed = %.3f ms\n", elapsed_ms);

    runtime_shutdown();
    return 0;
}
