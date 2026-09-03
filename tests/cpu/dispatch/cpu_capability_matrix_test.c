/* AstroCS CPU-001 feature matrix 模拟测试 — tests/cpu/dispatch/cpu_capability_matrix_test.c
 *
 * 目的 (CPU-001 验收 "模拟 feature matrix; 缺 AVX/OS state/子集拒绝"):
 *   以合成 CPUID/OS 证据 (leaf1_ecx/edx, leaf7_ebx, osxsave 经 leaf1_ecx.bit27,
 *   xcr0) 驱动生产判定引擎 acs_cap_classify_v1 → os_safe 平面, 再经
 *   acs_cap_os_safe_satisfies_v1 判定 provider required_features。
 *   负测 (缺 AVX hw / 缺 OS state / 缺 AVX-512 子集 / OS 不保存 ZMM) 全部
 *   必须在 os_safe 平面拒绝; 正测 (OS 全保存 + 五子集齐) 必须通过。
 *
 * 事实源: 判定规则唯一实现 = providers/cpu/common/src/capability_detect.c
 *   cap_classify (经 acs_cap_classify_v1 公开入口); 本测试不复制规则。
 *
 * 运行: 直接可执行 (exit 0 = 全 PASS); 亦被 run_cpu_capability_checks.py 调用。
 * 本测试不读取核心数、不执行任何 AVX* 指令。
 */
#include "astrocs/cpu/capability_v1.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "CHECK FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

/* CPUID 证据构造器: 直接置位 (模拟硬件), OS 状态单独给 */
static void mk_raw(acs_cap_result_v1* r, uint32_t ecx1, uint32_t edx1,
                   uint32_t ebx7, uint64_t xcr0, uint32_t osxsave) {
    memset(r, 0, sizeof(*r));
    r->struct_size = (uint32_t)sizeof(acs_cap_result_v1);
    r->abi_version = ACS_CAP_ABI_VERSION_V1;
    r->schema_version = ACS_CAP_SCHEMA_VER;
    r->leaf1_ecx = ecx1 | (osxsave ? (1u << 27) : 0u);
    r->leaf1_edx = edx1;
    r->leaf7_ebx = ebx7;
    r->xcr0 = xcr0;
}

