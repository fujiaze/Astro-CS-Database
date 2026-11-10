# 05｜Gaia XP：外定标仪器模型与合成光度

**状态：`PRIMARY_READ_AND_MAPPED`**（两篇均读 arXiv 版正文，A&A proofs 格式；A&A 文章号未核对）
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | 外定标 | 合成光度 |
|---|---|---|
| 题名 | Gaia Data Release 3: External calibration of BP/RP low-resolution spectroscopic data | Gaia Data Release 3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra |
| 作者 | P. Montegriffo et al.（DPAC） | Gaia Collaboration, P. Montegriffo, M. Bellazzini, F. De Angeli et al. |
| 出处 | A&A **674, A3**（2023）——文章号按仓库既有索引，**本次未核对 A&A 排版文章号** | A&A **674, A33**（2023）——同上 |
| DOI | <https://doi.org/10.1051/0004-6361/202243880>（解析 302） | <https://doi.org/10.1051/0004-6361/202243709>（解析 302） |
| 实际读取 | <https://arxiv.org/abs/2206.06205v1>（manuscript no. `43880final`, 2022-06-14） | <https://arxiv.org/abs/2206.06215v2>（manuscript no. `43709corr_MB_wzenodo`, 2023-01-11） |
| 许可 | arXiv 非独占；A&A 版权 ESO | 同左 |

> **版本声明**：节/式号按 arXiv 版。两篇的 A&A 出版版文章号（A3 / A33）与页码未在本次读取中核对；**不写出版版页号**。

## 2. 已核对的章节/式

### Montegriffo et al.（外定标，arXiv 2206.06205）

| 位置 | 原文内容要点 |
|---|---|
| §2 式 (1) | 仪器模型的**积分方程**：`n_e(u) = ∫ I(u,λ) · n_p(λ) dλ`；`n_e(u)` 是内部定标后的平均谱（单位 e⁻ s⁻¹），`u` 是**伪波长**（pseudo-wavelength）坐标 |
| §2（式 1 后） | 核 `I` = LSF × 色散模型 × 响应模型三者的组合；响应 = 每波长间隔探测到的光子数 / 入瞳光子数 |
| §2 式 (2) | 探测器是光子计数器件，故全用**光谱光子通量** `n_p(λ)`：`n_p(λ) = 10⁻⁸ λ/(hc) · f(λ)`（`f` 单位 W m⁻² nm⁻¹；`10⁻⁸` 补 nm 单位） |
| §2 关键假设 (a) | 外定标模型**对所有源唯一**，不依赖源光度或颜色；若违背，会表现为不同源类的系统差 |
| §2 关键假设 (b) | IM 参数化为**对名义仪器的改正**（名义仪器来自发射前知识与实验室测量），以处理 IM 解的简并 |
| §2 | 传统“观测谱/模型谱 = 响应”的做法对 Gaia 类仪器**不成立**：LSF 宽于响应变化尺度，导出的响应会随定标星光谱型变化 → 必须显式建模 LSF，且需要大定标星集 |
| §3 | 定标星：111 颗 SPSS，定标到 CALSPEC，通量精度约 **1%** |
| §5（标题） | “Basis inversion and SED reconstruction”；**DR3 的基函数数目 `N = 55`**（BP、RP 各一套） |
| §4.2 | LSF 模型：二维基函数 + 色散模型（本文只记结构，不引具体式号） |
| 摘要/§2 | 优化用 **total least-squares**（TLS），同时计入 Gaia 与外部谱的误差 |
| §9 | “Known problems and caveats”——系统差仍存在 |

### Gaia Collaboration（合成光度，arXiv 2206.06215）

