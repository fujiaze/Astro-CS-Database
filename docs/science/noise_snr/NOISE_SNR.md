# 噪声、条件参考 SNR 与面亮度精度

本分册从[统一科学模型](../unified/UNIFIED_SCIENCE_MODEL.md)推导 Normalize 参考精度及其在 Mosaic 中的适用条件。数据表达见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)，生产实现与验证状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 被估计量与观测域

局部观测写为
\[
 y=Fh+Z\beta+n,\qquad E[n]=0,\qquad \operatorname{Cov}(n)=C.
\]
平面域的y是像元积分量；原生总通量参数对应h=P，空间光度应用后的正式位置参考参数对应h=h_ref（第2节）。球面域的y是面亮度，h_sphere=R h_ref的单位为sr⁻¹。F始终与h配对，C的单位为y单位平方，Z的每列注明背景参数单位。把平面P直接放入面亮度模型，会漏掉像元面积与重采样响应。

P 来自像元积分 PSF，而不是单凭 FWHM 或像元中心取样。总通量定义下完整响应满足 ΣP=1；有限测量窗口允许 ΣP<1，表示窗口只收集总源通量的一部分。不能缩小窗口后重新归一 P，却沿用总通量 F 的名称。背景常量在面亮度域的模板是 1；若源域是积分量，其对应列是 Ω_j。源域常量计数模板经 R 后一般不是常量面亮度，两个背景模型不能混称。

参考源、实际场景、背景控制量分别定义：

| 对象 | 定义 | 单位 |
|---|---|---|
| reference flux variance | 固定参考源及模板、背景自由度、噪声条件下的 V_F | [F]² |
| reference SNR | F_ref/√V_F | 1 |
| actual-scene surface variance | 实际场景中指定叶的 V_B | [B]² |
| background-control variance | 指定 patch 位置统计量的 V_ctrl | [B]² |

source_snr 使用实际源的通量与误差；frame_snr_ref 是空间参考精度的摘要；两者都不替代逐位置 canonical controls。

## 2. 探测器、校准与光度标度

在电子计数的条件独立模型中，像元方差为
\[
 V_{e,i}=\mu_{\mathrm{src},i}+\mu_{\mathrm{sky},i}
          +\mu_{\mathrm{dark},i}+RN_{e,i}^2.
\]
泊松均值为非负期望电子数，不能直接采用含电子偏置的 raw 值或扣天空后为负的信号。原生增益 g 的单位为 e⁻/ADU，故 V_ADU=V_e/g²。经验空背景方差若已含读噪、天空和暗流，不再重复加入这些项。有效 gain 的数组单位语义可与 Photutils 的源泊松计算对照[3]。

校准母版、平场归一、修复插值与光度参数通过相应算子或雅可比传播，保留共享来源。确定性空间标度 x'=D_αx 给出 C'=D_α C D_αᵀ；由同一数据估计的 α 还引入参数协方差和交叉项，见[测光分册](../photometry/PHOTOMETRY.md)。缺项记为未建模，不能自动当作零。

### 位置参考参数与实际积分总通量

设原生解析总通量F_native，单位总通量完整模板P（Σ_all P=1），参考源位置u的实际声明标度α_*=α(u)>0。正式controls采用**位置参考幅度参数**
\[
 F_{\rm ref}(u)=\alpha_*(u)F_{\rm native},\qquad
 h_{{\rm ref},j}(u)=\frac{\alpha_j}{\alpha_*}P_j(u),\qquad
 E[y_j]_{\rm src}=F_{\rm ref}h_{{\rm ref},j}.
\]
α_j与实际图像逐像元应用一致，h_ref无量纲；P仍按原生analytic_total归一，**h_ref的完整积分一般不是1**。F_ref具有applied积分量的单位，但其语义是位置标度幅度，不称为applied图像实际积分总通量。有限窗只截取该模型，Σ_window α_jP_j只作诊断，不参与重新归一。α_*连续求值与最近像元近似分别标状态，不把后者称一般空间场精确值。

