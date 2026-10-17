# ACSD S1｜P1 标度、P2 面亮度方差与球面 Canonical 消费独立数学研究

- **身份**：独立研究与施工建议，不是生产修复、不代表 Issue 关闭。
- **研究基线 `main`**：`51e06540eb021c7272e87cf67b34e53c0648cf7a`（2026-10-17 05:57:23 UTC）；严格按此基线解释源码静态证据。后续 main 的其他提交需重新复核。
- **运行**：`python3 实验/innovation-closure/code/s1_oracle.py --output 实验/innovation-closure/code/s1_results.json`；NumPy 2.3.5、Python 版本记录于结果；**12 个独立小模型/反例，PASS_ANALYTIC_ONLY**。
- **NOT_RUN**：生产 C++ build、真实 FITS/HiPS、完整 HEALPix 外部几何对拍、Gaia XP 真实响应积分、真实同源误差经验验证、Monte Carlo、Mosaic E2E。Oracle 的局部球面 chart 不是 HEALPix NESTED 索引实现。
- **相关**：#9（单位/标度）、#8（IF→VB）、#13（canonical reader）；#14、#21、#55、#70、#75、#76、#80。
- **核读依据**：`AGENTS.md`、`UNIFIED_SCIENCE_MODEL.md`、`DATA_SEMANTICS.md`、`NORMALIZE_DATA.md`、`PHOTOMETRY.md`、`NOISE_SNR.md`、`INTEGRATION.md`、`THEORY_REVIEW_2026-10-17.md`；比对 `module_adapters.cpp`、`weight_chain.cpp`、`normalize_science.cpp` 与 Issue #8/#9/#13。合同所描述的 TARGET 状态不作为源码已经执行的证据。

## 0. 可辨识的估计量：先定义对象再给逆方差

记 `U_n` 为原始像元已校准的**线性积分**单位（可为 ADU、electron 或 electron/s，是否是曝光总计数由实际标定节点决定），`U_a` 为应用后的参考线性单位，面积 `Ω` 用 sr。源像元线性积分量 `x_j [U_n]` 与面亮度 `B_j=x_j/Ω_j [U_n sr^-1]` 不是同一估计量。`F_native [U_n]` 是单位积分 PSF 的原生解析总通量参数；`F_ref(u) [U_a]` 是**u 位置标度的幅度参数**；`F_applied,total [U_a]` 是完整 applied PSF 实际积分量；`SNR_ref` 无量纲。`V_F [U_a^2]` 是规定参考源和 nuisance 条件下估计该幅度参数的条件方差，`V_B [U_a^2 sr^-2]` 是指定球面输出面亮度估计量的方差。`T=V_B/V_F [sr^-2]`（在 `U_a` 与 F 所用线性单位相同时），没有理由等于 1。

给同一场景物理响应/同一目标量定义之后才能谈跨帧 BLUE。跨滤镜允许用户合成，但在未知逐位置 SED 下各帧 `B_k` 不必估计同一物理场：这时其加权输出是具名**空间混合响应约定**，不能借 `w=1/V` 宣称固定通带无偏。见 #14；下式均条件于共同估计量存在或其约定已冻结。

## 1. P1: Native、applied、Gaia XP 的量纲闭合

### 1.1 Gaia XP 积分究竟是何单位

Gaia DR3 XP 外定标光谱 `f_λ` 可使用 `W m^-2 nm^-1`，有效谱域约 330–1050 nm。对**无量纲能量透过率/光子效率** `R(λ)`，以一致 SI 波长单位积分：

`Q_ph = ∫ f_λ(λ) R(λ) λ/(hc) dλ`，单位 `photons s^-1 m^-2`；若仅 `∫ f_λ R dλ`，单位 `W m^-2`；带平均谱密度 `∫f_λR dλ / ∫R dλ` 才是 `W m^-2 length^-1`。如果定义 `R` 已含 `λ/(hc)`、口径、曝光、增益或任意归一常数，不可重复乘；其单位随定义而变。用 `f_λ` 的 nm 网格计算 `λ/(hc)` 前必须执行 nm→m 转换，绝不能仅凭结果数量级靠近 `1e-17` 就称其 SI 辐亮度。

