> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# Phase2 `psfsw_robust` 算法主规格

- 文档 ID：`ALG-P2-PSFSW-001-SPEC`
- 任务：`ALG-P2-PSFSW-001`（wave 3；`depends_on = SCI-ADJ-001`）
- 基线：`HEAD = main = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`
- 机器可读同源：`run/v6/alg-p2-psfsw/spec/psfsw_spec.json`（`astrocs.phase2.psfsw-algorithm-spec/v1`）
- 性质：**可实施、可验证的算法冻结建议**。不改变任何既有冻结门/容差；落定 W1 复核显式委托 W3 的数值（`PSFW_FREEZE_RESEARCH` §11-R6/R7、`SCI-P2-001` §6 open item 4）。
- 上游（只读）：宪章 §4.1/§4.3/§6.3；`PROJECT_SPEC` §3/§5/§7/§8；`DESIGN-P2-001` §6.3/§7/§9/§10；`DESIGN-P1-001` §8.1/§8.2/§8.3/§9/§11；`UNIFIED` §2..§11；`SCI-PSFW-001` §1..§8；`SCI-ADJ-001`（FZ-*/SC-ADJ-*）。

---

## 0. Claim → 条款/文献锚总表

| ID | Claim（一句话规格） | 条款锚 | 文献/来源锚 |
|---|---|---|---|
| A1 | 生产科学模式 = `point_information` / `surface_gls` / `psfsw_robust`；文档基线模式 = `equal` / `pixel_ivar`；`psf_snr_power` 本包 DEFERRED | `FZ-MODE-PRODUCTION` / `FZ-MODE-BASELINE` / `FZ-MODE-DEFERRED`；`SC-ADJ-S1.1..4`；`C-004.1` | `PROJECT_SPEC` §5；`PSF_SIGNAL_WEIGHT` §4 表 |
| A2 | `psfsw_robust_weight` 是无量纲组内相对质量，**不是** QA-only，但**不得**写成 ivar/variance/Fisher/`W_info` | `FZ-UNIT-PSFSW`；`FZ-FIELD-PSFSW-UNIT`；`SC-ADJ-P203.2`；`ADJ-P2-03`；`RULINGS.md` #3/#5 | PixInsight ImageWeighting §2.5（hybrid quality estimator，未声明 Fisher 最优） |
| A3 | 四分量 `signal`/`concentration`/`noise`/`background` 必须分别落产品，各有互异 `measurement_id` 与 `p05/p50/p95` + 有效覆盖 | `FZ-FIELD-PSFSW-4COMP`；`SC-ADJ-P203.1`；`RULINGS.md` #4 | `PSF_SIGNAL_WEIGHT` §3/§6 |
| A4 | 复合 `Wt_k=C_norm·S^α·Conc^β/(N^γ·B^δ)`，组内 `W_psfsw,k=Wt_k/median_j(Wt_j)`，`median=1`、全正 | `FZ-FORMULA-PSFSW-COMPOSITE`；`SC-ADJ-P203.2` | `SCI-PSFW-001` §3；`PSFW_FREEZE_RESEARCH` §4.3 |
| A5 | 指数、`C_norm`、截断、稳健估计器、归一常数**版本化且落产品**；标定样本 ≠ 验收样本 | `SCI-PSFW-001` §3 末段 | `SCI-PSFW-001` §3 |
| A6 | 最终 covariance 只能从实际组合系数传播 `C_out=R C_in Rᵀ`；禁止 `1/W_psfsw`、`ΣW_psfsw` 或任何权重标量反推 | `FZ-FORMULA-COV-PROP`；`FZ-GATE-PSFSW-COV`；`SC-ADJ-P203.3`；`RULINGS.md` #5 | Fruchter & Hook 2002（线性重建 + 相关噪声） |
| A7 | effective PSF（实际组合算子脉冲响应）必输，归一约定按产品族显式声明；只给 FWHM 标量不构成 effective PSF | `FZ-GATE-PSFSW-EPSF`；`SC-ADJ-P203.4` | `COVARIANCE_AND_EFFECTIVE_PSF` §3 |
| A8 | 共同星集必须**独立于待测帧测量**（外部参考星表或参考叠加单一门限），并附 selection function | `SC-ADJ-P203.6`；`FZ-GATE-PSFSW-FAILCLOSED` | `PSF_SIGNAL_WEIGHT` §3/§7.3；`PSFW_FREEZE_RESEARCH` §5.2 |
| A9 | 深度稳定性是**可量化门**：共同星集 `W_psfsw` 在深度扫描下相对变化 ≤ 5% 且无强单调漂移（数值本任务冻结，见 §12） | `SCI-PSFW-001` §7.3；`PSFW_FREEZE_RESEARCH` §5.3/R6 | `PSFW_FREEZE_RESEARCH` Oracle K8（sample-derived 13.2% vs `W_info` 0%） |
| A10 | fail-closed 原因属固定白名单 5 项；`valid=false` 时 `weight_value` 必须 `null`，**禁止**回退 median source SNR | `FZ-GATE-PSFSW-FAILCLOSED`；`SC-ADJ-P203.5` | `PSF_SIGNAL_WEIGHT` §3:49 |
| A11 | `median(SNR_F)`/`median_source_snr`、`support`、`coverage`、`FWHM`、`residual` 一律只作诊断/门，不得单独或组合冒充权重 | `FZ-GATE-MEDIAN-SNR`；`FZ-GATE-SUPPORT-COVERAGE`；`SC-ADJ-C00402.1..2`；`C-004.2`；宪章 §6.3 | `PROJECT_SPEC` §4 末段/§7；`UNIFIED` §3/§11 |
| A12 | 与等权/exposure/pixel-ivar/`W_info` 四基线比较，只允许声明“在指定验收数据上优于/不劣于指定基线”，不得声明 Fisher 最优 | `DESIGN-P2-001` §6.3/§10；`SCI-PSFW-001` §5/§7.4 | `SCI-PSFW-001` §4 表；Zackay & Ofek 2017 I/II |
| A13 | 标量降级须**同时**过空间残差/趋势门与功率损失门；否则存 map/model/控制点 | `FZ-DEGRADE-SCALAR`；`SC-ADJ-GEN04.1..4`；`UNIFIED` §8；`DESIGN-P1-001` §8.3 | `DESIGN-P1-001` §8.3 |
| A14 | 面亮度保持归一 `S_p=Σ_j B_j a_jp/Σ_j a_jp` 为上游输入口径；通量守恒为条件不变量并写 provenance | `FZ-FORMULA-DRIZZLE-SB`；`FZ-COND-FLUX-CONSERV`；`SC-ADJ-F02.1..4` | `DESIGN-P1-001` §9:120-126 |
| A15 | schema 词表由 W6 归一；本任务不发明第三套词表，`baseline_id` 为比较协议标识（非 `weight_mode` 枚举值） | `SC-ADJ-C00403.1..2`；`C-004.3` | `PSFW_FREEZE_RESEARCH` §8/§11-R2 |

