# 连续 WCS/SIP 到真实 HEALPix 叶的球面 Drizzle

**候选科学规范，待外部独立审核。** 本文给出已实现的小型求解方案，不把未来的 Green 公式设想当交付。观测量和坐标来自[观测定义](OBSERVATION_COORDINATES.md)，数值入口为 `实验/science-foundation/oracles/chart_overlap.py` 和独立的 `detector_independent.py`。生产 C++ 未修改。

## 1. 区域、被估计量和算子

普通原生矩形像元 \(P_j=[x_j-.5,x_j+.5]\times[y_j-.5,y_j+.5]\)。一般入口的 P_j 来自 `DetectorFootprint`，不能从抽样间隔猜收光面积：Bayer 子格在 channel 坐标的实际半宽为 .25，sum-bin 是输入足迹并集，缺像元可留洞。以下矩形参考实现消费显式矩形分量；其他形状必须由同一 `FootprintProvider` 提供包含/积分能力或返回 unsupported。pixfrac \(0<f\le1\) **先在 detector 中**收缩得到 Q_j(f)。连续 WCS W 在该矩形上单射，球面 drop \(D_j=W(Q_j)\)。目标 \(H_p\) 为 ICRS NESTED 真叶，不是四角大圆多边形。定义
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

## 5. 科学量驱动的面积预算和公共接口

接口的对象、所有权、生命周期、坐标和错误总表由[观测合同 §7](OBSERVATION_COORDINATES.md)唯一拥有。几何消费不可变 `WcsModel/DetectorFootprint/HealpixLeaf`，返回 `SphericalAreaInterval` 与同一 primitive 的约束身份；`OverlapContribution` 包含 a、A、Ω，不能只返回无误差的权。`GeometryBudget` 同时携带请求的 B/积分/Cov 泛函及单位、逐叶面积绝对限额、支持要求、work/depth/bytes 上限。资源预算不是科学精度的替代品。

对包含盒 `a∈[aL,aU], A∈[AL,AU], Ω∈[ΩL,ΩU]`，AL、ΩL>0：
\[
w\in[a_L/A_U,\min(1,a_U/A_L)],\quad
F=\sum wx,\quad N=\sum w\Omega.\tag{9}
\]
负 x 的乘积取全部端点 min/max。N 的同一身份、每列 A 的身份及 `Σ_p a_jp=A_j` 保留在算子约束中。盒放宽可保守，但不能把各叶误差解释为独立随机量。

**支持与信号分开**：NU=0 为 `no_support`；NL=0<NU 为 `support_unresolved`，不发布有保证的 B；0<NL<Nmin 为 `weak_support`，其中 Nmin 是请求的归一条件阈值；其余是正支持。科学容差是否满足另报。正支持且 F=0 是合法真零，绝不改成 NaN。极弱支持可以有很准确的 B；小面积本身也不能证明 B 的存在。有效覆盖并集 S 还需对证明块作 OR；`max aL≤S≤min(ΩH,ΣaU)` 只是粗界，N、Σa 均不能叫覆盖率。

### 5.1 避免 signed numerator 与分母依赖造成虚假结果

NL>0 时，给 q 定义
\[
G_+(q)=\max_{w,\Omega}\sum_jw_j(x_j-q\Omega_j),\quad
G_-(q)=\min_{w,\Omega}\sum_jw_j(x_j-q\Omega_j).
\tag{10}
\]
`Bmax≤q iff G+≤0`，`Bmin≥q iff G−≥0`。q≥0 时 G+ 取 ΩL、G− 取 ΩU；q<0 交换；再按残差正负取 w 端点。一次 O(m)，对单调根二分 O(m log(initial_width/εB))。区间符号无法决定时提升精度或返回未决。独立有理数参考用有限顶点 Dinkelbach 更新并与三组64顶点穷举逐分数相等；不承诺其最坏迭代数。将 x 换 e_j 即得同一 R_j 的包含界，不能独立归一 R 的中点来伪造精度。

积分输入 x 精确与输入平均强度 b 是两种身份。只有明确声明 `x=bΩ`，才可设 c=wΩ 后求 `Σc b/Σc`，从而常场 b0 的区间严格是 [b0,b0]。独立精确 x 配不精确 Ω 不能暗中利用这项消去。

### 5.2 由目标误差反推 primitive 请求

