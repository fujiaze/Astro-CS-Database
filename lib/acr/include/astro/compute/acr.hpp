// astro/compute/acr.hpp — ACR 公共 API 头（Phase A 占位骨架）
// Phase B 将完整实现 parallel_for/tiles/reduce/batch/scan/chunks/run_for + Buffer/Event。
// 当前为占位，保证 CMake configure 通过、不引入第三方类型。

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace astro::compute {

// Kernel 标识（Phase B 完整化）
enum class KernelId : std::uint32_t {
    Custom = 0,
    Copy,
    Triad,
    AXPY,
    Dot,
    Transpose,
    Convolution2D,
    Histogram256,
    Scan,
    Gather,
    Scatter,
    Mandelbrot,
    Gemm,
    Fft,
};

// 1D 范围
struct Range1D {
    std::size_t begin{0};
    std::size_t end{0};
    constexpr std::size_t size() const noexcept { return end - begin; }
};

// 2D 范围
struct Extent2D {
    std::size_t width{0};
    std::size_t height{0};
};

// Tile 形状
struct TileShape {
    std::size_t tile_w{0};
    std::size_t tile_h{0};
};

// 数值精度策略（Phase B 详细实现）
enum class Precision {
    Default,       // FP32，允许 IEEE 754 末位差异
    FP32,
    FP64,          // 声明性，需要确定性归约
    Integer,       // exact
};

// 执行提示
struct ExecutionHints {
    Precision precision{Precision::Default};
    bool deterministic{false};
    bool prefer_resident{false};
    std::uint32_t chunk_hint{0};   // 0 = 自动
};

// 前向声明（Phase B 完整实现）
class Event;
template<class T> class BufferView;
template<class T> class Buffer;

} // namespace astro::compute
