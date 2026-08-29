// lib/backend_host/host_services.cpp — host services 默认实现(ABI-001)
// backend 经函数指针消费; 本实现提供可验证的 allocator 计数/原子 cancel/budget 租借。
// 并发合同: 所有回调 threadsafe=yes; 内部并行=none(计数为 atomic)。
#include "astrocs/common_abi_v1.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <malloc.h>
#else
#include <stdlib.h>
#endif

namespace {

struct HostState {
    std::atomic<long> alloc_count{0};     // 在途分配数(alloc-free, selftest 断言归零)
    std::atomic<long> alloc_total{0};     // 累计分配次数(可验证性)
    std::atomic<bool> cancelled{false};
    std::atomic<long> active_workers{0};
    std::atomic<long> max_workers{1};     // 宿主策略上限(set_budget 覆盖)
};

void* host_alloc(void* ud, uint64_t size, uint64_t align) {
    if (size == 0 || align == 0 || (align & (align - 1)) != 0) return nullptr;  // align 2 的幂
    auto* st = static_cast<HostState*>(ud);
    void* p = nullptr;
#ifdef _WIN32
    p = _aligned_malloc(static_cast<size_t>(size), static_cast<size_t>(align));
#else
    // C11/POSIX: std::aligned_alloc 要求 size 是 align 的整数倍(如 align=16 时 size=229 非法)
    // → 将 size 向上取整到 align 的倍数, 满足合同且释放方用 host_free(不依赖原始 size)。
    const std::size_t alloc_sz = (static_cast<std::size_t>(size) + align - 1) & ~(align - 1);
    p = std::aligned_alloc(static_cast<std::size_t>(align), alloc_sz);
#endif
    if (p) {
        st->alloc_count.fetch_add(1);
        st->alloc_total.fetch_add(1);
    }
    return p;
}

void host_free(void* ud, void* p) {
    if (!p) return;  // 合同: NULL 允许
    auto* st = static_cast<HostState*>(ud);
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
    st->alloc_count.fetch_sub(1);
}

void host_log(void* ud, int level, const char* component, const char* msg) {
    (void)ud;
    static const char* kLevel[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char* lv = (level >= 0 && level <= 3) ? kLevel[level] : "LOG";
    std::fprintf(stderr, "[astrocs:%s][%s] %s\n", lv,
                 component ? component : "backend", msg ? msg : "");
}

int host_is_cancelled(void* ud) {
    return static_cast<HostState*>(ud)->cancelled.load(std::memory_order_relaxed) ? 1 : 0;
}

int host_acquire(void* ud, uint32_t n) {
    auto* st = static_cast<HostState*>(ud);
    // CAS 循环: Σ(active)+n ≤ max_workers 才成功(ARCH-004 §1 Σ≤budget)
    long cur = st->active_workers.load(std::memory_order_relaxed);
    const long cap = st->max_workers.load(std::memory_order_relaxed);
    while (true) {
        if (cur + static_cast<long>(n) > cap) return ACS_ERR_BUDGET;
        if (st->active_workers.compare_exchange_weak(cur, cur + static_cast<long>(n),
                                                     std::memory_order_relaxed))
            return 0;
    }
}

void host_release(void* ud, uint32_t n) {
    static_cast<HostState*>(ud)->active_workers.fetch_sub(static_cast<long>(n));
}

}  // namespace

// ───────── 宿主侧装配 C 接口(供 CLI/测试构造 host services) ─────────

extern "C" {

/* 填充一份默认 host services(线程安全计数; 崩溃安全: 无异常路径)。
 * reentrant=yes; threadsafe=yes; 返回 0 成功。state_out 由调用方持有并传入 destroy。 */
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out) {
    if (!out || !state_out) return ACS_ERR_PARAM;
    auto* st = new HostState();
    std::memset(out, 0, sizeof(*out));
    out->struct_size = static_cast<uint32_t>(sizeof(astrocs_host_services_v1));
    out->abi_version = ACS_ABI_VERSION_V1;
    out->allocator.struct_size = static_cast<uint32_t>(sizeof(acs_allocator));
    out->allocator.abi_version = ACS_ABI_VERSION_V1;
    out->allocator.alloc = &host_alloc;
    out->allocator.free = &host_free;
    out->allocator.user_data = st;
    out->logger.struct_size = static_cast<uint32_t>(sizeof(acs_logger));
    out->logger.abi_version = ACS_ABI_VERSION_V1;
    out->logger.log = &host_log;
    out->logger.user_data = st;
    out->cancel.struct_size = static_cast<uint32_t>(sizeof(acs_cancel));
    out->cancel.abi_version = ACS_ABI_VERSION_V1;
    out->cancel.is_cancelled = &host_is_cancelled;
    out->cancel.user_data = st;
    out->budget.struct_size = static_cast<uint32_t>(sizeof(acs_thread_budget));
    out->budget.abi_version = ACS_ABI_VERSION_V1;
    out->budget.available_cpus = 1;   // 宿主启动时按 ARCH-004 §1 检测覆盖
    out->budget.max_workers = 1;
    out->budget.acquire = &host_acquire;
    out->budget.release = &host_release;
    out->budget.user_data = st;
    *state_out = st;
    return ACS_OK;
}

void astrocs_host_services_destroy_state_v1(void* state) {
    delete static_cast<HostState*>(state);
}

void astrocs_host_state_set_cancel(void* state, int v) {
    static_cast<HostState*>(state)->cancelled.store(v != 0, std::memory_order_relaxed);
}

long astrocs_host_state_alloc_count(void* state) {
    return static_cast<HostState*>(state)->alloc_count.load();
}

long astrocs_host_state_alloc_total(void* state) {
    return static_cast<HostState*>(state)->alloc_total.load();
}

long astrocs_host_state_active_workers(void* state) {
    return static_cast<HostState*>(state)->active_workers.load();
}

void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out) {
    auto* st = static_cast<HostState*>(state);
    st->max_workers.store(static_cast<long>(max_workers), std::memory_order_relaxed);
    out->budget.available_cpus = cpus;
    out->budget.max_workers = max_workers;
}

}  // extern "C"
