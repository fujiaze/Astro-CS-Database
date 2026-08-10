// lib/acr/tests/unit/test_kernel_registry.cpp — KernelRegistry 单元测试
//
// 23 号计划 §1 验收：
//   - host callback（函数指针/lambda）不得被标记为 GPU-capable；
//   - KernelRegistry 缺少设备 launcher 时必须回退并如实报告；
//   - OperationId + KernelInvocation 携带 buffer/scalar 绑定；
//   - 重复注册、并发查找安全。
#include <gtest/gtest.h>

#include "astro/compute/kernel_registry.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace astro::compute;

namespace {

// CPU AXPY launcher：y[a..b) = a*x[a..b) + y[a..b)
void cpu_axpy_launcher(const KernelInvocation& inv, void* /*user_data*/) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    ASSERT_NE(yb, nullptr);
    ASSERT_NE(xb, nullptr);
    auto a = read_scalar<float>(inv.scalars, 0);
    ASSERT_TRUE(a.has_value());
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        y[i] = *a * x[i] + y[i];
    }
}

// CUDA launcher（本测试只验证注册/查找；真实设备执行由 cuda backend 提供）
void cuda_axpy_launcher(const KernelInvocation&, void*) {
    // 真实实现位于 backends/cuda（ACR_BUILD_CUDA）；此处仅作注册标记
}

KernelRegistration make_axpy_registration(bool with_cuda) {
    KernelRegistration reg;
    reg.id = "kernel.axpy";
    reg.args.buffer_count = 2;
    reg.args.scalar_bytes = sizeof(float);
    reg.cpu = &cpu_axpy_launcher;
    if (with_cuda) {
        reg.cuda = &cuda_axpy_launcher;
    }
    return reg;
}

} // anonymous namespace

// ============================================================================
// 1. host callback 不得被标记为 GPU-capable
// ============================================================================
TEST(KernelRegistry, HostCallbackNotGpuCapable) {
    KernelRegistry reg;
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(/*with_cuda=*/false)));

    EXPECT_TRUE(reg.supports("kernel.axpy", "cpu"));
    EXPECT_FALSE(reg.supports("kernel.axpy", "cuda"));
    EXPECT_FALSE(reg.supports("kernel.axpy", "cuda:0"));
    EXPECT_FALSE(reg.supports("kernel.axpy", "hip"));

    const KernelRegistration* r = reg.find("kernel.axpy");
    ASSERT_NE(r, nullptr);
    EXPECT_NE(r->cpu, nullptr);
    EXPECT_FALSE(r->cuda.has_value());
}

// ============================================================================
// 2. 注册 CPU+CUDA launcher：两种 backend 均支持
// ============================================================================
TEST(KernelRegistry, CudaLauncherRegistered) {
    KernelRegistry reg;
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(/*with_cuda=*/true)));

    EXPECT_TRUE(reg.supports("kernel.axpy", "cpu"));
    EXPECT_TRUE(reg.supports("kernel.axpy", "cuda"));
    EXPECT_TRUE(reg.supports("kernel.axpy", "cuda:0"));
    const KernelRegistration* r = reg.find("kernel.axpy");
    ASSERT_NE(r, nullptr);
    ASSERT_TRUE(r->cuda.has_value());
    EXPECT_EQ(r->args.buffer_count, 2u);
    EXPECT_EQ(r->args.scalar_bytes, sizeof(float));
}

// ============================================================================
// 3. 缺少设备 launcher：supports=false（调用方必须回退 CPU 并如实报告）
// ============================================================================
TEST(KernelRegistry, MissingDeviceLauncherFallsBack) {
    KernelRegistry reg;
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(/*with_cuda=*/false)));

    // 设备 launcher 缺失 → supports(cuda)=false → Dispatcher 不得把任务交给 GPU executor
    EXPECT_FALSE(reg.supports("kernel.axpy", "cuda"));
    // 回退路径：CPU launcher 仍可用，且报告必须说明未使用 GPU
    EXPECT_TRUE(reg.supports("kernel.axpy", "cpu"));
}

// ============================================================================
// 4. 重复注册被拒绝（保留首个）
// ============================================================================
TEST(KernelRegistry, DuplicateRegistrationRejected) {
    KernelRegistry reg;
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(false)));
    EXPECT_FALSE(reg.register_kernel(make_axpy_registration(false)));
    EXPECT_EQ(reg.size(), 1u);
}

// ============================================================================
// 5. CPU launcher 端到端执行（host 正确性）
// ============================================================================
TEST(KernelRegistry, CpuLauncherExecutesInvocation) {
    KernelRegistry reg;
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(false)));

    constexpr std::size_t kN = 64;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, kN};
    inv.buffers.add(0, y.data(), kN);
    inv.buffers.add(1, x.data(), kN);
    append_scalar(inv.scalars, 3.0f);

    const KernelRegistration* r = reg.find(inv.id);
    ASSERT_NE(r, nullptr);
    r->cpu(inv, nullptr);

    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 5.0f);  // 3*1 + 2
    }
}

