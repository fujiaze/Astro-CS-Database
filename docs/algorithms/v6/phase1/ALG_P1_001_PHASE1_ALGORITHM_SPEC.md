# `ALG-P1-001` — Phase1 V6 算法实施规格（calibration covariance / PSF information / PSFSW 四分量 / Drizzle）

- 文档 ID：`ALG-P1-001-PHASE1-ALG`
- 任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/ALG-P1-001.md`（wave 3，`depends_on = SCI-ADJ-001`）
- 写域：`docs/algorithms/v6/phase1/`（仅此一处）
- 机器可读伴生：alg_p1_001_spec.json、alg_p1_001_test_matrix.json
- 可复跑验证器：`tools/verify_alg_p1_001.py`
- 基线：`HEAD = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`（执行时 `git rev-parse HEAD` 复核一致）
- 性质：**目标态算法规格 + 独立 Oracle + 负向 mutation**。本任务不改上位规范、不改冻结公式/容差/冻结门、不改生产源码、不 commit/push、不派生子代理。

> **F1 基线分歧（强制标注，只登记不裁决）**：工作树相对 HEAD 含预存回退（控制器 C-002 记录 16 tracked 回退 + 10 tracked 删除；本任务实测 27 tracked-M / 10 tracked-D）。控制器级事项 `CTRL-F1` / C-004.6，本任务**只登记**。本规格一切生产面描述以 **HEAD** 为准；所引科学锚点（`SCI-ADJ-001` / 三份 Phase 设计 / UNIFIED / PSF_SIGNAL_WEIGHT）均取自已提交 HEAD 的文档，不依赖回退态。

## 0. 范围与权威

本规格把 `SCI-ADJ-001`（Wave 2 科学裁决）冻结的口径离散化为 **Phase1 可实现、可验证**的算法与测试矩阵，交付四个算法域：

1. **校准 covariance** `ALG-P1-CAL-COV-001`：由观测模型 `d = A x + n` 的线性化给出逐像素方差与共享 master 的 covariance 表示；
2. **PSF information** `ALG-P1-PSFINF-001`：`A_NEA` 与 `PᵀC⁻¹P`（含空间模型标量降级门）；
3. **PSFSW 四分量提取** `ALG-P1-PSFW-001`：共同星集上的 signal / concentration / noise / background 与组内归一契约；
4. **球面 Drizzle** `ALG-P1-DRZ-SB/VAR/CORR/FLUX-001`：面亮度保持归一、方差/相关传播、条件通量守恒。

权威分层（宪章 §1.1）：FROZEN 宪章 > PROJECT_SPEC > 三份 PHASE 详细设计 > UNIFIED / PSF_SIGNAL_WEIGHT > 专项 SCI/ALG/DATA/API > 代码/测试 > 历史文档。本规格属 ALG 层，不得反向定义上位 SCI；冲突一律登记并交 CONTRACT-FREEZE-001(W4)。

**条款锚记法**：CONSTITUTION §x = ASTROCS_PROJECT_CONSTITUTION.md；DESIGN-P1-001 §x = `docs/design/PHASE1_DETAILED_DESIGN.md`；UNIFIED §x = `docs/science/UNIFIED_SCIENCE_MODEL.md`；SCI-PSFW-001 §x = `docs/science/PSF_SIGNAL_WEIGHT.md`；FZ-* = `docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md` §3；ADJ-* = `reports/v6/science-adjudication/adjudications.json`。

## 1. 冻结继承与单位表

### 1.1 必须原样继承的冻结集合

本规格继承 `SCI-ADJ-001` 的结构完整性 19 条（FREEZE_LIST §4）以及全部与本任务相关的单位/公式/门/适用域/降级/provenance 条目（机器表 `inherited_frozen_ids`，共 40 条）。关键项：

| 冻结条目 | 值 | 本规格落点 |
|---|---|---|
| `FZ-FORMULA-DRIZZLE-SB` | `S_p = Σ_j B_j a_jp / Σ_j a_jp` | §6.2 |
| `FZ-FORMULA-DRIZZLE-VAR` | `variance_p = Σ_j v_j w_jp² / D_p²` | §6.3 |
| `FZ-COND-FLUX-CONSERV` | pixfrac=1 严格 `Σ_p F_p = Σ_j x_j`；pixfrac<1 为 `pixfrac² · Σ_j x_j` | §6.4 |
| `FZ-GATE-CONST-SB` | 按 B0 构造；`abs(S_p/B0 − 1) < 1e-3`（沿用，不改数值） | §6.7 |
| `FZ-FORMULA-WINFO` | `W_info = a² PᵀC⁻¹P = 1/Var(F̂)` | §3.2 |
| `FZ-COND-WHITENOISE` | `W_info = a²/(σ_pix² A_NEA)`，`A_NEA = 1/ΣP²` | §3.3 |
| `FZ-FIELD-PSFSW-4COMP` | signal / concentration / noise / background 分别落产品 | §4.2 |
| `FZ-FORMULA-PSFSW-COMPOSITE` | `Wt_k = C_norm · S^α Conc^β / (N^γ B^δ)`；组内 median=1 | §4.4 |
| `FZ-GATE-PSFSW-COV` | `covariance.method = propagated_from_composite_coefficients` | §4.6 |
| `FZ-GATE-PSFSW-EPSF` | effective PSF 必输 | §4.6 |
| `FZ-GATE-PSFSW-FAILCLOSED` | unavailable 原因白名单；valid=false 时 weight=null | §4.7 |
| `FZ-GATE-MEDIAN-SNR` | median(SNR_F) 仅诊断，禁入权重面 | §3.6 / §4.8 |
| `FZ-DEGRADE-SCALAR` | 帧级标量双门 + `p05/p50/p95` + 覆盖 + 模型误差 + 适用域 | §3.2 |
| `FZ-PROV-SHARED-SYSTEMATIC` | 共享项三种允许表达并进 covariance 链 | §2.3 |
| `FZ-PROV-MINIMAL-SET` | provenance 最小集 | §5.2 |
| `FZ-MODE-PRODUCTION` | `{point_information, surface_gls, psfsw_robust}` | §5.3 |

### 1.2 冻结单位表（FREEZE_LIST §1，原样）

| 量 | 单位 | 方差单位 | ivar 单位 | 锚 |
|---|---|---|---|---|
| signal_sb（Phase1 Drizzle/HiPS 面亮度） | ADU/px² | ADU²/px⁴ | px⁴/ADU² | `FZ-UNIT-SIGNAL-SB` / VAR-SB / IVAR-SB |
| pixel_variance_in（输入 v_j） | ADU² | — | — | `FZ-UNIT-VAR-IN` |
| W_info | ADU⁻² | — | — | `FZ-UNIT-WINFO` |
| Q | ADU⁻¹ | — | — | `FZ-UNIT-Q` |
| flux F̂ | ADU | ADU² | ADU⁻² | `FZ-UNIT-FLUX` |
| psfsw_robust_weight | 1 | — | — | `FZ-UNIT-PSFSW` |
| A_NEA | px² | — | — | DESIGN-P1-001 §6 |
| B_j | ADU/px² | — | — | SCI-DRZ-001 §5 |
| x_j | ADU | ADU² | ADU⁻² | SCI-DRZ-001 §3/§5 |
| a_jp, A_pixel, A_drop, D_p | px²（球面立体角等价） | — | — | SCI-DRZ-001 §3 |
| w_jp, c_jp | 1 | — | — | 本规格 §6 |

**量纲律门**：`variance = signal²`、`ivar = 1/variance`、`W_info = signal⁻²`、`psfsw = 1`；Phase3 输出 `variance BUNIT = (signal BUNIT)²`（`FZ-P3-BUNIT-QUADRATIC`）。任一不一致即 REJECT（`ADJ-GEN-01`；CONSTITUTION §4.1）。

---

## 2. 校准 covariance —— `ALG-P1-CAL-COV-001`

**条款锚**：DESIGN-P1-001 §4.1/§4.2；PROJECT_SPEC §3；`ADJ-OBS-01` / `ADJ-F-OBS-04`；`FZ-PROV-SHARED-SYSTEMATIC` / `FZ-FORMULA-COV-PROP`；CONSTITUTION §4.1/§5.3。

### 2.1 信号（原样继承 `SCI-CAL-001` §5 口径）

```text
dark_opt = 0（dark 已含 bias）:           y_p = (r_p − d_p) / max(f_p, 0.1)                 [flat == NULL 时不除法]
dark_opt = 1（显式 bias/dark 分离）:      y_p = (r_p − b_p − α·(d_p − b_p)) / max(f_p, 0.1),  α = t_light / t_dark
```

**禁止**裁切负值、加未声明 pedestal 或夹紧（CONSTITUTION §5.3；`SCI-CAL-001` §9a）。校准路径至少 float32、累积用 float64（CONSTITUTION §5.3）。

### 2.2 方向导数与总方差

对 `y = (r − b − α(d − b))/f`：

```text
J = ∂y/∂(r, b, d, f) = [ 1/f,  −(1−α)/f,  −α/f,  −y/f ]
C_cal = J C_in Jᵀ ;  Var(y_p) = J C_in Jᵀ
```

**独立随机项**（逐像素对角）：

| term_id | 公式 | 单位 | 耦合 | 来源 |
|---|---|---|---|---|
| read_noise | `V_rn = (read_noise_e / gain)²` | ADU² | independent | detector metadata（gain [e⁻/ADU]、read_noise_e [e⁻]） |
| photon_light | `V_ph = max(r_p, 0) / gain` | ADU² | independent | light signal + gain |
| quantization | `V_q = q_adu² / 12` | ADU² | independent | ADC quantum q_adu（default 1 ADU，声明） |
| dark_photon | `V_dp = max(d_p, 0) / gain` | ADU² | independent | dark master signal + gain |

于是逐像素独立方差为

```text
Var_ind(y_p) = [ V_r,p + (1−α)² V_b,p + α² V_d,p + y_p² V_f,p ] / f_p²      (同一 bias master 只计一次)
V_r,p = V_rn + V_ph + V_q
```

其中 `V_b, V_d, V_f` 为对应 master 的自身估计方差。

> **OI-02（finding，登记交 W4）**：DESIGN-P1-001 §4.2 写作 `V(y) = {V(r) + V(b) + α²[V(d) + V(b)] + y²V(f)} / f²`（即 `V_b + α²V_b`）。该式**仅在 light-path 与 dark-path 的 bias 为不同 `master_id` 时**成立（两个独立项）。使用**同一** `master_id` 时，按 `ADJ-OBS-01`「共享 master 只进一次 covariance 通道」，必须折叠为 `(1−α)²`；否则把同一共享系统项重复计为随机项。本规格按同一 `master_id` 的精确形式实现，设计式作为「独立来源」情形保留；不修改上位设计文本，交 CONTRACT-FREEZE-001(W4) 登记。

### 2.3 共享 master 项（`FZ-PROV-SHARED-SYSTEMATIC` / `ADJ-OBS-01`）

共享 master（bias / dark / flat）在归一化帧组内**跨帧、跨像素相关**，必须进入 covariance 面。允许三种表达之一：

1. **低秩因子**：`C_shared = L Lᵀ`（存 L 因子与秩）；
2. **相关核**：`σ（scale）+ kernel`（存 kernel id/version 与尺度）；
3. **共同 master ID + 强度参数**：`master_id + α_m`（存 `master_id`、α_m、master 方差）。

强度参数（同一 master 折叠后）：`α_b = −(1−α)/f`、`α_d = −α/f`、`α_f = −y/f`。

master 自身方差：`V(master) = V_single_frame / N_combined + 已声明共同模式项`；combine 规则（mean / median / sigma-clip）与 `N_combined` 必须记录。

**三类量划分**（`ADJ-OBS-01`）：独立随机项 → 对角 variance/ivar；共享系统项 → covariance 面（低秩 / 相关核 / master ID + 强度参数）；模型偏差（光度零点、PSF 模型误差、WCS、UPM 参数）→ validity/quality + 系统误差预算 + 参数 covariance，**禁止伪装随机 ivar**。

### 2.4 fail-closed 与输出

- 缺少 gain 或 read_noise 且未声明 `variance_source = empirical_mad_fallback` 时 → unavailable / REJECT；
- 缺少 `master_id` / 归一版本 / 单位 → 单位不可判 → unavailable / REJECT（`FZ-BUNIT-SEMANTICS`）；
- `f_p ≤ 0` 或非有限 → REJECT（不得静默用 floor 造值）；
- 共享项无法表示且无系统误差预算 → unavailable（不得按独立项静默处理）。

输出：`v_cal,p`（ADU²）、`variance_source`、`master_ids`（含 combine 规则与 N_combined）、`shared_systematic`（三种表达之一）；全部进 provenance 最小集（`FZ-PROV-MINIMAL-SET`）。

> **OI-03（取代登记，交 W4/`SO-06`）**：`SCI-CAL-001` §1/§9a 的「不传播 variance / 不建模 gain-readnoise」与 V6 目标态（DESIGN-P1-001 §4.2、PROJECT_SPEC §3）冲突。按 PROJECT_SPEC §11 登记为待迁移基线，正式取代归 CONTRACT-FREEZE-001(W4)，**不改 FROZEN SCI 原文**。

**门与 mutation**：`CAL-COV-FORMULA`（J C Jᵀ 闭式）、`ADJ-OBS-01-SHARED`（联合 / 朴素 ratio > 1）、`CAL-UNIT`、`CAL-NO-CLIP`、`CAL-COV-REPRESENTATION`；对应 `M-C1` / `M-C2` / `M-C4` / `M-S13` / `M-S14`。

---

## 3. PSF information —— `ALG-P1-PSFINF-001`

**条款锚**：DESIGN-P1-001 §6/§8.1/§8.3；UNIFIED §4/§5/§8；SCI-PSFW-001 §2；`FZ-FORMULA-WINFO` / `FZ-COND-WHITENOISE` / `FZ-UNIT-WINFO` / `FZ-DEGRADE-SCALAR` / `FZ-GATE-MEDIAN-SNR`；`ADJ-P2-01`。

### 3.1 PSF 归一与噪声等效面积

```text
P_k(u,v;x,y) ≥ 0,   Σ_p P_k,p = 1         （含像素响应；支持域 S_P = {p : P_p > 0}）
A_NEA,k = 1 / Σ_p P_k,p²                   [px²]        （白色噪声下的噪声等效面积）
```

### 3.2 点源信息权重（唯一权威式）

```text
Q_k       = a_k P_kᵀ C_k⁻¹ d_k                              [ADU⁻¹]
W_info,k  = a_k² P_kᵀ C_k⁻¹ P_k = 1 / Var(F̂_k)              [ADU⁻²]
F̂         = Σ_k Q_k / Σ_k W_info,k ;   Var(F̂) = 1 / Σ_k W_info,k
```

`W_info` 默认是**空间量** `W_info(x,y)`；压成帧级标量必须同时过 (a) 空间残差/趋势门 与 (b) 功率损失门（对最终 flux bias / variance / detection power），并带 `p05/p50/p95` + 最大系统偏差 + 采样覆盖 + 模型误差 + 适用域；否则存 map / model / control points（`FZ-DEGRADE-SCALAR`；`ADJ-GEN-04`）。

### 3.3 白噪声条件式

```text
C_k = σ_pix,k² I   ⟹   W_info,k = a_k² / (σ_pix,k² · A_NEA,k)
```

仅条件成立（C 对角且 σ_pix 声明）；不得在 `C` 含相关项时无条件使用（`FZ-COND-WHITENOISE`）。

### 3.4 离散计算

- **对角 C**：`W = a² Σ_p P_p² / σ_p²`；
- **一般 SPD C**：对支持域窗口做 Cholesky，解 `C x = P`，`W = a² Pᵀx`；C 非正定或不可解 → REJECT；
- **低秩 `C = D + L Lᵀ`**：Woodbury，`C⁻¹P = D⁻¹P − D⁻¹L(I + LᵀD⁻¹L)⁻¹LᵀD⁻¹P`；
- **仅用对角近似 C~**：必须报告 `c~ᵀ C c~`（用**真实** C）与相对理想 `1/W` 的偏差比，**不得直接报告理想 1/W**（PROJECT_SPEC §3）。

### 3.5 最优性前提与注入验证

声明 `W_info` 最优需全部满足：模型正确、`ΣP = 1`、C 正确且可表示、高斯噪声或 CRLB 意义、目标为点源；跨帧相关必须用联合 C。注入单位点源验证 `σ_F = 1/√W` 与实测散度一致；改变源亮度分布不得改变 `W_info`，但会改变 median(source SNR)（DESIGN-P1-001 §11；`ADJ-P2-01`）。

### 3.6 禁止来源与诊断

`W_info` 不得来自 median_source_snr、support、coverage、fwhm、residual，也不得来自「仅 Drizzle 后逐像素 ivar」（会丢 PSF / 协方差）。PSF 拟合残差、q_psf、FWHM 仅 validity / 诊断（DESIGN-P1-001 §6 末段；`FZ-GATE-MEDIAN-SNR`）。

**门与 mutation**：`FZ-FORMULA-WINFO`（Q/W == GLS、Var = 1/W、a² 律）、`FZ-COND-WHITENOISE`、`FZ-WINFO-DIAG-APPROX`、`FZ-GATE-MEDIAN-SNR`、`FZ-DEGRADE-SCALAR`；对应 `M-W1` / `M-W2` / `M-W3` / `M-S12` / `M-S15` / `M-S16`。

---

## 4. PSFSW 四分量提取 —— `ALG-P1-PSFW-001`

**条款锚**：DESIGN-P1-001 §8.2/§10；DESIGN-P2-001 §6.3；SCI-PSFW-001 §3/§6/§7；`FZ-FIELD-PSFSW-4COMP` / `FZ-FIELD-PSFSW-UNIT` / `FZ-FORMULA-PSFSW-COMPOSITE` / `FZ-GATE-PSFSW-COV` / `FZ-GATE-PSFSW-EPSF` / `FZ-GATE-PSFSW-FAILCLOSED` / `FZ-GATE-MEDIAN-SNR`；`ADJ-P2-03`；控制包 RULINGS #3/#4/#5。

### 4.1 帧组定义

组 `G` = 同波段 × 同目标/重叠连通分量 × 光度已归一帧组。组内比较合法；跨组比较必须先声明 selection function。PSFSW 是**无量纲组内相对**量，不是 ivar，也不得冒充 W_info。

### 4.2 四分量（分别落产品）

| 分量 | 规范名 | 单位 | 定义 | 备注 |
|---|---|---|---|---|
| signal | psfsw.signal | ADU | `S_k = Σ_{s∈S_k} f̂_{k,s}`（PSF 拟合总 flux） | 不是帧总信号、不是孔径和 |
| concentration | psfsw.concentration | ADU/px | `Conc_k = mean_{s∈S_k} f̂_{k,s} / A_NEA,k` | **禁止**用 FWHM 或拟合残差代替 |
| noise | psfsw.noise | ADU | `N_k` = 稳健噪声（1.482602218505602·MAD 或 Sn 类，估计器与版本声明） | 不是像素 σ、不是 m5 |
| background | psfsw.background | ADU | `B_k` = 稳健均值背景（MMT 残差），必须 > 0 | 不是 support / coverage |

每分量必须带：¤`measurement_id`¤（四者互异）、`p05 <= p50 <= p95`、`valid_area_fraction ∈ [0,1]`、单位、估计器/版本。显著空间非均匀时拆 region/tile 或拒绝标量（SCI-PSFW-001 §6；`FZ-FIELD-PSFSW-4COMP`）。

### 4.3 共同星集与 selection function

共同星集 `S` 的成员判定必须**独立于待测帧的 PSFSW 测量本身**，否则选择函数与目标量耦合、跨帧比较不可解释。允许两条路径：外部参考星表（版本化哈希）；或参考叠加上的**单一门限**固定星表。**禁止**用逐帧检测阈值分别取星后交集成「共同星集」。

成员字段：¤`common_star_set_id`¤、¤`selection_function_id`¤、¤`member_hash`¤、¤`n_common`¤、排除旗标。排除项必须显式列举：saturated / blended / trailed / moving / psf_mismatch / edge_truncated。selection_correction ∈ {common_star_set, explicit_selection_function, both}。

### 4.4 复合与组内归一

```text
Wt_k      = C_norm(version) · S_k^α · Conc_k^β / (N_k^γ · B_k^δ)        α,β,γ,δ >= 0
W_psfsw,k = Wt_k / median_{j∈G}(Wt_j)                                   组内 median = 1 ; 全正
```

指数、截断、稳健估计器、C_norm 与归一常数**版本化并落产品**；标定样本不得与最终验收样本相同。数值由 W4 冻结（本任务只冻结结构与版本化要求，不擅改数值；`SO-07` 登记）。

### 4.5 Phase1 / Phase2 接口拆分（`OI-01`，交 W4 批准）

Phase1 是**单帧**产品，无法独立形成帧组。因此本规格规定：

- **Phase1 输出**：四分量 + **未归一** `Wt_k` + 归一契约（`normalization.scope = group`，`median_target = 1.0`，常量版本）+ validity；
- **组内归一** `W_psfsw,k = Wt_k / median_{j∈G}(Wt_j)` 在 Phase2 组装帧组后执行；
- Phase1 **不得**输出以单帧 median 造出的伪归一权重，也**不得**回退 median SNR。

> **`OI-01`（interface_ratification，交 CONTRACT-FREEZE-001(W4)）**：PHASE1 设计 §10 把 psfsw_robust（四分量 + 相对权重 + validity）列为 Phase1 产品；本规格将其解释为「PSFSW 产品族」的 Phase1 贡献 = 四分量 + 未归一复合 + 归一契约 + validity，最终相对权重由 Phase2 在帧组存在时产生。该拆分是接口决定而非科学变更，须 W4 批准后生效。

### 4.6 covariance 边界与 effective PSF

```text
α_k(p)     = W_psfsw,k · v_k(p) / Σ_j W_psfsw,j · v_j(p)      v = validity 门（0/1）
I_out(p)   = Σ_k α_k(p) d_k(p)
Var(I_out) = Σ_{k,l} α_k(p) α_l(p) [C_in]_{kl}(p)  ->  Σ_k α_k(p)² σ_k(p)²  (帧独立)
```

- 最终 covariance 只能 `C_out = R C_in Rᵀ`（实际组合系数）；¤`covariance.method` = propagated_from_composite_coefficients¤，¤`variance_from_weight` = false¤，¤`uses_relative_weight_as_ivar` = false¤；
- **禁止** `Var = 1/W_psfsw`、`Var = Σ_k W_psfsw,k` 或把任何诊断量当方差面（`FZ-GATE-PSFSW-COV`）；
- **effective PSF 必输**：实际组合算子对单位点源脉冲响应，与重采样核 `K_k` 和归一约定一起保存；只给 FWHM 标量不构成 effective PSF（`FZ-GATE-PSFSW-EPSF`）。定义与 conventional / matched-filter 形式见 SCI-P2-001 COVARIANCE_AND_EFFECTIVE_PSF §3。

### 4.7 fail-closed

unavailable 原因白名单：`{no_common_star_set, background_nonpositive_undefined_transform, insufficient_valid_stars, selection_bias_gate_failed, spatial_nonuniformity_gate_failed}`。¤valid = false¤ 时 ¤`weight_value`¤ 必须为 `null`；**禁止**回退 median source SNR（`FZ-GATE-PSFSW-FAILCLOSED`）。

### 4.8 诊断与 depth

Phase1 的 depth_m5 与 source_snr 为诊断/深度表达，不得作权重来源；帧级 median(SNR_F) 只允许登记为诊断（控制器 C-004.2 / `ADJ-C004-02` / `FZ-GATE-MEDIAN-SNR`）。

**门与 mutation**：`FZ-FIELD-PSFSW-4COMP`、`FZ-FIELD-PSFSW-UNIT`、`FZ-FORMULA-PSFSW-COMPOSITE`、`FZ-GATE-PSFSW-EPSF`、`FZ-GATE-PSFSW-FAILCLOSED`、`FZ-GATE-PSFSW-NOKEYS`、`FZ-PSFSW-COMMON-STAR-SET`、`FZ-PSFSW-RECORD`；对应 `M-P1`..`M-P9` 与 `M-S7`..`M-S10`。

---

## 5. 跨域合同

### 5.1 provenance 最小集

每个 Phase1 产品 manifest 至少记录（`FZ-PROV-MINIMAL-SET`；CONSTITUTION §4.3；UNIFIED §9）：产品类型 / schema 版本、软件完整 SHA、run ID、输入产品哈希、科学配置哈希、单位（BUNIT 语义与 `pixel_area_power`）、坐标 frame、像素/采样语义、算法 ID、module/provider、近似与降级（含 unavailable 原因）、归一/权重版本、相关核/低秩表示摘要、`flux_conservation_factor`（pixfrac² 条件项）、k_corr 所用值与适用域、生成时间与输出哈希。unavailable 必须显式登记（`uncertainty_available = false`，附原因）。

### 5.2 BUNIT 与量纲

`BUNIT` 必须量纲可判：显式含 px 幂次（canonical `ADU/px²` 与 `ADU²/px⁴`），或 `BUNIT = ADU` 时 provenance 声明 ¤`pixel_semantics` = surface_brightness¤、¤`pixel_area_power` = -2¤ 与目标像素面积；缺声明 = 单位不可判 → unavailable / REJECT（`FZ-BUNIT-SEMANTICS`）。

### 5.3 权重模式枚举

生产科学模式 = `{point_information, surface_gls, psfsw_robust}`；文档基线模式 = `{equal, pixel_ivar}`（仅基线比较，非科学最优）；延迟模式 = `{psf_snr_power}`（DEFERRED / NOT_IMPLEMENTED，不进 V6 生产路由，控制器 C-004.1）。legacy 整数 `{0 = support×snr², 1 = equal, 2 = ivar}` 被取代，`0` 不得进任何科学权重面。未知模式 / auto / support_x_snr2 出现即 REJECT（`FZ-MODE-PRODUCTION` / `FZ-MODE-DEFERRED` / `ADJ-S1`）。

---

## 6. 球面 Drizzle —— `ALG-P1-DRZ-SB/VAR/CORR/FLUX-001`

**条款锚**：DESIGN-P1-001 §9/§10/§11；UNIFIED §7；`FZ-FORMULA-DRIZZLE-SB` / `FZ-FORMULA-DRIZZLE-VAR` / `FZ-COND-FLUX-CONSERV` / `FZ-GATE-CONST-SB` / `FZ-FORMULA-COV-PROP` / `FZ-GATE-PARENT-VAR`；`ADJ-F-OBS-01`/02/03；`ADJ-S3`；`ADJ-AR-02`；SCI-DRZ-001 §5/§7（FROZEN，部分被取代）；ALG-DRZ-001。

### 6.1 记号与几何

`a_jp` = 源像素 j 与目标 HEALPix NESTED leaf p 的球面交叠面积；`A_pixel,j` = 源像素面积；`A_drop,j = pixfrac² · A_pixel,j`；`D_p = Σ_j a_jp`（覆盖面积）；`pixfrac ∈ (0,1]` 非法值显式拒绝、不夹逼。几何实现沿用 ALG-DRZ-001（三层候选缓冲、Sutherland–Hodgman 球面裁剪、FP64 面积）。

### 6.2 面亮度保持归一与组合系数

```text
B_j      = x_j / A_pixel,j                                          [ADU/px²]
S_p      = Σ_j B_j a_jp / Σ_j a_jp = Σ_j w_SB_jp x_j / D_p = Σ_j c_jp x_j
c_jp     = w_SB_jp / D_p      (作用于 x_j)          ;  c_B_jp = a_jp / D_p   (作用于 B_j)
```

**权重重定义（关键，交 `SO-02` 签字）**：历史 DRIZZLE.md §5 的 drop 权重 `w_legacy_jp = a_jp / A_drop,j` 与目标态 SB 权重满足

```text
w_SB_jp = a_jp / A_pixel,j = pixfrac² · w_legacy_jp
```

即在 V6 目标态下，最终信号层使用 SB 权重 `w_SB_jp`；历史式在 `pixfrac = 1` 时与目标一致（`A_drop = A_pixel`）。**任务卡「correlation c_jp = w_jp/D_p」在目标态以 w_jp := w_SB_jp 成立**；legacy `w_legacy` 仅作等价映射与 provenance 登记，不得用于 pixfrac<1 的信号层。

**常量面亮度不变量（`FZ-GATE-CONST-SB`）**：按面亮度 `B0` 构造 `x_j = B0 · A_pixel,j`，则 `S_p = B0` 对**全部** `pixfrac ∈ (0,1]` 成立，容差沿用 `abs(S_p/B0 − 1) < 1e-3`（不改数值）。禁止：`S_p = F_p`（漏 D_p 归一）；用每像素常量 ADU `x_j = C` 直接断言 `S_p = C`；把不变量写成对任意 pixfrac 无条件成立而不声明构造。

### 6.3 方差传播

```text
variance_p = Σ_j c_jp² v_j = Σ_j v_j (w_SB_jp)² / D_p²                 [ADU²/px⁴]
ivar_p     = 1 / variance_p                                            [px⁴/ADU²]
缩放律: x -> α x  =>  variance -> α² variance ,  ivar -> ivar / α²   (逐像素精确)
```

**等价说明**：`FZ-FORMULA-DRIZZLE-VAR` 的 `Σ_j v_j w_jp² / D_p²` 在与信号层同定义（即 `w_jp := w_SB_jp`）下逐字成立；若按 legacy drop 权重解释则只在 `pixfrac = 1` 成立。实施必须按实际组合系数传播（`FZ-FORMULA-COV-PROP`）。

### 6.4 条件通量守恒

定义面亮度层的覆盖积分通量 `Φ_out := Σ_p S_p D_p`：

```text
Φ_out = Σ_p S_p D_p = Σ_j B_j A_drop,j = pixfrac² · Σ_j x_j          (精确，全覆盖；线性恒等)
pixfrac = 1 时:  S_p D_p = F_p^legacy  且  Φ_out = Σ_j x_j           (两式退化同一式)
```

`flux_conservation_factor = pixfrac²` 必须写入 provenance；缺因子即不可用于绝对通量（`FZ-COND-FLUX-CONSERV`）。失效域：`pixfrac < 1` 且消费者按 legacy 权重直接把 `S_p` 当绝对面亮度（缺 `1/pixfrac²` 补偿）→ 光度零点偏差 `1/pixfrac²`；旧产品/旧算子必须显式声明归一版本（`ADJ-F-OBS-02`）。

### 6.5 相关与 aperture / 父级方差

```text
Cov(S_p, S_q) = Σ_j c_jp c_jq v_j                          (输入像素独立)
rho_pq        = Cov(S_p,S_q) / sqrt(Cov(S_p,S_p) Cov(S_q,S_q))
Var(Σ_p a_p S_p) = Σ_{p,q} a_p a_q Cov(S_p,S_q)             (aperture 精确二次型)
Var(S_parent)    = Σ_{p,q} (D_p D_q / (Σ D)²) Cov(S_p,S_q)
variance_parent_diag = Σ_p variance_p D_p² / (Σ D)²
deficit          = (exact − diag) / exact
```

- 输入像素独立时对角元严格等于 §6.3，非对角元一般非零；对角-only 是**严格下界**（权重与重叠面积非负），实测缺口可达 ~2.46×（aperture）/ 0.82（父级），不得声明精确（`ADJ-F-OBS-03` / `ADJ-AR-02`）；
- 只落对角 variance 必须另存相关核 `rho_ij` 或可重建算子摘要（`C_out = R C_in Rᵀ`）；
- 父级对角归约必须同时满足：声明 `lower_bound = true` 且不得用于 aperture/总量误差；另存相关核/算子摘要；给出 deficit 误差门。**阈值状态**：`PROPOSED_PENDING_OWNER_SIGNOFF_SO07`（提案 `0.20`）；未签字生效前该 variance 面不得声明精确（`SO-07`）。

### 6.6 边界 / 单位 / fail-closed

- pixfrac ≤ 0 或 > 1、RING ordering、多通道、缺 WCS/尺度非法 → 显式拒绝（ALG-DRZ-001 §5）；
- 源像素 NaN/Inf 经 `F_p` 直接传播，drizzle 层不掩膜；非有限值由下游积分 INVALID_INPUT 合同处理；
- 写盘 signal/variance 必须满足二次律；单位不可判 → unavailable / REJECT（`FZ-BUNIT-SEMANTICS` / `FZ-P3-BUNIT-QUADRATIC`）。

### 6.7 门与负向 mutation

`FZ-GATE-CONST-SB`（常量面亮度）、`FZ-FORMULA-DRIZZLE-VAR`（方差/缩放律）、`FZ-COND-FLUX-CONSERV`（pixfrac² 因子）、`FZ-DRZ-CORRELATION`（非对角）、`FZ-DRZ-APERTURE`（严格下界）、`FZ-GATE-PARENT-VAR`、`FZ-DRZ-SB-DEF`、`FZ-DRZ-FLUX-PROV`；对应 `M-D1`/`M-D2`/`M-D3`/`M-D4`/`M-D5`/`M-D6`/`M-D7`/`M-S11`。

---

## 7. 测试矩阵与验证

- 人读矩阵：`ALG_P1_001_TEST_MATRIX.md`（34 行，逐行给输入/期望/容差/门/条款锚）；
- 机器可读矩阵：`alg_p1_001_test_matrix.json`；
- 验证器：`tools/verify_alg_p1_001.py`（38 项基线门 + 39 项负向 mutation；含规格/测试矩阵门覆盖一致性门）。

容差政策：只沿用已冻结数值容差（常量场 `abs(S_p/B0−1) < 1e-3`、缩放律精确、GLS/Q-W rel < 1e-9 类）。本规格提案但未签字的阈值一律标 PENDING_SO07，未签字时相应面 fail-closed（不得声明精确/可用）。

## 8. 未决风险与需裁决事项

### 8.1 需负责人签字项（只登记，本任务无权签署）

`SO-01`（DRIZZLE §3 术语/单位修正）、`SO-02`（F-OBS-02/S2 面亮度归一改为 (B) + A_drop→A_pixel）、`SO-03`（S3 常量场 Oracle 判据取代 DRIZZLE §11）、`SO-04`（AR-030/AR-031 协方差产品非目标声明取代）、`SO-05`（AR-036/AR-019/AR-026 宪章 §10.5/§17.6）、`SO-06`（AR-032 非 v6 SCI 迁移/取代清单，含 `SCI-CAL-001` §9a variance 不传播）、`SO-07`（F-OBS-03/04/05 与 surface_gls epsilon 的数值阈值/数据面/标定脚本，含父级 deficit threshold 与非均匀/深度门阈值）。

### 8.2 控制器级事项（只登记不裁决）

`CTRL-F1`（工作树 != HEAD：27 tracked-M / 10 tracked-D）、`CTRL-AR033`（根构建面无 V6 owner）、`CTRL-AR034`（历史 CI 红）。

### 8.3 开放项

- **`OI-01`**（interface_ratification，W4）：Phase1 单帧 → 四分量 + 未归一 Wt + 归一契约；组内 median=1 归一归 Phase2（§4.5）；
- **`OI-02`**（finding，W4/负责人）：DESIGN-P1-001 §4.2 的 `V_b + α²V_b` 与同 master_id 的 `(1−α)²` 合并关系（§2.2）；
- **`OI-03`**（supersede_registration，W4/`SO-06`）：`SCI-CAL-001` §9a/§1 不传播 variance / 不建模 gain-readnoise 与 V6 目标冲突（§2.4）；
- **`OI-04`**（coverage_gap，DOC-CONVERGE-001(W12)/控制器）：`docs/algorithms/v6/`** 未登记进 `docs/DOCUMENT_INDEX.yaml`，check_doc_index.py 的 docs_fully_covered 现为红（与 Wave1 `docs/science/v6/`** 同源）；
- **`OI-05`**（threshold_pending，ALG-P2-PSFSW-001(W3)/W4 `SO-07`）：common star set `n_common` 下限、selection bias 深度稳定性阈值、四分量非均匀拆 tile 阈值未冻结。

## 9. 声明

- 未 commit / push / git add；未建分支或 worktree；未 stash/reset/clean/rebase；git 仅只读使用。
- 仅写 `docs/algorithms/v6/phase1/`（唯一 write_scope）；未改 `docs/science/`*.md、`docs/owner/`**、`docs/design/`**、`docs/references/`**、生产源码、CI。
- 未修改任何冻结公式、容差或冻结门；`SO-01`..07 只登记不擅改；F1 / AR-033 / AR-034 只登记不裁决。
- 未派生子代理；未宣布任何发布或 VERIFIED 状态。
