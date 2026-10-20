# 独立 detector 域真叶交叠 Oracle

本代码独立于 ACSD 的生产几何与本轮 chart 求解器。没有读取或调用它们的边界、角点、面积、候选或归一化实现。共享 FITS header 与交叠的数学定义是允许的输入。主入口为 `solve_overlap(header_or_WCS, center_xy0, pixfrac, nside, ...)`，fixture 为 `fixtures()` 返回的字典列表；`center_xy0` 是 Astropy `origin=0` 的连续像元中心坐标。所有面积均为 sr。

## 数学定义与独立推导

令 `Q=[x0,x1]×[y0,y1]` 为 detector 收缩正方形，`u=x+1-CRPIX1`、`v=y+1-CRPIX2`，SIP 映射 `F=(u+A(u,v),v+B(u,v))`，`(ξ,η)=CD_rad F`。TAN 的单位向量在其本地正交坐标系为

\[
 n(\xi,\eta)=\frac{(1,\xi,\eta)}{\sqrt{1+\xi^2+\eta^2}}.
\]

对 `ξ,η` 求偏导取叉积，得 `|n_ξ×n_η|=(1+ξ²+η²)^(-3/2)`。旋转到 FITS 天球坐标不改变球面面积。因此

\[
 g=\frac{|\det CD_{rad}|\,|\det(I+\nabla(A,B))|}{(1+\xi^2+\eta^2)^{3/2}},\qquad
 a_p=\int_Q 1_{H_p}(W(x,y))g(x,y)\,dx\,dy.
\]

这个式子只有在 `W` 在 Q 单射时才表示集合交叠面积。代码不是只检查 Jacobian 非零：以中心导数 `J0` 作预条件，区间证明

\[
 \kappa=\sup_Q\|J_0^{-1}J_F-I\|_F<1.
\]

沿 Q 内连接两点的线段积分即得 `|J0^-1(F(p)-F(q))|≥(1-κ)|p-q|`，因而全局单射。不能证明的输入明确拒绝；这是一项充分条件，拒绝不表示所有此类输入都真的折叠。负例 `F_x=x+2x²,F_y=y` 在 `x=-1/2` 与 `x=0` 重合，确实非单射并被拒绝。

## 连续曲边与每叶包围

每个自适应 detector 子矩形 R，以 `mpmath.iv` 的 30 位区间求完整 SIP 多项式 `ξ(R),η(R)`，再显式旋转到 ICRS 单位向量。令 α、δ 为 CRVAL，`r=LONPOLE−180°`，`e`、`n` 为天球东、北正交基，则

\[
 v=v_0+\xi(e\cos r-n\sin r)+\eta(e\sin r+n\cos r),\quad W=v/\sqrt{1+\xi^2+\eta^2}.
\]

默认 `candidate_mode='interval_floor'`。取 `z=W_z`、`tt=φ/(π/2)`，按官方 HEALPix `loc2pix` NESTED 正向 floor 算法，区间枚举所有可能的整数与分支。赤道区 `|z|≤2/3`：

\[
 jp=\lfloor N(\tfrac12+tt-\tfrac34z)\rfloor,\quad
 jm=\lfloor N(\tfrac12+tt+\tfrac34z)\rfloor.
\]

`ifp=jp//N,ifm=jm//N`；相等时 face=`ifp|4`，`ifp<ifm` 时 face=`ifp`，否则 face=`ifm+8`。`ix=jm mod N, iy=N−1−(jp mod N)`。极区按四象限分片，`ntt=floor(tt), tp=tt−ntt`，

\[
 t=N\sqrt{3(1-|z|)}
 =\frac{N\sqrt3\sqrt{v_x^2+v_y^2}}{\sqrt{|v|(|v|+|v_z|)}}.
\]

右式避免接近极点的相消；`jp=floor(tp*t),jm=floor((1−tp)*t)`，分别 clamp 到 `[0,N−1]`。北极 `(face,ix,iy)=(ntt,N−jm−1,N−jp−1)`，南极为 `(ntt+8,jp,jm)`。NESTED 位交错由 ix 占偶位、iy 占奇位独立实现。

跨 z 转折、象限、RA wrap 的区间取候选并集，而非给指示函数取平均。候选唯一时整个 R 归该叶；否则各叶保留 R 面积上界并继续细分。组合过多时返回显式全 sky 未决质量，每一已列叶 upper 加该质量，未列叶的区间也是 `[0,该质量]`；不能把空候选列表解释为无覆盖。该方法不调用逆 face→sky chart，也不调用外部 cone 作证明。

对已归叶的矩形，先计算自然区间积分 `|R|g(R)`。必要时以区间解析二阶导数计算乘积中点求积包围：