---

## 1. 适用域、输入与输出

### 1.1 主体

本规格约束 Phase2 的显式模式 `weight_mode = psfsw_robust`（`DESIGN-P2-001` §6.3；`FZ-MODE-PRODUCTION`）。它在同一**帧组 `G`** 内定义：同波段 × 同目标或重叠连通分量 × 光度已归一的帧集合（`SCI-PSFW-001` §3）。跨组比较在未声明 selection function 前禁止。

### 1.2 输入

- Phase1 逐帧：`psfsw` 四分量（`S_k`/`Conc_k`/`N_k`/`B_k`）及其空间摘要与有效覆盖、共同星集 `S`、selection function、PSF 模型 `P_k`、光度响应/归一 `a_k`、像素方差/协方差 `C_k`、validity/rejection、provenance（`DESIGN-P1` §10:134）；
- Phase2 组内：UPM 光度尺度与背景（`y_k=g_k s+b_k+ε_k`，`DESIGN-P2-001` §4）、排异结果 `v_k`、coverage（只作门，`宪章` §6.3）；
- 单位输入口径：上游面亮度 signal `signal_sb` = ADU/px²、方差 `sb_variance_out` = ADU²/px⁴（`FZ-UNIT-SIGNAL-SB`/`FZ-UNIT-VAR-SB`，由 `SC-ADJ-F02.1` 面亮度保持归一定义）。

### 1.3 输出（`psfsw_integration` 产品族，`DESIGN-P2-001` §9:99）

1. 四分量（`FZ-FIELD-PSFSW-4COMP`）；
2. 组内相对权重 `W_psfsw`（无量纲，`FZ-UNIT-PSFSW`）与版本化复合参数；
3. conventional coadd signal `I_out`；
4. variance/correlation（从实际组合系数传播，`FZ-FORMULA-COV-PROP`）；
5. effective PSF（`FZ-GATE-PSFSW-EPSF`）；
6. 与四基线的比较报告（`PSFSW_BASELINE_COMPARISON.md`）；
7. support/coverage/validity/rejection + UPM + provenance（不在本规格内，但为强制伴随面）。

