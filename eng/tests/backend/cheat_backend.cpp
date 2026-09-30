// eng/tests/backend/cheat_backend.cpp — 故意错误 backend fixture (BENCH-002 验收)
// 校准 kernel 返回恒 0(错误)但极快; 合法 handshake/self_test(仅 Oracle 门可拦它)。
#include "acsd/common_abi_v1.h"
#include "baseline_kernels.h"

#include <cstring>

namespace {

acsd_status cheat_kernel(const acsd_host_services_v1* host,
                        const void* params, uint32_t params_bytes,
                        const void* in, void* out) {
    (void)in; (void)out;
    if (host == nullptr || host->struct_size != sizeof(acsd_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (params == nullptr || params_bytes != sizeof(acsd_baseline_params_v1)) return ACS_ERR_PARAM;
    auto* p = const_cast<acsd_baseline_params_v1*>(static_cast<const acsd_baseline_params_v1*>(params));
    const uint32_t n = p->w * p->h;
    for (uint32_t i = 0; i < n; ++i) p->out0.data[i] = 0.0f;   // 错误结果, 零成本
    p->workers_used = 1;
    return ACS_OK;
}

acsd_status cheat_self_test(const acsd_host_services_v1* host) {
    if (host == nullptr || host->struct_size != sizeof(acsd_host_services_v1))
        return ACS_ERR_ABI_MISMATCH;
    return ACS_OK;   // self_test 通过: 只有 Oracle 门能拦它(测 harness 的正确性筛选)
}

acsd_backend_api_v1 g_api = {};

acsd_backend_api_v1* cheat_api() {
    if (g_api.struct_size != sizeof(acsd_backend_api_v1)) {
        std::memset(&g_api, 0, sizeof(g_api));
        g_api.struct_size = static_cast<uint32_t>(sizeof(acsd_backend_api_v1));
        g_api.abi_version = ACS_ABI_VERSION_V1;
        std::strncpy(g_api.backend_id, "cheat", sizeof(g_api.backend_id) - 1);
        g_api.kernel_count = 1;
        static acsd_kernel_entry_v1 k;
        std::strncpy(k.science_contract_id, "ALG-001", sizeof(k.science_contract_id) - 1);
        std::strncpy(k.algorithm_id, "calibration-pixel-transform", sizeof(k.algorithm_id) - 1);
        std::strncpy(k.kernel_version, "1.0.0", sizeof(k.kernel_version) - 1);
        k.precision = ACS_PRECISION_F32;
        k.determinism_class = ACS_DET_BITWISE;
        k.fn = &cheat_kernel;
        g_api.kernels = &k;
        g_api.self_test = &cheat_self_test;
    }
    return &g_api;
}

}  // namespace

extern "C" int acsd_backend_get_api_v1(uint32_t host_abi_version,
                                          uint32_t host_struct_size,
                                          const acsd_host_services_v1* host,
                                          acsd_backend_api_v1* out_api) {
    if (host_abi_version != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (host_struct_size != sizeof(acsd_host_services_v1)) return ACS_ERR_ABI_MISMATCH;
    if (!host || !out_api) return ACS_ERR_PARAM;
    std::memcpy(out_api, cheat_api(), sizeof(acsd_backend_api_v1));
    return ACS_OK;
}
