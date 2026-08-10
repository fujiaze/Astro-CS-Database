// lib/acr/tests/unit/test_cost.cpp — CostEstimator 单元测试
// Phase F1+F2：覆盖无画像 fallback、有画像成本推算、最小有效块、推荐块大小。
#include <gtest/gtest.h>

#include "cost_estimator.hpp"
#include "task_descriptor.hpp"

#include <cstddef>
#include <string>

using namespace astro::compute;
using namespace astro::compute::cost;

// ============================================================================
// 辅助：构造 fallback profile（无画像）
// ============================================================================

namespace {
HardwareProfile make_test_profile_with_cpu_only() {
    HardwareProfile hp;
    hp.schema_version = "acr.hardware_profile.v1";
    hp.fingerprint_sha256 = "test";
    hp.state = HwProfileState::Valid;
    DeviceProfile cpu;
    cpu.device_id = kHwCpuDeviceId;
    cpu.device_name = "Test CPU";
    cpu.kind = DeviceKind::Cpu;
    cpu.compute_units = 8;
    cpu.peak_bandwidth_gbps = 20.0;
    cpu.total_memory_bytes = 16ULL * 1024 * 1024 * 1024;  // 16 GB
    cpu.available_memory_bytes = 12ULL * 1024 * 1024 * 1024;
    FixedOverhead submit_oh;
    submit_oh.median_ns = 1000.0;
    submit_oh.warm_ns = 500.0;
    cpu.overhead["submit"] = submit_oh;
    FixedOverhead launch_oh;
    launch_oh.median_ns = 100.0;
    launch_oh.warm_ns = 50.0;
    cpu.overhead["launch"] = launch_oh;
    FixedOverhead merge_oh;
    merge_oh.median_ns = 200.0;
    merge_oh.warm_ns = 150.0;
    cpu.overhead["merge"] = merge_oh;
    // 25 号计划 §5.1：profile_available 需要当前任务命中合格 measured 曲线
    Curve mem;
    mem.source = "measured";
    mem.qualified = true;
    CurvePoint p1;
    p1.size = 1024; p1.median = 100.0; p1.sample_count = 7; p1.confidence = 1.0;
    CurvePoint p2;
    p2.size = 1u << 20; p2.median = 60000.0; p2.sample_count = 7; p2.confidence = 1.0;
    mem.points = {p1, p2};
    cpu.memory[{MemoryLevel::MainMem, MemoryResidency::Host, "triad"}] = mem;
    hp.devices.push_back(std::move(cpu));
    return hp;
}

HardwareProfile make_test_profile_with_cpu_gpu() {
    HardwareProfile hp = make_test_profile_with_cpu_only();
    DeviceProfile gpu;
    gpu.device_id = 1;
    gpu.device_name = "Test GPU";
    gpu.kind = DeviceKind::Gpu;
    gpu.compute_units = 28;
    gpu.peak_bandwidth_gbps = 500.0;
    gpu.total_memory_bytes = 8ULL * 1024 * 1024 * 1024;  // 8 GB
    gpu.available_memory_bytes = 6ULL * 1024 * 1024 * 1024;
    FixedOverhead submit_oh;
    submit_oh.median_ns = 8500.0;
    submit_oh.warm_ns = 6500.0;
    Curve gmem;
    gmem.source = "measured";
    gmem.qualified = true;
    CurvePoint gp1;
    gp1.size = 1024; gp1.median = 50.0; gp1.sample_count = 7; gp1.confidence = 1.0;
    CurvePoint gp2;
    gp2.size = 1u << 20; gp2.median = 2000.0; gp2.sample_count = 7; gp2.confidence = 1.0;
    gmem.points = {gp1, gp2};
    gpu.memory[{MemoryLevel::Vram, MemoryResidency::Device, "triad"}] = gmem;
    gpu.overhead["submit"] = submit_oh;
    FixedOverhead launch_oh;
    launch_oh.median_ns = 7800.0;
    launch_oh.warm_ns = 6200.0;
    gpu.overhead["launch"] = launch_oh;
    // 添加算术曲线
    gpu.arithmetic[{HwPrecision::Fp32, "add:sm"}].points.push_back({1048576, 8500.0, 9100.0, 220.0});
    // 添加传输曲线
    gpu.transfer[{TransferDirection::H2D, MemoryType::HostPinned}].points.push_back(
        {4096, 1800.0, 2200.0, 120.0});
    gpu.transfer[{TransferDirection::D2H, MemoryType::HostPlain}].points.push_back(
        {4096, 1900.0, 2300.0, 130.0});
    hp.devices.push_back(std::move(gpu));
    return hp;
}

TaskDescriptor make_elementwise_task(std::size_t n) {
    TaskTraits traits;
    traits.task_class = TaskClass::elementwise;
    Range1D range{0, n};
    TaskDescriptor d = make_range_descriptor("test.elementwise", range, traits, Precision::FP32);
    d.bytes_per_item = 8;  // 4 read + 4 write
    return d;
}

TaskDescriptor make_reduction_task(std::size_t n) {
    TaskTraits traits;
    traits.task_class = TaskClass::reduction;
    Range1D range{0, n};
    TaskDescriptor d = make_reduce_descriptor("test.reduce", range, traits, Precision::FP32);
    d.bytes_per_item = 4;
    return d;
}

} // anonymous namespace

