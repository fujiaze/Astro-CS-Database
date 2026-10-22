> 当前科学设计状态：已完成独立科学及工程完整性复核，未发现尚存P0/P1设计阻碍；结论与范围见[最终审核记录](../../audit/final-review.md#科学审核状态)。不再以重复的通用独立科学审核作为启动实现前置；负责人启动授权、实现后的独立数值/产品/性能验证及真实数据适用性验证仍分别成立。公共字段、签名与枚举只由[contracts.hpp](../../../include/acsd/contracts.hpp)拥有。
>
> 历史证据边界：下文所有“已运行/生产探针/本轮/独立脚本”、研究性能数字及研究文件名均指[固定基线档案c5fee09fe](https://github.com/fujiaze/Astro-CS-Database/tree/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation)。这些脚本及旧生产源码不在当前工作树；复算只能在该提交的单独历史checkout及其依赖环境中进行，不能在本分支根目录直接运行，也不能用其结果验收新实现。当前可运行内容仅为[根README](../../../README.md)列出的设计/schema/声明检查，未来科学测试另按VERIFICATION重新编写。


# 观测、坐标与像元：重建科学基础

**当前科学设计基线（审核范围见页首）。** 本目录的五篇文档是 #111 所要求的重新开发依据；旧 science、工程合同和 C++ 只作参考，不要求新算法继承其对象或调度。本文拥有观测模型、坐标和 Export 定义；[响应](GAIA_RESPONSE.md)、[几何](SPHERICAL_DRIZZLE.md)、[统计](STATISTICS_SKY_COADD.md)、[证据](INDEPENDENT_ORACLES.md)分别展开其余定义。本文不声称旧产品或真实数据已验收。

## 1. 量、单位与前向模型

| 符号 | 定义 | 单位 |
|---|---|---|
| \(I_\lambda(\omega,t)\) | 到达大气前的天体光谱辐亮度，含所有真实扩展结构 | W m⁻² sr⁻¹ nm⁻¹ |
| \(f_{s,\lambda}\) | 恒星的光谱辐照度 | W m⁻² nm⁻¹ |
| \(\omega\) | ICRS 单位方向；另附观测历元 | 无量纲向量 |
| \(A_0,T,Q,E_{\rm atm}\) | 集光面积、光学/滤镜透过率、QE、大气透过率 | m²、其余无量纲 |
| \(P_i(\omega,\lambda,t)\) | 一个入射光子最终被像元 i 收集的几何概率，尚未乘 QE | 无量纲 |
| \(g,b_{\rm elec},\sigma_r\) | 原生 gain、电子偏置、读出 RMS | e⁻/ADU、ADU、e⁻ |
| \(d_i,x_i,B_p\) | 原始数值、校准后积分线性量、重建面亮度 | ADU、L、L/sr |

这里 L 必须绑定具名尺度：native ADU、同设备相对线性尺度、或本文响应分册定义的 photon-model 标度。标度未获得物理锚时不得写成电子或 SI 辐亮度。

假定探测器在线性、未饱和域，曝光 k 的电子期望为

\[
\mu_{ki}=\int_{t_k}^{t_k+\Delta t_k}\!dt\int d\Omega\int d\lambda_{\rm nm}
 I_\lambda(\omega,t) A_{0,k} T_k(\lambda,\omega,t)Q_k(\lambda)
 E_{{\rm atm},k}(\lambda,\omega,t){\lambda_{\rm nm}10^{-9}\over hc}
 P_{ki}(\omega,\lambda,t)+\mu_{{\rm sky},ki}+\mu_{{\rm dark},ki}.
\tag{1}
\]

\(e_{ki}\sim\operatorname{Poisson}(\mu_{ki})\)，读噪 \(r_{ki}\) 零均值，
\(d_{ki}=b_{{\rm elec},ki}+(e_{ki}+r_{ki})/g_{ki}\)。额外 sky 项只含尚未放进 \(I_\lambda\) 的大气/仪器背景；不得双算夜空辐射。偏置不是 photon 均值。相关读噪、非线性、IPC、CTI、rolling shutter、flat 和共享母版另作为有身份的响应/误差项；它们不因式(1)写成简式而被假定为零。

一个明确的收集核是
\[
P_i(\omega,\lambda,t)=\int_{P_i^{\rm det}}
 p_\lambda(\omega'\mid\omega,t)\,\rho_i(u,v,\lambda)\,
 \left|J_\Omega W(u,v)\right|du\,dv,\quad\omega'=W(u,v).
\tag{2}
\]
PSF \(p\) 的单位 sr⁻¹，按出射方向积分为1；\(\rho_i\) 为 PRF/相对像内效率。无损完整探测器且 PRF 覆盖完整时 \(\sum_i P_i=1\)，有限 ROI/坏区允许小于1。源位置是连续 PixelCenter0 参数；用式(2)拟合像元**积分**，不能默认在整数位置采样连续 PSF 就等于积分。高斯可用 erf，Moffat/Airy 用自适应二维求积并给尾通量及求积误差。有限窗不重新归一为“总通量”。

作用顺序为：光谱/时间天空 → 波长相关传播与 PSF → PRF/像元积分 → 光电子及读噪 → 母版/flat 校准 → 经验光度乘性标度 → 球面 R → 可比较观测的相对 sky → 合成 → Export。只有空间恒定标量与线性空间算子可无条件交换；空间 flat 与卷积、色相关 PSF 与通带积分、像元积分与采样、背景拟合与排异通常不交换。跨仪器不存在由坐标对齐自动得到的共同 PSF。

**DERIVED**：式(1)的量纲、线性算子次序及 shot-noise 传播。**ASSUMPTION**：给定曝光内的线性响应、充分 PSF/PRF 模型。**REQUIRES_REAL_DATA**：真实 flat/大气色项、非线性及 PSF/PRF 适用性。

## 2. 唯一坐标合同

数组为 `data[y,x]`，shape 为 `(height,width)`，x 为列、y 为行。第一像元中心 `(0,0)`，面积单元 `[-.5,.5]²`；完整图像域 `[-.5,width-.5]×[-.5,height-.5]`。中心和边缘都允许实数，不能把两者混成“整数像素”。

字段与实际类型只见[唯一声明合同](../../../include/acsd/contracts.hpp)；本篇保留数学角色、单位与误差语义，不另声明工程结构。

坐标角以 radians 运算；FITS CD/CUNIT 与外部 RA/Dec 接口按明确单位转换。天空角误差用 `atan2(norm(n1×n2),n1·n2)`，极点不比较 RA，RA 回绕不作欧氏减法。Gaia catalog 的 ICRS 位置须按 proper motion、parallax 和观测时间传播到实际匹配历元；输入合同包含 epoch 与时间尺度、`pmRA*cosDec` 约定、观测者站址或 barycentric position，已知 RV 时纳入 perspective acceleration。缺失信息时记录忽略项及其位置误差预算，不能只凭 parallax 和时间制造已知 topocentric 方向。大气折射/DCR 另有观测条件，不能偷偷改变 ICRS 定义。

FITS/WCS 接口仅消费 `FitsPixel1`，其逆只返回同类；公共内部接口负责一次 `±1`。Astropy `all_pix2world(p0,0)` 等价 `all_pix2world(p0+1,1)`；`pixel` API 顺序 x,y，`array_index` API 顺序 row,col。完整 SIP 要用 `all_*` 而非只含线性核心的 `wcs_*`。

## 3. TAN、SIP 与逆函数

令 \(r={\tt CRPIX}\) 为 FITS 1-based，\(u=p_0+1-r\)，SIP 多项式 \(A(u,v),B(u,v)\) 单位 pixel，
\[
q=C_{\rm rad}\{u+(A,B)^T\},\quad
n={c+q_x e+q_y n_N\over\sqrt{1+q_x^2+q_y^2}}.
\tag{3}
\]
c 为 CRVAL 方向，e 为当地东向，n_N 为北向。CD 作用在 SIP **之后**；PC 形式是 `CD=diag(CDELT) PC`。CD 的负行列式合法，表达手性，不是负面积。上述切基对应 `LONPOLE=180°` 的 TAN；下面明确规定其他 LONPOLE 的入口规范化，几何内核只接收规范化模型。

逆过程先 \(q=(n\cdot e,n\cdot n_N)/(n\cdot c)\)，`n·c<=0` 不属于该 TAN 图；再解 \(u+(A,B)=C^{-1}q\)，最后 \(p_0=u+r-1\)。AP/BP 只是可选逆多项式初值，不能用其误差替代真实残差。用有阻尼 Newton 和解析 Jacobian，按 detector pixel 残差停止；遇到非单射、奇异 Jacobian 或不收敛返回具名状态。对单个 Q，\(\sup_Q\|\nabla(A,B)\|_\infty<1\) 充分保证 SIP 全局单射；这是可编码的充分域，并非所有有效 SIP 的必要条件。未满足时不作“已证明 fold”的判断，必须增加单射证据或返回 `injectivity_unresolved`。

点前向/逆向与面积操作分别调用声明中的forward、inverse、measure；完整参数、返回和错误语义见[API](../../engineering/rebuild/API.md)，不另起接口名。

多项式次数 d、一个点的直接计算 O(d²)，Newton m 次 O(md²)；批量广播/缓存不改变坐标定义。精度预算分为头模型误差、坐标运算误差、逆求解残差；小 round-trip 只能说明内部自洽，不能发现同向的原点错误。

### 3.1 首期冻结的模型域与输入处理

首期科学重建支持以下**有限模型类**。这是可计算能力选择，不是对所有合法 FITS 的宣称；reader 能打印世界坐标不意味着其曲边可以按本规范认证。Normalize 输入与科学 Export 输出使用同一模型类；HiPS 的 HPX 序列化头是单独的标准几何映射，不借此扩大输入 WCS 能力。

| 属性 | 首期接受及拒绝行为 |
|---|---|
| 维度、轴 | 仅二维 `RA---TAN,DEC--TAN` 或两轴均带 `-SIP`；内部固定 x,y 顺序。额外 spectral/time 轴、交换天空轴或其他投影返回 `unsupported_model`，不猜测切片 |
| 参考系 | 输入明确 `RADESYS=ICRS`，附本曝光历元及天测模型身份。FK4、FK5/J2000、Galactic、缺失参考系均不自动当 ICRS；可由独立标准 frame converter 生成有误差身份的 ICRS 模型，再重新准入 |
| 单位与线性部分 | 首期头入口接受显式 degree；内部统一 rad。接受 CD，或 `diag(CDELT) PC`（缺 PC 元按单位阵）；两者同时出现或 CROTA 与其混用返回 `ambiguous/unsupported`。CD 有限、可逆；首期 `cond₂(CD)≤10⁸`，否则先按目标位置精度论证再扩域，不截断奇异值 |
| CRPIX、CRVAL、手性 | CRPIX 可非整数、图内或图外；RA 按周期理解、Dec∈[−90°,90°]，包括精确两极。det CD 可正可负；不要求居中、不要求方形像元或对角 CD |
| SIP | 无 SIP 为 d=0；标准 forward A/B 各自阶数2…9，允许不同阶数、稀疏系数。仅2≤i+j≤order，缺项为0。非有限、非法阶数、低阶 forward 项、无 `-SIP` 却带 SIP 系数返回 `invalid/unsupported`。AP/BP 可作初值，完全不作为误差证书 |
| 天球旋转 | 任意有限 LONPOLE 模360°；缺省按 FITS 的 φp 规则：TAN θ₀=90°，CRVAL2=+90°时为0°，其余合法Dec为180°。入口按下式折进 CD。CRVAL 极点仍保留指定 RA 定义的切基，不从无定义的极点 RA 差判断精度。其他改变投影原生 fiducial 的参数首期不解释；须通过 WCSLIB 解码并证明与规范模型完全等价才接受 |
| 有效域 | 调用者给出闭凸 detector 矩形 U，包含所请求完整像元足迹；不能以少数星点 convex hull 代替。连续包围证明 `sup_U ||q||₂≤tan60°`（即离切点不超过60°）；这是首期保守数值范围，非 TAN 的数学极限。过宽或无法证实返回 `domain_unresolved/unsupported`，不把域外外推静默纳入 |
| 畸变及单射 | PV/TPV、lookup/CPDIS、D2IM/DET2IM、SIP 外的畸变不丢弃，返回 `unsupported_distortion`。整个 U 必须有下述单射证书；失败是 `injectivity_unresolved`。有折叠反例才称 `non_injective`，二者分开 |

令 ρ=(LONPOLE−180°)，\(e_\rho=e\cos\rho-n_N\sin\rho,\)
\(n_\rho=e\sin\rho+n_N\cos\rho\)。则不改变 SIP 变量、CRPIX 或 CRVAL，取
\[
C_{canonical}=\begin{bmatrix}\cos\rho&\sin\rho\\-\sin\rho&\cos\rho\end{bmatrix}C,
\qquad {\tt LONPOLE}_{canonical}=180^\circ .\tag{5}
\]
这是二维基变换的代数恒等式，并非修改天区。保存原头、规范化矩阵及数值误差身份；角度转换/旋转的舍入不得算作零。独立新 fixture 已将非180°、精确双极点与 WCSLIB 对照（§6）。未经证明与式(5)等价的 native rotation 不走此入口。

单射首选用有向舍入的多项式/ Bernstein 导数界计算 \(L=\sup_U\|\nabla D\|_\infty<1\)。更广的充分条件是选择固定可逆 H，证明
\(\eta=\sup_U\|I-HJ_F(u)\|_\infty<1, F=u+D\)：将 HF 写成恒等加压缩扰动即得全局单射，且
\(\|u-v\|_\infty\le\|H\|_\infty\|F(u)-F(v)\|_\infty/(1-\eta)\)。H=J_F(u_c)⁻¹ 可改善强畸变的充分证书。不得把相邻小块各自单射当成全域单射；若选择分片，还需所有片的像无重复及边界一致证明。det J 在采样点均非零也不够：`exp(u)(cos v,sin v)` 在完整2π条带处处 det>0，首尾仍映同一点。

### 3.2 完整微分、面积与连续足迹接口

令 \(F=u+D(u), J_F=I+\nabla D,\ q=CF,\ s=\sqrt{1+q^Tq}, E=[e,n_N]\)。由单位向量归一化的微分直接得到
\[
J_W={\partial n\over\partial(x,y)}={(I-nn^T)ECJ_F\over s}\quad[\mathrm{rad/pixel}],
\quad g=\sqrt{\det(J_W^TJ_W)}=
{|\det C|\,|\det J_F|\over(1+q^Tq)^{3/2}}\quad[\mathrm{sr/pixel^2}].\tag{6}
\]
向量导数实际是 dimensionless/pixel；rad 是角误差的标注。面积是 \(\int_Pg\,du\,dv\)，不取中心 g 乘面积冒充真值。导数及 Hessian 从同一个多项式解析生成；点导数、区间导数、g 包围必须对应同一模型。数值有限差分用于外部验证，不是正式导数或误差界。

矩形边 p(t)=a+tb，t∈[0,1] 代入 CF，给 d 阶一维多项式曲边；输出幂/Bernstein系数、方向、参数域及系数区间，而非只交四个 sky corners。单射保证四边形成 Jordan 曲线；共同边使用相反方向的同一个几何对象。FootprintProvider 对一般复合/缺孔足迹保留各有向环与原生收集区域，几何模块按[真实球面算法](SPHERICAL_DRIZZLE.md)求面积/交叠。点 API 成功不够资格返回 certified footprint。

### 3.3 逆函数的数值停止和存在性

逆投影要求 dc=n·c>0。天空在背半球、horizon 或与其舍入区间相交时分别返回 `outside_tangent_hemisphere` 或 `projection_unresolved`，不使用 abs(dc) 放行。Newton 初值可为线性解或 AP/BP，采用解析 J、残差下降阻尼和有限迭代；不收敛保留状态，可切换有界 interval Newton/区间根隔离。

在全域 L<1 的情况下，计算候选 û 的多项式残差**包围** ε=||F(û)−z||∞。取局部闭球 V⊂U、r>ε/(1−L)，则 T(v)=z−D(v) 满足 `T(V)⊂V` 且为压缩映射，故存在唯一根，距离≤ε/(1−L)。实现同时检查 V⊂U；贴域边界而根盒跨界时报告 `inverse_domain_boundary_unresolved`，不 clamp。预条件证书以 H、η 给对应界。返回根盒、像素误差及 forward sky 残差，不能只以一次步长小停止。

该界条件于目标切平面 z。对天空/旋转/CD 运算引入的 q 区间 δq，另加
\(\|C^{-1}\|_\infty\|\delta q\|_\infty/(1-L)\)；其中
\(\partial q/\partial n=(E^T-qc^T)/(n\cdot c)\)。参考代码把 rounded z 的多项式根界与外部天空总误差分列，没有把前者伪装成总误差。正式 C++ 使用有保证的区间线性求解/三角函数，或将未认证运算明确标为 approximate。真实 WCS 参数估计误差通过共享参数 Jacobian/非线性模拟另传；本节数值逆证明不覆盖模型偏差。

## 4. crop、bin、Bayer 与手性

统一记录 `p_old=M p_new+b`。坐标变换先作用到**连续像元及面积域**，随后调用旧 WCS；图像数值变换的算子另存，以便传播噪声。

| 操作 | M、b 或数值规则 |
|---|---|
| crop 左上 `(x_c,y_c)` | M=I，b=(x_c,y_c)，CRPIX_new=CRPIX_old−b |
| sum-bin `(b_x,b_y)` | M=diag(b_x,b_y)，b=((b_x−1)/2,(b_y−1)/2)+crop_offset；值是覆盖原像元之和 |
| mean-bin | 同一几何，值再除 b_x b_y，方差按平方除，单位说明为平均而非原积分 |
| flip_x | M=diag(−1,1)，b=(width_old−1,0) |
| flip_y | M=diag(1,−1)，b=(0,height_old−1) |
| transpose | M=[[0,1],[1,0]]，新 shape 交换 |
| Bayer 子格 `(a,b)` | `p_old=(2x+a,2y+b)`；采样间隔不等于实际收光区域变成 2×2 原像元 |

多次变换按矩阵复合，不交换顺序。新 CRPIX 满足
\(r_{new}=1+M^{-1}(r_{old}-1-b)\)，`CD_new=CD_old M`，
\(D_{new}(u)=M^{-1}D_{old}(M u)\)。这给出非对称 SIP 的完整基变量变换；只改 CD 不改 SIP 系数一般错误。Bayer/demosaic 是具有不同 response 与相关性的观测算子，不是无损的“换头”。

sum-bin 的真实收集核是各输入 P_i 之和；用大矩形 WCS 只描述覆盖，并不消除不同 PRF、缺像元或读噪次数。硬件 bin 与软件 bin 的读噪模型不同。

特别是 Bayer 子格的 `M=2I` 只表达采样格；它的单个 photosite 仍收集原 detector 的1×1区域，在 channel 坐标对应 `center±.25`，不是 `center±.5`。`DetectorFootprint` 必须显式携带这一面积域；普通原生矩形像元才由整数中心默认 `±.5`。掩掉的 bin 成员产生有缺孔的 footprint/PRF；demosaic 输出的核是原观测核的线性组合，通常不能再解释为互不重叠的新物理像元。

令原观测的重组为 x′=Gx，则真实收集核 P′=GP、C′=GCGᵀ；PRF 相对像内响应必须随面积变量换元，不能只复制一份“新PRF”。sum-bin 的 G 元为1，mean-bin 再除成员数；信号与单位说明是否是积分或平均。软件12像元各读一次且每次读噪方差4时，总方差48；理想硬件先合电荷后单读为4，实际硬件需真实读出电路模型。模型含共享读噪/IPC 时按 C 完整传播。线性去马赛克若输出共用同一原像元，C′非对角；非线性/数据依赖去马赛克须保存实际线性化/模拟条件，不能发出虚假独立噪声。

这里 GP 在一般重采样中是**有效观测核**；G 带负系数时不能再称非负收光概率。mean-bin 若要交给要求积分 x 的 Drizzle 入口，先乘有效成员数还原sum及其平方方差尺度，或使用明确的平均强度入口与对应面积；不能将mean数值和大矩形Ω直接混用。demosaic/G 的复合核不能靠取footprint并集恢复，首期严格路径区分两种维度：物理重组G将n个原生样本变为m个实际积分样本，R_out由m个有效收光域构造，传播为R_out G；一般signed G不自动给出可用于面积归一的物理重组样本，保留原始观测并直接用R_native在原生通道footprint上投影，G派生仅作有条件线性观测视图。不能把leaf×n的R_native右乘m×n的G。

## 5. Export 的坐标与科学量闭环

给定输出 `(width,height)`，将指定中心放在内部 `((width−1)/2,(height−1)/2)`，对应 `CRPIX=((width+1)/2,(height+1)/2)`，奇偶尺寸都成立。指定方向/尺度建立 CD，不再在 Export 局部补偿上游错误。无支持、跨投影无定义域、不可计算位置输出 NaN；合法负值与有限零保留。

正式科学 Export 采用目标像元真实 WCS footprint \(E_i\) 与球面叶的面积交叠 \(b_{ip}=\Omega(E_i\cap H_p\cap U_p)\)，其中 \(U_p\) 是已知支持区域。球面场按叶内常量解释：
\[
 \bar B_i={\sum_p b_{ip}B_p\over\sum_p b_{ip}},\qquad
 F_{i,\rm supported}=\sum_p b_{ip}B_p .\tag{4}
\]
前者是**被支持区域的面亮度平均** [L/sr]；后者是被支持区域的积分 [L]，不自动填充缺覆盖部分。若要求整目标像元通量 `Ω(E_i) Bbar_i`，须明确假定未覆盖部分与已覆盖部分相同，另记外推。仅有全叶 signal、没有精确 subleaf support 时使用声明的 full-leaf/有效面积近似，不能由一个 support 比例恢复真实子叶形状。

`R_export` 对面亮度的行为是式(4)中的归一行，对支持积分是不归一面积行；协方差均按同一行传播。只拿到最终 signal 时不制造完整方差。显示/后期可以另选 nearest 或插值，但记录核及插值目标，不能冒充式(4)的面积积分。几何直接复用本目录的真实曲边交叠定义，将目标 WCS 像元作为 source footprint、HEALPix 叶作为 target，**不做第二次 pixfrac 收缩**。

FITS `BUNIT` 明确对应 L/sr 或 L；非标准模型单位不伪装成标准 SI 单位，附 reference SED、response 和归一说明。单个最终 signal HiPS/FITS 可以附表记录响应、mask 和内部算子身份，不新增虚构物理信号层。HiPS NESTED 的 sky index 与 tile 图像行列是两个对象：tile 宽 2^t 时 leaf_order=tile_order+t；NESTED x 占偶数位、y 占奇数位。采用 IVOA HiPS tile 朝向时明确 `row=W−1−x,col=y`，独立序列化方向验证不能由内部 writer-reader round-trip 代替。其HPX头的CD列分别为 `(-45/N,+45/N)` 与 `(-45/N,−45/N)` degree/pixel；CRPIX由所属tile的face线性offset反解，不套用居中TAN的CRPIX。赤道face6跨全局HPX经度±180中断时，用旋转180°的原生投影中心或合法分片，不能以数组±1补缝。

`export_independent.py` 已实际构造2×2偶数尺寸、CRPIX=(1.37,1.12)、镜像非对角CD及RA回绕，输入真实叶内−2/0/+3的分段常数。四百万独立detector点用外部完整WCS/成员与独立TAN Jacobian积分，对全/部分支持给Hoeffding联合置信界。部分支持像元覆盖约30.08%，正确支持均值−2，误用全像元分母得到−.6015。面积界逐项传播到式(4)，几何中点仅用于固定R的条件covariance实验；同叶重复引用先合并，否则拆.35/.65再假独立会将白噪声方差压到正确值的.545。该参考接口验证不是旧生产Export E2E，初始较紧预算失败与后续实际精度均在证据中说明。

独立 `hips_orientation.py` 由上述HPX头推导直接调用WCSLIB，另用外部HEALPix叶中心对照；12面、Nside=8/512/2¹⁸共3600点，0个无定义值，最大向量角差9.82e−14rad。初次未旋转face6的HPX头出现92个无定义点，失败及修正均保留。该检查证明声明数组/头/天空几何在这些点一致，尚不是第三方真实HiPS生产者互操作或旧writer E2E。

## 6. 坐标证据与使用范围

独立脚本 `coordinates_independent.py` 从式(3)及标准 API 构造真值，并编译未改动的相关生产 TU 探针。9027 个 TAN+2/3/4阶 SIP 测点含非整数 CRPIX、非对角镜像 CD、RA 回绕及双极区；解析向量与 Astropy 最大误差约 `3.63e−10 arcsec`。这仅证明指定数值构型，不是物理 WCS 精度。

IPV 迭代逆接口的 +1 已由 1003 点生产函数探针对照确认；详细位置、调用可达性与 detector 半像元疑点见[独立证据 §3](INDEPENDENT_ORACLES.md)。不把该接口错误扩大成“当前所有 CLI 星位都偏1像元”。crop/bin/flip/transpose 用复合 WCS 对拍，真实 FITS 读写与 HiPS 第三方生产者方向仍需单独验证。

第二轮独立实现 [round2_wcs_contract.py](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/oracles/round2_wcs_contract.py) 不 import 第一轮坐标或几何实现；[实际结果](https://github.com/fujiaze/Astro-CS-Database/blob/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation/evidence/round2_wcs_contract.json) 含完整新头、环境、源码hash及运行数。7个新构型各303个前向点共2121点，CRPIX=(17.293,41.617)、SIP2/3/4/5/7/9、精确双极点、RAwrap、PC+CDELT、非180° LONPOLE、缺省LONPOLE的双极点及0.3°/pixel宽场。10种连续变换（含3×4 bin与四个Bayer相）各200点；不同于第一轮 fixture。

| 本轮独立验证 | 实际结果与证据边界 |
|---|---|
| 自写 forward 对 Astropy7.1.0/WCSLIB8.4 | 最大1.403×10⁻¹⁵rad；2121点，含足迹边缘与参考点；有限测试不是全域机器证明 |
| LONPOLE规范化后外库坐标 | 最大8.429×10⁻¹⁶rad；覆盖107/191/0/270/11°及精确两极 |
| 自写逆对外库世界真值 | 最大2.201×10⁻⁹pixel；rounded z的多项式根界最大8.306×10⁻¹¹pixel，二者不混同；每个内部点的存在/单射充分判据满足 |
| 解析 J_W/g 对外库四阶中心差分 | 最大相对差6.727×10⁻⁸ /1.051×10⁻⁷；差分本身有截断及浮点误差，不能称这些为解析导数的严格误差界 |
| 连续四条边多项式对外库 | 每构型84个点，不以四角替代曲边；完整系数由脚本构造，面积返回自然区间 |
| 无SIP非中心像元面积 | 独立70位球面三角 excess 对照积分及自然区间；SIP的12/20阶Gauss差只作数值诊断，不当认证余项 |
| 10种变换链 | 最大世界差6.343×10⁻¹⁶rad，g换元相对差≤2.590×10⁻¹⁵；Bayer此项仅验证坐标格，收光面积另外定义 |
| 新crop负例 | 不更新CRPIX产生至少8.097×10⁻⁵rad绝对错位；只改bin的CD而不变换SIP最大1.095×10⁻⁶rad |
| 精确北极省略LONPOLE的负例 | 一概默认180°替代标准0°，本构型最大错位6.735×10⁻⁴rad；极点CRVAL本身仍相同，单检查参考点不能发现 |
| 输入负例 | FK5/PV/lookup/非TAN/10阶/过宽域/NaN具名拒绝；已折叠SIP构型正确返回充分证据不足，不冒称已检测全部fold |
| 原生产证据身份 | 三个WCS生产TU字节hash仍与第一轮最小探针一致；本轮只复核身份及源码，不冒称重新执行CLI |

脚本初次运行出现区间端点至NumPy类型转换错误、以及Astropy禁止在FITS头写NaN（负例应在解析前的普通输入映射构造）；两次失败文件保留为 `round2_wcs_failed_*.json`。它们是研究脚本输入/类型失败，未生成科学成功结果；最终数值证据来自修正后完整实跑。

HST coadd 只能作为已知有效 PSF 与分辨率的**条件场景**。若模拟目标仪器 PSF，须从 `P_HST * K=P_target` 求可实现的额外退化核；未证明该卷积关系时不能直接再卷一个“目标 PSF”并称无 PSF 宇宙真值。未知高频不可恢复，真实 M42、DCR、PSF 尾、滤镜角入射和亚像元 PRF 均须实测。

## 7. 重新开发的公共科学接口

本节解释数学对象责任；实际类型与签名由唯一声明合同定义，容器、资源与磁盘协议由工程正本定义。所有实数默认 binary64，角以rad、面积以sr、像素长度以声明的 detector 单位；累计与包含原语另按几何规范决定更高精度/有向舍入。接口不通过隐式double重载转换原点。`Result<T>` 同时返回值、状态与所含数值/物理误差身份，失败时不得给可被误认成功的任意坐标。

字段与实际类型只见[唯一声明合同](../../../include/acsd/contracts.hpp)；本篇保留数学角色、单位与误差语义，不另声明工程结构。

`FootprintProvider` 是 WcsModel 的数值几何能力入口，TAN/SIP是首期具体实现。未来非TAN或lookup模型只有同时实现以下包含接口、给出有效域及全局单射/可分片证据，并由独立Oracle验证后，才可作为新实现注册；仅能点前向的对象只能用于点查询，不能流入严格Drizzle。

创建WCS、坐标原点转换、点/导数/面积/交叠和Drizzle的完整调用仅由声明合同定义；反向原点转换分别是from_fits_center与from_fits_edge。FootprintProvider的包含能力与错误合同见[API](../../engineering/rebuild/API.md)。

每个字段的数学责任与下游约束如下，面积/算子误差的细算由[几何规范](SPHERICAL_DRIZZLE.md)拥有，随机误差action由[统计规范](STATISTICS_SKY_COADD.md)拥有，以下不另定义一套估计量。

| 类型 | 输入/输出与数学责任 | 所有权、寿命、精度与失败 |
|---|---|---|
| PixelCenter0 / PixelEdge0 / FitsPixel1 | 前两者共享连续数轴但角色不同；FitsPixel1只过外部边界。检测/PSF/Gaia预测、孔径及Drizzle均使用同一Center0 | 小值对象按值；非有限拒绝。角色改变显式，不因“边缘”自动+.5；历史sdet的首中心.5属于另一原点，仅说明历史差异；新核心直接产生Center0，不要求旧适配器 |
| SkyICRS | 坐标向量+外附观测历元/传播条件；匹配、WCS、HEALPix唯一公共天空对象 | 按值；检查归一误差并纳预算，零向量拒绝。不能静默把apparent/topocentric与ICRS混同 |
| WcsModel | 拥有规范化系数、原始身份、域和连续单射证书；点/导数/面积共享同一F | 不可变拥有数据，可共享只读。调用不保留临时header指针；域外及不支持模型具名失败，不能丢畸变后降级 |
| DetectorFootprint / FootprintProvider | 前者定义真实收集域与PRF；后者给连续W及包含运算。drop先在声明detector域收缩，源Ω与drop A分别计算 | 拥有rings，model/response引用为有寿命的只读共享句柄。像元改变/缺孔不只改shape；provider无认证能力就拒绝严格交叠 |
| SphericalAreaInterval | 0≤L≤真面积≤U，单位sr；source Ω/drop A/overlap a不可混称area。共同bound/分割约束保留相关性 | 小值+有寿命约束身份；0下界不等于无支持。NaN/负上界/反序拒绝；耗尽仍可保有效bounds |
| HealpixLeaf | order∈[0,29]，0≤ipix<12·4^order，真实标准叶；共同边零测集另有一致tie规则 | 按值uint64，先检查溢出；混order不能悄悄当同一网格；tile数组方向另定义 |
| OverlapContribution | 同source的Ω/A复用，同leaf的a和约束生成w=a/A；保留0≤a≤A≤Ω、Σ_leaf a=A的同源约束；source身份关联signal/noise/PSF | 拥有或共享只读面积证据，禁止把重复source引用当独立观测；资源不足位置随status保留 |
| GeometryBudget / Status | 从εB[L/sr]、εflux[L]、εcov[对应量平方]及孔径functional反推per-leaf绝对面积限额[sr]；停止原因、bounds有效性及支持分类正交 | 请求拥有值/单位/科学functional；不跨调用悬挂span。达到work上限不伪装tolerance_met；科学上足够的未决贡献仍标bounded_undecided；positive但N过小另记weak_support |
| DrizzleOperator | 具体名义R及其允许集合，单位sr⁻¹；同一R作用signal/PSF/noise/basis，附N与原始几何约束 | 拥有CSR/因子及source index，输入span仅借用本次调用。输出行不能只归一中点就丢bounds；无支持行无可用R |
| SignalEstimate | 具名signal、响应、支持、单位及数值包围；面亮度[L/sr]与支持积分[L]分开 | 拥有数组/区间，缺noise仍可出signal；unsupported/未收敛不填0，真实0/负值保留；field bounds不声称联合独立 |
| NoiseModel | 独立创新、共享母版/Gaia/sky参数及同数据影响组成同一个随机变量模型 | 所有因子/算子链有拥有寿命；shared_id相同即同误差，不按frame重置。未知项保留，非零影响触及未知项即full variance未知 |
| CovarianceAction | `v→L C₀ Lᵀv`、局部/低秩action及孔径quadratic form，避免N²存储 | 拥有模型/算子依赖，按请求流式取块；只读可并发、scratch由调用者拥有，无隐藏可变全局缓存。截断/近似有显式action界，无界则approximate；同数据sky须含交叉项 |

这些接口以独立合成观测即可复现：提供上述输入头/连续域/footprint、具名线性signal与已知创新噪声，验证forward绝对坐标、真面积包围、同一R和CovarianceAction，而无需理解旧调度器。真实HST/M42需要验证的物理模型仍如§1、§6及另外两分册所列。

## 8. 一手依据

1. [FITS 4.0，§3.3.2、§8.1–8.2、式(9)–(10)](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf)：整数数组索引与像元中心、WCS 级联；采用其一基外部定义。
2. [Greisen & Calabretta 2002，§2，DOI 10.1051/0004-6361:20021326](https://arxiv.org/abs/astro-ph/0207407)；[Calabretta & Greisen 2002，§2.2–2.3、§5.1.3](https://arxiv.org/abs/astro-ph/0207413)：TAN、天球旋转及精确北极LONPOLE缺省例外；采用成熟 WCS，不自行发明天球坐标。
3. [SIP registered convention，§2–3、式(1)–(6)及§6](https://fits.gsfc.nasa.gov/registry/sip/SIP_distortion_v1_0.pdf)：CRPIX 相对基变量、先 SIP 后 CD和近似逆误差限制。
4. [Astropy WCS conventions](https://docs.astropy.org/en/stable/wcs/index.html)：完整 WCS 外部数值对照，不能用自写正逆互相证明。
5. [IVOA HiPS 1.0，§3–4](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)：层级及 tile 序列化；不等价于科学面积算法已验证。