对恒星 s，`q_s=Q_R(f_s)` 是对应响应约定的参考点源量。拟合 `F_native,s ≈ a_k(u_s) q_s`，其逆因子 `α≈1/a_k` 可将 ADU 映为 `Q_ph` 等物理积分参考单位，**仅当** `R`、Gaia 光谱单位、通带积分规则、通量零点、口径/曝光吸收定义以及外部标定完整。对缺少这些身份的经验/相对定标，应指定 `U_a=Urelative(reference_id)`，不能伪造 W、photon 或 Jy。Gaia 光谱本身有外定标，不等于任意 ACSD instrument response 已知，也不等于不同滤镜图像自动是同一个 L_λ 的辐亮度。

### 1.2 空间标度：F_ref 与 applied 真总通量不相同

以单位总通量原生 PSF `Σ_all P_j=1`、完整 PSF 支持域、原生幅度 `F_n`、确定性的 `α_j [U_a/U_n]`，选中心标度 `α_*=α(u)>0`：

`F_ref(u)=α_*F_n`，`h_ref,j=(α_j/α_*)P_j`，所以源分量 `E[y'_j]=F_ref h_ref,j`，这里 `h_ref` 无量纲、**不要求** `Σ h_ref=1`。完整 applied 实际积分为 `F_applied,total=F_n Σ_all α_j P_j`。令 `c=(Σα_jP_j)/α_*`，则 `F_applied,total=cF_ref`，`P'_j=h_ref,j/c`，`V_F,total=c² V_F,ref`，在 c 确定时匹配 SNR 不变。局部窗口 PSF 漏尾不可偷偷重新归一或声称 c 已计算。若 c 有估计误差，重新加入参数不确定性。

**解析 Oracle**：`P=(.25,.75)`、`α=(1,3)`、`α_*=2`、`F_n=100`，则 `F_ref=200`，`F_applied,total=250`，`h_ref=(.125,1.125)`，`c=1.25`；`V_F,ref=4` 对应 `V_F,total=6.25`。两种参数化等价，但两种总通量名称不能混用。

### 1.3 确定/随机标度的协方差和误差分层

固定 `α` 且观测 `y'=D_α y`：`C'_conditional=D_α C_y D_α^T`，`ivar'_j=ivar_j/α_j²` **仅当**该 `ivar` 属同一观测量、原 C 对角且 α_j≠0；完整协方差逆 `C'^{-1}=D_α^{-T} C^{-1}D_α^{-1}`，不能混同 `1/diag(C')` 与 `diag(C'^{-1})`。Drizzle `R [sr^-1]` 后 `C_B=R D_α C_y D_α^T R^T`。有效像元集合改变时，必须按实际行与列重建算子。

设 `α(θ)` 由星标/共享 master 参数估计，取联合扰动向量 `(δy,δθ)`；`J_y=D_{α(θ0)}`，`(J_θ)_{ir}=y_{0i} ∂α_i/∂θ_r`，则线性化

`C'_marg ≈ J_y C_y J_y^T + J_θ C_θ J_θ^T + J_y C_{yθ} J_θ^T + J_θ C_{θy} J_y^T`。

`C_{yθ}` 不在训练/测量独立时随意设为 0；其正负与参数拟合方式有关。若 α 与 y 同数据相关，均值也可能存在 `E[δα δy]` 的二阶偏置，一阶 delta 法不是高 SNR/大参数不确定时的精确结论。多个帧共用 θ 时 `Cov(y'_k,y'_m)` 还含跨帧 `J_{θ,k} C_{θ,km} J_{θ,m}^T` 与交叉项；每帧单独加上校准方差不够。对已复用共享源估计的背景拟合，应从**同一联合随机向量**构造 `J Σ J^T`，不重复加相同项。