int main(void) {
    /* CPUID 位面 (与 capability_detect.c cap_classify 唯一事实源一致) */
    const uint32_t ECX1_FMA = 1u << 12;
    const uint32_t ECX1_SSE41 = 1u << 19;
    const uint32_t ECX1_SSE42 = 1u << 20;
    const uint32_t ECX1_AVX = 1u << 28;
    const uint32_t EDX1_SSE2 = 1u << 26;
    const uint32_t EBX7_BMI1 = 1u << 3;
    const uint32_t EBX7_BMI2 = 1u << 8;
    const uint32_t EBX7_AVX512F = 1u << 16;
    const uint32_t EBX7_AVX512DQ = 1u << 17;
    const uint32_t EBX7_AVX512CD = 1u << 28;
    const uint32_t EBX7_AVX512BW = 1u << 30;
    const uint32_t EBX7_AVX512VL = 1u << 31;

    const uint64_t F_AVX = ACS_CAP_FEAT_AVX;
    const uint64_t F_AVX2 = ACS_CAP_FEAT_AVX2;
    const uint64_t F_FMA = ACS_CAP_FEAT_FMA;
    const uint64_t F_512 = (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET;

    acs_cap_result_v1 raw, out;

    /* ═══ R1: baseline 机 (无 AVX hw) — 缺 AVX 拒绝 ═══ */
    mk_raw(&raw, ECX1_SSE41 | ECX1_SSE42, EDX1_SSE2, EBX7_BMI1, 0, 0);
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R1 classify");
    CHECK((out.hw_features & (F_AVX | F_AVX2 | F_FMA)) == 0, "R1 no avx hw");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_AVX2) == 0, "R1 reject avx2 (no hw)");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_SSE2) == 1, "R1 sse2 pass");
    CHECK(acs_cap_hw_satisfies_v1(&out, ACS_CAP_FEAT_AVX2) == 0, "R1 hw reject avx2");

    /* ═══ R2: AVX2 hw 齐但 OS 禁 AVX (osxsave=0) — 缺 OS state 拒绝 ═══ */
    mk_raw(&raw, ECX1_FMA | ECX1_SSE41 | ECX1_AVX, EDX1_SSE2,
           EBX7_BMI1 | EBX7_BMI2 | (EBX7_AVX512F & 0u), 0, 0);
    /* 注意: AVX2 位在叶 7 EBX bit5; 上面没置 → 补正 R2 需置 */
    raw.leaf7_ebx = EBX7_BMI1 | EBX7_BMI2 | (1u << 5); /* +AVX2 */
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R2 classify");
    CHECK((out.hw_features & (F_AVX | F_AVX2 | F_FMA)) == (F_AVX | F_AVX2 | F_FMA), "R2 hw has avx family");
    CHECK(out.osxsave == 0, "R2 osxsave=0");
    CHECK((out.os_safe & (F_AVX | F_AVX2 | F_FMA)) == 0, "R2 os_safe no avx (no os state)");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, F_AVX2) == 0, "R2 reject avx2 (no os state)");
    CHECK(acs_cap_hw_satisfies_v1(&out, F_AVX2) == 1, "R2 hw pass avx2 (diagnose)");

    /* ═══ R3: AVX2 hw + OS 保存 XMM|YMM (xcr0=0x6) — 正测 ═══ */
    mk_raw(&raw, ECX1_FMA | ECX1_SSE41 | ECX1_SSE42 | ECX1_AVX, EDX1_SSE2,
           EBX7_BMI1 | EBX7_BMI2 | (1u << 5), 0x6, 1);
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R3 classify");
    CHECK((out.os_safe & (F_AVX | F_AVX2 | F_FMA)) == (F_AVX | F_AVX2 | F_FMA), "R3 avx family os_safe");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, F_AVX2) == 1, "R3 avx2 pass");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_AVX512F) == 0, "R3 avx512f reject (no hw)");
    CHECK(acs_cap_os_saves_avx512_state_v1(&out) == 0, "R3 xcr0 no zmm");

    /* ═══ R4: AVX512F-only hw, CD/BW/DQ/VL 缺 — 缺子集拒绝 ═══ */
    mk_raw(&raw, ECX1_FMA | ECX1_SSE41 | ECX1_SSE42 | ECX1_AVX, EDX1_SSE2,
           EBX7_BMI1 | (1u << 5) | EBX7_AVX512F, 0xE6, 1);
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R4 classify");
    CHECK((out.hw_features & F_512) != 0, "R4 hw has some avx512 (F only)");
    CHECK((out.hw_features & F_512) != F_512, "R4 hw lacks subset bits");
    CHECK((out.os_safe & F_512) == 0, "R4 os_safe no avx512 (subset missing)");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_AVX512F) == 0, "R4 reject avx512f (subset)");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_AVX512VL) == 0, "R4 reject avx512vl (subset)");
    CHECK((out.os_safe & F_AVX) != 0, "R4 avx family still safe (os state ok)");

    /* ═══ R5: 全 AVX-512 (F/CD/BW/DQ/VL 齐) + OS 保存 ZMM — 正测 ═══ */
    mk_raw(&raw, ECX1_FMA | ECX1_SSE41 | ECX1_SSE42 | ECX1_AVX, EDX1_SSE2,
           EBX7_BMI1 | EBX7_BMI2 | (1u << 5) | EBX7_AVX512F | EBX7_AVX512CD |
               EBX7_AVX512BW | EBX7_AVX512DQ | EBX7_AVX512VL,
           0xE6, 1);
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R5 classify");
    CHECK((out.os_safe & F_512) == F_512, "R5 avx512 subset os_safe");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, F_512) == 1, "R5 avx512 pass");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_AVX512VL) == 1, "R5 avx512vl pass");
    CHECK(acs_cap_os_saves_avx512_state_v1(&out) == 1, "R5 zmm state saved");

    /* ═══ R6: 全 AVX-512 hw 但 OS 只保存 XMM|YMM (xcr0=0x6) — OS ZMM state 缺拒绝 ═══ */
    mk_raw(&raw, ECX1_FMA | ECX1_SSE41 | ECX1_SSE42 | ECX1_AVX, EDX1_SSE2,
           EBX7_BMI1 | EBX7_BMI2 | (1u << 5) | EBX7_AVX512F | EBX7_AVX512CD |
               EBX7_AVX512BW | EBX7_AVX512DQ | EBX7_AVX512VL,
           0x6, 1);
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R6 classify");
    CHECK((out.hw_features & F_512) == F_512, "R6 hw full avx512");
    CHECK((out.os_safe & F_512) == 0, "R6 os_safe no avx512 (no zmm os state)");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, ACS_CAP_FEAT_AVX512F) == 0, "R6 reject avx512f (os zmm)");
    CHECK(acs_cap_os_saves_avx512_state_v1(&out) == 0, "R6 no zmm saved");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, F_AVX2) == 1, "R6 avx2 still safe (xmm/ymm ok)");

    /* ═══ R7: 非法 required 位拒绝 + required=0 通过 + ABI 失配 ═══ */
    mk_raw(&raw, ECX1_SSE41, EDX1_SSE2, 0, 0, 0);
    CHECK(acs_cap_classify_v1(&raw, &out) == ACS_CAP_OK, "R7 classify");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, 0) == 1, "R7 required=0 pass");
    CHECK(acs_cap_os_safe_satisfies_v1(&out, (uint64_t)1 << 40) == 0, "R7 illegal bit reject");
    raw.abi_version = 0;
    acs_cap_result_v1 out2;
    memset(&out2, 0, sizeof(out2));
    CHECK(acs_cap_classify_v1(&raw, &out2) == ACS_CAP_ERR_ABI_MISMATCH, "R7 abi mismatch classify");
    raw.abi_version = ACS_CAP_ABI_VERSION_V1;
    memset(&out2, 0, sizeof(out2));
    out2.abi_version = 0; /* 探测 API 侧 ABI 拒 */
    CHECK(acs_cap_detect_v1(&out2) == ACS_CAP_ERR_ABI_MISMATCH, "R7 abi mismatch detect");

    /* ═══ R8: 本机实测 (os_safe ⊆ hw; SSE2 恒在; 层次自洽) ═══ */
    {
        acs_cap_result_v1 self;
        memset(&self, 0, sizeof(self));
        self.abi_version = ACS_CAP_ABI_VERSION_V1;
        int rc = acs_cap_detect_v1(&self);
        CHECK(rc == ACS_CAP_OK || rc == ACS_CAP_ERR_UNSUPPORTED, "R8 detect rc");
        if (rc == ACS_CAP_OK) {
            CHECK((self.hw_features & (self.os_safe)) == self.os_safe, "R8 os_safe subset hw");
            CHECK((self.os_safe & ACS_CAP_FEAT_SSE2) != 0, "R8 sse2 always");
            if ((self.os_safe & ACS_CAP_FEAT_AVX2) != 0) {
                CHECK((self.os_safe & ACS_CAP_FEAT_AVX) != 0, "R8 avx2 implies avx");
            }
            /* feature 名双平面一致性: hw 与 os_safe 集合名可序列化 */
            char buf[8192];
            size_t need = acs_cap_serialize_json_v1(&self, buf, sizeof(buf), NULL, 0);
            CHECK(need > 100, "R8 serialize need");
            CHECK(strstr(buf, "\"os_safe\":") != NULL, "R8 serialize os_safe present");
            /* 双次序列化逐字节一致 (稳定 schema) */
            char buf2[8192];
            size_t need2 = acs_cap_serialize_json_v1(&self, buf2, sizeof(buf2), NULL, 0);
            CHECK(need == need2 && strcmp(buf, buf2) == 0, "R8 serialize stable");
        }
    }

    if (failures == 0) {
        printf("CPU-001 feature matrix: ALL PASS (R1 baseline no-avx reject, R2 no-os-state "
               "reject, R3 avx2 pass, R4 avx512 subset-missing reject, R5 avx512 pass, "
               "R6 os-zmm-missing reject, R7 illegal/abi, R8 live self-consistency)\n");
        return 0;
    }
    fprintf(stderr, "CPU-001 feature matrix: FAIL (%d)\n", failures);
    return 1;
}
