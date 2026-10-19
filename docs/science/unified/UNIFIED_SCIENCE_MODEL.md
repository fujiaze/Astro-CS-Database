# ACSD 统一科学模型｜观测、精度、球面重建与最终产品

本分册依据[最高设计](../../ACSD_DESIGN.md)，统一观测量、条件精度、球面算子和最终产品含义。专题推导由[Normalize](../normalize/NORMALIZE_SCIENCE.md)、[噪声](../noise_snr/NOISE_SNR.md)、[Drizzle](../drizzle/DRIZZLE.md)与[背景](../sky/UPM.md)承接；实现与证据状态见[科学实现差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 系统边界与用户选择

ACSD 有三个命令：
1. normalize：原始 CCD/CMOS 观测校准、Gaia XP 参考测光、噪声与 PSF、稀疏参考 SNR 测量、球面 Drizzle，输出**每帧 HiPS 信号及其用于阶段二的辅助数据**。
2. mosaic：在同一 HEALPix NESTED 球面网格上，重建稠密参考精度场、计算科学叠加权重、相对背景与通带差处理、排异和合成，**最终只发布一个 signal 科学图像子产品**，连同标准 HiPS 必需目录/元数据以及运行摘要。中间的支持度、方差、权重和控制点不成为最终科学图像层。
3. export：读取兼容的单信号 HiPS 产品，按目标 WCS 重投影并输出**普通二维 FITS 信号图像**，供后期处理。不得声称从单信号图可恢复已经丢失的独立方差或精确 PSF 协方差。

输入可由用户任意组合不同相机、望远镜、曝光、滤镜及通带宽度。**禁止以设备/通带一致性设项目级拒绝条件**。兼容范围描述的是能够运算，而不是物理上能够无偏地变换为同一通带辐射场。质量、误差和失配信息以诊断/元数据如实揭示。不建立审批式或测试通过率式“门禁”；仍应检测数据损坏、非法几何和不可计算数值并给出错误。

## 2. 单帧前向观测模型

第 k 帧原始传感器像元 i 的期望值是

\[
 E[d_{ki}]=b_{ki}+\int_{\lambda}\int_{\Omega}
 L_{\lambda}(\omega)\,R_k(\lambda,\omega)\,
 P_{ki}(\omega,\lambda)\,d\omega\,d\lambda .
\]

其中 b 是加性天空/仪器背景，R 包含望远镜、滤镜、QE、大气、曝光与电荷换算的综合响应，P 是到探测器像元 i 的完整光学/像素积分核。未知仪器分量通常只以乘积可辨识；星表零点不能分别反演口径、增益和大气消光。观测噪声 n=d−E[d] 的条件协方差 C 包含散粒噪声、读噪、校准母版传播及其它已测项。

设校准后单个平面像元携带积分线性信号 x_j（在选定统一标度下表示该源像元收集的量），像元真实球面面积为 Ω_j [sr]，则其面亮度 B_j=x_j/Ω_j [所选线性单位/sr]。若标定尚为 ADU，则 B 的量纲为 ADU/sr；若 Gaia XP 标定应用后成为模型通带光度单位，不能继续无条件把它称为 ADU/sr。FITS BUNIT 与具体标度来源必须一致。

不可把星等、SNR、像元计数与面亮度互相当作相同对象；显示像素值可为负（扣除加性天空之后），不得默默截断为零。


### 2.1 R1–R5 共同的前向算子与估计量（2026-10-19 综合收敛）

以真实天体光谱辐亮度 \(L_\lambda(\omega)\) [W m⁻² sr⁻¹ m⁻¹] 为起点，其中 \(\omega\) 是 ICRS 球面方向，\(\lambda\) 采用 SI 米。第 k 次曝光、原生像元 i 的理想 ADU 观测为
\[
d_{ki}=\mathcal H_{ki}[L_\lambda]+b_{ki}+\epsilon_{ki},\quad
\mathcal H_{ki}[L_\lambda]=
\int_{\lambda}\int_{\mathbb S^2}L_\lambda(\omega)
{\lambda\over hc}\,
{t_k A_{{\rm eff},k}(\lambda,\omega)T_k(\lambda)QE_k(\lambda)T_{{\rm atm},k}(\lambda,\omega)\over g_k}
P_{ki}(\omega,\lambda)\,d\Omega\,d\lambda .
\]
\(t\)[s] 为曝光，\(A_{\rm eff}\)[m²] 是有效集光面积，\(T,QE,T_{\rm atm}\) 无量纲，\(g\)[e⁻/ADU]，\(P_{ki}\) 是含 WCS/SIP、色相关 PSF、探测器像元积分的收集核，采用能使该积分输出 ADU 的归一规范。\(b_{ki}\)[ADU] 是额外天光/暗电流/电子偏置均值，必须区分读出前有 photon shot noise 的天空和纯电子均值；\(\epsilon\) 保留源/天光 Poisson、读噪、master 及校准残差。已经 bias/dark/flat 校准的输入须使用相应的条件观测和噪声模型，而不是二次扣除。未知口径/gain/大气只可由 Gaia 恒星经验拟合乘积，不能虚构各因子物理真值。

对线性像元 \(x_k=A_{\alpha,k}(d_k-\widehat b_k^{\rm instrument})\)[L]，原生总通量与球面面亮度经严格 Drizzle \(y_k=R_kx_k\)[L/sr] 连通。\(R_{kpj}=(a_{kjp}/A_{kj})/N_{kp}\)[sr⁻¹]，\(N_{kp}=\sum_j(a_{kjp}/A_{kj})\Omega_{kj}\)[sr]；这是**源积分量到目标面亮度**的定义，不能把结果继续叫积分总通量。对控制背景的单独观测模型 \(y_k=H_kS+A_k^{\rm sky}\beta_k+\varepsilon_k\)，仅识别在可比响应组内的相对加性差模；校正 \(z_k=y_k-A_k^{\rm sky}\widehat\beta_k\)。传统最终合成 \(\widehat m_p=\sum_k q_{kp}(z)\,z_{kp}\)，若 q 含排异/数据驱动权重，整个算法是非线性估计量，不能套固定矩阵的全误差公式冒充完整选择后方差。

固定几何/掩膜/参数条件下 \(C_y=R A_\alpha C_d A_\alpha^TR^T\)。标定、天光参数与源图像有共享观测时，需令 \(\zeta=(d,\widehat\theta_{\rm Gaia},\widehat\beta_{\rm sky},\ldots)\)，按 \(J_\zeta\operatorname{Cov}(\zeta)J_\zeta^T\) 传播，包括两两交叉项；额外 WCS/响应/PSF 偏差分别作为模型系统误差，不当作 iid photon variance。点源 Fisher \(V_F\)、球面场景方差 \(V_B\)、背景控制统计方差 \(V_{\rm ctrl}\) 是三种不同估计量，不共享无语义的 precision 字段。

同通带/兼容空间算子下可针对同一面亮度目标作 BLUE/GLS。异滤镜或不同 PSF 下，一般输出是观测算子的**用户定义混合**：
\[
K_{\rm eff}(p,\omega,\lambda)=\sum_k q_{kp}K_k(p,\omega,\lambda).
\]
仅在固定选择/固定权重并且忽略核差异的特定近似下，才能写成位置相关 \(R_{\rm eff}(\lambda,p)=\sum_k q_{kp}R_k(\lambda)\)；不能把任意星云的真实光谱压为一个未证明的共同标量天空。同阶 HEALPix 是 Mosaic 的输入前提：Normalize 全部投到用户选定的阶数，Mosaic 检出混阶应拒绝，不在此层实现混阶 remap。


## 3. 跨设备光度：参考通带与不可辨识部分

给每次 mosaic 定义**参考响应或混合响应规则**。R_0(λ)可以是选定设备响应或具名标准响应；相对参考若无已知曲线，不能把该名称当作已知R_0。Gaia DR3 XP 给出合格恒星的校准光谱 f_{λ,s}；合成参考量按同一响应与光子/能量计数约定计算：

\[
 q_{ks}=\int f_{\lambda,s}(\lambda)R_k(\lambda)\lambda\,d\lambda,\qquad
 q_{0s}=\int f_{\lambda,s}(\lambda)R_0(\lambda)\lambda\,d\lambda.
\]

上式中的 λ 因子适用于以光子计数为响应的约定；若响应已包含 λ/(hc)，禁止重复乘。零点及空间乘性场由合格的非饱和匹配恒星拟合，施加时线性值与随机协方差须分别按一、二次幂变换。用于近似通带合并的颜色项应明确其训练恒星 SED 范围、目标通带、系统残差及支持域。

对于任意天空位置的光谱 L_λ(ω)，一般有
\[
 \int L_\lambda R_k\,d\lambda\neq c_k\int L_\lambda R_0\,d\lambda .
\]
因此恒星色项校准**不能**保证 Hα/[N II]/[S II] 等窄发射线星云或极端颜色源在不同通带上无偏。用户始终可以合并，算法不得伪造等效性；若无源的光谱信息，最终信号应标为“在指定规则下的混合/参考响应信号”，其物理绝对解释条件受限。禁止让背景面把通带差引起的真实结构差消掉。

跨设备不同曝光、相机 QE、成像面积与 PSF 的作用必须由统一标度、噪声模型和源响应分别承担，不应通过人为“同设备奖励权重”或“不同设备惩罚权重”替代统计估计。

## 4. 噪声、系统误差与被估计量分离

要分开至少四个对象：

| 对象 | 数学定义/单位 | 目的 |
|---|---|---|
| pixel/surface variance V_{B,k}(p) | 同一球面面亮度估计的方差，[B]² | 阶段二合成信号的统计权重 |
| reference flux variance V_{F,k}(u;F_ref) | 固定 PSF、光度系统、参考源通量和 nuisance 假设下的通量估计条件方差，[F]² | 阶段一稀疏 SNR 的科学基础 |
| reference SNR S_k(u) | F_ref,k / sqrt(V_F,k(u;F_ref))，无量纲 | 可跨位置解释的参考深度与空间精度摘要 |
| background-control variance V_{b,k}(u) | 特定背景采样统计量的方差，[B]² | P5 加性背景拟合与重叠图解算 |

**SNR²/F_ref²=1/V_F 是代数恒等式，只对同一通量估计量成立**。不能把它未经论证地当成 V_B 的逆，也不能把背景中位数的 control_ivar 当成点源信息量。

从完整仪器与重采样模型可推导 V_B 与 V_F 的**精度传递因子** T_k(u)：
\[
 T_k^{(c)}(u) \equiv V_{B,k}^{(c)}(u)/V_{F,k}^{(c)}(u),\quad
 w_{B,k}^{(c)}(u)=1/V_{B,k}^{(c)}(u)=\big(S_k^{(c)}(u)/F_{\rm ref,k}\big)^2/T_k^{(c)}(u).
\]
c明确标识reference-source、actual-scene或background-only条件。Canonical控制点的参考源泊松项与真实场景不同，不能直接用T_ref换算实际场景权重；必须重求场景条件或验证背景主导近似。T 有面积/PSF 相关量纲，须由当前像素响应、重采样算子、有效 PSF、背景消除方式与相关噪声等计算或外部标定；不得默认为 1。当两类方差计量的单位不同，T 的单位必须显式携带，使 w_B 单位为 1/[B]²。此桥梁的具体求值与退化策略见 NOISE_SNR.md。

随机 C_random 与校准零点、乘性响应误差、背景拟合和通带混合误差 C_systematic 分开描述。光度零点是跨像素相关的共同参数，不可像像素白噪声一样按独立项逐像素相加。测得的点源 scatter 与解析条件噪声不符时，不得直接把差异归罪于随机泊松噪声。

空间标度下分别命名原生总通量F_native、位置参考参数F_ref(u)=α(u)F_native和实际积分总通量F_applied,total=F_native Σ_all α_jP_j。正式参考模板h_ref,j=(α_j/α(u))P_j，球面模板R h_ref；h_ref完整积分一般不为1。F_ref、V_F与SNR配对，不能把位置参数叫实际总通量；两种确定性参数化换算、源Poisson及有限窗定义由[噪声分册](../noise_snr/NOISE_SNR.md)拥有。

## 5. 球面 Drizzle 与关联噪声

线性重建的相关噪声见[1]，HEALPix的曲边与等面积性质见[2]。

对源像元 j 缩小后的 drop 区域 D_j，真实目标 HEALPix 单元 P_p，定义交叠立体角 a_{jp}=area(D_j∩P_p)、A_j=A_drop,j=area(D_j) 与 Ω_j=未收缩源像元面积。几何核 w_{jp}=a_{jp}/A_drop,j，输出的面积归一量 N_p=Σ_j w_{jp}Ω_j，得到

\[
 F_p=\sum_j w_{jp}x_j,\qquad
 S_p=F_p/N_p=\frac{\sum_j a_{jp}(\Omega_j/A_j)B_j}{\sum_j a_{jp}(\Omega_j/A_j)},
 \qquad c_{pj}=w_{jp}/N_p .
\]

这里 F_p 是 drop 分配的累加中间量；严格的每源像元分配守恒是 Σ_p w_jp=1（当候选单元完整且所用几何精确）。**不得把 Σ_p S_p Ω_p 等同于 Σ_j x_j，除非另行证明覆盖与归一条件**；覆盖重叠时这一等式通常不成立。信号常数场保持、drop 完整分割、面积和、点源总通量是不同实验目标。

\[
 C_{\rm out}=R\,C_{\rm in}R^T,\quad
 V_p=\sum_{ij}c_{pi}C_{ij}c_{pj},\quad
 {\rm Cov}(S_p,S_q)=\sum_{ij}c_{pi}C_{ij}c_{qj}.
\]

若输入独立，才简化为 V_p=Σ_j c_pj²v_j。**输出相邻像元通常相关**；像素看似更平滑、单像元 RMS 更小不等于孔径测光 SNR 同比例提高。科学诊断需考虑相关噪声、空间功率谱与有效 PSF。

HEALPix NESTED 的每个 leaf 等面积 Ω_p=4π/(12 nside²)，但其真实曲边一般不等于连接四角的大圆弧。四角球面多边形及切平面近似都属于近似求积，须用独立 HEALPix/chart oracle 界定几何误差。HiPS tile 行/列与 NESTED 局部位序必须由**唯一共享坐标转换**定义，并对全部 12 面及极区交叉对拍。

## 6. 稀疏参考 SNR → 阶段二现场精度

点源检测/测光的最优组合需保留PSF与统计假设[3]；以下面亮度合成是不同的目标量。

阶段一保留每帧参考通量、光度尺度、PSF/NEA、条件噪声约定、球面坐标与有效支持域，并在稀疏点测量 S_k(u)。阶段二在输出位置现场重建稠密正精度场，优先研究 log V_F（以及 log SNR 对照），而不是给所有插值结果同样的统计解释。

不可把 SNR 的空间插值误差当成新的独立像素噪声；这是**估计模型误差**。需要显式支持域、孔洞、空间交叉验证、外推/退化读数，以及从 V_F 到 V_B 的精度桥梁 T。自然三次样条不保证形状保持，值域裁剪可能有偏；IDW指数与其它模型参数由独立留出误差和最终估计偏差共同确定。

在不同帧观测同一经标度定义的面亮度、无额外模型失配、且各帧随机误差相互独立时，
\[
 \widehat B_p=\frac{\sum_{k\in E_p}w_{B,kp}\,\widetilde B_{kp}}
 {\sum_{k\in E_p}w_{B,kp}},\quad w_{B,kp}=V_{B,kp}^{-1}.
\]
E_p 是已排除异常输入后的合格样本集合。如果存在跨帧共享校准误差或相关项，则一般需 GLS 权重 C^{-1}1/(1^T C^{-1}1)；仅知道边际逆方差不足以保证最优。鲁棒排异使估计器非线性，方差不能一律等于 1/Σw。严格条件与数值实验见 NOISE_SNR.md。

## 7. 背景、接缝与真实天体结构

对已归一的同一位置 p，第 k 帧可用
\[
 y_k(p)=B(p)+a_k(p)+\Delta^{\rm band}_k(p)+
 \Delta^{\rm psf}_k(p)+\eta_k(p)B(p)+n_k(p)
\]
表述差异：a 是加性背景差，η 是残余乘性响应，Δ_band 是未知 SED 下的通带混合误差，Δ_psf 是采样/卷积产生的差分项。**不允许预设全部差异都是加性天光。**

相对加性场由真实重叠图约束，连通分量独立固定规范；没有外部绝对背景锚时，连通分量间低频零点不可辨识。P1 的空间光度修正不得在 P5 无根据重复执行。乘性残差须在独立留出恒星与颜色/PSF 分层对照支持时才有物理意义。接缝变小不代表天体结构更准确；必须同时检查背景保真、弥散源总通量、低频功率、星点光度和未评估的边界。详见 UPM.md。

## 8. 阶段二单信号产品与阶段三含义

最终 mosaic HiPS：一个科学图像子产品 signal；内部 sample support/variance/ivar/rejection 不属于最终图像层。对无数据的 leaf 用不与零亮度混淆的明确无效表示（浮点 FITS 可使用 NaN）；文件标准要求的 HiPS properties、目录层级、WCS/坐标与测光响应声明保持完整。HiPS 层级的派生值应按面积与统计目标定义；粗尺度不是新观测，也不是重新生成一套独立白噪声。

export 是既有单信号 HiPS 的平面重采样，不从单图像恢复不存在的 covariance、SNR 或仪器历史。不应把输出图像的平滑度误当成噪声物理标定。一般显示和后期模式可选择受控插值，科学评估须测试额外重采样的有效 PSF、孔径通量和噪声相关性。最终FITS BUNIT继承实际输入HiPS的有效光度量纲与响应状态。

## 9. 条件信息与科学证据

Normalize采用固定参考源条件下的GLS方差。协方差半正定时区分观测子空间、精确约束与参数可辨识性；冗余背景参数不自动使通量不可估计。满列秩R并且模板/背景/协方差经同一算子传递时，条件信息保持；降采样与选择一般不保持。推导见[噪声分册](../noise_snr/NOISE_SNR.md)。

独立证据分别回答解析量纲与恒等式、数值求积误差、随机覆盖率、真实输入适用域和生产实现是否命中；局部算术正确不等于真实全链完成。实现与运行事实由[科学实现差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)维护。

## 10. 参考文献

[1] Fruchter, A. S., Hook, R. N. Drizzle: A Method for the Linear Reconstruction of Undersampled Images. PASP, 114, 144–152. [原文](https://arxiv.org/html/astro-ph/9808087v2)。

[2] Górski, K. M. et al. HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere. ApJ, 622, 759–771. [论文](https://arxiv.org/abs/astro-ph/0409513)与[官方几何说明](https://healpix.sourceforge.io/html/intro_Geometric_Algebraic_Propert.htm)。

[3] Zackay, B., Ofek, E. O. How to Coadd Images? I. Optimal Source Detection and Photometry Using Ensembles of Images. ApJ, 836, 187. [原文](https://arxiv.org/pdf/1512.06872)。
