# 06｜Montage 算法页：重投影、overlap 背景与 area 加权 coadd

**状态：`PRIMARY_READ_AND_MAPPED`**（读的是 IRSA 官方算法页全文）；Montage 工具论文（arXiv 1005.4454）**未读**
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | 内容 |
|---|---|
| 一手来源 | Montage **Algorithms** 页：<https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html>（IRSA/Caltech，Montage 官方文档） |
| 工具论文 | Jacob et al., “Montage: a grid portal and software toolkit for science-grade astronomical image mosaicking”, arXiv:1005.4454（**本轮未读**） |
| 项目主页 | <https://montage.ipac.caltech.edu/> |
| 性质 | **软件算法说明页**，不是同行评议论文；它是一手软件文档（作者方），可作为“Montage 实际怎么做”的原始依据，但**不能**当作算法正确性/最优性的证明 |
| 许可 | Montage 工具本身为开源许可（具体条款本轮未核对）；页面文本版权 Caltech/IPAC，本卡只做摘要引用 |

## 2. 已核对的章节（按页面小节）

| 小节 | 原文内容要点 |
|---|---|
| *Image Reprojections* | 重投影 = 把输入像元能量按**精确重叠**重新分配到输出像元；「project **both input and output pixels onto the celestial sphere**」，于是问题化为「calculating the area of overlap of two convex polygons on a sphere」；用球面三角 + **Girard 定理**算球面多边形面积 |
| 同节 | 明确假设：「the polygon defining a single pixel can be approximated by the set of **great circle segments** connecting the pixel's corners」，理由是现实像元 ≲1°，非线性可忽略，影响仅约 <0.01 像元；「Total energy is still conserved」 |
| *A Fast Reprojection Algorithm* | plane-to-plane：两个 TAN 平面之间的解析变换（来自 Spitzer 库），比球面投影快 30 倍以上；只适用于两个投影都是切平面类；可用“扭曲 TAN + 多项式改正”近似小区域的其他投影 |
| **Background Modeling and Rectification** | 假设输入已定标到绝对能量标度、差异只来自**低频**背景（「the ‘non-sky’ background has very little energy in any but the lowest spatial frequencies」）；对每幅图与其邻居的**重叠区**做最小二乘拟合（梯度+偏置）；「we only adjust the image by **half** this amount … to avoid ringing」；**迭代固定次数**（原文：“currently for a fixed number of times though we may later introduce convergence criteria”）；为省内存/时间，用**平面拟合**近似重叠区差分的最小二乘 |
| *Coadditions and Weighting of Output Pixel Fluxes* | 每个输入像元的能量按**天空面积重叠**加权累加，同时累加一个“area 覆盖”图；输出 = 各图的**面积加权平均**；area 图通常很平；mean 为默认，2.x 支持 mean/median（可带/不带 area 权重）；median 会丢弃覆盖率低于阈值的像元；无 area 文件时等权 |
| 同节 | 用逐行读取实现大图 coadd（内存只受单行限制），提到 6 GB 图像 |
| *The “Drizzle” Algorithm* | Montage 把 drizzle 实现为“换一组更小的角点再投影”（例：`(3.5,11.5)–(4.5,12.5)` 用 0.8 收缩 → `(3.6,11.6)–(4.4,12.4)`），面积图自动承担权重 |

## 3. 原文的假设与适用域

1. **像元边界可用大圆近似**（≲1° 像元）。对 ACSD 的宽视场/大像元或强畸变情形，这个近似不再是 <0.01 像元的量级，必须自证。
2. **背景是低频的**；若背景有与“sky”同尺度的结构，Montage 明说“no generalized background matching algorithm will be able to distinguish”。
3. **每幅图相对邻居调整一半 + 固定迭代次数**——这是**启发式**，没有收敛证明，也没有全局唯一性（gauge）讨论。
4. **coadd 权重 = 面积覆盖**，不是逆方差；原文没有误差传播、没有相关噪声、没有选择效应。
5. **平面拟合替代最小二乘**是为速度的近似，原文自己说明。
6. 原文**不提供**：全局 gauge 的可辨识性、共享噪声协方差、重叠区选择偏差、面积加权的统计最优性。

## 4. ACSD 采用/变体所需的独立推导要点（也是与 Montage 的**本质区别**）

