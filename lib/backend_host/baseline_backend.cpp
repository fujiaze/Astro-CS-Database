// lib/backend_host/baseline_backend.cpp — baseline backend 参考实现(ABI-001)
// astrocs_backend_get_api_v1 唯一入口: handshake→静态 kernel 表→self_test/warmup/shutdown。
// kernel fn 科学实现属 ABI-003(当前返回 ACS_ERR_UNSUPPORTED, 注册结构先行)。
// 编译合同: 本 TU 可 -fno-exceptions 编译; 边界函数 catch-all 证明异常不跨边界。
#include "astrocs/common_abi_v1.h"

#include <cstdio>
#include <cstring>

#if !defined(ASTROCS_NO_EXCEPTIONS)
#include <stdexcept>
#endif

namespace {

/* kernel 表(05 §5 粒度; science_contract_id 锚定 ALG 合同)
 * 实现状态: ABI-003 落地前 fn 恒 ACS_ERR_UNSUPPORTED(显式拒绝, 不静默)。 */
acs_status k_stub(const astrocs_host_services_v1* host,
                  const void* params, uint32_t params_bytes,
                  const void* in, void* out) {
    (void)params; (void)params_bytes; (void)in; (void)out;
    if (host == nullptr || host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (host->cancel.is_cancelled != nullptr && host->cancel.is_cancelled(host->cancel.user_data))
        return ACS_ERR_CANCELLED;
    return ACS_ERR_UNSUPPORTED;
}

const astrocs_kernel_entry_v1 kKernels[] = {
    {"ALG-001", "calibration-pixel-transform", "1.0.0", ACS_PRECISION_F32, ACS_DET_BITWISE, &k_stub},
    {"ALG-004", "noise-snr-reductions",        "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &k_stub},
    {"ALG-002", "wcs-psf-batch",               "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &k_stub},
    {"ALG-005", "drizzle-overlap",             "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &k_stub},
    {"ALG-005", "drizzle-accumulate",          "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &k_stub},
    {"ALG-005", "drizzle-normalize",           "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &k_stub},
    {"ALG-006", "upm-spmv",                    "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &k_stub},
    {"ALG-006", "upm-residual",                "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &k_stub},
    {"ALG-006", "upm-weight-update",           "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &k_stub},
    {"ALG-008", "rejection-statistics",        "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &k_stub},
    {"ALG-009", "integration-accumulate",      "1.0.0", ACS_PRECISION_F32, ACS_DET_FIXED_ORDER, &k_stub},
    {"ALG-P3-002", "hips-bulk-transform",      "1.0.0", ACS_PRECISION_F64, ACS_DET_BITWISE, &k_stub},
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
        std::strncpy(g_api.backend_id, "baseline", sizeof(g_api.backend_id) - 1);
#if defined(NDEBUG)
        std::strncpy(g_api.backend_build_id, "release", sizeof(g_api.backend_build_id) - 1);
#else
        std::strncpy(g_api.backend_build_id, "debug", sizeof(g_api.backend_build_id) - 1);
#endif
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

}  // namespace

extern "C" {

int astrocs_backend_get_api_v1(uint32_t host_abi_version,
                               uint32_t host_struct_size,
                               const astrocs_host_services_v1* host,
                               astrocs_backend_api_v1* out_api) {
    // handshake: 版本+宿主结构尺寸双验, 失配即拒(不猜布局)
    if (host_abi_version != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (host_struct_size != sizeof(astrocs_host_services_v1)) return ACS_ERR_ABI_MISMATCH;
    if (host == nullptr || out_api == nullptr) return ACS_ERR_PARAM;
    if (host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    std::memcpy(out_api, backend_api(), sizeof(astrocs_backend_api_v1));
    return ACS_OK;
}

/* ABI 边界验证(05 §4 "异常不跨边界"的机制证明):
 * mode=1: 内部抛异常→catch→ACS_ERR_INTERNAL; 其他: ACS_OK。
 * 编译 -fno-exceptions 时(异常不存在)恒返 ACS_OK。 */
int astrocs_abi_boundary_probe(int mode) {
#if defined(ASTROCS_NO_EXCEPTIONS)
    (void)mode;
    return ACS_OK;
#else
    try {
        if (mode == 1) throw std::runtime_error("boundary-probe");
        return ACS_OK;
    } catch (...) {
        return ACS_ERR_INTERNAL;   // 边界处转结构化错误码, 不外泄异常/STL 类型
    }
#endif
}

}  // extern "C"
