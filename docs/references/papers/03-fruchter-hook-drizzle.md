# 03｜Fruchter–Hook 2002：Drizzle、pixfrac 与 drop 分配

**状态：`PRIMARY_READ_AND_MAPPED`**（读的是 arXiv v2 全文，AAS LaTeX 稿；PASP 起页已核对，止页未核对）
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | 内容 |
|---|---|
| 题名 | Drizzle: A Method for the Linear Reconstruction of Undersampled Images |
| 作者 | A. S. Fruchter, R. N. Hook |
| 出处 | PASP **114, 144**（2002）；止页未在本次读取中核对（仓库既有索引写 144） |
| DOI | <https://doi.org/10.1086/338393>（解析 302，本次未打开出版版） |
| 永久链接（实际读取） | <https://arxiv.org/abs/astro-ph/9808087v2>（v2, 2001-10-19；AAS LaTeX 接受稿） |
| 许可 | arXiv 非独占分发；PASP 出版版版权 ASP |

> **版本声明**：节/式号按 arXiv v2 核对。PASP 出版版页号 144–152 属常见引用，但**本次未核对**，故不写入本卡作为已核事实。

## 2. 已核对的章节/式

| 位置 | 原文内容要点 |
|---|---|
| §1 式 (1) | 传统 shift-and-add 的最终图像 `I = T ⊗ O ⊗ E ⊗ P ⊗ G`：真实天光 `T`、光学 `O`、电子像元 `E`、物理像元 `P`、输出网格 `G`；原文指出 Drizzle 用 `p`（pixfrac 核）**替换** `P` 这一卷积 |
| §2（p.5） | 机制：把输入像元映射到细分输出网格，**先把像元“收缩”成 drop 再落到输出**；drop 的线性尺寸 = `pixfrac` × 输入像元线性尺寸（在几何畸变修正**之前**度量）；输出细分由 `scale s` = 输出像元/输入像元的线性尺寸比控制 |
| §2 式 (2)(3) | 迭代更新：`W' = a·w + W`；`I' = (d·a·w·s² + I·W)/W'`，其中 `0 < a < 1` 为 drop 与输出像元的**面积重叠比例**，`s²` 因子用于**保持面亮度** |
| §2 式 (4)(5) | 全部输入处理完后的闭式（爱因斯坦求和）：`W = Σ a·w`；`I = Σ d·a·w·s² / W` |
| §2 | 「interlacing 等价于 `pixfrac → 0`，shift-and-add 等价于 `pixfrac = 1.0`」；「the linear weighting scheme is statistically optimum **when inverse variance maps are used as the input weights**」 |
| §2 | 「if the drop size is sufficiently small not all output pixels have data added … choose a drop size small enough to avoid degrading, but large enough so that after all images are drizzled the coverage is reasonably uniform」——**覆盖 vs 分辨率的权衡是用户责任** |
| §5（Photometry） | `pixfrac = 1` 时对几何畸变的光度修正是**精确的**；模拟（scale=0.5, pixfrac=0.6）给出附加 RMS 0.004 mag；用 HDF-N 真实 dither + 宇宙线掩膜时 RMS ≲0.015 mag |
| §6（Astrometry） | 同样的模拟给出位置 RMS ≈0.018 输入像元；作者认为这来自质心能力的极限，而非 Drizzle 引入的新误差 |
| §7.1 | 相关噪声的定性机制：`(a² + b₁² + b₂² + b₃²)ε² < ε²`，交叉项被逐像素方差求和漏掉 |
| §7.2 式 (6)(7) | `p=0` 时 `σ_c² = Σ_{d∈C} w² s⁴ σ² / (Σ w)²`；`p>0` 时同式但集合换成 `P`（drop 有重叠者）并带 `a²` |
| §7.2 式 (8) | 噪声相关比 `R² = σ_c²/σ_p² = Σ_C w²σ² / Σ_P a²w²σ²` |
| §7.2 式 (9)(10) | 均匀填满 dither 的解析式：令 `r = p/s`，`r ≥ 1` 时 `R = r/(1 − 1/(3r))`；`r ≤ 1` 时 `R = 1/(1 − r/3)`；例：`p=0.6, s=0.5 → R = 1.662` |
| §3 | 宇宙线流程（pixfrac=1 先做、取中值、Blot 回投、导数、掩膜、再 drizzle）——**依赖多次 dither 与掩膜** |

## 3. 原文的假设与适用域

1. **线性**：`I` 与 `W` 都是输入的线性组合（式 2–5）；非线性（如中值、sigma-clip）不在原法内。
2. **几何已知**：输入像元 → 输出网格的映射（平移/旋转/畸变）视为**已给定且正确**；原法不估计 WCS。
3. **drop 是“收缩后的输入像元”**，其形状与输入像元同形（在畸变修正前的坐标系里定义），重叠 `a` 是**面积比**。原文的实现里 drop 与输出像元是直线多边形。
4. **权重**：只有 `w = 1/σ²` 时才是统计最优；`a` 的引入使输出噪声**相关**，逐像素方差会低估大尺度噪声（式 6–10 就是为此）。
5. **R 的解析式只在“每个输出像元收到等价输入”的均匀 dither 假设下成立**（§7.2 明说），非均匀 dither/边缘像元要逐案算。
6. **原文不提供**：真实曲边像元与球面叶的交叠、面积误差界、WCS 误差传播、源混淆下的 PSF 重建、`a` 的数值包含。

