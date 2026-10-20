# Gaia DR3 XP 跨仪器光度标定与通带差异

本分册依据[最高设计](../../ACSD_DESIGN.md)与[统一科学模型](../unified/UNIFIED_SCIENCE_MODEL.md)，定义逐帧光度响应、参考光谱、标度应用和通带差异。阶段一的生产边界见[Normalize科学链](../normalize/NORMALIZE_SCIENCE.md)，实现状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 科学任务

测光拟合要完成：
1. 将每帧的恒星计数与 Gaia DR3 XP 校准恒星光谱及该帧综合响应进行正向比较；
2. 估计帧级线性乘性尺度与在匹配恒星支持域内可辨识的空间响应；
3. 如果用户选择跨设备、近似通带合并，明确**参考响应 R_0**与恒星颜色依赖；
4. 将标度用于图像并把单位、条件、偏差/系统误差留给 SNR 和 mosaic 解释；
5. 不把星等残差小视为任意发射线/扩展源光谱都等价的证明。

同一仪器/通带可以获得较高一致性；不同设备广泛合并可以增加信号，但不能假定随机叠加降低通带系统误差。

## 2. 前向合成测光与响应约定

给第 k 帧的系统总响应 \(R_k(\lambda)\)，包括滤镜、透镜/镜面、大气、传感器 QE 以及已选的时间/空间变化模型。定义响应时必须注明是能量响应还是光子计数响应。若 R_k 是不包含光子能量倒数的效率函数，光子计数型合成响应可写
\[
 Q_{ks}=\frac{1}{hc}\int f_{\lambda,s}(\lambda)
 R_k(\lambda)\lambda\,d\lambda,
\]
其中 f_{\lambda,s} 是 Gaia XP 外定标谱[1,2]。若响应定义已包含 λ/(hc) 或使用能量积分，禁止再乘一次；数值实现应明确物理单位、光谱采样、插值与积分误差。Gaia XP 的有限波段覆盖、外定标误差、颜色极端源和强窄发射线都可能不满足高精度条件。

### 2.1 普通窄带解析响应输入（设计已定；**生产接线状态见 §2.1.1**）

普通窄带仅要求中心波长 \(c\)[nm]、半高全宽 \(\Delta\)[nm]，均须有限、正且 \(c-\Delta/2>0\)。默认模型具名为 **parametric_narrowband/top_hat_approx/v1**：
\[
T_{\rm approx}(\lambda)=\begin{cases}
1,&c-\Delta/2\le\lambda\le c+\Delta/2,\\
0,&\text{其它}.
\end{cases}
\]
峰值按定义归一为1、FWHM等于\(\Delta\)，边界评价规定为包含端点（积分值不受孤立端点影响）。这是工程形状假设，**不是实测透过率**。整体灰色透过率可归入 Gaia 星光度比例因子，谱形、快速光锥角度效应、温漂、发射线相对透过率不可由单个灰度因子恢复。对精密测光或特殊滤镜允许另一互斥来源 **measured_curve**：完整有序的 \((\lambda,T)\) 实测/自定义曲线。即使两者有相同中心/FWHM，也不能认作同一响应。

OD5+ 是在明确声明的阻带范围及测试条件下带外透过率约 \(10^{-5}\) 的指标；既不证明带内为矩形，也不提供带内峰值/边沿/实际中心、更不保证 f/2–f/4 入射角分布下的干涉滤镜没有蓝移、带宽/峰值变化。Hα+[N II] 等窄线谱即使恒星色差拟合很好，也可能产生不可忽略的通带模型偏差。

**QE 必须独立消费**：无需用户提供滤镜库文件不等于相机 \(QE_k(\lambda)\) 可缺省为1。保留当前具名 QE 曲线或用户显式声明理想-QE 的路径，并保留其真实/假想 provenance。可知的光学与大气项另相乘，未知项单独声明。物理光子计数预期为 \(Q_{ks}=(hc)^{-1}\int f_{\lambda,s}T_k QE_k T_{{\rm other},k}\lambda\,d\lambda\)（波长和通量单位先转 SI）；**当前 C++ 的 XPMODEL 惯例是未除 hc 的积分**，在没有同步改变单位合同之前不可悄悄改其量纲。

