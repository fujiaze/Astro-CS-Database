// lib/acr/tests/unit/test_cpu_profile.cpp — CPU 画像生成与读取单元测试
// Phase C：验证 ProfileGenerator 生成的 HardwareProfile 结构正确、JSON 可解析
//
// 覆盖：
// 1. ProfileGenerator::generate_hardware_profile 基本流程
// 2. CPU device 默认存在（即使无 benchmark 结果）
// 3. benchmark 结果映射到能力曲线（memory/arithmetic/reduction）
// 4. JSON 序列化包含必需字段（schema_version/devices/fingerprint）
// 5. write_hardware_profile_to_file + 读取回放
// 6. 指纹非空（SHA-256 64 字符）
// 7. DeviceProfile 查询接口
#include <gtest/gtest.h>

#include "astro/compute/hardware_profile.hpp"
#include "profile_generator.hpp"
#include "profile_schema.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace astro::compute;
using namespace astro::compute::qualification;

namespace {

// 构造一个简单的 KernelBenchmarkResult（模拟 STREAM Copy）
KernelBenchmarkResult make_copy_result(std::size_t size_bytes, double ns) {
    KernelBenchmarkResult r;
    r.kernel_id = 1;  // Copy → memory curve
    r.kernel_name = "copy_fp32";
    r.backend = "cpu";
    r.precision = "fp32";
    r.problem_size = size_bytes;
    r.bytes_per_element = 4;
    r.median_kernel_ns = static_cast<std::uint64_t>(ns);
    r.median_total_ns = static_cast<std::uint64_t>(ns);
    r.median_throughput_gbps = static_cast<double>(size_bytes) / ns;
    r.stddev_kernel_ns = ns * 0.05;
    RawBenchmarkSample s;
    s.kernel_ns = static_cast<std::uint64_t>(ns);
    s.total_ns = static_cast<std::uint64_t>(ns);
    s.throughput_gbps = static_cast<double>(size_bytes) / ns;
    r.samples.push_back(s);
    return r;
}

// 构造一个 AXPY 结果（→ arithmetic curve）
KernelBenchmarkResult make_axpy_result(std::size_t n, double ns) {
    KernelBenchmarkResult r;
    r.kernel_id = 3;  // AXPY → arithmetic curve
    r.kernel_name = "axpy_fp32";
    r.backend = "cpu";
    r.precision = "fp32";
    r.problem_size = n;
    r.bytes_per_element = 8;  // x + y
    r.median_kernel_ns = static_cast<std::uint64_t>(ns);
    r.median_total_ns = static_cast<std::uint64_t>(ns);
    r.median_throughput_gbps = static_cast<double>(n * 8) / ns;
    r.stddev_kernel_ns = ns * 0.05;
    RawBenchmarkSample s;
    s.kernel_ns = static_cast<std::uint64_t>(ns);
    s.total_ns = static_cast<std::uint64_t>(ns);
    s.throughput_gbps = static_cast<double>(n * 8) / ns;
    r.samples.push_back(s);
    return r;
}

// 构造一个 Dot 结果（→ reduction curve）
KernelBenchmarkResult make_dot_result(std::size_t n, double ns) {
    KernelBenchmarkResult r;
    r.kernel_id = 4;  // Dot → reduction curve
    r.kernel_name = "dot_fp32";
    r.variant = "dot";  // 25 §4：dot 与 sum 是不同曲线
    r.backend = "cpu";
    r.precision = "fp32";
    r.problem_size = n;
    r.bytes_per_element = 8;
    r.median_kernel_ns = static_cast<std::uint64_t>(ns);
    r.median_total_ns = static_cast<std::uint64_t>(ns);
    r.median_throughput_gbps = static_cast<double>(n * 8) / ns;
    r.stddev_kernel_ns = ns * 0.05;
    RawBenchmarkSample s;
    s.kernel_ns = static_cast<std::uint64_t>(ns);
    s.total_ns = static_cast<std::uint64_t>(ns);
    s.throughput_gbps = static_cast<double>(n * 8) / ns;
    r.samples.push_back(s);
    return r;
}

// 临时文件路径助手
std::string temp_file_path(const std::string& suffix) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "acr_test_cpu_profile_%lld.%s",
                  static_cast<long long>(std::time(nullptr)), suffix.c_str());
    return buf;
}

// 检查 JSON 字符串是否包含子串
bool json_contains(const std::string& json, const std::string& key) {
    return json.find(key) != std::string::npos;
}

} // anonymous namespace

// ============================================================================
// 1. ProfileGenerator 基本：无 benchmark 结果时仍生成 CPU device
// ============================================================================