实际applied源图像积分总通量是另一量：
\[
 a_{\rm all}(u)=\sum_{j\in\text{完整响应域}}\alpha_jP_j(u),
 \qquad F_{\rm applied,total}=a_{\rm all}F_{\rm native}.
\]
完整域a_all正有限且可计算时，另定义P'_j=α_jP_j/a_all、Σ_all P'=1，F_applied,total P'表示同一均值。这是另一参数化；c=a_all/α_*固定已知时，F_applied,total=cF_ref、P'=h_ref/c、V_total=c²V_ref，配对SNR相同，I_total=I_ref/c²、T_total=T_ref/c²。α或c有估计误差时还须传播参数与交叉项。

常量α或恰满足ΣαP=α_*时两种通量相等；一般梯度/曲率与不对称P不保证相等。完整域不可评估不阻止计算局部位置参考条件精度，但不得虚构F_applied,total；需要全域积分时另记缺项、支持和尾部误差。analytic_total限定原生P/F_native，不能把F_ref改认作实际applied总通量。

在原生gain g_j（e⁻/原生计数）及相应Poisson模型成立时，源项为
\[
 V'_{\mathrm{src},j}=\alpha_j^2F_{\rm native}P_j/g_j
 =\frac{F_{\rm ref}\alpha_j^2P_j}{g_j\alpha_*}
 =\alpha_jF_{\rm ref}h_{{\rm ref},j}/g_j.
\]
标量α时退化为αF_ref P/g；直接用F_ref P/g少一次标度。实际总通量参数下同一式写α_jF_applied,total P'_j/g_j，不改变物理方差。有效gain g_j/α_j只表示数组单位，不覆盖设备原生gain来源；平场或修复后先从正确原生模型传播，不能无条件套用未校准探测器公式。

## 3. GLS 与通量可辨识性

当 C 正定、背景已知且模板固定时，最佳线性无偏通量估计及方差为
\[
 \widehat F=\frac{h^TC^{-1}y}{h^TC^{-1}h},\qquad
 I_F=h^TC^{-1}h,\qquad V_F=I_F^{-1}.
\]
若同时估计背景，令 A=[h,Z]。满列秩时
\[
 \widehat\theta=(A^TC^{-1}A)^{-1}A^TC^{-1}y,
 \quad V_F=[(A^TC^{-1}A)^{-1}]_{FF}.
\]
这些是固定设计、固定协方差下的线性估计结果；最优检测/测光还取决于 PSF 与观测响应[4]。

冗余背景列并不自动使 F 不可估计。白化后令 h_w=C⁻¹ᐟ²h、Z_w=C⁻¹ᐟ²Z，Π_Z=Z_w Z_w⁺ 为背景列空间的正交投影，则
\[
 I_{F\mid\beta}=\|(I-\Pi_Z)h_w\|^2,\qquad V_F=I_{F\mid\beta}^{-1}.
\]
只要该信息量为正，F 可辨识；可先去除冗余背景基再求解。等价地，M=AᵀC⁻¹A 与选取 F 的向量 e_F 满足 e_F∈range(M) 时，V_F=e_FᵀM⁺e_F。不能只取伪逆的 FF 元而不检查可估计性。例如 h=(1,0,0)ᵀ，Z 两列为 (0,1,1)ᵀ 与 (0,2,2)ᵀ，C=I，M 秩亏但 V_F=1；若 h 本身在背景列空间，通量不可辨识。广义逆的代数依据见[1]，白化后求伪逆与参数秩可对照 Statsmodels 实现[2]。

当 C 随 F 变化时，本项目在 F_ref 处冻结 C_ref，报告该条件模型的 GLS 方差。它不是任意亮度下的无条件 Fisher 下界。真正的阈值深度 F_q 满足 F_q/√V_F(F_q)=q；仅在 V_F 对 F 近似不变时才能写 F_q≈q√V_F(F_ref)。

## 4. 半正定协方差、观测秩与参数秩

对 C=U_rΛ_rU_rᵀ 的正特征子空间，白化观测为 Λ_r⁻¹ᐟ²U_rᵀy，模板和背景必须施加同一变换。显著负特征值意味着模型不是协方差，不能直接截掉后宣告有效；微小数值负值的容忍度由单位归一、矩阵尺度和分解残差解释。

