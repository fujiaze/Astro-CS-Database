# UPM Solver Algorithms (ALG-UPM)

> 上游 SCI: SCI-UPM-001..010, WEIGHT/PERSIST  状态: DERIVED (T206 冻结, 2026-08-23)  模块: phase2/upm

## 1 上游 SCI 与输入输出

- 上游: `SCI-UPM-001..010, SCI-UPM-WEIGHT-001, SCI-UPM-PERSIST-001`
- 输入: control观测 P2ControlObservation[] + P2UpmConfig (robust/weight/anchors/连通分量)
- 输出: Model C[frame][control] + frame_index + frame_id_by_index + components + hash

## 2 离散公式

```text
F1: w = quality·control_ivar (production) 或 qf·support^p·snr²/(1+snr²)/unc² (ablation)
    control_ivar=1/(k_corr·π/2·σ²/N_retained)
F2: per-control归一化: w_norm = w / Σw · geometric_reliability
F3: Huber IRLS: loss=0.5r² if |r|≤δ else δ|r|−0.5δ², δ=1.345·median_abs_r, iterative reweight + 弱零锚 + 平滑
F4: calibrated = raw − C(frame, leaf) 双线性 8×8
F5: 連通分量 gauge = min frame_id per component, harmonic continuation 单帧区
F6: hash = SHA256(C), persist: sparse json + dense cache materialize, 1e-12等价
```

来源: `upm.cpp:6,10-16,1107-1123` `sampler.cpp:672`

## 3 伪代码

```text
function p2_upm_build(observations, cfg):
  p2_upm_raw_weight(obs,cfg) → w if cfg.use_ivar else ablation
  if control_ivar≤0/nonfinite → rc=2 fail
  w_norm = w/Σw · geom per-control
  collect frames(set) → cell/control → 连通分量 (min frame_id gauge)
  Huber IRLS (w_norm, 弱零锚, 平滑) → C
  hash SHA256(C) → save sparse json / dense cache
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| control_ivar≤0/nonfinite | rc=2 build fail |
| frame_id重复 | 去重, persist同长校验 |
| NaN weight | INVALID_INPUT |
| 无观测 | NO_DATA |

## 5 确定性与归约

- 求解串行reference; 块级求值OpenMP按control索引固定顺序；IRLS迭代顺序固定。

## 6 复杂度

- IRLS O(iter·(obs+K log K)) K=controls

## 7 CPU/GPU

- 块级求值可GPU, 求解串行, dense/sparse 1e-12等价门。

## 8 参考实现/Oracle

- dense==sparse 1e-12; G1空间真值; PR-UPM绑定门; UPMW-001..007权重门

## 9 容差来源

- 1e-12 (dense/sparse), 预冻结。

## 10 关联 ARC/API/TST

- API: upm.h: p2_upm_build/calibrate_block/raw_weight
- TST: PR-UPM-001..010, UPMW-001..007
