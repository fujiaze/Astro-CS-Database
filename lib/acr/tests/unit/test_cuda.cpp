// lib/acr/tests/unit/test_cuda.cpp — Phase D CUDA backend 单测
// 验收（spec.md §7 Phase D）：
//   - 设备枚举：至少 1 个设备（RTX 3060 Ti）
//   - cuda_parallel_for AXPY：结果与 CPU 对照一致（FP32 容差）
//   - CudaBuffer h2d/d2h round-trip 数据正确
//   - 无设备时降级（本机有设备，验证 available 语义）
//   - CUDA event 计时非负
//   - GPU 报告回调注册后 hardware_report 包含 GPU 字段
//
// 注：CPU-only 构建时本文件不编译（CMake 用 if(ACR_BUILD_CUDA) 保护）。
//     文件内双重 #ifdef ACR_BUILD_CUDA 保险。
#ifdef ACR_BUILD_CUDA

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "cuda_backend.hpp"
#include "cuda_buffer.hpp"

#include "astro/compute/topology.hpp"  // generate_hardware_report

using astro::compute::StatusCode;
using astro::compute::cuda::CudaBackend;
using astro::compute::cuda::CudaBuffer;
using astro::compute::cuda::axpy;
using astro::compute::cuda::cuda_event;
using astro::compute::generate_hardware_report;

namespace {
constexpr float kTol = 1e-4f;  // FP32 容差（spec.md §8 放宽：AXPY 累积误差）
} // anonymous namespace

// ===== 设备枚举 =====
TEST(CudaBackend, DeviceEnumeration) {
    auto& backend = CudaBackend::instance();
    StatusCode s = backend.initialize();
    if (s == StatusCode::Ok) {
        // 本机应有 RTX 3060 Ti
        EXPECT_GT(backend.device_count(), 0);
        EXPECT_TRUE(backend.available());
        EXPECT_FALSE(backend.device_info().name.empty());
        EXPECT_GT(backend.device_info().total_memory, static_cast<std::size_t>(0));
        EXPECT_GT(backend.device_info().sm_count, 0);
    } else {
        // 无设备时降级（不崩溃）
        EXPECT_FALSE(backend.available());
    }
}

TEST(CudaBackend, InitializeIdempotent) {
    auto& backend = CudaBackend::instance();
    StatusCode s1 = backend.initialize();
    StatusCode s2 = backend.initialize();
    // 幂等：多次调用结果一致
    EXPECT_EQ(s1, s2);
}

TEST(CudaBackend, StreamValidWhenAvailable) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    if (backend.available()) {
        EXPECT_NE(backend.stream(), nullptr);
    }
}

// ===== cuda_parallel_for AXPY（间接验证 cuda_parallel_for）=====
TEST(CudaAxpy, MatchesCpu) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    ASSERT_TRUE(backend.available());

    constexpr std::size_t kN = 1024;
    constexpr float kA = 2.5f;
    std::vector<float> x(kN), y(kN), y_expected(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        x[i] = static_cast<float>(i % 17) - 8.0f;
        y[i] = static_cast<float>(i % 5);
        y_expected[i] = kA * x[i] + y[i];
    }

    CudaBuffer<float> dx(kN), dy(kN);
    ASSERT_TRUE(dx.valid());
    ASSERT_TRUE(dy.valid());
    ASSERT_EQ(dx.copy_h2d(x.data(), kN, backend.stream()), StatusCode::Ok);
    ASSERT_EQ(dy.copy_h2d(y.data(), kN, backend.stream()), StatusCode::Ok);

    ASSERT_EQ(axpy(dy.data(), dx.data(), kA, kN, backend.stream()),
              StatusCode::Ok);
    ASSERT_EQ(backend.sync(), StatusCode::Ok);

    std::vector<float> y_actual(kN);
    ASSERT_EQ(dy.copy_d2h(y_actual.data(), kN, backend.stream()),
              StatusCode::Ok);

    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_NEAR(y_expected[i], y_actual[i], kTol)
            << "Mismatch at i=" << i;
    }
}

TEST(CudaAxpy, EmptyRangeNoCrash) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    float dummy = 0.0f;
    EXPECT_EQ(axpy(&dummy, &dummy, 1.0f, 0, backend.stream()),
              StatusCode::Ok);
}

TEST(CudaAxpy, NullPointerRejected) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    EXPECT_EQ(axpy(nullptr, nullptr, 1.0f, 100, backend.stream()),
              StatusCode::InvalidArgument);
}

