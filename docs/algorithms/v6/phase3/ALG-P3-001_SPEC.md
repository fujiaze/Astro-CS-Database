> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# ALG-P3-001 — Phase3 算法实施规格（三模式采样 / 传播 / QW / effective PSF / 流式原子 FITS）

> 文档 ID：`ALG-P3-001-SPEC`
> 状态：`ALG_PROPOSED`（W4 `CONTRACT-FREEZE-001` 正式冻结）
> 上位：`DESIGN-P3-001` §1–§7；`ASTROCS-SCIENCE-MODEL-001` §2/§4/§6/§7/§8/§9/§10/§11；
>      `SCI-PSFW-001` §2/§4/§5；`SCI-P3-001-REVIEW` C-P3-PROP-1..16；
>      `SCI-P2-001-COVARIANCE-EPSF` §0/§1/§3/§5；`SCI-P2-001-WEIGHT-GATE` §2；
>      冻结条目见 `SCI-ADJ-001_FREEZE_LIST.md`。
> 文献锚：Fruchter & Hook 2002, PASP 114,144；Horne 1986, PASP 98,609（DOI:10.1086/131801）；
>      Naylor 1998, MNRAS 296,339；Zackay & Ofek 2017, ApJ 836,187/188（arXiv:1512.06872/1512.06879）；
>      Calabretta & Greisen 2002, A&A 395,1077（Paper II）；Greisen & Calabretta 2002, A&A 395,1061（Paper I）；
>      Górski et al. 2005, ApJ 622,759（HEALPix）。

---

## 1. 范围、符号与统一重采样模型

### 1.1 范围（ALG-P3-001）

本规格定义 Phase3 的**算法层**：三输出模式的采样语义、`C_y = R C_x Rᵀ` 协方差传播、输出帧 Q/W 重算、
effective PSF 传播、流式原子 FITS、采样核 registry 与独立 Oracle 验收。它**不**定义 Phase1/2 上游科学
（`point_information`/`surface_gls`/`psfsw_robust` 的构造）、不写生产源码、不改冻结门与容差。

锚：任务卡 ALG-P3-001；`DESIGN-P3-001` §1；`00_READ_FIRST.md`；C-004.5。

### 1.2 符号

| 符号 | 含义 | 单位 | 锚 |
|---|---|---|---|
| `x_j` | 输入 HiPS 像素值（面亮度语义） | ADU/px² | `FZ-UNIT-SIGNAL-SB` |
| `Omega_j` | 输入像素立体角 | sr | Paper II §5/§6 |
| `d_j = x_j Omega_j` | 输入像素积分通量 | ADU | `SCI-P3-001-REVIEW` §1 |
| `y_i` | 输出像素 SB（`surface_brightness`） | ADU/px² | `FZ-UNIT-SIGNAL-SB` |
| `Omega′_i` | 输出像素立体角（投影 Jacobian） | sr | `DESIGN-P3-001` §3 |
| `f_i` | 输出像素积分通量（`f = S d`） | ADU | `SCI-P3-001-REVIEW` §1 |
| `R` | SB 语义线性重采样算子（行归一） | 1 | `SCI-P3-001-REVIEW` C-P3-PROP-1/2 |
| `S` | 通量语义线性重采样算子（列归一） | 1 | 同上 |
| `p_j` | 输入 effective PSF（`Σ_j p_j = 1`） | 1 | `DESIGN-P1-001` §6 |
| `pi_i = (S p)_i` | 输出 effective PSF | 1 | `SCI-P3-001-REVIEW` C-P3-PROP-11 |
| `C_x` / `C_y` | 输入/输出 covariance | (BUNIT)² | `UNIFIED` §7 |
| `a` | 光度响应尺度（归一到公共通量尺度后） | 1 | `DESIGN-P1-001` §7 |
| `rho_ij` | 输出相关核 `[C_y]_ij/sqrt([C_y]_ii[C_y]_jj)` | 1 | `DESIGN-P3-001` §4 |

### 1.3 统一线性模型（ALG-P3-002 的前置）