取包含盒中心 w0、Ω0，令 N0=Σw0Ω0、B0=Σw0x/N0，ew=max|w−w0|、eΩ=max|Ω−Ω0|。直接展开得
\[
B-B_0={\sum_j(w_j-w_{0j})(x_j-B_0\Omega_{0j})
-B_0\sum_jw_j(\Omega_j-\Omega_{0j})\over N}.
\tag{11}
\]
所以误差≤`[Σew|x−B0Ω0|+|B0|ΣwU eΩ]/NL`。结合分母损失得到充分停止判据
\[
\sum_j e_{wj}(|x_j-B_0\Omega_{0j}|+\epsilon_B\Omega_{0j})+
\sum_j w_{Uj}e_{\Omega j}(|B_0|+\epsilon_B)
\le\epsilon_B N_0,\qquad N_L>0.\tag{12}
\]
这比统一面积百分比直接体现亮暗对比；常场中残差消失。仅 A、Ω 精确时选 λj≥0、Σλj<1，逐项请求
\[
\epsilon_{a,j}={\lambda_j\epsilon_B N_0 A_j\over
|x_j-B_0\Omega_j|+\epsilon_B\Omega_j}.\tag{13}
\]
默认 λ=1/(2m) 给分母和其他数值项留余量。A 也不精确时可规划 `ew≤εa/AL+|a0|εA/(AL A0)`，但停止必须重新计算式(10)/(12)的完整界。真实 Ω 的模型拟合误差与当前 WCS 的求积误差分账。

```text
request(science_functionals, source signals, initial area enclosures):
  构造同一primitive约束、w/F/N/B/R区间及支持状态
  若所有请求泛函误差界达标：返回 science_bounded，可含有界未决小片
  若NL=0：继续对支持证据细化；不能用“中点很小”发布B
  按式(12)左边的影响量分配per-leaf绝对预算
  优先细化影响最大的a/A/Ω；重算B、孔径、Cov及下游solve的界
  资源/浮点精度耗尽：保留原包含区间，返回 science_unconverged
```

分配积分量 `|δF|≤Σ|x|ew`。有符号孔径 `I=tᵀB` 有 `|δI|≤Σ|t_p|E_Bp`；若请求支持积分 B·S，S 必须一起区间传播。完整分割的 `Σp Fp=Σj xj` 是精确列守恒关系，不应用逐叶宽度相加冒充其实际总误差。Export 的支持均值/积分都使用该规则，不对缺覆盖区域作隐含外推。

### 5.3 几何数值界、噪声和下游条件数

固定已知 C，中心 R0、逐元 `|R−R0|≤E`，令 ep=E[p,:]、rp=R0[p,:]：
\[
|\delta(RCR^T)_{pq}|\le e_p^T|Cr_q|+e_q^T|Cr_p|+e_p^T|C|e_q.
\tag{14}
\]
对孔径 t 令 u0=R0ᵀt、δ=Eᵀ|t|，方差误差≤`2δᵀ|Cu0|+δᵀ|C|δ`。若 C=ΣLkLkᵀ，可用 `ηk=|| |Lk|ᵀδ ||2`，上界为 `Σ[2||Lkᵀu0||2 ηk+ηk²]`，按对角/局部/共享因子 action 流式算，不产生亿叶稠密矩阵。黑盒 C action 没有绝对系数结构时须另外提供可信谱范数界，不能用带抵消的 C action 代替 |C| action。

这些是**确定数值误差**，不加到 variance 冒充 iid 噪声。同一源分到多个叶仍是同一随机变量。SNR、sky、coadd、Export 都沿实际复合算子传播。固定 P/权重/选择时可继续对 L=P R 用式(14)；几何改变 sky/GLS 解时还要包围求解本身。[统计规范 §2](STATISTICS_SKY_COADD.md)给出 `|| |I−GM0|+|G|EM ||∞<1` 的线性求解余项证书，失败为 `geometry_solve_unresolved`；不能以冻结中心权、改变 gauge、加 ridge 或略去选择分支来声称理想重算流程已达标。

## 6. C++ 主路线：有余项界的 affine-strip cut cell

`round2_affine.cpp` 是已编译实测的独立微型实现。它不翻译 A 的像素内外四叉树，也不将真实叶改为四角多边形。主路线在 detector 子矩形 U 上，用**连续正向 HPX 坐标函数**的有界局部仿射模型，生成真实交叠的内/外凸多边形。积分直接计算 cut cell，而不必把其整块质量都留到很深层。

### 6.1 连续 chart 与可证明的局部余项