### 1.4 单位表（继承 `FZ-UNIT-*`，不得改写）

| 量 | 符号 | 单位 | 冻结锚 |
|---|---|---|---|
| 面亮度 signal | `signal_sb` | ADU/px² | `FZ-UNIT-SIGNAL-SB` |
| 输入逐像素方差 | `pixel_variance_in` | ADU² | `FZ-UNIT-VAR-IN` |
| 输出面亮度方差 | `sb_variance_out` | ADU²/px⁴ | `FZ-UNIT-VAR-SB` |
| 复合权重（未归一） | `Wt` | `component_flux_unit/px²`（声明口径） | `SC-ADJ-P203.2` |
| **稳健相对权重** | `W_psfsw` | **1（无量纲）** | `FZ-UNIT-PSFSW` |
| 点源信息权重 | `W_info` | ADU⁻² | `FZ-UNIT-WINFO` |
| 点源充分统计量 | `Q` | ADU⁻¹ | `FZ-UNIT-Q` |
| 点源通量 | `F_hat` | ADU | `FZ-UNIT-FLUX` |

**单位一致性规则**：`S_k`、`N_k`、`B_k` 共享同一 `component_flux_unit`（组内常量、显式声明）；`Conc_k` 的单位是 `component_flux_unit/px²`。`Wt` 因指数作用可能带 `1/px²` 量纲，**`W_psfsw` 由组内比值定义因而严格无量纲**——量纲审查以 `W_psfsw` 为对象。

---

## 2. 模式与对象身份

### 2.1 模式面（继承冻结，不新增）

- 生产科学模式：`point_information` / `surface_gls` / `psfsw_robust`（`FZ-MODE-PRODUCTION`）；
- 文档基线模式：`equal` / `pixel_ivar`（`FZ-MODE-BASELINE`，仅基线比较，不得冒充科学最优）；
- 延迟模式：`psf_snr_power`（`FZ-MODE-DEFERRED`，本包 `NOT_IMPLEMENTED/unavailable`，不进生产路由，`C-004.1`）；
- legacy 整数 `{0=support×snr², 1=equal, 2=ivar}` 被取代，`0` 不得进入任何科学权重面（`FZ-FIELD-WEIGHTMODE`、`SC-ADJ-S1.4`）。

### 2.2 对象身份（`weight_kind` 与单位）

```text
weight_kind   = "relative_dimensionless"
weight_units  = "1"
group_normalized = true
normalization.scope   = "group"
normalization.median_target = 1.0
```
（语义冻结锚：`FZ-FIELD-PSFSW-UNIT`；字段词表由 `SCHEMA-INTEGRATE-001`/W6 归一，`SC-ADJ-C00403.1..2`。本规格同时给出与 `SCI-P2-001` 词表的双向映射建议，见 §7.4。）

### 2.3 `baseline_id` 与 `weight_mode` 的分离（避免污染冻结枚举）

基线比较需要的 `exposure` 不是 `FZ-MODE-BASELINE` 的枚举值。本规格引入**比较协议标识** `baseline_id ∈ {equal, exposure, pixel_ivar, point_information}`，它属于比较报告字段，**不是** `weight_mode` 合法取值，因此不扩展冻结模式枚举（`C-004.3`）；其 schema 归属 `SCHEMA-INTEGRATE-001`(W6)。

---

## 3. 共同星集与 selection function（A8/A9）

### 3.1 定义

对帧组 `G` 的**共同星集** `S`：在组覆盖天区内、由独立于逐帧 PSFSW 测量的单一来源确定、并在帧 `k` 中通过排除旗标后仍有效的星集合。逐帧有效子集记 `S_k`。

### 3.2 允许构造路径（二选一，必选其一）

1. **外部参考星表路径**：版本化外部星表（Gaia DR3 / 离线星表）在组天区内的固定子集；记录 `reference_catalog_id` 与 `reference_catalog_version_hash`；
2. **参考叠加单门限路径**：在**参考叠加**（非逐帧检测）上取单一门限得到的固定星表。

### 3.3 禁止构造（负向判据）

**禁止**把逐帧检测阈值分别取星后求交得到“共同星集”。该构造使样本随帧 SNR/seeing 变化，selection function 与待测量耦合（A8）。机器判据 `PSFSW-G09` 要求 `star_selection.independence_proof` 属于 `{external_reference_catalog, reference_stack_single_threshold}`；任何形如 `per_frame_threshold_intersection` 的取值直接 REJECT。