```text
投影 Jacobian:   Omega′_i = |det( d(sky)/d(pixel) )|_i
SB 算子（行归一）:  R_ij = |Omega_j ∩ Omega′_i| / Omega′_i ,   Σ_j R_ij = 1
通量算子（列归一）: S_ij = |Omega_j ∩ Omega′_i| / Omega_j ,   Σ_i S_ij = 1
                 S_ij = R_ij · Omega′_i / Omega_j
输出:              y = R x ;   f = S d
```

**R 与 S 不可互替**：`Omega′_i != Omega_j` 时数值不同；行归一只保证常量面亮度不变量，列归一只保证点源总通量守恒
（`SCI-P3-001-REVIEW` C-P3-PROP-2；`DESIGN-P3-001` §3）。

独立 Oracle 实测（`run/v6/alg-p3/data/oracle_results.json`）：
- 行和误差 `max|R·1 − 1| = 1.11e-16`（0）；`max|Σ_j R_ij − 1| = 0`；
- 列归一与点源通量守恒相对误差 `5.10e-12`；`Σ pi = 0.9999999999949`；
- 常量面亮度场偏差 `max|y − B0| = 4.44e-16`（B0=3.7）。

---

## 2. 三模式采样（ALG-P3-002）

### 2.1 模式集合（FZ-P3-MODES）

生产模式 = `{surface_brightness, point_source_flux, visualization}`；`psf_snr_power` 为 DEFERRED 不进生产
（C-004.1）；legacy `{auto, support_x_snr2, 0, 1, 2}` 不得出现在生产枚举。
锚：`FZ-P3-MODES`；`FZ-MODE-DEFERRED`；ADJ-C004-01；ADJ-S1；`DESIGN-P3-001` §1:11-13。

### 2.2 逐模式语义与输入输出

| 模式 | 采样算子 | 主 HDU | 单位 | 必需输入 | 必需输出层 | measurement_capable |
|---|---|---|---|---|---|---|
| `surface_brightness` | `y = R x`（行归一） | `y` | 输入 SB BUNIT | signal、`C_x`（或对角+相关核）、coverage、validity；做 flux 换算时 `Omega′` | PRIMARY、VARIANCE/IVAR、COVERAGE、VALIDITY、SUPPORT、correlation、effective PSF、provenance | true |
| `point_source_flux` | `pi = S p`，`f = S d`（列归一） | `Q`/`W`/`F_hat`/`detection` | `Q=ADU⁻¹`、`W=ADU⁻²`、`F_hat=ADU` | 输入 PSF `p`（`Σp=1`）、`C_x`、`a`、`point_information` 或可由逐帧 PSF+cov 重建 | PRIMARY、POINT_INFORMATION/W、effective PSF、COVERAGE、VALIDITY、provenance | true |
| `visualization` | 显示拉伸（非科学采样） | display | display | 源产品 | PRIMARY + provenance（拉伸/降级参数） | **false** |

锚：`DESIGN-P3-001` §1/§3/§5；`SCI-P3-001-REVIEW` C-P3-PROP-4/16；`UNIFIED` §9；
`ASTROCS-CONSTITUTION-001` §7.1/§7.3/§4.1。

### 2.3 算法（可实施伪代码）

```text
function phase3_sample(hips, mode, wcs_plan, kernel_id):
  assert mode in {surface_brightness, point_source_flux, visualization}
  assert kernel_id in registry.production_kernels(mode)          # FZ-P3-KERNEL-REGISTRY
  for band in row_bands(wcs_plan.shape):                          # 流式；内存 O(W·band_h + tile_cache)
    if cancelled(band): abort_without_publish()                   # 原子性：无可见半成品
    for (i, sky) in pixels(band):
      Omega_i = |det(dsky/dpixel)|(i)                             # 必需：逐像素面积元
      j_list  = healpix_locate(sky)                               # 球面几何，禁平面距离替代
      w       = kernel_weights(j_list, kernel_id)                 # registry 核；Σw 语义由核声明
      if mode == surface_brightness:  y[i] = Σ_j w_SB · x[j]
      if mode == point_source_flux:   f[i] = Σ_j w_flux · d[j]
      coverage[i], validity[i] = coverage_and_validity(j_list)    # 缺 tile -> NaN, 禁零填
  return product
```

