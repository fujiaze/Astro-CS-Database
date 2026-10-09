# UPM Solver Algorithms (ALG-UPM)

> 上游：ACSD_DESIGN.md [D-1]「信号面（UPM）」一节（信号面与统一相对模型）
> 行号与并行表述按源码实测登记；实现级合同见 ALG-P2-UPM-IMPL-001 (docs/science/algorithms/PHASE2_UPM_IMPL.md)

## 1 上游 SCI 与输入输出

- 上游: `SCI-UPM-001..010, SCI-UPM-WEIGHT-001, SCI-UPM-PERSIST-001`
- 输入: control观测 P2ControlObservation[] + P2UpmConfig (robust/weight/anchors/连通分量)
- 输出: Model C[frame][control] + frame_index + frame_id_by_index + components + hash

## 2 离散公式

```text
F1: w_UPM = quality·control_ivar（绝对式，ADU⁻²，唯一生产式）或
    qf·support^p·snr²/(1+snr²)/unc²（非生产：use_ivar_weight=0 的 ablation/诊断域）
    天光控制点的被估量是**变化的背景电平**，SNR² 在该处不是有效逆方差代理，
    生产一律取 control_ivar，SNR 只作 veto/质量门（docs/science/sky/UPM.md[S-1]「公式与推导」一节）
    control_ivar=1/(k_corr·π/2·σ²/N_retained)，定义域 1 < k_corr
    （k_corr=1 ⇔ 忽略相关，p2_upm_control_variance 显式拒 rc=2；k_corr<1 拒 rc=1
    ）
F2: w_cell = w_UPM / Σ_cell w_UPM · control_reliability（份额式，无量纲，实锚 `lib/algorithms/coverage/src/upm.cpp` 归一化注释），
    Σ_cell w_cell = control_reliability；求解器实际消费的就是它）
F3: Huber IRLS (标准无量纲残差, 对齐 `lib/algorithms/coverage/src/upm.cpp` 的 Huber rho/IRLS 段):
    z = r/sigma_eff; r = value − M − C; sigma_eff=max(|uncertainty|,sigma_floor)
    loss(z)=0.5z² if |z|≤δ else δ(|z|−0.5δ);  w(z)=1 if |z|≤δ else δ/|z|
    δ=1.345 (无量纲, 单位=sigma_eff), iterative reweight + 弱零锚 + 平滑
F4: 内核求值 calibrated = raw − C(frame, leaf) 双线性 8×8（分块、仅供 fit 侧与诊断，不在生产扣除链上；
    生产扣除 = raw − δ_k，δ_k 为每帧相对公共连续信号面的低阶偏差，见 PHASE2_SAMPLER §5.11）
F5: 連通分量 gauge = min frame_id per component, harmonic continuation 单帧区
F6: hash = SHA256(C), persist: sparse json + dense cache materialize, 1e-12等价
```

来源（实测复核）: `lib/algorithms/coverage/src/upm.cpp`（冻结头注释 / Huber rho·w / Huber IRLS / `p2_upm_build` / raw weight / per-control 归一化 / sparse persist）、`lib/algorithms/coverage/src/sampler.cpp`（control_variance·control_ivar，k_corr 承接 ALG-UPM-CONTROL-IVAR-001）

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
| k_corr<1（越域：N_eff>N_retained 物理不可达） | rc=1（`p2_upm_control_variance`）/ rc=7（`p2_upm_ma_build` provenance） |
| frame_id重复 | 去重, persist同长校验 |
| NaN weight | INVALID_INPUT |
| 无观测 | NO_DATA |
| 迭代耗尽（未达 tolerance） | `p2_upm_build` rc 仍 0；只读访问器 `p2_upm_convergence` 报 `converged=0`（M7-H-101，收敛结论以 `converged` 为准，rc=0 只表构建成功） |

## 5 确定性与归约

- 求解串行reference; **无 OpenMP**（实测 grep `#pragma omp` 于 upm.cpp 零命中）。并行=std::thread 池五段（均在 `lib/algorithms/coverage/src/upm.cpp`）: raw+归一化聚合、Huber w、M 更新、C 更新逐帧 CG、dense materialize (kChunk=16); worker-local 分块 + 按 tid 升序合并（确定性三档见[S-1]「判据与误差」一节：同配置重复位精确；跨 worker 数 1e-12，非位精确——`compute_raw` 的 per-control 求和结合顺序随 worker 数变化）, worker 数=cfg.cpu_workers (Runtime lease 唯一来源, `lib/phase2_session/p2_session.cpp`; 无 hardware_concurrency 硬件探测, 同 `upm.cpp`); IRLS 迭代顺序固定。

## 6 复杂度

- IRLS O(iter·(obs+K log K)) K=controls

## 7 CPU-only 后端策略（V5）

- 仅 CPU：IRLS 求解串行为确定性 reference；块级(C frame×control)求值可 worker pool（按 affinity, **线程数取自 benchmark profile**）, 控制索引固定顺序归约；dense/sparse 1e-12 等价门保留(实现自检, 非跨后端)。

## 5c SIMD 安全与取消点

- 残差/Huber 权重逐观测独立(SIMD 安全: 数组连续无别名)；加权法方程累加=**观测索引固定序归约**(FP64, 禁重结合)；弱零锚为固定行附加, 与并行无关。
- 取消点: IRLS 迭代间检查; 取消时不写 Model/persist, 返回未完成状态(半成品 hash 不产生)——模型原子性以整模型为单元。

## 8 参考实现/Oracle

- dense==sparse 1e-12; G1空间真值; PR-UPM绑定门; UPMW-001..007权重门

## 9 容差来源

