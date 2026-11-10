# 01｜FITS WCS Paper I / Paper II：像素原点、PC/CD、TAN、LONPOLE

**状态：`PRIMARY_READ_AND_MAPPED`**（读的是 arXiv 版正文；A&A 卷页由 NASA 官方 FITS WCS 页核对）
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | Paper I | Paper II |
|---|---|---|
| 题名 | Representations of world coordinates in FITS | Representations of celestial coordinates in FITS |
| 作者 | E. W. Greisen, M. R. Calabretta | M. R. Calabretta, E. W. Greisen |
| 期刊卷页 | A&A **395, 1061–1075** (2002) | A&A **395, 1077–1122** (2002) |
| 永久链接（本次实际读取） | <https://arxiv.org/abs/astro-ph/0207407v2>（v2, 2002-11-13） | <https://arxiv.org/abs/astro-ph/0207413v1>（v1, 2002-07-19，标 “Draft dated 17 Jul 2002”） |
| 出版版入口 | <https://doi.org/10.1051/0004-6361:20021326>（未在本次读取中打开；DOI 解析 302 到 aanda.org） | <https://doi.org/10.1051/0004-6361:20021327>（同上，**两 DOI 的 I/II 归属未逐一对上**） |
| 卷页出处 | NASA FITS WCS 官方页 <https://fits.gsfc.nasa.gov/fits_wcs.html> 明列 Paper I 395,1061-1075 / Paper II 395,1077-1122 | 同左 |
| 许可 | arXiv 非独占分发许可；A&A 出版版版权 ESO | 同左 |

> **版本声明**：下文节/式号按上述 **arXiv 版** 核对。A&A 出版版页码（1061–1122）与 arXiv 排版不同，**出版版的具体页码未逐一核对**，故本卡不写出版版页号。Paper II 的 v1 自标 “Draft dated 17 Jul 2002”，是 A&A 投稿稿的 LaTeX 源；式号与出版版是否完全一致**未验证**。

## 2. 已核对的章节/式（逐条）

### Paper I（astro-ph/0207407v2）

| 位置 | 原文内容要点 |
|---|---|
| §2.1.1 式 (1) | `q_i = Σ_j m_ij (p_j − r_j)`，`r_j = CRPIX_j`；`m_ij` 为 N×N 非奇异方阵 |
| §2.1.1 式 (2) | `x_i = s_i q_i`（`s_i = CDELT_i`），即先无量纲像素偏移、后乘物理尺度 |
| §2.1.2 式 (3) | CD 形式合并：`x_i = Σ_j (s_i m_ij)(p_j − r_j)` |
| §2.1.2 | PC 默认 `1.0 (i=j), 0.0 (i≠j)`；CD 默认**只要出现一个 CD 卡，其余全为 0**；「**We specifically prohibit mixing of the PCij and CDij nomenclatures in any FITS header data unit**」；PC/CD 必须可逆 |
| §2.1.2 式 (4) | 可选的 PC 归一化 `Σ_j PC_ij² = 1`；原文明确「this constraint is optional」 |
| §2.1.4 | 「**integer pixel numbers refer to the center of the pixel** … the first pixel runs from pixel number 0.5 to pixel number 1.5 on every axis」 |
| §2.1.4 | 可逆性要求：一个像素坐标必须能由世界坐标唯一恢复；但**不要求**每个像素都有有效世界坐标（投影边界外无定义） |
| §1 / Table 8 | 关键字状态：`CROTAi` deprecated，`PCi_ja`/`CDi_ja` new；`CTYPEia` 的 “4-3” 形式（前 4 字符坐标类型、第 5 字符 “-”、后 3 字符算法码） |

### Paper II（astro-ph/0207413v1）