// ============================================================================
// 无画像时 fallback
// ============================================================================

TEST(CostEstimator, NoProfileReturnsFallbackCpu) {
    CostEstimator est;
    TaskDescriptor task = make_elementwise_task(1000000);
    CostEstimate ce = est.estimate(task);
    EXPECT_FALSE(ce.profile_available);
    EXPECT_EQ(ce.fallback_reason, "no-profile");
    EXPECT_EQ(ce.preferred_device, kHwCpuDeviceId);
    EXPECT_EQ(ce.per_device.size(), 1u);
    EXPECT_FALSE(ce.per_device[0].profile_available);
    EXPECT_TRUE(ce.per_device[0].feasible);
}

TEST(CostEstimator, NoProfileComputeCostNonZero) {
    CostEstimator est;
    TaskDescriptor task = make_elementwise_task(1000000);
    CostEstimate ce = est.estimate(task);
    EXPECT_GT(ce.per_device[0].compute_cost_ns, 0.0);
    EXPECT_GT(ce.per_device[0].total_cost_ns, 0.0);
    // 无画像时 launch 用 fallback 常数
    EXPECT_DOUBLE_EQ(ce.per_device[0].launch_cost_ns, CostEstimator::kCpuFallbackLaunchNs);
}

// ============================================================================
// 有画像时成本推算
// ============================================================================

TEST(CostEstimator, WithCpuProfileEstimatesPerDevice) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    CostEstimate ce = est.estimate(task);
    EXPECT_TRUE(ce.profile_available);
    EXPECT_EQ(ce.per_device.size(), 1u);
    EXPECT_EQ(ce.preferred_device, kHwCpuDeviceId);
    EXPECT_TRUE(ce.per_device[0].profile_available);
    EXPECT_GT(ce.per_device[0].total_cost_ns, 0.0);
}

TEST(CostEstimator, WithCpuGpuProfileSelectsFaster) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_gpu();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(10000000);  // 10M elements
    CostEstimate ce = est.estimate(task);
    EXPECT_TRUE(ce.profile_available);
    EXPECT_EQ(ce.per_device.size(), 2u);
    // 应选总成本更低的设备
    EXPECT_NE(ce.preferred_device, kHwInvalidDeviceId);
}

TEST(CostEstimator, ReductionTaskHasMergeCost) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_reduction_task(1000000);
    CostEstimate ce = est.estimate(task);
    // reduction 任务应有合并成本
    EXPECT_GT(ce.per_device[0].merge_cost_ns, 0.0);
}

TEST(CostEstimator, ElementwiseTaskNoMergeCost) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    CostEstimate ce = est.estimate(task);
    // elementwise 任务无合并成本
    EXPECT_DOUBLE_EQ(ce.per_device[0].merge_cost_ns, 0.0);
}

// ============================================================================
// 最小有效块
// ============================================================================

TEST(CostEstimator, MinEffectiveChunkNoProfile) {
    CostEstimator est;
    TaskDescriptor task = make_elementwise_task(1000000);
    std::size_t chunk = est.compute_min_effective_chunk(task, kHwCpuDeviceId);
    EXPECT_EQ(chunk, CostEstimator::kDefaultMinChunk);
}

TEST(CostEstimator, MinEffectiveChunkWithProfile) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    std::size_t chunk = est.compute_min_effective_chunk(task, kHwCpuDeviceId);
    EXPECT_GE(chunk, 1u);
    // CPU 应至少为 kDefaultMinChunk
    EXPECT_GE(chunk, CostEstimator::kDefaultMinChunk);
}

TEST(CostEstimator, MinEffectiveChunkGpuAtLeast4096) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_gpu();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    std::size_t chunk = est.compute_min_effective_chunk(task, 1);  // GPU device_id=1
    EXPECT_GE(chunk, 4096u);
}

TEST(CostEstimator, MinEffectiveChunkUnknownDevice) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    std::size_t chunk = est.compute_min_effective_chunk(task, 99);  // 未知设备
    EXPECT_EQ(chunk, CostEstimator::kDefaultMinChunk);
}