| 场/量 | native 原生积分 | applied 参考积分 | sphere signal/variance |
|---|---|---|---|
| signal | `y [U_n]` | `Dα y [U_a]` | `R Dα y [U_a sr^-1]` |
| `C` / 单像元 variance | `[U_n]^2` | `[U_a]^2` | `[U_a]^2 sr^-2` |
| `ivar`（只当对应量方差确实可用） | `[U_n]^-2` | `[U_a]^-2` | `[U_a]^-2 sr^2` |
| `F_ref`、`V_F`、`I_F` | 原生 F / V / I 定义 | `[U_a]`, `[U_a]^2`, `[U_a]^-2` | **不可替代** `B` 的 precision |
| `response` | 仪器综合响应身份、单位 | 参考 SED/响应/photometric normalization 身份 | 组合时还需空间混合响应/适用域 |
| `application_id` | `not_applied` / 显式状态 | 同实际 scale map 的内容 hash、response/ref、版本、空间坐标约定绑定 | Drizzle signal、variance 和 sidecar 使用**同一**应用身份 |
| FITS `BUNIT` | `U_n / sr` | `U_a / sr` | 与写入后数组数值对应的单位，而非上游默认串 |

`relative_scaled` 且 α 无量纲时 U_n 的物理维度可能保持，但数值的 reference_id 与 normalization 必须声明；`response_calibrated` 使用自洽参考物理量及 FITS 可解析单位；非标准模型参考尺度可以选**可解析的正规量纲**并用 sidecar 记录乘法归一常数，或明确非标准单位及解释能力，绝不采用看似标准但错误的 SI BUNIT。若单位变换信息缺失，输出科学能力可降为 `relative_only`，不能默认为 native ADU。旧产品需要明确版本处理，不能静默重释。

**静态差异 #9**：该基线附近 `module_adapters.cpp` 的 Drizzle 帧路径确实优先取 `photoapplied`、`frame_photscal`，并有 `α²` floor 说明；Issue #9 追踪原生/施加图像与 HiPS sidecar 的 ADU/sr 常量写出。本轮没有直接解析生产 FITS 头，不宣布所有 writer 已修好/仍未修好；修复核对点是实际写入与输出消费者的 **numeric ratio + unit identity**，而不是比对两个 JSON 字符串。

## 2. P2：条件点源 Fisher/GLS 到目标面亮度 V_B

### 2.1 固定模型的 GLS 与 nuisance

对同一窗口的观测向量 `d [U]`，单位幅度响应 `h`，背景/其它 nuisance 基 `Z`，模型 `d=Fh+Zβ+ε`、`E ε=0`、`Cov ε=C≻0`，**固定 C、h、Z、合格观测集合**。已知 β 时 `I_F=h^T C^-1 h`；未知 β 时令 `tilde h=C^-1/2 h`、`tilde Z=C^-1/2 Z`，`P_Z=tilde Z tilde Z^+`，`I_{F|β}=||(I-P_Z)tilde h||²`、`V_F=1/I_F`（仅 `I_F>0`）。`Z` 自身不满秩不必妨碍 F 可估计；通量 h 落在 nuisance span 时 F 不可辨识。C 半正定时必须另审噪声零空间的精确约束，不能直接把 Moore–Penrose 伪逆当成所有问题的答案。

**异方差二维精确反例**：`h=(.8,.2)`、`C=diag(1,4)`、`Z=(1,1)^T`。已知背景 `I_F=.65`、`V_F=20/13≈1.53846`；背景未知 `I_F=.072`、`V_F=125/9≈13.8889`。这不是两个观测场景的经验误差，仅是同一个两维模型 nuisance 自由度改变所致。若源泊松令 C(F) 变化，本项是**在给定 F_ref 冻结 C 的 GLS 信息**，不自动等于真实 Poisson 模型的 Fisher；高斯、泊松的 likelihood 信息定义不同，不能重复叠加协方差导数项。

### 2.2 真正的球面面亮度估计方差

对同一观测/参数扰动联合向量 `ξ`、同一输出叶 p 的**已归一** Drizzle+sky+gain（必要时经 linearization）行算子 `l_p`，`z_p=B_p+l_p^T ξ`，故 `V_{B,p}=l_p^T Σ_scene l_p`。最简单 `R_pj=(area_drop_overlap/area_drop)/N_p`（单位 sr^-1），`N_p=Σ_j w_jp Ω_j`，于是 `S_p=Σ_j R_pj x_j`，`V_p=Σ_ij R_pi C_ij R_pj`，相邻叶协方差 `Cov(S_p,S_q)=R_p C R_q^T`。若 `sky_fit` 自同一帧/多帧估计，`l` 包含其实际 Jacobian；只把 sky 结果当常量是 **conditional** 方差而非完整 margin。

