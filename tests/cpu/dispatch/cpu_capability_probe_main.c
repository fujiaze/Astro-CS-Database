/* AstroCS CPU-001 probe — tests/cpu/dispatch/cpu_capability_probe_main.c
 *
 * 用途: 本机 AMD64 能力探测证据输出。stdout = 单行/单对象 JSON (稳定 schema,
 * providers/cpu/common/schemas/cpu_capability.schema.json); 供机器校验与审计。
 * stderr: 人类可读摘要 + os_safe 判定提示。exit: 0=探测成功 (非 x86 环境
 * 返回 5=ACS_CAP_ERR_UNSUPPORTED, 此时 stdout 为空)。
 *
 * 本 probe 不读取核心数; 不执行 AVX* 指令; 可重入。
 */
#include "astrocs/cpu/capability_v1.h"

#include <stdio.h>
#include <string.h>

static const char* yn(int v) { return v ? "yes" : "no"; }

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    acs_cap_result_v1 cap;
    memset(&cap, 0, sizeof(cap));
    cap.abi_version = ACS_CAP_ABI_VERSION_V1;
    const int rc = acs_cap_detect_v1(&cap);
    if (rc != ACS_CAP_OK) {
        fprintf(stderr, "acs_cap_detect_v1 rc=%d (unsupported arch?) no JSON output\n", rc);
        return rc == ACS_CAP_ERR_ABI_MISMATCH ? 2 : 5;
    }
    char buf[16384];
    const size_t need = acs_cap_serialize_json_v1(&cap, buf, sizeof(buf), NULL, 0);
    if (need == 0 || need >= sizeof(buf)) {
        fprintf(stderr, "serialize failed (need=%zu)\n", need);
        return 1;
    }
    fputs(buf, stdout);
    fputc('\n', stdout);

    /* stderr 摘要 (人类可读; 不作为 schema 校验输入) */
    fprintf(stderr, "vendor=%s brand=\"%s\" family=%u model=%u stepping=%u "
                    "osxsave=%s xcr0=0x%llx\n",
            cap.vendor, cap.brand, (unsigned)cap.family, (unsigned)cap.model,
            (unsigned)cap.stepping, yn((int)cap.osxsave),
            (unsigned long long)cap.xcr0);
    fprintf(stderr, "hw_features=0x%llx os_safe=0x%llx\n",
            (unsigned long long)cap.hw_features,
            (unsigned long long)cap.os_safe);
    fprintf(stderr,
            "judgement: avx_family_os_safe=%s avx512_subset_os_safe=%s "
            "os_saves_xmm_ymm=%s os_saves_opmask_zmm=%s\n",
            yn((int)((cap.os_safe & (uint64_t)ACS_CAP_GROUP_AVX_FAMILY) ==
               (uint64_t)ACS_CAP_GROUP_AVX_FAMILY)),
            yn((int)((cap.os_safe & (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET) ==
               (uint64_t)ACS_CAP_GROUP_AVX512_SUBSET)),
            yn((int)(cap.osxsave && (cap.xcr0 & 0x6u) == 0x6u)),
            yn((int)(cap.osxsave && (cap.xcr0 & 0xE0u) == 0xE0u)));
    return 0;
}
