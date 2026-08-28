// backend_table.inc — kernel 注册表+生命周期函数+get_api(baseline/variant DSO 共享)
// 共享合同纪律: 表与 self_test 来自同一 .inc, 变体仅编译旗标/backend_id 不同, 禁复制漂移。
#ifndef ASTROCS_BACKEND_TABLE_INC
#define ASTROCS_BACKEND_TABLE_INC

#ifndef ASTROCS_BACKEND_ID
#define ASTROCS_BACKEND_ID "baseline"
#endif
#ifndef ASTROCS_BACKEND_BUILD_ID
#ifdef NDEBUG
#define ASTROCS_BACKEND_BUILD_ID "release"
#else
#define ASTROCS_BACKEND_BUILD_ID "debug"
#endif
#endif

namespace {

const astrocs_kernel_entry_v1 kKernels[] = {
    {"ALG-001", "calibration-pixel-transform", "1.0.0", ACS_PRECISION_F32, ACS_DET_BITWISE, &kernel_dispatch},
    {"ALG-004", "noise-snr-reductions",        "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &kernel_dispatch},
    {"ALG-002", "wcs-psf-batch",               "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &kernel_dispatch},
    {"ALG-005", "drizzle-overlap",             "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &kernel_dispatch},
    {"ALG-005", "drizzle-accumulate",          "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &kernel_dispatch},
    {"ALG-005", "drizzle-normalize",           "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &kernel_dispatch},
    {"ALG-006", "upm-spmv",                    "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &kernel_dispatch},
    {"ALG-006", "upm-residual",                "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &kernel_dispatch},
    {"ALG-006", "upm-weight-update",           "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &kernel_dispatch},
    {"ALG-008", "rejection-statistics",        "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &kernel_dispatch},
    {"ALG-009", "integration-accumulate",      "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &kernel_dispatch},
    {"ALG-P3-002", "hips-bulk-transform",      "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &kernel_dispatch},
};

/* self_test: handshake→allocator 往返(计数可验证)→cancel 读→budget 租借/归还→logger。
 * reentrant=yes; threadsafe=yes; internal_parallel=none; 取消点=无(短任务)。 */
acs_status backend_self_test(const astrocs_host_services_v1* host) {
    if (host == nullptr) return ACS_ERR_PARAM;
    if (host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (host->allocator.alloc == nullptr || host->allocator.free == nullptr ||
        host->logger.log == nullptr || host->cancel.is_cancelled == nullptr ||
        host->budget.acquire == nullptr || host->budget.release == nullptr)
        return ACS_ERR_PARAM;
    // allocator 往返: alloc→写→free; align=64 校验对齐路径
    void* p = host->allocator.alloc(host->allocator.user_data, 128, 64);
    if (p == nullptr) return ACS_ERR_NOMEM;
    if ((reinterpret_cast<uintptr_t>(p) & 63u) != 0) {
        host->allocator.free(host->allocator.user_data, p);
        return ACS_ERR_SELFTEST;
    }
    std::memset(p, 0xAB, 128);
    host->allocator.free(host->allocator.user_data, p);
    if (host->cancel.is_cancelled(host->cancel.user_data)) return ACS_ERR_CANCELLED;
    if (host->budget.acquire(host->budget.user_data, 1) != 0) return ACS_ERR_BUDGET;
    host->budget.release(host->budget.user_data, 1);
    host->logger.log(host->logger.user_data, ACS_LOG_INFO, "baseline", "self_test ok");
    return ACS_OK;
}

acs_status backend_warmup(const astrocs_host_services_v1* host) {
    if (host == nullptr || host->struct_size != sizeof(astrocs_host_services_v1))
        return ACS_ERR_ABI_MISMATCH;
    return ACS_OK;  // baseline 无预热需求; 变体 backend 在此做缓存/页预备(ABI-002+)
}

acs_status backend_shutdown(const astrocs_host_services_v1* host) {
    if (host == nullptr || host->struct_size != sizeof(astrocs_host_services_v1))
        return ACS_ERR_ABI_MISMATCH;
    return ACS_OK;
}

astrocs_backend_api_v1 g_api = {};

astrocs_backend_api_v1* backend_api() {
    if (g_api.struct_size != sizeof(astrocs_backend_api_v1)) {
        std::memset(&g_api, 0, sizeof(g_api));
        g_api.struct_size = static_cast<uint32_t>(sizeof(astrocs_backend_api_v1));
        g_api.abi_version = ACS_ABI_VERSION_V1;
        std::strncpy(g_api.backend_id, ASTROCS_BACKEND_ID, sizeof(g_api.backend_id) - 1);
std::strncpy(g_api.backend_build_id, ASTROCS_BACKEND_BUILD_ID, sizeof(g_api.backend_build_id) - 1);
        g_api.required_features = 0;         // baseline: 最低 amd64(SSE2 基线), 无附加位
        g_api.detected_features = 0;
        g_api.alignment_bytes = 64;
        g_api.precision_class = ACS_PRECISION_F32;
        g_api.determinism_class = ACS_DET_FIXED_ORDER;
        g_api.aliasing_contract = 0;         // in/out 不重叠
        g_api.nested_parallel_allowed = 0;   // ARCH-004 §3
        g_api.kernel_count = static_cast<uint32_t>(sizeof(kKernels) / sizeof(kKernels[0]));
        g_api.kernels = kKernels;
        g_api.self_test = &backend_self_test;
        g_api.warmup = &backend_warmup;
        g_api.shutdown = &backend_shutdown;
    }
    return &g_api;
}

/* get_api: handshake 双验→填充静态表(05 §4)。
 * reentrant=yes; threadsafe=yes; internal_parallel=none。 */
}

extern "C" int astrocs_backend_get_api_v1(uint32_t host_abi_version,
                                          uint32_t host_struct_size,
                                          const astrocs_host_services_v1* host,
                                          astrocs_backend_api_v1* out_api) {
    if (host_abi_version != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (host_struct_size != sizeof(astrocs_host_services_v1)) return ACS_ERR_ABI_MISMATCH;
    if (host == nullptr || out_api == nullptr) return ACS_ERR_PARAM;
    if (host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    std::memcpy(out_api, backend_api(), sizeof(astrocs_backend_api_v1));
    return ACS_OK;
}

#endif  // ASTROCS_BACKEND_TABLE_INC
