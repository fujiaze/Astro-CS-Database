# 连续 WCS/SIP 到真实 HEALPix 叶的球面 Drizzle

**候选科学规范，待外部独立审核。** 本文给出已实现的小型求解方案，不把未来的 Green 公式设想当交付。观测量和坐标来自[观测定义](OBSERVATION_COORDINATES.md)，数值入口为 `实验/science-foundation/oracles/chart_overlap.py` 和独立的 `detector_independent.py`。生产 C++ 未修改。

## 1. 区域、被估计量和算子

原生像元 \(P_j=[x_j-.5,x_j+.5]\times[y_j-.5,y_j+.5]\)，pixfrac \(0<f\le1\) **先在 detector 中**收缩得到 Q_j(f)。连续 WCS W 在该矩形上单射，球面 drop \(D_j=W(Q_j)\)。目标 \(H_p\) 为 ICRS NESTED 真叶，不是四角大圆多边形。定义
\[
\Omega_j=\Omega(W(P_j)),\quad A_j=\Omega(D_j),\quad
a_{jp}=\Omega(D_j\cap H_p),\quad w_{jp}=a_{jp}/A_j.\tag{1}
\]
所有面积为 sr，输入 x_j 为像元积分量 L：
\[
 F_p=\sum_j w_{jp}x_j,\quad N_p=\sum_jw_{jp}\Omega_j,\quad
 B_p=F_p/N_p,\quad R_{pj}=w_{jp}/N_p.\tag{2}
\]
N=0 时为 unavailable。若输入已经是平均强度 b_j[L/sr]，则 `B_p=Σw_jp Ω_j b_j/N_p`；两种接口不能混用。附加源权重若采用，必须同时进入 F/N/R，并成为有效响应的一部分；基线单帧几何重建不以 Fisher SNR 改写面积权。

**DERIVED**：真叶分割球面且无重复计数时 `Σ_p a_jp=A_j`、`Σ_p F_p=Σ_j x_j`。常面亮度 `x_j=b0 Ω_j` 给 `B_p=b0`。这些都不推出 `Σ_p B_p Ω_leaf=Σ_j x_j`；覆盖归一、有限孔径和有效 PSF 是额外测量问题。`D_p=Σ_j a_jp` 是累计交叠、可大于叶面积，不是覆盖并集。`A_j=f²Ω_j` 在非线性 SIP/TAN 中一般不精确；两套面积均实际计算。

Drizzle 是像元重建核，不能恢复未观测子像元结构。几何准确只保证式(1)的区域正确，不能证明任何场景的 PSF、孔径通量或超分辨信息无偏。

## 2. 全 12 face 的连续等面积坐标

取 face f=0..11、\(s,t\in[0,1]\)，下表定向与标准 NESTED x 偶位/y 奇位相同：

| f | 0 1 2 3 | 4 5 6 7 | 8 9 10 11 |
|---|---|---|---|
| J_f | 2 2 2 2 | 3 3 3 3 | 4 4 4 4 |
| L_f | 1 3 5 7 | 0 2 4 6 | 1 3 5 7 |

令 \(r=J_f-s-t,d=s-t\)。逆映到球面：
\[
\begin{array}{lll}
0\le r<1:& z=1-r^2/3,& \phi={\pi\over4}(L_f+d/r),\\
1\le r\le3:& z=(2-r)2/3,& \phi={\pi\over4}(L_f+d),\\
3<r\le4:& z=-1+(4-r)^2/3,& \phi={\pi\over4}(L_f+d/(4-r)).
\end{array}\tag{3}
\]
\(n=(\sqrt{1-z^2}\cos\phi,\sqrt{1-z^2}\sin\phi,z)\)。r=0/4 为极点，用该 face 经度楔的极限；不能做 0/0。跨 r=1/3 的区间分别求值取并包。\(d/r\)、\(d/(4-r)\) 在所属 polar face 中总在 [-1,1]；这也是区间包围在 apex 的安全界。φ 不必先回绕，sin/cos 自然周期，面缝和同一极点的多种表达不产生重复面积。

