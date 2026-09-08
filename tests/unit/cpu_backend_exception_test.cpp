// tests/unit/cpu_backend_exception_test.cpp — P1-4 baseline kernels 异常收敛注入单测
// 缺陷机理(P1-4): baseline_kernels_impl.inc run_banded 原实现 std::thread body 直接
// 调用 kernel body, body 抛异常 → 异常逃逸 thread 函数 = std::terminate;
// 串行/主线程路径抛出则会穿越 extern "C" kernel fn 边界跨 ABI(冻结约束 F.3 禁止)。
// 修复对齐 f1cb487c P0-4 "C 边界异常屏障"家族口径: catch → 结构化错误码
// ACS_ERR_INTERNAL(不外泄异常/STL 类型), join 后由 kernel_dispatch 统一返回。
//
// 注入方式(最小侵入, 如实取舍): 真实科学 body OpComputer 在合法参数域内无
// 可注入抛出点(栈类 resize 受 frames*N span 校验约束, 无法以合法参数触发
// bad_alloc); 因此本测试 TU 以匿名命名空间 include baseline_kernels_impl.inc,
// 直接调用 run_banded 并注入可抛 std::function body —— 与 OpComputer 走
// 完全相同的 run_body_guarded → std::thread 通路, 不改产品源码, 不注入
// kernel_dispatch(其 body=OpComputer 不可注入; 修复点本就在 run_banded)。
// 关键断言: worker 行带注入抛出时测试进程必须存活(修复前 std::terminate)
// 且 run_banded 返回 ACS_ERR_INTERNAL + 租借/归还配平。
//
// 注: 本 TU 不引用 baseline_backend.cpp.o 的任何外链符号, 匿名命名空间实体
// 均为内部链接, 与 lib 内同名 .inc 实例无 ODR 冲突。
#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "baseline_kernels_impl.inc"  // 匿名命名空间: run_banded/run_body_guarded 可注入

namespace {

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                               \
        }                                                               \
    } while (0)

/* inline host services(与 cpu001_provider_selftest / cpu_lease_test 同款合同) */
std::atomic<uint32_t> g_acquired{0}, g_released{0};

void* fake_alloc(void*, uint64_t size, uint64_t align) {
    return ::operator new(static_cast<size_t>(size),
                          std::align_val_t(static_cast<size_t>(align)));
}
void fake_free(void*, void* p) {
    ::operator delete(p, std::align_val_t(static_cast<size_t>(64)));
}
void fake_cancel(void*) {}
int fake_acquire(void*, uint32_t n) {
    g_acquired.fetch_add(n);
    return 0;  // 恒成功: 观察租借/归还配平
}
void fake_release(void*, uint32_t n) { g_released.fetch_add(n); }
/* 预算耗尽型 acquire: n>1 拒绝(不占用预算) → run_banded 走"预算耗尽仍须可跑(串行)"路径 */
int exhaust_acquire(void*, uint32_t n) {
    if (n > 1) return 1;
    g_acquired.fetch_add(n);
    return 0;
}

/* 计数型 logger: 记录调用次数与 ERROR 级次数(线程安全, 屏障在多线程并发调用) */
struct LogCount {
    std::atomic<int> calls{0};
    std::atomic<int> errors{0};
};
void counting_log(void* ud, int level, const char*, const char*) {
    auto* lc = static_cast<LogCount*>(ud);
    lc->calls.fetch_add(1);
    if (level >= ACS_LOG_ERROR) lc->errors.fetch_add(1);
}

astrocs_host_services_v1 make_host(uint32_t max_workers,
                                   int (*acquire)(void*, uint32_t),
                                   LogCount* log_ud) {
    astrocs_host_services_v1 h{};
    h.struct_size = sizeof(h);
    h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = {sizeof(acs_allocator), ACS_ABI_VERSION_V1, &fake_alloc,
                   &fake_free, nullptr};
    h.logger = {sizeof(acs_logger), ACS_ABI_VERSION_V1, &counting_log, log_ud};
    h.cancel = {sizeof(acs_cancel), ACS_ABI_VERSION_V1,
                [](void*) { return 0; }, nullptr};
    h.budget = {sizeof(acs_thread_budget), ACS_ABI_VERSION_V1, max_workers,
                max_workers, acquire, &fake_release, nullptr};
    return h;
}