对相同条件 c、同一位置和两种不同估计量，`T_p^(c)=V_B,p^(c)/V_F^(c)`，`w_B=I_F/T` 是一个带单位的恒等式。正确生产路径通常**直接计算** `V_B=lΣl^T`，不需先绕 `V_F`。如需用 sparse `I_F`：必须提供已验证 `T_scene` 或由可证明同模型 noise transfer 求出 scene 版本，不可以默认 `T=1` 或 `T_ref=T_scene`。

**PSF / 同亮度方差双反例**：各帧二维观测 `C=I`、恒定背景场的已冻结面亮度估计 `l=(1,0)`，因此 `V_B,A=V_B,B=1`。点源模板 `h_A=(1,0)`、`h_B=(.5,.5)`，于是 `I_F,A=1`、`I_F,B=.5`、`T_A=1`、`T_B=.5`。采用错误 IF 权重 `[1,.5]`，Mosaic 方差为 `(1+.25)/(1.5)²=5/9`，正确共同亮度模型权重应 `[1,1]`，方差 `1/2`，错误方式增加 **11.11%**。这里每帧的 brightness estimator 已定义，不能偷偷改成与 PSF 匹配的星点最优提取再声称反例无效。

**Drizzle 相关反例**：原积分像元 `C_in=diag(1,4) [L²]`、原像元面积 `.2 sr`，输出行 `R=([[.75,.25],[.25,.75]])/.2 [sr^-1]`，`C_leaf=[[20.3125,23.4375],[23.4375,57.8125]] [L² sr^-2]`。非零 `23.4375` 是同源观测被两个叶重复消费的后果；不能通过输出单叶方差推出孔径完整方差。

**参考源 vs 场景**：简单 detector `Var_ADU = 1 + F_ADU/g`，`g=2 e-/ADU`，参考 `F=100 ADU` 得 `V_ref=51 ADU²`，场景空源为 `V_scene=1 ADU²`。当 actual scene 源分布/背景缺失时，只从控制点参考 SNR **不能识别**该像元实际方差；且 source Poisson 均值应使用真实非负期望与曝光/增益，不能用天空扣除后负信号直接算。

### 2.3 跨帧相关：一般 GLS 而非 `1/Σivar`

冻结有效集合、各帧估计**同一个** B，`z=1·B+e`，`Γ=Cov(e)≻0`，则 BLUE `a=Γ^-1 1/(1^TΓ^-1 1)`、`Bhat=a^Tz`、`Var(Bhat)=(1^TΓ^-1 1)^-1`；一般 `Var(a^Tz)=a^T Γ a`。只有 Γ 对角、`w_k=1/V_B,k`、选择冻结且同对象无偏，才有 `Var=1/Σw`。`Γ=[[2,1],[1,2]]`（共享 zero-point 方差1 + 各帧独立方差1）时各帧精确**边际** ivar=.5；错误对角公式声称方差1，真方差1.5，低估33.33%。即使各自边际方差已完整，丢弃 off-diagonal 仍会出错（#21）。相关项未知，两个世界 `Γ=I` 与 `Γ=[[1,.99],[.99,1]]` 有相同 marginal variance，但等权均值方差分别 .5 与 .995；所以只知 diagonal 无法证明完整 uncertainty。

Sky、flat、photo θ 的共享随机来源必须构成 `Γ` 块，且 sky 自观测中估计还可能导致**负相关**；对称性、PSD、rank、gauge、条件/边际口径先冻结。经过 residual-based rejection 的 `a` 本身为数据函数，条件 on fixed mask 的 `a^T Γ a` 不能代表无条件选择后准确置信区间；需 paired truth/MC 校验 coverage。

