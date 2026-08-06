// astro/compute/kernel_registry.hpp — 可加速 Kernel 注册模型（23 号计划 §1）
//
// 背景（audits/SECOND_FIX_REVIEW_AUDIT.md §一.3）：
//   普通 C++ host 函数指针、捕获 lambda 和 void* user_data 只能作为 CPU 兼容
//   执行入口；CUDA/HIP/SYCL device 不能直接调用它们。
//
// 本头文件建立两层 API：
//   1. CPU-only compatibility API（acr.hpp 中的 parallel_for(OperationId, ...)
//      等 lambda 接口）——保留并明确标记 CPU-only；
//   2. accelerator-capable API：OperationId + KernelRegistration +
//      KernelInvocation，通过 KernelRegistry 选择设备实现。
//
// 约束：
//   - 公共头不暴露 CUDA/HIP/SYCL/oneTBB 类型（launcher 只是函数指针类型）；
//   - Dispatcher 只能把 invocation 交给支持该 OperationId 的 executor；
//   - 设备 launcher 缺失时必须回退 CPU 并如实报告。
#pragma once

#include "astro/compute/task_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astro::compute {

// ===== 工作域 =====
struct WorkDomain {
    std::size_t begin{0};
    std::size_t end{0};

    constexpr std::size_t size() const noexcept {
        return end >= begin ? end - begin : 0;
    }
    constexpr bool empty() const noexcept { return end <= begin; }
};

// ===== Buffer 绑定 =====
// 对 CPU executor：data 是 host 指针；
// 对 CUDA executor：data 是设备指针（由 executor 驻留解析）。
struct BufferBinding {
    std::size_t index{0};      // 绑定槽位（0..buffer_count-1）
    void* data{nullptr};       // host/device 指针（executor 解析）
    std::size_t count{0};      // 元素数
    std::size_t stride{1};     // 元素步长（默认连续）
};

struct BufferBindingList {
    std::vector<BufferBinding> bindings;

    void add(std::size_t index, void* data, std::size_t count,
             std::size_t stride = 1) {
        bindings.push_back(BufferBinding{index, data, count, stride});
    }
    const BufferBinding* find(std::size_t index) const {
        for (const auto& b : bindings) {
            if (b.index == index) return &b;
        }
        return nullptr;
    }
    std::size_t size() const noexcept { return bindings.size(); }
};

// ===== 标量参数（type-erased，按字节存储）=====
struct ScalarArgBlob {
    std::vector<unsigned char> bytes;
};

// 追加一个标量到 blob（按字节拷贝）
template<class T>
void append_scalar(ScalarArgBlob& blob, const T& value) {
    const auto* p = reinterpret_cast<const unsigned char*>(&value);
    blob.bytes.insert(blob.bytes.end(), p, p + sizeof(T));
}

// 安全读取 blob 中偏移 offset 处的标量。
// 24 号计划 §5.2：使用 memcpy 拷贝到对齐局部变量，禁止 reinterpret_cast 可能
// 产生的未对齐访问（C++ UB）。越界返回 nullopt。
template<class T>
std::optional<T> read_scalar(const ScalarArgBlob& blob, std::size_t offset) noexcept {
    if (offset + sizeof(T) > blob.bytes.size()) return std::nullopt;
    T v{};
    std::memcpy(&v, blob.bytes.data() + offset, sizeof(T));
    return v;
}

// ===== KernelInvocation：一次可加速执行的最小描述 =====
struct KernelInvocation {
    OperationId id{};              // 注册表中的 OperationId
    WorkDomain domain{};           // 工作域 [begin, end)
    BufferBindingList buffers;     // buffer 绑定
    ScalarArgBlob scalars;         // 标量参数
    TaskTraits traits{};           // 任务特征（数值策略等）
    // 聚焦版（08 号计划）：分块契约与路由模式
    PartitionKind partition{PartitionKind::IndependentOutputTiles};
    RouteMode mode{RouteMode::AutoMixed};
    // 聚焦版 v3（08 号计划 §3）：输入是否已在设备显存（launcher 用 resident 路径）
    bool input_resident{false};
    std::uint64_t token_id{0};     // 执行时由 executor 回填的工作块 token id
                                   // （归约等需要按块定位输出的 kernel 使用）
    std::uint32_t attempt{0};      // 当前领取尝试次数（重试时 partial 需清零）
};

