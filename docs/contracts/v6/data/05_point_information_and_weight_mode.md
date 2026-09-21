> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# 05 — W_info（point_information）与 weight_mode 对象 schema（已按 §9.73 A44 作废：该概念不存在）

> 上游：ASTROCS_DESIGN.md §3.1（数据对象）、§8.4（模块与 ABI）

上位锚：`FZ-FORMULA-WINFO/Q/FHAT`、`FZ-COND-WHITENOISE`、`FZ-GATE-MEDIAN-SNR`、`FZ-GATE-SUPPORT-COVERAGE`、`FZ-DEGRADE-SCALAR`；
`FZ-MODE-PRODUCTION`/`-BASELINE`/`-DEFERRED`、`FZ-FIELD-WEIGHTMODE`；UNIFIED §4/§4.1/§11；PSF_SIGNAL_WEIGHT §2/§4；ADJ-P2-01/ADJ-S1/ADJ-C004-01/02/03；C-004.1/2/3。
机器：`astrocs.v6.point-information.v1.schema.json`、`astrocs.v6.weight-mode.v1.schema.json`；正例 `examples/point-information.example.json`。

## 1. W_info 权威式（唯一，不得改写）

```text
FZ-FORMULA-Q:      Q_k = a_k P_k^T C_k^-1 d_k
FZ-FORMULA-WINFO:  W_info,k = a_k^2 P_k^T C_k^-1 P_k = 1/Var(F_hat_k)
FZ-FORMULA-FHAT:   F_hat = Sum_k Q_k / Sum_k W_info,k;  Var(F_hat) = 1/Sum_k W_info,k
独立帧: Q=Sum Q_k, W=Sum W_info,k; 相关帧必须用联合 C，禁止简单求和
FZ-COND-WHITENOISE（条件式，仅 C 对角且 sigma_pix 声明时）:
                   W_info,k = a_k^2/(sigma_pix,k^2 * A_NEA,k),  A_NEA,k = 1/Sum_p P_k,p^2
```

| 字段 | 类型 | 单位 | 适用域 | fail-closed | 条款锚 |
|---|---|---|---|---|---|
| `authoritative_formula` | const | — | point_information | 被替换/改写 → REJECT | `FZ-FORMULA-WINFO` |
| `W_info` | {value,units=`ADU^-2`} | `ADU^-2` | 全部 | 单位 ≠ ADU^-2 → REJECT | `FZ-UNIT-WINFO` |
| `Q` | {value,units=`ADU^-1`} | `ADU^-1` | 全部 | 单位 ≠ ADU^-1 → REJECT | `FZ-UNIT-Q` |
| `flux` | {value,units=`ADU`} | `ADU` | 全部 | 单位 ≠ ADU → REJECT | `FZ-UNIT-FLUX` |
| `flux_variance` | {value,units=`ADU^2`} | `ADU^2` | 全部 | 必须 = 1/W_info | `FZ-FORMULA-FHAT` |
| `independent_frame_combination` | object | — | 全部 | 相关帧却用 Sum_k 且无 joint_covariance_ref → REJECT | `FZ-FORMULA-WINFO`；UNIFIED §4 |
| `white_noise_approximation` | object | — | 条件式 | applied=true 而任一 condition=false → REJECT | `FZ-COND-WHITENOISE` |
| `optimality.claims` + `preconditions_met` | object | — | 最优性声明 | claims 非空而任一 precondition=false → REJECT | ADJ-P2-01 |
| `representation` + `degradation` | enum + object | — | 空间 W_psf(x,y) | scalar 未过均匀性/功率损失门 → REJECT | `FZ-DEGRADE-SCALAR` |
| `frame_inputs[]` | {frame_id,a_ref,P_ref,C_ref} | — | point_information | 缺 P_k/a_k/C_k → unavailable | ADJ-P2-01；PROJECT_SPEC §3/§4 |

**最优性前提（全部满足才可声明 BLUE / 最大点源 SNR / 最小通量方差）**：模型正确、P 归一 ΣP=1、C 正确且可表示、高斯或 CRLB 意义、目标为点源。
仅有 Drizzle 后逐像素 ivar **无法重建** W_info（会丢 PSF/协方差）；相关帧简单求和必须被拒（Oracle C3 过度乐观比 1.575）。

## 2. weight_mode 语义（三面互斥，ADJ-S1）（已按 §9.73 A44 作废：该概念不存在）