**静态差异 #8/#21**：`weight_chain.cpp` `finish_pixel_weight` 路径计算 `(SNR_layer/F_ref)^2*g²` 并设 `weight_chain_closed=true`、`production_allowed=true`；该值至多有 `I_F` 与 gain 参数化关系的证据。`module_adapters.cpp` 实际还具有 corrected_variance/ivar 与 SNR fallback 多路径。不能用 `weight_chain` 的数值自等式证明各路径都是同对象 `V_B^-1`。对于 corrected variance 若没有拟合参数协方差，也不应自动标完整 marginal precision。

## 3. Stage1 Canonical 球面参考 → P2 真正的消费

### 3.1 精确消费次序

1. **发布对象**：Stage1 完整完成的 signal HiPS + 正式 `frame_science.json` + hash-verified local models + 单一完成身份。`source_sha256, input_occurrence, frame_id, run_id, config_sha256, software/build身份` 与产品绑定；不能靠路径/文件名或可能陈旧的 configure SHA 猜测。源目录/暂存没有已完成 sidecar 是未发布，不是 precision unavailable。
2. **合法 v2 reader**：按 `acsd.normalize.frame-science/v2`（目标版本）验证 schema、top-level、model refs/hash、FITS BUNIT 实值、`application_id`、`reference_id`、response/SED、signal linear_unit，controls `F_ref,V_F,SNR=F_ref/√V_F` 和本地 h/C/Z/R 同条件、所需低秩因子、PSD/可辨识性、operator 真实归一、support geometry、联合缺项。拒绝已经知道错误的 v1，而非自动 +9 修补。
3. **球面身份**：所有 control 使用 `ICRS(ra,dec)` 与 `leaf_order=K`、`leaf_ipix` 为 64bit decimal string，且 `ipix = ang2pix_NESTED(K,sky)` 按独立 HEALPix oracle 对拍。`Nside=2^K`，`Ω_leaf=4π/(12·4^K) sr`。`tile_order=K-9`（512 tile），仅用于目录；K=11 的 tile order=2 **不是** leaf order。若需要升/降阶，明确 parent-child NESTED 逻辑、面积支持/输出被估计量的重新计算，不把复制插值视为新增测量。
4. **球面支持**：对实际目标位置先用单位球向量/球面足迹或 MOC support 判资格，再选在同一支持分量的局部控制点；对 RA 0/360 回绕、极区跨面不用直线 RA 网格插值。局部内可选由球面几何推导的切平面 chart + 非负仿射精度重建，或有明确边界/误差的球面插值。未覆盖区 `NO_DATA`，覆盖但无精度 `PRECISION_UNAVAILABLE`，域外 `OUTSIDE_SUPPORT`，坏文件 `DATA_CORRUPT`，模型不可辨识 `UNIDENTIFIABLE`，应分开。星点 IF 可作为球面诊断，不应因此变成 mosaic 权重。
5. **精度重建**：只在有效支持内重建正值 `u=log V_F` 或在已经求得 T 后重建 `log V_B`。控制点只有参考源信息时产出的是**reference precision field**；缺节点不是 SNR=0，也不是最近邻可自动给真精度。插值估计的置信与仪器噪声的观测方差不是同一随机变量。
6. **同估计量变换**：在真实有效 mask/sky/gain/Drizzle 实际样本及 scene noise 条件下求 `V_B(p)`。能证明完整实际 `V_B>0` 时，`w=1/V_B`；若只能给独立支持的上界 U，用 `1/U` 可作指定模型下条件保守/minimax 权重（独立误差前提）；不满足时使用有 provenance 的 `heuristic_weight` / 等权**仅定义数值合成策略**，`uncertainty_kind=unavailable`，不发布伪完整随机置信度。
7. **归约与输出**：sample mask、accepted、support 与 weight 来源逐像素一致。记录 `source_used`、`conditional|marginal|approximate|unavailable`、noise scene/reference、相关项、低秩参数引用、geometry/model hashes、T 身份、是否外推、fallback 原因和计数。Stage2 **只发布一个科学 signal HiPS**，诊断/状态留内部运行摘要；Stage3 不从单 signal 猜出 variance。

### 3.2 稀疏重建的一个可计算条件误差界

