// lib/infrastructure/benchmark/backend_host/avx512_backend.cpp — AVX-512 变体 backend (ISA-004)
//   —— **门面 TU**（R-60 起只承载握手/自检/kernel 注册表；计算面已拆到 avx512_backend_kernels.cpp）
//
// 编译隔离（R-60 硬约束 1）: 本 TU 以**基线旗标**编译，因此 acsd_backend_get_api_v1 /
// backend_self_test / kernel 表在任何 AMD64 上（含仅 SSE2 的主机）都可安全执行；带 ISA 旗标的
// 只有计算面 TU（-mavx512f/-bw/-vl/-dq / MSVC: /arch:AVX512）。
// 为什么必须按源文件隔离: MSVC 没有函数级指令集覆盖，见 avx512_backend_kernels.cpp 头注释。
//
// 共享合同（零复制漂移，未变）: kernel 实现与 baseline 共用 baseline_kernels_impl.inc（在计算面
// TU 内编译），注册表/自检与 baseline 共用 backend_table.inc（在本 TU 内编译）。
#define ACSD_BACKEND_ID "avx512"

#include "cpu_features.h"
// CPU-001 / TRUTHFUL-CONCLUSION-01 / R-60: 声明必须与**编译许可面**逐位同源，且按工具链精确:
//   · GCC/Clang: 计算面 TU 以 -mavx512f -mavx512bw -mavx512vl -mavx512dq 编译
//     ⇒ 许可面 = F|BW|DQ|VL（无 CD），与 Linux 腿既有声明逐位相同（928，未变）；
//   · MSVC: 计算面 TU 以 /arch:AVX512 编译，而 MSVC **没有**子集档位旗标 —— 官方语
//     (/arch (x64) 预定义宏段 + C++ 团队博客《AVX-512 Auto-Vectorization in MSVC》):
//     /arch:AVX512 的许可面 = F + CD + BW + DQ + VL（五个、且仅此五个）
//     ⇒ 声明必须含 CD，否则「用了却没声明」: 一台有 F/BW/DQ/VL 而无 CD 的主机会被放行。
//   · 平台精确值由**编译器自己的预定义宏**推得（MSVC /arch:AVX512 与 clang-cl 对应档位定义
//     __AVX512CD__；GCC 腿的四子集旗标不定义它）⇒ 声明 = 编译器实际被许可发射的子集，不重不漏。
// 原订正记录(保留): 旧声明只写 ACS_FEAT_AVX512F，理由写作"硬件上 F 与 DQ/BW/VL 共存" —— 该
// 前提**是错的**：AVX-512F 不蕴含 BW/DQ/VL（KNL 一类的 F+CD+ER+PF 机器即有 F 而无 BW/DQ/VL），
// 于是"声明通过、加载放行、执行撞非法指令"。三者（声明／编译／检测）现由 CHK-ISA-SAME-SOURCE
// 逐位把关，并由 eng/tools/quality/check_variant_isa_disasm.py 在**产物级**双向复核。
#if defined(__AVX512CD__)
#define ACSD_BACKEND_REQUIRED_FEATURES \
    (ACS_FEAT_AVX512_PROVIDER_REQUIRED | ACS_FEAT_AVX512CD)
#else
#define ACSD_BACKEND_REQUIRED_FEATURES (ACS_FEAT_AVX512_PROVIDER_REQUIRED)
#endif

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

// 计算面桥（唯一跨 TU 符号）: kernel 注册表里的 &kernel_dispatch 指向计算面 TU 的实现。
#include "backend_variant_kernels.h"
#define kernel_dispatch acsd_variant_kernel_dispatch_v1

#include "backend_table.inc"

#undef kernel_dispatch