| 面 | 取值 | 说明 |
|---|---|---|
| 生产科学模式 | `point_information` / `surface_gls` / `psfsw_robust` | `FZ-MODE-PRODUCTION`；配置显式选择，不得自动切换 |
| 文档基线模式 | `equal` / `pixel_ivar` | `FZ-MODE-BASELINE`；仅基线比较，**非**科学最优声明 |
| 延迟模式 | `psf_snr_power` | `FZ-MODE-DEFERRED`；DEFERRED/NOT_IMPLEMENTED，**不进** V6 生产路由（C-004.1） |

legacy 整数 `{0=support×snr², 1=equal, 2=ivar}` 一律被取代，且 **0 不得进入任何科学权重面**（`FZ-FIELD-WEIGHTMODE`）。
生产枚举出现 `psf_snr_power` / `auto` / `support_x_snr2` / `0` → REJECT（ADJ-C004-01；ADJ-AR-01）。

## 3. 权重来源门（诊断量不得冒充权重）

禁止 token（ADJ-C004-02；`FZ-GATE-MEDIAN-SNR`/`FZ-GATE-SUPPORT-COVERAGE`）：
`median_source_snr`、`median_snr`、`source_snr_median`、`med_source_snr`、`support`、`support_area`、`coverage`、`coverage_area`、`fwhm`、`psf_fwhm`、`median_fwhm`、`source_fwhm`、`residual`、`psf_residual`、`psf_fit_residual`、`fit_residual`、`psfsw_robust_weight`、`psfsw`。

任一 token 出现在 `weight.sources` / `weight_value` / `covariance.variance_from` → REJECT。
支持该门的结构性反例：两归一化 PSF 在五个诊断量上相对差 <5%，但 W_info 相差 12.36%（Oracle C8.5/C8.6，预测误差下界 ≥5.13e-4）。

## 4. schema 词表（两套既有词表，不发明第三套）

C-004.3 / ADJ-C004-03：schema 词表由 SCHEMA-INTEGRATE-001(W6) 归一。本提案保留两套既有词表并显式互映：

| 语义 | SCI-PSFW 词表 | SCI-P2 词表 |
|---|---|---|
| 权重对象种类 | `weight_kind` | `weight.kind` |
| 权重单位串 | `weight_units` | `weight.units` |
| 归一域（组内/全局） | `normalization.scope` | `group_normalized` |
| 组内 median 目标 | `normalization.median_target` | `group_normalized=true (median=1)` |

本提案层曾包含 `kind_alias_sci_psfw` / `units_alias_sci_psfw` / `normalization_scope_alias` 三个可选别名字段，
**目的仅是让两套门在 W6 归一前互相可认**（**2026-09-20 订正**：「W6 归一前」状态已结束——归一已完成，见下方 :69 的 DOC-CONVERGE-001/W12 标注；依据 `W6_SCHEMA_INTEGRATION.md:29,38,104`），不是建立第三套名。

> **W6 归一已完成（DOC-CONVERGE-001/W12 收敛标注，2026-09-15）**：单一权威词表 = `contracts/data/v6_weight_vocabulary_v1.json`，canonical 字段 = `weight.kind` / `weight.units` / `weight.group_normalized` / `weight.normalization.{scope,median_target,constants_version}` / `weight.weight_value`；生产 schema（`contracts/schemas/v6/astrocs.v6.weight-mode.v1.schema.json`）**不含**上述别名字段，别名只保留在迁移层 reader 规则（`legacy_aliases`）。第三套名（`weight_normalized`/`normalization_scope`/`weight_type`/…）出现即 REJECT（`FZ-FIELD-WEIGHTMODE`，`C-004.3`）。见 `docs/contracts/v6/W6_SCHEMA_INTEGRATION.md` §2 与 `docs/contracts/DATA_SEMANTICS.md` §31.4。

## 5. 参数生效证明（AR-048）

`weight-mode.v1.parameter_effectiveness` 要求：声明的权重参数必须
`applied_in_combination_coefficients=true` 且 `applied_in_effective_psf=true`。
仅把 `weight_mode` 写进配置/日志而不影响实际组合系数与 effective PSF → REJECT（`G-PARAMETER-EFFECTIVENESS`）。（已按 §9.73 A44 作废：该概念不存在）
这是对 review-audit AR-048「R9 装饰性合规」在本设计面的直接闭合要求。

## 6. fail-closed 与负向门

- 未知模式 / legacy 0 / `psf_snr_power` 进生产枚举 → REJECT；
- baseline 模式声明科学最优 → REJECT；
- `weight.sources` 命中诊断别名集 → REJECT；
- 每模式必需原始量未同时出现在 `sources` 与 `primitives` → REJECT（门 R4）；
- 相关帧用 `Sum_k` 且无联合 C 引用 → REJECT；
- W_info 单位错误 → REJECT。