### 3.4 selection function（强制随产品落盘）

`selection_function` 必须显式记录：`selection_function_id`、`reference_catalog_id`、`reference_catalog_version_hash`、星等范围 `mag_range`、门槛 `detection_threshold_sigma`、匹配半径 `matching_radius_arcsec`、历元/自行处理、作用时刻 `applied_at`。缺任一 → `unavailable(no_common_star_set)`（`PSFSW-G08`）。

### 3.5 排除旗标（逐个显式，不得省略）

`saturated`、`blended`、`trailed`、`moving`、`psf_mismatch`、`edge_truncated`。逐帧 `S_k = S` 去掉上述旗标命中者；`n_common = |S|` 的跨帧有效交集（按 `star_selection` 定义）必须落产品。

### 3.6 `n_common` 下限与分档（数值本任务冻结，见 `PSFSW_FROZEN_THRESHOLDS.md`）

| 档 | 条件 | 处置 |
|---|---|---|
| HARD_FAIL | `n_common < 3` | `valid=false`，`reason=insufficient_valid_stars`，`weight_value=null` |
| LOW | `3 ≤ n_common < 10` | 允许生产但强制 `n_common_tier=low`、`confidence_class=low`、`bootstrap_ci_reported=true`；跨组比较禁止；非均匀阈值收紧至 0.20 |
| NORMAL | `10 ≤ n_common < 30` | 标准路径，`n_common_tier=standard` |
| PREFERRED | `n_common ≥ 30` | 无低计数旗标；深度稳定性门可在全扫描点上评估 |

硬下限 `n_common_min = 3` 与 `SCI-P2-001` R5 对齐；`10`/`30` 是本任务冻结的稳健分档（W1 复核 §5.1 只建议了 3，分档为 W3 落定值，登记 W4 确认）。

### 3.7 逐帧有效星不足

任一帧 `|S_k| < 3` → `unavailable(insufficient_valid_stars)`（`PSFSW-G10`）。

---

## 4. 四分量定义（A3）

在帧组 `G` 内对每帧 `k`、每共同星 `s ∈ S_k` 计算 PSF 测光通量 `fhat_{k,s}`：

```text
signal        S_k    = sum_{s in S_k} fhat_{k,s}                         [component_flux_unit]
concentration Conc_k = mean_{s in S_k}(fhat_{k,s}) / A_NEA,k              [component_flux_unit/px^2]
noise         N_k    = robust_scale({fhat_{k,s}})                        [component_flux_unit]
background    B_k    = bbar_k * A_ref,k                                  [component_flux_unit]
```

- `A_NEA,k = 1 / sum_p P_{k,p}^2`（`DESIGN-P1-001` §8.1；`SCI-PSFW-001` §2）；
- `bbar_k` 是稳健均值逐像素背景，`A_ref,k` 是把背景映射到通量尺度的参考面积（默认 `A_NEA,k`，可声明替换并版本化）；
- `robust_scale` 为 MAD 或 Sn 类稳健尺度估计器，估计器名与版本必须落产品（`SCI-PSFW-001` §3）。
- **规范产品键**（`FZ-FIELD-PSFSW-4COMP`）：`psfsw.signal` / `psfsw.concentration` / `psfsw.noise` / `psfsw.background`；键名 schema 由 W6 归一，语义以上表为准。

### 4.1 分量语义边界（禁止替换，`A3`/`A11`）

| 分量 | 禁止替代 | 理由锚 |
|---|---|---|
| `signal` | 帧总信号、孔径和 | `SCI-PSFW-001` §3（对应 PixInsight sum of PSF flux estimates） |
| `concentration` | FWHM、拟合残差、`median_fwhm` | `SCI-PSFW-001` §8 第二条 |
| `noise` | 像素 σ、`depth_m5` | `SCI-PSFW-001` §3 |
| `background` | `support`/`coverage` | 宪章 §6.3；`UNIFIED` §3 |

### 4.2 分量落盘结构（强制）

四分量各自：`measurement_id`（互异）、`p05`/`p50`/`p95`（满足 `p05 ≤ p50 ≤ p95`）、`valid_area_fraction ∈ [0,1]`、显式单位、估计器版本。

四分量**显著空间非均匀**时：拆 region/tile 权重，或对帧标量模式判 `unavailable(spatial_nonuniformity_gate_failed)`（`FZ-DEGRADE-SCALAR`；判据见 §12 与 `PSFSW-G18`）。

### 4.3 分量单位一致性门

