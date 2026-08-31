// lib/backend_host/avx512_backend.cpp — AVX512F 变体 backend (ISA-004)
// 编译隔离(05 §2): 本 TU(整个可选 DSO)用局部编译旗标 -mavx512f -mavx512bw -mavx512vl -mavx512dq 构建;
// 主 CLI 与 baseline TU 不受污染(各自独立编译, opcode scanner 分别把关)。
// 共享合同: kernel 实现与 baseline 共用 baseline_kernels_impl.inc/backend_table.inc
// 同一源(零复制漂移), 仅 ISA 旗标与 backend_id 不同。
#define ASTROCS_BACKEND_ID "avx512"

#include "cpu_features.h"
// CPU-001: AVX512 实际使用子集 F/DQ/BW/VL (ISA-004) — required = AVX512F 检测面位
// (硬件上 F 与 DQ/BW/VL 共存; 加载匹配以 avx512f 为准, 子集声明在 manifest)
#define ASTROCS_BACKEND_REQUIRED_FEATURES (ACS_FEAT_AVX512F)

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
