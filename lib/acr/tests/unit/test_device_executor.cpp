// lib/acr/tests/unit/test_device_executor.cpp — DeviceExecutor 单元测试（23 号计划 §3）
//
// 验收：
//   - CpuExecutor 通过 KernelRegistry CPU launcher 真实执行 KernelInvocation；
//   - SubmitHandle 记录真实 device/items/bytes/duration；
//   - 未注册 OperationId → Rejected（调用方回退并如实报告，不伪装 GPU）；
//   - supports() 与注册表一致；ExecutorRegistry 管理可用 executor。
#include <gtest/gtest.h>

#include "device_executor.hpp"

#include <string>
#include <vector>

using namespace astro::compute;
using namespace astro::compute::scheduler;

namespace {

// CPU AXPY launcher（与 test_kernel_registry.cpp 相同的语义）
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

KernelInvocation make_axpy_invocation(std::vector<float>& x,
                                      std::vector<float>& y) {
    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = WorkDomain{0, x.size()};
    inv.buffers.add(0, y.data(), y.size());
    inv.buffers.add(1, x.data(), x.size());
    append_scalar(inv.scalars, 2.0f);
    inv.traits.task_class = TaskClass::elementwise;
    inv.traits.bytes_read_per_item = sizeof(float);
    inv.traits.bytes_written_per_item = sizeof(float);
    return inv;
}

KernelInvocation make_unregistered_invocation(std::vector<float>& x,
                                              std::vector<float>& y) {
    KernelInvocation inv = make_axpy_invocation(x, y);
    inv.id = "kernel.not_registered";
    return inv;
}

} // anonymous namespace

// ============================================================================
// 1. CpuExecutor 通过注册表真实执行并回填统计
// ============================================================================
TEST(DeviceExecutor, CpuExecutorExecutesRegisteredKernel) {
    KernelRegistry registry;
    KernelRegistration reg;
    reg.id = "kernel.axpy";
    reg.args.buffer_count = 2;
    reg.args.scalar_bytes = sizeof(float);
    reg.cpu = &cpu_axpy_launcher;
    ASSERT_TRUE(registry.register_kernel(reg));

    CpuExecutor exec("cpu", 1024, 64, &registry);
    EXPECT_EQ(exec.id(), kHwCpuDeviceId);
    EXPECT_EQ(exec.device_id(), "cpu");
    EXPECT_EQ(exec.backend_type(), "cpu");
    EXPECT_TRUE(exec.available());
    EXPECT_TRUE(exec.supports("kernel.axpy"));
    EXPECT_FALSE(exec.supports("kernel.not_registered"));

    constexpr std::size_t kN = 64;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);
    KernelInvocation inv = make_axpy_invocation(x, y);
    WorkToken token;
    token.id = 0;
    token.begin = 0;
    token.end = kN;
    token.claimant = kHwCpuDeviceId;
    token.attempt = 1;

    SubmitHandle h = exec.submit(token, inv);
    EXPECT_EQ(h.status, SubmitStatus::Ok);
    EXPECT_EQ(h.device, kHwCpuDeviceId);
    EXPECT_EQ(h.items_done, kN);
    EXPECT_EQ(h.bytes_done, kN * 2 * sizeof(float));
    EXPECT_GE(h.elapsed_ns, 0u);
    EXPECT_FALSE(h.fallback);
    EXPECT_EQ(h.op_id, "kernel.axpy");
    EXPECT_EQ(h.attempt, 1u);

    for (std::size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(y[i], 4.0f);  // 2*1 + 2
    }
}

// ============================================================================
// 2. 未注册 OperationId → Rejected（缺失设备 launcher 回退并如实报告）
// ============================================================================
TEST(DeviceExecutor, UnregisteredOperationRejected) {
    KernelRegistry registry;
    KernelRegistration reg;
    reg.id = "kernel.axpy";
    reg.cpu = &cpu_axpy_launcher;
    ASSERT_TRUE(registry.register_kernel(reg));

    CpuExecutor exec("cpu", 1024, 64, &registry);
    std::vector<float> x(16, 1.0f);
    std::vector<float> y(16, 2.0f);
    KernelInvocation inv = make_unregistered_invocation(x, y);
    WorkToken token;
    token.id = 0;
    token.begin = 0;
    token.end = 16;
    token.claimant = kHwCpuDeviceId;
    token.attempt = 1;

    SubmitHandle h = exec.submit(token, inv);
    EXPECT_EQ(h.status, SubmitStatus::Rejected);
    EXPECT_FALSE(h.error.empty());
    // 不得伪装成功：items_done 必须为 0
    EXPECT_EQ(h.items_done, 0u);
}

// ============================================================================
// 3. 不可用 executor 拒绝提交
// ============================================================================
TEST(DeviceExecutor, UnavailableExecutorRejects) {
    KernelRegistry registry;
    KernelRegistration reg;
    reg.id = "kernel.axpy";
    reg.cpu = &cpu_axpy_launcher;
    ASSERT_TRUE(registry.register_kernel(reg));

    CpuExecutor exec("cpu", 1024, 64, &registry);
    exec.set_available(false);
    EXPECT_FALSE(exec.available());

    std::vector<float> x(16, 1.0f);
    std::vector<float> y(16, 2.0f);
    KernelInvocation inv = make_axpy_invocation(x, y);
    WorkToken token;
    token.id = 0;
    token.begin = 0;
    token.end = 16;
    token.claimant = kHwCpuDeviceId;
    token.attempt = 1;

    SubmitHandle h = exec.submit(token, inv);
    EXPECT_EQ(h.status, SubmitStatus::Rejected);
    EXPECT_EQ(h.items_done, 0u);
}

// ============================================================================
// 4. ExecutorRegistry 管理可用 executor
// ============================================================================
TEST(DeviceExecutor, RegistryManagesExecutors) {
    ExecutorRegistry reg = ExecutorRegistry::create_cpu_only();
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_NE(reg.find("cpu"), nullptr);
    EXPECT_EQ(reg.find("cuda:0"), nullptr);
    auto available = reg.available_executors();
    ASSERT_EQ(available.size(), 1u);
    EXPECT_EQ(available[0]->backend_type(), "cpu");
}

// ============================================================================
// 5. kernel 异常 → Failed（真实失败回传）
// ============================================================================
TEST(DeviceExecutor, KernelExceptionReportedAsFailed) {
    KernelRegistry registry;
    KernelRegistration reg;
    reg.id = "kernel.boom";
    reg.cpu = +[](const KernelInvocation&, void*) {
        throw std::runtime_error("boom");
    };
    ASSERT_TRUE(registry.register_kernel(reg));

    CpuExecutor exec("cpu", 1024, 64, &registry);
    KernelInvocation inv;
    inv.id = "kernel.boom";
    inv.domain = WorkDomain{0, 8};
    WorkToken token;
    token.id = 0;
    token.begin = 0;
    token.end = 8;
    token.claimant = kHwCpuDeviceId;
    token.attempt = 1;

    SubmitHandle h = exec.submit(token, inv);
    EXPECT_EQ(h.status, SubmitStatus::Failed);
    EXPECT_FALSE(h.error.empty());
}