// ===== 参数 schema（注册时声明，执行时校验）=====
struct KernelArgSchema {
    std::size_t buffer_count{0};   // 期望 buffer 数
    std::size_t scalar_bytes{0};   // 期望标量字节数
};

// ===== Launcher 类型 =====
// 全部是 host callable 的函数指针；CUDA/HIP launcher 的语义是"在设备上启动
// 对应 kernel"，其内部必须由 CUDA/HIP backend 实现（不在此头文件展开）。
// 设备 launcher 收到 invocation 时，invocation.buffers 中的 data 已由
// executor 解析为设备指针（或 executor 支持的驻留形式）。
using CpuKernelLauncher  = void (*)(const KernelInvocation&, void* user_data);
using CudaKernelLauncher = void (*)(const KernelInvocation&, void* user_data);
using HipKernelLauncher  = void (*)(const KernelInvocation&, void* user_data);

// ===== KernelRegistration：一个 OperationId 的设备实现集合 =====
struct KernelRegistration {
    OperationId id{};              // 诊断/实现兼容标识（注册表内部复制存储）
    KernelArgSchema args{};
    CpuKernelLauncher cpu{nullptr};
    std::optional<CudaKernelLauncher> cuda;   // 无 CUDA 实现时为空
    std::optional<HipKernelLauncher> hip;     // 无 HIP 实现时为空
    NumericPolicy numeric{};
};

// ===== KernelRegistry：按 OperationId 注册/查找设备实现 =====
// 线程安全：register_kernel 与 find/supports 可并发调用。
// 注册表内部持有 id 的稳定拷贝（string 存储于稳定节点），返回的
// KernelRegistration* 在注册项被移除前一直有效。
class KernelRegistry {
public:
    KernelRegistry() = default;
    KernelRegistry(const KernelRegistry&) = delete;
    KernelRegistry& operator=(const KernelRegistry&) = delete;

    // 注册一个 OperationId 的设备实现。
    // 重复注册同一 id 返回 false（保留首个注册，不覆盖）。
    bool register_kernel(const KernelRegistration& reg);

    // 按 OperationId 查找注册项；不存在返回 nullptr。
    const KernelRegistration* find(OperationId id) const;

    // 该 OperationId 是否支持指定 backend（"cpu"/"cuda"/"cuda:0"/"hip"）。
    // 设备 launcher 缺失时返回 false —— 调用方必须回退 CPU 并如实报告。
    bool supports(OperationId id, const std::string& backend) const;

    // 已注册的 OperationId 列表（诊断用）。
    std::vector<std::string> operation_ids() const;

    std::size_t size() const noexcept { return nodes_.size(); }
    bool empty() const noexcept { return nodes_.empty(); }

private:
    // 稳定节点：id_storage 拥有字符串，reg.id 指向 id_storage。
    struct Node {
        std::string id_storage;
        KernelRegistration reg;
    };

    mutable std::mutex mtx_;
    std::vector<std::unique_ptr<Node>> nodes_;
    std::unordered_map<std::string, Node*> by_id_;
};

// ===== Invocation 契约校验（24 号计划 §5.2）=====
// Executor 提交前统一验证：
//   - OperationId 一致；
//   - buffer_count / scalar_bytes 与 KernelArgSchema 一致；
//   - domain 非空；
//   - 目标 backend 的 launcher 已注册；
//   - 调用方 NumericPolicy 与注册声明一致（声明必须反映 launcher 真实行为）。
// 返回空串表示通过；否则返回错误描述（Executor 应 Rejected）。
std::string validate_invocation(const KernelRegistration& reg,
                                const KernelInvocation& inv,
                                const std::string& backend);

// ===== 全局默认注册表（经典实验 CPU launcher 注册于此）=====
// 由 classic backend 模块（lib/acr/backends/classic）在首次使用时惰性注册。
KernelRegistry& global_kernel_registry();

} // namespace astro::compute