- 1e-12 (dense/sparse)，预冻结；
- **跨 worker 数（1..N）= 1e-12 绝对容差**（实测 ΔC_max=2.22e-15 ≈ 1 ulp @10 ADU，
  见 `docs/science/sky/UPM.md` [S-1]「公式与推导」一节）；**同配置重复 = 位精确 +
  `model_hash` 逐字相同**（`synthetic_gate.cpp` CON-009 门）；门的等价面 = 同配置重复（跨后端等价不在门内）。

## 10 关联 ARC/API/TST

- API: upm.h: p2_upm_build/calibrate_block/raw_weight; API 面=API-P2-UPM-001 (PUBLIC_API.md 尚未落页, 登记于ALG-P2-UPM-IMPL-001 [S-1]「判据与误差」一节）
- TST: PR-UPM-001..010, UPMW-001..007; TEST-P2-UPM-001/002 设计冻结=ALG-P2-UPM-IMPL-001 [S-1]「判据与误差」一节
- 实现级合同: docs/science/algorithms/PHASE2_UPM_IMPL.md (ALG-P2-UPM-IMPL-001, 逐符号锚/DISP 登记/DISP-P2UPM-001..004)

## 11 数据布局

- 输入：control observations（`value, uncertainty, snr_available` per control），帧 ivar 产品；
  `frame_id[i]` 与 `control_by_id[control_id]` 索引（`lib/algorithms/coverage/src/upm.cpp`）。
- 图：frame-control 二分图邻接 + 连通分量（`lib/algorithms/coverage/src/upm.cpp`），每分量独立 gauge（参考帧=最小 frame_id,
  C=0）；无观测几何节点用 sentinel（`SIZE_MAX`, 同上文件）。
- 解：`M` per control（公共场）、`C[frame][control]` 校正；双线性 8×8 θ_f（每帧 θ）；
  `w[i]=raw_w[i]⊗huber_w`（`lib/algorithms/coverage/src/upm.cpp`）。
- 权重/字典：`raw_w` per-control 归一化 + `control_ivar`；弱零锚 `zero_anchor_weight=1e-3`。
- 持久化：`parameter_rows[index] ↔ frame_id_by_index[index]` 同长无重复（`SCI-UPM-PERSIST-001`）；
  `g_model_floor`/绑定仅由稳定 frame_id 决定（`aio_upm.cpp`）。
- 内存：O(n_ctrl + n_frame·n_ctrl)；双线性 θ = O(8×8·frame) 量级。

## 12 误差预算

- FP64 全链路；Huber IRLS 坐标下降稳态收敛（`lib/algorithms/coverage/src/upm.cpp`）。
- 弱零锚 `0.001`：正则化偏移 <~0.1%；帧绑定幂等门：`save→open` 重开值 `max_abs==0`
  （dense/sparse `1e-12` 等价门）；`k_corr` = k_gauss(N_retained)×k_geo 两因子几何查表
  （公式与查表由 P3 单元 实验/healpix-polar 承载；
  1.3883 = 标定几何专属单次 MC 实测值（带 1.27–1.43），冻结 1.4 在声明域两端
  低估 32%/约 2 倍，仅作实现记录与域外回退；证据源 `control_median_mc_test`
  **已注册（可复跑）**）。
- `control_variance=k_corr·(π/2)·σ_bg²/N_retained`，`control_ivar=1/var`；污染观测经
  `sigma_eff=max(|uncertainty|,sigma_floor)` 与无量纲 δ=1.345 强降权（`lib/algorithms/coverage/src/upm.cpp` 的 Huber rho/IRLS 段；δ 默认见同文件）。
- 误差排序：**数值 FP64(≪1e-12) ≪ 科学/统计容差(k_corr 查表回退, 控制噪声) ≪ 门禁**。
- 各 F 映射：`F1`→`p2_upm_raw_weight`/`p2_upm_normalized_weights`（`UPMW-001..003`）；
  `F3`→`lib/algorithms/coverage/src/upm.cpp`（Huber，δ 默认同文件，`UPMW-*`）；`F4`→`p2_upm_calibrate_block`；
  `F5`→分量 gauge（同上文件）。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/sky/UPM.md（信号面科学正本 SCI-UPM-001）。
> - [S-4] docs/DERIVATIONS-P3.md（相关科学正本）。
> - [A-1] docs/science/algorithms/mosaic/PHASE2_UPM_IMPL.md（UPM 实现级合同分册）。
> - [S-5] 实验/healpix-polar/（实验单元读数）。

## 参考文献与参考代码库（含许可证）

- Huber IRLS：Huber 1964, Ann. Math. Statist. 35, 73；Huber & Ronchetti 2009, Robust Statistics 2nd ed., Wiley。
- 弱零锚：Tikhonov 1963, Soviet Math. Dokl. 4, 1035（正则化弱零锚的原始出处）。
- 多帧相对定标：SCAMP（GPL-3.0；Bertin 2006, ASPC 351, 112）；Padmanabhan et al. 2008, ApJ 674, 1217。
- 稀疏信号面样条（目标表示）：Duchon 1977；Wahba 1990。
- var(median)≈πσ²/(2N)：Hoaglin et al. 1983（SCI-UPM [S-1]「判据与误差」一节）；实证比值读数（N=5 的 Var(median)/渐近式直接定征）正本 = `实验/healpix-polar/`docs/audit/kcorr/tables.md` [S-5]「输入输出端口」一节。
- k_corr 为 k_gauss(N)×k_geo 两因子几何查表（P3 单元承载）；MC 证据源 control_median_mc_test 可复跑；域外回退值与实现记录 = `实验/healpix-polar/docs/DERIVATIONS-P3.md` [S-4]「推导补遗」一节。
