// lib/acr/scheduler/partitioner.hpp — Range/Tile 拆分 + coverage bitmap
// Phase F：不重叠 chunk 拆分，用 bitmap 保证完整不重复。
//
// 设计（控制包 07_WORK_CONSERVING_DISPATCHER_SPEC.md）：
//   1. coverage bitmap：每个 chunk 一个 bit，1=已完成
//   2. 拆分策略：固定 chunk_size 或固定 chunk_count
//   3. 边界 chunk 自动 clamp（最后一个可能小于 chunk_size）
//   4. Tile 拆分：tiles_x * tiles_y 个 chunk，每个 chunk 一个 tile
//   5. 拆分结果不重叠：[begin, end) 互不相交，并集 = [0, total)
//   6. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== Range chunk =====
struct RangeChunk {
    std::size_t index;     // chunk 序号（0-based）
    std::size_t begin;
    std::size_t end;
};

// ===== Tile chunk =====
struct TileChunk {
    std::size_t index;        // chunk 序号（0-based，按 tile_y*tiles_x + tile_x 编号）
    std::size_t tile_x;
    std::size_t tile_y;
    std::size_t tile_w;
    std::size_t tile_h;
};

// ===== Coverage Bitmap =====
// 位图，每个 bit 对应一个 chunk。1 = 已完成。
class CoverageBitmap {
public:
    CoverageBitmap() = default;
    explicit CoverageBitmap(std::size_t chunk_count);

    std::size_t chunk_count() const noexcept { return chunk_count_; }

    // 标记 chunk i 为已完成
    void mark_done(std::size_t i) noexcept;

    // 查询 chunk i 是否已完成
    bool is_done(std::size_t i) const noexcept;

    // 查询所有 chunk 是否已完成
    bool all_done() const noexcept;

    // 已完成 chunk 数
    std::size_t done_count() const noexcept;

    // 未完成 chunk 的索引列表
    std::vector<std::size_t> pending_indices() const;

private:
    std::vector<std::uint64_t> words_;  // 64-bit words
    std::size_t chunk_count_{0};
    std::size_t done_count_{0};
};

// ===== Range 拆分 =====
// 按 chunk_size 拆分 [begin, end)，返回所有 chunk（不重叠）
std::vector<RangeChunk> partition_range(std::size_t begin, std::size_t end,
                                        std::size_t chunk_size);

// 按 chunk_count 拆分 [begin, end)，每个 chunk 大小近似相等
std::vector<RangeChunk> partition_range_into(std::size_t begin, std::size_t end,
                                             std::size_t chunk_count);

// ===== Tile 拆分 =====
// 按 tile_w × tile_h 拆分 extent，返回所有 tile chunk
std::vector<TileChunk> partition_tiles(std::size_t width, std::size_t height,
                                       std::size_t tile_w, std::size_t tile_h);

// 按 max_chunks 拆分 extent（自动计算 tile 大小）
std::vector<TileChunk> partition_tiles_into(std::size_t width, std::size_t height,
                                            std::size_t max_chunks);

} // namespace astro::compute::scheduler
