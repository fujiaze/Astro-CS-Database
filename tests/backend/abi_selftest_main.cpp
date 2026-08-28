// tests/backend/abi_selftest_main.cpp — ABI v1 运行时落锤(ABI-001)
// 断言: handshake 拒绝/自检通过/allocator 计数可验证/budget 租借/异常不跨边界/布局确定。
// Debug(-O0) 与 Release(-O2) 双构建各跑一遍, 输出布局行由 Python 测试比对相等。
#include <cstdio>
#include <cstring>

#include "astrocs/common_abi_v1.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
long astrocs_host_state_alloc_count(void* state);
long astrocs_host_state_alloc_total(void* state);
}

namespace {

int failures = 0;

void check(int cond, const char* what) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++failures;
}

}  // namespace

int main() {
    // ── 编译期布局冻结(双构建必须一致; Python 比对运行时输出) ──
    std::printf("layout %zu %zu %zu %zu %zu %zu %zu %zu\n",
                sizeof(acs_head), sizeof(acs_span_f64), sizeof(astrocs_host_services_v1),
                sizeof(acs_allocator), sizeof(acs_logger), sizeof(acs_cancel),
                sizeof(acs_thread_budget), sizeof(astrocs_backend_api_v1));
    static_assert(sizeof(acs_head) == 8, "head=2x u32");
    static_assert(sizeof(astrocs_backend_api_v1) > 128, "api v1 完整");

    astrocs_host_services_v1 host;
    void* state = nullptr;
    check(astrocs_host_services_default_v1(&host, &state) == ACS_OK, "host services default");
    check(host.struct_size == sizeof(astrocs_host_services_v1), "host struct_size handshake");
    astrocs_host_state_set_budget_v1(state, 4, 2, &host);

    // ── handshake 拒绝面 ──
    astrocs_backend_api_v1 api;
    std::memset(&api, 0, sizeof(api));
    check(astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1 + 1, sizeof(astrocs_host_services_v1),
                                     &host, &api) == ACS_ERR_ABI_MISMATCH,
          "abi_version mismatch rejected");
    check(astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1) - 4,
                                     &host, &api) == ACS_ERR_ABI_MISMATCH,
          "host struct_size mismatch rejected");

    // ── 正常获取 + kernel 表 ──
    check(astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1),
                                     &host, &api) == ACS_OK, "get_api_v1 ok");
    check(api.abi_version == ACS_ABI_VERSION_V1 && api.struct_size == sizeof(api), "api handshake");
    check(api.kernel_count == 12, "kernel table 12 entries (05 §5)");
    check(std::strcmp(api.kernels[0].science_contract_id, "ALG-001") == 0, "kernel contract id");
    check(api.nested_parallel_allowed == 0, "no nested parallel");
    check(api.kernels[0].fn(&host, nullptr, 0, nullptr, nullptr) == ACS_ERR_UNSUPPORTED,
          "kernel stub explicit UNSUPPORTED (ABI-003 落地前)");

    // ── self_test + allocator 可验证性 ──
    const long before_total = astrocs_host_state_alloc_total(state);
    check(api.self_test(&host) == ACS_OK, "self_test ok");
    check(astrocs_host_state_alloc_total(state) > before_total,
          "self_test went through host allocator (verifiable)");
    check(astrocs_host_state_alloc_count(state) == 0, "allocator balanced (no leak)");

    // ── budget: Σ(active) ≤ max_workers=2; 超租失败 ──
    check(host.budget.acquire(host.budget.user_data, 1) == 0, "budget acquire 1");
    check(host.budget.acquire(host.budget.user_data, 1) == 0, "budget acquire 2");
    check(host.budget.acquire(host.budget.user_data, 1) == ACS_ERR_BUDGET, "budget excess rejected");
    host.budget.release(host.budget.user_data, 2);
    check(host.budget.acquire(host.budget.user_data, 2) == 0, "budget released then full acquire");
    host.budget.release(host.budget.user_data, 2);

    // ── 异常不跨边界(baseline TU 内 catch→状态码) ──
    check(astrocs_abi_boundary_probe(0) == ACS_OK, "boundary probe pass-through");
    check(astrocs_abi_boundary_probe(1) == ACS_ERR_INTERNAL, "exception contained at boundary");

    astrocs_host_services_destroy_state_v1(state);
    std::printf("%s failures=%d\n", failures == 0 ? "ALL_OK" : "HAS_FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