### 2.4 适用域与硬约束

1. **球面几何一致**：反向映射与 HEALPix 定位必须在球面完成，禁用平面近似替代（`DESIGN-P3-001` §3）。
2. **逐像素面积元**：`Omega′_i` 随天区变化。独立 Oracle 实测 CAR 在 CRVAL2=0、±60° 视场内
   `max/min = 2.0000`；TAN（dec=+60° 心、12° 场）`max/min = 1.445`；0.2° 小场内球面四边形 `max/min = 1.00015`；
   自实现球面盈余与 Paper II 解析纬度带面积元交叉一致到 `1.27e-6`（残差为球面四边形大圆边与纬度小圆边的曲率差，
   与 `SCI-P3-001-REVIEW` P3-O7 同源）。把 `Omega` 当常数会产生系统通量/面亮度偏差 → **拒绝**。
   锚：`CONSTITUTION` §7.3；`DESIGN-P3-001` §2/§3；Calabretta & Greisen 2002 §5/§6；`SCI-P3-001-REVIEW` C-P3-PROP-3。
3. **缺失/非有限不零填**：`coverage=0` 或输入 NaN → 输出 NaN + provenance；零填污染 flux 积分与匹配滤波 → **拒绝**。
   锚：`SCI-P3-001-REVIEW` C-P3-PROP-6；`DESIGN-P3-001` §3/§5；`UNIFIED` §9。
4. **插值不制造信息**：`order_sel < order_needed` 时输出被过采样，R 只能插值；逐像素方差下降来自平滑与相关，
   点源信息 `W` 被输入 PSF/order 封顶。锚：`SCI-P3-001-REVIEW` C-P3-PROP-5；Naylor 1998。
5. **visualization 不得冒充测量**：`measurement_capable=false`，禁写 VARIANCE/IVAR/POINT_INFORMATION 作测量层。
   锚：`DESIGN-P3-001` §1；`FZ-P3-FAILCLOSED`。

---

## 3. 协方差传播 `C_y = R C_x Rᵀ`（ALG-P3-004 / ALG-P3-005）

### 3.1 一般式

```text
C_y    = R C_x Rᵀ
Var(y_i) = Σ_j R_ij² [C_x]_jj + 2 Σ_{j<k} R_ij R_ik [C_x]_jk
rho_ij = [C_y]_ij / sqrt([C_y]_ii [C_y]_jj)
```

锚：`FZ-FORMULA-COV-PROP`；`DESIGN-P3-001` §4:41-47；`UNIFIED` §7；`SCI-P3-001-REVIEW` C-P3-PROP-7；
`SCI-P2-001-COVARIANCE-EPSF` §0；RULINGS.md #5；Fruchter & Hook 2002。

### 3.2 对角公式的严格适用域（ALG-P3-005，F3-02 处置）

现行生产 `var_out = Σ_k c_k² u_k` **严格等于** `diag(R C_x Rᵀ)` 当且仅当 **输入 `C_x` 对角**。

- 独立 Oracle：对角输入时 `max rel err = 0.0`（`variance_diagonal_input`）。
- Drizzle 产生的 HiPS 存在相邻相关（`UNCERTAINTY_AND_COVARIANCE.md` 记录 nside=512 `mean|rho|≈0.19`）。
  独立 Oracle 在 `rho=0.19` 下实测对角省略的相对亏损中位数 **23.31%**，完整式与固定种子 MC 一致
  （中位相对误差 **0.387%**）。
- 因此只输出对角 variance 时，**必须**同时给相关核 `rho_ij` 或可重建算子摘要 + 近似误差，并过误差门；
  否则 **拒绝**（`G-P3-COV-01`/`G-P3-SB-04`）；`coverage` 不得代替 variance。

锚：`SCI-P3-001-REVIEW` C-P3-PROP-8/9；`FZ-P3-FAILCLOSED`；ADJ-AR-02；F3-01/F3-02；`PROJECT_SPEC` §3。