| 位置 | 原文内容要点 |
|---|---|
| §2 式 (1) | VEGAMAG / Johnson–Kron–Cousins 的平均通量：`<f_λ> = ∫ f_λ(λ) S(λ) λ dλ / ∫ S(λ) λ dλ`；**`S(λ)` 是光子响应曲线**（含 CCD 量子效率），故分子分母都带 `λ` |
| §2 式 (2) | AB 系统：`<f_ν> = ∫ f_λ S λ dλ / ∫ S(λ)(c/λ) dλ` |
| §2 式 (3)(4) | `mag = −2.5 log<f> + ZP`；VEGAMAG 的 ZP 由参考 SED 定义 `ZP = +2.5 log<f_λ,ref> + V` |
| **§2 式 (6)** | XP 谱的**基函数表示**：`f_λ^XP(λ) = Σ_{i=1}^{N} b_i^XP φ_i^XP(λ)`；Archive 存的是系数 + 协方差 |
| §2 式 (7)(8) | BP/RP 重叠区用线性权重 `w_BP(λ)` 合并，`w_RP = 1 − w_BP` |
| **§2 式 (9)** | **每个通带的基积分可预计算**：`s_i^XP = ∫ w_XP(λ) φ_i^XP(λ) S(λ) λ dλ / ∫ S(λ) λ dλ` |
| **§2 式 (10)** | 平均合成通量：`<f_λ> = Σ_i b_i^BP s_i^BP + Σ_i b_i^RP s_i^RP` —— 这正是“通带积分 = 系数 × 预积分基”的高效算法 |
| §1 脚注 4 / App. B | `R_f ≡ FWHM_passband / FWHM_LSF`；**经验判据：`R_f ≥ 1.4` 时可在强谱线存在下仍保通量**（“conservative criterion”） |
| **App. B 式 (B.1)** | `R_f = FWHM_passband(λ0) / FWHM_LSF(λ0)`；原文明确该参数**不能**处理所有细节（FWHM 不足以描述非对称 TC），`R_f > 1` 在理想对称情形下才保证通量守恒 |
| §1 | XP 波长范围 330–1050 nm，`λ/Δλ ≃ 25–100`；约 2.2 亿源，多数 G < 17.65 |
| §2.1/§2.2 | 误差估计与 **standardisation**（把 XP 合成星等对齐到外部系统）——standardisation 不是外定标本身 |

## 3. 原文的假设与适用域

1. **XP 谱是“低维基 + 系数 + 协方差”的表示**，不是逐波长独立测量；系数误差**相关**（基函数重叠 + 长程相关），所以“逐点误差条”会低估。
2. **仪器模型对所有源唯一**（Montegriffo §2(a)）；任何源依赖的响应变化都属模型违背。
3. **外定标的绝对精度约 1%**（SPSS 定标到 CALSPEC 的精度），不是“任意精度”。
4. **`R_f ≥ 1.4` 是经验/保守判据**，由 Hα/Hβ 强线实验得出（App. B 用 HR 谱对照），**不是定理**；原文自己说 FWHM 不足以描述非对称 TC。
5. **合成光度依赖通带定义与 `S(λ)` 的光子/能量身份**：式 (1)(2) 的 `λ` 因子来自光子计数；用能量通量约定会差一个 `λ`。
6. **原文不提供**：任意窄带（`R_f < 1`）的误差界、逐源协方差的完整传播到合成星等、3–7 nm 谱线通量的可恢复性、非恒星 SED 的适用性。

## 4. ACSD 采用/变体所需的独立推导要点

