// lib/infrastructure/benchmark/backend_host/avx2_backend_kernels.cpp
//   — AVX2+FMA 变体的**计算面 TU**（唯一带 AVX2/FMA 旗标的 TU; R-60）
//
// 为什么与门面 TU 分开（工具链约束，不是风格选择）:
//   · GCC/Clang 有**函数级**指令集覆盖（见 cpu/avx512/src/avx512_provider.cpp 的
//     ACS_CPU_AVX512_CAP_GATE_NOEVEX 宏），Linux 腿可让整 TU 带 -mavx* 再把判定函数逐函数降回；
//   · MSVC **没有**函数级覆盖: pragma 全表无 target、__declspec 全表无 cpu_specific/
//     cpu_dispatch、/arch: 的作用域是**整个 TU**（TU 内任意函数，含握手/自检/结构体清零内联，
//     都可能被生成宽向量指令）⇒ 要让握手/自检入口保证在基线 ISA 上可执行，唯一可控手段是
//     让它与高 ISA 旗标**不在同一个 TU**。
//
// 平台旗标口径（根 CMakeLists.txt 是唯一登记点；官方依据见
// run/FINAL-07/审核包/工程/变体能力面一致性报告.md §2）:
//   GCC/Clang: -mavx2 -mfma
//   MSVC     : /arch:AVX2（官方: AVX2 档 "enables use of Fused Multiply-Add (FMA) instructions"；
//              MSVC 没有独立的 /mfma 类开关，/arch 各档单调累积）
//              + /fp:contract（**仅 VS2022 起需要**，即 MSVC_VERSION>=1930: 该版起 /fp:precise
//              默认不生成 FMA 收缩，而 Linux 腿 GCC 默认 -ffp-contract=fast 会收缩；更早版本
//              /fp:contract 不存在——传了会被 D9002 静默忽略——且当时 /fp:precise 默认就收缩，
//              故按版本分支，不写会被静默忽略的旗标）
//
// 旗标失效不得静默（R-60 硬约束 2）: 下面的 #error 把「旗标被工具链忽略 / 工具链过老」
// 从「静默退化成与基线同码」变成**编译期红灯**——退回基线要显式改口径，不许悄悄发生。
#if defined(_MSC_VER) && !defined(__AVX2__)
#error "avx2 变体计算面 TU 未获得 AVX2 许可面: MSVC 需 /arch:AVX2（VS2013 Update 2 / MSVC 1800+ 支持）。工具链不支持时不得以基线同码产物冒充变体。"
#endif

#include "cpu_features.h"
#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

#include "baseline_kernels_impl.inc"

#include "backend_variant_kernels.h"

// 唯一跨 TU 桥: kernel 注册表（门面 TU 内）以本符号取用计算面实现。
extern "C" acs_status astrocs_variant_kernel_dispatch_v1(
        const astrocs_host_services_v1* host, const void* params, uint32_t params_bytes,
        const void* in, void* out) {
    return kernel_dispatch(host, params, params_bytes, in, out);
}