**窄带求积契约**：根据确切解析端点 \(l=c-\Delta/2,r=c+\Delta/2\)，按滤镜跳变端点、XP 可重建函数结点、QE 连续插值分段与有效谱域切分。分别对连续区间内的 \(f_\lambda(\lambda)QE(\lambda)\lambda\) 使用解析多项式积分（适用时）或自适应高阶带误差求积；不允许把阶跃响应先投到 1/2nm 等距格再用 Akima/Simpson 跨边界积分。3/5/7nm、中心与 XP 格错相、谱域外截断、端点与极窄线响应为必要负例。需分开记录数值求积误差、Gaia XP 的低谱分辨率/外定标及波长覆盖误差、滤镜实际形状误差；数值积分收敛不证明窄发射线物理准确。

**最小生产配置与兼容策略（设计；**当前生产解析口径见 §2.1.1**）**：目标在 photometry.fit 下增加唯一 response 对象，二选一：(A) mode=parametric_narrowband、center_nm、fwhm_nm、profile=top_hat_approx、profile_version=1；(B) mode=measured_curve、curve_name（使用已有曲线库键）。未提供新 response 时，旧 photometry.fit.filter 与块级 filter_passband 继续按原路径映射 measured_curve，并保留二者非空时必须字节相等的校验；新对象和旧滤镜字段同时出现视为 RESPONSE_SOURCE_CONFLICT，不静默择一。未知 profile/version、非正或越界数值为 INVALID_PARAMETRIC_RESPONSE；缺 QE 维持具名异常/无完整拟合时合法原生信号的当前降级语义。frame overrides 必须按每帧绑定而非把第一帧身份传播至其它帧。

响应身份必须冻结 response_mode、profile/version、c/Δ及单位、峰值与形状假设、入射角/温度适用条件、QE 身份与有序曲线内容 hash、实测滤镜有序曲线 hash（若有）、积分约定、XP 覆盖状态、有效响应已知程度和 provenance。规范浮点序列化后构造具名 stable response_id；曲线名或 n/sum/sumsq 不足以唯一识别响应。

**C++ 小切口**：module_adapters.cpp 的 p1_op_photometry 配置/逐帧 binding → frame_photometry_fit.h 的 FramePhotFitRequest/Result 增加 source 枚举及元数据 → frame_photometry_fit.cpp::fit_frame_photometry 在扫描滤镜之前按 mode 分发，参数模式绕开滤镜条目命中（QE 仍须独立读取），measured 沿用原查找 → filter_curve_json.h::scan_curve_library 保留实际曲线/QE资源与有序 hash → spectrum_integrator.h/.cpp::prepare_filter_cache/compute_f_syn_cached_xpsd 增加带不连续边界的分段积分专用路径 → CONFIG schema、帮助与 frame_science/HiPS 响应元数据同时修改。当前无一项新键被证实已接线；不修改原 C++ 的本轮仅设计。

### 2.1.1 生产接线状态（Issue #106 / C1，2026-10-20）

上段的配置与求积设计已在生产路径落地（`main`，实施基线 `ba81a1631`）。**科学定义未变**；此处只登记接线位置、实际求积约定与已验证边界。

**配置面**（schema：`eng/contracts/schemas/phase_config_normalize.schema.json`）：

- `photometry.fit.response` 是 **tagged one-of**：`{mode:"parametric_narrowband", center_nm, fwhm_nm, profile:"top_hat_approx", profile_version:1}` 或 `{mode:"measured_curve", curve_name}`；`photometry.frame_overrides[].response` 同构。
- **同层并存 ⇒ `RESPONSE_SOURCE_CONFLICT`**（配置错误，中止运行）：同一站点内 `response` 与旧 `filter` 字段并存；`photometry.fit.response` 与块级 `filter_passband` 并存。
- **跨层替换**：帧站点的显式声明整体替换块级声明（新旧两种形式皆可），来源逐项登记（`explicit_frame_override` / `block_inherit`），不静默。
- 旧 `photometry.fit.filter` / `filter_passband` / `frame_overrides[].filter` 的既有行为**逐位不变**；`fit.filter` 与 `filter_passband` 的字节相等校验保留。
- **QE 仍是独立必需项**：`photometry.fit.qe_name`（或逐帧 override）必须命中扫描集；参数化模式**不查滤镜库**（用户无需提供滤镜型号条目），但仍须提供 QE 曲线（含显式 `Ideal QE curve`）。

