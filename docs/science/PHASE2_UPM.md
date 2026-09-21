# Phase2 UPM (Control Photometry) Science (SCI-UPM)

> 上游：ASTROCS_DESIGN.md §5.4（天光平面与统一相对模型）

> ID: SCI-UPM-001  范围: SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 + SCI-UPM-PERSIST-001  状态: FROZEN (T106 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-UPM-001..  模块: phase2 (upm/sampler)

## 1 目的与非目标

- **目的**：在多帧覆盖并集上建立唯一的联合加性光度模型 UPM，消除逐帧背景/零点差，使校准后样本 `calibrated = raw − C_f(p)` 在全域可比；提供控制权重与持久化 frame_id 绑定。
- **非目标**：不处理乘性尺度差（**本期决议：纯加性模型**，`g_k ≡ 1` 不启用，见 §14a）；不做 Drizzle 方差估计（SCI-NOISE）；不做最终加权积分与排异判定（SCI-INT/SCI-REJ）；不跨滤镜统一（filter 分组由调用方保证）。

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
| `control_reliability`（旧名 `geometric_reliability`） | per-control 相对可靠度。**实现事实 = 配置常量**（`P2UpmBuildConfig.control_reliability`，默认 1.0，`upm.cpp:289-290`），**不是**按单 control 覆盖度算出的几何量；原「几何可靠性（单 control 覆盖度）」承诺**未实现**（缺陷登记 SC-005） | `p2_upm_normalized_weights` |
| `k_corr` | Drizzle 相关校正；定义域 **1 ≤ k_corr**，冻结默认 1.4（标定表 1.2112–2.8971 全在该域内） | `sampler.cpp:83,840` |
| `w_cell` | 求解器内 per-control 份额权重（无量纲，`Σ_cell w_cell = control_reliability`） | `upm.cpp:565` |
| `N_retained` | clipping 后保留样本数 | `P2ControlObservation` |
| `parameter_rows[index]` | 第 index 帧的 θ 行 | `upm.cpp:parameter_rows` |
| `frame_id_by_index[index]` | 第 index 帧的稳定 id | `frame_id_by_index` |

## 3 物理量和单位

- `C, raw, calibrated, σ_bg`: ADU；`control_variance`: ADU²；`control_ivar`: ADU⁻²；`quality, control_reliability`: 无量纲 [0,1]；`w_UPM`: **ADU⁻²（绝对逆方差量）**；`w_cell`: **无量纲（份额式，单位元 = 无量纲）**；`N_retained`: 无量纲；`frame_id`: 无量纲 uint64；`θ`: ADU。

## 4 输入有效域

- 帧数 `n_frames ≥2` 且至少一 control cell 有 `≥2` 帧 clean 覆盖，否则 harmonic continuation 填单帧区；`n_control_points` 可为 0（→ NO_DATA）。
- `control_ivar` 有效要求 `use_ivar_weight=1` 时 `control_ivar>0` 且有限，否则 `p2_upm_raw_weight rc=2 → build rc=2`（`DATA-UPM-CONTROL-UNC-001`）。
- `k_corr` 为 `frames[f].kcorr>0 ? per-frame : cfg.control_k_corr`，缺省 1.4（`sampler.cpp:857`）；
  **定义域 1 ≤ k_corr**：`p2_upm_control_variance` 与 `p2_upm_ma_build` 对 k_corr<1
  显式拒（`rc=1`/`rc=7`）——k_corr<1 ⇔ N_eff>N_retained，正相关样本的有效样本量
  不可能大于样本数（SC-005 / R-2 D-10）。
- 标定表 scale 维（300–600″/px）**已退役**：生产真帧源像素角尺度 0.9586″/px 在域外，
  域外一律显式回退冻结默认 1.4（禁 clamp 静默饱和）；域外帧保留 `kcorr=0` 并打
  `[sampler] k_corr 域外回退` 标（`sampler.cpp:560-574`）。
- `parameter_rows` 与 `frame_id_by_index` 同长、无重复，绑定仅由稳定 `frame_id` 决定，禁止容器遍历重建。

## 5 连续定义

```text
加性校正:
  calibrated_f(p) = raw_f(p) − C_f(p)
  C_f(p) = 双线性(8×8 control cell, θ_f)

科学权重 (V19R3 冻结 SCI-UPM-WEIGHT-001)：两级两个量，禁止混名——
  (1) 绝对式 w_UPM（科学定义，ADU⁻²，任何需要绝对精度的消费面：API/门/报告）
      w_UPM = quality_factor × control_reliability × control_ivar
  (2) 份额式 w_cell（求解器内归一约定，无量纲，per-control；实现消费的就是它）
      w_cell = w_UPM / Σ_cell w_UPM × control_reliability,  Σ_cell w_cell = control_reliability
  control_ivar = 1 / control_variance
  control_variance = k_corr × (π/2) × σ_bg² / N_retained
  # control estimator = patch median (非单 leaf)
  # N_retained = clipping 后保留数 (非 n_total)
  # k_corr = 1.4 保守冻结；1 ≤ k_corr（域外显式拒）；实证 1.3883 来自
  #   control_median_mc_test（**未注册：构建孤儿，见 §11/§13/§15 的 MISSING 登记**）
  # N_eff = N_retained / k_corr
  # 禁 production 乘 star SNR / snr²/(1+snr²) / support^p；support 仅 eligibility/coverage
  # legacy snr²/(1+snr²)/unc² 仅 use_ivar_weight=0 ablation/诊断 (SNR-015)

求解 (UPM_SOLVER.md):
  Huber IRLS + control-ivar 感知权重 + 弱零锚 (zero_anchor_weight=0.001) + 连通分量独立 gauge
  每分量参考帧 = 最小 frame_id

收敛与容差 (SCI-UPM-CONV-001；无量纲，禁绝对容差):
  tol_step = tolerance_step（默认 1e-6，相对量）
  tol_obj  = tolerance_obj（默认 1e-6，相对量）
  停止判据: max_dM / max(scale_obs, eps) < tol_step  且  |Δobj| / max(|obj_old|, eps) < tol_obj
  scale_obs = 观测量的尺度（control 观测值的稳健尺度），**不得**用 max|M|
  converged 状态枚举: 0 = max_iter / 1 = converged / 2 = stalled / 3 = invalid
  stalled 判据: 残差改善量低于数值地板连续 N_stall 次（可证伪，须有红/绿双向测试）
  ⚠ 绝对容差在 ~300 e⁻ 尺度永不收敛（300 次迭代 converged=0，SCI-C FIX-1）⇒ 生产必须走相对/尺度归一判据

持久化绑定 (SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001):
  parameter_rows[index] ↔ frame_id_by_index[index]   # 同长、无重复
  绑定仅由稳定 frame_id 决定；save→close→open 保持 frame_id→θ 映射
  payload = truncated-64 canonical SHA-256 of science payload (DATA-FRAME-ID-001)
```

与 `lib/algorithms/coverage/src/upm.cpp:6-500,1123-1139`、`sampler.cpp:257-371,689`、`aio_upm.cpp:4` 一致。

## 6 假设

- 帧间无乘性尺度差（**本期决议：纯加性模型**，`g_k ≡ 1`；乘性残留属低阶空间增益、归 Phase1，见 §14a）；控制点 SNR 与几何解耦（`snr_available` 语义 V4 R6）；控制采样 patch 足域近似高斯；Drizzle 相关可用 `k_corr≥1` 表征。

## 7 独立不变量

- **常量场不变量（SCI-004 gauge 对齐）**：常数**公共**输入（各帧同值 `raw_f=C`）时 `M=C`、`C_f=0`（每分量参考帧 gauge；弱零锚微调除外），全 control cell 无空间梯度——**不写 `C_f=C`**。仅当各帧存在独立零点差时 `C_f` 才吸收 per-frame offset（参考帧之外）。
- **空 control 不传播**：无合格 control 时不产伪 `C_f`，显式 NO_DATA。
- **Huber 对称性（无量纲标准化）**：残差先标准化 `z=r/sigma_eff`，其中 `r=value−M−C`，`sigma_eff=max(|uncertainty|,sigma_floor)`；`Huber(δ=1.345)` 作用于无量纲 `z`：小残差区 `loss=0.5z²`(等价 L2)，大残差区 `loss=δ|z|−0.5δ²`(L1)，位置估计对称。δ=1.345 无量纲（单位=sigma_eff）。
- **frame_id 绑定幂等**：`save→open` 后 `parameter_rows[index]` 重开值 `max_abs==0`（`dense/sparse 1e-12` 等价门）。
- **k_corr 缩放**：`control_variance` 随 `k_corr` 线性缩放，`N_eff` 反比缩放；定义域 1 ≤ k_corr。
- **两级权重三条性质（w_UPM 绝对式 vs w_cell 份额式）**：
  (i) 同一 control 内两观测的权比 = `w_UPM` 之比（两式在单元内一致）；
  (ii) 任何**公共**精度因子（含 `k_corr`）在份额式内严格消去（实测 model_hash 位相同）；
  (iii) 份额式**丢弃跨 control 精度**（单元总权恒 = `control_reliability`），在 λs>0
  时改变估计量 ⇒ 未同步换算 λs/λ0 不得改用绝对权（§10）。
- **并行确定性容差（三档，冻结）**：(a) 同配置重复构建 = **位精确** + `model_hash` 逐字相同
  （构造保证：连续块划分 + 每 k/每帧不相交写 + 无共享浮点累加器）；(b) **跨 worker 数
  （1..N）= 1e-12 绝对容差**，不是位精确 —— `compute_raw` 的 per-control 求和按 worker
  连续切片分块后按 worker 序合并，与串行索引序的结合顺序不同（FP 加法非结合）；
  实测 ΔC_max = 2.22e-15 ≈ 1 ulp @10 ADU（`run/PROJECT-GOVERNANCE-01/SCI-FIX-WEIGHT/
  logs/probe_1t2t.log`）；(c) 跨后端等价 = **不允许**（无此合同）。

## 7a 表示能力边界与近奇异处置（SCI-UPM-CAP-001 / SCI-UPM-KAPPA-001）

**表示能力边界（无接缝 ⟺ 公共面可表示）**：

- 参考面 `B_ref`（8×8 control cell 双线性，节点间距 ≈ `hips.tile_width/8` = 64 px）只能表示尺度 ≳ 2× 节点间距的分量；
- 当帧间天光差含「`B_ref` **不可表示**且沿向**相干**」的分量时，残余接缝 ≈ **0.80 × 该分量 RMS**（Pearson 0.896，`实验/additive-sky-seamless/results/c2_multiplicative.json`）；
- 空间尺度 ≲ 2× 节点间距（≈256 px）时显著：尺度 1600→50 px 扫描使残余接缝 **×5.07**；
- **工程要求**：节点间距须 ≤ 目标可表示尺度的 1/2；`roughness_penalty` 只在「面确实光滑」的前提下使用，不得用强平滑掩盖不可表示分量；
- 该边界是**条件结论**：在可表示域内接缝压缩 12.5× 成立，域外不成立；报告与产品必须与边界一同引用（§16.2）。

**近奇异（条件数）处置**：

- 天光面正规方程条件数 `κ = cond_2(D⁻¹AᵀWA D⁻¹)` 必须**可观测**并写入 provenance（真实 M42 样本实测 κ = **3.16e7**，`实验/additive-sky-seamless/results/c7_realdata.json`；χ²_red 1.004 但 κ 逼近默认上限）；
- 上限 `kappa_max` 默认 **1e6**（`FZ-AP2S-KAPPA-MAX`）：`κ > kappa_max` ⇒ 必须走**粗糙度正则化**（`roughness_penalty`）或**节点数自适应**，并在 provenance 记录所走分支、所用参数与 κ；**禁止**静默产出欠定解、**禁止**放宽 `kappa_max` 求绿；
- provenance 最小集：`gauge_mode`、每分量 `ref_frame_id`、`rank`、`rank_rtol`、`kappa`、`kappa_max`、`k_corr`、`model_hash`、`any_fail_closed_reason`。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无重叠/无 control | `NO_DATA` | `upm.cpp:6` 域检查 |
| `use_ivar_weight=1` 且 `control_ivar≤0/非有限` | `p2_upm_raw_weight rc=2 → build rc=2` | `DATA-UPM-CONTROL-UNC-001` |
| `S=0` (MAD=0) | 该 patch 方差为 0，不计 control | `noise_model` |
| 单帧区 | harmonic continuation 填 | `phase2` 域 |
| 畸形模型文件 | `ERR-P2-UPM-001` | `aio_upm.cpp` |
| 跨 endian payload | 理论 id 不同，当前仅 Linux x86_64 路径 | `sampler.cpp:257` |

## 9 精度策略

- FP64 求解；`dense cache` 与 `sparse` 求值 `1e-12` 等价门（`SparseEqualsDense`）；
  `k_corr` 冻结默认 `1.4`（保守取整）；其 MC 校准来源 `control_median_mc_test` **未注册
  （构建孤儿 / MISSING，见 §11/§13/§15）**——常数本身由 §5 定义冻结，但不可复跑该 MC 证据。
- 并行确定性容差见 §7 三档（同配置重复位精确 + hash exact；跨 worker 数 1e-12；跨后端不允许）。

## 10 不可接受变化

- 在生产 `w_UPM` 中乘 `star SNR / support^p / obs.ivar`(已弃用)；
- 将 `control_estimator` 改为单 leaf 或 `N_total` 替代 `N_retained`；
- 改变 `k_corr` 默认 1.4 或 `parameter_rows ↔ frame_id` 绑定语义；
- 在 `calibrate_block` 中引入乘性尺度；
- 在未同步换算 λs/λ0 的前提下把求解器内的 `w_cell`（份额式）替换为 `w_UPM`（绝对式）——
  份额式丢弃跨 control 精度，换绝对权等于换估计量（隐式改科学），须先重标定并登记。

## 11 验证 Oracle

- **UPMW 硬门 7 项**：001 snr 扰动不变、002 ivar 1:4→weight 1:4、003 星群不变、004 `Var(median)≈πσ²/2N`、005 MC `k_corr≈1.3883`（**未注册：`control_median_mc_test` 是构建孤儿 MISSING，不可复跑**）、006 无 legacy SNR consumer 且缺 ivar `rc=2`、007 patch 真值恢复（`synthetic_gate`）。
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
- 实现: `lib/algorithms/coverage/src/upm.cpp` (1107-1123, 493-510), `lib/algorithms/coverage/src/sampler.cpp` (250-364, 672), `lib/infrastructure/aio/src/aio_upm.cpp` (持久化)
- 公开 API: `p2_upm_build, p2_upm_calibrate_block, p2_upm_raw_weight, p2_upm_open/save`
- 测试: `TEST-UPMW-001..007, UPMW-001..007, UpmPersist*` (`synthetic_gate.cpp`)；
  `control_median_mc_test.cpp` **MISSING（未注册：不在任何 CMake/ctest/CI 面，构建孤儿）**

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
4. `k_corr=1.4`（MC 实测 1.3883，pixfrac=0.8，2000 次）：**项目自产 MC 证据**（`control_median_mc_test`，**未注册/MISSING：构建孤儿，本证据当前不可复跑**），非外部文献。
5. Tukey/MAD 常数：复用 SCI-002/SCI-003 文献链（PMC6768164 实证；Φ⁻¹(3/4) 恒等式）。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5 公式、§7 不变量与 §10 禁改清单。

- **多帧相对光度/天体联合定标**：SCAMP（GPL-3.0，https://github.com/astromatic/scamp；Bertin, E. 2006, ASP Conf. Ser. 351, 112，标题经 aspbooks.org 逐字核验 2026-09-17）；Padmanabhan, N. et al. 2008, ApJ 674, 1217（SDSS 重叠观测联合相对定标、gauge/连通性）。
- **马赛克逐帧背景扣除/coadd 权重**：SWarp（GPL-3.0，https://github.com/astromatic/swarp；Bertin, E. et al. 2002, ASP Conf. Ser. 281, 228）；Gruen et al. 2014, PASP 126, 158。
- **Huber IRLS**：Huber, P. J. 1964, Ann. Math. Statist. 35, 73（DOI 10.1214/aoms/1177703732）；Holland, P. W. & Welsch, R. E. 1977, Communications in Statistics A6, 813（DOI 10.1080/03610927708827533，δ=1.345 的 IRLS 出处）；Huber & Ronchetti 2009, Robust Statistics, 2nd ed., Wiley（ISBN 978-0-470-12990-6）。
- **弱零锚（弱 Tikhonov/岭正则）**：Tikhonov, A. N. 1963, Soviet Math. Dokl. 4, 1035（**核验状态**：文章级，卷页需网络核验）。
- **var(median)≈πσ²/(2N)**：正态样本中位数渐近方差的教科书结论（Hoaglin et al. 1983；Kendall & Stuart, The Advanced Theory of Statistics Vol.1）；UPMW-004 实证 ratio 0.997。
- **稀疏天光面样条（加性天光面的表示候选）**：Duchon, J. 1977, Constructive Theory of Functions of Several Variables, 85（薄板样条）；Wahba, G. 1990, Spline Models for Observational Data, SIAM（ISBN 0-89871-244-0）。§5 冻结的加性场当前实现为 8×8 control cell 双线性；稀疏样条是**同一加性场**的另一种表示候选（`OPEN-P2S-02` 数据面/schema 待合同流程），**不改变** §1/§5 的纯加性模型。
- **本期决议：纯加性（负责人 2026-09-19 裁决，claim `FIX-SCI-SNR-CANON-001`）**：本文件 §1/§5 的**纯加性**冻结模型 `calibrated_f(p)=raw_f(p)−C_f(p)` 为最终模型；**取代** `FIX-A-UPM-001` 提议的 `y_k(x)=g_k·s(x)+b_k(x)` 乘性方向。`g_k ≡ 1` **本期不启用**；`÷g²` 保持**恒等式**（`Var(corrected)=[σ_raw²+J_out C_θ J_outᵀ]/g²`，`g≡1` 时退化等价，公式不删）。**理论依据（负责人给出，须进论文）**：Phase1 正确归一化后，帧本身已是**同一测光体系的真信号**加**可等效为加性的天光**；残留天光**无论是加性还是乘性，都可以用加法移除**；乘性残留属**低阶空间增益**，已归 Phase1 处理（`I_photo = k_photo·m(x,y)·I_cal`），Phase2 只做加性扣除 `raw − C_k`。原「设计-实现模型冲突」UNRESOLVED **关闭**：`10_sampling.md`/`11_upm.md`/`UNIFIED_MODEL.md` 的目标态表述同步订正为纯加性；`OPEN-P2S-02` 仅剩数据面/schema 表示待合同流程。
- **k_corr=1.4 的 MC 证据**：control_median_mc_test 未注册（MISSING，构建孤儿），常数本身按 §5/§10 冻结但当前不可复跑（§9/§11/§13/§15）。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过（含 MC 一致性、gauge 唯一性、harmonic continuation 边界）；**例外**：
  UPMW-005 的 MC 证据源 `control_median_mc_test` **未注册（MISSING，构建孤儿）** ⇒ 该项
  只有常数冻结定义（§5/§10），没有可执行证据；收进 ctest 前不得称"已过"；
- §7 不变量门全过；
- `eng/tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-005 转换：已知低阶光度面恢复、重叠图 gauge/退化强度扫描、接缝指标预冻结门槛（SYN-005 数据与不变量表），参数恢复/残差/接缝降低且不破坏星 flux 全过。

## 16 SCI-C 实验结论（RELEASE-04 / SCI-403，仓内实测）

实验单元：`实验/additive-sky-seamless/`（报告 `README.md`，机器可读结果 `results/*.json`，
一键复跑 `code/run_all.sh`）。**本节只记录已定稿的实测结论，不改变任何公式或默认容差。**

### 16.1 已证实的结论（data 级，生产代码实测）

1. **多退少补正确**：产品 `calibrated_k = raw_k − δ_k`（**保留 B_ref**）在覆盖子集突变处把
   背景电平接缝从 4.80 e⁻ 压到 **0.383 e⁻（12.5×）**；产品中位 299.17 e⁻ ≈ B_ref 297.33 e⁻。
   全减背景臂（`raw − b_k`）产品中位 0.845 e⁻ ⇒ **退化，不得用作无接缝证据**。
2. **B_ref 是规范零点**：`b_k − δ_k` 与帧无关（5.7e-14）；gauge 0↔1 只造成 δ_k 的逐帧常数
   偏移（5.3e-15），接缝度量不变（差 0.0）。
3. **权重**：`control_ivar` 的伪影漏入 0.0245 e⁻，比 uniform（0.0651）小 2.7×、比 SNR²（0.3203）
   小 13×；噪声 RMS 亦为三臂最小（1.532 < 1.674 < 2.069 e⁻）。与 SCI-402 的独立结论一致。
4. **稀疏现场求值**：dense cache 与 sparse `calibrate_block` 在 1,048,576 点上 max|Δ| = 3.1e-15；
   稀疏模型 14,001 B vs 稠密 8,389,129 B（0.167%）；按需求值 64² 块峰值 RSS 低于稠密物化 512²。

### 16.2 适用域边界（**新增，必须与结论一同引用**）

1. **无接缝 ⟺ 公共面可表示**。当帧间天光差含「B_ref 不可表示且沿 y 相干」的分量时，
   残余接缝 ≈ 0.80 × 该分量 RMS（Pearson 0.896）；空间尺度 ≲2× 节点间距（≈256 px）时显著
   （1600→50 px 扫描使残余接缝 ×5.07）。
2. **纯加性前提**：帧间乘性差必须先在 Phase1 吸收（实测 5.89e-4）。未做 Phase1 归一时
   （历史 `photometry_applied=false`/`photscal=1.0`，1.56× 帧差），纯加性 UPM 后接缝 27.37 e⁻，
   做了 Phase1 后 6.31 e⁻（**4.33×**）。基外高频乘性分量（1%@24 px）对**电平**接缝贡献有界
   （<50% 基线），但会被分块 PSD 检出（k=4 vs 预期 5.33）。
3. **不可检验域**：`smoothing_lambda=0` 时 per-(frame,cell) 自由加性场恰好定解，公共场 M 只是
   每 cell 的规范选择 ⇒「拟合/堆叠权重同源」与「末端残差场扣除」在该域内**不可检验**；
   `final_gauge` 在 `m_full_frame=1` 时近似 no-op（3.7e-3 e⁻），且**不能**修复子集依赖。

### 16.3 生产缺陷登记（FIX，本单元未改任何生产代码）

| ID | 位置 | 现象 | 实测 |
|---|---|---|---|
| FIX-1 | `lib/infrastructure/scheduler/src/module_adapters.cpp:5941-5942` | 绝对容差 `tolerance=1e-6`, `tolerance_relative=0` 在 ~300 e⁻ 尺度**永不收敛** | 300 次迭代 `converged=0` |
| FIX-2 | 同上 `out_converged` | 只有 0/1，**无法区分** max_iter 与 stalled（规范要求 0/1/2/3） | 两例均返回 0 |
| FIX-3 | 天光面正规方程 | 真实 M42 样本 κ = 3.16e7（近奇异） | χ²_red 1.004 但条件数逼近默认 `kappa_max` |

**处置状态（RELEASE-05）**：FIX-1/2/3 的规范已冻结进 §5（相对容差 + `converged` 0/1/2/3 + stalled 判据）与 §7a（表示能力边界 + κ provenance）；生产实现修复与复跑归 SCI-502。

建议（供前台裁决）：FIX-1 启用 `tolerance_relative=1` 或按观测尺度归一；
FIX-2 按 §7 语义补 `stalled` 分支；FIX-3 提高粗糙度惩罚或节点数自适应。
