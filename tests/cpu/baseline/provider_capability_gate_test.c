/* AstroCS CPU baseline provider — capability 门负测 (stub 探测)
 * tests/cpu/baseline/provider_capability_gate_test.c (CPU-002)
 *
 * 覆盖 (CPU-002 验收 "能力矩阵消费: capability 不足 → baseline 路径"):
 *   生产 query 在真实 CPUID 上只要求 SSE2 (amd64 恒备)。能力不足 (非 amd64 /
 *   OS 不保证 SSE2) 的拒绝路径无法在真实 x86 触发, 以 stub 探测注入:
 *   - 替换 acs_cap_detect_v1 (链接期取本 TU 强符号): 返回
 *     ACS_CAP_ERR_UNSUPPORTED (非 amd64) → cap_gate 拒绝 (ACS_ERR_UNSUPPORTED);
 *   - 合成结果 os_safe 不含 SSE2 → cap_gate 拒绝;
 *   - 合成结果 os_safe 含 SSE2 (模拟 amd64) → cap_gate 通过;
 *   - acs_cpu_baseline_cap_gate 是本 provider 的 query 能力门 (命名导出,
 *     供 host/测试直接判定; 生产 query 亦经它)。
 *
 * 链接: 本 TU + baseline_provider.cpp (不带真实 capability_detect.c)。
 * 纯 C11; 退出码 0=全 PASS。
 */
#include "astrocs/cpu/baseline_provider_v1.h"
#include "astrocs/cpu/capability_v1.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)
#define CHECK_ST(expect, actual, what)                                    \
  do {                                                                    \
    int _e = (expect), _a = (actual);                                     \
    if (_e != _a) {                                                       \
      fprintf(stderr, "FAIL %s:%d: %s expect=%d got=%d\n", __FILE__,      \
              __LINE__, what, _e, _a);                                    \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

/* 本 TU 导出的 astrocs_cpu_baseline_cap_gate (provider 内声明) */
int acs_cpu_baseline_cap_gate(acs_cap_result_v1* out);

/* ── stub 探测 (链接期覆盖 capability_detect.c 符号) ── */
static int g_stub_mode = 0;   /* 0=unsupported 1=os_safe 无 SSE2 2=os_safe 有 SSE2 */
int acs_cap_detect_v1(acs_cap_result_v1* out) {
    if (out == NULL) return ACS_CAP_ERR_PARAM;
    memset(out, 0, sizeof(*out));
    out->struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    out->abi_version = ACS_CAP_ABI_VERSION_V1;
    out->schema_version = ACS_CAP_SCHEMA_VER;
    if (g_stub_mode == 0) return ACS_CAP_ERR_UNSUPPORTED;
    if (g_stub_mode == 1) {
        /* amd64 探测 OK 但 OS 不保证 SSE2 (理论面; os_safe 空) */
        out->os_safe = 0;
        return ACS_CAP_OK;
    }
    /* mode 2: 正常 amd64 (SSE2 os_safe) */
    out->os_safe = ACS_CAP_FEAT_SSE2;
    return ACS_CAP_OK;
}

/* os_safe 判定 stub: required ⊆ cap->os_safe (required=SSE2 单测场景) */
int acs_cap_os_safe_satisfies_v1(const acs_cap_result_v1* cap, uint64_t required) {
    if (cap == NULL) return 0;
    if (required == 0) return 1;
    return (cap->os_safe & required) == required;
}

int main(void) {
    acs_cap_result_v1 cap;
    memset(&cap, 0, sizeof(cap));
    cap.struct_size = (uint32_t)sizeof(cap);
    cap.abi_version = ACS_CAP_ABI_VERSION_V1;

    /* 1. 探测失败 (非 amd64) → 拒绝 */
    g_stub_mode = 0;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_baseline_cap_gate(&cap),
             "cap detect unsupported");

    /* 2. os_safe 不含 SSE2 → 拒绝 */
    g_stub_mode = 1;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_baseline_cap_gate(&cap),
             "os_safe no sse2");

    /* 3. os_safe 含 SSE2 → 通过; out 填充探测结果 */
    g_stub_mode = 2;
    CHECK_ST(ACS_OK, acs_cpu_baseline_cap_gate(&cap), "os_safe sse2 ok");
    CHECK(cap.os_safe == ACS_CAP_FEAT_SSE2);

    if (g_failures) {
        fprintf(stderr, "CAPABILITY GATE FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("provider_capability_gate_test: ALL PASS (unsupported/no-sse2 rejected, sse2 accepted)\n");
    return 0;
}