在局部合格 chart 中，真实 `u(x)=log V_B(x)` 若二阶连续且可由**独立证据**给出 `||Hessian u||₂≤M`；以 `λ_i≥0`、`Σλ_i=1`、`Σλ_i x_i=x` 的仿射重现控制点，测点误差上界 `|û_i-u(x_i)|≤δ_i`（也必须有外部依据），则 Taylor 余项逐点证明

`|û(x)-u(x)| ≤ Σ λ_i δ_i + (M/2)Σ λ_i||x_i-x||² ≔ Δ(x)`，

于是 `V_B(x)≤exp(û(x)+Δ(x))`、`w_conservative=exp(-û-Δ)`，**仅在假设覆盖范围内成立**。有误差置信覆盖而非确定上界时结论同样只是相应概率的条件界；chart 几何误差需要另行纳入。没有 `M` 或没有 `δ_i`、目标在凸包外/支持空洞时，任何有限点集均允许节点处相同而内部任意不同的光滑 bump：不存在无条件上界。不能把 `nearest_valid_fill`、GP 的模型内 posterior sigma 或结点重现残差自动包装为真实测量 uncertainty。与 #70 的支持域/近似误差一起施工。

**静态差异 #13/#70**：Stage1 在 `normalize_science.cpp` 可写球面 controls 的 sky/leaf/PSF/C/condition 等；P2 当前 `p2_sparse_layer_probe` 与 `weight_chain` 仍以 `ACSD_SPARSE_SNR_LAYER` 的 tile-local layer 消费，`nearest_valid_fill` 可以把 NaN 缺测节点写成有限插值，再设可用生产权重。这不等于球面 canonical `frame_science` 已被 Mosaic 消费。正式合同目前把 v2 作为目标；在验证生产真正写出且 reader 消费之前，**v2 E2E 是 NOT_RUN，而不是 PASS**。

### 3.3 三类科学/数据状态绝不合并

| 状态 | 对应情形 | signal 是否能继续 | precision 的表示 |
|---|---|---|---|
| `OK_PRECISION_UNAVAILABLE` | 完整合法的已发布 v2 signal，但 PSF/协方差/scene/T 信息不足 | 可以，按具名降级合成规则 | `null + reason + missing_terms`，绝不设 0/∞ |
| `DATA_INCOMPLETE` / `DATA_CORRUPT` / `DATA_CONFLICT` / `UNSUPPORTED_SCHEMA` | 声称完成的 sidecar 缺失、hash错误、模型字节坏、leaf/单位矛盾、旧v1 | 作为正式该产品输入不得隐蔽接收；可从可信上游重新生成 | 明确数据/版本错误，不假装正常 unavailable |
| `UNIDENTIFIABLE` / `OUTSIDE_SUPPORT` | 幅度与 nuisance 共线、协方差无法合法估计、无有效控制凸包/支持 | signal 本身可能可用 | `unavailable` 带数学或空间原因；外推数值只可命名 approximation |

对没有可证 V_B 的像元，在图像 signal 保留负值、零值与 finite 值的真实语义。若目标像元本身不被观测则 NaN/no data；**不能以 w=0 定义该像元真实方差无限**，那只是某种数值不使用权的决策。无可证 variance 时发布的 `uncertainty_available` 不应 true。

## 4. 独立 Oracle 的输入、判错能力和覆盖边界

独立脚本 `code/s1_oracle.py` 将每例的**矩阵/参数/量纲假设、结果和部分解析断言**写入 `code/s1_results.json`，不会导入 ACSD C++、生产模型文件、FITS 或自家 Drizzle 算子。包括：(1) deterministic scale、(2) stochastic share/cross、(3) PSF=flux Fisher vs B variance、(4) heteroscedastic nuisance GLS、(5) shared Γ、(6) general correlated BLUE、(7) reference vs scene Poisson、(8) unknown Γ two worlds、(9) spatial F_ref vs actual total、(10) Drizzle-related leaf Γ、(11) missing control two worlds、(12) RA seam/pole tangent support / absent / corrupt。数值线代采用 `numpy.linalg.cholesky/solve/svd`，可直接重跑。输出 `PASS_ANALYTIC_ONLY` **不表示**全部 12 个生产路径或物理世界已得到验收。脚本使用确定性解析输入，**没有 Monte Carlo，所以没有种子、样本数或覆盖区间**。