当 A 的列均位于 range(C)，噪声零空间不携带参数依赖，使用 C⁺ 与上节可辨识性条件即可。一般情形还需保留精确约束 U_0ᵀy=U_0ᵀAθ；直接伪逆会丢弃这些约束。例如 C=diag(1,0)、h=(0,1)ᵀ 的第二个观测精确确定 F，不能由 hᵀC⁺h=0 判定无信息。此类确定性约束需单独求解；不支持时明确报告模型能力，不加任意对角地板。

协方差秩、参数信息矩阵秩和 F 的可估计性分别记录。参数列量纲不同，应在可解释的尺度归一后判数值秩。数值删去小奇异值等于改变保留的观测模型，其误差不属于纯粹舍入。

## 5. Drizzle 的条件信息传递

Drizzle的相关噪声与重建背景见[5]。令源域正定协方差为 V、设计矩阵为 A_src，球面信号算子为 R。使用同一局部观测域与完整随机贡献，有
\[
 C=RVR^T,\qquad A_s=RA_{\rm src}.
\]
定义 Q=RV¹ᐟ²，紧 SVD 为 Q=U_rΣ_rW_rᵀ，则
\[
 A_s^TC^+A_s
 =A_{\rm src}^TV^{-1/2}W_rW_r^TV^{-1/2}A_{\rm src}.
\]
W_rW_rᵀ 是源白化空间的投影。R 满列秩时它为 I，得到
\[
 A_s^T(RVR^T)^+A_s=A_{\rm src}^TV^{-1}A_{\rm src}.
\]
这是保留信息的条件恒等式，球面过采样不创造信息。R 秩亏时信息矩阵在半正定次序下不增加，但与保留子空间对齐的参数信息仍可完全保留。每行、每列都有非零贡献并不证明满列秩。

若 N 是 Q 的零空间正交基，令 W=V⁻¹ᐟ²A_src，则同一信息矩阵也可写为
\[
 M=W^TW-(N^TW)^T(N^TW).
\]
这个式子允许在零空间较小时作低维修正，但求 N 本身仍需可靠分解；两个接近矩阵相减时也需评价消去误差。更一般的正定平面协方差可先分解 V=LLᵀ，使用 Q=RL、W=L⁻¹A_src。未知相关项不能因采用因子表达就被当作已建模。

实现可对 Qᵀ 作列主元 Householder QR，取得保留行空间，再将白化源列和背景列投影到该空间。数值截断定义了实际保留模型：QR 残余列范数、奇异值与 Gram 特征值是不同尺度，不能直接复用一个数值阈值并声称截断等价。若按奇异值截断，相应 Gram 特征值阈值应取其平方；QR 主元阈值是否产生相同子空间还需分解误差和独立 SVD 对照。

对已保留子空间中的通量列 t 和背景列空间，优先显式形成残差 e=(I−Π_Z)t，再计算 I_F=‖e‖²；近共线时直接以 ‖t‖²−‖Π_Zt‖²相减可能消去有效信息。背景有效秩、通量残差、观测秩和阈值分别报告。QR 主元范数比的平方只是指定方法的条件估计，不是协方差的谱条件数，也不自动构成保守上界。

当前complete_spherical_reference的常规QR路径已用两轮背景投影形成e，计算i_f=Σe_i²，旧t_norm2−proj消去缺陷已修复；旧构型保留为历史数值反例。当前接受条件i_f>rel_tol·‖t‖²对应残差范数比>√rel_tol，不能和QR列范数的rel_tol门限称为同一截断；该条件仍不是误差证书。两轮重投影降低舍入残留，不自动证明任意近退化处满相对精度，后续界仍需实际d、γ与独立SVD证据。

源空间 Gram 写法令 G=RᵀR、H=V¹ᐟ²GV¹ᐟ²、B=V¹ᐟ²GA_src，则
\[
 C^+=Q(H^+)^2Q^T,\qquad M=B^T(H^+)^2B.
\]
V¹ᐟ²与G一般不交换。此式用于推导和对照；Gram 运算会平方 Q 的条件数，数值实现优先考虑尺度合适的秩揭示分解。取 R=((0.8,0.2),(0.2,0.8),(0,1))、P=(0.6,0.4)、V=diag(1,9)，已知背景时 V_F=45/17。它同时检验异方差和矩阵乘法次序，不代表生产运行结果。

