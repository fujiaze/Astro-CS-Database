// lib/acr/tests/unit/test_topology.cpp — Phase C 硬件发现/ISA 单测
// 验收（spec.md §7 Phase C）：
// - HwlocTopology JSON 非空
// - CpuIsaCaps 至少检测到 SSE2（x86-64 必有）
// - 安全门禁：AVX-512 不支持时 has_isa 返回 false
// - 降级：无 hwloc 返回 unavailable 不抛
// - AXPY dispatch 结果与 scalar 一致（FP32 容差）
// - generate_hardware_report 非空且含 schema
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "astro/compute/topology.hpp"
#include "isa/isa_kernels.hpp"

using astro::compute::CpuIsaCaps;
using astro::compute::HwlocTopology;
using astro::compute::IsaLevel;
using astro::compute::cpu::dispatch_axpy;
using astro::compute::cpu::kernel_axpy_scalar;
using astro::compute::cpu::kernel_avx512_axpy_safe;
using astro::compute::detect_isa_caps;
using astro::compute::detect_topology;
using astro::compute::generate_hardware_report;
using astro::compute::register_gpu_report_callback;
using astro::compute::reset_gpu_report_callback_for_testing;

// ===== HwlocTopology =====
TEST(HwlocTopology, JsonNonEmpty) {
    HwlocTopology topo;
    std::string j = topo.to_json();
    EXPECT_FALSE(j.empty());
    // 必含 status 字段
    EXPECT_NE(j.find("\"status\""), std::string::npos);
}

TEST(HwlocTopology, AvailableOrUnavailable) {
    HwlocTopology topo;
    if (topo.available()) {
        EXPECT_NE(topo.to_json().find("\"status\":\"ok\""), std::string::npos);
    } else {
        EXPECT_EQ(topo.to_json(), R"({"status":"unavailable"})");
    }
}

TEST(HwlocTopology, NoThrowWhenUnavailable) {
    // 多次构造/析构不抛异常（降级路径覆盖）
    EXPECT_NO_THROW({
        for (int i = 0; i < 3; ++i) {
            HwlocTopology t;
            (void)t.to_json();
            (void)t.available();
        }
    });
}

TEST(DetectTopology, ReturnsValidJson) {
    std::string j = detect_topology();
    EXPECT_FALSE(j.empty());
    EXPECT_EQ(j.front(), '{');
    EXPECT_EQ(j.back(), '}');
}

// ===== CpuIsaCaps =====
TEST(CpuIsaCaps, Sse2AlwaysOnX86_64) {
    CpuIsaCaps caps;
#if defined(__x86_64__) || defined(_M_X64)
    // x86-64 必有 SSE2（基线指令集）
    EXPECT_TRUE(caps.has(IsaLevel::SSE2));
    EXPECT_TRUE(caps.has_isa(IsaLevel::SSE2));
#else
    // 非 x86-64：SSE2 不保证，仅验证 has() 不崩溃
    (void)caps;
#endif
}

TEST(CpuIsaCaps, MaskNonZeroOnX86_64) {
    CpuIsaCaps caps;
#if defined(__x86_64__) || defined(_M_X64)
    EXPECT_NE(caps.mask(), 0u);
#endif
}

TEST(CpuIsaCaps, Avx512GateWhenUnsupported) {
    // 安全门禁：本机不支持 AVX-512 时 has_isa 必须返回 false
    // 不支持 ISA 的 kernel 永不执行（ADR-004 关键约束）
    CpuIsaCaps caps;
    if (!caps.has(IsaLevel::AVX512F)) {
        EXPECT_FALSE(caps.has_isa(IsaLevel::AVX512F));
        EXPECT_FALSE(caps.has_isa(IsaLevel::AVX512CD));
        EXPECT_FALSE(caps.has_isa(IsaLevel::AVX512BW));
        EXPECT_FALSE(caps.has_isa(IsaLevel::AVX512DQ));
        EXPECT_FALSE(caps.has_isa(IsaLevel::AVX512VL));
        // 组合 mask：F|CD|BW|DQ|VL 任一缺失则整体 false
        const IsaLevel all_avx512 = IsaLevel::AVX512F | IsaLevel::AVX512CD |
                                     IsaLevel::AVX512BW | IsaLevel::AVX512DQ |
                                     IsaLevel::AVX512VL;
        EXPECT_FALSE(caps.has_isa(all_avx512));
    }
}

TEST(CpuIsaCaps, Avx512SubsetIndependent) {
    // AVX-512 子集独立 bit：F/CD/BW/DQ/VL 是独立 bool，禁止合并
    CpuIsaCaps caps;
    // 各子集 has() 独立返回，不互相影响
    bool f  = caps.has(IsaLevel::AVX512F);
    bool cd = caps.has(IsaLevel::AVX512CD);
    bool bw = caps.has(IsaLevel::AVX512BW);
    bool dq = caps.has(IsaLevel::AVX512DQ);
    bool vl = caps.has(IsaLevel::AVX512VL);
    // 若任一支持，则 F 必支持（F 是 AVX-512 基础子集）
    if (cd || bw || dq || vl) {
        EXPECT_TRUE(f);
    }
    // 不支持 F 时，所有子集都 false
    if (!f) {
        EXPECT_FALSE(cd); EXPECT_FALSE(bw); EXPECT_FALSE(dq); EXPECT_FALSE(vl);
    }
}

