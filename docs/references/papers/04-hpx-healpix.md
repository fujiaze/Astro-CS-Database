# 04｜HPX 投影与 HEALPix 像素化：等面积、NESTED、真叶与缝

**状态：`PRIMARY_READ_AND_MAPPED`（HEALPix 本体 + HPX 2004 预印本）；`PRIMARY_LOCATED_NOT_YET_READ`（Calabretta & Roukema 2007 MNRAS 出版版）**
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | HEALPix 本体 | HPX 投影 |
|---|---|---|
| 题名 | HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere | Mapping on the HEALPix grid |
| 作者 | K. M. Górski, E. Hivon, A. J. Banday, B. D. Wandelt, F. K. Hansen, M. Reinecke, M. Bartelmann | 2004 预印本：M. R. Calabretta（**单作者**）；2007 出版版：M. R. Calabretta, B. F. Roukema |
| 出处 | ApJ **622, 759**（2005）——卷页按仓库既有索引与常见引用，**本次未在原文/arXiv 页核对到 journal-ref** | MNRAS **381, 865**（2007）——同上，**未核对** |
| DOI | <https://doi.org/10.1086/427976>（解析 302，未打开出版版） | <https://doi.org/10.1111/j.1365-2966.2007.12297.x>（解析 302 → OUP，**出版版返回 403，未读**） |
| 实际读取的永久链接 | <https://arxiv.org/abs/astro-ph/0409513v1>（v1, 2004-09-21） | <https://arxiv.org/abs/astro-ph/0412607v1>（v1, 2004-12-23，A&A 稿件格式，**单作者**） |
| 许可 | arXiv 非独占；ApJ 版权 AAS | arXiv 非独占；MNRAS 版权 RAS |

> **版本声明（重要）**：ACSD 现有文档把 `astro-ph/0412607` 标成 “Calabretta & Roukema 2007，§2.1–2.4、式(3)–(10)”。我核对了该 arXiv 条目：它是 **2004 年单作者预印本**（A&A manuscript 格式），节号 §2.1–2.4、式号 (3)–(10) 确实存在于**这一版**；但 **2007 MNRAS 出版版（含 Roukema 合著）的节号/式号未读、未核对**。因此：本卡对 2004 预印本负责，**不对 2007 出版版负责**。

## 2. 已核对的章节/式

### Górski et al. 2005（arXiv v1）

| 位置 | 原文内容要点 |
|---|---|
| §5 开头 | `Npix = 12 Nside²`，等面积 `Ωpix = π/(3 Nside²)`；`4 Nside − 1` 条等纬环；赤道带 `−2/3 < cosθ* < 2/3` 每环 `4 Nside` 个像元；极冠 `|cosθ*| > 2/3` 每环每象限递增 1 |
| §5.1 式 (2)–(5) | 北极冠：`i = I(√(ph) − I(√ph)) + 1`、`j = p+1−2i(i−1)`、`z = 1 − i²/(3Nside²)`、`φ = (π/2i)(j − s/2)`，`s=1` |
| §5.1 式 (6)–(9) | 北赤道带：`i = I(p′/4Nside) + Nside`、`j = (p mod 4Nside)+1`、`z = 4/3 − 2i/(3Nside)`、`φ = (π/2Nside)(j − s/2)`，`s = (i−Nside+1) mod 2`；南半球由赤道镜像 |
| §5.1 末 | `|Δz Δφ| = Ωpix`（离散面积元恒定） |
| §5.2 式 (10)–(12) | 基础分辨像元 `f ∈ {0,11}`；`frow = I(f/Nφ)`；`F1(f)=frow+2`、`F2(f)=2(f mod Nφ) − (frow mod 2) + 1` |
| **§5.2 式 (13)(14)** | **NESTED 位交错**：`p′n = [. . . b2 b1 b0]₂`，`x = [. . . b2 b0]₂`（偶位）、`y = [. . . b3 b1]₂`（奇位）；`x` 沿 NE、`y` 沿 NW，原点在该基础像元最南角 |
| §5.2 式 (15)–(18) | `v=x+y`、`h=x−y`；环号 `i = F1(f)Nside − v − 1`；经度号 `j = (F2(f)Nside + h + s)/2` |
| **§5.3 式 (19)–(22)** | 像元边界**不是大圆**：赤道带 `cosθ = a + b φ`，极冠 `cosθ = a + b/φ²`；极冠式 (19)(20) 给出 `z = 1 − (k²/(3Nside²))(π/(2φt))²` 等；基础像元边界式 (21)：`z > 2/3`、`φ = k′π/2`（`k′=0,1,2,3`）；赤道带式 (22)：`z = 2/3 − 4k/(3Nside) ± 8φ/(3π)` |
| §5.3 末 | 由边界积分可验证每像元等面积；`θpix ≡ √Ωpix` |

### Calabretta 2004（astro-ph/0412607，单作者）

