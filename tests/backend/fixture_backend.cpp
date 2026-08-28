// tests/backend/fixture_backend.cpp — 合法 backend DSO fixture(ABI-002 加载测试)
// 导出 astrocs_backend_get_api_v1: handshake OK / 0 kernel / self_test OK。
#include "astrocs/common_abi_v1.h"

#include <cstring>

namespace {

acs_status fx_self_test(const astrocs_host_services_v1* host) {
    if (host == nullptr || host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    void* p = host->allocator.alloc(host->allocator.user_data, 32, 16);
    if (!p) return ACS_ERR_NOMEM;
    host->allocator.free(host->allocator.user_data, p);
    return ACS_OK;
}

astrocs_backend_api_v1 g_api = {};

astrocs_backend_api_v1* fx_api() {
    if (g_api.struct_size != sizeof(astrocs_backend_api_v1)) {
        std::memset(&g_api, 0, sizeof(g_api));
        g_api.struct_size = static_cast<uint32_t>(sizeof(astrocs_backend_api_v1));
        g_api.abi_version = ACS_ABI_VERSION_V1;
        std::strncpy(g_api.backend_id, "fixture", sizeof(g_api.backend_id) - 1);
        g_api.kernel_count = 0;
        g_api.kernels = nullptr;
        g_api.self_test = &fx_self_test;
    }
    return &g_api;
}

}  // namespace

extern "C" int astrocs_backend_get_api_v1(uint32_t host_abi_version,
                                          uint32_t host_struct_size,
                                          const astrocs_host_services_v1* host,
                                          astrocs_backend_api_v1* out_api) {
    if (host_abi_version != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (host_struct_size != sizeof(astrocs_host_services_v1)) return ACS_ERR_ABI_MISMATCH;
    if (!host || !out_api) return ACS_ERR_PARAM;
    std::memcpy(out_api, fx_api(), sizeof(astrocs_backend_api_v1));
    return ACS_OK;
}
