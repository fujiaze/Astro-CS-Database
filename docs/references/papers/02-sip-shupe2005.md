# 02｜SIP：A/B、AP/BP 与 CRPIX 单位

**状态：`PRIMARY_READ_AND_MAPPED`**（读的是 CaltechAUTHORS 公开的**出版版** PDF，ASP Conf. Ser. 347, pp. 491–495）
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | 内容 |
|---|---|
| 题名 | The SIP Convention for Representing Distortion in FITS Image Headers |
| 作者 | D. L. Shupe, M. Moshir, J. Li, D. Makovoz, R. Narron, R. N. Hook |
| 出处 | ASP Conference Series **Vol. 347**, *Astronomical Data Analysis Software and Systems XIV*, eds. Shopbell, Britton, Ebert, **pp. 491–495** (2005) |
| 永久标识 | CaltechAUTHORS 记录 <https://authors.library.caltech.edu/records/fqnyw-qdw79>（Eprint ID 24809）；**该章无 DOI** |
| 公开 PDF | <https://authors.library.caltech.edu/records/fqnyw-qdw79/files/Shupe2005p9679Astronomical_Data_Analysis_Software_And_Systems_Xiv_Proceedings.pdf>（174.5 kB；md5 `e754633c0f373a4d096169f86b428e6a`，与记录页所载一致，**本次下载后已核**） |
| 许可 | 记录页 Rights: “Other — No commercial reproduction, distribution, display or performance rights”。**可读、不可再分发**：本仓库不镜像该 PDF。 |
| 标准地位 | NASA 官方 FITS WCS 页把 SIP 列在「**World Coordinate Systems Conventions (non-standard)**」下——它是**注册的局部约定，不是 IAU-FWG 标准**。 |

## 2. 已核对的章节/式

| 位置 | 原文内容要点 |
|---|---|
| §2（p.491–492） | 「The SIP convention derives its name from the four characters ‘**-SIP**’ that are appended to the values of `CTYPE1` and `CTYPE2`」；`-SIP` 来自 Paper IV 早期草案，**最新草案已删除该形式** |
| §2（p.492） | 「We define `u, v` as **relative pixel coordinates with origin at CRPIX1, CRPIX2**」；`x, y` 为 Paper II 的 intermediate world coordinates（度），原点在 `CRVAL` |
| §2 式 (1) | `[x; y] = [CD1_1 CD1_2; CD2_1 CD2_2] · [u + f(u,v); v + g(u,v)]` —— **CD 作用在 SIP 之后** |
| §2 式 (2)(3) | `f(u,v) = Σ_{p+q≤A_ORDER} A_pq u^p v^q`，`g(u,v) = Σ_{p+q≤B_ORDER} B_pq u^p v^q`；p,q 从 0 起，但**线性项折进 CD**，故实际 p+q ≥ 2（例：三阶展开为 `A_2_0 u² + A_0_2 v² + A_1_1 uv + A_2_1 u²v + A_1_2 uv² + A_3_0 u³ + A_0_3 v³`） |
| §2 式 (4)(5)(6) | 逆变换：`[U;V] = CD⁻¹ [x;y]`，再 `u = U + F(U,V)`、`v = V + G(U,V)`，`F/G` 用 `AP_pq`/`BP_pq` 与 `AP_ORDER`/`BP_ORDER` |
| §3（p.493） | Spitzer IRAC ch4 实际头：`CTYPE1='RA---TAN-SIP'`、`CRPIX1=CRPIX2=128.`、`A_ORDER=2`、`AP_ORDER=2`；并指出「reverse coefficients have the opposite sign and roughly the same absolute values … **However, this is not true for some more distorted fields of view**, so the Spitzer headers retain the reverse coefficients in general」 |
| §4（p.494） | 读取方清单（MOPEX、WCSTools/DS9、Montage、GAIA/AST、IDL ASTROLIB、Drizzle）——**是采用面，不是正确性证明** |
| §5（p.494–495） | HST ACS/WFC 畸变在角上 >50 像元，另加更大的线性 skew；用 IDCTAB 生成 SIP 关键字 |

## 3. 原文的假设与适用域

1. **SIP 是“像素平面上的多项式畸变 + 一个线性 CD”**，不改变投影；投影仍由 `CTYPE` 的 3 字符码（TAN 等）决定。
2. **`u,v` 的单位是像素**，原点在 `CRPIX`，且**沿用 FITS 1-based 像元中心约定**（Paper I §2.1.4）。SIP 原文**没有重述**这一点——它假定读者已按 FITS 约定读 `CRPIX`。这是实现中最常见的 off-by-one 来源。
3. **线性项不入 A/B**：`A_1_0`、`A_0_1`、`B_1_0`、`B_0_1` 不在约定内（线性部分由 CD 承担）。原文的例子里也没有这些项。
4. **AP/BP 是“for fast inversion”的近似逆**，原文只承诺“fast”，**不承诺**它是 `(A,B)` 的精确逆，也不给残差界。
5. **原文不提供**：多项式有效域、单射条件、系数不确定度、高阶项数值稳定性、`-SIP` 与 Paper IV 的互操作规则。