**下一步 Monte Carlo 设计（NOT_RUN）**：冻结模型与随机数协议（建议 numpy PCG64, seeds=[23017,23018,23019,23020], N≥100000 每臂）、分别采 `y~Poisson(μ_e)` 加独立 Gaussian read noise、共享多帧 θ/mother flat 或 sky fitted from same pixels，完整通过原帧 Drizzle 输出 `z_p`；测 `bias=mean(z)-B_true`、样本方差 `s²=Σ(z-mean)²/(N-1)`、68%/95% CI coverage、标准化残差偏态，以及按 `(χ²_{N-1,0.025},χ²_{N-1,0.975})` 推导的方差置信区间（仅独立正态实验、固定估计器下严格成立；非正态 Poisson/非线性选择需 bootstrap/校准）。每臂保持真实相关 noise、共享 θ、reference/scene、rejection 配对 seed、原始矩阵输入 hash 与评价脚本版本。真实场景期望未知时不能拿 production weight 自己当 MC 真值。

**真实 FITS/HiPS E2E（NOT_RUN）**：固定现有 M42 六帧（必要时分同通带/异响应两批）、原始 FITS hash、校准母版、Gaia XP 谱/系数和协方差、response source、相机 gain/RN/exposure 与 WCS，分别运行 native vs applied Stage1 与实际 Stage2/3；外部 Astropy + astropy-healpix/HEALPix reader 独立核对几何、`BUNIT`、像元数值、source flux、variance 和单位，报可复算分母/有效像元/支持域/异常；sky 与校准 θ 进行 paired bootstrap/模拟。适用时另用独立小 patch C 与 R 在原始像元域重算 `V_B` 及 Γ。报告执行的二进制 SHA、source+config hashes，#75 的陈旧软件版本字段必须独立核证。外部 HEALPix 跨 12 base faces、两极、0/360、`K=9,11,13` 在当前 Oracle **没有执行**。

## 5. 对 #9 / #8 / #13 的执行施工建议与验收矩阵

| Issue | 建议唯一施工重点（由 Codex 写正式 docs，执行端改 C++） | 确切验收/负例 | 本轮状态 |
|---|---|---|---|
| **#9** | 一个 `UnitScaleContract`：`native_linear_unit, applied_linear_unit, scale_state, α_field/model_hash, response/reference/normalization, application_id, parameter_covariance_state`；writer 由**实际 applied plane**的单位写 HiPS FITS BUNIT、signal/variance/ivar/F_ref，Export 延续元数据；旧版本显式版本处理 | (i) native/applied 同帧像元/flux 比值与 α 栅格相符；(ii) `C' = DCDᵀ` 与 `ivar'` 逐位、跨像元核对；(iii) α=1e-17 不自动 SI；(iv) `ADU/sr` 与 `Uref/sr` 互换错误可由第三方 reader 发现；(v) response/应用不相配→DATA_CONFLICT | 解析 PASS，生产/E2E NOT_RUN |
| **#8** | 在 Mosaic `p2_validate_candidate_weights` / per-pixel integrate 前建立 `SurfaceVarianceForSample`：输入经过同一 sky+Drizzle 的 `l`, `Σ_scene`/共享来源, mask/condition; 输出 `V_B` 及精度身份；`weight_chain.cpp` 的 `SNR²/F_ref²*g²` 明确仅为 `I_F` 类型，只有经合法 T 后才升级 `inverse_variance` | (i) 两帧 PSF 反例正确返回 `[1,1]`、variance=.5 而不是5/9；(ii) heterosc/nuisance/scene Poisson/Drizzle offdiag；(iii) Γ=[[2,1],[1,2]] 真方差1.5；(iv) corr unknown→不发布完整 uncertainty；(v) rejection后 empirical coverage；(vi) 通带不一致只保留混合响应身份 | 解析 PASS，生产/E2E NOT_RUN |
| **#13** | Stage1 `frame_science/v2` writer/完成身份到 P2 真实 reader 形成一条实际消费链；按 `ICRS→NESTED leaf→spherical support→precision reconstruction→same-condition T/V_B→sample weight`；废除 legacy 假扮 canonical 的权威身份，保留 per-sample 真实 ivar 作为独立基线 | (i) valid v2 roundtrip+hash/model identities；(ii) v1、完成目录缺 sidecar、坏hash、错 BUNIT 分别返回 UNSUPPORTED/DATA_INCOMPLETE/CORRUPT/CONFLICT；(iii) K=9/11/13 与 tile order K-9，跨face/极区独立 oracle；(iv) missing/hole/unsupported→unavailable，不能 nearest_fill→closed；(v) reader 实际调用、weight provenance、scene T 证明；(vi) 有 signal 无 precision 仍正确存活 | 局部球面数学 PASS，完整 HEALPix/读写/E2E NOT_RUN |