| 位置 | 原文内容要点 |
|---|---|
| §2.1（式 3–6） | 等面积类：`x = φ`、`y = sinθ`（赤道带）；极区 `y = ±(2π/H)(2−σ)`、`σ = √(3(1−|sinθ|))`；由 Jacobian 恒定推出 |
| §2.1 式 (7)(8) | `x = φc + (φ−φc)σ`，`φc = (π/H)(⌊(φ+π)H/(2π)⌋ + 1) − π`（极区经度中断） |
| §2.1 式 (9)(10) | 偏导：赤道带 `(∂x/∂φ, ∂y/∂θ) = (1, (3π/2H)cosθ)`；极区 `(σ, (3π cosθ)/(2Hσ))` |
| §2.1.4 | 等面积且边界为“方形”：可被 `3H` 个等面积菱形（facet）完全铺满；`H=4` 时即 **12 facet**；Fig. 2 给出 12 面编号 |
| §2.2 | HEALPix 网格（在 HPX 上的规则网格） |
| §2.3 | 其他像素化（`H=6` 三角等）——ACSD **不采用** |
| **§2.4 式 (15)–(27)** | **FITS 形式（实现应直接对照这一组）**：`(φ0,θ0)_HEALPix = (0,0)`；`|sinθ| ≤ 2/3`：`x = φ`、`y = (270°/H) sinθ`；`|sinθ| > 2/3`：`x = φc + (φ−φc)σ`、`y = ±(180°/H)(2−σ)`、`σ = √(3(1−|sinθ|))`、`φc = (180°/H)(⌊(φ+180°)H/360°⌋ + 1) − 180°`；逆式 (22)–(27) 同构 |
| §2.4 末 | CTYPE 算法码 **`HPX`**；`PVi_1a`（附在纬度轴）指定 `H`，**默认 4**；WCSLIB 3.7 已实现 |

## 3. 原文的假设与适用域

1. **等面积是“整像元”性质**：`Ωpix` 对所有像元相同；但**任意子区域**（例如与 TAN 像元的交叠）的面积必须对真实边界积分，不能按“等面积 ⇒ 面积正比于像元数”直接算——除非该子区域恰为整像元之并。
2. **真叶边界非大圆**：Górski §5.3 明确 `cosθ = a + bφ` / `cosθ = a + b/φ²`。任何把叶当球面直线多边形的算法都在叶内部引入几何误差。
3. **HPX 的 `H` 是投影参数，HEALPix 的 `Nside` 是分辨率参数**：两者独立；FITS 的 `HPX` 头用 `H`（默认 4），而数据索引用 `Nside`（`order` 与之对应 `Nside = 2^order`）。混用会造成 4 倍/12 倍的索引错位。
4. **NESTED 的位交错方向**：`x` 取偶位、`y` 取奇位（式 13/14），且 `x` 沿 NE、`y` 沿 NW。**不是**任意一种“x 左移一位 + y”的实现都等价；位序（LSB 优先）必须照式 (13)(14)。
5. **极区/赤道带分界** `|sinθ| = 2/3` 是**投影的**分界（`|y| = 180°/H`），与 HEALPix 的 `|cosθ*| = 2/3` 是同一件事的两种写法（`θ* = 90°−θ`）。
6. **原文不提供**：真实叶与任意曲边多边形的交叠算法、面积包含区间、`Nside` 与角分辨率之间的误差界（`θpix` 只是名义值）、跨 face 缝的连续性证明（只给编号/边界式）。

## 4. ACSD 采用/变体所需的独立推导要点