// ============================================================================
// 6. 空 id / 无 CPU launcher 注册失败
// ============================================================================
TEST(KernelRegistry, InvalidRegistrationRejected) {
    KernelRegistry reg;
    KernelRegistration no_cpu;
    no_cpu.id = "kernel.copy";
    no_cpu.cpu = nullptr;
    EXPECT_FALSE(reg.register_kernel(no_cpu));

    KernelRegistration empty_id;
    empty_id.cpu = &cpu_axpy_launcher;
    EXPECT_FALSE(reg.register_kernel(empty_id));
    EXPECT_TRUE(reg.empty());
}

// ============================================================================
// 7. 并发注册/查找安全
// ============================================================================
TEST(KernelRegistry, ConcurrentAccessSafe) {
    KernelRegistry reg;
    std::atomic<bool> stop{false};
    std::atomic<bool> go{false};
    std::atomic<int> ready{0};
    std::atomic<std::size_t> found{0};

    // 先注册（保证 find 非空），再启动并发读者
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(false)));

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            ready.fetch_add(1, std::memory_order_release);
            while (!stop.load(std::memory_order_relaxed)) {
                if (reg.find("kernel.axpy") != nullptr) {
                    found.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // 并发重复注册（返回 false，但必须线程安全、不破坏已有注册）
    go.store(true, std::memory_order_release);
    // 等所有读者进入 find 循环，保证并发读与注册真正重叠
    while (ready.load(std::memory_order_acquire) < 4) {
        std::this_thread::yield();
    }
    for (int i = 0; i < 100; ++i) {
        KernelRegistration reg_axpy = make_axpy_registration(false);
        reg_axpy.id = "kernel.axpy";
        reg.register_kernel(reg_axpy);
    }
    stop.store(true, std::memory_order_relaxed);
    for (auto& th : readers) th.join();

    EXPECT_GT(found.load(), 0u);
    EXPECT_NE(reg.find("kernel.axpy"), nullptr);
}

// ============================================================================
// 8. 全局默认注册表可达
// ============================================================================
TEST(KernelRegistry, GlobalRegistryAccessible) {
    auto& reg = global_kernel_registry();
    // 默认未注册任何业务 kernel；注册后可见
    KernelRegistration r = make_axpy_registration(false);
    r.id = "kernel.global_probe";
    EXPECT_TRUE(reg.register_kernel(r));
    EXPECT_NE(reg.find("kernel.global_probe"), nullptr);
    EXPECT_TRUE(reg.supports("kernel.global_probe", "cpu"));
}

// ============================================================================
// 9. Invocation 契约校验（24 号计划 §5.2）
// ============================================================================
TEST(KernelRegistry, InvocationContractValidation) {
    KernelRegistry reg;
    ASSERT_TRUE(reg.register_kernel(make_axpy_registration(false)));
    const KernelRegistration* r = reg.find("kernel.axpy");
    ASSERT_NE(r, nullptr);

    constexpr std::size_t kN = 16;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);

    KernelInvocation ok_inv;
    ok_inv.id = "kernel.axpy";
    ok_inv.domain = WorkDomain{0, kN};
    ok_inv.buffers.add(0, y.data(), kN);
    ok_inv.buffers.add(1, x.data(), kN);
    append_scalar(ok_inv.scalars, 2.0f);
    EXPECT_TRUE(validate_invocation(*r, ok_inv, "cpu").empty());

    // buffer 数错误
    KernelInvocation bad_buffers = ok_inv;
    bad_buffers.buffers.bindings.pop_back();
    EXPECT_FALSE(validate_invocation(*r, bad_buffers, "cpu").empty());

    // scalar 字节错误（缺 alpha）
    KernelInvocation bad_scalars = ok_inv;
    bad_scalars.scalars.bytes.clear();
    EXPECT_FALSE(validate_invocation(*r, bad_scalars, "cpu").empty());

    // 空 domain
    KernelInvocation bad_domain = ok_inv;
    bad_domain.domain = WorkDomain{5, 5};
    EXPECT_FALSE(validate_invocation(*r, bad_domain, "cpu").empty());

    // NumericPolicy 与注册不一致（声明 FP64 累加而注册 FP32）
    KernelInvocation bad_numeric = ok_inv;
    bad_numeric.traits.numeric.accumulator = NumericPolicy::Accumulator::fp64;
    EXPECT_FALSE(validate_invocation(*r, bad_numeric, "cpu").empty());

    // cuda launcher 缺失 → cuda backend 校验失败
    EXPECT_FALSE(validate_invocation(*r, ok_inv, "cuda").empty());
}

// ============================================================================
// 10. ScalarArgBlob 对齐读取（memcpy，无 reinterpret_cast）
// ============================================================================
TEST(KernelRegistry, ScalarReadUnalignedSafe) {
    ScalarArgBlob blob;
    // 先写 1 字节填充，再写 double，验证偏移非对齐也能安全读取
    blob.bytes.push_back(0xAB);
    const double value = 3.141592653589793;
    append_scalar(blob, value);
    auto out = read_scalar<double>(blob, 1);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(*out, value);
    // 越界返回 nullopt
    EXPECT_FALSE(read_scalar<double>(blob, 100).has_value());
}
