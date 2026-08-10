// lib/acr/tests/unit/test_hardware_profile.cpp — HardwareProfile 数据结构单元测试
// Phase E1：覆盖 Curve 插值、DeviceProfile 查询、HardwareProfile 容器、枚举转字符串。
#include <gtest/gtest.h>

#include "astro/compute/hardware_profile.hpp"

#include <cstddef>
#include <vector>

using namespace astro::compute;

// ============================================================================
// Curve 预测
// ============================================================================

TEST(CurveTest, EmptyCurveReturnsZero) {
    Curve c;
    EXPECT_EQ(c.predict(1024), 0.0);
    EXPECT_FALSE(c.valid());
}

TEST(CurveTest, SinglePointReturnsConstant) {
    Curve c;
    c.points.push_back({1024, 100.0, 110.0, 5.0});
    EXPECT_DOUBLE_EQ(c.predict(512), 100.0);
    EXPECT_DOUBLE_EQ(c.predict(1024), 100.0);
    EXPECT_DOUBLE_EQ(c.predict(2048), 100.0);
    EXPECT_TRUE(c.valid());
}

TEST(CurveTest, TwoPointsLinearInterpolation) {
    Curve c;
    c.points.push_back({1024, 100.0, 110.0, 5.0});
    c.points.push_back({4096, 400.0, 420.0, 10.0});
    // log2(1024)=10, log2(4096)=12
    // 中间点 size=2048 → log2=11 → t=0.5 → predict = 100 + 0.5*(400-100) = 250
    double v = c.predict(2048);
    EXPECT_NEAR(v, 250.0, 1.0);
}

TEST(CurveTest, ExtrapolateBeyondRange) {
    Curve c;
    c.points.push_back({1024, 100.0, 110.0, 5.0});
    c.points.push_back({4096, 400.0, 420.0, 10.0});
    // 超出右端 → 返回最后一个点的 median
    EXPECT_DOUBLE_EQ(c.predict(8192), 400.0);
    // 超出左端 → 返回第一个点的 median
    EXPECT_DOUBLE_EQ(c.predict(256), 100.0);
}

TEST(CurveTest, PredictThroughput) {
    Curve c;
    c.points.push_back({1048576, 1000.0, 1200.0, 50.0});  // 1M 元素，1000 ns
    // bytes=4MB=4194304, ns=1000 → throughput = 4194304/1000 = 4194.304 GB/s
    double tp = c.predict_throughput(1048576, 4194304);
    EXPECT_NEAR(tp, 4194.304, 1.0);
}

TEST(CurveTest, PredictThroughputZeroSize) {
    Curve c;
    c.points.push_back({1024, 0.0, 0.0, 0.0});  // 0 耗时
    EXPECT_EQ(c.predict_throughput(1024, 4096), 0.0);
}

// ============================================================================
// DeviceProfile 查询
// ============================================================================

TEST(DeviceProfileTest, DefaultValuesCpu) {
    DeviceProfile dev;
    EXPECT_EQ(dev.device_id, kHwCpuDeviceId);
    EXPECT_EQ(dev.kind, DeviceKind::Cpu);
    EXPECT_FALSE(dev.has_gpu());
}

TEST(DeviceProfileTest, HasGpuWhenKindGpu) {
    DeviceProfile dev;
    dev.kind = DeviceKind::Gpu;
    EXPECT_TRUE(dev.has_gpu());
}

TEST(DeviceProfileTest, PredictArithmetic) {
    DeviceProfile dev;
    dev.arithmetic[{HwPrecision::Fp32, "add:avx2"}].points.push_back({1024, 100.0, 110.0, 5.0});
    dev.arithmetic[{HwPrecision::Fp32, "add:avx2"}].points.push_back({4096, 350.0, 380.0, 10.0});
    double cost = dev.predict_arithmetic(HwPrecision::Fp32, "add:avx2", 2048);
    EXPECT_GT(cost, 0.0);
    EXPECT_NEAR(cost, 225.0, 5.0);
}

TEST(DeviceProfileTest, PredictArithmeticMissing) {
    DeviceProfile dev;
    double cost = dev.predict_arithmetic(HwPrecision::Fp32, "nonexistent", 1024);
    EXPECT_EQ(cost, 0.0);
}

TEST(DeviceProfileTest, PredictMemory) {
    DeviceProfile dev;
    dev.memory[{MemoryLevel::MainMem, MemoryResidency::Host, "copy"}].points.push_back(
        {1048576, 68000.0, 72000.0, 1500.0});
    double cost = dev.predict_memory(MemoryLevel::MainMem, MemoryResidency::Host,
                                     "copy", 1048576);
    EXPECT_DOUBLE_EQ(cost, 68000.0);
}