赤道区 `tt=φ/(π/2)`，设
\[
a=N(1/2+tt-3z/4),\qquad b=N(1/2+tt+3z/4).
\tag{15}
\]
整数格 `[j,j+1]×[k,k+1]` 经算法B同一整数规则映到真叶；允许展开的 longitude sheet，整圈共同平移4N后归一，故 RA=0 不需要数据索引补缝。极区每个 sector σ 用 `tp=tt−σ`、`τ=N sqrt(3(1−|z|))`：`a=tp τ,b=(1−tp)τ`，仅保留 0≤a,b≤N，并额外夹北/南的 z 区间。跨 sector 用各连续延拓坐标分别裁剪，a、b≥0 自己排除错误扇区；跨 z=±2/3 分区求和，边界为零测集。τ 使用§4稳定的未归一向量式。

在 U 的可表示中心 c 取区间值 f(c)、导数，选其代表数 f0,J0，用一阶区间自动微分给 `∇f(U)`。均值公式直接给
\[
f(u)=f_0+J_0(u-c)+e(u),\quad
|e|\le \operatorname{mag}(f(c)-f_0)+
\sum_{k=x,y}\operatorname{mag}(\partial_k f(U)-J_{0k})h_k.
\tag{16}
\]
其中 `hk≥max(c_k−UkL,UkU−c_k)` **逐运算向外取界**；非dyadic pixfrac 时不能用舍入的半宽取代它。初版该处一ulp潜在漏包围经独立审核发现，源码/初始测时保留但初版 bounds 撤资格。修正版重新运行。

对 f∈[l,h]，把仿射值限制在 `[l+e,h−e]` 得 inner，限制在 `[l−e,h+e]` 得 outer。a、b、z 的所有半空间与 U 裁剪，得 `P_in⊆W^-1(H)∩U⊆P_out`。空 inner 不代表真实无交叠。两个多边形面积、一次矩由 GMP 有理数对**实际 binary64 半空间系数**精确裁剪与鞋带公式计算；半空间常数的区间舍入已并入内外约束。因此不靠几何库的经验 epsilon 处理切触和狭片。

对 g 也用式(16)，令 `g0+gx(u−cx)+gy(v−cy)±eg`，面积和一次矩解析积分给
\[
\int_{P_{in}} g_{low}\le a(U)\le\int_{P_{out}}g_{high}.
\tag{17}
\]
下端截0；全部上下界向外求和。g 的导数由 SIP 一/二阶解析多项式和 TAN 归一式产生，非有限差分。这里使用导数区间的均值余项而非声称截断二阶项已经为零。

### 6.2 候选、缓存、fallback 与资源

```text
solve(U, N, requested_absolute_width, resource_limit):
  可选用同WCS小tile缓存的导数区间做一次廉价式(16)请求
  若宽度足够：接纳；否则重算该U局部导数（缓存不缩小真实界）
  连续chart仿射外包 -> 整数格候选；候选积>4096则整块留未决并细分
  每候选：exact clip(inner/outer)，积分g的有界仿射式
  以每块Σ(upper−lower)+spill作为优先级，细分最有影响块
  误差账本用有理数精确增减；结果逐叶仍向外累加，重验停止状态
  pole/atan图无法定义的块：保留全块积分上界为all-sky spill，继续细分
  输出同一bounds/primitive接口，不归零未决片，不以点成员补漏候选
```

默认 C++ 主核选该 affine-strip cut-cell；科学请求允许按指定叶宽度细化，`--leaf`返回的成功只属于该叶，其他叶仍保留原有界，禁止冒充全drop成功。简单 inside/outside 是其多边形等于 U/空集的特例。正式精细 fallback 选 A 的 target 等面积包含积分，直接避开 apex 的正向 atan 导数奇异；B 的 detector floor+有界高阶积分为独立核查/替代证据，尤其用于狭片的定向细化。可合并独立有效界取交集，但不把旧外部 cone 或不完整候选纳入认证。主核失败由预算状态向上传递，不能无界切换四角 HPX 近似。A/B 的Python实现是参考；fallback 的C++集成不在本轮范围。

空间细分不能代替端点精度升级。binary64 的 f(c)/J0/半空间常数出现误差地板时，正式精细路径仍用式(16)–(17)，但全部区间端点、代表数、裁剪常数和ledger一起升级为MPFR128/192/256位，并继续向外舍入；仅把sin/cos内部精度提高后又立即回double没有解决这个问题。局部polygon可用精确有理数，跨块累计先向外量化到固定dyadic格 `2^-b`，每块上下端外包使总宽额外≤2q·2^-b，先选b使其低于预留的绝对面积预算；避免不同交点有理分母的全局乘积爆炸。最终接口若输出binary64端点还需向外转换并重新判断容差，无法表示时返回precision_exhausted。

