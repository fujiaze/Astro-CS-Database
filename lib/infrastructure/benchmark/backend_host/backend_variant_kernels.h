// lib/infrastructure/benchmark/backend_host/backend_variant_kernels.h
//   — 变体 DSO 的「门面 TU ↔ 计算面 TU」唯一跨 TU 桥 (R-60)
//
// 变体 DSO 由两个 TU 组成（R-60: 非 GCC 工具链没有函数级指令集覆盖 ⇒ 只能按源文件隔离）:
//   · 门面 TU   avx2_backend.cpp / avx512_backend.cpp  —— **基线旗标**编译:
//               astrocs_backend_get_api_v1 / backend_self_test / kernel 注册表；
//   · 计算面 TU avx2_backend_kernels.cpp / avx512_backend_kernels.cpp —— **目标 ISA 旗标**编译:
//               共享 kernel 实现 baseline_kernels_impl.inc。
// 两侧共享同一份实现源与同一份注册表 .inc（零复制漂移不变）。
#ifndef ASTROCS_BACKEND_VARIANT_KERNELS_H
#define ASTROCS_BACKEND_VARIANT_KERNELS_H

#include "astrocs/common_abi_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 变体 DSO 计算面唯一入口（= baseline_kernels_impl.inc 的 kernel_dispatch 逐一转发）。
 *
 * 为什么需要这道桥: baseline_kernels_impl.inc 把 kernel_dispatch 定义在匿名命名空间里
 * （同 TU 唯一 + 内部链接），跨 TU 取不到；而 kernel 注册表的函数指针必须指向**计算面**
 * 的实现（否则门面 TU 自己编出的基线版 kernel_dispatch 会被注册进去，变体就退化成同码）。
 * 故由计算面 TU 以本符号显式导出，门面 TU 以宏别名把它填进注册表。
 *
 * ABI 不变: 表项类型/条数/条序/backend_id/precision/determinism 与 baseline 完全一致，
 * 只有函数地址指向另一个 TU 的实现（同一份 impl 源，仅 ISA 旗标不同）。 */
acs_status astrocs_variant_kernel_dispatch_v1(const astrocs_host_services_v1* host,
                                              const void* params, uint32_t params_bytes,
                                              const void* in, void* out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_BACKEND_VARIANT_KERNELS_H */