TEST(DeviceProfileTest, PredictTransfer) {
    DeviceProfile dev;
    dev.transfer[{TransferDirection::H2D, MemoryType::HostPinned}].points.push_back(
        {4096, 1800.0, 2200.0, 120.0});
    double cost = dev.predict_transfer(TransferDirection::H2D, MemoryType::HostPinned, 4096);
    EXPECT_DOUBLE_EQ(cost, 1800.0);
}

TEST(DeviceProfileTest, PredictReduction) {
    DeviceProfile dev;
    dev.reduction[{"sum", HwPrecision::Fp32}].points.push_back({1048576, 850.0, 920.0, 30.0});
    double cost = dev.predict_reduction("sum", HwPrecision::Fp32, 1048576);
    EXPECT_DOUBLE_EQ(cost, 850.0);
}

TEST(DeviceProfileTest, GetOverhead) {
    DeviceProfile dev;
    FixedOverhead oh;
    oh.median_ns = 1100.0;
    oh.warm_ns = 600.0;
    dev.overhead["submit"] = oh;
    const FixedOverhead* p = dev.get_overhead("submit");
    ASSERT_NE(p, nullptr);
    EXPECT_DOUBLE_EQ(p->median_ns, 1100.0);
    EXPECT_DOUBLE_EQ(p->warm_ns, 600.0);
    EXPECT_EQ(dev.get_overhead("nonexistent"), nullptr);
}

TEST(DeviceProfileTest, GetLibrary) {
    DeviceProfile dev;
    LibraryCapability cap;
    cap.available = true;
    cap.implementation = "FFTW";
    cap.version = "3.3.10";
    dev.library["fft"] = std::move(cap);
    const LibraryCapability* p = dev.get_library("fft");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->available);
    EXPECT_EQ(p->implementation, "FFTW");
    EXPECT_EQ(p->version, "3.3.10");
    EXPECT_EQ(dev.get_library("nonexistent"), nullptr);
}

TEST(DeviceProfileTest, GetCurveConvolution) {
    DeviceProfile dev;
    dev.convolution["direct:3x3:fp32"].points.push_back({262144, 32000.0, 35000.0, 800.0});
    const Curve* c = dev.get_curve(CapabilityFamily::Convolution, "direct:3x3:fp32");
    ASSERT_NE(c, nullptr);
    EXPECT_DOUBLE_EQ(c->predict(262144), 32000.0);
}

TEST(DeviceProfileTest, GetCurveOverheadReturnsNull) {
    DeviceProfile dev;
    FixedOverhead oh;
    oh.median_ns = 100.0;
    dev.overhead["launch"] = oh;
    // overhead 不是 Curve，应返回 nullptr
    EXPECT_EQ(dev.get_curve(CapabilityFamily::Overhead, "launch"), nullptr);
}

TEST(DeviceProfileTest, PredictCostUnknownFamily) {
    DeviceProfile dev;
    EXPECT_EQ(dev.predict_cost(CapabilityFamily::Arithmetic, "unknown", 1024), 0.0);
}

// ============================================================================
// HardwareProfile 容器
// ============================================================================

TEST(HardwareProfileTest, DefaultStateMissing) {
    HardwareProfile hp;
    EXPECT_EQ(hp.state, HwProfileState::Missing);
    EXPECT_FALSE(hp.stale);
    EXPECT_EQ(hp.schema_version, "acr.hardware_profile.v1");
    EXPECT_FALSE(hp.has_gpu());
}

TEST(HardwareProfileTest, FindDeviceById) {
    HardwareProfile hp;
    DeviceProfile cpu;
    cpu.device_id = kHwCpuDeviceId;
    cpu.kind = DeviceKind::Cpu;
    DeviceProfile gpu;
    gpu.device_id = 1;
    gpu.kind = DeviceKind::Gpu;
    hp.devices = {cpu, gpu};

    ASSERT_NE(hp.find_device(kHwCpuDeviceId), nullptr);
    EXPECT_EQ(hp.find_device(kHwCpuDeviceId)->device_id, kHwCpuDeviceId);
    ASSERT_NE(hp.find_device(1), nullptr);
    EXPECT_EQ(hp.find_device(1)->device_id, 1);
    EXPECT_EQ(hp.find_device(99), nullptr);
}

TEST(HardwareProfileTest, FindDeviceConstById) {
    HardwareProfile hp;
    DeviceProfile cpu;
    cpu.device_id = kHwCpuDeviceId;
    hp.devices = {cpu};
    const HardwareProfile& chp = hp;
    ASSERT_NE(chp.find_device(kHwCpuDeviceId), nullptr);
    EXPECT_EQ(chp.find_device(kHwCpuDeviceId)->device_id, kHwCpuDeviceId);
}

TEST(HardwareProfileTest, HasGpu) {
    HardwareProfile hp;
    DeviceProfile cpu;
    cpu.kind = DeviceKind::Cpu;
    hp.devices = {cpu};
    EXPECT_FALSE(hp.has_gpu());

    DeviceProfile gpu;
    gpu.kind = DeviceKind::Gpu;
    hp.devices.push_back(gpu);
    EXPECT_TRUE(hp.has_gpu());
}

