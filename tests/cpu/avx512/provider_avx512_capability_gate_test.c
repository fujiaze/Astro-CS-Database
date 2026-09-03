/* AstroCS CPU AVX-512 provider — capability 门负测 (stub 探测)
 * tests/cpu/avx512/provider_avx512_capability_gate_test.c (CPU-004)
 *
 * 覆盖 (CPU-004 验收 "缺任何子集/OS ZMM state 拒绝; 不能只看 AVX512F"):
 *   生产 query 在真实 CPUID/XGETBV 上要求 required = F|CD|BW|DQ|VL 五子集
 *   ⊆ os_safe (CPU-001 classify: OSXSAVE=1 且 XCR0.opmask|ZMM_Hi256|Hi16_ZMM
 *   = 0xE0 全置 且五子集 hw 全置才整体进 os_safe)。非支持路径无法在真实
 *   全 AVX-512 主机触发, 以 stub 探测注入 (链接期替换 acs_cap_detect_v1 /
 *   acs_cap_os_safe_satisfies_v1, 同 baseline/avx2 gate 测试法):
 *   - 探测失败 (非 amd64) → 拒绝 ACS_ERR_UNSUPPORTED;
 *   - 缺任一子集 (F 有但 CD/BW/DQ/VL 缺) → 拒绝 (CPUID negative; 组整体
 *     不在 os_safe —— "不能只看 AVX512F");
 *   - OS 不保存 ZMM state (osxsave=1 但 xcr0 仅 0x6 无 0xE0) → 拒绝
 *     (XGETBV negative; 硬件支持但 OS 不保存 opmask/ZMM → os_safe 清除);
 *   - 全 AVX-512 机 (osxsave=1, xcr0=0xE6, 五子集全) → 通过; out 填充。
 *   - acs_cpu_avx512_cap_gate 是本 provider 的 query 能力门 (命名导出)。
 *
 * 链接: 本 TU + avx512_provider.cpp (不带真实 capability_detect.c)。
 * 纯 C11/C++17 双可编译; 退出码 0=全 PASS。
 */
#include "astrocs/cpu/avx512_provider_v1.h"
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

#ifdef __cplusplus
extern "C" {
#endif
int acs_cpu_avx512_cap_gate(acs_cap_result_v1* out);
#ifdef __cplusplus
}
#endif

/* ── stub 探测 (链接期覆盖 capability_detect.c 符号) ──
 * mode: 0=detect 失败  1=缺子集 (F-only)  2=OS 禁 ZMM (xcr0 0x6)
 *       3=全 AVX-512 (xcr0 0xE6)  4=无 AVX-512 硬件 */
static int g_stub_mode = 0;
int acs_cap_detect_v1(acs_cap_result_v1* out) {
    if (out == NULL) return ACS_CAP_ERR_PARAM;
    memset(out, 0, sizeof(*out));
    out->struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    out->abi_version = ACS_CAP_ABI_VERSION_V1;
    out->schema_version = ACS_CAP_SCHEMA_VER;
    if (g_stub_mode == 0) return ACS_CAP_ERR_UNSUPPORTED;   /* 非 amd64/探测失败 */
    if (g_stub_mode == 1) {
        /* AVX512F-only: 只有 F 位, CD/BW/DQ/VL 缺 → 组整体不在 os_safe */
        out->hw_features = ACS_CAP_FEAT_SSE2 | ACS_CAP_FEAT_AVX512F;
        out->os_safe = ACS_CAP_FEAT_SSE2 | ACS_CAP_FEAT_AVX512F;
        out->osxsave = 1;
        out->xcr0 = 0xE6u;
        return ACS_CAP_OK;
    }
    if (g_stub_mode == 2) {
        /* 全五子集硬件但 OS 只保存 XMM|YMM (xcr0 0x6 无 opmask/ZMM 0xE0):
         * XGETBV negative → AVX-512 组被 OS 状态裁剪 */
        out->hw_features = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY |
                           ACS_CAP_GROUP_AVX512_SUBSET;
        out->os_safe = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY;
        out->osxsave = 1;
        out->xcr0 = 0x6u;
        return ACS_CAP_OK;
    }
    if (g_stub_mode == 4) {
        /* 无 AVX-512 硬件 (仅 AVX2 家族) */
        out->hw_features = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY;
        out->os_safe = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY;
        out->osxsave = 1;
        out->xcr0 = 0x6u;
        return ACS_CAP_OK;
    }
    /* mode 3: 全 AVX-512 机 (OS 保存 opmask|ZMM_Hi256|Hi16_ZMM) */
    out->hw_features = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY |
                       ACS_CAP_GROUP_AVX512_SUBSET;
    out->os_safe = ACS_CAP_FEAT_SSE2 | ACS_CAP_GROUP_AVX_FAMILY |
                   ACS_CAP_GROUP_AVX512_SUBSET;
    out->osxsave = 1;
    out->xcr0 = 0xE6u;
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

    /* 1. 探测失败 (非 amd64) → 拒绝 */
    g_stub_mode = 0;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx512_cap_gate(&cap),
             "cap detect unsupported (非 amd64)");

    /* 2. CPUID negative: 缺子集 (AVX512F-only, CD/BW/DQ/VL 缺) → 拒绝
     *    (不能只看 AVX512F=true; 15 §2) */
    g_stub_mode = 1;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx512_cap_gate(&cap),
             "missing subset (F-only) rejected");

    /* 3. XGETBV negative: 全子集硬件但 OS 不保存 ZMM state (xcr0 0x6)
     *    → os_safe 清除 AVX-512 组 → 拒绝 */
    g_stub_mode = 2;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx512_cap_gate(&cap),
             "os no zmm state rejected");

    /* 4. 无 AVX-512 硬件 (仅 AVX2) → 拒绝 */
    g_stub_mode = 4;
    CHECK_ST(ACS_ERR_UNSUPPORTED, acs_cpu_avx512_cap_gate(&cap),
             "no avx512 hw rejected");

    /* 5. 全 AVX-512 机 (osxsave=1, xcr0=0xE6, 五子集 os_safe) → 通过 */
    g_stub_mode = 3;
    CHECK_ST(ACS_OK, acs_cpu_avx512_cap_gate(&cap), "full avx512 ok");
    CHECK((cap.os_safe & ACS_CPU_AVX512_REQUIRED_FEATURES) ==
          ACS_CPU_AVX512_REQUIRED_FEATURES);
    CHECK(cap.xcr0 == 0xE6u);

    if (g_failures) {
        fprintf(stderr, "AVX512 CAPABILITY GATE FAIL: %d failures\n",
                g_failures);
        return 1;
    }
    printf("provider_avx512_capability_gate_test: ALL PASS "
           "(unsupported/missing-subset/os-no-zmm/no-hw rejected, "
           "full avx512 accepted)\n");
    return 0;
}
