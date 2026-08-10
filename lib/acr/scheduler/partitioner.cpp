// lib/acr/scheduler/partitioner.cpp — coverage bitmap + range/tile 拆分实现
#include "partitioner.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace astro::compute::scheduler {

// ===== CoverageBitmap =====
CoverageBitmap::CoverageBitmap(std::size_t chunk_count)
    : chunk_count_(chunk_count), done_count_(0) {
    // 每个 word 64 bit
    std::size_t words = (chunk_count + 63) / 64;
    words_.assign(words, 0);
}

void CoverageBitmap::mark_done(std::size_t i) noexcept {
    if (i >= chunk_count_) return;
    std::size_t w = i / 64;
    std::size_t b = i % 64;
    std::uint64_t mask = std::uint64_t{1} << b;
    if ((words_[w] & mask) == 0) {  // 之前未标记
        words_[w] |= mask;
        ++done_count_;
    }
}

bool CoverageBitmap::is_done(std::size_t i) const noexcept {
    if (i >= chunk_count_) return false;
    std::size_t w = i / 64;
    std::size_t b = i % 64;
    return (words_[w] & (std::uint64_t{1} << b)) != 0;
}

bool CoverageBitmap::all_done() const noexcept {
    return done_count_ == chunk_count_;
}

std::size_t CoverageBitmap::done_count() const noexcept {
    return done_count_;
}

std::vector<std::size_t> CoverageBitmap::pending_indices() const {
    std::vector<std::size_t> out;
    out.reserve(chunk_count_ - done_count_);
    for (std::size_t i = 0; i < chunk_count_; ++i) {
        if (!is_done(i)) out.push_back(i);
    }
    return out;
}

// ===== Range 拆分 =====
std::vector<RangeChunk> partition_range(std::size_t begin, std::size_t end,
                                        std::size_t chunk_size) {
    std::vector<RangeChunk> out;
    if (begin >= end || chunk_size == 0) return out;
    std::size_t total = end - begin;
    std::size_t n = (total + chunk_size - 1) / chunk_size;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        RangeChunk c;
        c.index = i;
        c.begin = begin + i * chunk_size;
        c.end = std::min(c.begin + chunk_size, end);
        out.push_back(c);
    }
    return out;
}

std::vector<RangeChunk> partition_range_into(std::size_t begin, std::size_t end,
                                             std::size_t chunk_count) {
    std::vector<RangeChunk> out;
    if (begin >= end || chunk_count == 0) return out;
    std::size_t total = end - begin;
    if (chunk_count > total) chunk_count = total;  // 不能超过元素数
    out.reserve(chunk_count);
    // 近似均分：前 (total % chunk_count) 个 chunk 多 1
    std::size_t base = total / chunk_count;
    std::size_t rem = total % chunk_count;
    std::size_t cur = begin;
    for (std::size_t i = 0; i < chunk_count; ++i) {
        RangeChunk c;
        c.index = i;
        c.begin = cur;
        std::size_t sz = base + (i < rem ? 1 : 0);
        c.end = cur + sz;
        out.push_back(c);
        cur = c.end;
    }
    return out;
}

// ===== Tile 拆分 =====
std::vector<TileChunk> partition_tiles(std::size_t width, std::size_t height,
                                       std::size_t tile_w, std::size_t tile_h) {
    std::vector<TileChunk> out;
    if (width == 0 || height == 0 || tile_w == 0 || tile_h == 0) return out;
    std::size_t tiles_x = (width + tile_w - 1) / tile_w;
    std::size_t tiles_y = (height + tile_h - 1) / tile_h;
    out.reserve(tiles_x * tiles_y);
    std::size_t idx = 0;
    for (std::size_t ty = 0; ty < tiles_y; ++ty) {
        for (std::size_t tx = 0; tx < tiles_x; ++tx) {
            TileChunk c;
            c.index = idx++;
            c.tile_x = tx;
            c.tile_y = ty;
            c.tile_w = std::min(tile_w, width - tx * tile_w);
            c.tile_h = std::min(tile_h, height - ty * tile_h);
            out.push_back(c);
        }
    }
    return out;
}

std::vector<TileChunk> partition_tiles_into(std::size_t width, std::size_t height,
                                            std::size_t max_chunks) {
    if (width == 0 || height == 0 || max_chunks == 0) return {};
    // 求 tile_w, tile_h 使 tiles_x * tiles_y <= max_chunks 且 tile 尽量均匀
    // 简化：先按 max_chunks 估算每方向 tile 数（取 sqrt）
    std::size_t per_axis = 1;
    while (per_axis * per_axis < max_chunks) ++per_axis;
    if (per_axis < 1) per_axis = 1;
    std::size_t tile_w = (width + per_axis - 1) / per_axis;
    std::size_t tile_h = (height + per_axis - 1) / per_axis;
    if (tile_w == 0) tile_w = 1;
    if (tile_h == 0) tile_h = 1;
    return partition_tiles(width, height, tile_w, tile_h);
}

} // namespace astro::compute::scheduler