TEST(CpuProfileTest, GenerateEmptyHasCpuDevice) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> empty;
    HardwareProfile hp = gen.generate_hardware_profile(empty, ProfileKind::Quick);

    EXPECT_EQ(hp.schema_version, "acr.hardware_profile.v1");
    EXPECT_EQ(hp.profile_kind, "quick");
    EXPECT_EQ(hp.state, HwProfileState::Valid);
    EXPECT_FALSE(hp.stale);
    EXPECT_FALSE(hp.devices.empty());
    // CPU device 应存在
    const DeviceProfile* cpu = hp.find_device(kHwCpuDeviceId);
    EXPECT_NE(cpu, nullptr);
    EXPECT_EQ(cpu->kind, DeviceKind::Cpu);
}

// ============================================================================
// 2. 指纹非空（SHA-256 应为 64 字符十六进制）
// ============================================================================

TEST(CpuProfileTest, FingerprintNonEmpty) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> empty;
    HardwareProfile hp = gen.generate_hardware_profile(empty, ProfileKind::Standard);

    EXPECT_FALSE(hp.fingerprint_sha256.empty());
    // SHA-256 输出为 64 字符十六进制
    EXPECT_EQ(hp.fingerprint_sha256.size(), 64u);
    // 全部应为十六进制字符
    for (char c : hp.fingerprint_sha256) {
        bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        EXPECT_TRUE(is_hex) << "non-hex char in fingerprint: " << c;
    }
}

// ============================================================================
// 3. STREAM Copy 结果映射到 memory curve
// ============================================================================

TEST(CpuProfileTest, CopyResultMapsToMemoryCurve) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_copy_result(4 * 1024, 100.0));
    results.push_back(make_copy_result(64 * 1024, 1500.0));
    results.push_back(make_copy_result(4 * 1024 * 1024, 100000.0));

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);
    const DeviceProfile* cpu = hp.find_device(kHwCpuDeviceId);
    ASSERT_NE(cpu, nullptr);

    // memory curve 应包含 {MainMem, Host, copy}
    auto it = cpu->memory.find({MemoryLevel::MainMem, MemoryResidency::Host, "copy"});
    ASSERT_NE(it, cpu->memory.end()) << "memory[MainMem:Host:copy] curve missing";
    EXPECT_GE(it->second.points.size(), 1u);
    // 第一个点应是 4KB
    EXPECT_EQ(it->second.points[0].size, 4u * 1024u);
}

// ============================================================================
// 4. AXPY 结果映射到 arithmetic curve
// ============================================================================

TEST(CpuProfileTest, AxpyResultMapsToArithmeticCurve) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_axpy_result(1024, 500.0));
    results.push_back(make_axpy_result(65536, 30000.0));

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);
    const DeviceProfile* cpu = hp.find_device(kHwCpuDeviceId);
    ASSERT_NE(cpu, nullptr);

    // arithmetic curve 应包含 {Fp32, "add:baseline"}
    auto it = cpu->arithmetic.find({HwPrecision::Fp32, "add:baseline"});
    ASSERT_NE(it, cpu->arithmetic.end()) << "arithmetic[fp32:add:baseline] curve missing";
    EXPECT_GE(it->second.points.size(), 1u);
}

// ============================================================================
// 5. Dot 结果映射到 reduction curve
// ============================================================================

TEST(CpuProfileTest, DotResultMapsToReductionCurve) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_dot_result(1024, 200.0));
    results.push_back(make_dot_result(65536, 12000.0));

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);
    const DeviceProfile* cpu = hp.find_device(kHwCpuDeviceId);
    ASSERT_NE(cpu, nullptr);

    // reduction curve 应包含 {"dot", Fp32}
    auto it = cpu->reduction.find({"dot", HwPrecision::Fp32});
    ASSERT_NE(it, cpu->reduction.end()) << "reduction[dot:fp32] curve missing";
    EXPECT_GE(it->second.points.size(), 1u);
}

// ============================================================================
// 6. JSON 序列化包含必需字段
// ============================================================================

TEST(CpuProfileTest, SerializeContainsRequiredFields) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_copy_result(4 * 1024, 100.0));

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);
    std::string json = ProfileGenerator::serialize_hardware_profile(hp);

    EXPECT_TRUE(json_contains(json, "\"schema_version\""));
    EXPECT_TRUE(json_contains(json, "\"generated_at\""));
    EXPECT_TRUE(json_contains(json, "\"profile_kind\""));
    EXPECT_TRUE(json_contains(json, "\"fingerprint_sha256\""));
    EXPECT_TRUE(json_contains(json, "\"devices\""));
    EXPECT_TRUE(json_contains(json, "\"device_id\":0"));
    EXPECT_TRUE(json_contains(json, "\"kind\":\"cpu\""));
    // memory curve 应被序列化
    EXPECT_TRUE(json_contains(json, "\"memory\""));
}

