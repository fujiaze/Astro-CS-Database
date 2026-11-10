# 10｜CCD Poisson/读出噪声模型

**状态：`PRIMARY_LOCATED_NOT_YET_READ`**（EMVA 1288 许可受限、Janesick 为书）；本卡中的公式为 `DERIVATION_TO_REVIEW`（ACSD 自推，需独立复核）
**核对日期：2026-11-10　核对人：lit-researcher**

> **诚实声明**：本轮**没有读到**任何一份 CCD 噪声标准的正文。EMVA 1288 需要许可申请；Janesick 的 *Photon Transfer* / *Scientific Charge-Coupled Devices* 是付费书且无合法免费全文。因此本卡**不写**任何标准的章节号、式号或页号；下文的方差公式是从 Poisson/高斯一阶原理**独立推导**的，不是抄自标准。

## 1. 规范书目与永久标识

| 项 | 内容 | 状态 |
|---|---|---|
| EMVA 1288 | EMVA Standard 1288, *Standard for Characterization of Image Sensors and Cameras*，标准页 <https://www.emva.org/standards-technology/emva-1288/>；下载需走许可申请页 <https://www.emva.org/standards-technology/emva-1288/emva-standard-1288-license-application/> | **仅定位**（`EMVA1288-3.1.pdf` 直链返回 404；正文不可公开获取） |
| Janesick | J. R. Janesick, *Photon Transfer: DN → λ*, SPIE Press（2007）；*Scientific Charge-Coupled Devices*, SPIE Press（2001） | **仅定位**（书，无合法免费全文） |
| Poisson | S.-D. Poisson (1837), *Recherches sur la probabilité des jugements…* | **仅定位**（本卡不需要它来“证明”二项→Poisson 极限；该极限是标准结论） |
| 仪器手册（可选交叉核对） | HST ACS/WFC3 Instrument Handbook 的 SNR 节（STScI 公开 PDF） | **本轮未读** |

## 2. 已核对的内容

**无。** 本轮未读到上述任何一手标准的正文，故本节不列页/式。为避免“看起来有出处”，以下只列**本轮实际核对过的对象**：

| 对象 | 已核对的事实 |
|---|---|
| ACSD 源码 [src/observation/calibration_kernel.cpp](../../../src/observation/calibration_kernel.cpp) | `poisson_plugin` 分支：`signal = max(d − b, 0)`；`variance_adu = signal / gain`；`shot = sqrt(variance_adu)`；`read = read_rms_e / gain`；注释自承「the shot variance is therefore approximate」；无 raw noise 且无 gain/read 时 `NoiseState::unknown` |
| ACSD 源码 [src/observation/calibrate.cpp](../../../src/observation/calibrate.cpp) | 校准入口 `calibrate_observation`；平坦场 Jacobian `1/flat` 用于把 raw 振幅映射到 calibrated 样本 |
| ACSD 标准 [NUMERICS.md](../../engineering/rebuild/standards/NUMERICS.md) | “未知误差不填零”的总原则；噪声状态需具名 |

## 3. 独立推导（`DERIVATION_TO_REVIEW`）

设一个像元的电子数为 `N_e`，增益 `g`（e⁻/ADU），读出噪声（电子）`σ_r`，偏置 `b`（ADU），暗电流暂不计。

1. **散粒**：`N_e ~ Poisson(λ)`，`Var(N_e) = E[N_e] = λ`。
2. **读出**：`N_r ~ N(0, σ_r²)`，与 `N_e` 独立。
3. **数字化**：`d = (N_e + N_r)/g + b`（ADU）。于是
   `E[d] = λ/g + b`，`Var(d) = (λ + σ_r²)/g²`（ADU²）。
4. **标准差（ADU）**：`σ_d = sqrt(λ + σ_r²)/g`。
5. **plug-in**：用观测值代替未知期望，`λ̂ = max(d − b, 0) · g`（ADU→e⁻），则
   `Var̂(d) = max(d − b, 0)/g`，`σ̂_shot = sqrt(max(d−b,0)/g)`，`σ̂_read = σ_r/g`。
   这与源码的 `variance_adu = signal/gain`、`shot = sqrt(...)`、`read = read_rms_e/gain` 一致。