\[
 \left|\int_Rg-|R|g(c)\right|
 \le\frac{|R|}{24}\left(\sup_R|g_{xx}|w_x^2+\sup_R|g_{yy}|w_y^2\right).
\]

证明为先沿 x 应用一维中点余项、再沿 y 应用同一余项；不需要丢弃未知交叉导数。代码直接解析求导；对 `D=det J_F,r=1+ξ²+η²`，使用

\[
 h=r^{-3/2},\quad h_z=-\tfrac32r^{-5/2}r_z,
 \quad h_{zz}=\tfrac{15}{4}r^{-7/2}r_z^2-\tfrac32r^{-5/2}r_{zz},
\]

\[
 g_{zz}=|\det CD|\operatorname{sign}(D)
 (D_{zz}h+2D_zh_z+Dh_{zz}).
\]

单射证书保证 D 符号固定；包围绝对值时无需知道其正负。只需 SIP 至三阶导数。实际面积取自然区间与有界中点区间的交集。公式并未依赖两次细分结果相近的经验判据。

优先四分当前误差最大的矩形。若全部归叶矩形的面积区间宽度之和为 `Eint`，未决矩形面积上界之和为 `Eboundary`，则每一真实叶面积区间的宽度均不超过 `Eint+Eboundary`。这提供绝对误差控制。近零 sliver 的相对精度不由这一总面积预算保证，必须增加预算或直接以该叶绝对上界报告。

`areas[p].mid_sr` 仅是该叶区间中点。不同叶区间有相关的未决质量，所有 midpoint 不保证组成一致分割，禁止用这些 midpoint 归一化来制造守恒。`total_area` 是独立的 `∫Q g` 包围；`pixfrac=1` 与其他 pixfrac 应分别调用求 `Ω` 和 `A`，不假定 `A=f²Ω`。

设评估 N 个矩形、各次区间 floor 枚举 K_i 个候选组合；除固定阶多项式与区间代价，时间 `O(N log N + ΣK_i)`，内存 `O(N+ΣK_i)` 的保守上界。候选组合设显式工作预算，超预算保留全 sky 未决质量；Oracle 不作为生产吞吐设计。

## 独立数值对照

无 SIP 的线性 TAN 把 detector 直边严格映为大圆边。`tan_rectangle_area_mp` 用 70 位精度、独立球面三角形 `2 atan2(det,1+a·b+b·c+c·a)` 求真实矩形总面积。这不是错误地把一般 SIP 曲边替换为大圆。

`iid_reference` 直接从 Q 均匀独立抽样，用 Astropy 全 WCS、外部真实 HEALPix 成员及 g 求面积。若单次有界随机变量 `Yp=|Q|g(U)1_Hp` 上界 M，N 次采样的同时置信半宽为

\[
 M\sqrt{\frac{\log(2(12nside^2+1)/\delta)}{2N}}.
\]

union bound 覆盖全部叶而非只覆盖命中叶，因此不引入按数据选择叶的偏差。每个 fixture 的置信失败概率单独为 δ，跨全部 fixture 的概率不能仍宣称 δ。Monte Carlo 零命中不能证明零交叠；`tiny_boundary_sliver` 特意保留有外部内部点成员证据但无随机命中的叶。

另以 Astropy 天球单位向量的独立有限差分叉积对照 g。它是数值收敛检查，不能替代误差证书。

## 证明、数值包围与物理限制

- 上述 Jacobian、单射条件、正向 floor 区间扩张、积分余项、叶包围逻辑和 Hoeffding 不等式是数学结论。
- `mpmath.iv` 给输入为规范化二进制浮点 WCS 系数的多项式与积分区间。PC/CDELT 路径保留两个系数的实数乘积；TAN 解析对照使用 70 位有效数。
- 默认算法以 mpmath.iv 算术与超越函数包围性质为数值原语假设，对规范化 ICRS TAN/SIP 系数给区间包围。Astropy/WCSLIB 和外部 HEALPix 只用于独立数值验证。真实 WCS 参数估计误差仍需另外传播，本 Oracle 不估计它。
- `budget_exhausted`、`resolution_exhausted` 均保留当前全部区间，不输出伪成功。自然区间首轮、提高预算以及算法改进后的证据分文件保存，不能覆盖失败后改口径。
- 所有 fixture 为真值定义清晰的合成观测。没有验证 HST/M42 的真实畸变拟合、PSF、探测器响应、颜色依赖或生产吞吐；不宣称科学设计已独立正式验收。

## 外部 cone 旧结论的撤销

**旧 `legacy_external_cone` 的交叠区间已被反例推翻，不能用作有效误差证书。** 旧文件、旧源码 snapshot 与失败 cross 对拍全部保留；`candidate_mode='legacy_external_cone'` 可复算，结果明确带 `invalid_for_enclosure`。旧“20/20 与 Monte Carlo 置信区间相交”等宽置信对照没有发现这一系统偏移，不能反向覆盖反例。

