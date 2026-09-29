/* ACSD cpuprov 变体 DSO 的「门面 TU ↔ 计算面 TU」唯一跨 TU 桥 (R-60 配方)
 * lib/infrastructure/benchmark/cpu/common/include/astrocs/cpu/cpuprov_kernels_v1.h
 *
 * 消费者: lib/infrastructure/benchmark/cpu/{avx2,avx512}/src/{avx2,avx512}_provider.cpp
 *         （门面 TU, **零 ISA 旗标**）与
 *         lib/infrastructure/benchmark/cpu/{avx2,avx512}/src/{avx2,avx512}_kernels.cpp
 *         （计算面 TU, **唯一带目标 ISA 旗标**）。
 *
 * 为什么需要这道桥（与第一族 backend 变体同因, run/FINAL-07/审核包/工程/
 * 变体能力面一致性报告.md §2）:
 *   · GCC/Clang 有**函数级**指令集覆盖（本族 avx512 的
 *     ACS_CPU_AVX512_CAP_GATE_NOEVEX target 属性即此机制）；
 *   · MSVC/clang-cl **没有**: pragma 全表无 target、__declspec 全表无
 *     cpu_specific/cpu_dispatch、/arch: 的作用域是**整个 TU** —— TU 内任意函数
 *     （含 query 握手、self_test、结构体清零的编译器内联）都可能被生成宽向量指令。
 *     ⇒ 改前 cpuprov 两族的 query/self_test/cap_gate 与热点 kernel 同 TU，在
 *     「不支持该 ISA 的主机」上会在**判定自身之前**撞非法指令（鸡生蛋），
 *     MSVC 腿更是连 ISA 旗标都没有（变体与基线同码却仍声明能力）。
 *   ⇒ 唯一可控手段: 把计算面移到**另一个源文件**，两者只经本符号相连。
 *
 * ABI 不变: 本符号**不属** provider ABI（module_api_v1.h 的
 * astrocs_provider_query_v1 / 表项类型 / kernel 注册条数条序 / params POD
 * 布局逐条不变），只是 DSO 内部的 TU 间入口；GCC/Clang 下以 hidden visibility
 * 声明 ⇒ 不进 .so 动态符号表（Linux 侧产物导出面逐条不变）。
 * 注意（Windows）: 该 DSO 以 WINDOWS_EXPORT_ALL_SYMBOLS 构建 ⇒ MSVC 会把本符号
 * 一并列入导出表（与第一族 backend 变体的 astrocs_variant_kernel_dispatch_v1
 * 同款处置，见变体能力面一致性报告 §2；加载器按**名字**取
 * astrocs_provider_query_v1，导出表多一条不改变任何加载/选路语义）。
 *
 * 签名 = 计算面唯一入口（本族门面 TU 的 run_banded 逐行带调用）:
 *   params/kidx/in/out 与 baseline/avx2/avx512 三份 kernel_pixel_range 同型；
 *   [i0,i1) 是 host executor 划分的输出行带（每输出元素独立 ⇒ bitwise 不随
 *   worker 数变化，ARCH-004 §4 语义不变）。 */
#ifndef ASTROCS_CPU_CPUPROV_KERNELS_V1_H
#define ASTROCS_CPU_CPUPROV_KERNELS_V1_H

#include <stdint.h>

#include "astrocs/cpu/baseline_provider_v1.h"   /* acs_cpu_baseline_params_v1 (CPU-002 冻结) */

#if defined(_MSC_VER)
#define ASTROCS_CPUPROV_BRIDGE_API   /* 见上文 Windows 导出面备注 */
#else
#define ASTROCS_CPUPROV_BRIDGE_API __attribute__((visibility("hidden")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

ASTROCS_CPUPROV_BRIDGE_API
void astrocs_cpuprov_kernel_range_v1(const acs_cpu_baseline_params_v1* params,
                                     uint32_t kidx,
                                     const float* const* in, float* const* out,
                                     uint64_t i0, uint64_t i1);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_CPU_CPUPROV_KERNELS_V1_H */