等面积证明直接用 `dΩ=dφ dz`。赤道 `|det ∂(φ,z)/∂(s,t)|=π/3`；极区 `∂φ/∂d=π/(4r),|∂z/∂r|=2r/3,|∂(r,d)/∂(s,t)|=2`，乘积也为 π/3（南区同理）。因此
\[
d\Omega={\pi\over3}dsdt,\quad
H_{f,i,j}\leftrightarrow[i/N,(i+1)/N]\times[j/N,(j+1)/N],\quad
\Omega_H={\pi\over3N^2}.\tag{4}
\]
N=nside=2^order，`ipix=f N²+Σ(bit_i(i)2^(2i)+bit_i(j)2^(2i+1))`。uint64 索引在 order≤29 时容纳全叶；参考代码使用任意精度整数，生产接口先查范围。

这与 Calabretta–Roukema 的 HPX 投影等价：赤道 `x=φ,y=3πz/8`，极区 `σ=sqrt(3(1−|z|)),x=φ_c+σ(φ−φ_c),y=sgn(z)π(2−σ)/4`，`dΩ=8/(3π) dxdy`。本文用 square face 坐标直接积分，**不需要将源曲线跨所有 chart 中断显式拼成一个多边形**。

## 3. 正式小型算法 A：等面积目标域的包含型积分

核心将交叠写成
\[
a_{jp}={\pi\over3}\int_{cell_p}
1_{W(Q_j)}(n_f(s,t))\,dsdt .\tag{5}
\]
每个积分块不是用中心采样估面积，而是证明整个块在源区域内部/外部；无法证明者保留全块质量作为误差上界。这避开切触、多根、极窄 sliver 中“采样没看到”的假零。

### 3.1 源曲线的精确表示和单射条件

在 source TAN 中间平面（degree 单位便于保留头系数），Q 的四条边经 CD∘SIP 后都是单变量多项式 q(t)。将其幂系数 c_l 转为 Bernstein 系数
\[
 q(t)=\sum_{k=0}^{d} b_k {d\choose k}t^k(1-t)^{d-k},\quad
 b_k=\sum_{l=0}^k {{k\choose l}\over{d\choose l}}c_l .\tag{6}
\]
因 Bernstein 基非负且和为1，曲线落在控制点凸包；控制点区间的 min/max 给出整段曲线包围矩形。de Casteljau 在 t=1/2 分割，每步只作区间均值，保留数学包含性。没有边长60″、nside256或“中点恰好在线上”例外。

参考准入先以区间多项式导数证明 `sup_Q ||∇(A,B)||∞<1`。对任意两点，SIP 位移的 Lipschitz 常数 L<1，则 `||(I+D)(u)−(I+D)(v)||∞≥(1−L)||u−v||∞`，故全局单射；CD 非奇异，源边界是 Jordan 曲线。负 CDdet 只反转方向。此充分条件不能覆盖所有合法 WCS；超域时正式返回 `injectivity_unresolved`，另用可验证的域分片+全局无重叠证明后才能继续，不能仅凭局部 det 非零宣布单射。

### 3.2 整目标块的内外判定

对一个 face 四叉树块 U，用式(3)区间运算包围全部单位向量。令 `dc=n·c,de=n·east,dn=n·north`，先以 source TAN bbox 的四个锥半空间排除不可能相交块；`dc.upper<=0` 在 source TAN 对面，必在外部。`dc` 跨0而无法排除时继续细分，绝不穿过 TAN horizon 盲除。

`dc.lower>0` 时，以区间 `(de/dc,dn/dc)*180/π` 包围目标在 source TAN 平面的像 B。B 与源整体 bbox 分离则 outside。否则依次分割 source 四条 Bernstein 曲线，尝试将每段控制凸包矩形与 B 证明分离；任何争议不作 inside/outside 判定。

若全部曲段的包围矩形都与 B 分离，曲线可在各自凸包中连续变形成首尾直弦，**整个同伦不穿 B**，所以真实边界和得到的折线对 B 内任一点 winding number 相同。用区间射线相交谓词计算折线 winding；比较不确定则换射线或继续细分。B 连通且与真实边界不交，故整个目标块内外一致。这不是把真实源曲线永久替换为四角，直弦只在已证明不会改变该块内外关系的局部证明中使用。

### 3.3 候选、积分与停止