`S_k`/`N_k`/`B_k` 必须共享组内常量 `component_flux_unit`，`Conc_k` 为 `component_flux_unit/px²`。单位未声明、组内不一致或含 `flux^-2`/`ivar` 词 → REJECT（`PSFSW-G02`/`PSFSW-G23`）。

---

## 5. 复合权重与组内稳健归一（A4）

### 5.1 复合式

```text
Wt_k      = C_norm * S_k^alpha * Conc_k^beta / (N_k^gamma * B_k^delta)      alpha,beta,gamma,delta >= 0
W_psfsw,k = Wt_k / median_{j in G}(Wt_j)                                    组内 median = 1
```

冻结版本 `PSFSW-COMPOSITE-V1`（本任务落定，W4 确认）：`alpha=2`、`beta=1`、`gamma=2`、`delta=1`、`C_norm=1.0`。
指数约束：`(alpha>0 or beta>0) and (gamma>0 or delta>0)` 且非全零；任一违反 → REJECT（`PSFSW-G14`）。
方向性（任意正指数成立，Oracle V2 复算）：`S↑→W↑`、`Conc↑→W↑`、`N↑→W↓`、`B↑→W↓`。

### 5.2 `C_norm` 的尺度简并（审计不变量）

组内中值归一使 `W_psfsw` 对 `C_norm` **严格不变**：`C_norm` 同时乘分子与中值分母。因此：
- `C_norm` 必须记录（provenance 完整性），但改变它**不得**改变 `W_psfsw`；
- 任何“实现让 `C_norm` 影响 `W_psfsw`”的行为违反组内归一语义 → 门 `PSFSW-G16` 变红；
- 这意味着 PixInsight 式“把典型值调到 0.01–100”的归一常数在 AstroCS 的 `W_psfsw` 产品上**没有数值作用**，只保留在未归一 `Wt` 的 provenance 中。

### 5.3 截断策略（版本化）

顺序固定为“先 fail-closed、后分量下限”：

1. **fail-closed 检查**（在幂运算前）：`B_k ≤ 0` → `background_nonpositive_undefined_transform`；`N_k ≤ 0`、`S_k ≤ 0`、`Conc_k ≤ 0` → `insufficient_valid_stars`（退化星样本）；
2. **分量下限**（仅在通过 fail-closed 后）：`x ← max(x, x_floor)`，`S/Conc/N/B` 下限各 1e-12（防下溢，版本化）；
3. 计算 `Wt_k`；
4. `W_psfsw,k = Wt_k / median_j(Wt_j)`；
5. **禁止**对 `W_psfsw` 做后归一化裁剪（会破坏 `median=1`）。

`N_k ≤ 0` 映射到 `insufficient_valid_stars` 的理由：稳健尺度估计器对退化样本（全同通量）返回 0，语义是“星样本不提供信息”，而非背景变换未定义；白名单 `FZ-GATE-PSFSW-FAILCLOSED` 5 项不得增删，故不做第 6 类原因。

### 5.4 版本化落盘字段（A5）

```text
composite_version        = "PSFSW-COMPOSITE-V1"
exponents                = {alpha, beta, gamma, delta}
C_norm                   = 1.0
component_estimators     = {signal, concentration, noise, background} + estimator_version
truncation_policy        = {fail_closed_order, component_floors}
normalization            = {scope: group, estimator: median, median_target: 1.0}
calibration_sample_id    = <id>      # 标定该版本的样本
acceptance_sample_id     = <id>      # 最终验收样本，必须 != calibration_sample_id
```
缺任一版本字段 → REJECT（`PSFSW-G14`）；`calibration_sample_id == acceptance_sample_id` → REJECT（`PSFSW-G24`，`A5`）。

### 5.5 与 W_info 的严格边界（A2/A6）

| 对象 | 单位 | 声明 | 与对方关系 |
|---|---|---|---|
| `W_info` | ADU⁻²（信息量） | 有条件统计最优（BLUE/最大点源 SNR/最小通量方差，前提见 `SC-ADJ-P201.1..3`） | 无任何公式把 `W_psfsw` 写成 `W_info` 的函数 |
| `W_psfsw` | 1（无量纲组内相对） | 只在预注册验收数据上“优于/不劣于指定基线” | 唯一合法耦合 = covariance 传播（用实际 `alpha_k`）与基线比较 |

---

## 6. conventional coadd 与 covariance / effective PSF（A6/A7）

### 6.1 组合系数与输出

```text
alpha_k(p) = W_psfsw,k * v_k(p) / sum_j ( W_psfsw,j * v_j(p) )        v_k(p) in {0,1}
I_out(p)   = sum_k alpha_k(p) * d_k(p)
```

