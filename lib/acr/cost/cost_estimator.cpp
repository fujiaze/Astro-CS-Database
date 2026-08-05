// lib/acr/cost/cost_estimator.cpp — CostEstimator 实现
// Phase F1+F2：基于硬件画像的成本推算 + 最小有效块算法。
//
// 成本模型（07_STATIC_ROUTING_AND_MIXED_EXECUTION.md §3）：
//   T_device(chunk) = queue_wait + launch_or_submit + transfer_if_needed
//                    + compute_from_profile + local_merge_or_sync
//
// 无画像时：CPU fallback，用保守峰值带宽/overhead 估算。
#include "cost_estimator.hpp"

#include "../core/task_descriptor.hpp"
#include "../profile/profile_reader.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>

namespace astro::compute::cost {

namespace {

// ===== 从 TaskTraits 推算画像曲线 key =====
CurveLookup make_curve_lookup(const TaskDescriptor& task) noexcept {
    CurveLookup lk;
    lk.precision = (task.precision == Precision::FP64) ? HwPrecision::Fp64 : HwPrecision::Fp32;

    switch (task.traits.task_class) {
        case TaskClass::elementwise:
            lk.family = CapabilityFamily::Memory;
            // STREAM 风格：copy/scale/add/triad
            // 用 triad 作为通用 elementwise 代表（读2写1）
            lk.key = std::string(memory_level_str(MemoryLevel::MainMem)) + ":triad";
            lk.mem_level = MemoryLevel::MainMem;
            lk.residency = MemoryResidency::Host;
            lk.op = "triad";
            lk.valid = true;
            break;
        case TaskClass::reduction:
            lk.family = CapabilityFamily::Reduction;
            lk.key = "sum";  // 默认 sum；dot/min/max 由 traits.access 补充
            if (task.traits.access == AccessPattern::contiguous) lk.key = "sum";
            lk.valid = true;
            break;
        case TaskClass::stencil_2d:
        case TaskClass::convolution_direct:
            lk.family = CapabilityFamily::Convolution;
            lk.key = "direct:3x3:" + std::string(hw_precision_str(lk.precision));
            lk.valid = true;
            break;
        case TaskClass::convolution_separable:
            lk.family = CapabilityFamily::Convolution;
            lk.key = "separable:7:" + std::string(hw_precision_str(lk.precision));
            lk.valid = true;
            break;
        case TaskClass::resampling_gather:
        case TaskClass::sparse_gather:
            lk.family = CapabilityFamily::Irregular;
            {
                std::ostringstream os;
                os << "gather:random:" << task.traits.active_fraction_hint;
                lk.key = os.str();
            }
            lk.valid = true;
            break;
        case TaskClass::sparse_scatter:
            lk.family = CapabilityFamily::Irregular;
            lk.key = "scatter:atomic:hotspot";
            lk.valid = true;
            break;
        case TaskClass::histogram_atomic:
            lk.family = CapabilityFamily::Irregular;
            lk.key = "histogram:uniform";
            lk.valid = true;
            break;
        case TaskClass::branch_heavy:
            lk.family = CapabilityFamily::Branch;
            lk.key = work_uniformity_str(task.traits.uniformity);
            lk.valid = true;
            break;
        case TaskClass::batch_independent:
            // batch 视为 elementwise（每项独立）
            lk.family = CapabilityFamily::Memory;
            lk.key = std::string(memory_level_str(MemoryLevel::MainMem)) + ":copy";
            lk.mem_level = MemoryLevel::MainMem;
            lk.residency = MemoryResidency::Host;
            lk.valid = true;
            break;
        case TaskClass::fft_library:
            lk.family = CapabilityFamily::Library;
            lk.key = "fft";
            lk.valid = true;
            break;
        case TaskClass::gemm_library:
            lk.family = CapabilityFamily::Library;
            lk.key = "gemm";
            lk.valid = true;
            break;
        case TaskClass::custom:
        default:
            // 兜底：用算术曲线
            lk.family = CapabilityFamily::Arithmetic;
            lk.key = std::string(hw_precision_str(lk.precision)) + ":add:baseline";
            lk.valid = true;
            break;
    }
    return lk;
}

// ===== 估算单设备计算成本（纳秒）=====
// 优先用画像曲线；无曲线时用峰值带宽估算。
double estimate_compute_cost(const TaskDescriptor& task, const DeviceProfile& dev,
                              const CurveLookup& lk, std::size_t chunk_size,
                              bool& used_profile_curve,
                              bool& used_qualified_curve) {
    used_profile_curve = false;
    used_qualified_curve = false;
    std::size_t work = chunk_size;
    if (work == 0) work = task.work_size();
    if (work == 0) return 0.0;

    // 优先查画像曲线
    if (lk.valid) {
        double curve_cost = 0.0;
        switch (lk.family) {
            case CapabilityFamily::Arithmetic: {
                // key 格式 "fp32:add:avx2"
                std::string op_isa = lk.key;
                // 去掉前缀 "fp32:" 或 "fp64:"
                auto colon = op_isa.find(':');
                if (colon != std::string::npos) op_isa = op_isa.substr(colon + 1);
                auto it = dev.arithmetic.find({lk.precision, op_isa});
                if (it != dev.arithmetic.end()) {
                    curve_cost = it->second.predict(work);
                    used_qualified_curve = it->second.qualified;
                }
                break;
            }
            case CapabilityFamily::Memory:
                // 25 号计划 §4：内存曲线按 (level, residency, op) 区分；
                // GPU 用显存/设备驻留
                {
                    MemoryLevel lvl = lk.mem_level;
                    MemoryResidency res = lk.residency;
                    std::string op = lk.op.empty() ? "triad" : lk.op;
                    if (dev.kind == DeviceKind::Gpu) {
                        lvl = MemoryLevel::Vram;
                        res = MemoryResidency::Device;
                    }
                    curve_cost = dev.predict_memory(lvl, res, op, work);
                    const Curve* c = dev.memory.count({lvl, res, op})
                        ? &dev.memory.at({lvl, res, op}) : nullptr;
                    if (c) used_qualified_curve = c->qualified;
                }
                break;
            case CapabilityFamily::Transfer:
                // transfer 不在 compute 中算
                break;
            case CapabilityFamily::Reduction:
                {
                    auto it = dev.reduction.find({lk.key, lk.precision});
                    if (it != dev.reduction.end()) {
                        curve_cost = it->second.predict(work);
                        used_qualified_curve = it->second.qualified;
                    }
                }
                break;
            case CapabilityFamily::Convolution:
            case CapabilityFamily::Irregular:
            case CapabilityFamily::Branch: {
                const Curve* c = dev.get_curve(lk.family, lk.key);
                if (c) {
                    curve_cost = c->predict(work);
                    used_qualified_curve = c->qualified;
                }
                break;
            }
            case CapabilityFamily::Overhead:
                break;
            case CapabilityFamily::Library: {
                const LibraryCapability* cap = dev.get_library(lk.key);
                if (cap) {
                    auto it = cap->size_curves.find("default");
                    if (it != cap->size_curves.end()) {
                        curve_cost = it->second.predict(work);
                        used_qualified_curve = it->second.qualified;
                    }
                }
                break;
            }
        }
        if (curve_cost > 0.0) {
            used_profile_curve = true;
            if (!used_qualified_curve) {
                // 命中曲线但未合格（quick 或样本不足）：按合格曲线缺失处理
                used_profile_curve = false;
            }
            return curve_cost;
        }
    }

    // Fallback：用峰值带宽估算
    // 估算字节数：优先用 bytes_per_item × work_size
    std::size_t bytes = task.bytes_per_item > 0 ? (task.bytes_per_item * work) :
                        (task.bytes_read + task.bytes_written);
    if (bytes == 0) {
        // 默认假设：每元素 4 字节读 + 4 字节写（fp32 elementwise）
        bytes = work * 8;
    }
    double bw_gbps = dev.peak_bandwidth_gbps;
    if (bw_gbps <= 0.0) {
        // 无画像：保守估算
        bw_gbps = dev.kind == DeviceKind::Gpu
                  ? CostEstimator::kGpuFallbackBandwidthGbps
                  : CostEstimator::kCpuFallbackBandwidthGbps;
    }
    // bytes / (GB/s × 1e9 B/s/GB) × 1e9 ns/s = bytes / bw_gbps
    double cost_ns = static_cast<double>(bytes) / bw_gbps;
    if (cost_ns < 0.0) cost_ns = 0.0;
    return cost_ns;
}

// ===== 估算 GPU 传输成本（H2D + D2H，纳秒）=====
double estimate_transfer_cost(const TaskDescriptor& task, const DeviceProfile& gpu_dev,
                               std::size_t chunk_size, bool& used_profile_curve) {
    used_profile_curve = false;
    // CPU 任务不产生传输
    if (gpu_dev.kind != DeviceKind::Gpu) return 0.0;
    // 数据已在 GPU 上时不产生 H2D（output_residency == gpu_dev.device_id）
    if (task.input_residency == gpu_dev.device_id &&
        task.output_residency == gpu_dev.device_id) {
        return 0.0;
    }

    // 25 号计划 §5.2：传输字节必须是“单块”字节，禁止任务总量 × 块数重复放大
    const std::size_t work = chunk_size > 0 ? chunk_size : task.work_size();
    const std::size_t total_work = task.work_size();
    std::size_t per_chunk_bytes = 0;
    if (task.bytes_per_item > 0) {
        per_chunk_bytes = task.bytes_per_item * work;
    } else if (total_work > 0) {
        per_chunk_bytes = (task.bytes_read + task.bytes_written) * work / total_work;
    } else {
        per_chunk_bytes = work * 8;
    }
    if (per_chunk_bytes == 0) per_chunk_bytes = work * 8;
    const std::size_t read_bytes_per_chunk =
        task.bytes_read > 0 && total_work > 0
            ? task.bytes_read * work / total_work : per_chunk_bytes / 2;
    const std::size_t write_bytes_per_chunk =
        task.bytes_written > 0 && total_work > 0
            ? task.bytes_written * work / total_work : per_chunk_bytes / 2;

    // 优先查画像传输曲线
    // CPU→GPU 用 H2D + D2H 双向
    double h2d_cost = gpu_dev.predict_transfer(TransferDirection::H2D,
                                                 MemoryType::HostPinned,
                                                 read_bytes_per_chunk);
    double d2h_cost = gpu_dev.predict_transfer(TransferDirection::D2H,
                                                 MemoryType::HostPlain,
                                                 write_bytes_per_chunk);
    if (h2d_cost > 0.0 || d2h_cost > 0.0) {
        used_profile_curve = true;
        return h2d_cost + d2h_cost;
    }

    // Fallback：用 PCIe 带宽估算
    double pcie_gbps = CostEstimator::kGpuFallbackPcieGbps;
    double cost_ns = static_cast<double>(per_chunk_bytes) / pcie_gbps;
    if (cost_ns < 0.0) cost_ns = 0.0;
    return cost_ns;
}

// ===== 估算 launch/submit 开销（纳秒）=====
double estimate_launch_cost(const DeviceProfile& dev) {
    const FixedOverhead* oh = dev.get_overhead("submit");
    if (oh && oh->warm_ns > 0.0) return oh->warm_ns;
    if (oh && oh->median_ns > 0.0) return oh->median_ns;
    // Fallback
    return dev.kind == DeviceKind::Gpu
           ? CostEstimator::kGpuFallbackLaunchNs
           : CostEstimator::kCpuFallbackLaunchNs;
}

// ===== 估算合并成本（纳秒）=====
double estimate_merge_cost(const TaskDescriptor& task, const DeviceProfile& dev) {
    // 归约任务才有合并成本；elementwise 通常为 0
    if (task.traits.task_class != TaskClass::reduction) return 0.0;
    const FixedOverhead* oh = dev.get_overhead("merge");
    if (oh && oh->warm_ns > 0.0) return oh->warm_ns;
    if (oh && oh->median_ns > 0.0) return oh->median_ns;
    return 200.0;  // 保守估算
}

// ===== 推算最大块（受内存约束）=====
std::size_t compute_max_chunk_by_memory_impl(const TaskDescriptor& task,
                                              const DeviceProfile& dev) {
    std::size_t bytes_per_item = task.bytes_per_item > 0 ? task.bytes_per_item : 8;
    std::size_t avail = dev.available_memory_bytes > 0 ? dev.available_memory_bytes : dev.total_memory_bytes;
    if (avail == 0 || bytes_per_item == 0) {
        // 无内存信息：返回大值（不限制）
        return static_cast<std::size_t>(-1) / 2;
    }
    // 保留 25% 内存给系统/其他任务
    std::size_t usable = (avail * 3) / 4;
    return usable / bytes_per_item;
}

} // anonymous namespace

// ===== CurveLookup 公开接口 =====
CurveLookup lookup_curve_for_task(const TaskDescriptor& task) noexcept {
    return make_curve_lookup(task);
}

// ===== CostEstimator::Impl =====
struct CostEstimator::Impl {
    const HardwareProfile* profile{nullptr};
};

CostEstimator::CostEstimator() : impl_(std::make_unique<Impl>()) {}
CostEstimator::~CostEstimator() = default;

void CostEstimator::set_profile(const HardwareProfile* profile) noexcept {
    impl_->profile = profile;
}

const HardwareProfile* CostEstimator::profile() const noexcept {
    return impl_->profile;
}

void CostEstimator::refresh_from_reader(profile::HardwareProfileReader& reader) {
    // 取 fallback 引用的地址（fallback 在 reader 内部稳定存储，invalidate_cache 后会重建）
    impl_->profile = &reader.get_profile_or_cpu_fallback();
}

// ===== Phase F2：最小有效块 =====
std::size_t CostEstimator::compute_min_effective_chunk(const TaskDescriptor& task,
                                                        DeviceId device) const {
    const HardwareProfile* hp = impl_->profile;
    if (!hp) return kDefaultMinChunk;
    const DeviceProfile* dev = hp->find_device(device);
    if (!dev) return kDefaultMinChunk;

    double launch_ns = estimate_launch_cost(*dev);
    if (launch_ns <= 0.0) return kDefaultMinChunk;

    // 条件1：计算时间 >= kMinComputeToLaunchRatio × launch
    // 用 fallback 估算每元素计算成本（保守：1 ns/元素 for fp32 elementwise）
    // 实际从曲线查会更准，但最小块要保守（避免过小）
    double compute_per_elem_ns = 1.0;  // 保守
    // 如果有画像曲线，用小尺寸（如 1024）的预测值估算每元素成本
    CurveLookup lk = make_curve_lookup(task);
    if (lk.valid) {
        bool used = false;
        bool used_qualified = false;
        double sample_cost = estimate_compute_cost(task, *dev, lk, 1024,
                                                   used, used_qualified);
        if (sample_cost > 0.0) {
            compute_per_elem_ns = sample_cost / 1024.0;
        }
    }
    if (compute_per_elem_ns <= 0.0) compute_per_elem_ns = 1.0;

    // chunk_compute = chunk_size × compute_per_elem_ns >= kMinComputeToLaunchRatio × launch_ns
    std::size_t chunk_by_launch = static_cast<std::size_t>(
        (kMinComputeToLaunchRatio * launch_ns) / compute_per_elem_ns);
    if (chunk_by_launch < 1) chunk_by_launch = 1;

    // 条件2（GPU）：传输时间 <= kTransferGainRatio × 计算时间
    std::size_t chunk_by_transfer = 1;
    if (dev->kind == DeviceKind::Gpu) {
        // 传输 = bytes / pcie_bw；计算 = chunk × compute_per_elem
        // 满足：bytes/(pcie_bw) <= kTransferGainRatio × chunk × compute_per_elem
        // bytes = chunk × bytes_per_item
        // → chunk × bytes_per_item / pcie_bw <= kTransferGainRatio × chunk × compute_per_elem
        // → bytes_per_item / pcie_bw <= kTransferGainRatio × compute_per_elem
        // 不依赖 chunk（线性抵消）；只要 GPU 计算密集度足够，任意 chunk 都可
        // 但小 chunk 的 launch 开销主导，所以用条件1主导
        // 这里加一个下限：chunk >= 4096（GPU 最小粒度，避免过碎）
        chunk_by_transfer = 4096;
    }

    // 条件3：chunk × bytes_per_item <= available_memory
    std::size_t chunk_by_mem = compute_max_chunk_by_memory_impl(task, *dev);

    std::size_t min_chunk = std::max({chunk_by_launch, chunk_by_transfer,
                                       static_cast<std::size_t>(1)});
    min_chunk = std::min(min_chunk, chunk_by_mem);

    // Tile 任务：chunk 受 tile 总元素数约束
    if (task.is_2d() && task.tile.tile_w > 0 && task.tile.tile_h > 0) {
        std::size_t tile_elems = task.tile.tile_w * task.tile.tile_h;
        if (tile_elems > 0) min_chunk = std::min(min_chunk, tile_elems);
    }

    if (min_chunk == 0) min_chunk = 1;
    return min_chunk;
}

// ===== 推荐块大小 =====
std::size_t CostEstimator::compute_recommended_chunk(const TaskDescriptor& task,
                                                      DeviceId device) const {
    const HardwareProfile* hp = impl_->profile;
    if (!hp) return kDefaultRecommendedChunk;
    const DeviceProfile* dev = hp->find_device(device);
    if (!dev) return kDefaultRecommendedChunk;

    std::size_t min_chunk = compute_min_effective_chunk(task, device);
    std::size_t max_chunk = compute_max_chunk_by_memory_impl(task, *dev);

    // 目标：计算/launch = kTargetComputeRatio
    double launch_ns = estimate_launch_cost(*dev);
    double compute_per_elem_ns = 1.0;
    CurveLookup lk = make_curve_lookup(task);
    if (lk.valid) {
        bool used = false;
        bool used_qualified = false;
        double sample_cost = estimate_compute_cost(task, *dev, lk, 1024,
                                                   used, used_qualified);
        if (sample_cost > 0.0) compute_per_elem_ns = sample_cost / 1024.0;
    }
    if (compute_per_elem_ns <= 0.0 || launch_ns <= 0.0) {
        return std::min(std::max(min_chunk, kDefaultRecommendedChunk), max_chunk);
    }

    std::size_t target_chunk = static_cast<std::size_t>(
        (kTargetComputeRatio * launch_ns) / compute_per_elem_ns);
    if (target_chunk < min_chunk) target_chunk = min_chunk;
    if (target_chunk > max_chunk) target_chunk = max_chunk;

    // GPU 倾向更大块（减少 launch 占比）
    if (dev->kind == DeviceKind::Gpu) {
        target_chunk = std::max(target_chunk, static_cast<std::size_t>(65536));
        if (target_chunk > max_chunk) target_chunk = max_chunk;
    }

    // Tile 任务：不超过 tile 总元素数
    if (task.is_2d() && task.tile.tile_w > 0 && task.tile.tile_h > 0) {
        std::size_t tile_elems = task.tile.tile_w * task.tile.tile_h;
        if (tile_elems > 0 && target_chunk > tile_elems) target_chunk = tile_elems;
    }

    if (target_chunk == 0) target_chunk = min_chunk;
    return target_chunk;
}

std::size_t CostEstimator::compute_max_chunk_by_memory(const TaskDescriptor& task,
                                                         DeviceId device) const {
    const HardwareProfile* hp = impl_->profile;
    if (!hp) return static_cast<std::size_t>(-1) / 2;
    const DeviceProfile* dev = hp->find_device(device);
    if (!dev) return static_cast<std::size_t>(-1) / 2;
    return compute_max_chunk_by_memory_impl(task, *dev);
}

// ===== 23 号计划 §4：每设备块大小推算 =====
// 目标批次时长 × 设备吞吐 → requested_items；队列越深块越小；尾部收缩。
// 只使用该设备的 DeviceCost（每 executor 独立），与设备数量无关。
std::size_t CostEstimator::compute_requested_items(
    const DeviceCost& cost, std::size_t remaining,
    std::size_t queue_depth) const noexcept {
    if (remaining == 0) return 0;

    std::size_t base = cost.recommended_chunk;
    if (base == 0) base = kDefaultRecommendedChunk;

    // 队列越深，块越小（控制提交节奏与拖尾延迟）
    if (queue_depth > 0) {
        base /= (queue_depth + 1);
    }
    // 尾部收缩：剩余不足 2 块时直接取剩余
    if (remaining < base * 2) {
        base = remaining;
    }
    // 受该设备内存上限约束
    if (cost.max_chunk_by_memory > 0 && base > cost.max_chunk_by_memory) {
        base = cost.max_chunk_by_memory;
    }
    // 不小于该设备最小有效块
    if (cost.min_effective_chunk > 0 && base < cost.min_effective_chunk) {
        base = cost.min_effective_chunk;
    }
    if (base > remaining) base = remaining;
    if (base == 0) base = 1;
    return base;
}

// ===== 单设备成本估算 =====
DeviceCost CostEstimator::estimate_for_device(const TaskDescriptor& task,
                                               DeviceId device) const {
    DeviceCost dc;
    dc.device_id = device;
    dc.backend = device_id_to_backend(device);

    const HardwareProfile* hp = impl_->profile;
    if (!hp) {
        // 无画像：CPU fallback
        dc.device_name = "CPU (fallback, no profile)";
        dc.reason = "no-profile-fallback";
        dc.profile_available = false;
        // 保守估算
        std::size_t work = task.work_size();
        std::size_t bytes = task.bytes_per_item > 0 ? (task.bytes_per_item * work) :
                            (task.bytes_read + task.bytes_written);
        if (bytes == 0) bytes = work * 8;
        dc.compute_cost_ns = static_cast<double>(bytes) / kCpuFallbackBandwidthGbps;
        dc.transfer_cost_ns = 0.0;  // CPU 无传输
        dc.launch_cost_ns = kCpuFallbackLaunchNs;
        dc.merge_cost_ns = (task.traits.task_class == TaskClass::reduction) ? 200.0 : 0.0;
        dc.total_cost_ns = dc.compute_cost_ns + dc.launch_cost_ns + dc.merge_cost_ns;
        dc.min_effective_chunk = kDefaultMinChunk;
        dc.recommended_chunk = kDefaultRecommendedChunk;
        dc.estimated_chunk_count = work > 0 ? (work + dc.recommended_chunk - 1) / dc.recommended_chunk : 0;
        dc.max_chunk_by_memory = static_cast<std::size_t>(-1) / 2;
        dc.feasible = true;
        if (dc.compute_cost_ns > 0.0) {
            dc.predicted_throughput_gbps = static_cast<double>(bytes) / dc.compute_cost_ns;
        }
        return dc;
    }

    const DeviceProfile* dev = hp->find_device(device);
    if (!dev) {
        dc.feasible = false;
        dc.reason = "device-not-found";
        return dc;
    }

    dc.device_name = dev->device_name;
    // 25 号计划 §5.1：profile_available 仅当“当前任务命中合格（full、样本>=7）
    // measured 曲线”时为真；默认开销/峰值带宽不算合格画像。
    // （在成本计算后由 used_compute_qualified 更新）
    dc.profile_available = false;
    dc.profile_fallback_reason = "no-qualified-curve-for-task";

    std::size_t work = task.work_size();
    std::size_t bytes = task.bytes_per_item > 0 ? (task.bytes_per_item * work) :
                        (task.bytes_read + task.bytes_written);
    if (bytes == 0) bytes = work * 8;

    // 块大小
    dc.min_effective_chunk = compute_min_effective_chunk(task, device);
    dc.recommended_chunk = compute_recommended_chunk(task, device);
    dc.max_chunk_by_memory = compute_max_chunk_by_memory_impl(task, *dev);
    dc.estimated_chunk_count = work > 0 && dc.recommended_chunk > 0
        ? (work + dc.recommended_chunk - 1) / dc.recommended_chunk : 0;

    // 成本（按推荐块估算单块成本 × 块数）
    std::size_t chunk = dc.recommended_chunk > 0 ? dc.recommended_chunk : work;
    if (chunk == 0) chunk = 1;

    CurveLookup lk = make_curve_lookup(task);
    bool used_compute_curve = false, used_compute_qualified = false;
    bool used_transfer_curve = false;
    double per_chunk_compute = estimate_compute_cost(
        task, *dev, lk, chunk, used_compute_curve, used_compute_qualified);
    double per_chunk_transfer = estimate_transfer_cost(
        task, *dev, chunk, used_transfer_curve);
    double per_chunk_launch = estimate_launch_cost(*dev);
    double per_chunk_merge = estimate_merge_cost(task, *dev);

    std::size_t n_chunks = dc.estimated_chunk_count > 0 ? dc.estimated_chunk_count : 1;

    dc.compute_cost_ns = per_chunk_compute * static_cast<double>(n_chunks);
    dc.transfer_cost_ns = per_chunk_transfer * static_cast<double>(n_chunks);
    dc.launch_cost_ns = per_chunk_launch * static_cast<double>(n_chunks);
    dc.merge_cost_ns = per_chunk_merge * static_cast<double>(n_chunks);
    dc.total_cost_ns = dc.compute_cost_ns + dc.transfer_cost_ns +
                       dc.launch_cost_ns + dc.merge_cost_ns;

    dc.reason = used_compute_curve ? "profile-curve" : "fallback-peak";
    dc.profile_available = used_compute_qualified;
    if (!used_compute_qualified) {
        dc.profile_fallback_reason =
            used_compute_curve ? "curve-not-qualified" : "no-curve-for-task";
    }
    dc.feasible = (dc.max_chunk_by_memory > 0) && (chunk <= dc.max_chunk_by_memory);

    if (dc.compute_cost_ns > 0.0) {
        dc.predicted_throughput_gbps = static_cast<double>(bytes) / dc.compute_cost_ns;
    }

    return dc;
}

// ===== 主入口：估算任务成本 =====
CostEstimate CostEstimator::estimate(const TaskDescriptor& task) const {
    CostEstimate est;
    est.total_work_size = task.work_size();

    const HardwareProfile* hp = impl_->profile;
    if (!hp) {
        est.profile_available = false;
        est.fallback_reason = "no-profile";
        // 仅 CPU fallback
        DeviceCost cpu_cost = estimate_for_device(task, kHwCpuDeviceId);
        est.per_device.push_back(std::move(cpu_cost));
        est.preferred_device = kHwCpuDeviceId;
    } else {
        est.profile_available = true;
        if (hp->state == HwProfileState::Stale) {
            est.profile_stale = true;
            est.fallback_reason = "stale";
        } else if (hp->state == HwProfileState::Corrupt) {
            est.fallback_reason = "corrupt";
        }
        // 估算每个设备
        for (const auto& dev : hp->devices) {
            DeviceCost dc = estimate_for_device(task, dev.device_id);
            est.per_device.push_back(std::move(dc));
        }
        // 选最优（总成本最低的可行设备）
        double best_cost = std::numeric_limits<double>::max();
        DeviceId best_dev = kHwInvalidDeviceId;
        for (const auto& dc : est.per_device) {
            if (!dc.feasible) continue;
            if (dc.total_cost_ns < best_cost) {
                best_cost = dc.total_cost_ns;
                best_dev = dc.device_id;
            }
        }
        // 无可行设备时回退 CPU
        if (best_dev == kHwInvalidDeviceId) {
            est.preferred_device = kHwCpuDeviceId;
            est.fallback_reason = "no-feasible-device";
        } else {
            est.preferred_device = best_dev;
        }
    }

    // 生成摘要
    std::ostringstream os;
    os << "CostEstimate{work=" << est.total_work_size
       << ",profile=" << (est.profile_available ? "yes" : "no")
       << ",preferred=" << device_id_to_backend(est.preferred_device)
       << ",devices=" << est.per_device.size() << "}";
    est.estimate_summary = os.str();

    return est;
}

// ===== 全局单例 =====
CostEstimator& global_cost_estimator() {
    static CostEstimator inst;
    // 首次访问时从 global_profile_reader 加载画像
    // 用 call_once 保证线程安全初始化
    static std::once_flag flag;
    std::call_once(flag, []() {
        inst.refresh_from_reader(profile::global_profile_reader());
    });
    return inst;
}

} // namespace astro::compute::cost