**求积面**（`lib/algorithms/photometry/cpp/src/spectrum_integrator.{h,cpp}`）：

- 参数化模式的 F_syn 走**边界精确的分段求积**：断点 = 响应带解析端点 ∪ XP 采样点 ∪ QE 插值节点，每段用 4 点 Gauss–Legendre。因 Akima 在节点区间内是三次多项式，段内被积函数 \(F(\lambda)Q(\lambda)\lambda\) 是 ≤7 次多项式，4 点 GL 对其**精确** ⇒ **求积误差为 0**（仅浮点舍入）。
- measured 模式保持历史口径（Akima 重采样到 XP 网格 + XP 网格 Simpson）**逐位不变**；两条路径的 F_syn **不逐位可比**，模式与求积约定写入响应身份。
- 实测证据：3/5/7 nm Hα（中心 656.28，与 XP 2 nm 网格错相）在真实 KAF-16803 QE 曲线上，与独立 SciPy（`Akima1DInterpolator` + `quad`）相对误差 ≤ 5.0e-16；同一 top-hat 走历史 2 nm 粗格路径的相对误差为 55.6% / 33.3% / 4.8%，且 5 nm 与 7 nm 在粗格路径上给出**完全相同**的 F_syn（#43 的可复现反例）。真实 Gaia DR3 XP 光谱（172 颗匹配星）上生产 F_syn 与独立复算最大相对误差 5.0e-16。

**身份面**：`response_id = response:v1:<sha256(规范串)[0:32]>`，规范串冻结模式/剖面/版本/中心/带宽/解析端点/求积约定/覆盖状态/已知程度/QE 库键与**有序样本内容** SHA-256/滤镜库键与有序内容 SHA-256。参数化模式没有滤镜曲线文件，故 `response_id` 进入 `frame_science` 的 `photometry.response_id` 与 `application_id`（旧 measured 路径的规范串**逐字不变**）。

**仍未闭合**：响应**形状**误差（top-hat 是具名工程假设，不是实测透过率；OD5+ 不证明带内为矩形）、快光锥蓝移、XP 自身谱分辨率与外定标误差、窄线谱（Hα/[N II]）对未知星云 SED 的不可消除偏差；实测曲线的 2 nm 粗格求积误差（#43）**仍然 OPEN**（本轮只对参数化模式提供精确路径）。真实 HST/M42 数据验证属 #103。

第 k 帧实测恒星通量 \(f_{ks}^{\rm obs}\) 可以表示为
\[
 f_{ks}^{\rm obs}\approx a_k\,m_k(x_s,y_s)\,Q_{ks}+\epsilon_{ks},
\]
当f_λ以W m⁻² nm⁻¹、波长以nm输入时，未除hc的积分∫f_λRλdλ单位为W m⁻² nm；它不是谱密度。换算波长到与h、c相容的SI单位并除hc后，Q为光子率面密度；若再含口径和曝光，须相应变更单位。按∫Rλdλ归一的带平均谱密度是另一种量，单位才与f_λ相同。响应只相差与恒星无关的常因子时，拟合系数可吸收该因子，但元数据仍须声明所选约定。已外定标的XP谱不再乘Gaia G星等因子。

a_k 含口径、曝光、总体透过率与单位换算之积；如果组成因子无独立测量，**只能拟合乘积**。m_k 是小幅空间乘性响应，规定在合格参考星样本上的对数均值为零以固定与 a_k 的规范。稳健零点/IRLS 不应因为损失函数收敛而自动被视为无偏。

拟合应使用独立训练/留出星，检查饱和、混叠、变星、PSF 拟合/孔径定义、星表坐标、系统响应不确定性与残差分布。空间场阶数由星点位置对基函数的数值秩、条件数及留出集决定；无支持的边缘不应默默高阶外推。基础策略允许帧级标量 m≡1。

## 3. 原生通带绝对标定 ≠ 一幅图上的通带同一化

将仪器计数按 a_k 换算为自身 R_k 响应定义下的模型通量，可使恒星测光具有绝对标度，但**不同 R_k 在宽带/窄带光谱响应上的权重不同**，这不自动创建统一 R_0 的球面信号场。