TEST(CudaAxpy, NonAlignedSize) {
    // 非对齐长度（257），覆盖 tail 处理
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    ASSERT_TRUE(backend.available());

    constexpr std::size_t kN = 257;
    constexpr float kA = 3.14f;
    std::vector<float> x(kN), y(kN), y_expected(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        x[i] = static_cast<float>(i) * 0.1f;
        y[i] = 1.0f;
        y_expected[i] = kA * x[i] + y[i];
    }

    CudaBuffer<float> dx(kN), dy(kN);
    ASSERT_TRUE(dx.valid() && dy.valid());
    ASSERT_EQ(dx.copy_h2d(x.data(), kN, backend.stream()), StatusCode::Ok);
    ASSERT_EQ(dy.copy_h2d(y.data(), kN, backend.stream()), StatusCode::Ok);
    ASSERT_EQ(axpy(dy.data(), dx.data(), kA, kN, backend.stream()),
              StatusCode::Ok);
    ASSERT_EQ(backend.sync(), StatusCode::Ok);

    std::vector<float> y_actual(kN);
    ASSERT_EQ(dy.copy_d2h(y_actual.data(), kN, backend.stream()),
              StatusCode::Ok);

    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_NEAR(y_expected[i], y_actual[i], kTol);
    }
}

// ===== CudaBuffer round-trip =====
TEST(CudaBuffer, RoundTripInt) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);

    constexpr std::size_t kN = 256;
    std::vector<int> src(kN);
    for (std::size_t i = 0; i < kN; ++i) src[i] = static_cast<int>(i * 2);

    CudaBuffer<int> buf(kN);
    ASSERT_TRUE(buf.valid());
    ASSERT_EQ(buf.count(), kN);
    ASSERT_EQ(buf.bytes(), kN * sizeof(int));
    ASSERT_EQ(buf.copy_h2d(src.data(), kN, backend.stream()), StatusCode::Ok);

    std::vector<int> dst(kN, 0);
    ASSERT_EQ(buf.copy_d2h(dst.data(), kN, backend.stream()), StatusCode::Ok);

    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_EQ(src[i], dst[i]);
    }
}

TEST(CudaBuffer, MoveSemantics) {
    CudaBuffer<float> a(128);
    ASSERT_TRUE(a.valid());
    float* raw = a.data();

    CudaBuffer<float> b(std::move(a));
    EXPECT_TRUE(b.valid());
    EXPECT_EQ(b.data(), raw);
    EXPECT_FALSE(a.valid());  // 移走后源对象无效
    EXPECT_EQ(a.data(), nullptr);
    EXPECT_EQ(a.count(), static_cast<std::size_t>(0));
}

TEST(CudaBuffer, OutOfBoundsRejected) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    CudaBuffer<float> buf(64);
    ASSERT_TRUE(buf.valid());
    std::vector<float> big(128, 1.0f);
    EXPECT_EQ(buf.copy_h2d(big.data(), 128, backend.stream()),
              StatusCode::OutOfBounds);
}

// ===== CUDA event 计时 =====
TEST(CudaEvent, TimingNonNegative) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    ASSERT_TRUE(backend.available());

    constexpr std::size_t kN = 1 << 20;  // 1M
    CudaBuffer<float> dx(kN), dy(kN);
    std::vector<float> x(kN, 1.5f), y(kN, 2.0f);
    ASSERT_EQ(dx.copy_h2d(x.data(), kN, backend.stream()), StatusCode::Ok);
    ASSERT_EQ(dy.copy_h2d(y.data(), kN, backend.stream()), StatusCode::Ok);

    cuda_event start, end;
    start.record(backend.stream());
    ASSERT_EQ(axpy(dy.data(), dx.data(), 3.0f, kN, backend.stream()),
              StatusCode::Ok);
    end.record(backend.stream());
    end.sync();

    float ms = end.elapsed_since(start);
    EXPECT_GE(ms, 0.0f);  // 非负（极小 kernel 可能近 0）
}

// ===== 无设备降级（本机有 GPU，验证语义不崩溃）=====
TEST(CudaBackend, DegradePathNoCrash) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    if (backend.device_count() > 0) {
        SUCCEED() << "Device present (RTX 3060 Ti), degrade path skipped";
    } else {
        // 无设备时 available()=false，调用 sync 不崩溃
        EXPECT_FALSE(backend.available());
        EXPECT_EQ(backend.sync(), StatusCode::Ok);
    }
}

// ===== GPU 报告回调注册（hardware_report 含 GPU 字段）=====
TEST(CudaBackend, GpuReportCallbackRegistered) {
    auto& backend = CudaBackend::instance();
    ASSERT_EQ(backend.initialize(), StatusCode::Ok);
    ASSERT_TRUE(backend.available());

    std::string report = generate_hardware_report();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("\"gpu\":"), std::string::npos);
    // GPU 回调返回的 JSON 应包含设备名（RTX 3060 Ti）
    EXPECT_NE(report.find(backend.device_info().name), std::string::npos);
    EXPECT_NE(report.find("\"uuid\":\"GPU-"), std::string::npos);
}

#endif // ACR_BUILD_CUDA