```text
queue = 12 个完整 face；accepted[p]=0
递归达到正式目标 order：
    proved_outside -> 丢弃，漏选面积严格为0
    proved_inside  -> 展开实际被覆盖的正式叶，直接累加解析面积
    undecided      -> 四分；没有角点bbox候选捷径
达到目标order后：
    对 proved_inside 子块累加解析质量到相应叶下界 Lp
    对 undecided 子块保留质量到该叶未决上界 Up
    优先分裂质量最大的未决块
    直到 ΣUp <= 用户全drop绝对面积预算，或资源预算耗尽
return 每叶 [Lp,Lp+Up]、全drop上下界、ΣUp、实际工作数与停止原因
```

子块面积严格是 `Ω_H 4^(order−depth)`，无需 quadrature。真曲边的切触和零面积接触留在未决块，随细分面积趋于0；真实正 sliver 一定被 inside 或 undecided 覆盖。每个正式叶只拥有它的 dyadic 子块，分面边界是零测集，不会双算。预算用**所有未决质量之和**，不是前后两次数值差。约束 max_nodes、max_depth 或无法分离时返回 `budget_exhausted/precision_exhausted` 及仍有效的上下界，不能改为成功或归零。

**DERIVED**：由域完整分割和内外包含可得每个真 a 落在报告区间，且全drop误差不超过 ΣUp。该证明依赖区间原语实际包含其数学运算；`mpmath.iv` 原型采用25位十进制（独立对拍可用30位）、输出 binary64 端点向外扩一 ulp。质量累加使用精确有理数 dyadic 账本，不能只在普通float总和末端扩一ulp。参考最大depth=48，保证面内dyadic端点由binary64精确表示。所有根/粗叶展开都进入同一计数队列；预算不足以展开到正式叶时返回 `coarse_unresolved_candidates` 的位置与质量，不能省掉粗块。独立审核曾发现并已修正上述累计丢失和初始化绕预算问题，原反例保留。源码和有限数值对照不是对 Python/区间库的形式化认证。生产 C++ 必须使用有保证的向外舍入区间原语；不能把编译器 fast-math 的普通 double 同公式称为相同证书。

## 4. 独立算法 B：探测器域积分

不复用算法A的 face 公式、Bernstein 源边界或 winding，直接积分
\[
g(u,v)={|\det C_{rad}|\,|\det(I+\nabla D(u,v))|
\over(1+\xi(u,v)^2+\eta(u,v)^2)^{3/2}},\quad
a_{jp}=\int_Q 1_{H_p}(W(u,v))g(u,v)dudv.\tag{7}
\]
这来自归一化 TAN 向量的球面 Jacobian；整体旋转不改面积。P、Q 分别积分给 Ω_j、A_j。

`detector_independent.py` 独立实现区间多项式、导数、g和显式ICRS TAN向量，以**正向 sky→NESTED integer-floor** 包围整个子矩形的可能成员，不调用算法A的逆face公式、Bernstein边或winding。令 `tt=φ/(π/2)` 分支归一到[0,4]，逐区间分支计算：

```text
赤道 |z|<=2/3:
    jp=floor(N*(.5+tt−.75z)), jm=floor(N*(.5+tt+.75z))
    ifp=jp//N, ifm=jm//N
    face=(ifp|4) if ifp==ifm else ifp if ifp<ifm else ifm+8
    ix=jm mod N, iy=N−1−(jp mod N)
极区:
    sector=floor(tt), tp=tt−sector, tmp=N*sqrt(3*(1−|z|))
    jp=min(N−1,floor(tp*tmp)), jm=min(N−1,floor((1−tp)*tmp))
    north: face=sector, ix=N−jm−1, iy=N−jp−1
    south: face=sector+8, ix=jp, iy=jm
```

每个floor变量枚举区间端点间所有整数并取笛卡尔积，加入所有可能face/ix/iy；丢失变量相关性只会增加候选，不能漏真叶。跨 `z=±2/3`、tt整数和RAwrap分别取并集；极点不给φ唯一值，而取整圈保守候选。用未归一向量(X,Y,Z)、范数L，polar `tmp=N sqrt(3)*sqrt(X²+Y²)/sqrt(L*(L+|Z|))` 等价原式，避免 `1−|z|` 近极消去。区间atan2按已知象限分段；若无可分象限则保留完整经度区间。