如果用户定义马赛克参考通带 \(R_0\)，对某颗具有已知 SED 的恒星 s 存在
\[
 C_{ks}^{(0)}=Q_{0s}/Q_{ks},\quad
 Q_{0s}=\frac{1}{hc}\int f_{\lambda,s}R_0\lambda\,d\lambda.
\]
可以在恒星集合上对该变换与颜色代理拟合低阶关系。**但是单幅未知像素图像并没有已知的逐像元天体 SED**：没有额外色彩信息时不可能用一个标量 c_k 使所有星光谱、连续谱云气和 Hα/[N II]/[S II] 发射线源同时精确映射到 R_0。

因此须明确选择“参考光谱约定”：可选在某个指定参考颜色/参考 SED 处令各系统响应一致，在支持的恒星样本上估计颜色残差，并把整图线性缩放的适用域记为此约定。**不得对图像每个像素隐式应用只有观测恒星才知道的颜色变换**。若有经独立验证的逐位置 SED 模型，才可以考虑空间变化的物理通带变换。

使用者可把 3 nm 与 7 nm Hα 或不同品牌 R/G/B 组合进行成像；算法不限制。仅需说明结果是所选参考光谱/混合通带规则下的信号，恒星残差不能证明所有星云结构无系统差异。对真正未知的响应，可以只做具名相对测光并说明限制；**已选择**中心/FWHM 近似的滤镜是 PARAMETRIC_APPROX，不是实测也不需要滤镜文件。

### 3.1 P1 的有效响应定理和星云不可消除偏差（2026-10-17 补证）

把波长单位、积分通量/光子数因子预先合并进一致的线性响应函数 `R`，设 `Q_R(f)=\int f_\lambda R(\lambda)\,d\lambda`。对参考源 SED `f_*`、具名目标通带 `R_0`、满足 `Q_{R_k}(f_*)>0` 且响应全部位于有效谱域的第 k 帧，定义
\[
c_k=Q_{R_0}(f_*)/Q_{R_k}(f_*),\quad
R_{\rm eff}(\lambda,p)=\sum_k a_k(p)c_kR_k(\lambda),
\quad a_k(p)\ge0,\quad\sum_k a_k(p)=1 .
\]
这里 a 是对**已经同量纲、同观测场定义**的多帧信号的冻结非负合成权；若 PSF/重采样核不同，它还不是整个观测算子的完全描述。线性性严格保证 `Q_{R_eff}(f_*)=Q_{R_0}(f_*)`。但未知天体 SED `f=f_*+\delta f` 的偏差恰为
\[
\Delta Q(p)=Q_{R_{\rm eff}}(f)-Q_{R_0}(f)
=\int\delta f(\lambda)[R_{\rm eff}(\lambda,p)-R_0(\lambda)]\,d\lambda .
\]
由 Cauchy–Schwarz，若存在**外部给定**的 `m(\lambda)>0`，可给条件绝对偏差界
\[
|\Delta Q|\le
\Bigl(\int|\delta f|^2m\,d\lambda\Bigr)^{1/2}
\Bigl(\int|R_{\rm eff}-R_0|^2/m\,d\lambda\Bigr)^{1/2}.
\]
没有未知星云 `δf` 的有界模型时，**不存在由匹配星星等 RMS 推导出的任意扩展源通带转换全域上界**；Hα/[N II] 线谱尤其如此。若 coverage、背景、rejection 或 SNR 权重随位置变化，a_k(p) 随之变化，R_eff 也随位置变化；若 a_k 本身依赖所观察数据，它是**给定实际选择后**的条件响应，端到端已变为非线性估计器，不能简单宣称始终等于一个固定滤镜。

