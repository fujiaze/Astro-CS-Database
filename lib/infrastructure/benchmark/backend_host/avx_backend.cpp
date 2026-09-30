// lib/infrastructure/benchmark/backend_host/avx_backend.cpp — AVX(无 FMA)变体 backend (ISA-002)
//
// 【退役登记 · 未进构建目标】档位决策 = NOT_SHIPPED(见 docs/engineering/ISA_VARIANTS.md §1
// 表「AVX（无 FMA）」行：AVX 是 AVX2+FMA 的严格指令集子集，受控热点增益被严格主导，无独立收益)。
// 本文件当前**不被任何 CMake target 引用**(根 CMakeLists.txt 只登记 baseline/avx2/avx512 的
// 门面 TU + 计算面 TU)，故不进产品安装树、不参与运行期选路。保留原因：作为「AVX 档位曾被评估」
// 的可核对的源码面，随 ISA 变体家族同目录留存。
//
// 【为何不可直接复用】若将来要让本档位重新进图，须先补三件事，否则会静默退化为非法指令：
//   ① TU 级隔离(R-60/ISA_VARIANTS §3)：本文件仍是**单 TU** 形态 —— 门面与计算面同处一个 TU，
//      即 `baseline_kernels_impl.inc` + `backend_table.inc` 一并在此编译。该形态下握手入口
//      `acsd_backend_get_api_v1` 与计算面同 TU，「能力预检不过 ⇒ 干净拒绝」会退化为
//      「加载即撞非法指令」。avx2/avx512 已按门面 TU(零 ISA 旗标) + 计算面 TU(局部旗标) 拆开。
//   ② 能力声明：本文件未定义 `ACSD_BACKEND_REQUIRED_FEATURES`，backend_table.inc 会回落到
//      默认值 0 ⇒ 该 DSO 将自陈「不要求任何 ISA 特性」。一旦被 manifest 收录，在任何不支持
//      AVX 的主机上都能通过预检(backend_loader.cpp 的 required ⊆ detected 恒真)，首调 kernel
//      即 SIGILL。avx2/avx512 均显式声明所需位。
//   ③ 档位决策本身：须先在对应主机复测证明存在超越 AVX2+FMA 的独立收益。
// 处置口径：AGENTS §6「退役代码从代码库删除，或保留统一注释块写明原因」= 本注释块。
// 删除本文件的建议已登记在治理审核包（只提建议，本车道不删文件）。

// 编译隔离: 本 TU 用局部编译旗标 -mavx 构建(不含 -mfma/-mavx2);
// 主 CLI 与 baseline TU 不受污染(各自独立编译, opcode scanner 分别把关)。
// 共享合同: kernel 实现与 baseline 共用 baseline_kernels_impl.inc/backend_table.inc
// 同一源(零复制漂移), 仅 ISA 旗标与 backend_id 不同。
#define ACSD_BACKEND_ID "avx"

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

#include "backend_table.inc"
