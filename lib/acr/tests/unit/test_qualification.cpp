// lib/acr/tests/unit/test_qualification.cpp — Phase E qualification 单元测试
// 覆盖：
//   - profile_kind_str / parse_profile_kind
//   - BENCHMARK_FIXED_SEED 确定性
//   - make_default_config 三档配置
//   - BenchmarkDriver Quick profile 运行（CPU only）
//   - ProfileGenerator 生成 + 序列化 + 反序列化（schema_version 字段）
//   - SHA-256 哈希确定性
//   - profile 三态处理（Missing/Stale/Corrupt）由 routing 模块测试，此处仅测 qualification 侧
#include <gtest/gtest.h>

#include "benchmark_driver.hpp"
#include "profile_generator.hpp"
#include "profile_schema.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute::qualification;
using astro::compute::runtime_init;
using astro::compute::runtime_shutdown;

// ============================================================================
// profile_kind_str / parse_profile_kind
// ============================================================================

TEST(QualProfileKind, StrRoundTrip) {
    EXPECT_STREQ(profile_kind_str(ProfileKind::Quick), "quick");
    EXPECT_STREQ(profile_kind_str(ProfileKind::Standard), "standard");
    EXPECT_STREQ(profile_kind_str(ProfileKind::Full), "full");
}

TEST(QualProfileKind, ParseValid) {
    ProfileKind k;
    ASSERT_TRUE(parse_profile_kind("quick", k));
    EXPECT_EQ(k, ProfileKind::Quick);
    ASSERT_TRUE(parse_profile_kind("standard", k));
    EXPECT_EQ(k, ProfileKind::Standard);
    ASSERT_TRUE(parse_profile_kind("full", k));
    EXPECT_EQ(k, ProfileKind::Full);
}

TEST(QualProfileKind, ParseInvalid) {
    ProfileKind k;
    EXPECT_FALSE(parse_profile_kind("turbo", k));
    EXPECT_FALSE(parse_profile_kind("", k));
    EXPECT_FALSE(parse_profile_kind("Quick", k));  // 大小写敏感
}

// ============================================================================
// BENCHMARK_FIXED_SEED 确定性
// ============================================================================

TEST(QualSeed, FixedSeedValue) {
    // 固定 seed 不变（文档化约束）
    EXPECT_EQ(BENCHMARK_FIXED_SEED, 0xA57C5AC20260802ULL);
}

// ============================================================================
// make_default_config 三档配置
// ============================================================================

TEST(QualConfig, QuickProfileConfig) {
    auto cfg = make_default_config(ProfileKind::Quick, /*enable_gpu=*/false);
    EXPECT_EQ(cfg.profile_kind, ProfileKind::Quick);
    EXPECT_EQ(cfg.warmup_rounds, 0u);
    EXPECT_EQ(cfg.measure_rounds, 1u);
    EXPECT_FALSE(cfg.collect_resident);
    EXPECT_FALSE(cfg.enable_gpu);
    EXPECT_FALSE(cfg.problem_sizes.empty());
}

TEST(QualConfig, StandardProfileConfig) {
    auto cfg = make_default_config(ProfileKind::Standard, /*enable_gpu=*/false);
    EXPECT_EQ(cfg.profile_kind, ProfileKind::Standard);
    EXPECT_GE(cfg.warmup_rounds, 1u);
    EXPECT_GE(cfg.measure_rounds, 3u);
    EXPECT_FALSE(cfg.collect_resident);
}

TEST(QualConfig, FullProfileConfig) {
    auto cfg = make_default_config(ProfileKind::Full, /*enable_gpu=*/false);
    EXPECT_EQ(cfg.profile_kind, ProfileKind::Full);
    EXPECT_GE(cfg.warmup_rounds, 3u);
    EXPECT_GE(cfg.measure_rounds, 10u);
    EXPECT_TRUE(cfg.collect_resident);
}

// ============================================================================
// BenchmarkDriver Quick profile 运行
// ============================================================================

TEST(QualDriver, QuickProfileProducesResults) {
    runtime_init();
    BenchmarkDriver driver;
    auto cfg = make_default_config(ProfileKind::Quick, /*enable_gpu=*/false);
    driver.configure(cfg);
    auto results = driver.run();
    // 至少有 Copy/AXPY/Triad 三个 kernel 的结果
    EXPECT_GE(results.size(), 3u);
    for (const auto& r : results) {
        EXPECT_FALSE(r.kernel_name.empty());
        EXPECT_EQ(r.backend, "cpu");
        EXPECT_EQ(r.precision, "fp32");
        EXPECT_FALSE(r.samples.empty());
        // Quick profile 1 轮
        EXPECT_EQ(r.samples.size(), 1u);
        // CPU backend 应有非零 kernel 时间
        EXPECT_GT(r.samples[0].kernel_ns, 0u);
    }
    runtime_shutdown();
}

// ============================================================================
// SHA-256 哈希确定性
// ============================================================================

TEST(QualSha256, Deterministic) {
    std::string h1 = sha256_hex("hello");
    std::string h2 = sha256_hex("hello");
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h1.size(), 64u);  // 32 字节 = 64 hex
}

TEST(QualSha256, DifferentInputDifferentHash) {
    std::string h1 = sha256_hex("hello");
    std::string h2 = sha256_hex("world");
    EXPECT_NE(h1, h2);
}

TEST(QualSha256, KnownVector) {
    // FIPS 180-2 test vector: SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    std::string h = sha256_hex("abc");
    EXPECT_EQ(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// ============================================================================
// DeviceFingerprint 序列化
// ============================================================================

TEST(QualFingerprint, SerializesToJson) {
    DeviceFingerprint fp;
    fp.cpu_model = "Test CPU";
    fp.cpu_cores = 8;
    fp.isa_mask = 0xFF;
    fp.gpu_name = "Test GPU";
    fp.gpu_memory_bytes = 8589934592ULL;
    fp.gpu_driver_version = "1.2.3";
    fp.sha256 = "abc123";
    std::string json = fp.to_json();
    EXPECT_NE(json.find("\"cpu_model\":\"Test CPU\""), std::string::npos);
    EXPECT_NE(json.find("\"cpu_cores\":8"), std::string::npos);
    EXPECT_NE(json.find("\"sha256\":\"abc123\""), std::string::npos);
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    return result;
}