Gaia DR3 BP/RP 系数与协方差参见 [De Angeli et al. 2023](https://doi.org/10.1051/0004-6361/202243680)，外定标和合成测光参见 [Gaia DR3 synthetic photometry](https://doi.org/10.1051/0004-6361/202243709)。XP 光谱有限波段外的真实通量/积分除非另外测量，不存在仅由 XP 决定的严格数值；“截断后 Q>0”不表示完整通带响应已闭合。P1 可投稿的贡献需与已有 synthetic photometry 和 ubercal 比较，重点应是**空间响应身份、适用域和误差传递**，而不是声称首次用 XP 合成恒星标度。


### 3.2 可辨识的 Gaia 拟合与响应不确定性（R1 收敛）

Gaia 匹配恒星 s 的实测 \(C_{ks}\)[ADU] 是已定义 PSF/孔径、sky 扣除后的**总星积分测光**，不是峰值 ADU；合成 \(Q_{ks}=\int f_{\lambda,s}T_k QE_k\lambda\,d\lambda\)[XPMODEL 原生积分单位] 目前由生产 compute_f_syn_cached_xpsd 计算，不能在元数据仍用 XPMODEL 时暗称已除 hc 的 photons。经验回归采用
\[
r_{ks}=\ln C_{ks}-\ln Q_{ks}
=\theta_{0k}+b(u_s)^T\theta_{{\rm sp},k}
+\gamma_k(c_s-c_0)+\eta_{ks}.
\]
灰色 \(\theta_0\) 吸收口径/曝光/gain/灰效率不可分辨乘积；\(b\) 为零均值空间基 \(\sum_s w_sb(u_s)=0\) 固定 gauge；\(\gamma\) 是可辨识时的恒星颜色/响应失配诊断，而不是在没有未知星云 SED 时逐像元颜色校正。将色形失配物理化须改用低维 \(R(\lambda,\tau)\) 的前向合成积分，并用星色支持与留出样本辨识 \(\tau\)；未知响应零空间不能从有限 XP 恒星恢复。

先用 scalar robust 基线，再对 \(X=[1,b(u_s),c_s-c_0]\) 做白化 QR/SVD 秩与 \(\kappa\) 判定；色度和空间完全相关则退回可辨识子模型。与生产匹配保持相同 global Gaia source_id、积分通量、过滤曲线版本；饱和硬截顶不妨碍其它图像像元处理，但不能直接将平台峰值当未饱和积分恒星通量。对星样本做按 source_id 隔离的颜色/位置留出。无可靠测量方差时允许无权鲁棒回归，不能把 MAD 残差简单等同 \(\mathrm{Cov}(\widehat\theta)\)：使用可靠方差时可采用相应加权最小二乘及稳健 sandwich，否则用重采样/经验状态说明不确定性。

\(\mathrm{Var}(r_s)\simeq \mathrm{Var}(C_s)/C_s^2+\mathrm{Var}(Q_s)/Q_s^2-2\mathrm{Cov}(C_s,Q_s)/(C_sQ_s)\)。XP 谱系数协方差若可用，\(\mathrm{Cov}(Q_{ks},Q_{\ell s})=w_k^TC_{f_s}w_\ell\)，重复 Gaia source 与共享响应造成跨帧相关参数组。对 \(\alpha(u)=\exp[-\theta_0-b(u)^T\theta_{\rm sp}]\) 和 \(x'=\alpha x\)，\(J_{x',\theta}=-x'[1,b(u)^T]\)，因此 \(\mathrm{Cov}(x')=A_\alpha C_xA_\alpha^T+J_\theta C_\theta J_\theta^T\) 加同数据自定标/共享组的交叉项；不得丢掉低秩校准协方差。参考 SED \(f_*\) 与 \(c_k=Q_0(f_*)/Q_k(f_*)\) 必须具名；该比例只为**参考 SED**定义，并非一般发射线源的无偏转换。