明确反例：nside=2^18、header CRVAL=(359.99999°,89.9°)、1 arcsec/pixel 的 TAN+SIP，detector R=`(.175,.35,−.175,0)`。包围球冠 radius=`8.187563679155838e−7 rad`，外部 cone 只返回叶 274877380095。R 角点 `(.35,0)` 的外部真实成员为 274877380437；用 70 位 ICRS 向量计算，该点到球冠中心的距离仅为 radius 的 `0.7576461802162086`，确实在球冠内部。新 IV 正向候选同时包含这两个叶。见 `detector_interval_validation.json`。这证明旧候选存在真实漏叶，不能通过增大节点预算、归一化或放宽阈值修复。

旧外部 cone 的 .005/52001 高阶例也保留为失败记录；其未决区域缩小缓慢不能单独证明某个数值精度地板。关键撤销依据是上述漏叶反例及交叉对拍四叶全部无交。

## 新算法的独立数值验证

最终 `evidence/detector_independent.json` 含 20 构型：12 base faces、RA wrap、南北极、赤道/极区转折、pixfrac=.37、四叶交点、零随机命中 sliver，以及 nside=2^18 的 1 arcsec/pixel 小 drop。每例 200000 个 iid 样本；单例 δ=1e-5，全部 20 例 union bound 至多 2e-4。Python 3.12.2、NumPy 1.26.4、Astropy 7.1.0、astropy-healpix 1.0.3、mpmath 1.3.0；环境以 JSON 实录为准。

| 实际检查 | 结果及含义 |
|---|---|
| 独立 Astropy/WCSLIB + 外部成员 iid 置信区间，与交叠上下界相交 | 20/20；不是所有叶精确面积的解析证明 |
| 无 SIP 的 70 位 TAN 解析总面积落入区间 | 14/14；12 faces、四叶交点、sliver |
| 12 faces × nside={1,8,2^18} × 8个真实叶内部点 | 288点，正向IV候选对外部成员零遗漏 |
| 精确南北极、多个CRVAL纬度、LONPOLE={0,90,180,270} | 60个单位向量，对Astropy最大角差7.02e−16 rad |
| 已知折叠、pixfrac=0、非法 nside 被拒绝 | 3/3 |
| g 与 Astropy 天球单位向量有限差分最大相对差 | 1.0967118991e-8；仅是收敛检查 |

最终区间预算与时间以新的 `detector_independent.json`、追加的 `detector_interval_refinements.json` 为准。原 .005/40001 预算超限构型继续以相同阈值追加80001节点，原状态保留。`detector_per_leaf_budget_audit.py` 不改求解器状态；逐叶宽度与全局充分判据分开报告。

sliver 例中主叶为 304，严格位于 detector 内部的两个点由外部库分别归叶 282、293；200000 个随机点均未命中这两个叶。求解器仍保留候选的正面积上界，未把零命中当作面积为零。

`detector_dual_area_check.py` 独立求同一 SIP 像元的 Ω 与 f=.37 时的 A：

\[
 A/\Omega\in[0.13686678389684193,\ 0.13686950436340525],
 \qquad f^2=0.1369.
\]

双面积脚本也以新默认 IV forward 重跑；区间排除 f²，证明在这个明确的连续 WCS/SIP 模型中不能用 `A=f²Ω` 替代独立面积计算。精确新端点与节点数见 `detector_dual_area.json`。

可重现命令（从仓库根目录运行）：

```text
python 实验/science-foundation/oracles/detector_independent.py --output 实验/science-foundation/evidence/detector_independent.json --relative-tolerance .005 --max-nodes 40001 --samples 200000
python 实验/science-foundation/oracles/detector_interval_validation.py
python 实验/science-foundation/oracles/detector_per_leaf_budget_audit.py
python 实验/science-foundation/oracles/detector_dual_area_check.py
```

自然区间 5001 节点首轮、乘积中点 14001 节点中间轮、26001/52001节点及 pilot 的 cone 路线原始证据另存，统一撤销交叠包围身份；不删除失败、不改宽断言。

参考接口：[HEALPix 官方 loc2pix NEST](https://healpix.sourceforge.io/html/Healpix_cxx/healpix__base_8cc_source.html)、[Astropy WCS](https://docs.astropy.org/en/stable/wcs/)、[astropy-healpix](https://astropy-healpix.readthedocs.io/en/latest/api/astropy_healpix.HEALPix.html)、[mpmath 区间上下文](https://mpmath.org/doc/current/contexts.html#arbitrary-precision-interval-arithmetic-iv)。