- 对每个有定义的像素 `sum_k alpha_k(p) = 1`（Oracle V4 复算）；
- `v_k(p)` 是 validity/rejection 门；`support`/`coverage` 只作门，永不作权重（宪章 §6.3；`FZ-GATE-SUPPORT-COVERAGE`）；
- 分母为 0（该像素无有效帧）→ 输出 `null`/`unavailable`；
- 单位与输入帧 signal 一致（面亮度帧为 ADU/px²）。

### 6.2 variance/covariance（唯一权威式）

```text
C_out = R C_in R^T                                                              # 算子形式
Var(I_out(p)) = sum_{k,l} alpha_k(p) alpha_l(p) [C_in]_{kl}(p)                  # 标量形式
Var(I_out(p)) = sum_k alpha_k(p)^2 sigma_k(p)^2                                 # 帧独立 + 对角 C_in
```

- `R` 含实际 `alpha_k`、UPM 光度尺度、重采样/插值核与 validity 掩码（`COVARIANCE_AND_EFFECTIVE_PSF` §0）；
- `C_in` 必须纳入随机项与可表示的相关项（共享 master、共同天光、Drizzle 相关、UPM 参数；`UNIFIED` §6/§7）；只存对角 variance 时必须另存相关核 `rho_ij` 或可重建算子摘要；
- **禁止**：`Var = 1/W_psfsw`、`Var = sum_k W_psfsw,k`、把任何权重标量/诊断量当方差来源（`FZ-GATE-PSFSW-COV`；门 `PSFSW-G06`；mutation m05/m18）；
- 字段：`covariance.method = "propagated_from_composite_coefficients"`、`variance_from_weight = false`、`uses_relative_weight_as_ivar = false`。

### 6.3 effective PSF（必输）

```text
P_eff(x;x_o) = [ sum_k alpha_k(x_o) a_k (P_k (x) K_k)(x - x_o) ]
             / [ sum_k alpha_k(x_o) a_k (P_k (x) K_k)(0) ]        (peak 归一)
```

- `alpha_k` 是**实际**使用的组合系数（含组内归一、validity、UPM 尺度）；`K_k` 是重采样/插值核；
- 面亮度产品要求积分归一 `sum_x P_eff = 1`；点源/detection 统计用 peak 归一；**归一约定按产品族显式声明**，否则 FWHM/EE 有歧义；
- FWHM/encircled energy 必须从 `P_eff` 测量，不得用逐帧 FWHM 的 median/均值代替（Oracle V7 复算 `median(FWHM_k) != FWHM(P_eff)`）；
- `effective_psf_id` 非空且必须随 `K_k`（或 transfer 描述）与归一约定保存；只给 FWHM 标量 → REJECT（`FZ-GATE-PSFSW-EPSF`；门 `PSFSW-G07`；mutation m12）。

### 6.4 与 point_information 的差异登记

`point_information` 的输出图像面是 matched-filter/proper coadd：`q_k=C_k⁻¹P_k`、`P_eff ∝ Σ_k(q_k⊗P_k)`（`COVARIANCE_AND_EFFECTIVE_PSF` §3）。`psfsw_robust` 是 conventional coadd：`P_eff = Σ_k alpha_k a_k(P_k⊗K_k)/Σ_k alpha_k a_k`。二者 effective PSF 定义不同，不得互相替代或混名（A12）。

---

## 7. 基线比较（A12；详见 `PSFSW_BASELINE_COMPARISON.md`）

### 7.1 比较基线集与权重定义

| `baseline_id` | 权重 | 说明 |
|---|---|---|
| `equal` | `w_k = 1` | 文档基线模式（`FZ-MODE-BASELINE`） |
| `exposure` | `w_k = t_k`（有效曝光时间，来自 provenance） | 比较协议标识，非 `weight_mode` 枚举值 |
| `pixel_ivar` | `w_k(p) = 1/v_k(p)` | 文档基线模式（`FZ-MODE-BASELINE`） |
| `point_information` | `W_info,k = a_k² P_kᵀ C_k⁻¹ P_k` | 生产模式 `point_information` 作为 `W_info` 基线（`FZ-FORMULA-WINFO`） |

### 7.2 协议要素

同一组合机制（§6.1）对全部基线计算 `I_out`、variance/correlation、effective PSF，并报告：点源 detection power、photometric variance、effective PSF FWHM/EE、flux bias、面亮度偏差、伪影指标。
数据**预注册**（M42 / 银心 / 合成集），标定样本与验收样本不相交（`A5`）。