独立 `round2_mp_affine_fallback.py` 已实际实现这个精细路径的局部版本：60位区间→精确Fraction半空间裁剪/矩→2^-256 dyadic外包ledger，绝不以double端点作科学停止。其明确支持新TAN及二阶shear的N=1叶0附近；其余约束在整源域证明不活跃。对±1e12原生积分、.01分配通量容差，binary64聚焦6401节点仍有约.54通量半宽；高精度TAN只33节点约.06秒，预测半宽.002222；q=.07 SIP约14089节点/29–33秒，半宽.002576，均包含独立解析真值。源A另用16²块的包含型g积分，并共同传播到F。9节点失败及一次未完成的有理分母爆炸记录保留。这证明精度升级路线可执行，不冒称此局部reference已覆盖任意投影、极点或一般SIP；一般模型采用同一高精度原语和通用包含fallback，仍按科学预算与资源停止。

微原型先检查有限输入、SIP合法项、CD区间行列式符号、整tile简单SIP压缩单射证书及TAN≤60°域；不满足时具名拒绝，不能以有歧义的det符号取错误绝对值。已发现并保留近奇异CD的真实反例：旧分支absdet上界低估正面积，即使状态为budget_exhausted，其bound仍无效。最终代码在准入拒绝该例；不把资源失败当无效bound的理由。缓存试探/局部重算都计入max_work。

SIP 真实边界仍可按§3的 Bernstein/de Casteljau 凸包快速排除候选，公共原生边以 `(WcsIdentity, edge endpoints, pixfrac/domain)` 缓存；f=1相邻矩形理论上从4边/像元降到约2条独立边/像元，f<1通常不共享边。该缓存已在 A 单像元曲段树中存在，**跨像元共享尚未实测，不能把2倍复用当吞吐2倍**。本微原型实测的是另一种**同源tile导数区间复用**，含失败后局部重算，真实缓存收益见下表。目标tile的 leaf-square常数/面参数可只读共享，按目标tile排序贡献、CSR行索引复用；当前原型只实现候选格枚举，没有声称实测目标tile缓存收益。

推荐实现布局为小块 SoA `U endpoints / f0 / J0 / residual / chart`，每线程有界队列和小多边形工作区，目标贡献分块归并；WCS和tile界不可变共享。逐运算 binary64 外包可做 SIMD 算术筛选，超越函数用 MPFR/有保证的正确舍入实现，模棱两可谓词升级有理数裁剪。**本轮实测是标量 MPFR/GMP，未测 SIMD/普通double过滤器的倍数**。OpenMP仅用于独立像元，输出每像元归约次序固定；不引入旧调度器对象。

### 6.3 复杂度与测量的解释

令 q 为实际 detector 节点数、K 为每节点候选格数、v 为裁剪后的顶点数（矩形加6条半空间，v为小常数）、d为SIP阶数。算术操作量 O(q(d²+K v)+q log q)，有理数成本另随分子分母位长增长；内存 O(q+output nnz+tile bounds)。微原型保留每像元结果用于输出，正式可按tile流式释放，不需要全帧稠密协方差。候选枚举量本身至少Ω(真实接触叶数)，source比leaf大时不能承诺常数像元成本。

光滑图域下式(16)余项 O(h²)，曲边带的面积宽度为 O(L h²)；边界节点 O(L/h)，边界主项相对 A 的 O(1/ε) 改为 O(ε^-1/2)。g 的面积积分全域余项 O(h²)仍可带来 O(ε^-1) 的内部细化；强SIP、极点/分支、非横截切触不套常数。最坏情况以可计算bounds和资源停止，不能只引用渐近阶保证吞吐。

`round2_performance_final_kernel.json` 记录最终全22构型；`round2_performance.py` 固定源码、编译参数、输入和实测时间；全量/初始失败保留。相对面积宽度只是**性能扫描横轴**，科学发布仍按§5。与A/B不同输入/容差的毫秒/秒不能当成同精度speedup。

| 场景 | 实测范围（当前单机，详细数值见证据） | 科学解释 |
|---|---|---|
| 0.19″、N=2¹⁸、f=.63，16相邻像元 | 约.25 ms/像元，1节点/像元，最多4叶 | subleaf快路径可执行 |
| 1.7″ TAN/弱SIP，N=2¹⁸，16相邻 | 约5.4 ms/像元，5–6.5节点/像元 | 宽度请求1e−4面积proxy全部达到 |
| 18.3″、N=2¹⁶，最多56叶 | 约76 ms/像元，81节点/像元 | 输出叶数成本实际出现 |
| 强SIP .018 pix⁻¹，16相邻 | 约.35 s/像元，15/16达到预算 | 不得称全批精度已满足 |
| 双跨极点，N=2¹⁸ | 约.49 s/像元，1001节点预算耗尽并带spill | fallback需求有实际证据 |
| 256相邻subleaf，1线程，无/有tile缓存 | 约202/180 μs/像元，缓存256次接纳 | 约11%收益，此构型缓存使界变宽但仍达标 |
| 同256像元，2线程，无/有缓存 | 约105/100 μs/像元 | 实测线程加速约1.9/1.8，不外推无限核 |