### 3.3 完整 `C_y` 与匹配滤波（点源信息）

点源匹配滤波的 `W = piᵀ C_y⁻¹ pi` 必须使用完整 `C_y`（或带相关核的等价表示）：

- 独立 Oracle（白噪声输入、`C_x=σ²I`）：完整 `C_y` 下 `claimed_var = actual_var`（rel err `0.0`）；
- 只保留对角 `C_y` 时，声明方差 `21.71` 比真实方差 `30.45` **低 28.71%**（过度乐观）→ 必须被检出并 **拒绝**。

锚：`DESIGN-P3-001` §4；`SCI-P3-001-REVIEW` C-P3-PROP-9；`SCI-P2-001-COVARIANCE-EPSF` §0/§1；
`G-P3-QW-04`；Horne 1986；Fruchter & Hook 2002。

### 3.4 禁止

- 从权重标量/诊断量反推 variance（`variance_from ∈ {weight, psfsw_robust_weight, median_source_snr, support,
  coverage, fwhm, psf_residual, …}`）→ **拒绝**（`G-P3-COV-02`/`G-P3-GLB-01`）。
- `coverage` 当 variance → **拒绝**（`FZ-GATE-SUPPORT-COVERAGE`、ADJ-AR-02）。
- 上游无量纲相对复合权重写入 ivar/variance → **拒绝**（`FZ-GATE-PSFSW-COV`、RULINGS #5）。

---

## 4. Q/W 输出帧重算（ALG-P3-006）

### 4.1 冻结式（`FZ-P3-QW-RECOMPUTE`）

```text
pi   = S p                                  # 输出 effective PSF（flux 归一）
Q    = a · piᵀ C_y⁻¹ f
W    = a² · piᵀ C_y⁻¹ pi
F_hat = Q / W
Var(F_hat) = 1 / W
```

全部量在**输出帧**计算，用**输出有效 PSF `pi`** 与**完整（或带相关核的）`C_y`**。

锚：`FZ-P3-QW-RECOMPUTE`；`SCI-P3-001-REVIEW` C-P3-PROP-14:175-194；ADJ-P3-01；
`SCI-P2-001-COVARIANCE-EPSF` §1；`UNIFIED` §4；Horne 1986；Zackay & Ofek 2017 I。

### 4.2 `R` 与匹配滤波不可交换（禁重采样输入 Q/W）

独立 Oracle 白噪声反例：输出帧 `W_out = 0.033248`；把输入帧逐像素信息场 `W_in,j = p_j²/σ²` 按行归一权重重采样到
输出得到的 `W_naive = 0.002740`；**相对差 91.76%**。用 `W_naive` 预测方差与 MC 相差 **91.70%**，
而输出帧重算与 MC 一致（`0.715%`）。

因此：`W_out ≠ Σ_i (重采样核权重) W_in,i`；**禁止**由重采样输入 Q/W 得到输出 Q/W（`G-P3-QW-01`），
**禁止** `W = Σ W_in`（`G-P3-QW-02`），`frame` 必须为 `output_frame_recompute`。

锚：`FZ-P3-QW-RECOMPUTE`；`SCI-P3-001-REVIEW` C-P3-PROP-14；`DESIGN-P3-001` §3/§4。

### 4.3 上游 `W_info` 的消费边界

Phase3 **消费**上游 `point_information`/`W_info`（登记于 provenance），**不重算、不替换**：
`psfsw_robust_weight` 等无量纲组内相对复合权重**不得**写入 ivar/variance，也不得冒充 `W_info`
（`G-P3-QW-03`、`G-P3-GLB-01`）。

> 一致性说明：4.1 的“输出帧重算”指 Phase3 自产的 Q/W 层；4.3 的“不重算不替换”指上游 `W_info` 输入面。
> 二者不矛盾：输出帧 `W` 是新对象，provenance 记 `w_recomputed_in_output_frame=true`、`input_w_resampled=false`。

