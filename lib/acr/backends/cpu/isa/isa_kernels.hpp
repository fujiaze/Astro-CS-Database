// lib/acr/backends/cpu/isa/isa_kernels.hpp — ISA 专用 kernel 声明
// Phase C：SSE/AVX/AVX2/AVX-512 AXPY kernel + 运行时安全门禁。
//
// 设计（ADR-004）：
//   - kernel_<name>_axpy：纯 SIMD 实现，调用方必须先经 has_isa 门禁
//   - kernel_<name>_axpy_safe：门禁版，不支持 ISA 返回 false 不调用
//   - baseline kernel_axpy_scalar：永远可用，无 ISA 依赖
//   - dispatch_axpy：按 caps 自动选最优 kernel
#pragma once

#include <cstddef>
#include "astro/compute/topology.hpp"

namespace astro::compute::cpu {

// baseline 标量 AXPY（永远可用，无 ISA 依赖）
void kernel_axpy_scalar(float* y, const float* x, float a, std::size_t n) noexcept;

// SSE AXPY（4 floats/iter）
void kernel_sse_axpy(float* y, const float* x, float a, std::size_t n) noexcept;
bool kernel_sse_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                          float a, std::size_t n) noexcept;

// AVX AXPY（8 floats/iter，AVX1 浮点）
void kernel_avx_axpy(float* y, const float* x, float a, std::size_t n) noexcept;
bool kernel_avx_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                          float a, std::size_t n) noexcept;

// AVX2+FMA AXPY（8 floats/iter，FMA 融合乘加）
void kernel_avx2_axpy(float* y, const float* x, float a, std::size_t n) noexcept;
bool kernel_avx2_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                           float a, std::size_t n) noexcept;

// AVX-512 AXPY（16 floats/iter，要求 F/CD/BW/DQ/VL 全套）
void kernel_avx512_axpy(float* y, const float* x, float a, std::size_t n) noexcept;
bool kernel_avx512_axpy_safe(const CpuIsaCaps& caps, float* y, const float* x,
                             float a, std::size_t n) noexcept;

// 自动 dispatch：按 caps 选最优 kernel（AVX-512 > AVX2 > AVX > SSE > scalar）
void dispatch_axpy(const CpuIsaCaps& caps, float* y, const float* x,
                   float a, std::size_t n) noexcept;

} // namespace astro::compute::cpu
