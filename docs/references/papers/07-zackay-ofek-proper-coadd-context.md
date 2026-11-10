# 07｜Zackay–Ofek proper coaddition：**仅作对照，不是 ACSD 已实现算法**

**状态：`PRIMARY_READ_AND_MAPPED`**（读 2015 预印本 v1；ApJ 出版版式号未核对）
**核对日期：2026-11-10　核对人：lit-researcher**

> **本卡的存在意义是划界**：ACSD 本期实现的是传统面亮度线性 coadd（BLUE/等权/QP），**不是** proper coaddition。凡 ACSD 文档出现 “proper coadd / 最优 coadd / Zackay–Ofek” 字样，必须同时写出本卡 §3 的假设，否则就是偷换。

## 1. 规范书目与永久标识

| 项 | 内容 |
|---|---|
| 题名 | How to coadd images? II. A coaddition image that is optimal for any purpose in the background dominated noise limit |
| 作者 | Barak Zackay, Eran O. Ofek |
| 出处 | ApJ **836, 188**（2017）；DOI <https://doi.org/10.3847/1538-4357/836/2/188>（解析 302） |
| 实际读取 | <https://arxiv.org/abs/1512.06879v1>（v1, 2015-12-21，页眉标 “SUBMITTED TO ApJ”） |
| 前作 | Paper I：*How to coadd images? I. Optimal source detection and photometry using ensembles of images*，<https://arxiv.org/abs/1512.06872v1>（**本轮只读摘要**） |
| 许可 | arXiv 非独占；ApJ 版权 AAS |

> **版本声明**：节/式号按 2015 预印本 v1 核对；ApJ 836,188 的出版版式号**未核对**。引用出版版时必须重新核。

## 2. 已核对的章节/式

| 位置 | 原文内容要点 |
|---|---|
| §2 式 (1) | 图像模型：`M_j = (F_j T) ⊗ P_j + ε_j`；`T` 为真实无噪图像，`P_j` 为 PSF，`F_j` 为透明度因子（含曝光时间、有效口径、探测效率），**所有图像假定已配准** |
| §2 式 (2) | 噪声方差：`V[ε_j] = B_j + (F_j T) ⊗ P_j`（背景项 + 源项） |
| §3 | “proper image” 定义：**(I)** 加性噪声所有像元**等方差**；**(II)** 加性噪声像元**不相关**。满足 I+II + 高斯 ⇒ i.i.d.。原文举反例：matched-filtered 图、deconvolved 图、平坦场化后的图（方差随位置变）、含强源/饱和的图 **都不是 proper** |
| §3 | 「any **linear combination** of images that are directly measured from the telescope, is (at least locally) a proper image」 |
| §4 式 (7) | proper coadd 的傅里叶域权重：`R̂ = [Σ_j (F_j/σ_j²) P̂_j* M̂_j] / sqrt(Σ_j (F_j²/σ_j²) |P̂_j|²)`；原文说明每个频率被按其标准差归一，使噪声在频域等方差 |
| §4 | `R` 是 proper image，且是**背景主导极限下**的最优 proper image；每个频率是 `T̂` 的最大 S/N 线性估计 |
| 摘要 | 声称：输出像元不相关、保留所有空间频率信息、PSF 不宽于最好的一幅、任何在单图上做的检验都可在该 coadd 上做 |
| Paper I（仅摘要） | 点源检测/测光：先各自 matched filter 再按权重求和；PSF 同质化后再 coadd 会损失灵敏度；声称相对 weighted coadd 提升几个 % 到 25% 巡天速度 |

## 3. 原文的假设与适用域（引用时必须同时写出）