1. **从式 (9)(10) 到 ACSD 的 `∫ b_l T` 预积分**：ACSD 的 [GAIA_RESPONSE.md](../../science/foundation/GAIA_RESPONSE.md) 写“对 Gaia 外定标基函数 `b_l(λ)` 先算 `∫ b_l T ...`”，与式 (9) 同构。**必须**显式声明：`s_i` 是**每个通带**的常数，因此换通带只需重算 `s_i`，不必重读源系数；这是实现效率的来源，也是“通带可加性”成立的前提（线性）。
2. **光子/能量身份**：式 (1)(2) 的分母分别是 `∫Sλdλ` 与 `∫S(c/λ)dλ`，差别就是光子计数权重。ACSD 的 `photon_weight(λ) = λ·k`（[src/response/spectral.cpp](../../../src/response/spectral.cpp)）必须与式 (1) 的 `λ` 因子对齐；若响应已含 QE，则不能再乘一次。
3. **协方差传播**：`<f> = Σ b_i s_i` 是线性的，故 `Var(<f>) = sᵀ Cov(b) s`。ACSD 必须保留 `Cov(b)`（若可得）或显式标 unknown；**不能**用 `Σ (s_i σ_i)²` 冒充（那假设系数独立）。
4. **`R_f` 的正确用法**：ACSD 文档写“`R_f ≳ 1.4`，但并非所有源的硬阈值”——与 App. B 一致。实现上 `R_f` 只能作为**输入校验/警告**，不能当误差界；真正的误差要另算（数值求积 + 系数协方差 + 模型偏差）。
5. **外定标 ≠ standardisation**：Montegriffo 的 ECS 是绝对通量标度；Gaia SP 论文的 standardisation 是把合成星等对齐到外部系统（引入额外经验改正）。ACSD 若用 standardisation，必须把它记成独立的响应/零点层，不能混进 `T×QE`。
6. **`N = 55` 的基不是正交完备谱**：基展开的截断误差属模型偏差，不是随机误差。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `response.integrate` | [src/response/spectral.cpp](../../../src/response/spectral.cpp)：`basis_value`（式 6）、`response_value`（`S(λ)`）、`photon_weight`（式 1 的 `λ`）、`provenance_declares_zero_tail` | 式 (6)(9)(10) 的谱表示与预积分 | 不能声称 Gaia 官方实现等价：ACSD 只做通带积分，不做 TLS 定标 |
| `response.integrate` | [src/response/integrate_process.cpp](../../../src/response/integrate_process.cpp) | 分段积分与误差分账 | 原文无分段求积误差界 |
| `photometry.fit` / `photometry.apply` | [src/photometry/joint_fit.cpp](../../../src/photometry/joint_fit.cpp)、[spatial_basis.cpp](../../../src/photometry/spatial_basis.cpp)、[apply_scale.cpp](../../../src/photometry/apply_scale.cpp) | 联合标度、空间应用 | 不能声称恢复 XP 的谱线信息 |
| `sources.measure` | [src/sources/measure.cpp](../../../src/sources/measure.cpp) | 把 Gaia 星作为参考/定标 | 不能把 1% 绝对精度当随机误差 |

## 6. 不能声称的结论（红线）

- ❌「XP 谱是逐波长独立测量，误差条可逐点用」：基表示 + 长程相关，逐点误差会低估。
- ❌「`R_f ≥ 1.4` 保证 3 nm 窄带正确」：App. B 的判据是经验性的，且 FWHM 不足以描述非对称 TC；`R_f < 1` 时原文明确会失败。
- ❌「外定标模型对任何源都成立」：Montegriffo §2(a) 明说这是**假设**，违背会表现为源类系统差。
- ❌「ACSD 复现了 Gaia 的外定标」：ACSD 只消费已定标的 XP 系数，不做 TLS/仪器模型拟合。
- ❌「合成星等误差 = 系数误差传播」：还有模型偏差、standardisation 改正、通带形状误差。

## 7. 与现有 ACSD 文档的差异

1. [GAIA_RESPONSE.md](../../science/foundation/GAIA_RESPONSE.md) 参考 1 写「Gaia Collaboration 2023，A&A 674 A33，**§2、Appendix B**」，并称「原文 Appendix B 定义 `R_f = FWHM_band/FWHM_LSF`，一般建议 `R_f ≳ 1.4`」——**核对通过**：App. B 式 (B.1) 与“R_f ≥ 1.4”确实存在（arXiv v2 中位于 App. B 开头与 §1 脚注 4）。唯一需注意的是原文用词是 “conservative criterion”，不是“一般建议”。
2. 同文说「3–7 nm 的求积可以极精确，但未知高频谱结构不能由低分辨 XP 唯一恢复」——与原文一致，无夸大。
3. 同文参考 2 写「Montegriffo et al. 2023，外定标，A&A 674 A3」并给 aanda PDF 链接——方向正确；本卡补上**式 (1)(2) 与 §5 的 `N=55`**，便于审计。
4. 同文正文写「若响应自身已包含 photon conversion，则不再乘 `λ/(hc)`」——与 Montegriffo 式 (2) 一致（该式只在把 `f_λ` 转成光子通量时才乘 `λ/(hc)`）。

## 8. 待办

- [ ] 核对两篇 A&A 出版版文章号/页码（A3 / A33 目前只来自仓库既有索引）。
- [ ] 取 `Cov(b)` 的实际结构（Gaia Archive 文档），确认 ACSD 的协方差传播是否可实现；若不可得，必须在文档里标 `unknown`。
- [ ] 独立复核者用式 (9)(10) 对一条已知 SED 的通带积分做外部对照（GaiaXPy 只作参照，不作证明）。