锚：`SCI-P3-001-REVIEW` C-P3-PROP-15；`SCI-PSFW-001` §5/§8；`FZ-GATE-PSFSW-COV`；`FZ-GATE-MEDIAN-SNR`；
`FZ-GATE-SUPPORT-COVERAGE`；C-004.2。

---

## 5. effective PSF 传播（ALG-P3-007）

### 5.1 定义（操作式）

```text
flux 归一（面亮度产品）:  pi_i = (S p)_i ,   Σ_i pi_i = 1
peak 归一（点源/检测）:    使用 Phase2 §3 操作式定义，并按产品族显式声明归一约定
```

Phase3 输出的 effective PSF 是**实际采样算子对单位通量点源的脉冲响应**，即对输入 effective PSF 施加同一重采样核。

锚：`SCI-P3-001-REVIEW` C-P3-PROP-11；`SCI-P2-001-COVARIANCE-EPSF` §3；`FZ-GATE-PSFSW-EPSF`；
`DESIGN-P3-001` §3/§4；`UNIFIED` §9；Horne 1986；Naylor 1998。

### 5.2 硬约束

1. **必输**：`surface_brightness` 与 `point_source_flux` 在 `measurement_capable=true` 时必须给非空
   `effective_psf_id` 与实际脉冲响应；**只给 FWHM 标量不构成 effective PSF** → 拒绝（`G-P3-EPSF-01`、`G-P3-PSF-06`）。
2. **不得机械套用 signal 插值规则**：点源匹配滤波必须用 `pi`；把输出 PSF 当单像素 δ（或沿用输入帧 PSF）产生大偏差。
   独立 Oracle 实测 δ 近似通量偏差 **94.77%**，而正确传播 `pi` 通量偏差 **2.74e-5**、`Σpi=0.9999999999949`。
3. **FWHM/EE 必须从 `pi` 测量**，不得由逐帧 FWHM 摘要代替（`SCI-P2-001-COVARIANCE-EPSF` §3；C6）。
4. **PSF 缺失即 fail-closed**：输入无 PSF 层或 `ΣP≠1` 时 `point_source_flux` 必须拒绝或显式 unavailable，
   不得静默退化（`G-P3-PSF-01/02`）。

---

## 6. 单位与 BUNIT 二次律（ALG-P3-009）

| 对象 | 单位 | 门 |
|---|---|---|
| `signal_sb` | ADU/px² | BUNIT 量纲可判 |
| `pixel_variance_in` | ADU² | 量纲代数 |
| `sb_variance_out` | ADU²/px⁴ | `variance = signal²` |
| `phase3_signal_out` | BUNIT（ADU/px² 或 ADU） | 显式 px 幂次 或 `pixel_semantics=surface_brightness + pixel_area_power=-2` |
| `phase3_var_out` | (主 HDU signal BUNIT)² | 非二次律即拒（`G-P3-SB-03`） |
| `W_info` | ADU⁻² | `W_info = signal⁻²`；禁与 psfsw 混名 |
| `Q` / `F_hat` | ADU⁻¹ / ADU | `Q/W` 量纲自洽 |
| `psfsw_robust_weight` | 1 | 无量纲门（禁 flux⁻²/ivar） |

`ivar = 1/variance`；variance 与 ivar 语义择一且一致。
锚：`FZ-UNIT-*`、`FZ-P3-BUNIT-QUADRATIC`、`FZ-BUNIT-SEMANTICS`、`FZ-FIELD-PSFSW-UNIT`、ADJ-GEN-01；
`DATA_SEMANTICS` §30.4:2475；`DESIGN-P3-001` §5。

---

## 7. 流式原子 FITS（ALG-P3-008）

### 7.1 HDU 布局与关键字

- PRIMARY：所选科学 signal/flux/statistic；
- 扩展 HDU：`VARIANCE`/`IVAR`（语义择一且一致）、`COVERAGE`、`VALIDITY`、`SUPPORT`、`REJECTION`（若存在）、
  `POINT_INFORMATION`/`W`（若模式需要）、PSF 表/图与 correlation 描述；