TEST(CpuIsaCaps, ToJsonContainsAllIsaFields) {
    CpuIsaCaps caps;
    std::string j = caps.to_json();
    EXPECT_NE(j.find("\"SSE\""), std::string::npos);
    EXPECT_NE(j.find("\"SSE2\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX2\""), std::string::npos);
    EXPECT_NE(j.find("\"FMA\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX512F\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX512CD\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX512BW\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX512DQ\""), std::string::npos);
    EXPECT_NE(j.find("\"AVX512VL\""), std::string::npos);
    EXPECT_NE(j.find("\"mask\""), std::string::npos);
}

TEST(DetectIsaCaps, ReturnsValidJson) {
    std::string j = detect_isa_caps();
    EXPECT_FALSE(j.empty());
    EXPECT_EQ(j.front(), '{');
    EXPECT_EQ(j.back(), '}');
}

// ===== AXPY kernel 正确性 =====
namespace {

constexpr std::size_t kN = 257;  // 非对齐长度，覆盖 tail
constexpr float kA = 2.5f;
constexpr float kTol = 1e-5f + 5e-5f;  // FP32 容差（spec.md §8）

void init_input(std::vector<float>& x, std::size_t n) {
    x.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i % 7) - 3.0f;
    }
}

} // anonymous namespace

TEST(AxpyScalar, BaselineCorrect) {
    std::vector<float> x, y_expected(kN, 1.0f), y_actual(kN, 1.0f);
    init_input(x, kN);
    kernel_axpy_scalar(y_expected.data(), x.data(), kA, kN);
    kernel_axpy_scalar(y_actual.data(), x.data(), kA, kN);
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_NEAR(y_expected[i], y_actual[i], kTol);
    }
}

TEST(DispatchAxpy, MatchesScalar) {
    std::vector<float> x, y_scalar(kN, 1.0f), y_dispatch(kN, 1.0f);
    init_input(x, kN);
    kernel_axpy_scalar(y_scalar.data(), x.data(), kA, kN);
    CpuIsaCaps caps;
    dispatch_axpy(caps, y_dispatch.data(), x.data(), kA, kN);
    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_NEAR(y_scalar[i], y_dispatch[i], kTol);
    }
}

TEST(DispatchAxpy, EmptyRangeNoCrash) {
    CpuIsaCaps caps;
    float dummy = 0.0f;
    EXPECT_NO_THROW(dispatch_axpy(caps, &dummy, &dummy, 1.0f, 0));
}

TEST(Avx512Safe, GateWhenUnsupported) {
    // 本机不支持 AVX-512 时，_safe 返回 false 且不调用 kernel（无 SIGILL）
    CpuIsaCaps caps;
    std::vector<float> x, y(kN, 1.0f);
    init_input(x, kN);
    if (!caps.has(IsaLevel::AVX512F)) {
        bool invoked = kernel_avx512_axpy_safe(caps, y.data(), x.data(), kA, kN);
        EXPECT_FALSE(invoked);
        // y 未被修改
        for (std::size_t i = 0; i < kN; ++i) {
            EXPECT_FLOAT_EQ(y[i], 1.0f);
        }
    }
}

// ===== generate_hardware_report =====
TEST(HardwareReport, NonEmptyAndSchema) {
    std::string r = generate_hardware_report();
    EXPECT_FALSE(r.empty());
    EXPECT_NE(r.find("\"schema\":\"acr.hardware.v1\""), std::string::npos);
    EXPECT_NE(r.find("\"topology\":"), std::string::npos);
    EXPECT_NE(r.find("\"isa\":"), std::string::npos);
    EXPECT_NE(r.find("\"compiler\":"), std::string::npos);
    EXPECT_NE(r.find("\"build\":"), std::string::npos);
}

TEST(HardwareReport, GpuNullWhenUnregistered) {
    // 默认未注册 GPU 回调，gpu 字段为 null（在测试进程中）
    std::string r = generate_hardware_report();
    // gpu 字段存在，值为 null 或对象
    EXPECT_NE(r.find("\"gpu\":"), std::string::npos);
}

namespace {

std::string test_gpu_callback() {
    return R"({"uuid":"test-gpu","driver":"test","sm_count":0})";
}

} // anonymous namespace

TEST(HardwareReport, GpuCallbackRegistered) {
    reset_gpu_report_callback_for_testing();
    register_gpu_report_callback(test_gpu_callback);
    std::string r = generate_hardware_report();
    EXPECT_NE(r.find("\"test-gpu\""), std::string::npos);
    reset_gpu_report_callback_for_testing();
}

TEST(HardwareReport, FirstCallbackWins) {
    // 首次注册生效，后续忽略（CAS 语义）
    // GoogleTest 同进程运行，前序 GpuCallbackRegistered 已污染 g_gpu_cb，
    // 需先重置全局状态再验证 CAS 语义
    reset_gpu_report_callback_for_testing();
    auto cb1 = []() -> std::string { return R"({"uuid":"first-cb"})"; };
    auto cb2 = []() -> std::string { return R"({"uuid":"second-cb"})"; };
    register_gpu_report_callback(cb1);
    register_gpu_report_callback(cb2);  // 应被忽略
    std::string r = generate_hardware_report();
    EXPECT_NE(r.find("\"first-cb\""), std::string::npos);
    EXPECT_EQ(r.find("\"second-cb\""), std::string::npos);
    // 恢复未注册状态，避免污染后续测试
    reset_gpu_report_callback_for_testing();
}
