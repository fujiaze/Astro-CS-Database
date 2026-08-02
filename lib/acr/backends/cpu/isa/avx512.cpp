// lib/acr/backends/cpu/isa/avx512.cpp — AVX-512 AXPY kernel
// 编译期保护：仅 GCC/Clang x86 编译 SIMD 路径；其他平台 fallback scalar。
// 运行时门禁：kernel_avx512_axpy_safe 检查 caps.has(AVX512F|CD|BW|DQ|VL)，不支持则不调用。
// AVX-512 子集独立校验（ADR-004：禁止合并为单一 "AVX-512" 标志）。
#include "isa_kernels.hpp"

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#  include <immintrin.h>
#  define ACR_X86_AVX512 1
#endif

namespace astro::compute::cpu {

#ifdef ACR_X86_AVX512

// 全套 AVX-512 子集（F/CD/BW/DQ/VL）一起 target，确保 intrinsics 可用
__attribute__((target("avx512f,avx512cd,avx512bw,avx512dq,avx512vl")))
void kernel_avx512_axpy(float* y, const float* x, float a, std::size_t n) noexcept {
    __m512 va = _mm512_set1_ps(a);
    std::size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __m512 vx = _mm512_loadu_ps(x + i);
        __m512 vy = _mm512_loadu_ps(y + i);
        vy = _mm512_fmadd_ps(va, vx, vy);
        _mm512_storeu_ps(y + i, vy);
    }
    for (; i < n; ++i) y[i] = a * x[i] + y[i];
}

#else

void kernel_avx512_axpy(float* y, const float* x, float a, std::size_t n) noexcept {
    kernel_axpy_scalar(y, x, a, n);
}

#endif

bool kernel_avx512_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                             float a, std::size_t n) noexcept {
    // AVX-512 子集独立校验：F/CD/BW/DQ/VL 必须全部支持才放行
    const IsaLevel required = IsaLevel::AVX512F | IsaLevel::AVX512CD |
                              IsaLevel::AVX512BW | IsaLevel::AVX512DQ |
                              IsaLevel::AVX512VL;
    if (!caps.has_isa(required)) return false;  // 安全门禁
    kernel_avx512_axpy(y, x, a, n);
    return true;
}

} // namespace astro::compute::cpu
