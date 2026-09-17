// p1noise_abi_layout_probe — MASK-002 (claim SC-009) C ABI 布局探针
//
// 先例: lib/infrastructure/aio/tests/abi/aio_abi_layout_probe.cpp (SCI-FIX-AIO SC-006)。
// 目的: 把「跨边界结构首部 = struct_size@0 + abi_version@4」这一纪律变成**可执行断言**:
//   1) 静态断言偏移/类型/版本常量 (编译期, 布局被改动即编译失败);
//   2) 运行时打印 JSON 布局 (偏移/sizeof/版本常量), 由 p1noise_abi_layout_check.py
//      与锁定文件 p1noise_abi_layout_lock.json 比对 (失配即 rc=1)。
// 生产源零改动: 本探针只 include 生产头, 不链接生产实现。
#include "snr_estimator.h"

#include <cstddef>
#include <cstdio>
#include <type_traits>

#define P1NOISE_OFF(T, F) \
    static_assert(offsetof(T, F) == 0 || offsetof(T, F) == 4, \
                  #T "." #F " 必须在偏移 0 或 4 (ASTROCS_DESIGN 7.3 版本化 C ABI)")
#define P1NOISE_HEAD(T)                                                     \
    static_assert(std::is_same<decltype(T::struct_size), uint32_t>::value,  \
                  #T ".struct_size 必须为 uint32_t");                       \
    static_assert(std::is_same<decltype(T::abi_version), uint32_t>::value,  \
                  #T ".abi_version 必须为 uint32_t");                       \
    static_assert(offsetof(T, struct_size) == 0, #T ".struct_size 必须在偏移 0"); \
    static_assert(offsetof(T, abi_version) == 4, #T ".abi_version 必须在偏移 4")

P1NOISE_HEAD(SnrNoiseModelConfig);
P1NOISE_HEAD(NoiseWeightModelV1);

int main(void) {
    std::printf("{\n");
    std::printf("  \"abi_mismatch_rc\": %d,\n", (int)SNR_ABI_MISMATCH);
    std::printf("  \"config\": {\"sizeof\": %zu, \"struct_size_off\": %zu, "
                "\"abi_version_off\": %zu, \"abi_version\": %u},\n",
                sizeof(SnrNoiseModelConfig),
                offsetof(SnrNoiseModelConfig, struct_size),
                offsetof(SnrNoiseModelConfig, abi_version),
                (unsigned)SNR_NOISE_CONFIG_ABI_VERSION);
    std::printf("  \"model\": {\"sizeof\": %zu, \"struct_size_off\": %zu, "
                "\"abi_version_off\": %zu, \"abi_version\": %u},\n",
                sizeof(NoiseWeightModelV1),
                offsetof(NoiseWeightModelV1, struct_size),
                offsetof(NoiseWeightModelV1, abi_version),
                (unsigned)SNR_NOISE_MODEL_ABI_VERSION);
    std::printf("  \"config_fields\": {\"mask_k_sigma\": %zu, \"mask_r_min_px\": %zu, "
                "\"mask_fwhm_floor_scale\": %zu, \"mask_budget_min_patches\": %zu, "
                "\"mask_budget_min_sky\": %zu},\n",
                offsetof(SnrNoiseModelConfig, mask_k_sigma),
                offsetof(SnrNoiseModelConfig, mask_r_min_px),
                offsetof(SnrNoiseModelConfig, mask_fwhm_floor_scale),
                offsetof(SnrNoiseModelConfig, mask_budget_min_patches),
                offsetof(SnrNoiseModelConfig, mask_budget_min_sky));
    std::printf("  \"model_fields\": {\"mask_degraded\": %zu, \"mask_radius_p50\": %zu, "
                "\"mask_frac\": %zu}\n",
                offsetof(NoiseWeightModelV1, mask_degraded),
                offsetof(NoiseWeightModelV1, mask_radius_p50),
                offsetof(NoiseWeightModelV1, mask_frac));
    std::printf("}\n");
    return 0;
}
