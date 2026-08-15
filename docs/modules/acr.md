# Module: acr

## 职责

异构计算运行时：kernel registry、CPU/GPU 调度、device executor、
route calibration、cuda bridge（phase2 集成 CPU reference + CUDA）。

## 非职责

不改变业务算法语义；CPU reference 是权威科学语义。

## Public API

astro/compute/*.hpp（acr.hpp、kernel_registry、task_traits、topology）。

## Data contract

kernel 描述/路由配置/硬件 profile。

## Ownership

runtime 句柄 RAII；device buffer 显式所有权。

## Thread safety

work_pool 调度；dispatcher 纯 CPU 回退；partial 契约（V19R2 前已修）。

## Errors

设备不可用 → CPU fallback；kernel 失败 → 显式状态。

## Science IDs

依赖 phase2（ALG-UPM-* 等）；无独立科学定义。

## 性能特征

benchmark route estimator；qualification 矩阵。

## Tests

unit/classic/fault/integration 套件；cuda 不可用 GTEST_SKIP。

## Known limitations

GPU kernel 仅 phase2 legacy launcher 注册（W9）。

## Source files

lib/acr/（api/backends/core/cost/diagnostics/profile/routing/scheduler/
tools/tests）。