**算法/源码最小切口**：保留 spectrum_integrator.cpp 的 XP 字节解码与有效积分路径；filter_curve_json.h 当前的样本数、和/平方和无法识别交换波长位置，改用有序 (wavelength,T,QE) 与积分约定的内容 hash (#42)。窄 Hα 网格在误差预算不足时自适应高精度求积 (#43)，XP 域外能量/模型误差记录而非忽略 (#44)；star_matcher.cpp 零 MAD 多数簇逻辑已有修复，不应回退；spatial_gain.cpp 已有秩检验但边缘支撑及预测协方差仍需补 (#46)。对 XP unavailable 允许原生 ADU/用户定义相对模式继续生成合法信号，不能凭拟合状态要求完整物理单位（#96 延期）。


## 4. 归一化应用和协方差变换

设像素信号 x_k 乘以已确定的尺度 α_k(u)，得到校准信号
\[
 x'_k(u)=\alpha_k(u)x_k(u).
\]
对于**已知且固定**的 α，随机像素协方差
\[
 C'_k=D_{\alpha_k}C_kD_{\alpha_k}^{T}.
\]
若 α 由匹配恒星估计，设参数 θ_k 与传播雅可比 J_θ，则还需描述
\[
 C_{{\rm total},k}\approx D_\alpha C_kD_\alpha^T
 +J_\theta C_{\theta,k}J_\theta^T
 +C_{\rm cross},
\]
其中 cross 包括像素观测与拟合参数共享数据产生的相关项，不能无依据一律设为零。通带 SED 模型失配一般另作**系统偏差或有条件不确定性**，未必可归入正态独立噪声。

像素产品始终保存线性强度/通量，不存星等。单位应由实际校准定义给出：未校准 ADU/sr、模型通带物理量/sr 或指定参考归一信号/sr；不要不论状态都写 ADU/sr。若软件不能用标准 FITS 单位表达某合成模型标度，应采用合规的通用物理单位并把归一约定保存在元数据中，或诚实标为自定义量，而不是伪造 SI 绝对量。

本项目的冻结自定义量（#9）：已施加 Gaia XP 合成通带标度的帧，其线性基单位为 `XPMODEL`（该帧 response 归一的模型通带通量；signal `XPMODEL/sr`、方差 `XPMODEL^2/sr^2`、ivar `sr^2/XPMODEL^2`）。`k_photo` 是带单位转换 `U_out/ADU`（`U_out` = `XPMODEL`），故应用后信号、方差、ivar 与 `F_ref` 必须同步改为模型单位族，并逐帧登记 response/application 身份；未施加帧保持 `ADU/sr`。二者不可在同一合成中被当作可加标的同一单位。

## 5. 与阶段一 SNR 和阶段二接缝的连接

参考通量 F_ref,k 必须与每帧采用的 α_k、对应 R_k 或 R_0 的约定一致。SNR 在正的**确定性标量**缩放下，通量和标准差采用配对单位时保持不变，但反推 w_F=1/V_F 时要在统一通量单位下计算；随机标度估计的不确定性与滤镜颜色依赖不因 SNR 无量纲而消失。

阶段二不应再次把 P1 的 a_k、m_k 作为新的未知增益完整拟合一遍。只有独立重复星光度证据表明残余的**差模**确实存在，才估计额外 g_k；分层比较颜色、PSF、孔径和天空位置，避免把其中的系统差当作普遍增益。

星云通带差引起的形态不同，不是“可减掉的加性天光”。背景算法必须保护天体真实的低空间频率结构。有关可辨识性、断连图与扩展源保真见[背景分册](../sky/UPM.md)。

## 6. 可证伪实验与统计误差

1. 合成 XP 光谱 + 确定响应 + Poisson/read-noise，检验恢复 a_k 和已知颜色转换；
2. 不同相机 QE、同通带、两近似宽带、Hα 3/7 nm 构型逐组建立真实天体光谱前向世界，给恒星/星云分别测残差；
3. 星点留出比对零点偏差、颜色斜率、空间拟合过冲及 PSF/光圈系统差；
4. 对混合设备但相同天体真值世界，用正确与错误的响应输入比较背景校正、SNR 权重和输出低频；
5. 不把本模型在真实帧中的观测 scatter 自己用作“误差预算上界”的组成来形成不可判错循环；测量误差和系统误差用独立来源界定；
6. 真实 M42/Gaia XP 数据须按每帧自身 WCS、通带、曝光、图像/母版哈希重跑，分别记录拟合、应用、球面产品和留出评价，局部残差不代表全链已获支持。

独立合成光度的约定和保留读数见[合成通量推导](../../../实验/photometric-magnitude/docs/fsyn_convention.md)，真实观测及限制见[光度实验报告](../../../实验/photometric-magnitude/REPORT_experiment.md)。它们只支持各自输入、响应和测量条件；残差不能自动归为Gaia误差或完整误差上界。重叠测光的全局相对约束见[3]。

## 7. 数据交接

归一参数、每帧 response、application ID、空间参考模板和输出单位的工程表达见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。默认完整 signal 可以是 native_calibrated 或明确相对标度；未知曲线不伪造 response-calibrated 标志。

## 8. 参考文献

[1] Montegriffo, P. et al. Gaia Data Release 3: External calibration of BP/RP low-resolution spectroscopic data. A&A, 674, A3. [论文](https://arxiv.org/abs/2206.06205)。

[2] Gaia Collaboration et al. Gaia Data Release 3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra. A&A, 674, A33. [论文](https://arxiv.org/abs/2206.06215)。

[3] Padmanabhan, N. et al. An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data. ApJ, 674, 1217–1233. [论文](https://arxiv.org/abs/astro-ph/0703454)。