候选唯一时整个块同成员，积分g；多个候选则各自保留可能上界再细分。候选笛卡尔积超过工作预算时返回all-sky未决域及质量，不能把“暂不枚举”当空支持。最终所有面积和由区间累加后向外转换。其证明依赖同样的区间原语包含性，外部Astropy/WCSLIB/HEALPix仅用于独立点成员、坐标和MC对照。

**已否定的旧路线**：第一版以保守TAN球冠+外部inclusive cone唯一叶作包含证明。虽然TAN映射对平面距离的Lipschitz≤1且球冠本身正确，实际Astropy-HEALPix1.0.3在高阶近极点有漏叶反例，导致B逐叶假包围、总面积仍闭合。旧cone模式全部撤销作为bound证据的资格，只保留复现。不能通过增加经验angle margin或调宽误差阈值掩盖该失效；具体输入与修正后的双算法对拍见[独立证据](INDEPENDENT_ORACLES.md)。

已实现的积分选项包括自然区间 `|Q|[gmin,gmax]` 和有界乘积中点：
\[
|\int_U g-|U|g(mid)|\le{|U|\over24}
\{w_u^2\sup_U|g_{uu}|+w_v^2\sup_U|g_{vv}|\}.\tag{8}
\]
逐轴应用一维中点余项即得式(8)，Hessian 用区间自动微分/解析组合求界。它控制实际余项，不以两次求积一致代替误差。先前自然区间在预算内未收敛的结果保留，最终高阶路线与其交叉比较。

另有 iid detector MC，`X_p=g(U)|Q|1_H`，使用已知 g 上界 M 的 Hoeffding simultaneous bound `M sqrt(log(2K/δ)/(2n))`。全体 K=12N² 叶用 union bound，无命中也保留这个正上界。它是置信区间，不是确定界；Sobol/QMC 不能直接套 iid Hoeffding。

## 5. API、计算预算和误差传播

```cpp
struct AreaInterval { double lower_sr, upper_sr; };
struct Overlap { NestedLeaf leaf; AreaInterval area; };
struct GeometryBudget { double total_abs_sr, relative_resolved;
  uint64_t max_work; uint32_t max_depth; };
struct CurvedFootprint { WcsModel forward; PixelCenter0 center;
  double pixfrac; DomainCertificate injectivity; };
struct DropSolution { AreaInterval omega_source, omega_drop;
  vector<Overlap> sorted_unique; double unresolved_mass_upper_sr;
  GeometryStatus status; NumericalMethod method; };
compute_overlaps(CurvedFootprint, TargetOrder, GeometryBudget) -> DropSolution;
build_surface_operator(SourceSignals[L], DropSolutions) -> SparseRows[sr^-1];
```

Python A/B 的公共 `solve_overlap(header,center_xy0,pixfrac,nside,...)` 返回每叶 bounds 和停止原因。上层只可按面积区间传播非负比值，例如 `w∈[aL/AU,aU/AL]`，要求 AL>0；N 也包含 Ω 的误差。若 `NL<=0` 不发布有保证的归一值。对 signed x，用区间线性组合而非直接除两个中点。真实负信号完全合法。

当输入面亮度在 [bmin,bmax]、真实未归一非负系数 \(c_j=a_{jp}\Omega_j/A_j\)、计算值 \(\tilde c_j\) 时，
\[
 |B-\tilde B|\le(b_{max}-b_{min})
 {\sum_j|c_j-\tilde c_j|\over\min(\sum c_j,\sum\tilde c_j)}.\tag{9}
\]
由两组归一权的总变差界可得；它说明应从科学亮度/测光预算反推面积精度。常值天空差为零不证明几何正确。几何误差是数值/模型界，不自动是额外 iid variance。

设 K 为实际接触叶数、q 为已处理 target 四叉树节点数、b 为缓存 source 曲段数、d 为 SIP 次数。参考实现保守上界 O(q b d²+K)，优先队列 O(q log q)，内存 O(q+b+K)，实际曲线凸包排除通常访问少数段。规则光滑边界、目标 chart 中长度 L 时，纯面积包围在边长 h 下未决量 O(Lh)，边界节点 O(L/h)，故达到面积 ε 的最坏主项 O(L²/ε)；apex 映射非光滑时不套此常数，直接用实际 ΣUp 停止。