## 4. ACSD 采用/变体所需的独立推导要点

1. **完整前向链**：`p0`（0-based 中心）→ `u = p0+1−CRPIX` → `(u+f, v+g)` → `CD` → `(x,y)` 度 → TAN（Paper II 式 54）→ `(φ,θ)` → 旋转 → `(α,δ)`。ACSD 的 `PixelCenter0/FitsPixel1` 分型就是为了让 ±1 只出现一次。
2. **逆解**：由式 (4) 得初值 `(U,V) = CD⁻¹q`，再用**有阻尼 Newton**解 `u + f(u,v) = U`、`v + g(u,v) = V`，Jacobian 为 `I + ∇(A,B)`。`AP/BP` 只能当初值。停止判据必须建在 detector 像素残差上（ACSD 规范如此），不能建在 `AP/BP` 的自洽上。
3. **单射充分条件（ACSD 自研）**：若在闭凸域 `U` 上 `sup ||∇(A,B)||∞ < 1`，则 `(u,v) ↦ (u+f, v+g)` 在 `U` 上单射（`I + ∇` 非奇异且映射为压缩扰动）。这是**充分不必要**条件；不满足时只能报 `injectivity_unresolved`，不能宣称“检测到 fold”。
4. **SIP 与裁剪/装箱的复合**：若新像素坐标 `p_new = M p_old + b`，则 `u_new = M u_old + (M r_old + b − r_new)`，且 `D_new(u) = M⁻¹ D_old(M u)`。**只改 CD 不改 SIP 系数一般错误**（ACSD 文档已有此结论，本卡提供式 (1) 的依据）。
5. **曲边**：`(u+f, v+g)` 是多项式映射，其像元的像是曲边；面积/交叠必须按曲线积分，不能把 SIP 后的四边形当直线多边形。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `coordinates.wcs` | [src/coordinates/tan_sip.cpp](../../../src/coordinates/tan_sip.cpp)：`sip_value`（式 2/3）、`sip_grad`、`sip_hessian`、`sip_value_iv`（区间版） | 式 (1)–(3) 的前向与导数；区间版是 ACSD 自研 | 不能声称区间版“证明了”多项式包含：那是区间原语的包含性 + 自研推导 |
| `coordinates.wcs` | [src/coordinates/wcs.cpp](../../../src/coordinates/wcs.cpp)（`mat2_invert`、Newton 逆、`kTan60` 域） | 式 (4) 的 `CD⁻¹` 与逆解 | 不能声称 AP/BP 是逆：源码用 Newton，AP/BP 若用也只作初值 |
| `coordinates.wcs` | [src/coordinates/footprint_provider.cpp](../../../src/coordinates/footprint_provider.cpp) | 曲边足迹的包含接口 | 原文无曲边面积公式 |
| `geometry.overlap` | [src/geometry/overlap.cpp](../../../src/geometry/overlap.cpp) | 用足迹做球面交叠 | SIP 不提供交叠算法 |

## 6. 不能声称的结论（红线）

- ❌「SIP 是 FITS 标准」：它是注册的**局部约定**（NASA 页明列 non-standard），且 Paper IV 最终版未采用 `-SIP` 形式。
- ❌「AP/BP 给出精确逆」：原文只说 fast inversion；Spitzer 自己保留两套系数正是因为一般情形下不互为精确逆。
- ❌「SIP 原文规定了 CRPIX 的 1-based 语义」：它只是沿用 FITS；写卡/写码时必须以 Paper I §2.1.4 为依据。
- ❌「A/B 的阶数越高越准」：原文无有效域与稳定性分析；高阶系数在像素尺度上可能病态。
- ❌「SIP 保证了单射」：原文完全没有单射讨论。

## 7. 与现有 ACSD 文档的差异

1. [OBSERVATION_COORDINATES.md](../../science/foundation/OBSERVATION_COORDINATES.md) §3 写「令 `r=CRPIX` 为 FITS 1-based，`u=p0+1−r`，SIP 多项式单位 pixel」——与原文式 (1) 及 Paper I §2.1.4 一致，**无矛盾**；本卡补上原文出处，便于审计。
2. 同文把「AP/BP 只作初值，不作误差证书」写成规范——比原文更严，属合理加强，不是对原文的曲解。
3. [02-LITERATURE-AND-PROVENANCE.md](../../workplans/v2-science-engineering/science/02-LITERATURE-AND-PROVENANCE.md) 的 SRC-SIP 行写「原文 A/B、AP/BP 与完整 WCS」待细化——本卡已细化。

## 8. 待办

- [ ] 独立复核者从式 (1) 出发，脱离源码重推 `D_new(u)=M⁻¹D_old(Mu)`，并核对 ACSD 复合实现。
- [ ] 若要引用 Paper IV 的 `-PLP`/`-SIP` 关系，需先取得 Paper IV 定稿（当前只有 2004-04-22 草案，NASA 页标注未获 IAU-FWG 批准）。
