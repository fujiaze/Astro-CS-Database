> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 03 — covariance / variance / correlation 对象 schema

上位锚：`FZ-FORMULA-COV-PROP`、`FZ-PROV-SHARED-SYSTEMATIC`、`FZ-GATE-PARENT-VAR`、`FZ-GATE-PIXIVAR-APPROX`；
UNIFIED §6/§7/§8；DESIGN-P3 §4；ADJ-AR-02/ADJ-OBS-01/ADJ-P2-02；`RULINGS.md` #5；PROJECT_SPEC §3。
机器：`contracts/proposals/v6/data/astrocs.v6.covariance.v1.schema.json`（`covariance.v1`）。

## 1. 统一不变量（三模式共用）

```text
FZ-FORMULA-COV-PROP:  C_out = R C_in R^T            （线性算子/矩阵形式）
                      Var(out) = c^T C_in c         （单输出标量形式）
```

`R`（或向量 `c`）= **实际**组合算子：含 UPM 光度尺度、重采样/插值核、rejection/validity 掩码、归一化与逐帧权重。
禁止从任何权重标量（psfsw_robust_weight、pixel ivar、median SNR、support、coverage、FWHM、residual）反推 variance。

## 2. 字段规格

| 字段 | 类型 | 单位 | 适用域 | fail-closed | 条款锚 |
|---|---|---|---|---|---|
| `propagation` | const `C_out = R C_in R^T` | — | Phase1/2/3 | 空/缺失 → REJECT | `FZ-FORMULA-COV-PROP` |
| `combination_coefficients` | number[] minItems 1 | 无量纲组合系数 | 全部 | 空且无 operator_descriptor → REJECT | `FZ-FORMULA-COV-PROP` |
| `operator_descriptor` | object（reconstructable=true + summary_ref） | — | 空间算子 | 不可重建/缺摘要 → REJECT | UNIFIED §7 |
| `variance_from` | enum `{combination_coefficients, linear_combination_coefficients, actual_combination_coefficients}` | — | 全部 | 命中禁止来源 → REJECT | `FZ-GATE-PSFSW-COV`；门 R6 |
| `input_covariance` | enum provided/declared/unavailable | — | 全部 | psfsw_robust 必须非 unavailable | `FZ-GATE-PSFSW-COV` |
| `representation` | enum diagonal / +correlation_kernel / low_rank / common_master / unavailable | — | 全部 | 只对角而无核/摘要 → REJECT | UNIFIED §6/§7；ADJ-AR-02 |
| `correlation_kernel.rho_summary` | {mean_abs_rho,max_abs_rho} | 无量纲 | 仅对角输出时强制 | 缺失 → REJECT | PHASE3_REVIEW C-P3-PROP-9 |
| `low_rank` / `common_master` | object | — | 共享系统项 | 未进传播链 → REJECT | `FZ-PROV-SHARED-SYSTEMATIC` |
| `diagonal_approximation` | object | — | HiPS 父级/对角归约 | 缺三要素(N4) → variance 面 unavailable | `FZ-GATE-PARENT-VAR` |
| `approximation.error_gate` | {metric=Var_approx/Var_GLS,value,bound_ref} | — | surface_gls 近似 | value > 1+epsilon → REJECT（epsilon 由 ALG-P2-SURF-001 冻结） | `FZ-GATE-PIXIVAR-APPROX` |
| `psfsw_boundary` | const method/variance_from_weight=false/uses_relative_weight_as_ivar=false | — | psfsw_robust | 任一不符 → REJECT | `FZ-GATE-PSFSW-COV`；RULINGS #5 |
| `upm_contribution` | {J_ref,C_theta_ref,variance_ratio} | — | Phase2 UPM | UPM 不确定度未进链 → REJECT | DESIGN-P2 §4 |
| `avail`/`unavailable_reason` | enum | — | 全部 | unavailable 无原因/不在白名单 → REJECT | ADJ-GEN-03；宪章 §18.3 |

## 3. 三类量的冻结划分（ADJ-OBS-01）

| 类别 | 物理来源 | 合同表示 | 禁止 |
|---|---|---|---|
| 独立随机项 | 天空/读出/暗电流/量化逐像素噪声 | 对角 variance/ivar | 伪装成共享项 |
| 共享系统项 | 共同 master、共同天空/背景、重采样相关 | 低秩 `L L^T` / 相关核 `sigma+kernel` / 共同 master ID+`alpha_m`（三种允许形式之一） | 按独立随机项处理（实测低估 3.48×） |
| 模型偏差 | 光度零点、PSF 模型误差、WCS、UPM 参数 | validity/quality + 系统误差预算 + 参数 covariance | 伪装随机 ivar |

## 3b. surface_gls 权威式（\`FZ-FORMULA-GLS\`）

\`\`\`text
x_hat = (A^T C^-1 A)^-1 A^T C^-1 d;  Cov(x_hat) = (A^T C^-1 A)^-1
\`\`\`

## 4. 恒等式与 Oracle（S2 已证）

```text
point_information: c = C_in^-1 A /(A^T C_in^-1 A);  Var(F_hat) = c^T C_in c = 1/W   （C 正确时恒等）
surface_gls:       R = (A^T C^-1 A)^-1 A^T C^-1;     C_out = R C_in R^T = (A^T C^-1 A)^-1
psfsw_robust:      alpha_k(p) = W_psfsw,k v_k(p) / Sum_j W_psfsw,j v_j(p)
                   Var(I_out(p)) = Sum_{k,l} alpha_k alpha_l [C_in]_{kl}
```

这些恒等式由 SCI-P2-001 独立 Oracle C1.3/C5/C7.4–7.5 数值验证（误差 <1e-9 / <1e-16 / rel<3%）。
本设计只把它们变成可校验字段，不重推。

## 5. fail-closed 与负向门

- `variance_from` 写成 `psfsw_robust_weight`/`median_source_snr`/`support`/`coverage`/`fwhm`/`psf_residual` → REJECT（`G-COV-VARIANCE-FROM`）；
- `Var = 1/W_psfsw` 或 `Var = Sum_k W_psfsw,k` → REJECT（`G-PSFSW-COV`）；
- 只出对角 variance 而无相关核/可重建算子摘要 → REJECT（`G-COV-CORRELATION-KERNEL`）；
- 把 HiPS 父级对角归约声明为**精确** → REJECT（`G-PARENT-VAR-DEFICIT`）；
- 共享系统项存在却按独立项处理（联合 vs 朴素方差比 > 1 须检出）→ REJECT（`G-SHARED-SYSTEMATIC`）；
- surface_gls 像素 ivar 近似无 `R~ C_in R~^T` 报告或无误差门 → REJECT（`G-PIXIVAR-APPROX`）。

## 6. 迁移建议

- `DRIZZLE.md` §1/§9a「协方差产品为非目标」被取代：协方差/相关核为**强制输出面**（SO-04，只登记）。
- 现有产品只写对角 variance：读取侧必须检出缺失的 correlation kernel/低秩项，不得静默当独立像素（FD-1/FD-3）。
- 数值阈值（`deficit` 阈值、`epsilon`、低秩秩上限）不由本任务定；见 `10_migration_and_open_items.md` DI-02。
