> 本分支将本篇作为既定科学输入，保留以下历史审核措辞和有限证据身份。工程状态只由[新设计入口](../../engineering/rebuild/README.md)定义；历史中的“不创建分支/生产未修改/复算命令”描述其原始研究时点，不约束本分支。历史实验源码和证据已从新树移除，本篇相关路径仅指[固定基线研究档案](https://github.com/fujiaze/Astro-CS-Database/tree/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation)，不是新实现/测试依赖。实际公共字段由新合同唯一拥有。

# 统一误差、相对天光和传统合成

**候选科学规范，待外部独立审核。** 从[同一观测模型](OBSERVATION_COORDINATES.md)与[同一几何 R](SPHERICAL_DRIZZLE.md)出发。本文定义可执行的统计基线；独立代码 `statistics_independent.py` 实现小矩阵参考，真实大规模数据适用性另列。

## 1. 三种估计量与真实缺参数

| 对象 | 定义 | 单位/使用 |
|---|---|---|
| V_F | 固定 PSF/参考源/背景自由度下的通量方差 | L²；点源深度 |
| V_B,p | 实际场景目标叶 `r_p C_x r_pᵀ` | L² sr⁻²；面亮度合成 |
| V_ctrl | 真实控制统计 `T y` 的方差 | 与控制量平方一致；sky拟合 |

参考源、空背景和 actual scene 分别标记 condition。`SNR_ref²/F_ref²=1/V_F` 不能直接当作 `1/V_B`。若同一模型下两者都已知，`T=V_B/V_F` [sr⁻²] 只是换算恒等式，不是从一个 SNR 恢复未知场景方差的方法。

独立电子噪声条件下
\[
V_{d,i}=(\mu_{{src},i}+\mu_{{sky},i}+\mu_{{dark},i}+\sigma_{r,i}^2)/g_i^2.\tag{1}
\]
电子期望均非负；扣 sky 后负 ADU 不作为负 Poisson 均值。减去 sky 均值后其 photon shot noise 仍在。经验空背景 RMS 已含 sky/dark/read 时不能再次累加；没有 gain 时不能推导未测亮源 shot noise。重复帧可估特定场景经验误差，但必须保留估计自由度、相关性、稳定场景和样本选择假设。

校准例 `x=(d−b−t dark)/flat` 的 Jacobian 为 `(1/flat,−1/flat,−t/flat,−x/flat)`，母版 combine rule 决定其 covariance；共同母版按同一个 shared_id 跨帧归属。修复/去马赛克若是线性插值也传播其算子；非线性处理另作线性化或模拟。

误差状态使用 `known/conditional/approximate/unknown`，附 included_terms、omitted_terms、conditioning、单位、共享身份及模型偏差。非零 R 系数对应任何未知误差项，完整 V_B 就未知；已知部分可作诊断，不能将未知项填0。缺精度不删除合法信号，默认可用明确 equal/经验权，输出不标 full_IVAR。

## 2. 同一随机变量的一次传播

固定几何、选择及校准参数，`y=R Dα d`，`C_y=R Dα C_d Dαᵀ Rᵀ`。共享外部参数 θ 给 `Jθ Σθ Jθᵀ`；若 θ 从同图训练，必须包含 Cov(d,θ)。统一做法是以独立原始随机块 ξ 为根，全部输出写成 `z=f(ξ)`，用 `Jξ Cov(ξ) Jξᵀ` 一次传播；参数自身作为 d 的函数时，不再额外独立加一次参数方差。

对任何孔径权 t（可带负背景环），
\[
 V(t^Ty)=(R^Tt)^T C_x(R^Tt).\tag{2}
\]
相邻输出共用源像元产生 covariance。忽略非对角元对正孔径常低估，但有负权时误差方向不一定。更小像元 RMS 不能证明测光信息增加。

点源模型 `y=Fh+Zβ+ε`：C SPD 时白化 `h_w=C^-1/2 h,Z_w=C^-1/2 Z`，
\[
I_{F|β}=\|(I-\Pi_{Z_w})h_w\|^2,\quad V_F=1/I_{F|β}.\tag{3}
\]
用 QR/SVD 投影残差再求平方，避免两个大数相减。F 不可辨识时返回 unavailable，不取伪逆 FF 元冒充有限误差。PSD covariance 的零空间是精确约束 `U0ᵀy=U0ᵀ[h,Z]θ`，先解约束再对白噪声子空间 GLS；直接 C⁺ 会丢失无噪声信息。显著负谱不是协方差，不用随意 ridge 掩盖。

若 R 满列秩并且 h、Z、C 同时传播，条件信息保持；降秩投影的信息不增加。独立反例：两帧 `h=(.5,.5),v=.5` 与 `h=(.9,.1),v=.82` 有相同 V_F=1，但单叶面亮度权分别2和1.219512。选择权必须匹配目标量。

### 2.1 几何数值界不是另一个随机噪声

面积求解器输出 `[aL,aU]、[AL,AU]、[ΩL,ΩU]`；先按[几何分册](SPHERICAL_DRIZZLE.md)形成同身份的 `w=a/A`、`N=ΣwΩ`、`B=Σwx/N` 与 R 的包含界。`F=Σwx` 为分配积分量，N 是归一有效面积；它们与真实覆盖并集不同。负 x 合法，区间乘法取全部端点积的最小/最大值。`NL=0<NU` 是 `support_unresolved`，不能发布一个有保证的归一 B；`NU=0` 是 `no_support`。`0<NL<Nmin` 可具名 `weak_support`，Nmin 是声明的条件阈值；它和科学误差是否达标是两个状态。正支持上的精确 B=0 是真实零信号，不是无支持。

不把各叶的几何误差设为独立正态分布。R0 为中心算子、`|R−R0|<=E`，固定已知原生 C，记 `ep=E[p,:]、rp=R0[p,:]`，直接展开得
\[
 |\Delta C_{pq}|\le
 e_p^T|Cr_q|+e_q^T|Cr_p|+e_p^T|C|e_q.\tag{3a}
\]
该界保留负的 C 元素和共享输入；不是把 `R interval` 的各元素当作可独立随机变化。对任意含负背景环的孔径 t，令 `u0=R0ᵀt、δ=Eᵀ|t|`，则
\[
 |V(t^TB)-u_0^TCu_0|\le
 2\delta^T|Cu_0|+\delta^T|C|\delta.\tag{3b}
\]
面积区间的原始 primitive identity、共享 A/Ω 与每源列和 `Σp wjp=1` 一并保留。完整分配量求和时列守恒使数值误差可消去；逐叶误差独立相加会丢失该信息。面积上界含零仅表示该叶贡献未决，不能宣称存在真实非零支持。在已有可靠正支持上，一个未决 sliver 对科学量的上界低于容差即可保留；sliver 单独存在时无论面积上界多小都不能据此创造归一信号。

式(3b)不需要物化输出大矩阵。原生 `C=Σ LkLkᵀ` 时，可计算 `ηk=|| |Lk|ᵀδ ||2`，用 `Σk[2||Lkᵀu0||2 ηk+ηk²]` 作界。对角/局部/共享稀疏因子的绝对系数 action 可分块计算；不能用会发生正负抵消的普通 C action 代替 `|C|` action。仅有 C action 而没有绝对系数或可信谱范数上界时，正式 covariance 仍可求值，但没有由它自动得到的严格几何扰动界。未知物理噪声项仍为 unknown，面积精确不补出它们。

科学停止条件从实际估计量反推。取 `ew=max|w−w0|、eΩ=max|Ω−Ω0|`，中心 N0、B0 对应同一 w0、Ω0；要求 `NL>0`，且
\[
 \sum_j e_{wj}(|x_j-B_0\Omega_{0j}|+\epsilon_B\Omega_{0j})+
 \sum_j w_{Uj}e_{\Omega j}(|B_0|+\epsilon_B)
 \le\epsilon_B N_0,\tag{3c}
\]
足以保证 `|B−B0|<=εB`，其中分母损失已经计入。实际可用分式盒极值进一步收紧，详见几何分册。常场且 Ω 精确时第一项残差为零；Ω 不精确而输入 x 是独立积分量时，不暗中强加 `x=bΩ` 消掉该项。亮星1e9和 B 绝对容差 .01 的独立例要求交叠相对半径约1e−11，说明统一1%面积误差不是通用科学精度。

### 2.2 几何误差经过 sky 与合成权重求解

选择、T、K、E 及权重固定时，组合 `L=P R` 后重复式(3a–b)，不另加独立 sky 方差。若后续 Export/固定权 coadd 的 action 为 Q，`|Q−Q0|<=EQ`，则 `L=QR` 的包含误差可取 `EL=|Q0|ER+EQ|R0|+EQ ER`，同一 L 同时作用于信号、PSF/response 和 noise；共享 primitive 身份仍保留，盒界只作包含放宽。若 K、GLS 权或控制响应也随几何变化，中心值冻结只定义一个条件估计量，不能声称覆盖了理想几何重算流程；必须传播其线性求解扰动。

通用可实施界：`M0 u0=b0、|M−M0|<=EM、|b−b0|<=eb`，G 为有可审计误差的逆或预条件器。若
\[
 \rho=\||I-GM_0|+|G|E_M\|_\infty<1,
\]
则
\[
 \|u-u_0\|_\infty\le
 {\|G(b_0-M_0u_0)\|_\infty+
 \||G|(e_b+E_M|u_0|)\|_\infty\over1-\rho}.\tag{3d}
\]
固定 gauge/可估秩后的 sky 增广系统、BLUE 的 `Γq=1,t=q/(1ᵀq)` 都可使用；归一分母还需严格正下界。大型系统使用同一 QR/迭代 action 及可验证预条件器，不要求显式形成稠密逆；无法给出有限 ρ 界时保留条件估计与 `geometry_solve_unresolved`，不靠 ridge 更改目标。秩、接触支持、排异或权重分支在区间内可能改变时，证明分支稳定、包围全部允许分支或报 `selection_unresolved`。固定最终 mask 的 covariance 仍有原有统计选择问题，几何界不消除它。

独立 `round2_science_budget.py` 以标准库有理数实现分式盒极值，和三个各64顶点的独立枚举逐分数相等；八类科学场景及58个真实等面积 chart 矩形目标叶都包含已知真值。它检验已给面积区间的传播，不认证 WCS 相交器。signed 孔径方差真值 `.8369525738`，错误独立输出算法 `4.9512278532`；几何 perturbation 给有限误差界。进一步 BLUE 初始 `ρ=1.27217465` 显式未决，面积半径缩小100倍后 `.01270681` 得有保证区间，再缩小100倍为 `.0001270666`。全部失败、精确端点、精度请求与资源见 `evidence/round2_science_budget.json` 和研究记录 `ROUND2_SCIENCE_BUDGET.md`。确定数学界和这些有限真值检查分开，也不等价于新 C++ 机器浮点的形式化证明。

`round2_cpp_science_bridge.py` 把实际 C++ 原型的六个无SIP构型、九个目标叶接入同一传播，独立60位区间 TAN 球三角计算 Ω/A；构造近1e9/−2 L/sr的亮/暗两贡献，并计算 R 与 covariance 界。B目标 .001 时，fine_subleaf 误差界1.65e−5达标；normal_tan 误差界126–5137、coarse_many_leaf 2408–16877仍为 `science_unconverged`。两个有独立解析交叠真值的例子，真实 B 误差约4961和29980，均落在其更宽预测界内。其他叶没有解析真值，明确仅给条件区间，不把中心值当真值。面积proxy达到要求、几何有界与最终高动态科学精度达标必须分别报告，详见 `evidence/round2_cpp_science_bridge.json`。

## 3. 亿级叶网格可执行的 covariance 表示

原生模型存
\[
C_0=B D B^T+C_{loc}+U\Lambda U^T,\tag{4}
\]
D 为独立源方差，B 包含已声明局部校准/插值影响；C_loc 用有支持 stencil 的 CSR 或可证明 PSD 的局部平稳核块，不能任意拼核生成非PSD矩阵。U 的列按独立 shared_id 组织，通常是帧/母版/响应组的稀疏参数 Jacobian。未知相关项不因未存储而成为零。非平稳处可保留完整原生稀疏算子，不强迫用一个全局 correlation_factor。

实际保留组合影响算子 L，而不是逐叶存 N×N：`C(v)=L[C0(Lᵀv)]`。单像元方差/跨帧 Γ、孔径方差按请求求值，局部块可缓存。对角场+局部相关核+共享低秩是**有限近似或结构化计算表示**；本规范不声称任意真实噪声都有固定小秩。若忽略远相关项 E，需给 `|tᵀ E t|≤||E||₂||t||²` 或实际留出/频谱误差；无界则状态 approximate。

同数据 sky 后的 P 可能减少某些模式方差，不能总写成“原对角+正的 sky 低秩”。令 `βhat=K T y,E` 为输出求值，`V=C_y Tᵀ Kᵀ,Cβ=K T C_y TᵀKᵀ`，有
\[
C_z=C_y+[E,V]
\begin{bmatrix}C_β&-I\\-I&0\end{bmatrix}[E,V]^T.\tag{5}
\]
中间低秩修正可不定，完整 C_z 仍 PSD。首选安全表示 `C_z=P C_y Pᵀ` 的因子/算子 action；若用式(5)必须支持 signed block 和稳定 LDL，不能只加独立参数正方差。

叶数 N、局部宽度 q、每位置有效 shared 列 r_loc 时，显式局部场 O(Nq)、稀疏U O(N r_loc)，逐块流式读取无需全部驻留。以 N=10^8、q=25 为例，单独25邻域FP32数值已约10 GB，尚不含索引；绝不默认全图常驻。每64k叶块的25邻域数值约6.6 MB；用卷积核参数/源R因子按需恢复可进一步减量，但近似有单独误差身份。全图 r 不应当作每叶固定稠密列，按组共享稀疏元数据。

原生 R 有 nnz_R 项，局部方差约 O(nnz_R)（独立输入）；一般 low-rank action O(nnz(U)+cost(Λ action))，稠密 r×r 的 Λ 另需 O(r²)。按共享组预分解 Λ 并令 U 吸收其因子、latent covariance为I时，可省去在线稠密乘法，但必须计入预分解的存储/计算。孔径回溯仅读取触及源及共享因子。Sky K/T/E 保存稀疏提取/求解 action，不物化稠密 P。外部独立低秩+正对角的 inverse action 可用 Woodbury；含同数据 sky 或奇异模式时用保留子空间/稳定求解，不能直接套 SPD inverse。

### 3.1 稀疏 actual-scene 精度的有支持重建

首先直接从原生噪声/同一R正向计算V_B。只剩稀疏控点时，要求它们属于同一estimator、单位、response及scene condition；reference-source SNR控点不能直接改名为actual-scene variance。对正V建 `u=log V`，在有限局部球面gnomonic chart上建立Delaunay三角网；查询点在观测支持三角形内部时，使用非负barycentric权λ，`Σλ=1,Σλ x_i=x`，`uhat=Σλ u_i`。图洞、跨不兼容响应和支持域外返回unavailable，不nearest-fill“认证精度”。

如果控点log误差有外部可信界δ_i，且该chart/条件下真实u的Hessian谱范数有已声明上界M，则Taylor余项给
\[
|\hat u-u|\le\sum_i\lambda_i\delta_i+
{M\over2}\sum_i\lambda_i\|x_i-x\|^2=\Delta,\quad
V(x)\in[\exp(\hat u-\Delta),\exp(\hat u+\Delta)].\tag{4a}
\]
δ/M是外部信息或物理先验，不能从同一稀疏点拟合后自证。无M时可以发布具名conditional预测和空间块留出误差，不给上述严格区间；该预测误差不是新生成的photon noise，也不补出未知跨叶covariance。smooth函数在全部有限控点为0、查询处任意大bump的反例证明：没有额外约束就不存在有限上界。

`reconstruct_scene_variance(Controls,Chart,SupportedTriangulation,Query,optional δ/M)` 返回 `{value,log_error_bound,interval_or_null,support_state,estimator_identity}`。小域三角剖分预处理约O(M_c log M_c)，每查询定位约O(log M_c)、三权计算O(1)，按球面分块及halo处理，不在全天空直接作单个平面Delaunay。曲率M只适用于声明chart，不把角坐标欧氏Hessian直接搬入球面。

独立 `sparse_variance_reference.py` 的25控点/5000查询中，log平面一次再现最大误差1.61e−15；加入已知δ和二次真值Hessian界后，两个5000查询实验均无区间违例，最大`|error|/Δ`约.949/.960；图外不可用和无M的bump负例均实际执行。这是有限数学真值检查，不是对真实稀疏场曲率的测量。

## 4. 相对 sky 的成熟最小解

只在通带及有效空间响应可比较的组中建立控制模型
\[
c=T y=H s+Aβ+e,\quad C_c=T C_y T^T .\tag{6}
\]
s 是任意共同真实天空，包括星云斜坡、暗云、恒星晕；Aβ 是逐帧加性差模。T 基线使用固定支持的线性 patch 平均/明确像元核，mask 先固定；median/clip 若采用，要另有影响函数或全过程MC，不能默称其 covariance 为单像元variance/n。

同一控制需要相同 H；不同 PSF 先在控制尺度以可实现的卷积核匹配共同较宽核，或在式(6)显式建各 H。不同谱响应、未知星云 SED 时不给共同 s 差模边，仍允许最后用户定义混合。对照组身份比滤镜名称更重要。加性拟合不能吸收真实色差/PSF残差并宣称保护结构。

求解采用 Montage 重叠差与 Madam 公共天空消元的成熟思想。每个连通分量选 gauge；对于所有帧共有的 `[1,u,v]` 基，公共常量和两种斜坡都不可辨识，并非只有一个零点。选 reference-frame 系数0或加权系数和0只是规范，不是测得绝对天空。

令 G 的列张成满足 gauge 的参数子空间，白化 W 满足 `W C_c Wᵀ=I`，Q_w 为 `W H` 列空间的正交补。求
\[
\hat\eta=\arg\min_\eta\|Q_w W(c-A G\eta)\|^2,\quad
\hatβ=G\hat\eta=Kc .\tag{7}
\]
`K=G(Q_w W A G)^+ Q_w W`，参数满可估子空间后才发布有限协方差。只有连接图连通不够：所有控制共线时二维平面仍缺秩。列尺度归一后用 QR/SVD 识别实际零空间，新增未识别方向不得靠 ridge 变“测量”。孤立帧可取 correction0 的 gauge，但无相对测量，状态单列。

```text
fit_relative_sky(controls, H, basis, C, gauge):
    按响应/空间可比性与有效控制构图，分连通分量
    用同一个 basis.eval(frame,sky_position) 装配 A 和输出 E
    对公共s白化消元，gauge子空间内QR/SVD/稀疏LSQR
    返回β、参数可估子空间、K的action、支持域及残差/留出诊断
apply_sky(y, fit): z=y−Eβhat；同时返回P=I−EKT的action
```

`K H=0` ⇒ 在声明 H 中任意共同梯度加到输入都不改变 βhat；这就是低频保护证明。它不证明错误 H 下结构仍保护。所有 pairwise difference 若复用同一帧，差分 covariance 非对角；例如 `(y1−y2,y2−y3)` 为 `[[2,−1],[−1,2]]`，不能独立计数。优先原始 controls+profile，避免边压缩丢掉依赖。

固定 T、权、basis、gauge 下完整校正 covariance 是
\[
C_z=(I-EKT)C_y(I-EKT)^T.\tag{8}
\]
展开有两项负交叉；独立 holdout 像元可能只增加拟合方差，参与拟合像元则有自相关影响。鲁棒 Huber IRLS 可处理异常残差，但冻结最终权的式(8)只作条件线性近似；稳健权来自数据时正式 coverage 由联合模拟/按独立来源分组bootstrap确认。对无控制支持区域，输出预测/外推状态与 sky 参数预测项 `E Covβ Eᵀ`，不能以平滑填充值冒充观测。该参数项不是完整 corrected covariance；与训练控制共享 flat/Gaia/Drizzle 随机变量时，即使空间上 holdout，仍需式(8)的交叉项。

稀疏联合设计 `[H,A G]` 不显式造 Q；普通 LSQR/LSMR 作用于白化设计，一轮成本 O(nnz(H)+nnz(A)+cost(whiten)+cost(whiten_transpose))。仅有 C(v) 不能当成白化；可改用含 C^-1 action 的广义正规/增广系统和合适的迭代求解，并记录内层线性求解误差。小分量密集 SVD 只作参考。常数/平面基参数少，未知模式仍按数据谱判断，不固定迭代数冒称最小二乘收敛。

## 5. 传统合成与小 N 排异

同一量 \(E[z_k]=B\)、固定资格/权、已知帧间 Γ 时，BLUE
\[
t={\Gamma^{-1}1\over1^T\Gamma^{-1}1},\quad
\hat B=t^Tz,\quad V=t^T\Gamma t.\tag{9}
\]
独立时化为 IVAR `1/V_B`。若 Γ PSD，保留精确约束再解可估子空间；未知贡献不补地板后称已知。若要求非负权/有效非负通带，用约束 QP `min tᵀΓt,s.t.1ᵀt=1,t>=0`，不把非负解称无约束BLUE。不同 PSF/响应的 z_k 是不同 μ_k，合法输出是[响应分册](GAIA_RESPONSE.md)定义的条件混合 K_eff；不声称测同一天空标量。

默认小 N 无自动时间排异，先使用可信 DQ/二维伪影证据。用户启用统计排异且有外部可信 Γ 时，N≥4 的保守基线只做一次、最多拒一个最大标准化留一残差：
\[
\mu_{-k}=t_{-k}^Tz_{-k},\quad
v_{res,k}=\Gamma_{kk}+t_{-k}^T\Gamma_{--}t_{-k}
 -2\Gamma_{k,-}t_{-k},\quad
q_k=(z_k-\mu_{-k})/\sqrt{v_{res,k}} .\tag{10}
\]
`t_-k` 为真实 covariance 子块的 GLS。清洁共同均值 Gaussian 且 Γ 外部固定时每 q_k 边际 N(0,1)，故阈值 `Φ^-1(1−α/(2N))` 给逐位置 family false-reject probability≤α；q_k 不需互相独立。参考 α=.001，是研究基线参数而非全图误拒率承诺。N=1..3 不自动识别异常身份；N4..7 已实跑。N更大可仍用该保守基线、二维mask或成熟异方差 clipped mean；多异常策略须对其污染/PSF模型独立验证，不凭此单异常实验外推。

禁止默认 `median±p|median|`：`[-2,-1,0,1,2]` 留0，整体+10后留[8,9,10,11]均值9.5≠10，违反加性平移等变性。真实天体零点、负信号不应改变噪声显著性。未知 variance 时用户可选明确启发式 robust 策略，但不附该 Gaussian 定理。变星、瞬变、移动天体不自动定义为坏像素。

不能并行丢弃所有超阈 q_k：一个坏帧污染其余留一预测，可能 swamping。失败试验 N5 单12σ污染后95%coverage=.91603；单步只拒最大者改为 .95096。**这不证明任何污染幅度或多污染都可靠**。固定接受集合后 `tᵀΓt` 是冻结选择公式，数据选择后的真正条件分布已截尾；需全过程forward MC/可识别污染模型的bootstrap。不得把“给定mask”误写成选择后精确95%区间。

小N预计算 LOO solve O(N⁴)，每位置残差 O(N²)，实际N≤7为小常数；接受mask≤2^N可缓存。一般每叶 GLS O(K³)，共享误差结构可用Woodbury/分组缓存；不要求每个像元重复大矩阵逆。

## 6. 最小输入/输出合同

```cpp
struct NoiseModel { EstimatorIdentity estimator; Unit variance_unit;
  NoiseState state; CovarianceAction action; DependencyGroups shared;
  TermSet included, omitted; Domain observed_support; };
struct SkyFit { Basis basis; Gauge gauge; Vector beta; LinearAction K,T,E;
  CovarianceAction parameter_cov; RankReport rank; Domain support; };
struct ScientificFrameView { FrameId id; TargetGrid grid; Array signal; Mask valid;
  SupportGeometry support; Response response; EffectivePSF psf;
  NoiseModel noise; LinearAction native_to_leaf; SkyFit sky; };
struct CoaddRequest { FramesProvider frames_and_neighborhood;
  EstimatorIdentity target; RejectionPolicy rejection; WeightPolicy weights; };
struct CoaddResult { Array single_signal; InternalDiagnostics diagnostics;
  CovarianceAction optional_covariance; SpatialResponse effective_response; };
```

接口按真实球面位置、单位和响应交换，不引用旧scheduler或adapter。不同相机直接 Normalize 到同一 leaf order；Mosaic 混order明确拒绝，不做同tile/index错误叠加或额外混阶重映射。未来 SACoadd 可访问逐帧邻域、PSF、noise/response/support；不能只传预累加 `Σwy,Σw`。本轮只定义成熟传统合成。输出一个正式 signal HiPS；协方差 action、support 和选择为内部/诊断，不创造另一个物理信号。

## 7. 证据、假设和文献

独立已运行：4×3 R 的矩阵/因子 covariance、共享floor、PSD精确约束、sky共同梯度保护与失配反例、Gaia共享潜通量联合拟合、Gaussian/Poisson与 N1..7 MC。正确sky算例方差 `.9869281`，错误独立加法 `1.0653595`；实际MC `.9880928`。N5清洁Gaussian误拒 `.00102`，精确二项95%区间 `[.00083176,.00123808]`；单12σ污染后95% coverage `.95096`，区间 `[.9496034,.9522904]`。全部seed/分母/其他CI见[证据](INDEPENDENT_ORACLES.md)。经验差异不是算法统一误差上限。

**DERIVED**：线性协方差、profile/gauge、共同天空保护、固定Gaussian外部C的Bonferroni界。**INDEPENDENTLY_TESTED**：具体合成矩阵和MC。**ASSUMPTION**：真实响应/PSF可比、低阶加性差模足够、共享参数与噪声模型充分。**REQUIRES_REAL_DATA**：实际共模/低频噪声、HST重复曝光coverage、M42真实色差与星云保护、宇宙线/卫星线/时变源及数据选择全流程。

成熟来源及采用范围：

1. [Montage 官方算法，Background Modeling](https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html)：借用重叠差与全局背景一致性；不照搬其固定迭代及四角几何假设。
2. [Keihänen et al. 2010，Madam](https://arxiv.org/abs/0907.0367)：共同天空消元与加性基线；CMB扫描噪声先验不自动适用于CCD。
3. [Fruchter & Hook 2002，§7](https://arxiv.org/html/astro-ph/9808087v2)：重采样相关噪声；同一R传播是本规范统一接口。
4. [Zackay & Ofek 2017 I](https://arxiv.org/abs/1512.06872)、[II](https://arxiv.org/abs/1512.06879)：点源matched filter及背景主导proper coadd的适用域。本文传统面亮度合成不宣称异PSF下全目标最优。
5. [Gruen et al. 2014，clipped image combination](https://arxiv.org/abs/1401.4169)：成熟残差排异需处理PSF失配，不能用小N比例阈值替代噪声模型。