R 必须是最终 signal 的 w_jp/N_p，不能用尚待除以覆盖面积的中间载荷系数；N_p 来自同次生产的全部有效贡献。局部窗口遗漏对观测叶有贡献的随机源时，不能继续称完整 C。曲边与归一条件见[Drizzle 分册](../drizzle/DRIZZLE.md)。

### 5.1 数值误差怎样传到参考信息

在固定 m 维源白化空间，令w=V⁻¹ᐟ²h_ref、B=V⁻¹ᐟ²Z_src、J 为 Q 行空间投影。背景投影必须取 Π=Π_range(JB)，残差算子为 E=(I−Π)J=J−Π；它是正交投影，I_F=‖Ew‖²。不能用未投影背景 range(B) 代替 range(JB)。仅有很小的 Q 扰动不足以保证信息相对稳定：删去小奇异方向可丢掉全部通量信息，即使观测秩稳定，通量接近投影背景也会令相对误差放大。

若已证明 ‖Ê−E‖₂≤ε，固定 w，记 a=‖Ew‖₂、d=ε‖w‖₂，则
\[
 (\max(0,a-d))^2\le\widehat I_F\le(a+d)^2,
 \qquad |\widehat I_F-I_F|\le2ad+d^2.
\]
当 η=d/a<1，固定非零 F_ref，有
\[
 (1-\eta)^2\le\widehat I_F/I_F\le(1+\eta)^2,
 \quad (1+\eta)^{-2}\le\widehat V_F/V_F\le(1-\eta)^{-2},
 \quad 1-\eta\le\widehat{SNR}/SNR\le1+\eta.
\]
实际 d 必须包含分解、投影、模板和浮点计算误差；小矩阵残差或 QR 主元比不自动给出该证书。若算得残差范数 â 且真实范数误差不超过 d，只有 â>d 才能据此确认信息正值，再由区间核对目标相对精度。秩阈值、科学可辨识性和允许的数值误差分别定义。

模板误差在固定正定 C 与背景 H 下令 K=(I−Π_range(C⁻¹ᐟ²H))C⁻¹ᐟ²；h_sphere=R h_ref时可用d≥‖KRδh_ref‖₂。真实逐像元误差界 e 给出 d≤‖KR‖₂√N e，N 为输入像元数；相邻求积阶数差不是这样的 e。若冻结源泊松项也随 P 变化，还需在共同正定支持上控制 ‖C⁻¹ᐟ²(Ĉ−C)C⁻¹ᐟ²‖₂≤γ<1，方可得到
\[
 \frac{(1-\eta)^2}{1+\gamma}
 \le\widehat I_F/I_F\le
 \frac{(1+\eta)^2}{1-\gamma}.
\]
该界由 I(h,C)=min_β(h−Hβ)ᵀC⁻¹(h−Hβ) 和协方差 Loewner 界推得；方差比取倒数，SNR 比取平方根。秩/支持改变须单独处理。这里约束条件信息的计算误差，不自动约束模板失配引起的通量偏差。

## 6. 参考源与实际场景的精度桥梁

同一位置、同一条件 c 下定义
\[
 T_p^{(c)}=\frac{V_{B,p}^{(c)}}{V_F^{(c)}},\qquad
 \frac1{V_{B,p}^{(c)}}=\frac{S_{\rm ref}^{(c)2}/F_{\rm ref}^2}{T_p^{(c)}}.
\]
T 的单位为 [B]²/[F]²，通常是 sr⁻²。这是有条件的恒等式，不是由单个 SNR 恢复 V_B 的算法。

若同一条件下 C=σ²K，背景已知，K 正定且形状已知，则
\[
 V_F=\frac{\sigma^2}{h^TK^{-1}h},\qquad
 V_{B,p}=\sigma^2K_{pp},\qquad
 T_p=K_{pp}h^TK^{-1}h.
\]
共同拟合背景时，以白化并去背景后的有效信息替换 hᵀK⁻¹h。K=I、h_i=P_i/Ω_i 时 T=Σ(P_i/Ω_i)²，故不同 PSF 和面积即使 SNR 相同，也可有不同面亮度最优权。例如 F_ref=1、P_A=(0.5,0.5)、P_B=(0.9,0.1)、Ω=1；取 σ_A²=0.5、σ_B²=0.82，两帧 V_F=1，但面亮度权重是 2 与 1/0.82。

