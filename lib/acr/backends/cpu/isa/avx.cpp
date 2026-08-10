// lib/acr/backends/cpu/isa/avx.cpp — AVX1 AXPY kernel
// 编译期保护：仅 GCC/Clang x86 编译 SIMD 路径；其他平台 fallback scalar。
// 运行时门禁：kernel_avx_axpy_safe 检查 caps.has(AVX)，不支持则不调用。
#include "isa_kernels.hpp"

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#  include <immintrin.h>
#  define ACR_X86_AVX 1
#endif

namespace astro::compute::cpu {

#ifdef ACR_X86_AVX

__attribute__((target("avx")))
void kernel_avx_axpy(float* y, const float* x, float a, std::size_t n) noexcept {
    __m256 va = _mm256_set1_ps(a);
    std::size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 vx = _mm256_loadu_ps(x + i);
        __m256 vy = _mm256_loadu_ps(y + i);
        vy = _mm256_add_ps(_mm256_mul_ps(va, vx), vy);
        _mm256_storeu_ps(y + i, vy);
    }
    for (; i < n; ++i) y[i] = a * x[i] + y[i];
}

#else

void kernel_avx_axpy(float* y, const float* x, float a, std::size_t n) noexcept {
    kernel_axpy_scalar(y, x, a, n);
}

#endif

bool kernel_avx_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                          float a, std::size_t n) noexcept {
    if (!caps.has_isa(IsaLevel::AVX)) return false;  // 安全门禁
    kernel_avx_axpy(y, x, a, n);
    return true;
}

} // namespace astro::compute::cpu
