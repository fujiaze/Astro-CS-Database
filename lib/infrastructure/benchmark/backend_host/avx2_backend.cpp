// lib/infrastructure/benchmark/backend_host/avx2_backend.cpp — AVX2+FMA 变体 backend (ISA-001)
//   —— **门面 TU**（R-60 起只承载握手/自检/kernel 注册表；计算面已拆到 avx2_backend_kernels.cpp）
//
// 编译隔离（R-60 硬约束 1）: 本 TU 以**基线旗标**编译（根 CMakeLists.txt 对本 target 不给任何
// ISA 旗标），因此 astrocs_backend_get_api_v1 / backend_self_test / kernel 表在**任何** AMD64 上
// （含仅 SSE2 的主机）都可安全执行 —— 这是「能力预检不过 ⇒ 干净拒绝、不加载」而非「加载即撞
// 非法指令」的前提。带 ISA 旗标的只有计算面 TU（-mavx2 -mfma / MSVC: /arch:AVX2 + /fp:contract）。
// 为什么必须按源文件隔离: MSVC 没有函数级指令集覆盖（GCC 侧靠函数级属性），细节见
// avx2_backend_kernels.cpp 头注释与本目录 backend_variant_kernels.h。
//
// 共享合同（零复制漂移，未变）: kernel 实现与 baseline 共用 baseline_kernels_impl.inc（在计算面
// TU 内编译），注册表/自检与 baseline 共用 backend_table.inc（在本 TU 内编译）。
#define ASTROCS_BACKEND_ID "avx2"

#include "cpu_features.h"
// CPU-001: AVX2+FMA 分别声明(ISA-001) — required features = AVX2 | FMA
#define ASTROCS_BACKEND_REQUIRED_FEATURES (ACS_FEAT_AVX2 | ACS_FEAT_FMA)

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

// 计算面桥（唯一跨 TU 符号）: kernel 注册表里的 &kernel_dispatch 指向计算面 TU 的实现。
#include "backend_variant_kernels.h"
#define kernel_dispatch astrocs_variant_kernel_dispatch_v1

#include "backend_table.inc"

#undef kernel_dispatch