### 7.3 声明门

只允许 `better_than(baseline_id)` 或 `non_inferior_to(baseline_id)`；统计证据用配对 bootstrap（重采样 ≥ 200、置信水平 0.95）：前者要求改进量 CI 下界 > 0，后者要求 CI 下界 > −不劣界（默认 0.02）。禁止 `fisher_optimal`/`equals_ivar`/`equivalent_to_W_info`/`looks_better`（`SCI-PSFW-001` §4 表/§8）。

### 7.4 与 SCI-P2-001 词表的双向映射建议（供 W6 使用，不在本任务写 schema）

| 本规格/SCI-PSFW 词表 | SCI-P2-001 词表 |
|---|---|
| `weight_kind="relative_dimensionless"` | `weight.kind="psfsw_robust_weight"` |
| `weight_units="1"` | `weight.units="dimensionless_relative"` |
| `normalization.scope="group"` + `median_target=1.0` | `group_normalized=true` |

W6 归一后必须双射；本规格不发明第三套词表（`SC-ADJ-C00403.1..2`）。

---

## 8. fail-closed 与门（A10）

### 8.1 固定原因白名单（不得增删，`FZ-GATE-PSFSW-FAILCLOSED`）

```text
no_common_star_set
background_nonpositive_undefined_transform
insufficient_valid_stars
selection_bias_gate_failed
spatial_nonuniformity_gate_failed
```

### 8.2 触发映射

| 触发 | 原因 |
|---|---|
| 缺共同星集 id / selection function，或 `S` 为空 | `no_common_star_set` |
| `B_k ≤ 0`（背景变换未定义） | `background_nonpositive_undefined_transform` |
| `n_common < 3`、`|S_k| < 3`、`S_k/N_k/Conc_k ≤ 0` | `insufficient_valid_stars` |
| 深度稳定性门失败（§12）、selection function 非独立 | `selection_bias_gate_failed` |
| 分量空间非均匀/趋势超阈、标量功率损失/通量偏差超阈 | `spatial_nonuniformity_gate_failed` |

### 8.3 状态与回退

- `valid=false` → `weight_value=null` 且 `reason` ∈ 白名单；**禁止**回退成 `median source SNR`（`A10`；门 `PSFSW-G12`；mutation m11）；
- `valid=true` → 不得携带失败 reason（门 `PSFSW-G13`；mutation m10）；
- 权重动态范围 `max_k W_psfsw / min_k W_psfsw > 100` → **不**判 unavailable，强制置 validity 旗标 `weight_dynamic_range_exceeded` 并报告权重集中度 `sum_k alpha_k²/(sum_k alpha_k)²`（防单帧支配的可审计 guard）。

### 8.4 psfsw 产物禁止键（任何层命中即 REJECT，`FZ-GATE-PSFSW-COV`）

```text
ivar, variance, var, sigma, sigma2, inverse_variance, fisher, information, w_info, w_psf
```

### 8.5 禁止的权重来源诊断别名（`weight.sources`/`weight_value`/`covariance.variance_from` 命中即 REJECT）

`FZ-GATE-MEDIAN-SNR` 与 `FZ-GATE-SUPPORT-COVERAGE` 冻结集：`median_source_snr`、`median_snr`、`source_snr_median`、`med_source_snr`、`support`、`support_area`、`coverage`、`coverage_area`、`fwhm`、`psf_fwhm`、`median_fwhm`、`source_fwhm`、`residual`、`psf_residual`、`psf_fit_residual`、`fit_residual`、`psfsw_robust_weight`、`psfsw`。

---

## 9. provenance 最小集（继承 `FZ-PROV-MINIMAL-SET`，PSFSW 附加字段）

继承最小集：schema / 软件 SHA / run ID / 输入+配置哈希 / 单位 + `pixel_area_power` / frame / 像素语义 / 算法 ID / provider / 近似+降级原因 / 归一版本 / 相关核摘要 / `flux_conservation_factor` / `k_corr` / 时间 / 输出哈希。

PSFSW 附加：`composite_version`、`exponents`、`C_norm`、`component_estimators`、`truncation_policy`、`normalization`、`calibration_sample_id`、`acceptance_sample_id`、`common_star_set_id`、`selection_function_id`、`reference_catalog_version_hash`、`n_common`、`exclusion_flags`、`component_flux_unit`、`effective_psf_id`、`effective_psf_normalization`、`covariance.method`、`baseline_comparison_id`。

---

## 10. 验证门（可执行）

