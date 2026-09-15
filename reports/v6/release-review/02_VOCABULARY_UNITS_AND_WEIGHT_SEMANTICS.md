# V6 口径收敛（二）：权重词表 / 单位表 / 权重与方差语义

- 语义权威（唯一）：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json`（96 条款）
- 单一词表事实源：`contracts/data/v6_weight_vocabulary_v1.json`
- 生产 schema：`contracts/schemas/v6/astrocs.v6.*.v1.schema.json`（10 件）
- 消费面：`docs/contracts/DATA_SEMANTICS.md` §31（`DATA-V6-SCHEMA`）、§「V6 消费面」（`API-V6-WEIGHTMODE-001`）
- 机器检查：`reports/v6/release-review/tools/check_doc_convergence.py`（C1/C2/C3/C8/C9）

## 1. 三生产模式 / 基线 / 延迟（`FZ-MODE-*`）

| 面 | 合法值 | 语义 |
|---|---|---|
| 生产科学模式 | `point_information` / `surface_gls` / `psfsw_robust` | 配置显式选择，不得自动切换 |
| 文档基线模式 | `equal` / `pixel_ivar` | 仅基线比较，**非**科学最优声明 |
| 延迟模式 | `psf_snr_power` | **DEFERRED / NOT_IMPLEMENTED，生产拒绝**（`C-004.1`，本包不解冻） |

- 生产枚举禁止值：`psf_snr_power` / `auto` / `support_x_snr2` / legacy `0` / 未知值 → **REJECT**（`FZ-MODE-DEFERRED` / `FZ-FIELD-WEIGHTMODE`）。
- legacy 整数：`0=support×snr² → REJECT`；`1 → equal`；`2 → pixel_ivar`（后两者仅文档基线）。

## 2. 冻结单位表（`FZ-UNIT-*` / `FZ-P3-BUNIT-QUADRATIC`）

| 符号 | 单位 | 含义 | 方差单位 | ivar 单位 |
|---|---|---|---|---|
| `signal_sb` | `ADU/px²` | Phase1 Drizzle/HiPS 面亮度 signal | `ADU²/px⁴` | `px⁴/ADU²` |
| `pixel_variance_in` | `ADU²` | 输入源像素逐像素方差 v_j | — | — |
| `sb_variance_out` | `ADU²/px⁴` | Phase1 输出面亮度方差 | — | — |
| `sb_ivar_out` | `px⁴/ADU²` | Phase1 输出 ivar | — | — |
| `W_info` | `ADU⁻²` | 点源信息权重 = 1/Var(F̂) | — | — |
| `Q` | `ADU⁻¹` | 点源线性充分统计量 | — | — |
| `flux`（F̂） | `ADU` | 点源通量估计 | `ADU²` | `ADU⁻²` |
| `psfsw_robust_weight` | `1` | 无量纲组内相对复合权重 | — | — |

- 二次律：`variance = signal²`、`ivar = 1/variance`；Phase3 输出 variance BUNIT =（主 HDU signal BUNIT）²。
- BUNIT 量纲可判（`FZ-BUNIT-SEMANTICS`）：显式含 px 幂次，或 `BUNIT=ADU` 时 provenance 声明 `pixel_semantics=surface_brightness` + `pixel_area_power=-2` + 目标像素面积；否则单位不可判 → unavailable/REJECT。
- 归属状态：`ADU/px²` 系为 FROZEN；`pixel_variance_in`/`sb_variance_out`/`sb_ivar_out`/`FZ-BUNIT-SEMANTICS` 为 PENDING_OWNER_SIGNOFF（`SO-01`），生效前 fail-closed。

## 3. 权重/方差语义（禁止冒充）

- **Q/W 与无量纲前提**：点源模型下 `Q_k = a_k P_kᵀ C_k⁻¹ d_k`（`ADU⁻¹`）、`W_info,k = a_k² P_kᵀ C_k⁻¹ P_k`（`ADU⁻²`）、`F̂ = ΣQ/ΣW`、`Var(F̂) = 1/ΣW`。
  白噪声近似 `W_info,k = a_k²/(σ_pix,k²·A_NEA,k)`、`A_NEA = 1/ΣP²`（单位 `px²`）**仅在 C 对角且 σ_pix 声明时**可用（`FZ-COND-WHITENOISE`）。
- **psfsw_robust**：`Wt_k = C_norm·S^α·Conc^β/(N^γ·B^δ)`、`W_psfsw,k = Wt_k/median_j(Wt_j)`。
  **无量纲、组内 median=1、是 conventional integration 相对权重，不是 ivar / 不是 Fisher information / 不是 W_info**（`FZ-FIELD-PSFSW-UNIT`；`RULINGS.md #3/#5）。
