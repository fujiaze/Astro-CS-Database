// lib/backend_host/avx_backend.cpp — AVX(无 FMA)变体 backend (ISA-002)
// 编译隔离: 本 TU 用局部编译旗标 -mavx 构建(不含 -mfma/-mavx2);
// 主 CLI 与 baseline TU 不受污染(各自独立编译, opcode scanner 分别把关)。
// 共享合同: kernel 实现与 baseline 共用 baseline_kernels_impl.inc/backend_table.inc
// 同一源(零复制漂移), 仅 ISA 旗标与 backend_id 不同。
#define ASTROCS_BACKEND_ID "avx"

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