一般1e−2到1e−4宽度扫描成本显著增加，1e−6/1e−9在3201节点仍耗尽，全部保存。按100 μs/像元线性推算，千万像元约1000秒（此2线程subleaf构型）；按5.4ms单线程约15小时，强畸变更慢。这是**复杂度/线性外推，不是千万像元实跑**。已具备有界、分块、可平行实现路线；没有证据宣称任意高对比/强SIP精度下达到某生产吞吐。若实际预算超出现有资源，必须返回科学未收敛，优化不能改变该语义。

## 7. 正例、反例及已证精度

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

### 7.1 极细正面积、切触和科学停止

原 `tiny_boundary_sliver` 的282/293不是已证明的零。第二轮独立解析源TAN边界与真叶交点，对great-circle边作两段球面积分，100位区间各得 `[1.7943443981693295e−14,1.7943443981693300e−14] sr`。B以1e−16sr全drop预算、31941节点、最终约39.5秒（先行运行约51.7秒单列保留）实际得到两叶各 `[1.7920708298562244e−14,1.7966119907953488e−14] sr`，证明正支持并夹住解析值。旧A/B包含零的结果仍是原预算下有效保守界，不能倒写成当时已识别支持。

另一组全新 N=1、RA45°顶点构型独立证明 strict nonoverlap、纯顶点touch、普通正片和tiny正片。极细真值约1.17809724505e−14sr；普通正片约4.712388704684e−8sr，A/B均取得正下界；tiny在初始资源内仍含0。只要已有正背景支持，是否继续细化由其最终 B/F/Cov 影响决定：同一面积上界乘1或1e12不能使用相同停止结论。单独未决片不能因影响小而伪造归一B。完整解析、实际迭代、signed场与强光反例见[独立证据](INDEPENDENT_ORACLES.md)。

## 8. 原始参考与物理风险

1. [Fruchter & Hook 2002，PASP 114,144，§2与§7](https://arxiv.org/html/astro-ph/9808087v2)，DOI 10.1086/338393：采用像元收缩、线性分配和相关噪声；原论文不提供本文真实 HEALPix/SIP 相交器。
2. [Calabretta & Roukema 2007，§2.1–2.4、式(3)–(10)](https://arxiv.org/html/astro-ph/0412607)，DOI 10.1111/j.1365-2966.2007.12297.x：采用 HPX 等面积 chart。本文另外推导全12face表与包含型积分。
3. [Górski et al. 2005](https://arxiv.org/abs/astro-ph/0409513)：NESTED 和真实叶；[Astropy-HEALPix API](https://astropy-healpix.readthedocs.io/en/latest/api/astropy_healpix.HEALPix.html)作为独立坐标、成员和查询参照。
4. [Farin，Curves and Surfaces for CAGD，Ch.4.1–4.6](https://www.farinhansford.com/books/cagd/toc.html)：Bernstein 凸包、de Casteljau 与细分；成熟几何算法用于多项式源曲线。
5. [Moore, Kearfott & Cloud 2009，Ch.1–4、8](https://epubs.siam.org/doi/10.1137/1.9780898717716.fm)：包含型运算和积分；采用显式上下界，非采样收敛口号。

6. [GNU MPFR 手册，Rounding Modes/Transcendental Functions](https://www.mpfr.org/mpfr-current/mpfr.html)：采用显式RNDD/RNDU的超越函数。普通C++ libm或Boost interval的`rounded_transc_std`不能自动证明正确舍入；[Boost官方舍入说明](https://github.com/boostorg/interval/blob/develop/doc/rounding.htm)明确提示该前提常不成立。
7. [GMP 有理数接口](https://gmplib.org/manual/Rational-Number-Functions)：半空间裁剪/多边形矩使用精确有理算术。局部affine strip是本文由均值公式推导的包含方法，不把某个成熟库API名当其正确性证明。

**ASSUMPTION**：固定WCS/像元核的观测模型充分；pixfrac策略适合实际采样。**REQUIRES_REAL_DATA**：HST/M42 的 WCS/PSF/孔径、弱扩展结构、细叶相关噪声与实际吞吐；所有真实物理效果不能由面积区间代替。数学解算与实际科学产品验收分开。
