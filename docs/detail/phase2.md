# Module: phase2

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

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
ALG-REJ-001..008、SCI-INT-001/002/004/008、SCI-NOISE-015、
SCI-UPM-WEIGHT-001、ALG-UPM-CONTROL-IVAR-001、DATA-UPM-CONTROL-UNC-001
（合同冻结集合）。

## ABI 接口（P2ControlObservation 尾部新增 control_variance/control_ivar）

- p2_upm_raw_weight：production=quality×control_ivar（缺 control ivar
显式 rc=2）；use_ivar_weight=0 走 snr² ablation 诊断域；
- sampler：uncertainty=SE(patch median)，N_retained + k_corr=1.4
  （标定读数与条件见 `实验/healpix-polar/`）；obs.ivar 弃用为诊断；
- stage2：use_ivar_weight 显式透传（默认 1）；weight_policy=ivar 时
   ACR 块强制 CPU（ACR-IVAR-001）；ivar 产品整体缺失 = 硬科学错误；
- integration：零权重合法（ZERO_VALID_WEIGHT），NaN/Inf/负 INVALID；
  reducer 不持有权重键（policy/reducer 分离）。

## 性能特征

block planner 内存估算；dense cache 加速求值。

## 缓存

UPM dense cache（model_hash 校验，stale=2）。

## Diagnostics

P2.* stage 日志；astrocs-diagnose 支持。

## Tests

synthetic_gate 组（含 UPMW 组）；G5 ivar 真值；SNR-015 ablation；
control-median MC 在 healpix_drizzle/tests/control_median_mc_test.cpp；
k_corr 规范取值 1.4（读数与条件见 `实验/healpix-polar/`）。

## Known limitations

ACR 仅 `mosaic_reject_legacy` CPU launcher（无 CUDA kernel）；输出仅 signal/support。

## Source files

lib/algorithms/coverage/{src,lib/include/astro/phase2,tools,tests}/。

## Coverage 子模块合同

- coverage 环节合同（ALG-COV-001 / DATA-COV-001 / API-COV-001、端口与缺陷登记）唯一正本 =
  `docs/science/algorithms/PHASE2_COVERAGE.md`；模块合同 = `lib/algorithms/coverage/`
  （README + module.yaml）；registry 登记页 = docs/detail/registry/astrocs.phase2.coverage.md。
  本页只留指针。
- 本模块归属边界：处理链阶段序为 coverage → sampler → …，非本模块归属；
  coverage/support/validity 三概念分离，coverage 禁作隐式科学权重
  （w_UPM 唯一冻结式 = `docs/science/PHASE2_UPM.md` §5）。