| 位置 | 原文内容要点 |
|---|---|
| §1 式 (1) | `x_i = s_i Σ_j m_ij (p_j − r_j) = Σ_j (s_i m_ij)(p_j − r_j)`；「Equation (1) establishes that **the reference point is the origin of intermediate world coordinates**」；投影平面坐标 `(x,y)` 的原点即参考点 |
| §1 | 天球轴要求 `CUNITia = 'deg'`；投影平面 (x,y) 以度为单位 |
| §2.2 | `LONPOLEa` = 天球极在原生坐标系中的经度；**默认值：δ0 ≥ θ0 取 0°，δ0 < θ0 取 180°**；天顶投影（θ0=90°）默认恒为 180°（δ0=90° 除外） |
| §2.3 式 (2) | 由 (αp,δp,φp) 给出的 ZXZ 欧拉旋转：`α = αp + arg(sinθ cosδp − cosθ sinδp cos(φ−φp), −cosθ sin(φ−φp))`，`δ = sin⁻¹(sinθ sinδp + cosθ cosδp cos(φ−φp))` |
| §2.3 式 (5) | 逆旋转（(α,δ)→(φ,θ)），式 (6)(7) 为常用恒等式 |
| §2.4 式 (8)(9)(10) | 非天顶投影由 (α0,δ0) 反解 (αp,δp)；式 (8) 的 `cos⁻¹` 有双解，用 `LATPOLEa` 选择 |
| §5.1 式 (12)(13)(14)(15) | 天顶类：`x = Rθ sinφ`，`y = −Rθ cosφ`；`φ = arg(−y,x)`；`Rθ = √(x²+y²)` |
| §5.1.1 式 (16) | 天顶透视一般式 `Rθ = (180°/π)(μ+1)cosθ/(μ+sinθ)` |
| **§5.1.3 式 (54)(55)** | **TAN（球心投影，μ=0）**：`Rθ = (180°/π) cot θ`；逆 `θ = tan⁻¹(180°/(π Rθ))` |
| Table 12 | TAN：φ0=0°, θ0=90°, 无必需 PV 参数 |
| Appendix A.1 式 (A.1)(A.2) | 方向余弦矩阵形式的球面旋转（可预计算、转置即逆） |
| Appendix A.2 | 明确指出投影逆解**一般需要迭代**，且 p2(δ) 在跨 φ=±180° 与过极时可能不连续/多值；原文不解决这些困难，交由 WCSLIB |

## 3. 原文的假设与适用域

1. **线性变换 + 可分步非线性**：Paper I 的多步模型（pixel → intermediate pixel → intermediate world → world）成立的前提是第一步为可逆线性映射；投影非线性只在最后一步。
2. **参考点是中间世界坐标原点**（Paper II §1），不是“图像中心”，也不要求落在图内。
3. **TAN 定义域**：`θ=0`（即距切点 90°）处发散（Fig. 8 与式 (54) 的 `cot θ`）。原文**没有**给出“安全角距”常数；任何 60° 之类的限域都是实现方自选的保守域。
4. **旋转参数化**：LONPOLE/LATPOLE 只定义 (φ,θ)→(α,δ) 的旋转；它们**不**改变投影本身。
5. **原文不提供**：逆解收敛证明、曲边像元面积、SIP 类畸变（Paper IV 当时未定稿）、自写求解器的数值误差界。

## 4. ACSD 采用/变体所需的独立推导要点

以下为“从原文式到 ACSD 目标量”的桥，**属于 ACSD 自己的推导**（正本在 [OBSERVATION_COORDINATES.md](../../science/foundation/OBSERVATION_COORDINATES.md) §3），不是原文结论：

1. **像素原点 ±1**：Paper I §2.1.4 规定整数像素号在像元中心、第一像元占 [0.5,1.5]。因此若内部用 0-based 中心坐标 `p0`，则 `u = p0 + 1 − CRPIX`（CRPIX 为 FITS 1-based）。这一步是**恒等换元**，但漏掉即整幅偏 1 像元；ACSD 把 `FitsPixel1` 与内部 0-based 分开正是为此。
2. **TAN 前向闭式**：把式 (54) 代入式 (12)(13) 得
   `x = (180°/π) cotθ sinφ`、`y = −(180°/π) cotθ cosφ`；
   反向用式 (14)(15)(55)。ACSD 的切基形式 `q = ((n·e), (n·n_N))/(n·c)` 与之等价（相差 180°/π 的度-弧度与符号约定），需在实现里显式核对符号，不能“看起来一样”。
