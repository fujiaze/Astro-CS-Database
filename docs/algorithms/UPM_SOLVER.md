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
F3: Huber IRLS (标准无量纲残差, 对齐 upm.cpp:200-210,619-629):
    z = r/sigma_eff; r = value − M − C; sigma_eff=max(|uncertainty|,sigma_floor)
    loss(z)=0.5z² if |z|≤δ else δ(|z|−0.5δ);  w(z)=1 if |z|≤δ else δ/|z|
    δ=1.345 (无量纲, 单位=sigma_eff), iterative reweight + 弱零锚 + 平滑
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

## 11 数据布局

- 输入：control observations（`value, uncertainty, snr_available` per control），帧 ivar 产品；
  `frame_id[i]` 与 `control_by_id[control_id]` 索引（`upm.cpp:618`）。
- 图：frame-control 二分图邻接 + 连通分量（`upm.cpp:86,419`），每分量独立 gauge（参考帧=最小 frame_id,
  C=0）；无观测几何节点用 sentinel（`SIZE_MAX`, `upm.cpp:220`）。
- 解：`M` per control（公共场）、`C[frame][control]` 校正；双线性 8×8 θ_f（每帧 θ）；
  `w[i]=raw_w[i]⊗huber_w`（`upm.cpp:629`）。
- 权重/字典：`raw_w` per-control 归一化 + `control_ivar`；弱零锚 `zero_anchor_weight=1e-3`。
- 持久化：`parameter_rows[index] ↔ frame_id_by_index[index]` 同长无重复（`SCI-UPM-PERSIST-001`）；
  `g_model_floor`/绑定仅由稳定 frame_id 决定（`aio_upm.cpp`）。
- 内存：O(n_ctrl + n_frame·n_ctrl)；双线性 θ = O(8×8·frame) 量级。

## 12 误差预算

- FP64 全链路；Huber IRLS 坐标下降稳态收敛（`upm.cpp:497-693`）。
- 弱零锚 `0.001`：正则化偏移 <~0.1%；帧绑定幂等门：`save→open` 重开值 `max_abs==0`
  （dense/sparse `1e-12` 等价门）；`k_corr=1.4`（MC 实测 1.3883）保守冻结。
- `control_variance=k_corr·(π/2)·σ_bg²/N_retained`，`control_ivar=1/var`；污染观测经
  `sigma_eff=max(|uncertainty|,sigma_floor)` 与无量纲 δ=1.345 强降权（`upm.cpp:619-629`）。
- 误差排序：**数值 FP64(≪1e-12) ≪ 科学/统计容差(k_corr 冻结, 控制噪声) ≪ 门禁**。
- 各 F 映射：`F1`→`p2_upm_raw_weight`/`p2_upm_normalized_weights`（`UPMW-001..003`）；
  `F3`→`upm.cpp:200-210,619-629`（Huber, `UPMW-*`）；`F4`→`p2_upm_calibrate_block`；
  `F5`→分量 gauge（`upm.cpp:631`）。