为消除原函数名歧义：`PixelWeightResult.weight_chain_closed` 当前基于 SNR/f_ref/g 数值合法，不具 `V_B` 统计闭合证据；`uncertainty_available` 只有在目标像元、实际 scene、sky+master 共享模型和条件定义完整或明确条件化时才准确。保留结构的数值计算能力，同时提高状态可解释性；不以禁用用户混合设备/滤镜或停止输出科学信号替代科学修正。

## 6. 科学未决与论文范围

1. **Gaia**：XP 响应真实曲线、带外能量、源系数 covariance、受波段/SED 条件限制的标准量；需真实数据/仪器测量，不可在无谱信息的星云上保证固定 R0 物理无偏。
2. **P2 噪声**：实际 light/master/flat/cosmetic 源协方差、背景参数和 source Poisson 本底、精确 PSF 漏尾/积分、rejection 选择偏差、跨帧共享系统项；理论公式正确不等于生产估计已能提供所有 Σ。
3. **Canonical**：v2 一致发布、独立 HEALPix 12 face + 极区验证、支持域局部光滑上界的实际经验校准、可辨识局部模型及大量真实控制点的误差分布。
4. **下游 #76/#80**：P5 sky correction 可改变 `l` 与共享 Γ；真实 mixed Hα/Red 不一定估计同一场，非 SPD 应独立定位 gauge/约束/数值，而不能简单把 P1 的 reference IF 当成减小残差的补丁。
5. **论文贡献边界**：Gaia synthetic photometry、GLS、HEALPix、Drizzle、点源 Fisher 已有成熟先例。可研究的新颖性在于跨任意设备的**有身份的估计量传播、场景精度转移与可识别支持域、相关参数误差在球面多阶段执行的组合**；需要真正独立 E2E、真实真值/外部样本和可复现的性能增益才能主张创新，而非凭公式和一个 toy oracle 自证论文闭合。

### 主要外部来源（原始资料）

- Gaia DR3 synthetic photometry, A&A 674 A33 (2023): https://doi.org/10.1051/0004-6361/202243709
- Gaia DR3 BP/RP external calibration, A&A 674 A3: https://doi.org/10.1051/0004-6361/202243680
- Fruchter & Hook, Drizzle (2002): https://doi.org/10.1086/338393
- Zackay & Ofek, How to coadd images (2017): https://doi.org/10.3847/1538-4357/836/2/187
- Naylor, optimal extraction (1998): https://doi.org/10.1046/j.1365-8711.1998.01314.x
- FITS standard 4.0: https://fits.gsfc.nasa.gov/fits_standard.html
- IVOA VOUnits 1.1: https://ivoa.net/documents/VOUnits/20231215/REC-VOUnits-1.1.html

**结论**：当前可以独立证明“IF 不自动是 SB 权重”、“随机/共享标度不能仅乘 α² 当完整边际协方差”、“无支持控制点不能推出可信 precision”；正确算法路线是由输出面亮度算子和完整联合噪声先得到 `V_B`，Canonical 的参考点源信息另保留身份。**这一数学结论不代替当前 P1/P2/P3 生产程序验收**，也不需要伪造 E2 实验结果。研究到此停止，等待独立数学复审。