1. **投影 ↔ 索引分离**：ACSD 必须把“HPX chart 映射（式 16–21）”与“NESTED 索引（Górski 式 13/14 + `F1/F2`）”当成两个独立可测对象。前者决定面积/几何，后者决定数据寻址。文档/代码若只实现其中一个，另一个必须显式标 `unsupported`。
2. **真叶的面积积分**：叶 = `{(φ,θ): φ ∈ [φ_j, φ_{j+1}], z ∈ [z_k, z_{k+1}]}` 在 HPX chart 下的原像。因为 chart 是等面积的，`Ω(leaf) = ∫∫ |J| dφ dz` 且 `|J|` 在 chart 内分段常数（赤道带）/与 `σ` 有关（极区）；ACSD 的 `dΩ = π/3 ds dt` 之类公式必须由 §2.4 式 (16)–(21) 的 Jacobian 推出，而不是从“等面积”直接断言。
3. **face 缝**：`H=4` 时 12 个 facet；`φc` 的中断在 `φ = ±180°` 与极区扇形边界。跨缝的像元/叶交叠必须按**两个 chart 片**分别积分再相加，且不能重复计边界（零测集）。ACSD 文档提到“赤道 face6 跨全局 HPX 经度 ±180 中断时用旋转 180° 的原生投影中心”——这是 ACSD 的实现选择，需证明与式 (18)/(21) 的 `φc` 分支一致。
4. **`H` 与 `Nside` 的桥**：FITS `HPX` 头的 `H`（默认 4）只描述投影；`Nside` 由数据决定。ACSD 若从 HiPS 读入，`Nside = 2^order`，`order` 是 tile/leaf 的层级，二者不可互换。
5. **NESTED 位序验证**：ACSD 文档称“NESTED x 占偶数位、y 占奇数位”，与式 (13)(14) 一致；但**必须用独立实现**（不复用 writer）验证，因为 LSB/MSB 约定一错就是整图镜像。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `geometry.overlap` | [src/coordinates/healpix.cpp](../../../src/coordinates/healpix.cpp)：`face_point`、`healpix_valid`、`healpix_face`、`healpix_face_xy`、`healpix_face_index` | 12-face chart 与 NESTED 索引；`face_point` 是 ACSD 的 face 参数化 | 不能声称等价于 Górski 式 (13)(14)：位序需独立对照 |
| `coordinates.wcs` | [src/coordinates/chart.cpp](../../../src/coordinates/chart.cpp)、[healpix.hpp](../../../src/coordinates/healpix.hpp) | chart 映射 | 不能声称覆盖 `H≠4`：ACSD 只用 12-face |
| `projection.export` | [src/projection/export_projection.cpp](../../../src/projection/export_projection.cpp)：`leaf_area_sr`（`4π/(12·4^order)`） | 整叶面积 | 整叶面积等式成立，但**不能**把它用于部分叶/交叠面积 |
| `geometry.overlap` | [src/geometry/containment.cpp](../../../src/geometry/containment.cpp)、[exact_polygon.cpp](../../../src/geometry/exact_polygon.cpp) | 叶内包含/裁剪 | 原文无包含算法 |
| `geometry.drizzle` | [src/geometry/drizzle.cpp](../../../src/geometry/drizzle.cpp) | 叶作为 target | 原文的 Drizzle 与 HEALPix 无关，二者拼接是 ACSD 自研 |

## 6. 不能声称的结论（红线）

- ❌「引用了 Calabretta & Roukema 2007 的式 (3)–(10)」：我读的是 **2004 单作者预印本**；2007 出版版未读。
- ❌「HEALPix 等面积 ⇒ 任意交叠面积可数像元得到」：只有整像元之并才成立。
- ❌「叶是大圆四边形」：Górski §5.3 明确边界是 `cosθ = a + bφ` / `a + b/φ²`。
- ❌「`θpix = √Ωpix` 是分辨率误差界」：它只是名义角尺度。
- ❌「NESTED 索引就是简单的 x/y 位交错」：必须同时满足 `F1/F2` 与 LSB 位序（式 10–18）。
- ❌「HPX 的 `H` 等于 HEALPix 的 `Nside`」：前者是投影中断数，后者是分辨率。

## 7. 与现有 ACSD 文档的差异

1. **[SPHERICAL_DRIZZLE.md](../../science/foundation/SPHERICAL_DRIZZLE.md) 参考条目 2** 写「Calabretta & Roukema 2007，§2.1–2.4、式(3)–(10)」，链接指向 `arxiv.org/html/astro-ph/0412607`。该链接是 **2004 单作者预印本**（我核对了节号与式号，确实对得上）。**问题**：(a) 作者/年份与链接不符；(b) 式 (3)–(10) 是**推导式**，而实现应直接对照的 FITS 形式是式 (15)–(27)。建议改为「Calabretta 2004 预印本 §2.1–2.4、式(3)–(10)；FITS 形式见 §2.4 式(15)–(27)」，或补上 2007 MNRAS 的核对。
2. 同文提到「`dΩ=π/3 dsdt`、12face/`r=0,1,3,4` 分支」——这些是 ACSD 自研参数化，与 HPX 原文的 `(φ,θ)` 形式不同；文档已标“自研候选”，无夸大，但**必须**给出从式 (16)–(21) 到该参数化的映射推导，否则读者无法核。
3. [OBSERVATION_COORDINATES.md](../../science/foundation/OBSERVATION_COORDINATES.md) §（HiPS 段）说「HPX 头的 CD 列分别为 `(-45/N,+45/N)` 与 `(-45/N,−45/N)` degree/pixel」——这是 IVOA HiPS 的序列化约定，**不是** HPX 原文内容；原文只说 `PVi_1a` 给 `H`。该处应引 IVOA HiPS 规范，而不是 HPX 论文。
4. `docs/references/README.md` 的 HEALPix 行写「Górski et al. (2005), DOI 10.1086/427976，NESTED 规则」——方向正确；本卡补上具体式号。

## 8. 待办

- [ ] 取得 2007 MNRAS 出版版（机构订阅/作者主页），核对 §2.1–2.4 与式号，替换本卡的“2004 预印本”锚点。
- [ ] 核对 ApJ 622, 759 与 MNRAS 381, 865 的卷页（本次只核对了 DOI 解析）。
- [ ] 独立实现 NESTED 编码，对照 Górski 式 (13)(14) 与 `F1/F2`，覆盖 `Nside = 1,2,8` 与极区/赤道带边界。
