# Phase2 UPM (Control Photometry) Science (SCI-UPM)

> ID: SCI-UPM-001  范围: SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 + SCI-UPM-PERSIST-001  状态: FROZEN (T106 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-UPM-001..  模块: phase2 (upm/sampler)

## 1 目的与非目标

- **目的**：在多帧覆盖并集上建立唯一的联合加性光度模型 UPM，消除逐帧背景/零点差，使校准后样本 `calibrated = raw − C_f(p)` 在全域可比；提供控制权重与持久化 frame_id 绑定。
- **非目标**：不处理乘性尺度差（已撤销）；不做 Drizzle 方差估计（SCI-NOISE）；不做最终加权积分与排异判定（SCI-INT/SCI-REJ）；不跨滤镜统一（filter 分组由调用方保证）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `f, frame_id` | 帧标识（稳定科学 payload uint64，`DATA-FRAME-ID-001`） | `upm.cpp:sampler.cpp` |
| `C_f(p)` | 帧 f 在像素 p 的加性校正场（8×8 control cell 双线性） | `calibrate_block` |
| `θ` | UPM 控制系数向量（每帧每 control cell 一值） | `parameter_rows` |
| `raw` | 校准前样本 patch median | `P2ControlObservation` |
| `control_variance` | `k_corr·(π/2)·σ_bg²/N_retained` | `ALG-UPM-CONTROL-IVAR-001` |
| `control_ivar` | `1/control_variance` | `p2_upm_raw_weight` |
| `quality_factor` | 质量因子（cosmic/geom 质量） | `SCI-UPM-WEIGHT-001` |
| `geometric_reliability` | 几何可靠性（单 control 覆盖度） | `p2_upm_normalized_weights` |
| `k_corr` | Drizzle 相关校正 1.4 | `sampler.cpp:672` |
| `N_retained` | clipping 后保留样本数 | `P2ControlObservation` |
| `parameter_rows[index]` | 第 index 帧的 θ 行 | `upm.cpp:parameter_rows` |
| `frame_id_by_index[index]` | 第 index 帧的稳定 id | `frame_id_by_index` |

## 3 物理量和单位

- `C, raw, calibrated, σ_bg`: ADU；`control_variance`: ADU²；`control_ivar`: ADU⁻²；`quality, geom`: 无量纲 [0,1]；`w_UPM`: ADU⁻²；`N_retained`: 无量纲；`frame_id`: 无量纲 uint64；`θ`: ADU。

## 4 输入有效域

- 帧数 `n_frames ≥2` 且至少一 control cell 有 `≥2` 帧 clean 覆盖，否则 harmonic continuation 填单帧区；`n_control_points` 可为 0（→ NO_DATA）。
- `control_ivar` 有效要求 `use_ivar_weight=1` 时 `control_ivar>0` 且有限，否则 `p2_upm_raw_weight rc=2 → build rc=2`（`DATA-UPM-CONTROL-UNC-001`）。
- `k_corr` 为 `frames[f].kcorr>0 ? per-frame : cfg.control_k_corr`，缺省 1.4（`sampler.cpp:672`）。
- `parameter_rows` 与 `frame_id_by_index` 同长、无重复，绑定仅由稳定 `frame_id` 决定，禁止容器遍历重建。

## 5 连续定义

```text
加性校正:
  calibrated_f(p) = raw_f(p) − C_f(p)
  C_f(p) = 双线性(8×8 control cell, θ_f)

科学权重 (V19R3 冻结 SCI-UPM-WEIGHT-001):
  w_UPM = quality_factor × geometric_reliability × control_ivar
  raw = quality × control_ivar ; normalized = raw / Σraw · geom (per-control)
  control_ivar = 1 / control_variance
  control_variance = k_corr × (π/2) × σ_bg² / N_retained
  # control estimator = patch median (非单 leaf)
  # N_retained = clipping 后保留数 (非 n_total)
  # k_corr = 1.4 保守冻结 (MC 实测 1.3883, control_median_mc_test, pixfrac=0.8, 2000次)
  # N_eff = N_retained / k_corr
  # 禁 production 乘 star SNR / snr²/(1+snr²) / support^p；support 仅 eligibility/coverage
  # legacy snr²/(1+snr²)/unc² 仅 use_ivar_weight=0 ablation/诊断 (SNR-015)

求解 (UPM_SOLVER.md):
  Huber IRLS + control-ivar 感知权重 + 弱零锚 (zero_anchor_weight=0.001) + 连通分量独立 gauge
  每分量参考帧 = 最小 frame_id

持久化绑定 (SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001):
  parameter_rows[index] ↔ frame_id_by_index[index]   # 同长、无重复
  绑定仅由稳定 frame_id 决定；save→close→open 保持 frame_id→θ 映射
  payload = truncated-64 canonical SHA-256 of science payload (DATA-FRAME-ID-001)
```

与 `lib/phase2/src/upm.cpp:6-493,1107-1123`、`sampler.cpp:250-364,672`、`aio_upm.cpp:4` 一致。

## 6 假设

- 帧间无乘性尺度差（乘性 photometric scale 已撤销）；控制点 SNR 与几何解耦（`snr_available` 语义 V4 R6）；控制采样 patch 足域近似高斯；Drizzle 相关可用 `k_corr≥1` 表征。

## 7 独立不变量

- **常量场不变量（SCI-004 gauge 对齐）**：常数**公共**输入（各帧同值 `raw_f=C`）时 `M=C`、`C_f=0`（每分量参考帧 gauge；弱零锚微调除外），全 control cell 无空间梯度——**不写 `C_f=C`**。仅当各帧存在独立零点差时 `C_f` 才吸收 per-frame offset（参考帧之外）。
- **空 control 不传播**：无合格 control 时不产伪 `C_f`，显式 NO_DATA。
- **Huber 对称性（无量纲标准化）**：残差先标准化 `z=r/sigma_eff`，其中 `r=value−M−C`，`sigma_eff=max(|uncertainty|,sigma_floor)`；`Huber(δ=1.345)` 作用于无量纲 `z`：小残差区 `loss=0.5z²`(等价 L2)，大残差区 `loss=δ|z|−0.5δ²`(L1)，位置估计对称。δ=1.345 无量纲（单位=sigma_eff）。
- **frame_id 绑定幂等**：`save→open` 后 `parameter_rows[index]` 重开值 `max_abs==0`（`dense/sparse 1e-12` 等价门）。
- **k_corr 缩放**：`control_variance` 随 `k_corr` 线性缩放，`N_eff` 反比缩放。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无重叠/无 control | `NO_DATA` | `upm.cpp:6` 域检查 |
| `use_ivar_weight=1` 且 `control_ivar≤0/非有限` | `p2_upm_raw_weight rc=2 → build rc=2` | `DATA-UPM-CONTROL-UNC-001` |
| `S=0` (MAD=0) | 该 patch 方差为 0，不计 control | `noise_model` |
| 单帧区 | harmonic continuation 填 | `phase2` 域 |
| 畸形模型文件 | `ERR-P2-UPM-001` | `aio_upm.cpp` |
| 跨 endian payload | 理论 id 不同，当前仅 Linux x86_64 路径 | `sampler.cpp:250` |

## 9 精度策略

- FP64 求解；`dense cache` 与 `sparse` 求值 `1e-12` 等价门（`SparseEqualsDense`）；`control_median_mc_test` 的 `k_corr` 用 MC 校准后保守取整 `1.4`。

## 10 不可接受变化

- 在生产 `w_UPM` 中乘 `star SNR / support^p / obs.ivar`(已弃用)；
- 将 `control_estimator` 改为单 leaf 或 `N_total` 替代 `N_retained`；
- 改变 `k_corr` 默认 1.4 或 `parameter_rows ↔ frame_id` 绑定语义；
- 在 `calibrate_block` 中引入乘性尺度。

## 11 验证 Oracle

- **UPMW 硬门 7 项**：001 snr 扰动不变、002 ivar 1:4→weight 1:4、003 星群不变、004 `Var(median)≈πσ²/2N`、005 MC `k_corr≈1.3883`、006 无 legacy SNR consumer 且缺 ivar `rc=2`、007 patch 真值恢复（`control_median_mc_test, synthetic_gate`）。
- **持久化门**：`UpmPersistAllPermutations/RandomStableIds/SparseDenseBinding` 等 PR#1 全排列。
- **不变量门**：常量场、空 control、Huber 对称、绑定幂等四门。
- **Python 参考**：NumPy 对同 `raw` 的 Huber IRLS + `control_ivar` 权重复算 `θ`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-UPM-001` `p2_upm_build` Huber IRLS 求解
- `ALG-UPM-002` `p2_upm_calibrate_block` 双线性校正
- `ALG-UPM-003` `p2_upm_raw_weight / p2_upm_normalized_weights` 权重
- `ALG-UPM-004` `aio_upm` 持久化绑定

## 13 追溯与测试

- 权威文件: `docs/science/PHASE2_UPM.md` (SCI-UPM-001..010, SCI-UPM-WEIGHT-001, SCI-UPM-PERSIST-001)
- 实现: `lib/phase2/src/upm.cpp` (1107-1123, 493-510), `lib/phase2/src/sampler.cpp` (250-364, 672), `lib/astro_image_io/src/aio_upm.cpp` (持久化)
- 公开 API: `p2_upm_build, p2_upm_calibrate_block, p2_upm_raw_weight, p2_upm_open/save`
- 测试: `TEST-UPMW-001..007, UPMW-001..007, UpmPersist*` (`synthetic_gate.cpp, control_median_mc_test.cpp`)

## 3a 坐标 frame

UPM 在**像素域 control cell**（8×8 双线性网格）上工作，无 WCS/天球参与；帧绑定唯一由稳定 `frame_id`（truncated-64 SHA-256，DATA_SEMANTICS §5）决定，容器索引仅实现细节；每连通分量参考帧=最小 `frame_id`（gauge 锚，§5）。

## 9a 专属问题回答（SCI-005 指定问题逐项）

- **观测方程**：`calibrated_f(p) = raw_f(p) − C_f(p)`，`C_f(p)`=帧 f 的加性校正场（8×8 control cell 双线性插值，§5）；**纯加性模型**（乘性尺度差已撤销，§1 非目标）；`raw`=校准前样本 patch median。
- **控制点**：8×8 control cell；control estimator=patch median（非单 leaf）；`N_retained`=clipping 后保留样本数（非总数）。
- **光度面 basis**：分块常数加性背景面——每帧每 control cell 一个自由度 `θ_f`，经双线性插值成连续场；自由度 = n_frames × n_control_points。
- **正则化**：Huber IRLS 鲁棒求解 + control-ivar 感知权重 + **弱零锚** `zero_anchor_weight=0.001`（弱 Tikhonov/岭型锚定向全局零）。
- **gauge/退化**：加性场对全局常数规范自由——以连通分量独立 gauge 固定（参考帧=最小 frame_id）；退化路径：无 ≥2 帧 clean 覆盖 → harmonic continuation 填单帧区；`control_ivar≤0/非有限` → `rc=2` 显式拒（§4）。
- **接缝指标**：接缝=C_f 场跨帧差在 cell 边界的不连续残余；量化门槛**预冻结于 SYN-005**（已知低阶光度面+重叠图：参数恢复、残差、接缝降低且不破坏星 flux；本合同登记映射，禁止"视觉可接受"替代）。

## 14 Primary literature（引用定位声明）

1. **本合同为项目原创推导**（观测方程/光度面 basis/gauge/接缝语义均为 Project-defined，无外部公式依赖）。
2. Huber IRLS：Huber, P. J. 1964, "Robust Estimation of a Location Parameter", Ann. Math. Statist. 35, 73——文章级定位（bibcode 1964AnMS...35...73H，未逐页核验），仅 robust 求解框架上下文。
3. 弱零锚=弱 Tikhonov 正则：Tikhonov 解的正则化概念——教科书级，无公式引用。
4. `k_corr=1.4`（MC 实测 1.3883，pixfrac=0.8，2000 次）：**项目自产 MC 证据**（`control_median_mc_test`），非外部文献。
5. Tukey/MAD 常数：复用 SCI-002/SCI-003 文献链（PMC6768164 实证；Φ⁻¹(3/4) 恒等式）。

## 15 Acceptance

- §11 Oracle 全过（含 `control_median_mc_test` MC 一致性、gauge 唯一性、harmonic continuation 边界）；
- §7 不变量门全过；
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-005 转换：已知低阶光度面恢复、重叠图 gauge/退化强度扫描、接缝指标预冻结门槛（SYN-005 数据与不变量表），参数恢复/残差/接缝降低且不破坏星 flux 全过。