- 标准 WCS（TAN/SIN/CAR/AIT）、`BUNIT`、`DATASUM`/`CHECKSUM`；FITS 1-based 关键字与内部 0-based 像素中心转换唯一；
- **所有 HDU shape/WCS 对齐**。

锚：`DESIGN-P3-001` §5:49-57；`PROJECT_SPEC` §9；Greisen & Calabretta 2002；Calabretta & Greisen 2002。

### 7.2 流式与内存

输出按行带/块执行，内存 `O(width × band_height + tile_cache)`，不允许整幅超大图常驻；
cache 只缓存，不改变 order/核/科学值；线程预算来自 Runtime，**输出科学值与 block/cache/worker 数无关**。

锚：`DESIGN-P3-001` §6:59-61；`PROJECT_SPEC` §9；`CONSTITUTION` §10.4。

### 7.3 原子发布（fail-closed）

```text
1. 写临时文件（同目录，保证 rename 原子）
2. flush / close / fsync
3. 计算并写 DATASUM/CHECKSUM
4. 原子 rename 到目标
5. 重开并独立验证（shape/WCS/层完整性/单位）
失败或取消：删除 tmp，rename 不发生 → 无可见半成品
```

锚：`DESIGN-P3-001` §5:57；`CONSTITUTION` §4.3/§14.4；`PROJECT_SPEC` §9。

### 7.4 provenance 最小集

`source_product_id`、`source_product_hash`、`software_full_sha`、`run_id`、`config_hash`、
`input_manifest_hash`、`unit`、`pixel_semantics`、`pixel_area_power`、`wcs_frame`、`projection`、
`sampler_kernel_id`、`order_sel`、`order_native`、`approximation`、`degradation_reason`、
`weight_version`、`normalization_version`、`correlation_kernel_summary`、`flux_conservation_factor`、
`k_corr`、`generation_time`、`output_hash`。

`unavailable` 必须显式登记（`uncertainty_available=false` + 原因），禁止占位/静默缺键。
锚：`FZ-PROV-MINIMAL-SET`；`FZ-COND-FLUX-CONSERV`；`FZ-PROV-KCORR`；ADJ-GEN-03；`CONSTITUTION` §4.3；`UNIFIED` §9。

---

## 8. 三模式 fail-closed 门（ALG-P3-010，`FZ-P3-FAILCLOSED`）

共 **12** 条模式规则（machine gate：`run/v6/alg-p3/tools/alg_p3_gate.py`）：

| # | 门 | 条件（命中即 REJECT） | 锚 |
|---|---|---|---|
| 1 | `G-P3-SB-01` | SB 模式做 flux 换算却无逐像素 `Omega` | C-P3-PROP-16；F3-03 |
| 2 | `G-P3-SB-02` | 声明测量却 `uncertainty_available=false` 且无原因 | C-P3-PROP-16；ADJ-GEN-03 |
| 3 | `G-P3-SB-03` | variance BUNIT ≠ signal BUNIT² | `FZ-P3-BUNIT-QUADRATIC` |
| 4 | `G-P3-SB-04` | 只出对角 variance 且无相关核/近似误差 | `FZ-P3-FAILCLOSED`；ADJ-AR-02 |
| 5 | `G-P3-PSF-01` | `point_source_flux` 缺 PSF 层 | C-P3-PROP-13 |
| 6 | `G-P3-PSF-02` | PSF 未归一 `ΣP≠1` | C-P3-PROP-13 |
| 7 | `G-P3-PSF-03` | 缺 `point_information` 且不可重建 | C-P3-PROP-16 |
| 8 | `G-P3-PSF-04` | covariance 仅对角且无相关核 | ADJ-AR-02 |
| 9 | `G-P3-PSF-05` | 缺光度尺度 `a` | C-P3-PROP-16 |
| 10 | `G-P3-PSF-06` | 未输出 effective PSF | `FZ-GATE-PSFSW-EPSF` |
| 11 | `G-P3-VIS-01` | visualization `measurement_capable=true` 或写测量层 | `DESIGN-P3-001` §1 |
| 12 | `G-P3-GLB-01` | 任一模式把上游相对复合权重写成 ivar/variance | C-P3-PROP-15；RULINGS #5 |

