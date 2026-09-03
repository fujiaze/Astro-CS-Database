/* AstroCS CPU AVX2/FMA provider — capability 门负测 (stub 探测)
 * tests/cpu/avx2/provider_avx2_capability_gate_test.c (CPU-003)
 *
 * 覆盖 (CPU-003 验收 "非支持 CPU 不加载; CPUID/XGETBV negative"):
 *   生产 query 在真实 CPUID/XGETBV 上要求 required=(AVX|AVX2|FMA) ⊆ os_safe
 *   (OSXSAVE=1 且 XCR0.XMM|YMM=0x6; CPU-001 classify 组包含)。非支持路径
 *   无法在真实 AVX2 主机触发, 以 stub 探测注入 (链接期替换
 *   acs_cap_detect_v1/acs_cap_os_safe_satisfies_v1, 同 baseline gate 测试法):
 *   - 探测失败 (非 amd64) → 拒绝 ACS_ERR_UNSUPPORTED;
 *   - 合成 CPUID 证据缺 AVX2 (os_safe 不含 AVX2) → 拒绝
 *     (硬件不支持 / CPUID negative);
 *   - 合成 OS 状态缺 YMM (osxsave=0 或 xcr0 缺 0x6; XGETBV negative) → 拒绝
 *     (硬件支持但 OS 不保存 YMM → os_safe 平面清除);
 *   - 合成 os_safe 含 AVX|AVX2|FMA (模拟 AVX2 机) → 通过; out 填充判定结果。
 *   - acs_cpu_avx2_cap_gate 是本 provider 的 query 能力门 (命名导出, 供
 *     host/测试直接判定; 生产 query 亦经它)。
 *
 * 链接: 本 TU + avx2_provider.cpp (不带真实 capability_detect.c)。
 * 纯 C11; 退出码 0=全 PASS。
 */
#include "astrocs/cpu/avx2_provider_v1.h"
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

/* 本 TU 声明的 astrocs_cpu_avx2_cap_gate (provider 内 extern "C" 定义;
 * C/C++ 双编译保护: gcc -std=c11 或 g++ -std=c++17 皆可链接) */
#ifdef __cplusplus
extern "C" {
#endif
int acs_cpu_avx2_cap_gate(acs_cap_result_v1* out);
#ifdef __cplusplus
}
#endif

/* ── stub 探测 (链接期覆盖 capability_detect.c 符号) ──
 * mode: 0=detect 失败  1=无 AVX 家族  2=OS 禁 YMM  3=AVX2 机 (通过) */
static int g_stub_mode = 0;
int acs_cap_detect_v1(acs_cap_result_v1* out) {
    if (out == NULL) return ACS_CAP_ERR_PARAM;
    memset(out, 0, sizeof(*out));
    out->struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    out->abi_version = ACS_CAP_ABI_VERSION_V1;
    out->schema_version = ACS_CAP_SCHEMA_VER;
    if (g_stub_mode == 0) return ACS_CAP_ERR_UNSUPPORTED;   /* 非 amd64/探测失败 */
    if (g_stub_mode == 1) {
        /* baseline 机: hw 仅 SSE2..SSE4_2; os_safe 同 (无 AVX 家族) */
        out->hw_features = ACS_CAP_FEAT_SSE2;
        out->os_safe = ACS_CAP_FEAT_SSE2;
        return ACS_CAP_OK;
    }
    if (g_stub_mode == 2) {
        /* 全 AVX2 硬件但 OS 禁 YMM: osxsave=0 (或 xcr0 缺 0x6) →
         * classify 不清 AVX 家族入 os_safe (XGETBV negative) */
        out->hw_features = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY;
        out->os_safe = ACS_CAP_FEAT_SSE2;   /* AVX 家族被 OS 状态裁剪 */
        out->osxsave = 0;
        out->xcr0 = 0;
        return ACS_CAP_OK;
    }
    /* mode 3: AVX2 机 (OS 保存 XMM|YMM) */
    out->hw_features = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY;
    out->os_safe = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY;
    out->osxsave = 1;
    out->xcr0 = 0x6u;
    return ACS_CAP_OK;
}

/* os_safe 判定 stub: required ⊆ cap->os_safe (等价生产 os_safe_satisfies) */
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

    /* 1. 探测失败 (非 amd64) → 拒绝加载 (ACS_ERR_UNSUPPORTED) */
    g_stub_mode = 0;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx2_cap_gate(&cap),
             "cap detect unsupported (非 amd64)");

    /* 2. CPUID negative: 硬件缺 AVX2/FMA (baseline 机) → 拒绝 */
    g_stub_mode = 1;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx2_cap_gate(&cap),
             "cpuid negative (无 AVX2/FMA hw)");

    /* 3. XGETBV negative: 硬件有 AVX2 但 OS 不保存 YMM (osxsave=0/xcr0=0)
     *    → os_safe 平面清除 → 拒绝 */
    g_stub_mode = 2;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx2_cap_gate(&cap),
             "xgetbv negative (OS 不保存 YMM)");

    /* 4. AVX2 机 (osxsave=1, xcr0=0x6, AVX 家族 os_safe) → 通过 */
    g_stub_mode = 3;
    CHECK_ST(ACS_OK, acs_cpu_avx2_cap_gate(&cap), "avx2 machine ok");
    CHECK((cap.os_safe & ACS_CPU_AVX2_REQUIRED_FEATURES) ==
          ACS_CPU_AVX2_REQUIRED_FEATURES);
    CHECK(cap.xcr0 == 0x6u);

    if (g_failures) {
        fprintf(stderr, "AVX2 CAPABILITY GATE FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("provider_avx2_capability_gate_test: ALL PASS "
           "(unsupported/no-avx2/os-no-ymm rejected, avx2 accepted)\n");
    return 0;
}
