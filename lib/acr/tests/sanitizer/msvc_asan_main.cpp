// lib/acr/tests/sanitizer/msvc_asan_main.cpp — MSVC ASan 实际验证
//
// 背景：本机 MinGW（g++/clang）均无 ASan/UBSan 运行库（libasan/libubsan/
// clang_rt.asan 缺失），完整 ACR（oneTBB 等 MinGW ABI 依赖）无法 ASan 构建。
// 但 ACR 的不依赖 oneTBB/MinGW 库的组件——共享工作池（shared_work_pool.cpp）、
// KernelRegistry（kernel_registry.cpp）、CpuExecutor（device_executor.cpp）——
// 可用 MSVC 14.50 + /fsanitize=address 编译真实源码做 ASan 验证。
// 26 §2：CPU 利用率控制器已删除，不再纳入 ASan 覆盖。
//
// 用法：
// acr_sanitizer_msvc.exe # 运行工作池/注册表压力 + 并发测试
// acr_sanitizer_msvc.exe --uaf # 故意触发 use-after-free（ASan 应终止）
//
// 构建（证据命令）：
// vcvars64 && cl /nologo /std:c++20 /O2 /fsanitize=address /I <acr>/include \
// /I <acr>/scheduler \
// /I <acr> \
// <acr>/scheduler/shared_work_pool.cpp \
// <acr>/api/kernel_registry.cpp \
// <acr>/scheduler/device_executor.cpp \
// <acr>/utilization/system_metrics.cpp \
// <acr>/tests/sanitizer/msvc_asan_main.cpp \
// /link /out:acr_sanitizer_msvc.exe
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "scheduler/shared_work_pool.hpp"
#include "scheduler/device_executor.hpp"
#include "astro/compute/kernel_registry.hpp"

using astro::compute::KernelInvocation;
using astro::compute::kHwCpuDeviceId;
using astro::compute::scheduler::SharedWorkPool;
using astro::compute::scheduler::WorkToken;
using astro::compute::scheduler::CpuExecutor;
using astro::compute::scheduler::SubmitStatus;

