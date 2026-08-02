// lib/acr/backends/cpu/isa/sse.cpp — SSE4.1 AXPY kernel
// 编译期保护：仅 GCC/Clang x86 编译 SIMD 路径；其他平台 fallback scalar。
// 运行时门禁：kernel_sse_axpy_safe 检查 caps.has(SSE41)，不支持则不调用。
#include "isa_kernels.hpp"

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#  include <immintrin.h>
#  define ACR_X86_SSE 1
#endif

namespace astro::compute::cpu {

#ifdef ACR_X86_SSE

// target attribute 启用 SSE4.1 intrinsics（函数级 ISA 启用，TU 默认仍是 baseline）
__attribute__((target("sse4.1")))
void kernel_sse_axpy(float* y, const float* x, float a, std::size_t n) noexcept {
    __m128 va = _mm_set1_ps(a);
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 vx = _mm_loadu_ps(x + i);
        __m128 vy = _mm_loadu_ps(y + i);
        vy = _mm_add_ps(_mm_mul_ps(va, vx), vy);
        _mm_storeu_ps(y + i, vy);
    }
    for (; i < n; ++i) y[i] = a * x[i] + y[i];
}

#else // 非 x86 / 非 GCC：fallback scalar

void kernel_sse_axpy(float* y, const float* x, float a, std::size_t n) noexcept {
    kernel_axpy_scalar(y, x, a, n);
}

#endif

bool kernel_sse_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                          float a, std::size_t n) noexcept {
    if (!caps.has_isa(IsaLevel::SSE41)) return false;  // 安全门禁
    kernel_sse_axpy(y, x, a, n);
    return true;
}

} // namespace astro::compute::cpu
