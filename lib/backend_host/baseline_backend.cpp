// lib/backend_host/baseline_backend.cpp — baseline backend 参考实现(ABI-001)
// astrocs_backend_get_api_v1 唯一入口: handshake→静态 kernel 表→self_test/warmup/shutdown。
// kernel fn 科学实现属 ABI-003(当前返回 ACS_ERR_UNSUPPORTED, 注册结构先行)。
// 编译合同: 本 TU 可 -fno-exceptions 编译; 边界函数 catch-all 证明异常不跨边界。
#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>

#if !defined(ASTROCS_NO_EXCEPTIONS)
#include <stdexcept>
#endif

namespace {

#include "baseline_kernels_impl.inc"

}  // namespace

#include "backend_table.inc"

extern "C" {

/* 最近一次 kernel 调用的实际 worker 数(多线程观测辅助; 非科学接口) */
uint32_t astrocs_baseline_last_workers_used(void);

uint32_t astrocs_baseline_last_workers_used(void) {
    return g_last_workers_used.load(std::memory_order_relaxed);
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