## 4. ACSD 采用/变体所需的独立推导要点

1. **估计量身份**：ACSD 只实现式 (4)/(5) 的**线性加权面积平均**，但把 `a` 从“直线 drop ∩ 直线输出像元”换成“**收缩后的曲边源像元 ∩ 球面 HEALPix 真叶**”。这一步**没有**原文依据，必须由 ACSD 自己给出：`a_p = Ω(D_p ∩ H_leaf)/Ω(D_p)`（`D_p` 为 drop 域，`H_leaf` 为真叶），并用区间/有理数给出 `a` 的包含区间。
2. **s² 与 Ω/A 的分工**：原文用 `s²` 保持面亮度（输入像元面积/输出像元面积比）。在球面上“面积”不是常数，ACSD 必须区分：`Ω`（未收缩像元的球面面积）、`A`（drop 的球面面积）、`a`（交叠比）。三者的关系需显式写出，不能把 `s²` 直接搬成常数。
3. **同 R 传播**：式 (8) 只给“相关噪声比”，不给协方差矩阵。ACSD 用“同一 R 同时作用于 signal/noise/support/response”的统一接口（见 [SPHERICAL_DRIZZLE.md](../../science/foundation/SPHERICAL_DRIZZLE.md)），这是 ACSD 的**接口设计**，其数学内容等价于把式 (8) 的标量 R 提升为算子层面的协方差传播；必须证明在 ACSD 的几何下 R 的定义仍然成立（尤其当 `a` 随位置变化时，式 8 的“等价输入”前提不成立）。
4. **pixfrac 的语义边界**：原文的 pixfrac 在**畸变修正前**的输入像元上定义。ACSD 若在球面上定义 drop（例如按球面测地边长收缩），必须证明与“先在切平面收缩再投影”的差异在预算内，或明确采用哪一种并给反例。
5. **R 的适用性反例**：非均匀 dither、边缘叶、`a` 非常数时式 (9)(10) 不适用；此时只能逐案数值算 `R² = Σw²σ²/Σa²w²σ²`（式 8 本身仍成立，因为它不依赖均匀性）。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `geometry.drizzle` | [src/geometry/drizzle.cpp](../../../src/geometry/drizzle.cpp)、[drizzle_process.cpp](../../../src/geometry/drizzle_process.cpp) | 式 (4)/(5) 的加权面积平均；区间化的 `IntervalValue`/`hull`/`multiply` 是 ACSD 自研 | 不能声称实现了 Fruchter–Hook 原算法：输入是曲边叶，不是直线 drop |
| `geometry.overlap` | [src/geometry/overlap.cpp](../../../src/geometry/overlap.cpp)、[exact_polygon.cpp](../../../src/geometry/exact_polygon.cpp)、[containment.cpp](../../../src/geometry/containment.cpp) | 提供 `a` 的包含区间 | 原文没有包含算法，也没有面积误差界 |
| `geometry.drizzle` | [src/geometry/kernel.cpp](../../../src/geometry/kernel.cpp)、[refine.cpp](../../../src/geometry/refine.cpp) | 核/细分策略 | 原文的“drop 大小 vs 覆盖均匀”只是经验建议，不是收敛定理 |
| `projection.export` | [src/projection/export_projection.cpp](../../../src/projection/export_projection.cpp) | 面积积分型导出，**不做第二次 pixfrac 收缩** | 不能把 Export 的归一行说成 Drizzle |

## 6. 不能声称的结论（红线）

- ❌「ACSD 的球面 Drizzle 是 Fruchter–Hook 算法」：相同的是线性面积分配思想；不同的是几何对象（曲边 ∩ 真叶）、面积定义与误差证书。
- ❌「Fruchter–Hook 证明了曲边交叠的正确性/误差界」：原文的 `a` 是直线多边形的面积比，没有误差分析。
- ❌「R = 1.662 适用于我们的数据」：那是 `p=0.6, s=0.5` 且均匀 dither 的例值。
- ❌「pixfrac 越小越好」：原文明确说太小会造成输出空洞（覆盖不足），要用户权衡。
- ❌「Drizzle 能超分辨」：原文 §1 明确它**不做** image restoration，只做线性重构。

## 7. 与现有 ACSD 文档的差异

1. [SPHERICAL_DRIZZLE.md](../../science/foundation/SPHERICAL_DRIZZLE.md) 参考条目写「Fruchter & Hook 2002，PASP 114,144，**§2与§7**」——§2/§7 的选择正确（式 2–5 与式 6–10 都在其中）；建议把**式号**补进引用，避免读者只能凭节号找。
2. 同文说「原论文不提供本文真实 HEALPix/SIP 相交器」——与原文一致，无夸大。
3. [STATISTICS_SKY_COADD.md](../../science/foundation/STATISTICS_SKY_COADD.md) 引「§7 重采样相关噪声；同一R传播是本规范统一接口」——§7 确为相关噪声；但「同一 R」是 ACSD 接口约定，**不是**原文结论，建议在该处加一句“R 的标量形式见原文式 (8)，算子化是本规范自研”。

## 8. 待办

- [ ] 核对 PASP 出版版页码与式号（本次只有 arXiv v2）。
- [ ] 独立复核者用式 (8) 在非均匀 dither 下构造反例，检验 ACSD 的“同 R”接口是否仍给出包含性误差。