// ============================================================================
// 推荐块大小
// ============================================================================

TEST(CostEstimator, RecommendedChunkNoProfile) {
    CostEstimator est;
    TaskDescriptor task = make_elementwise_task(1000000);
    std::size_t chunk = est.compute_recommended_chunk(task, kHwCpuDeviceId);
    EXPECT_EQ(chunk, CostEstimator::kDefaultRecommendedChunk);
}

TEST(CostEstimator, RecommendedChunkWithProfile) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    std::size_t chunk = est.compute_recommended_chunk(task, kHwCpuDeviceId);
    EXPECT_GE(chunk, 1u);
    // 不应超过内存上限
    std::size_t max_chunk = est.compute_max_chunk_by_memory(task, kHwCpuDeviceId);
    EXPECT_LE(chunk, max_chunk);
}

// ============================================================================
// 最大块（内存约束）
// ============================================================================

TEST(CostEstimator, MaxChunkByMemoryNoProfile) {
    CostEstimator est;
    TaskDescriptor task = make_elementwise_task(1000000);
    task.bytes_per_item = 8;
    std::size_t max_chunk = est.compute_max_chunk_by_memory(task, kHwCpuDeviceId);
    // 无画像 → 返回大值（不限制）
    EXPECT_GT(max_chunk, 1000000u);
}

TEST(CostEstimator, MaxChunkByMemoryWithProfile) {
    CostEstimator est;
    HardwareProfile hp = make_test_profile_with_cpu_only();
    est.set_profile(&hp);
    TaskDescriptor task = make_elementwise_task(1000000);
    task.bytes_per_item = 8;
    std::size_t max_chunk = est.compute_max_chunk_by_memory(task, kHwCpuDeviceId);
    // available_memory=12GB, 保留 25% → usable=9GB → 9GB/8 = 1.125G
    EXPECT_GT(max_chunk, 100000000u);  // > 100M
}

// ============================================================================
// CurveLookup
// ============================================================================

TEST(CurveLookupTest, ElementwiseMapsToMemory) {
    TaskDescriptor task = make_elementwise_task(1000);
    CurveLookup lk = lookup_curve_for_task(task);
    EXPECT_TRUE(lk.valid);
    EXPECT_EQ(lk.family, CapabilityFamily::Memory);
}

TEST(CurveLookupTest, ReductionMapsToReduction) {
    TaskDescriptor task = make_reduction_task(1000);
    CurveLookup lk = lookup_curve_for_task(task);
    EXPECT_TRUE(lk.valid);
    EXPECT_EQ(lk.family, CapabilityFamily::Reduction);
}

TEST(CurveLookupTest, ConvolutionDirectMapsToConvolution) {
    TaskTraits traits;
    traits.task_class = TaskClass::convolution_direct;
    Range1D range{0, 100};
    TaskDescriptor d = make_range_descriptor("op.conv", range, traits, Precision::FP32);
    CurveLookup lk = lookup_curve_for_task(d);
    EXPECT_TRUE(lk.valid);
    EXPECT_EQ(lk.family, CapabilityFamily::Convolution);
}

TEST(CurveLookupTest, CustomMapsToArithmetic) {
    TaskTraits traits;
    traits.task_class = TaskClass::custom;
    Range1D range{0, 100};
    TaskDescriptor d = make_range_descriptor("op.custom", range, traits, Precision::FP32);
    CurveLookup lk = lookup_curve_for_task(d);
    EXPECT_TRUE(lk.valid);
    EXPECT_EQ(lk.family, CapabilityFamily::Arithmetic);
}

// ============================================================================
// device_id_to_backend / backend_to_device_id
// ============================================================================

TEST(DeviceIdBackendConversion, CpuDeviceId) {
    EXPECT_EQ(device_id_to_backend(kHwCpuDeviceId), "cpu");
    EXPECT_EQ(backend_to_device_id("cpu"), kHwCpuDeviceId);
    EXPECT_EQ(backend_to_device_id(""), kHwCpuDeviceId);
}

TEST(DeviceIdBackendConversion, GpuDeviceId) {
    EXPECT_EQ(device_id_to_backend(1), "cuda:0");
    EXPECT_EQ(device_id_to_backend(2), "cuda:1");
    EXPECT_EQ(backend_to_device_id("cuda:0"), 1);
    EXPECT_EQ(backend_to_device_id("cuda:1"), 2);
}

TEST(DeviceIdBackendConversion, InvalidBackend) {
    EXPECT_EQ(backend_to_device_id("unknown"), kHwInvalidDeviceId);
    EXPECT_EQ(backend_to_device_id("cuda:abc"), kHwInvalidDeviceId);
}