1. **背景主导**：`B_j` 主导 `(F_j T)⊗P_j`；源噪声可忽略。亮源/强发射区不满足。
2. **噪声近似高斯**（原文摘要明说 Paper I 用高斯近似）。
3. **图像已配准**：`M_j` 是同一位置的配准图；**没有**重采样、WCS 投影、drizzle 的几何重分配。
4. **加性噪声等方差且不相关**（proper 条件 I/II）——这正是平坦场化/重采样会破坏的性质。
5. **线性操作**；非线性（中值、sigma-clip、CR 掩膜）不在框架内。
6. 最优性是**对“常数天图 T 的假设检验/测量”**而言；对**面亮度**（扩展发射）的目标函数不同，最优权也不同。
7. 原文**不提供**：非背景主导情形的界、重采样引入的相关噪声处理、与 WCS/几何误差的复合、对真实巡天端到端的验证（Paper I 有 PTF 数据演示，但那是点源检测场景）。

## 4. 与 ACSD 的关系（必须写清的三句话）

1. **ACSD 未实现 proper coadd**。ACSD 的 `coadd.weights` 求的是“同一面亮度量的最小方差线性无偏权”（BLUE）或显式非负 QP；`coadd.integrate` 是加权积分。二者的目标函数与统计模型不同。
2. **Zackay–Ofek 的结论不能用来评价 ACSD 的 BLUE**：proper coadd 优化的是“对任意假设检验保留信息”，BLUE 优化的是“给定线性模型下方差最小”。在背景主导 + 点源检测场景，proper coadd 更优；在面亮度 + 已知 `Γ` 场景，BLUE 是 BLUE。**两者不是同一最优性概念**。
3. **ACSD 若未来要做 SACoadd/proper 类求解器**，必须重新推导：把式 (7) 的频域权重搬到球面重采样后的相关噪声上，需要 `Γ` 的完整协方差（不是逐像素方差），并证明 proper 条件 I/II 在重采样后仍成立（一般**不成立**）。这属于未来工作，本轮不做（见总任务 README §4 管理禁令 5）。

## 5. 对应 Process 与当前源码路径

| 位置 | 说明 |
|---|---|
| 无实现 | `src/` 下**没有** proper coadd / SACoadd 实现；`coadd.weights` 的 `solve_blue`/`solve_nonnegative_qp`（[src/coadd/weights.cpp](../../../src/coadd/weights.cpp)）是 BLUE/QP，不是式 (7) |
| 对照用途 | 写 `coadd.*` 文档时，若要讨论“最优 coadd”，只能作为**对照文献**引用，并注明“非本期实现” |

## 6. 不能声称的结论（红线）

- ❌「ACSD 实现了 Zackay–Ofek proper coadd」——没有实现。
- ❌「ACSD 的 BLUE 是 proper coadd 的特例」——不是；目标函数不同。
- ❌「proper coadd 对任何目的都最优」——原文限定背景主导 + proper 噪声 + 已配准。
- ❌「proper coadd 给出无偏面亮度」——原文的最优性是检验/测量的信息保留，不是面亮度无偏性。
- ❌「把式 (7) 直接用到 drizzle 后的图上」——重采样破坏 proper 条件 I/II。

## 7. 与现有 ACSD 文档的差异

1. [02-LITERATURE-AND-PROVENANCE.md](../../workplans/v2-science-engineering/science/02-LITERATURE-AND-PROVENANCE.md) 的 SRC-COADD-CONTEXT 行写「点源/背景主导 optimal coadd 的**对照文献**；不可偷换为 ACSD 传统面亮度 BLUE 的原论文或已实现 SACoadd」——**准确**，本卡提供原文依据（式 1/2/7 + proper 条件）。
2. [docs/references/README.md](../README.md) 的 Proper coadd 对照行写「**对照资料**，仅在其背景主导、稳定源等假设内讨论最优性；不是本期已实现算法」——**准确**。
3. 若 `coadd.select`/`coadd.weights` 的正文出现“最优”字样而未限定，应改为“在给定线性模型与已知 `Γ` 下的 BLUE”，避免与 proper coadd 混淆。

## 8. 待办

- [ ] 核对 ApJ 836,188 出版版式号（本卡按 2015 预印本）。
- [ ] 若要引用 Paper I 的具体结论（matched filter 顺序、25% 提升），需读其正文，不能只引摘要。
