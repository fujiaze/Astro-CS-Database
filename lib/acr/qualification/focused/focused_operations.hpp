// lib/acr/qualification/focused/focused_operations.hpp — 聚焦目标合成 Operation
//
// 08 §3：为积分/Drizzle 类重负载像素算法建立合成基准。
// 每个 Operation 提供：
// - CPU 实现（生产 CPU 后端，parallel_for/parallel_reduce）
// - GPU launcher（经桥接 DLL 的真实 CUDA kernel）
// - 统一 WorkloadDescriptor（CPU/GPU 工作量完全等价）
//
// 目标 OperationId：
// synthetic.dense_pixel_accumulate.fp32
// synthetic.dense_pixel_accumulate.fp64acc
// synthetic.pixel_reduce.fp64acc
// synthetic.drizzle_like_scatter.fp64acc
// synthetic.resident_chain
#pragma once

#include "astro/compute/acr.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::qualification::focused {

// ===== 统一工作量描述（CPU/GPU 共享同一语义）=====
struct FocusedWorkload {
    std::size_t logical_items{0};   // 总工作项数
    std::size_t input_bytes{0};     // 输入字节
    std::size_t output_bytes{0};    // 输出字节
    std::size_t bins{256};          // drizzle scatter 桶数（仅该操作使用）
    std::uint64_t seed{0xA57C5AC20260802ULL};
};

// ===== 单次测量样本 =====
struct FocusedSample {
    std::uint64_t kernel_ns{0};     // 纯计算（CPU：内核执行；GPU：resident 计算）
    std::uint64_t transfer_ns{0};   // H2D + D2H（CPU=0）
    std::uint64_t total_ns{0};      // 端到端
};

// ===== 目标 Operation 枚举 =====
enum class FocusedOp : std::uint8_t {
    DenseAccumulateFp32 = 0,
    DenseAccumulateFp64Acc = 1,
    PixelReduceFp64Acc = 2,
    DrizzleScatterFp64Acc = 3,
    ResidentChain = 4,
};

// OperationId 字符串（与 task_traits.hpp 常量一致）
const char* focused_op_id(FocusedOp op) noexcept;

// 每 item 输入/输出字节（memory 预算与传输估算）
std::size_t focused_input_bytes_per_item(FocusedOp op) noexcept;
std::size_t focused_output_bytes_per_item(FocusedOp op) noexcept;

// ===== CPU 实现（生产 CPU 后端，多线程）=====
// 返回纯计算耗时（ns）。输入由调用方填充（确定性 seed）。
std::uint64_t run_cpu_operation(FocusedOp op,
                                const std::vector<float>& x,
                                std::vector<float>& y,
                                std::vector<double>& partials,
                                std::size_t bins);

// ===== GPU launcher（经桥接 DLL；false=不支持/不可用）=====
// resident=true 表示数据已在显存（仅测计算+同步，不含 H2D/D2H 计时拆分）；
// 当前桥接为同步语义（H2D→launch→D2H 整体计时），host_roundtrip 返回总耗时。
bool run_gpu_operation(FocusedOp op,
                       const std::vector<float>& x,
                       std::vector<float>& y,
                       std::vector<double>& partials,
                       std::size_t bins,
                       std::uint64_t& elapsed_ns,
                       std::uint64_t& transfer_ns);

// GPU resident 测量：数据先上传一次并保留，之后只 launch（必要时 D2H 输出）。
// elapsed_ns 为 launch+sync（含必要 D2H）；transfer_ns 记录本测量内传输。
// 返回 false=不支持/不可用。
bool run_gpu_operation_resident(FocusedOp op,
                                const std::vector<float>& x,
                                std::vector<float>& y,
                                std::vector<double>& partials,
                                std::size_t bins,
                                std::uint64_t& elapsed_ns,
                                std::uint64_t& transfer_ns);

// ===== 确定性数据填充（CPU/GPU 同源）=====
void fill_uniform_fp32(float* p, std::size_t n, std::uint64_t seed);

// ===== 数值参考（可靠性基线，标量 FP64）=====
// dense accumulate：累加后 y[i]
void reference_dense_accumulate(const std::vector<float>& x,
                                std::vector<float>& y,
                                bool fp64_acc);
// pixel reduce：标量参考和
double reference_pixel_reduce(const std::vector<float>& x);
// drizzle scatter：标量参考桶
void reference_drizzle_scatter(const std::vector<float>& x,
                               std::vector<double>& bins,
                               std::size_t n_bins,
                               std::uint64_t seed);
// resident chain：z[i] = (x[i]+1)*2
void reference_resident_chain(const std::vector<float>& x,
                              std::vector<float>& z);

// ===== KernelRegistry 注册（CPU + CUDA launcher）=====
// 供 dispatch_invocation / Mixed 测试使用。幂等（可多次调用）。
// launcher 处理 invocation.domain 子域（chunk 范围）。
void register_focused_kernels();

// ===== 私有 partial 明确 merge=====
// drizzle：各 token 私有桶 [token_id*bins, (token_id+1)*bins) → 合并到 out
void merge_drizzle_partials(const double* token_partials,
                            std::size_t token_count,
                            std::size_t bins,
                            double* out);
// reduction：各 token 私有 partial[token_id] → 累加到 out（标量引用）
double merge_reduce_partials(const double* token_partials,
                             std::size_t token_count);

// ===== partial scratch 契约=====
// 按工作量与最小高效块计算所需 token 槽位数（调用方据此分配 partial buffer，
// 禁止按常数猜测）。槽位数 = ceil(work_size / min_chunk) + 1（防边界）。
std::size_t partial_slots_for(std::size_t work_size,
                              std::size_t min_chunk);

} // namespace astro::compute::qualification::focused