namespace {

void cpu_axpy_launcher(const KernelInvocation&, void*) {}

// ===== 工作池 1000 轮高并发压力（exactly-once）=====
int run_work_pool_stress() {
    constexpr std::size_t kRounds = 1000;
    constexpr std::size_t kItems = 1024;
    constexpr std::size_t kThreads = 4;
    for (std::size_t round = 0; round < kRounds; ++round) {
        SharedWorkPool pool;
        pool.init_dynamic(0, kItems, 64, 256);
        std::vector<std::atomic<unsigned>> coverage(kItems);
        for (auto& c : coverage) c.store(0, std::memory_order_relaxed);
        std::vector<std::thread> workers;
        for (std::size_t w = 0; w < kThreads; ++w) {
            workers.emplace_back([&] {
                while (true) {
                    auto t = pool.claim_next_dynamic(kHwCpuDeviceId, 256);
                    if (!t.valid()) break;
                    for (std::size_t i = t.begin; i < t.end; ++i) {
                        coverage[i].fetch_add(1, std::memory_order_relaxed);
                    }
                    if (!pool.mark_done(t)) {
                        std::fprintf(stderr, "round %zu: mark_done failed\n", round);
                        std::exit(2);
                    }
                }
            });
        }
        for (auto& th : workers) th.join();
        if (!pool.all_done() || pool.completed_items() != kItems) {
            std::fprintf(stderr, "round %zu: all_done=%d items=%zu\n",
                         round, pool.all_done(), pool.completed_items());
            return 1;
        }
        for (std::size_t i = 0; i < kItems; ++i) {
            if (coverage[i].load() != 1u) {
                std::fprintf(stderr, "round %zu: item %zu covered %u times\n",
                             round, i, coverage[i].load());
                return 1;
            }
        }
    }
    std::printf("work_pool_stress: 1000 rounds OK (no ASan errors)\n");
    return 0;
}

// ===== KernelRegistry 并发注册/查找 =====
int run_registry_concurrent() {
    astro::compute::KernelRegistry reg;
    {
        astro::compute::KernelRegistration r;
        r.id = "kernel.axpy";
        r.cpu = &cpu_axpy_launcher;
        reg.register_kernel(r);
    }
    std::atomic<bool> stop{false};
    std::atomic<bool> go{false};
    std::atomic<int> ready{0};
    std::atomic<std::size_t> found{0};
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            ready.fetch_add(1, std::memory_order_release);
            while (!stop.load(std::memory_order_relaxed)) {
                if (reg.find("kernel.axpy") != nullptr) {
                    found.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    go.store(true, std::memory_order_release);
    while (ready.load(std::memory_order_acquire) < 4) std::this_thread::yield();
    for (int i = 0; i < 100; ++i) {
        astro::compute::KernelRegistration r;
        r.id = "kernel.axpy";
        r.cpu = &cpu_axpy_launcher;
        reg.register_kernel(r);
    }
    stop.store(true, std::memory_order_relaxed);
    for (auto& th : readers) th.join();
    if (found.load() == 0 || reg.find("kernel.axpy") == nullptr) {
        std::fprintf(stderr, "registry concurrent: found=%zu\n", found.load());
        return 1;
    }
    std::printf("registry_concurrent: OK (found=%zu, no ASan errors)\n", found.load());
    return 0;
}

// ===== CpuExecutor 真实提交 + 契约校验（24 §5）=====
int run_cpu_executor_and_contract() {
    astro::compute::KernelRegistry reg;
    {
        astro::compute::KernelRegistration r;
        r.id = "kernel.axpy";
        r.args.buffer_count = 2;
        r.args.scalar_bytes = sizeof(float);
        r.cpu = +[](const KernelInvocation& inv, void*) {
            const auto* yb = inv.buffers.find(0);
            const auto* xb = inv.buffers.find(1);
            auto a = astro::compute::read_scalar<float>(inv.scalars, 0);
            if (!yb || !xb || !a) throw std::runtime_error("bad axpy invocation");
            float* y = static_cast<float*>(yb->data);
            const float* x = static_cast<const float*>(xb->data);
            for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
                y[i] = *a * x[i] + y[i];
            }
        };
        if (!reg.register_kernel(r)) { std::fprintf(stderr, "register failed\n"); return 1; }
    }

    CpuExecutor exec("cpu", 1024, 64, &reg);
    constexpr std::size_t kN = 256;
    std::vector<float> x(kN, 1.0f);
    std::vector<float> y(kN, 2.0f);
    KernelInvocation inv;
    inv.id = "kernel.axpy";
    inv.domain = astro::compute::WorkDomain{0, kN};
    inv.buffers.add(0, y.data(), kN);
    inv.buffers.add(1, x.data(), kN);
    astro::compute::append_scalar(inv.scalars, 2.0f);

    WorkToken token;
    token.id = 0;
    token.begin = 0;
    token.end = kN;
    token.claimant = kHwCpuDeviceId;
    token.attempt = 1;

    auto h = exec.submit(token, inv);
    if (h.status != SubmitStatus::Ok) {
        std::fprintf(stderr, "cpu executor submit failed: %s\n", h.error.c_str());
        return 1;
    }
    for (std::size_t i = 0; i < kN; ++i) {
        if (y[i] != 4.0f) { std::fprintf(stderr, "axpy result wrong\n"); return 1; }
    }

    // 契约拒绝路径：buffer 数错误、scalar 缺失、numeric 不匹配 → Rejected
    KernelInvocation bad = inv;
    bad.buffers.bindings.pop_back();
    auto h1 = exec.submit(token, bad);
    if (h1.status != SubmitStatus::Rejected) {
        std::fprintf(stderr, "buffer-count contract not rejected\n");
        return 1;
    }
    KernelInvocation bad_num = inv;
    bad_num.traits.numeric.accumulator = astro::compute::NumericPolicy::Accumulator::fp64;
    auto h2 = exec.submit(token, bad_num);
    if (h2.status != SubmitStatus::Rejected) {
        std::fprintf(stderr, "numeric-policy contract not rejected\n");
        return 1;
    }
    std::printf("cpu_executor_contract: OK (submit + 2 reject paths, no ASan errors)\n");
    return 0;
}

// ===== 故意 UAF：ASan 应检测并终止进程 =====
int trigger_uaf() {
    int* p = new int(42);
    delete p;
    volatile int x = *p;  // use-after-free
    (void)x;
    std::printf("ERROR: ASan did NOT catch use-after-free\n");
    return 0;
}

} // anonymous namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--uaf") {
        return trigger_uaf();
    }
    const int r1 = run_work_pool_stress();
    const int r2 = run_registry_concurrent();
    const int r3 = run_cpu_executor_and_contract();
    if (r1 != 0 || r2 != 0 || r3 != 0) {
        std::fprintf(stderr, "ACR MSVC ASan stress FAILED\n");
        return 1;
    }
    std::printf("ACR MSVC ASan stress PASSED "
                "(work pool + registry + cpu executor/contract)\n");
    return 0;
}
