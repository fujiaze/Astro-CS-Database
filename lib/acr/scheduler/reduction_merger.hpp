// lib/acr/scheduler/reduction_merger.hpp — 局部 reduction 合并
// Phase F：多设备/多 chunk 的局部 reduction 结果合并。
//
// 设计：
//   1. 每个 chunk 产生一个局部 accumulator
//   2. ReductionMerger 按顺序合并所有 accumulator 到全局结果
//   3. 合并函数指针签名：void(void* dst, const void* src)
//   4. 线程安全（合并时加锁）
//   5. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace astro::compute::scheduler {

// 合并函数签名：dst = op(dst, src)
using MergeFn = void(*)(void* dst, const void* src);

// ===== ReductionMerger =====
// 类型擦除的 reduction 合并器。元素大小 elem_size，合并函数 fn。
class ReductionMerger {
public:
    ReductionMerger();
    ~ReductionMerger();

    // 初始化：设置 identity 值、元素大小、合并函数
    void init(const void* identity, std::size_t elem_size, MergeFn fn);

    // 添加一个局部 accumulator（拷贝 elem_size 字节）
    // 多线程调用安全（内部加锁）
    void add_local(const void* local_acc);

    // 合并所有 local 到 result_out，并写入最终结果
    // 多次调用安全（每次重新合并）
    void finalize(void* result_out);

    // 已添加 local 数
    std::size_t local_count() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::scheduler