/* 单用例: throw_bands=0 全不抛; 1 仅 worker 行带抛(tid!=0); 2 仅主行带抛(tid==0);
 * 3 全部行带抛。返回 0=通过(断言在内部)。 */
int run_case(const char* name, uint32_t max_workers,
             int (*acquire)(void*, uint32_t), uint32_t n_rows, int throw_bands) {
    LogCount lc;
    astrocs_host_services_v1 host = make_host(max_workers, acquire, &lc);
    g_acquired.store(0);
    g_released.store(0);
    std::atomic<int> caught_std{0}, caught_nonstd{0};

    auto body = [&](uint32_t y0, uint32_t y1, uint32_t tid) {
        (void)y0;
        (void)y1;
        const bool want_throw =
            (throw_bands == 3) || (throw_bands == 1 && tid != 0) ||
            (throw_bands == 2 && tid == 0);
        if (want_throw) {
            try {
                throw std::runtime_error("p1-4 inject");
            } catch (const std::exception&) {
                caught_std.fetch_add(1);  // 注入确为 std::exception 派生
                throw;                    // 交由 run_body_guarded 收敛
            } catch (...) {
                caught_nonstd.fetch_add(1);
                throw;
            }
        }
    };

    acs_status err = ACS_OK;
    const uint32_t used = run_banded(&host, n_rows, body, &err);  // 若未修复: worker 路径在此 std::terminate

    std::printf("  case %-24s throw_bands=%d err=%d workers=%u log_err=%d "
                "acq=%u rel=%u\n",
                name, throw_bands, static_cast<int>(err), used,
                lc.errors.load(), g_acquired.load(), g_released.load());

    if (throw_bands != 0) {
        CHECK(caught_std.load() >= 1);       // 注入生效且为 std::exception 派生
        CHECK(caught_nonstd.load() == 0);
        CHECK(err == ACS_ERR_INTERNAL);      // 收敛为结构化错误码(不外泄异常)
        CHECK(lc.errors.load() >= 1);        // 屏障记录 ERROR 级日志
    } else {
        CHECK(err == ACS_OK);                // 回归: 正常 body 结果不变
    }
    CHECK(used >= 1 && used <= max_workers); // worker 数口径不变
    CHECK(g_acquired.load() == g_released.load());  // 租借/归还配平(body 失败也归还)
    return 0;
}

}  // namespace

/* ABI 边界回归(家族机制证明): f1cb487c 引入的边界屏障仍在 */
extern "C" int astrocs_abi_boundary_probe(int mode);

int main() {
    std::printf("P1-4 baseline kernels exception-barrier injection tests\n");
    // 1) 多线程路径: worker 行带(tid=1..3)注入抛出 —— 修复前: 异常逃逸
    //    std::thread body → std::terminate(本进程直接 abort, 测试即红)
    CHECK(run_case("parallel_worker_throw", 4, &fake_acquire, 64, 1) == 0);
    // 2) 多线程全行带抛: 主行带 + worker 行带同时注入
    CHECK(run_case("parallel_all_throw", 4, &fake_acquire, 64, 3) == 0);
    // 3) 串行路径: max_workers=1, 主线程 body 抛出(修复前: 穿越 extern "C" 边界)
    CHECK(run_case("serial_main_throw", 1, &fake_acquire, 8, 2) == 0);
    // 4) 预算耗尽兜底: acquire(n>1) 拒绝 → 串行 body 抛出
    CHECK(run_case("budget_exhausted_throw", 4, &exhaust_acquire, 8, 2) == 0);
    // 5/6) 回归保护: 正常 body 多线程/串行路径 ACS_OK、行为不变
    CHECK(run_case("parallel_ok", 4, &fake_acquire, 64, 0) == 0);
    CHECK(run_case("serial_ok", 1, &fake_acquire, 8, 0) == 0);

    // 7) ABI 边界屏障回归: astrocs_abi_boundary_probe(mode=1) 仍收敛 ACS_ERR_INTERNAL
    CHECK(astrocs_abi_boundary_probe(0) == ACS_OK);
    CHECK(astrocs_abi_boundary_probe(1) == ACS_ERR_INTERNAL);

    std::printf("CPU-BACKEND-EXCEPTION TESTS PASS\n");
    return 0;
}