见 `PSFSW_GATES_AND_MUTATIONS.md` 的 PSFSW-G01..G25 机器判据与负向 mutation 册；Oracle V1..V13 的实测 rc 见 `run/v6/alg-p2-psfsw/summary.json`。

正向控制必须存在（至少一条合法记录被 ACCEPT），否则视为“空门”不得 PASS；负向 mutation 必须逐条 rc≠0（注入即红）。

---

## 11. 追踪矩阵

| 上游冻结/条款 | 本规格落点 |
|---|---|
| `FZ-FORMULA-PSFSW-COMPOSITE` | §5 |
| `FZ-FIELD-PSFSW-4COMP` | §4 |
| `FZ-FIELD-PSFSW-UNIT` | §2.2 |
| `FZ-UNIT-PSFSW` / `FZ-UNIT-WINFO` | §1.4 |
| `FZ-GATE-PSFSW-FAILCLOSED` | §8.1/§8.2/§8.3 |
| `FZ-GATE-PSFSW-COV` | §6.2/§8.4 |
| `FZ-GATE-PSFSW-EPSF` | §6.3 |
| `FZ-FORMULA-COV-PROP` | §6.2 |
| `FZ-DEGRADE-SCALAR` | §4.2/§12 |
| `FZ-MODE-PRODUCTION` / `FZ-MODE-BASELINE` / `FZ-MODE-DEFERRED` | §2.1 |
| `FZ-FIELD-WEIGHTMODE` | §2.1/§2.3 |
| `FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE` | §8.5 |
| `FZ-FORMULA-DRIZZLE-SB` / `FZ-COND-FLUX-CONSERV` | §1.2/§1.4 |
| `SC-ADJ-P203.1..6` | §3/§4/§5/§6/§8 |
| `SC-ADJ-S1.1..4` / `SC-ADJ-C00401..03` | §2.1/§2.3/§7.4 |

---

## 12. 冻结阈值（本任务落定，W4 确认）

数值、来源与门见 `PSFSW_FROZEN_THRESHOLDS.md`。摘要：深度稳定性最大相对偏差 0.05、深度扫描点数 ≥ 5、深度跨度 ≥ 1.0 mag、单调漂移 `|rho_s| ≤ 0.8`、`n_common` 硬下限 3 / 稳健下限 10 / 优选 30、分量非均匀 `(p95−p05)/p50 ≤ 0.30`、趋势 ≤ 0.10、标量功率损失 ≤ 0.05、标量通量偏差 ≤ 0.01。

本规格**不修改**任何既有 `FZ-*` 数值/容差；上述数值是 `PSFW_FREEZE_RESEARCH` §11-R6/R7 与 `SCI-P2-001` §6 open item 4 显式委托 W3 落定的量。

---

## 13. 未决风险、需控制器裁决与需负责人签字项

| ID | 事项 | 处置 |
|---|---|---|
| R1 | 本规格落定的数值（深度阈值/`n_common` 分档/非均匀判据/复合指数）需 W4 `CONTRACT-FREEZE-001` 正式冻结 | 本任务只登记并提交 W4，不擅称已冻结 |
| R2 | 词表双轨（本规格 vs `SCI-P2-001`） | 归 `SCHEMA-INTEGRATE-001`(W6)，`C-004.3`；本规格给双向映射 |
| R3 | `baseline_id` 字段归属与新字段命名 | 归 W6；本任务只冻结比较协议语义 |
| R4 | `exposure` 基线不在 `FZ-MODE-BASELINE` 枚举内 | 已用 `baseline_id` 分离，不扩展冻结枚举；登记 W6/W4 |
| R5 | 实现面（`IMPL-P1-PSFW-001`/`P2-INTEGRATE-001`）尚未存在 | 本规格为实施输入；不改生产码 |
| R6 | 真实数据验收（M42/银心）归 Wave 10 `REAL-SCIENCE-001` | 不在本任务范围 |
| S1 | `SO-01..SO-07` 需负责人签字项 | 只登记，不擅改（见 `SCI-ADJ-001_FREEZE_LIST` §7） |
| S2 | `CTRL-F1`（工作树≠HEAD）与 `CTRL-AR033`（根构建面 owner） | 控制器级，只登记不裁决 |

---

## 14. 声明

- 本规格不写生产源码、不改任何 `docs/science/*.md`、`docs/owner/**`、`docs/design/**`、`docs/references/**`、`contracts/**`；
- 未 commit/push/add/分支/worktree/stash/reset/clean/rebase；未派生子代理；未宣布发布；
- 全部命令带退出码并留日志于 `run/v6/alg-p2-psfsw/logs/`。
