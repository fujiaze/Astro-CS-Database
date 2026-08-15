# Module: phase2

## 职责

Phase2 多帧统一模型：coverage → sampler → UPM → block calibrate →
rejection → integration → HiPS 写/验证。生产入口 astrocs-stage2。

## 非职责

不做单帧校准/星点/PSF/plate solve（由 Phase1 帧 HiPS 提供输入）。

## Public API

astro/phase2/{upm,stage2_common,coverage,sampler,rejection,block,
integrate,acr_kernels}.h；P2_API extern "C"。

## Data contract

- 帧 HiPS：signal/support/SNR catalogue；
- UPM sparse：astrocs-upm-v2（DATA-UPM-MODEL-001）；
- 产品：signal/support HiPS（variance/ivar 可选诊断）。

## Ownership

UPM model 由 p2_upm_build 创建 → p2_upm_close 释放；失败路径已统一释放。

## Thread safety

求值/块校准 OpenMP；模型只读后并行；无共享 mutable。

## Errors

ERR-P2-UPM-001（畸形模型）；UNDERDETERMINED/NO_CANDIDATES/
ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT 状态。

## Config

stage2 JSON（模型/integration/output）；typed parser + schema 单源。

## Science IDs

SCI-UPM-001..010、SCI-UPM-PERSIST-001、ALG-UPM-FRAME-BIND-001、
ALG-REJ-001..008、SCI-INT-001/002/004/008、SCI-NOISE-015。

## 性能特征

block planner 内存估算；dense cache 加速求值。

## 缓存

UPM dense cache（model_hash 校验，stale=2）。

## Diagnostics

P2.* stage 日志；astrocs-diagnose 支持。

## Tests

synthetic_gate 82 项（含 PR-UPM-001..010）；G5 ivar 真值；
SNR-015 ablation。

## Known limitations

W9 ACR 仅 legacy CPU launcher（无 CUDA kernel）；输出仅 signal/support。

## Source files

lib/phase2/{src,include/astro/phase2,tools,tests}/。