Canonical controls 的 C_ref 是加入规定参考源后的条件协方差。实际天空的 C_scene 采用实际场景的泊松期望；二者可能相同、也可能不同。若底噪和其它模型完全一致，源项差异已知，则可按
\[
 C_{\rm scene}=C_{\rm ref}
 +R(C_{\mathrm{src,scene}}-C_{\mathrm{src,ref}})R^T
\]
求场景方差；括号内的差不必半正定。无实际源期望时不能声称这一转换已知。单像元底噪方差1、gain=2、参考源100 ADU 时 V_ref=51，空场 V_scene=1，直接沿用参考 T 会错配条件。

如需写 V_B,scene=T_ref V_F,ref，必须额外证明 C_scene 与 C_ref 在所用估计量上的关系；否则重新求 T_scene 和配对的 V_F,scene，或明确采用经检验的背景主导近似。不同帧的 T 在同一输出位置为共同正因子时，才可在该位置的归一加权平均中消去；无需强迫该因子在整个天区为常数。

### 6.1 P2→P4→Mosaic：从实际算子求同被估计量的权重（2026-10-17 独立推导）

这一桥梁的**可执行解法**不是把 \`SNR_ref²/F_ref²\` 改名为 \`ivar\`，而是从实际输出估计量出发。给帧 k 定义已冻结的 calibrated + Drizzle + sky-correction + gain 线性化算子 \`L_k\`，将输入观测/拟合参数扰动 \`ξ_k\` 映到最终同一个球面位置 p 的校正面亮度：\`δz_{kp}=l_{kp}^T ξ_k\`。若完整联合协方差为 \`Σ_k\`，则

\[
V_{B,kp}=l_{kp}^{T}\Sigma_k l_{kp},
\qquad w_{B,kp}=V_{B,kp}^{-1}\quad(V_{B,kp}>0).
\]

这里 \`l\` 必须来自正式**已归一** Drizzle signal、实际天空参数更新和 gain，不能从 Drizzle 的 drop 原始面积或点源 h 猜测。若单独定义点源条件信息 \`I_{F,kp}>0\`，同条件的 \`T_{kp}=V_{B,kp} I_{F,kp}\`，才有 \`w_B=I_F/T\`。当 \`C_{B,kp}\` 已可直接计算时无需绕 SNR/T 回转。若某个 \`V_B\` 不含天空/增益**参数不确定性及交叉项**，它只代表冻结这些参数后的条件方差，不能将其称为完整边际方差。经 rejection 的自适应选择集合也不能由固定 L 自动保证无选择偏差。

一个很重要的**可证明降级条件**是：若独立校准能在相同随机贡献空间上保证
\[
(1-\delta)C_{\rm ref}\preceq C_{\rm scene}\preceq (1+\delta)C_{\rm ref},
\qquad 0\leq\delta<1,
\]
则对同一个输出线性泛函 l，
\[
(1-\delta)V_{B,\rm ref}\leq V_{B,\rm scene}\leq
(1+\delta)V_{B,\rm ref}.
\]
所以可以用上界 \`V^+_{B,\rm scene}=(1+\delta)V_{B,\rm ref}\` 作**条件保守**合成权重。前提是 \`C_ref\` 与 \`C_scene\` 用相同单位、模板、可观测空间和相关结构，并且 δ 有独立来源；参考源 Poisson 与实际星云/暗场差异未知时不能凭空指定 δ。仅知 SNR_ref、F_ref 与 gain，不足以得到此 \`V^+\`。

**可执行状态组合**：优先采用实际同对象 \`corrected_scene_variance\`；其次用有外部误差界的 \`certified_scene_variance_upper\` 并声明 \`conditional_conservative\`；再下才是 \`reference_flux_information_only\` 或 \`heuristic_snr_weight\`，这两种数值可供明确用户选择的近似/诊断，不能在 manifest 中自动声称 \`inverse_variance_closed\`。未识别支持、单位或参数模型，报告 \`precision_unavailable\`。Stage2 仍只发布 signal HiPS；这些状态随运行 provenance/内部评估传递。

### 6.2 独立噪声下的 minimax 合成：为什么保守权重不是随意削弱 SNR

若多帧在**同一个被估计量**上无偏、资格固定且条件相互独立，每帧真实方差只知 \`0<V_k≤U_k\`，选择非负系数 \`a_k≥0\`、\`Σa_k=1\`，其最坏方差恰为 \`sup Var(Σa_kz_k)=Σa_k² U_k\`。Lagrange 乘子或 Cauchy–Schwarz 证明其唯一最优（所有 U 有限正）为

\[
a_k^*=\frac{U_k^{-1}}{\sum_j U_j^{-1}},
\qquad \sup\operatorname{Var}(\widehat B)=
\left(\sum_k U_k^{-1}\right)^{-1}.
\]

这是**方差上界明确且独立**时的 minimax 解释，不是无条件证明比真实精确逆方差更好。若允许未知、最大程度正相关（\`|\Gamma_{ij}|\le\sqrt{U_iU_j}\`），则最坏方差可达 \`(\sum_k a_k\sqrt{U_k})^2\`，其最优可能只用一个最小 U 的帧。故**不知道帧间相关性就无法证明随合成帧数增加必然提高 SNR**。共享 Master、零点、天空面与增益参数的协方差必须建模或分开报预算。此结论不要求发布第二张科学图像。

## 7. 缺项上界的适用域

设固定观测域、固定 A、相同参考 F，C_true=C_0+Δ，Δ半正定。对估计 F 的同一组线性无偏系数 a（aᵀh=1、aᵀZ=0），有 aᵀC_true a≥aᵀC_0 a；在两侧对同一个可行集合取最小，得到 V_F,true≥V_F,0。因此省略这些噪声分量所得 SNR_0 是条件上界。

这比“缺 gain 就给 upper_bound”严格：需确认底噪未重算、模板/掩膜/背景自由度/通量定义不变，遗漏项合计确为 PSD，且参数仍可估计。仅比较相同保留子空间的伪逆公式时，应固定该子空间；出现新的精确约束则用完整约束问题处理。估计 gain 错误、PSF 失配、数据拟合交叉项、选择偏差或未知系统误差一般不能写成 PSD 加项，至多报告 approximate。该上界不约束实际场景 T、不保证物理绝对误差完整，也不保证深度阈值覆盖率。

## 8. 稀疏控制、重建与相关结构

Normalize 先建立已应用光度标度的平面模型，Drizzle 后由同一 noise 库完成球面 h、C_ref、Z 与 controls；唯一正式载体为 frame_science.json。每点保存自己的 F_ref、V_F、SNR、条件、模型身份、支持域及缺项。Catalogue 为浏览投影，规整网格为兼容视图。

控制点位于确定性有效 patch 中心，不要求当地存在真实恒星。帧摘要为非重复 patch 支持面积加权的 log SNR 中位数再取 exp，仅作摘要。求解窗口、patch 分区与 drop 覆盖并集分别定义，不以求解预算面积替代支持面积。

阶段二可以重建 log V_F、log SNR 或噪声尺度/形状参数；节点测量误差、插值预测误差与随机像元方差是三个对象。exp(E[log V])不等于E[V]；自然样条可过冲，正值变换、裁剪或 IDW 指数也不自动无偏。方法与空间尺度通过空间块/跨帧留出、PSF/几何/噪声分层比较确定，空洞和外推须有状态。

相关模型需满足 PSD；逐项相关系数位于[-1,1]不足以保证。例如四维单位对角、最近邻0.7、其余0的三对角矩阵最小特征值为1−1.4cos(π/5)<0，不能作为协方差。背景中位数的渐近方差 πσ²/(2N)仅适用于独立高斯样本；相关噪声、剪裁和污染需按该统计量重新估计。点源、背景控制与像元精度不能共用无对象定义的 k_corr。

## 9. 合成、同数据拟合与后选择

对于固定资格集合、固定权重、相同面亮度目标和独立帧，逆场景方差权重给出
\[
 \widehat B=\frac{\sum_k y_k/V_{B,k}}{\sum_k1/V_{B,k}},
 \qquad \operatorname{Var}(\widehat B)=\frac1{\sum_k1/V_{B,k}}.
\]
有共享随机项时用帧协方差 Γ，GLS 系数为 Γ⁻¹1/(1ᵀΓ⁻¹1)。共同校准项不随帧数自动消失；通带或 PSF 不同产生的目标不一致也不由逆方差消除。

若背景由同一 y 拟合，校正 z=y−Aβ̂ 有
\[
 C_z=C_y+A C_{\widehat\beta}A^T
 -C_{y,\widehat\beta}A^T-A C_{\widehat\beta,y}.
\]
β̂=Ly 且 L 固定时为 (I−AL)C_y(I−AL)ᵀ。把自己的样本均值减去会减少部分方差并引入负相关，不能一律只加拟合方差。稳健拟合及数据依赖权重还需要其完整估计器的误差分析。

数据决定的排异集合 E(y)使估计器非线性。把观测到的掩膜固定，再套未选择协方差的公式，只是条件计算近似；真正条件于“被接受”的分布已截断，方差不必是该公式。误拒率、信号偏差和区间覆盖用独立模拟或重复观测测量，不能把无污染等同于零误拒。

Mosaic 最终仅发布 signal HiPS；本节方差用于内部估计与评价，不要求 Export 从单图恢复协方差。科学验证应分别检验量纲、解析反例、独立数值实现、合成重复采样和真实生产输出，状态见工程正本。

## 10. 参考文献与开源对照

[1] Penrose, R. A generalized inverse for matrices. Proceedings of the Cambridge Philosophical Society, 51, 406–413. [原文](https://doi.org/10.1017/S0305004100030401)。

[2] Statsmodels. Linear regression source: whitened design、pinv_extended 和 normalized_cov_params。 [源码](https://www.statsmodels.org/stable/_modules/statsmodels/regression/linear_model.html)。这是数值对照，不替代 F 的可估计性检查。

[3] Photutils. calc_total_error：背景误差与按数组单位定义的有效 gain。 [文档与源码入口](https://photutils.readthedocs.io/en/stable/api/photutils.utils.calc_total_error.html)。

[4] Zackay, B., Ofek, E. O. How to Coadd Images? I. Optimal Source Detection and Photometry Using Ensembles of Images. ApJ, 836, 187. [原文](https://arxiv.org/pdf/1512.06872)。

[5] Fruchter, A. S., Hook, R. N. Drizzle: A Method for the Linear Reconstruction of Undersampled Images. PASP, 114, 144–152. [原文](https://arxiv.org/html/astro-ph/9808087v2)，相关噪声见第7节。


## D6：背景控制与排异的真实实现差距

[Sampler 正式算法](../algorithms/mosaic/PHASE2_SAMPLER.md)已独立定义 median/MAD、单侧剪裁、相关样本有效数和 Drizzle k_corr 的条件适用域。P2 control_variance 的被估计量是**patch 中位数**，单位随已施加 P1 linear_unit 为 (L/sr)^2，不能因该帧标为 ADU/sr 就忽略 #9 的光度应用后 BUNIT 错配，也不能把 #8 的参考点源 Fisher 或实际像元 variance 填入这一字段。当前 Sampler 对 MAD=0 使用 1e−12 的数值地板，并把脱离 300/600 arcsec/pixel 标定域的 k_corr 回退为1.4；这些是**实现事实而非误差证书**，科学闭合要求对应被估计量的独立 MC、相关场 Bootstrap 和单位/资格有效性检验，#17/#71 仍 OPEN。

[Rejection 详细算法](../algorithms/mosaic/PHASE2_REJECTION.md)已明确实际逐像素几何N路由、十显式核及其状态、真实 production large_scale、per-sample source index 和后选择真值。数据驱动选择不能把一个未条件化的 1/Σw 当真实覆盖区间的方差；当前 status 消费与真实 E2E 科学验证尚未闭合。两篇均未更改科学算法/参数。