- **final covariance**：只能由**实际组合系数**经 `C_out = R C_in Rᵀ` 传播，并**必须输出 effective PSF**；
  `variance_from_weight` / `uses_relative_weight_as_ivar` 必须为 `false`，禁止 `1/W_psfsw` 反推（`FZ-FORMULA-COV-PROP` / `FZ-GATE-PSFSW-COV` / `FZ-GATE-PSFSW-EPSF`）。
  只给 FWHM 标量不构成 effective PSF。
- **诊断量不得冒充权重或方差来源**：帧级 `median(SNR_F)`/`median_source_snr`、support、coverage、FWHM、residual
  进入 `weight.sources` / `weight_value` / `covariance.variance_from` → **REJECT**（`FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE`；`C-004.2`）。
  帧级 `median(SNR_F)` 只作诊断/深度表达。

## 4. 单一权威权重词表（canonical）

| 语义 | SCI-PSFW 旧词表 | SCI-P2 旧词表 | **canonical（唯一权威）** | psfsw canonical 值 |
|---|---|---|---|---|
| 权重对象种类 | `weight_kind` | `weight.kind` | `weight.kind` | `psfsw_robust_weight`（legacy 别名 `relative_dimensionless` 仅 reader） |
| 权重单位串 | `weight_units` | `weight.units` | `weight.units` | `"1"`（legacy 别名 `dimensionless_relative` 仅 reader；`DI-07`） |
| 归一域 | `normalization.scope` | `group_normalized` | `weight.group_normalized` | `true`（同步 `normalization.scope="group"`） |
| 组内 median 目标 | `normalization.median_target` | （隐含） | `weight.normalization.median_target` | `1.0` |
| 权重标量 | — | — | `weight.weight_value` | valid=false 时必须 `null`（fail-closed） |

- **第三套词表禁止**：`weight_normalized`/`normalization_scope`/`weight_type`/`weight_kind_name`/`norm_scope`/`unit_string`/`is_group_normalized`/`weight_dimensionless` 出现即 REJECT。
  规范化只发生在 reader/迁移层；生产 writer/schema 只产出 canonical（`C-004.3`；`DI-01`/`DI-07` 已由 W6 闭合）。
- 相关登记：`01_CONVERGENCE_CORRECTIONS.md` #7（`05_point_information_and_weight_mode.md` §4 追加 W6 归一完成标注）。第三套词表现存出现均在「禁止/第三套/legacy/别名/REJECT/迁移」语境。

## 5. provenance 与 fail-closed

- 最小集（`FZ-PROV-MINIMAL-SET`）：schema/软件 SHA/run ID/输入+配置哈希/单位+`pixel_area_power`/frame/像素语义/算法 ID/provider/近似+降级（含 unavailable 原因）/归一版本/相关核或低秩摘要/`flux_conservation_factor`/`k_corr` 值+适用域/生成时间/输出哈希。缺键/单位不可判/unavailable 无原因 → REJECT。
- `k_corr=1` 忽略相关、跨域外推/内插 → REJECT（`FZ-PROV-KCORR`；`FZ-PROV-KCORR-VALUE` = 1.4 为 PENDING/`SO-07`）。

## 6. 与冻结口径的一致性判定（机器）

- 检查器 C1（canonical 词表 / 第三套词表）、C2（psfsw 非 ivar/Fisher/方差来源）、C3（DEFERRED 且生产拒绝）、
  C4（concentration 单位）、C8（R C_in Rᵀ + effective PSF）、C9（模式元组）全部 PASS；
  负向注入 9/9 判红（见 `05_VERIFICATION_AND_NEGATIVE_CHECKS.md`）。
- 结论：**文档面与冻结 V6 口径一致**；未解冻任何条款、未改任何冻结值。
