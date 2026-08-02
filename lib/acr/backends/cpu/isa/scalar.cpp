// lib/acr/backends/cpu/isa/scalar.cpp — baseline 标量 AXPY（永远可用）
// 无 ISA 依赖，作为 fallback。x86-64 上 GCC 默认开 SSE2，但本文件不使用 intrinsics。
#include "isa_kernels.hpp"

namespace astro::compute::cpu {

void kernel_axpy_scalar(float* y, const float* x, float a, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = a * x[i] + y[i];
    }
}

} // namespace astro::compute::cpu