3. **LONPOLE 折进 CD 的合法性（仅天顶投影）**：LONPOLE 只经 `(φ−φp)` 进入式 (2)/(5)。对天顶类，式 (14) 给出 `φ = arg(−y,x)`，故 `φp → φp+Δ` 严格等价于把 `(x,y)` 绕原点旋转 `Δ`。因此在 **TAN 域内**把 LONPOLE 折进线性矩阵是代数恒等式（ACSD 规范化的依据）；**对圆柱/圆锥等 (φ0,θ0)≠(0,90°) 的投影不成立**，因为那些投影的 `(x,y)` 与 `φ` 不是简单极坐标关系。
4. **CD 行列式符号**：Paper I 只要求可逆。负行列式表示手性翻转，是合法头；ACSD 不把负行列式当负面积，需在面积计算处另取 `|det|` 并保留方向。
5. **PC 与 CD 互斥**：Paper I 明文禁止同一 HDU 混用。ACSD reader 必须显式拒绝，而不是“先读 CD 再补 PC”。
6. **可逆性 ≠ 单射证明**：Paper I 的可逆性要求是“不得两个像素映到同一世界坐标”，但原文不给判定算法。ACSD 的 SIP 单射证书（`sup_U ||∇(A,B)||∞ < 1` 为充分条件）是自研的，不能引 Paper I 作证明。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 与原文的对应 | 不能声称 |
|---|---|---|---|
| `coordinates.wcs` | [src/coordinates/wcs.cpp](../../../src/coordinates/wcs.cpp)（`mat2_invert`、`kTan60` 域常数）、[wcs_process.cpp](../../../src/coordinates/wcs_process.cpp) | Paper I 式 (1)–(3) 的线性步；Paper II 式 (2)/(5)/(8)/(54)/(55) | 不能声称实现了一般 FITS WCS：ACSD 首期只有二维 `RA---TAN`/`DEC--TAN`（±`-SIP`） |
| `coordinates.wcs` | [src/coordinates/tan_sip.cpp](../../../src/coordinates/tan_sip.cpp)（`sip_value`/`sip_grad`/`sip_hessian`/`sip_value_iv`） | TAN 前向/逆 + SIP（见卡 02） | 自写逆 Newton 的正确性不由 Paper II 保证（Appendix A.2 明说逆解困难） |
| `coordinates.wcs` | [src/coordinates/footprint_provider.cpp](../../../src/coordinates/footprint_provider.cpp) | 曲边足迹 = 连续映射下的像元边界，不是原文内容 | 原文没有“曲边像元面积”的公式 |
| `projection.export` | [src/projection/export_projection.cpp](../../../src/projection/export_projection.cpp)（`make_target_footprint`） | 目标像元 WCS 由 Paper I/II 定义 | 不能声称 FITS 外部读者会得到相同数值：只保证头语义 |
| `sources.astrometry` | [src/sources/astrometry_wcs.cpp](../../../src/sources/astrometry_wcs.cpp) | 用同一 WCS 做像素↔天球 | 不能把 WCS 的舍入误差当星位物理误差 |

## 6. 不能声称的结论（红线）

- ❌「ACSD 实现了 FITS WCS 标准」：只实现了二维 TAN（±SIP）子集，PV/TPV/CPDIS/D2IM 明确 `unsupported_distortion`。
- ❌「Paper I/II 证明了我们的逆求解器正确」：原文只给前向定义，逆解交给迭代实现且自认有困难。
- ❌「LONPOLE 折进 CD 对任意投影都成立」：只对天顶类成立（§4.3）。
- ❌「TAN 的有效域是原文规定的」：原文只说 θ=0 发散；60° 是 ACSD 自选保守域。
- ❌「出版版页码/式号已核对」：本卡只对 arXiv 接受版负责；两篇的 A&A DOI 归属也未逐一确认（见 §1）。

## 7. 与现有 ACSD 文档的差异（2026-11-10 发现）

1. [OBSERVATION_COORDINATES.md](../../science/foundation/OBSERVATION_COORDINATES.md) §3 说 LONPOLE 规范化「是二维基变换的代数恒等式」，结论正确，但**未写出“仅天顶投影成立”这一前提**；建议补上，否则未来扩到 CAR/MOL 时会静默错。
2. 同文 §3 的「`sup_Q ||∇(A,B)||∞ < 1` 是充分条件」是自研判据，文档已标“可编码的充分域”，与原文无冲突；但**不能**把它写成“FITS/SIP 的要求”。
3. [SPHERICAL_DRIZZLE.md](../../science/foundation/SPHERICAL_DRIZZLE.md) 引 Fruchter–Hook 与 HPX 时未涉及 WCS 页/式；本卡补上 Paper I 式 (1)–(3)、Paper II 式 (54) 的锚点。

## 8. 待办

- [ ] 打开 A&A 出版版 PDF，逐条确认式号与 arXiv 版一致，并把出版版页码写入本卡（当前只写卷页，不写页号）。
- [ ] 确认 `10.1051/0004-6361:20021326` / `...327` 分别对应 Paper I / II。
- [ ] 由独立复核者脱离源码、从式 (54) 推 ACSD 切基形式并核对符号。