**工程裁决**：正式计算方案已确定为包含型 target-domain 积分，带独立 detector-domain fallback/核查；不以角长或高 nside 作为四角快路径。完整 inside/outside 块的解析面积、同一WCS相邻像元共享边的 Bernstein 树、分面常量和 SIMD 区间批处理可以直接实施；这些只缓存证明，不改变数学定义。Python 是独立小型 reference，不是可直接用于亿级像元的吞吐实现。本文不承诺生产性能，真实 C++ 吞吐/内存仍需 benchmark；若误差要求下计算代价过大，正确行为是带 bounds 的未收敛，而非换成无界四角结果。更高阶 boundary/cut-cell 加速须与本规范同真值对照后采用，不能预先宣称已完成。

## 6. 正例、反例及已证精度

实跑结果详见[证据](INDEPENDENT_ORACLES.md)，全量输入头在 `geometry_experiments.json`。

| 构型/判断 | 结果与范围 |
|---|---|
| 全12 face连续坐标，N=1,4,256,2¹⁸，每阶60点 | 与外部 Astropy-HEALPix 最大向量角差约1.45e−15 rad；有限测试非全域形式证明 |
| 12 face中心带4阶非对称 SIP、另双apex/转折/RAwrap/细阶，共17例 | A均按其全drop绝对预算收敛，MC没有已命中叶被候选漏掉；定理仍由区间覆盖而非MC支持 |
| 无SIP 2° TAN方形 | 独立解析 `4atan(ab/sqrt(1+a²+b²))` 落在总面积区间 |
| 北极apex真实叶 vs 四角测地叶，N=256/8192 | 四角相对误差约−9.96812%/−9.96836814%；高nside不消除该局部相对误差 |
| apex中央经度楔 `|φ−φc|≤π/8` | 用球面 `dφ dz` 独立积分得真叶的2/3，非整叶面积闭合自证 |
| SIP `ξ=u,η=v+.12u²` 与 `η<0` | 真平面交叠 .49，四角 .47；单射、总面积正确也不能省略短边曲率 |

直接二套求解器最终8例全部按实际预算收敛，逐叶和总面积区间均相交；其中包括旧外部cone已被证伪的N=2¹⁸近极点构型。B另20例在同0.5%全drop绝对预算策略下最终均达到充分停止判据（11例初轮、9例保持原失败后追加预算），原始状态与源码加载快照均保留。tiny sliver、预算/精度耗尽及候选溢出记录由 evidence 文件给出。面积上界只涵盖当前 WCS 的数值几何，不含真实 WCS 拟合偏差。WCS 参数共享误差可通过扰动/Jacobian/联合 MC 传播，边界拓扑变化时不能强用单一线性导数。

## 7. 原始参考与物理风险

1. [Fruchter & Hook 2002，PASP 114,144，§2与§7](https://arxiv.org/html/astro-ph/9808087v2)，DOI 10.1086/338393：采用像元收缩、线性分配和相关噪声；原论文不提供本文真实 HEALPix/SIP 相交器。
2. [Calabretta & Roukema 2007，§2.1–2.4、式(3)–(10)](https://arxiv.org/html/astro-ph/0412607)，DOI 10.1111/j.1365-2966.2007.12297.x：采用 HPX 等面积 chart。本文另外推导全12face表与包含型积分。
3. [Górski et al. 2005](https://arxiv.org/abs/astro-ph/0409513)：NESTED 和真实叶；[Astropy-HEALPix API](https://astropy-healpix.readthedocs.io/en/latest/api/astropy_healpix.HEALPix.html)作为独立坐标、成员和查询参照。
4. [Farin，Curves and Surfaces for CAGD，Ch.4.1–4.6](https://www.farinhansford.com/books/cagd/toc.html)：Bernstein 凸包、de Casteljau 与细分；成熟几何算法用于多项式源曲线。
5. [Moore, Kearfott & Cloud 2009，Ch.1–4、8](https://epubs.siam.org/doi/10.1137/1.9780898717716.fm)：包含型运算和积分；采用显式上下界，非采样收敛口号。

**ASSUMPTION**：固定WCS/像元核的观测模型充分；pixfrac策略适合实际采样。**REQUIRES_REAL_DATA**：HST/M42 的 WCS/PSF/孔径、弱扩展结构、细叶相关噪声与实际吞吐；所有真实物理效果不能由面积区间代替。数学解算与实际科学产品验收分开。