// ============================================================================
// 7. write_hardware_profile_to_file 写入文件并可读回
// ============================================================================

TEST(CpuProfileTest, WriteAndReadbackFile) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_copy_result(4 * 1024, 100.0));
    results.push_back(make_axpy_result(1024, 500.0));
    results.push_back(make_dot_result(1024, 200.0));

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);

    std::string path = temp_file_path("json");
    bool ok = ProfileGenerator::write_hardware_profile_to_file(path, hp);
    ASSERT_TRUE(ok) << "write to file failed: " << path;

    // 读回内容
    std::ifstream f(path);
    ASSERT_TRUE(f.is_open()) << "cannot open file for reading: " << path;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    f.close();

    // 内容应包含必需字段
    EXPECT_TRUE(json_contains(content, "\"schema_version\""));
    EXPECT_TRUE(json_contains(content, "\"fingerprint_sha256\""));
    EXPECT_TRUE(json_contains(content, "\"devices\""));
    EXPECT_TRUE(json_contains(content, "\"memory\""));
    EXPECT_TRUE(json_contains(content, "\"arithmetic\""));
    EXPECT_TRUE(json_contains(content, "\"reduction\""));

    // 清理临时文件
    std::remove(path.c_str());
}

// ============================================================================
// 8. DeviceProfile 查询接口：predict_memory / predict_arithmetic / predict_reduction
// ============================================================================

TEST(CpuProfileTest, DeviceProfilePredictionApis) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_copy_result(4 * 1024, 100.0));        // 4KB → 100ns
    results.push_back(make_copy_result(64 * 1024, 1500.0));     // 64KB → 1500ns

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);
    const DeviceProfile* cpu = hp.find_device(kHwCpuDeviceId);
    ASSERT_NE(cpu, nullptr);

    // predict_memory：4KB 应接近 100ns
    double pred_4k = cpu->predict_memory(MemoryLevel::MainMem, MemoryResidency::Host,
                                         "copy", 4 * 1024);
    EXPECT_NEAR(pred_4k, 100.0, 5.0);

    // 8KB 应在 100 和 1500 之间线性插值
    double pred_8k = cpu->predict_memory(MemoryLevel::MainMem, MemoryResidency::Host,
                                         "copy", 8 * 1024);
    EXPECT_GT(pred_8k, 100.0);
    EXPECT_LT(pred_8k, 1500.0);
}

// ============================================================================
// 9. 多 benchmark 结果混合：CPU 设备仍有 default overhead
// ============================================================================

TEST(CpuProfileTest, DefaultOverheadsPresent) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> results;
    results.push_back(make_copy_result(4 * 1024, 100.0));

    HardwareProfile hp = gen.generate_hardware_profile(results, ProfileKind::Standard);
    const DeviceProfile* cpu = hp.find_device(kHwCpuDeviceId);
    ASSERT_NE(cpu, nullptr);

    // 默认 overhead 应包含 submit/launch/event/alloc/merge
    EXPECT_NE(cpu->get_overhead("submit"), nullptr);
    EXPECT_NE(cpu->get_overhead("launch"), nullptr);
    EXPECT_NE(cpu->get_overhead("event"), nullptr);
    EXPECT_NE(cpu->get_overhead("alloc"), nullptr);
    EXPECT_NE(cpu->get_overhead("merge"), nullptr);

    // CPU submit median 应 < GPU（CPU ~1100ns, GPU ~8500ns）
    const FixedOverhead* submit = cpu->get_overhead("submit");
    EXPECT_GT(submit->median_ns, 0.0);
    EXPECT_LT(submit->median_ns, 5000.0);  // CPU 应远小于 5us
}

// ============================================================================
// 10. has_gpu() 在纯 CPU 画像下为 false
// ============================================================================

TEST(CpuProfileTest, NoGpuInCpuOnlyProfile) {
    ProfileGenerator gen;
    std::vector<KernelBenchmarkResult> empty;
    HardwareProfile hp = gen.generate_hardware_profile(empty, ProfileKind::Standard);

    EXPECT_FALSE(hp.has_gpu());
    EXPECT_TRUE(hp.gpu_device_ids().empty());
    EXPECT_NE(hp.cpu_device(), nullptr);
}
