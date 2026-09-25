// lib/infrastructure/benchmark/backend_host/avx512_backend.cpp — AVX512F 变体 backend (ISA-004)
// 编译隔离: 本 TU(整个可选 DSO)用局部编译旗标 -mavx512f -mavx512bw -mavx512vl -mavx512dq 构建;
// 主 CLI 与 baseline TU 不受污染(各自独立编译, opcode scanner 分别把关)。
// 共享合同: kernel 实现与 baseline 共用 baseline_kernels_impl.inc/backend_table.inc
// 同一源(零复制漂移), 仅 ISA 旗标与 backend_id 不同。
#define ASTROCS_BACKEND_ID "avx512"

#include "cpu_features.h"
// CPU-001 / TRUTHFUL-CONCLUSION-01: 声明必须与**编译旗标**逐位同源 ——
// 本 TU 由根 CMakeLists.txt 以 -mavx512f -mavx512bw -mavx512vl -mavx512dq 编译，
// 故 required = F|BW|DQ|VL。原声明只写 ACS_FEAT_AVX512F，理由写作"硬件上 F 与 DQ/BW/VL
// 共存" —— 该前提**是错的**：AVX-512F 不蕴含 BW/DQ/VL（KNL 一类的 F+CD+ER+PF 机器即
// 有 F 而无 BW/DQ/VL），于是"声明通过、加载放行、执行撞非法指令"。同仓另一处早已写明
// 这条规则：lib/infrastructure/benchmark/cpu/avx512/include/astrocs/cpu/avx512_provider_v1.h
// 「不能只看一个 AVX512F=true」。三者（声明／编译／检测）现由 CHK-ISA-SAME-SOURCE 逐位把关。
#define ASTROCS_BACKEND_REQUIRED_FEATURES (ACS_FEAT_AVX512_PROVIDER_REQUIRED)

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

#include "backend_table.inc"