每条均有一个负向 mutation（`M-P3-*`）使门变红（见 VERIFICATION §4）。

---

## 9. 禁止项（ALG-P3-012）

- **诊断别名不得进 `weight.sources`/`variance_from`**：`median_source_snr, median_snr, source_snr_median,
  med_source_snr, support, support_area, coverage, coverage_area, fwhm, psf_fwhm, median_fwhm, source_fwhm,
  residual, psf_residual, psf_fit_residual, fit_residual, psfsw_robust_weight, psfsw`。
  锚：`FZ-GATE-MEDIAN-SNR`；`FZ-GATE-SUPPORT-COVERAGE`；`UNIFIED` §3/§11；C-004.2；`PROJECT_SPEC` §4/§7。
- **`psf_snr_power` 不进生产路由**（C-004.1；`FZ-MODE-DEFERRED`）。
- **psfsw 无量纲相对权重不写 ivar/variance、不冒充 `W_info`**（`FZ-GATE-PSFSW-COV`、`SCI-PSFW-001` §5/§8）。
- **禁止**重采样输入 Q/W、上游 `W_info` 重算/替换、缺 tile 零填、visualization 写测量层。

> 词表归一归 W6 `SCHEMA-INTEGRATE-001`（C-004.3）；本规格只给语义与取值域，不发明第三套词表。

---

## 10. 下游接口与验证要求（ALG-P3-011）

| 下游任务 | 波次 | 消费条款 |
|---|---|---|
| `IMPL-P3-PROJ-001` | W5 | ALG-P3-002/003/008（投影、核 registry、流式 FITS） |
| `IMPL-P3-RSMP-001` | W5 | ALG-P3-002/003/004/005/006/007 |
| `IMPL-AIO-001` | W5 | ALG-P3-008 |
| `DATA-DESIGN-001` | W3 | ALG-P3-009（单位/BUNIT/逐像素 `Omega` 对象） |
| `SCHEMA-INTEGRATE-001` | W6 | ALG-P3-012（词表归一） |
| `P3-INTEGRATE-001` | W7 | ALG-P3-010/011（fail-closed 与 Oracle 门） |
| `CONTRACT-FREEZE-001` | W4 | 全部条款与门 |

验证要求（强制，详见 `ALG-P3-001_VERIFICATION.md`）：
1. 每个科学量有解析/独立高精度/MC Oracle，且 Oracle 不调用生产实现；
2. 每个近似/压缩有负向 mutation 使门变红（rc≠0）；
3. 注入点源验证理论 `Var(F_hat)=1/W` 与实测 flux dispersion；
4. 常量面亮度、点源通量、variance/correlation、effective PSF 传播、Q/W 输出帧重算、缺 tile/取消均覆盖；
5. 不同 block/cache/worker 输出科学值一致。

锚：`CONSTITUTION` §13.1/§14.4；`PROJECT_SPEC` §8；`UNIFIED` §10；`DESIGN-P3-001` §7。

---

## 11. 治理登记（只登记不裁决）

- **需负责人签字**：`SO-07`（F-OBS-03/04/05 数值阈值/数据面/标定；Phase3 相关核近似误差阈值 `epsilon_corr`
  的具体数值由 `CONTRACT-FREEZE-001`/负责人确认，本规格只给候选定义与 mutation）、`SO-01`/`SO-02`/`SO-04`（只继承不修改）。
- **控制器级**：`CTRL-F1`（工作树≠HEAD）、`CTRL-AR033`（根构建面 owner）、`CTRL-AR034`（7 项历史 CI 红）、
  `CTRL-AR035`（785 账本）、`CTRL-AR036`（宪章修订签字）——本任务只登记不裁决（ADJ-CTRL-01；C-004.4/C-004.6）。
- **F3-05**（FROZEN `SCI-P3-001` §1/§9a-10 陈旧文字）与 **F3-06**（F1 回退）不在本任务处置范围。
