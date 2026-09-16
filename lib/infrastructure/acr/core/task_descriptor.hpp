// astro/compute/core/task_descriptor.hpp — ACR 任务描述符
// Phase B3：Public API → TaskDescriptor → CostEstimator → Dispatcher → backend 调用链的中间数据。
//
// 设计：
// 1. TaskDescriptor 由 detail::submit_*_with_desc 构造，传递给 CostEstimator 和 Dispatcher
// 2. operation_id 是 std::string（复制 OperationId 的 string_view，保证生命周期）
// 3. 数据驻留位置由 Buffer 自动填充（CPU default；GPU buffer 填 GPU device_id）
// 4. bytes_per_item / bytes_read / bytes_written 用于 CostEstimator 推算传输与计算成本
// 5. precision 来自 NumericPolicy.compute，CostEstimator 据此选画像曲线
// 6. 公共头不暴露第三方类型
#pragma once

#include "astro/compute/acr.hpp"
#include "astro/compute/task_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace astro::compute {

// ===== DeviceId：设备标识 =====
// 0 = CPU；1..N = GPU 0..N-1（与 topology 对齐）
using DeviceId = std::int32_t;

constexpr DeviceId kCpuDeviceId = 0;
constexpr DeviceId kInvalidDeviceId = -1;

// ===== TaskDescriptor =====
// CostEstimator 的输入；Dispatcher 据此分块派发。
struct TaskDescriptor {
    // 标识与特征
    std::string operation_id;        // OperationId 的副本（诊断、缓存键）
    TaskTraits traits;               // 任务特征（类别、访存、强度等）

    // 工作域（互斥：range 用于 1D；extent+tile 用于 2D；item_count 用于 batch）
    Range1D range{};                 // 1D 范围 [begin, end)
    Extent2D extent{};               // 2D 范围
    TileShape tile{};                // 2D tile 形状
    std::size_t item_count{0};       // batch 模式独立项数

    // 数据规模（CostEstimator 推算成本用）
    std::size_t bytes_per_item{0};   // 每工作项字节数
    std::size_t bytes_read{0};       // 总读取字节
    std::size_t bytes_written{0};    // 总写入字节
    Precision precision{Precision::Default};

    // 数据驻留（由 Buffer 自动填充，CostEstimator 据此决定是否需要传输）
    DeviceId input_residency{kCpuDeviceId};
    DeviceId output_residency{kCpuDeviceId};

    // 派生辅助
    std::size_t work_size() const noexcept {
        // 优先级：range > extent > item_count
        if (range.size() > 0) return range.size();
        if (extent.count() > 0) return extent.count();
        return item_count;
    }

    bool is_2d() const noexcept { return extent.count() > 0; }
    bool is_batch() const noexcept { return item_count > 0 && range.size() == 0 && extent.count() == 0; }
};

// ===== TaskKind：调用入口（CostEstimator 据此选公式分支）=====
enum class TaskKind : std::uint8_t {
    parallel_for,     // 1D 元素级
    parallel_tiles,   // 2D tile
    parallel_reduce,  // 1D 归约
    parallel_batch,   // 独立批次
};

// ===== 构造辅助 =====
inline TaskDescriptor make_range_descriptor(OperationId id, Range1D range,
                                             TaskTraits traits, Precision prec) {
    TaskDescriptor d;
    d.operation_id = std::string(id);
    d.traits = traits;
    d.range = range;
    d.precision = prec;
    return d;
}

inline TaskDescriptor make_tiles_descriptor(OperationId id, Extent2D extent, TileShape tile,
                                             TaskTraits traits, Precision prec) {
    TaskDescriptor d;
    d.operation_id = std::string(id);
    d.traits = traits;
    d.extent = extent;
    d.tile = tile;
    d.precision = prec;
    return d;
}

inline TaskDescriptor make_batch_descriptor(OperationId id, std::size_t item_count,
                                             TaskTraits traits, Precision prec) {
    TaskDescriptor d;
    d.operation_id = std::string(id);
    d.traits = traits;
    d.item_count = item_count;
    d.precision = prec;
    return d;
}

inline TaskDescriptor make_reduce_descriptor(OperationId id, Range1D range,
                                              TaskTraits traits, Precision prec) {
    TaskDescriptor d;
    d.operation_id = std::string(id);
    d.traits = traits;
    d.range = range;
    d.precision = prec;
    return d;
}

// ===== Precision 工具 =====
inline const char* precision_str(Precision p) noexcept {
    switch (p) {
        case Precision::Default:  return "default";
        case Precision::FP32:      return "fp32";
        case Precision::FP64:      return "fp64";
        case Precision::Integer:   return "integer";
    }
    return "unknown";
}

inline Precision numeric_policy_to_precision(const NumericPolicy& np) noexcept {
    return np.compute == NumericPolicy::Compute::fp64 ? Precision::FP64 : Precision::FP32;
}

// ===== 诊断/校验辅助（实现在 task_descriptor.cpp）=====
// TaskDescriptor 摘要（JSON 字符串，日志/调试用）
std::string task_descriptor_summary(const TaskDescriptor& d);

// TaskTraits 默认值校验（测试/诊断用）：active_fraction_hint 必须在 [0, 1]
bool task_traits_valid(const TaskTraits& t) noexcept;

} // namespace astro::compute