TEST(HardwareProfileTest, CpuDevice) {
    HardwareProfile hp;
    DeviceProfile cpu;
    cpu.device_id = kHwCpuDeviceId;
    cpu.kind = DeviceKind::Cpu;
    DeviceProfile gpu;
    gpu.device_id = 1;
    gpu.kind = DeviceKind::Gpu;
    hp.devices = {cpu, gpu};
    const DeviceProfile* p = hp.cpu_device();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->kind, DeviceKind::Cpu);
}

TEST(HardwareProfileTest, GpuDeviceIds) {
    HardwareProfile hp;
    DeviceProfile cpu;
    cpu.device_id = kHwCpuDeviceId;
    cpu.kind = DeviceKind::Cpu;
    DeviceProfile gpu1;
    gpu1.device_id = 1;
    gpu1.kind = DeviceKind::Gpu;
    DeviceProfile gpu2;
    gpu2.device_id = 2;
    gpu2.kind = DeviceKind::Gpu;
    hp.devices = {cpu, gpu1, gpu2};
    auto ids = hp.gpu_device_ids();
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 2);
}

TEST(HardwareProfileTest, CpuDeviceNullWhenNoCpu) {
    HardwareProfile hp;
    DeviceProfile gpu;
    gpu.kind = DeviceKind::Gpu;
    hp.devices = {gpu};
    EXPECT_EQ(hp.cpu_device(), nullptr);
}

// ============================================================================
// 枚举转字符串
// ============================================================================

TEST(EnumStringTest, DeviceKindStr) {
    EXPECT_STREQ(device_kind_str(DeviceKind::Cpu), "cpu");
    EXPECT_STREQ(device_kind_str(DeviceKind::Gpu), "gpu");
}

TEST(EnumStringTest, HwPrecisionStr) {
    EXPECT_STREQ(hw_precision_str(HwPrecision::Fp32), "fp32");
    EXPECT_STREQ(hw_precision_str(HwPrecision::Fp64), "fp64");
}

TEST(EnumStringTest, MemoryLevelStr) {
    EXPECT_STREQ(memory_level_str(MemoryLevel::L1), "L1");
    EXPECT_STREQ(memory_level_str(MemoryLevel::L2), "L2");
    EXPECT_STREQ(memory_level_str(MemoryLevel::L3), "L3");
    EXPECT_STREQ(memory_level_str(MemoryLevel::L4), "L4");
    EXPECT_STREQ(memory_level_str(MemoryLevel::MainMem), "MainMem");
    EXPECT_STREQ(memory_level_str(MemoryLevel::Vram), "Vram");
}

TEST(EnumStringTest, MemoryResidencyStr) {
    EXPECT_STREQ(memory_residency_str(MemoryResidency::Host), "host");
    EXPECT_STREQ(memory_residency_str(MemoryResidency::HostPinned), "host_pinned");
    EXPECT_STREQ(memory_residency_str(MemoryResidency::Device), "device");
    EXPECT_STREQ(memory_residency_str(MemoryResidency::DeviceManaged), "device_managed");
}

TEST(EnumStringTest, TransferDirectionStr) {
    EXPECT_STREQ(transfer_direction_str(TransferDirection::H2D), "h2d");
    EXPECT_STREQ(transfer_direction_str(TransferDirection::D2H), "d2h");
    EXPECT_STREQ(transfer_direction_str(TransferDirection::D2D), "d2d");
    EXPECT_STREQ(transfer_direction_str(TransferDirection::Bidir), "bidir");
}

TEST(EnumStringTest, MemoryTypeStr) {
    EXPECT_STREQ(memory_type_str(MemoryType::HostPlain), "host_plain");
    EXPECT_STREQ(memory_type_str(MemoryType::HostPinned), "host_pinned");
    EXPECT_STREQ(memory_type_str(MemoryType::Device), "device");
}

TEST(EnumStringTest, CapabilityFamilyStr) {
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Arithmetic), "arithmetic");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Memory), "memory");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Transfer), "transfer");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Reduction), "reduction");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Convolution), "convolution");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Irregular), "irregular");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Branch), "branch");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Overhead), "overhead");
    EXPECT_STREQ(capability_family_str(CapabilityFamily::Library), "library");
}

TEST(EnumStringTest, HwProfileStateStr) {
    EXPECT_STREQ(hw_profile_state_str(HwProfileState::Missing), "missing");
    EXPECT_STREQ(hw_profile_state_str(HwProfileState::Stale), "stale");
    EXPECT_STREQ(hw_profile_state_str(HwProfileState::Corrupt), "corrupt");
    EXPECT_STREQ(hw_profile_state_str(HwProfileState::Valid), "valid");
}