6. **平坦场缩放**：若 calibrated 样本 `s = d/flat`，则 `Var(s) = Var(d)/flat²`；这正是源码用 `1/flat` 的线性作用（Jacobian）传播方差的原因。

**已知偏差（必须在文档中写明）**：
- `max(d−b, 0)` 是**有偏**的期望估计：`E[max(X,0)] ≠ λ`（当 `λ` 接近 0 时偏差最大）。因此 `Var̂` 不是无偏方差估计；它只是一个 plug-in。
- 暗电流、量化噪声、CTE、平场不确定度、偏置漂移**都不在**上式内；若未显式建模，必须标 `unknown` 而不是“已包含”。
- 上式假设像元间独立；若存在共享的读出模式/固定图案，`Γ` 非对角，逐像元方差不足。

## 4. 原文的假设与适用域（对标准的“预期”，未读，不得引用为已证）

- EMVA 1288 类标准的典型适用域：线性响应、无饱和、暗电流可忽略或已扣除、光子转移曲线线性段、单一增益。**这些是本卡的预期，不是已核对的条文**；在读到正文前，不得写“符合 EMVA 1288”。
- Janesick 的 photon transfer 方法同样假设线性、均匀照明、暗场可扣。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `observation.calibrate` | [src/observation/calibration_kernel.cpp](../../../src/observation/calibration_kernel.cpp)（`poisson_plugin`、`shot_poisson_plugin`、`read`） | §3 的方差模型 | 不能声称“符合 EMVA 1288”：未读标准 |
| `observation.calibrate` | [src/observation/calibrate.cpp](../../../src/observation/calibrate.cpp)、[calibration_kernel.cpp](../../../src/observation/calibration_kernel.cpp) 的 `1/flat` | 平坦场 Jacobian | 不能把平场误差当零 |
| `observation.calibrate` | `NoiseState::unknown` 分支 | 无 gain/read 时如实标 unknown | 不能默认 `σ_r=0` 或 `g=1` |
| `observation.regroup` | [src/observation/regroup.cpp](../../../src/observation/regroup.cpp) | 装箱改变读出次数 | 不能把软件 bin 与硬件 bin 的读出噪声混为一谈 |

## 6. 不能声称的结论（红线）

- ❌「ACSD 的噪声模型符合 EMVA 1288 / Janesick」：本轮未读其正文。
- ❌「`max(d−b,0)/g` 是无偏方差估计」：它是有偏 plug-in。
- ❌「方差模型已包含暗电流/量化/CTE/平场误差」：没有；必须显式列 `unknown`。
- ❌「像元独立」：共享读出模式/固定图案会破坏；ACSD 的 `Γ` 需能表达非对角项。
- ❌「gain 可以从数据里自动辨识」：本模型把 `g` 当输入；未知时只能标 unknown 或另立标定 process。

## 7. 与现有 ACSD 文档的差异

1. [docs/references/README.md](../README.md) 与 [02-LITERATURE-AND-PROVENANCE.md](../../workplans/v2-science-engineering/science/02-LITERATURE-AND-PROVENANCE.md) 都把 CCD 噪声列为“应分别确定真正使用的算法版本与有权威的一手原文/标准”——**尚未完成**，本卡明确标为 `PRIMARY_LOCATED_NOT_YET_READ`，不冒充已审。
2. 源码注释自承 “approximate”，与文档“未知误差不填零”的原则一致；但**文档层**尚未把“plug-in 有偏”写成显式限制。建议在 `observation.calibrate` 的算法来源节补上 §3 的推导与偏差说明。
3. `observation.regroup` 的 bin 语义（硬件 bin vs 软件 bin 的读出噪声不同）在源码/文档中已有意识；本卡把它列为噪声模型的一个**分支条件**，提醒不要用同一公式。

## 8. 待办

- [ ] 走 EMVA 许可申请取得 1288 正文，读到具体节/式后再升级为 `PRIMARY_READ_AND_MAPPED`；在此之前本卡不得被引作“符合标准”。
- [ ] 或改用可公开获取的一手替代（如某仪器手册的 SNR 节），并明确它是仪器特定而非通用标准。
- [ ] 独立复核者用 Monte Carlo（Poisson + 高斯）验证 §3 的方差公式与 plug-in 偏差量级，特别是 `λ → 0` 与 `λ` 很大两种极限。
