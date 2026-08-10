// lib/acr/backends/cpu/isa/dispatch.cpp — AXPY 自动 dispatch
// 按 caps 选最优 kernel：AVX-512 > AVX2 > AVX > SSE > scalar
#include "isa_kernels.hpp"

namespace astro::compute::cpu {

void dispatch_axpy(const CpuIsaCaps& caps, float* y, const float* x,
                   float a, std::size_t n) noexcept {
    // 从高到低尝试，首个匹配即用（门禁在 _safe 内部）
    if (kernel_avx512_axpy_safe(caps, y, x, a, n)) return;
    if (kernel_avx2_axpy_safe(caps, y, x, a, n))   return;
    if (kernel_avx_axpy_safe(caps, y, x, a, n))    return;
    if (kernel_sse_axpy_safe(caps, y, x, a, n))    return;
    kernel_axpy_scalar(y, x, a, n);  // baseline 永远可用
}

} // namespace astro::compute::cpu