1. **gauge（规范）**：Montage 的“每图对邻居调一半、迭代 N 次”没有定义“所有图共同加一个常数”的零空间。ACSD 的 sky 是**全局增广系统 + 显式 gauge 约束**（如 `1ᵀq = 1` 或固定参考帧），其解在约束下唯一；这是 ACSD 自研，**不能**引 Montage 作依据，也不能说“Montage 也这么做”。
2. **权重**：Montage 用 area 覆盖；ACSD 的 BLUE 用 `Γ⁻¹`（含 shared/sky 交叉项）。二者在“方差 ∝ 1/area 且互不相关”时才一致——这是一个**需要证明的等价条件**，不能默认。
3. **重叠差分的最小二乘**：Montage 对**图像对**做局部拟合；ACSD 对**所有帧**做联合 GLS。前者的解依赖遍历顺序与迭代次数，后者依赖 `Γ` 的正确性。ACSD 若声称“比 Montage 好”，必须给出具体反例（例如三图链式重叠时 Montage 的半步迭代不收敛到全局最小）。
4. **球面多边形面积**：Montage 用 Girard 定理 + 大圆边。ACSD 用真叶（非大圆边）与曲边 SIP 像元的包含型积分；**不能**用 Montage 的“能量守恒”论证替代 ACSD 的面积误差界。
5. **drizzle 的“换角点”**：Montage 的 drizzle 是几何上缩小多边形；ACSD 的 pixfrac 语义必须与此区分（见卡 03）。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `sky.controls` | [src/sky/sky.cpp](../../../src/sky/sky.cpp)、[sky_controls_process.cpp](../../../src/sky/sky_controls_process.cpp) | 连通图/控制、重叠关系 | 不能声称“按 Montage 的 overlap-difference 实现”：ACSD 是全局系统 |
| `sky.fit` | [src/sky/sky.cpp](../../../src/sky/sky.cpp)、[sky_fit_process.cpp](../../../src/sky/sky_fit_process.cpp) | 全局 sky 解与 gauge | 不能引 Montage 证明 gauge 唯一 |
| `coadd.integrate` | [src/coadd/coadd.cpp](../../../src/coadd/coadd.cpp)、[coadd_integrate_process.cpp](../../../src/coadd/coadd_integrate_process.cpp) | 加权积分 | Montage 的 area 加权 ≠ ACSD 的 BLUE |
| `coadd.weights` | [src/coadd/weights.cpp](../../../src/coadd/weights.cpp) | 权重解 | 原文无 BLUE/QP |
| `geometry.overlap` | [src/geometry/overlap.cpp](../../../src/geometry/overlap.cpp) | 球面交叠 | ACSD 的包含型积分不是 Montage 的 Girard 多边形 |

## 6. 不能声称的结论（红线）

- ❌「Montage 证明了 area 加权是最优的」：原文只说它是自然的权重，没有统计最优性证明。
- ❌「Montage 有全局 gauge」：没有；它是局部两两调整 + 固定迭代。
- ❌「Montage 的 overlap 背景就是 ACSD 的 sky」：目标函数、约束、可辨识性都不同。
- ❌「Girard 定理 + 大圆边对 ACSD 的叶足够准」：叶边界不是大圆（见卡 04），大像元下近似失效。
- ❌「Montage 是同行评议的一手论文」：它是官方软件文档；工具论文另有一篇（本轮未读）。

## 7. 与现有 ACSD 文档的差异

1. [docs/references/README.md](../README.md) 的 Montage 行写「重叠差分 sky；ACSD 统一 gauge/同数据 C 为独立推导」——**准确**，本卡提供原文依据（“half this amount”“fixed number of times”“planar fit”）。
2. [STATISTICS_SKY_COADD.md](../../science/foundation/STATISTICS_SKY_COADD.md) 若引用 Montage 作为“背景匹配”的对照，建议补上“Montage 用两两重叠差分的半步迭代，无 gauge、无误差传播”这句限定，避免读者误以为 ACSD 复用了它的解算器。
3. Montage 算法页把 drizzle 说成“换角点”，与 Fruchter–Hook 的 drop 语义在**多边形收缩**上一致，但 Montage 没有 `s²` 面亮度因子与相关噪声 R；ACSD 引用时不要混。

## 8. 待办

- [ ] 读 Montage 工具论文（arXiv:1005.4454），核对它是否给出背景算法的收敛/误差陈述；若有，补进本卡。
- [ ] 构造“三图链式重叠”反例，验证 Montage 式半步迭代与全局 GLS 的差异（这是 ACSD 声称“独立推导”的最小证据）。
