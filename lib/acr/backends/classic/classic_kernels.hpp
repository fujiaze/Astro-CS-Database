// lib/acr/backends/classic/classic_kernels.hpp — 经典实验内核注册
//
// 23 §1.4：经典实验至少提供 Copy、AXPY、Reduction、Convolution 的
// CPU 和 CUDA 注册实现。
//
// Buffer 约定：
// copy: buffers[0]=y, buffers[1]=x
// axpy: buffers[0]=y, buffers[1]=x, scalars[0]=float alpha
// reduce: buffers[0]=x, buffers[1]=partials,
// partials 为 double（FP64 累加），大小 = total_chunks * kReduceBlocks，
// 每 chunk 写 partials[token_id*kReduceBlocks .. +kReduceBlocks)
// conv3x3: buffers[0]=y, buffers[1]=x（w*h 行主序），
// scalars: size_t width, size_t height, float kernel9[9]
#pragma once

#include <cstddef>

namespace astro::compute::classic {

// 归约每 chunk 的 partial 槽位跨度（span）。
// CUDA kernel grid 块数 = ceil(chunk_items / 256)，实际写入
// partials[chunk_index*span + blockIdx]（blockIdx < span）；
// merge 阶段遍历整个 span（未写槽为 0）。
constexpr std::size_t kReduceBlocks = 1024;

// 注册 Copy/AXPY/Reduction/Convolution 的 CPU+CUDA launcher（幂等，call_once）
void register_classic_kernels();

} // namespace astro::compute::classic
