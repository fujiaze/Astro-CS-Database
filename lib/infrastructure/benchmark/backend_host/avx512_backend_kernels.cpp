// lib/infrastructure/benchmark/backend_host/avx512_backend_kernels.cpp
//   — AVX-512 变体的**计算面 TU**（唯一带 AVX-512 旗标的 TU; R-60）
//
// 拆 TU 的工具链依据与 avx2_backend_kernels.cpp 头注释同（MSVC 无函数级指令集覆盖 ⇒
// 按源文件隔离；门面 TU 承载握手/自检/注册表并以基线旗标编译）。
//
// 平台旗标口径（根 CMakeLists.txt 是唯一登记点）:
//   GCC/Clang: -mavx512f -mavx512bw -mavx512vl -mavx512dq（既有口径，未变）
//   MSVC     : /arch:AVX512 —— MSVC **没有**子集档位旗标，官方语（/arch (x64) 预定义宏段 +
//              C++ 团队博客《AVX-512 Auto-Vectorization in MSVC》）: /arch:AVX512 的许可面
//              = AVX512F + AVX512CD + AVX512BW + AVX512DQ + AVX512VL（五个、且仅此五个）。
//              版本门槛: VS2017 起「有限支持」，**VS2019 16.3（MSVC 1923）起自动向量化器才真正
//              支持 AVX-512**；官方未把版本钉到 _MSC_VER ⇒ 用下面的 #error 实测而不是猜版本号。
//
// 旗标失效不得静默（R-60 硬约束 2）: 不支持的 /arch: 取值在 MSVC 下只是 D9002 警告并**被忽略**
// ⇒ 必须靠编译器自己的预定义宏实测; MSVC 不定义 __AVX512F__ 即判为「未获得许可面」并编译失败。
#if defined(_MSC_VER) && !defined(__AVX512F__)
#error "avx512 变体计算面 TU 未获得 AVX-512 许可面: MSVC 需 /arch:AVX512（自动向量化面自 VS2019 16.3 / MSVC 1923 起）。工具链不支持时不得以基线同码产物冒充变体。"
#endif

#include "cpu_features.h"
#include "acsd/common_abi_v1.h"
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
extern "C" acsd_status acsd_variant_kernel_dispatch_v1(
        const acsd_host_services_v1* host, const void* params, uint32_t params_bytes,
        const void* in, void* out) {
    return kernel_dispatch(host, params, params_bytes, in, out);
}
