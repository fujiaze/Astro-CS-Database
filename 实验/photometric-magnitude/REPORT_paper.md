# P1｜基于 Gaia DR3 XP 合成测光的天文图像逐帧光度标定：模型、误差预算与真实数据失效域

> 本篇给出模型、单位、可辨识性与判据的定义，并按最低图表集登记真实帧读数、证据级别与失效域。实验过程、逐星读数、可复跑脚本与输入哈希台账见 `REPORT_experiment.md` 与本单元 `code/s04/`；配套一手文献与 BibTeX 见 `../shared/references/CORE_REFERENCES_VERIFIED.md`。

## 摘要

面对曝光、增益、通带和大气透过率信息不完全的天文 FITS，本文研究如何使用 Gaia DR3 BP/RP（XP）外定标光谱，为观测帧建立**声明实际响应、零点规范与数值单位的相对线性光度尺度**，并定量区分合成通量可计算性、星点测光残差及最终校准可信度。方法以源光谱与滤镜/量子效率响应卷积得到参考合成通量，通过逐星对数残差的稳健估计求取帧级尺度系数，可选低阶空间响应。我们将理论可辨识性、合成天区真值恢复、物理前向成像与真实观测三类证据分开。在 M42 M1 天区、Red、300 s 的 8 帧合规子集上，参照帧残差散度在方差式订正前后的两个代码版本上分别为 `0.02651984` 与 `0.02687307 mag`，均高于该帧已登记项合成的**下包络**（同两版本分别为 `0.020561` 与 `0.01900 mag`）；该下包络不是总误差上界，观测到的超界只说明存在未登记的误差贡献，不构成“科学不合格”。颜色残差趋势明确且可跨仪器复现，但去趋势后仍不达该下包络。这些数值是历史 S04 Python 测量及对应旧代码版本的记录，本轮没有现行C++链的重复实验。本篇不主张开创 Gaia XP 合成测光、未知星云跨波段无偏同一化，也不宣称 ACSD 已达到毫米星等精度。

**关键词**：Gaia DR3 XP；合成测光；稳健回归；测光零点；空间平场；误差预算；可辨识性。

## 1 科学问题与相关工作

Gaia Collaboration / Montegriffo 等给出 Gaia DR3 XP 的外定标光谱与合成测光框架 [R01]；Xiao 等已将 XP 合成测光用于 CMOS 精密光度定标并取得重复观测结果 [R02]；Padmanabhan 等的重叠观测全局校准说明了相对零点与绝对锚的区别 [R03]。因此 ACSD 可检验的论文贡献应收窄为：对不完备 FITS 元数据的实际链路建立可计算参考通量、帧级标度与空间修正的明确单位接口，并以独立预算和真实帧反证来界定成功/失败的适用域，而非把这些成熟基础方法宣称为首次提出。

研究问题：H1 在光谱有效域与响应积分约定明确时，逐帧相对尺度能否辨识；H2 在已知PSF/像元相位/响应的前向世界中是否无偏；H3 真实帧的逐星聚类CI能否由**独立且不重计相关项**的误差预算解释；H4 低阶增益是否和WCS、颜色、PSF/光圈选择混淆；H5 应用后的像元值、参考通量、球面面积归一、方差及BUNIT是否共享一个物理身份。H2与H5的现行生产E2E未完成。

**表 1 相关工作与本文区别**

| 文献 | 已确立的内容 | 本文与之的区别与增量 | 本文不主张的部分 |
|---|---|---|---|
| Gaia Collaboration / Montegriffo 等 [R01] | Gaia DR3 XP 外定标光谱与合成测光框架、采样有效域 | 只取其作为输入光谱；本文研究的是把它接到元数据不完备的实测 FITS 时的单位接口与逐帧规范固定 | 不主张 XP 光谱或合成测光方法本身的新颖性 |
| Xiao 等 [R02] | XP 合成测光已用于 CMOS 精密光度定标并取得重复观测结果 | 本文以**独立误差预算 + 分层残差反证**界定该链路在真实帧上的适用域，而非再报一个定标精度 | 不主张“首次以 XP 做 CMOS 标定”；不与其精度数字直接比较（被估量、通带与帧集不同） |
| Padmanabhan 等 [R03] | 重叠观测的全局相对校准，相对零点与绝对锚的区别 | 本文把“帧级相对规范固定”与“绝对锚缺失”写成显式可辨识条件，并只报相对刻度 | 不主张本文给出绝对测光零点 |
| Huber [R10] | 稳健位置估计的损失函数与影响函数 | 直接采用 Tukey 双权/IRLS，不修改；本文增量在于报告初值、尺度固定方式、有效自由度与失败状态 | 不主张稳健估计量的新理论 |

引文状态见 `../shared/references/CORE_REFERENCES_VERIFIED.md`；全部为书目级核验，正文不含须逐页核对的式号断言。

## 2 观测模型、单位与可辨识性

设第 k 帧第 i 颗星的扣背景实测积分通量 `f_ki>0`，其中饱和、混叠、坏点与失配源须在拟合前排除；Gaia 给出的光谱辐照度 `F_lambda,i` 单位可写为 W·m^-2·nm^-1，波长网格需转换成与 `hc` 一致的 SI 单位。对于**光子响应**，理想参考光子通量满足

```text
q_i = integral F_lambda,i(lambda) * T(lambda) * Q(lambda) *
               [lambda/(h*c)] d(lambda)
```

其中 `T` 和 `Q` 无量纲；在 `F_lambda` 和 `lambda` 均使用一致 SI 量纲后 `q_i` 为光子·m^-2·s^-1。代码如省去常数 `1/(hc)`，定义的是`Qproxy_i=integral F_lambda*T*Q*lambda d(lambda)`，**不得把 proxy 的数值和单位误写为真实光子通量**，也不得跨不同积分/波长单位约定直接比较。G 星等不应再次乘在已经包含绝对通量的 XP 谱上。

仅当本征PSF、光圈/总通量定义、入选源和响应适用域一致时，相对响应模型才可近似写成 `f_ki = c_k · m_k(x_i,y_i)^(-1) · q_i + δ_PSF,i + δ_WCS,i + δ_SED,i + e_ki`，`c_k` 合并曝光、口径、透过率和探测器换算等不可分离的正系数。无独立仪器标定时这些物理乘子**仅积可辨识**；以 `m_k` 的空间均值或参考位置固定为 1 确定规范，从 `r_ki=log10(f_ki/q_i)` 的稳健位置估计 `mu_k` 求 `k_photo=10^(-mu_k)`。输出 `I_photo=k_photo*m_k(x,y)*I_cal` 仍是线性光度图像，但比例场的真实物理单位、空间支持与共同参数误差必须进入下游；星等 `m=ZP-2.5log10(f)` 只作为派生展示。对于跨滤镜产品，不存在可不加色彩变换直接比较的单一物理通量。

稳健位置采用 Tukey biweight/IRLS，要求独立报告初值、固定/更新尺度的区别、损失函数、收敛、样本数与离群过滤。空间增益模型以零阶作为基线，最多试低阶多项式；阶数由留出恒星和空域覆盖决定，不能仅因训练残差降低便接受。

### 2.1 XP 光谱积分、真实被估计量和单位

Gaia sampled XP 外定标谱以 F_lambda（W m^-2 nm^-1）给出；T(lambda)为总透过、Q(lambda)为探测QE（均无量纲）。在将波长、h、c统一到SI后，单位面积入射光子率为
\[
q_i=\int F_{\lambda,i}(\lambda)\,T_k(\lambda)Q_k(\lambda)\frac{\lambda}{hc}\,d\lambda
\quad[\mathrm{photons\ m^{-2}\ s^{-1}}].
\]
本仓旧S04/SCIA使用的 F_syn = integral F_lambda T Q lambda d(lambda)，若lambda以nm计算，量纲为 W m^-2 nm，属于相对测光可用的 proxy，**不是q的数值或量纲**；固定倍率可被逐帧截距吸收，但绝对零点、响应转换和源通量的物理身份不能因此自动成立。进一步换算到电子、ADU还需要口径、曝光、大气、转换增益的定义和来源。Gaia XP 已绝对标定的谱不能再乘10^(-0.4 G)。具体XP sampled网格343点、336–1020nm与广义有效域约330–1050nm的表述不同，超出实际可用谱域的响应、窄线或SED缺项必须登记，不能端点常值外推后仍宣称合规。负对照要用解析常量/一次谱、已知响应、能量/光子两种响应积分和不等距lambda，独立复算积分并检查#12不等距输入的失败语义。

### 2.2 原生PSF、空间增益和源幅度的三个不同对象

令P_j是某源在探测器像元j的单位总光子响应（真正像元积分PSF而不只中心采样），Σ_all P_j=1；有限观测窗可能仅有Σ_win P_j=η<1，并随 seeing、星位、波段、饱和/混叠、亚像元相位、WCS残差变化。孔径和本征PSF拟合的残差不能自动解释为纯粹平场增益，PSF尾部外推/局部sky扣除必须独立评估。对原生解析总通量F_native和标度场alpha_j，本帧应用后的像元真值为 x'_j=alpha_j F_native P_j。以星中心u_*的alpha_*=alpha(u_*)定义局部参考振幅F_ref=alpha_* F_native，必须使用 h_ref,j=(alpha_j/alpha_*)P_j 作为与该参考幅度配对的模板。而实际应用后总通量是
\[
 F_{\rm applied,total}=F_{\rm native}\sum_j\alpha_jP_j
 =F_{\rm ref}\sum_jh_{\rm ref,j}.
\]
二者并不恒等。S1严格解析例P=(.25,.75)，alpha=(1,3)，alpha_*=2，F_native=100得F_ref=200、F_applied,total=250；再有窗口漏尾时第三种观测量F_window=Σ_win x'_j仍不同。把后两者误称F_ref会令P2点源条件SNR、局部权重与P5匹配星增益同时偏离同一estimand。当前源码应从p1_sources实际PSF拟合的flux estimator、PSF model、WCS和quality记录入手比对，不能凭字段同名定义单位。

### 2.3 P1真实应用、BUNIT与随机公共误差

CURRENT函数链：module_adapters.cpp::p1_op_photometry从p1_sources.json及校准帧构造测光解和逐帧应用平面；p1_op_drizzle检查photscales，存在标度时必须读取对应photoapplied帧，并将nm_data_scale按其实际frame_photscal传给噪声模型；源几何由WCS和PSF端提供；随后astro_sphere_sink.cpp::write_hips_phase1与p1_op_writer发布HiPS和frame_science。该实际数值处理与p1_stack.json仍硬编码ADU/sr的声明是Issue#9的SOURCE_CONFIRMED冲突，并非论文中能默认忽略的格式细节。假定alpha只是无量纲relative scaling时必须把normalization/reference_id与scale_state保存；有量纲转换时必须把integrated linear_unit=L、HiPS BUNIT=L/sr、方差单位L²/sr²和同一response ID一起转化，不能把值已变的数组伪装为纯native ADU。

固定alpha的条件协方差是 C'=D_alpha C D_alpha^T；当alpha从与science sources共用的星测量估计时，以J_theta为对参数的Jacobian，边际一阶还需 J_theta C_theta J_theta^T及 D_alpha Cov(x,theta)J_theta^T和其转置；同Gaia源/主平场/光度零点跨帧参数还会诱导交叉协方差。历史用同星PSF vs aperture残差构造sigma_psfsys，再与sigma_fit_robust直接平方相加（Issue#51），没有证明二者的共享噪声不重计；其预算只能记CONDITIONAL/PARTIAL，不是严格独立总方差。参数未知或缺模型以UNKNOWN明示，不能把0当零测量误差。

### 2.4 面亮度Drizzle和HEALPix面积归一

源像元x_j单位L，未收缩球面面积Ω_j，drop缩小面积A_drop,j，与目标球面HEALPix叶重叠面积a_jp，定义w_jp=a_jp/A_drop,j。积分类分子F_p=Σ_j w_jp x_j，面积归一N_p=Σ_j w_jp Ω_j，正确输出面亮度B_p=F_p/N_p（L/sr），而不是F_p除以几何cover或直接写成L。现行sink还计算D_p=Σ_j a_jp，并以k=D_p/N_p将flux_sum=F_p*k、covered_area=D_p送AIO，最终signal=F_p/N_p；variance numerator同乘k²。若使用固定线性算子R，则C_B=R C_x R^T，独立源像元才可取其对角简化Σ_j R_pj²v_j；空间标度和sky参数随机时还加共享项。HiPS support会对D_p/Ω_leaf裁剪为≤1，不能用被裁剪值反演真实归一面积。#15的实际四角近似并非完整HEALPix曲边，内部通量分配和真实球面像元几何不是同一命题。P1的真值验证必须同时检测有限PSF窗、总通量、孔径协方差和边界/极区，而非单点RMS。


## 3 误差传播与判据

以星等残差 `d_ki=-2.5log10(f_ki/q_i)` 的留出散度为被估量，分解（至少）光子/读噪、Gaia XP 绝对谱、通带响应、背景拟合、PSF/光圈、空间平场、色差与探测器非线性。各项需说明是**帧内随机量、逐帧共同项还是跨帧系统项**；共同误差不能直接假设相互独立并逐颗平方相加。未独立估计的项以 `UNKNOWN` 记录。任何依赖受检残差自身的 `sigma_flat` 不能回灌阈值形成自指通过。

复现既有判据时同时保留 `sigma_obs`、`sigma_ceiling`、入选星集合/训练留出划分、具体预算分量和计算代码；本文只在独立测量验证其预算与检测功效以后，才把统计上界用于通过/失败裁决。特别是 `sigma_flat=0.0007 mag` 暂为**仓内未验证的预算假设**，不应被称为来自一手文献的标定常数。对小匹配星数同时报告有效自由度、稳健估计量有限样本误差及 `NOT_IDENTIFIABLE` 状态。

### 3.1 统计判据的识别与条件层级

历史sigma_obs = 2.5 MAD(log10(f/F_syn) - robust_location)/0.6744897501960817 [mag]，其中inliers由所用方差图、质量过滤和IRLS共同决定。S04的订正前后数字不是固定相同受检恒星上的两个相互可比的标准差：参照帧样本从157/151内点变为178/171内点。被称为sigma_ceiling的历史量是已登记误差分量相加形成的**下包络**，并非实际总不确定度上限；ABOVE_CEILING只能推出存在该预算尚未捕获的散度，不能据此判断真实光度“失败”。sigma_flat=0.0007mag来源未获实测确认；PSF同源差分、测光拟合残差、Gaia零点、flat和sky共享参数不能未经协方差辨识当成独立方差相加。若检测和拟合内点由残差确定，则用同内点散度检验自设上界产生选择效应；S04的1.5MAD进一步截尾能把判红翻成PASS，是检测判据能力的负例，而非优选手段。每个CI要对同星跨帧聚类，并报告训练/验证星集合、sky/PSF/颜色分层、稳健有效样本数和上游模型条件。


## 4 数据与实验设计

**表 2 数据、仪器、帧集与许可**

| 项 | 值 | 来源 |
|---|---|---|
| 数据集 / 天区 | `testdata/M42_T2T3_mosaic_Flying_dutchman`，M42 M1 panel | `实验/shared/data_catalog.json`（906 帧枚举） |
| 参照帧 | T2 `M42_M1_...20251212@012404-300S-Red`，`CRVAL=(83.2833557851, −6.37428025059)`、`CRPIX=(2048,2048)` | 帧头 FITS 头，逐帧读 |
| 合规帧集 | 8 帧：T2 两台（含 20251224）、T3 六台；与参照帧 `CRVAL` 最大球面距 `0.003368°`（断言容差 `0.01°`） | `code/s04/s04_inventory.py` |
| 判输入不合格 | 35 帧：同 `(dataset, Red, 300 s)` 但属 M2–M6 其它 panel，`CRVAL` 距 `0.934°–1.13°` | 同上 |
| 缺全部几何键 | 31 帧，逐帧登记、不猜补、不进分析 | 同上 |
| 望远镜 / 相机 | Telescope2、Telescope3；FLI + KAF-16803（QE 曲线 `config/response_curves/KAF_16803.json`） | 帧头 + 曲线库 |
| 滤镜 / 曝光 | Red（Baader R × KAF QE）；`EXPTIME=300 s` | 帧头 + `config/response_curves/Baader_R.json` |
| 像元尺度 / 视场 | `0.96688″/px`；4096 px ⇒ FOV `1.1001°` | 由 `CD` 矩阵导出 |
| 参考光谱 | Gaia XP 锥缓存，n=827，343 点 @2 nm | `run/SCI-401/data_cache/gaia_xp_real_8328_-637.csv` |
| 许可 | `internal-not-redistributable`，仅索引、不上传原始 FITS | `实验/shared/data_catalog.json → data_license` |
| 线性/标度元数据 | 帧头无 `GAIN/RN/DATAMAX`；`gain/read noise/饱和电平` 由硬编码给出，来源 `UNVERIFIED` | 帧头扫描 |

**表 3 实验设计、假说与变量**

| 实验 | 数据和控制 | 关键指标 | 能证明/不能证明 |
|---|---|---|---|
| P1-A 解析 oracle | XP 光谱、封闭式平坦/高斯/已知多项式响应，0 噪/已知噪；单位与常数谱积分反例 | `k_photo` 相对误差、积分守恒、残差分布、色彩斜率 | 算法代数正确；不等于真帧通过。尚未复跑 |
| P1-B HST 结构前向 | HST M16 drizzled图作为真实结构模板（仅在有效掩膜内），注入已知星通量、PSF、泊松、读噪、通带/空间增益及饱和；HST coadd不是原始无噪声真值 | 零点偏差、95% 区间覆盖、场残差、光学位置偏差 | 模型在已声明前向世界有效；不证明真实探测器线性。尚未复跑 |
| P1-C 真实帧 | 8帧合规子集；Baader R × KAF QE；二阶空间场；候选434星在不同帧入选比例不同，CI和分母按互异星而不是观测行处理 | `σ_obs`、颜色/星等/位置分层残差、留出星预测差、重复位置对照 | 直接否证预算下包络；不因单一门值就给出具体物理成因 |
| P1-D 负对照 | 错滤镜/QE、重复施加 G、颜色项截断、过拟合空间场、近饱和源、错误 FITS 标度 | 检出功效、误拒率、是否失败关闭 | 验证门不是恒绿；不能利用调松门获得成功 |

判据式（照正本，未改一行）：`sigma_obs = 2.5·MAD(r_inliers)/0.6744897501960817 [mag]`；
`sigma_ceiling = (1 + 3·1.166/√n)·√(σ_fit_robust² + σ_psfsys² + σ_color² + σ_gaia² + σ_flat² + σ_skyres² + σ_q²) [mag]`；
判序 `ABOVE_CEILING → LOWER_BOUND_UNDEFINED(n≤12.236) → BELOW_FLOOR → PASS`。

分别输出星点数量与抽样方式（训练/留出）、通带信息、拟合参数、bootstrap 置信区间、残差诊断图、探测器线性有效区间。星点按物理观测单元分组以免同一对象同时进入训练和验证。真实帧不得靠文件名子串选取数据集。

### 4.1 HST/M42的真实数据身份、有效区域和物理前向边界

HST M16源图F657N/F673N/F502N为drizzled天体空间结构模板，见../shared/data/real/m16_scene_index.json；其中三band有效像元比例约0.8975，valid/flags由../shared/synthetic/m16_mask.py重建，实际mask FITS与大体积合成数据在Git忽略的run目录中。HST coadd中的历史噪声、残留坏点和滤镜谱不可能被不加条件称作“原相机的无噪声真值”或任意Red/H-alpha响应真值；实验B若要有严格真值应只在valid区域建立已知星SED/注入PSF、曝光和独立Poisson/read/flat/seeing噪声，成对比较而非把HST统计量当原始单帧随机独立误差。

地面M42无绝对天体光度真值。原S04在M1 panel中只接受2台T2+6台T3的Red 300s 8帧（非M2–M6其他panel的35帧），另有31帧缺完整几何而未纳入。Gaia XP缓存827个候选、同帧实际星候选434、八帧存活比例0.106–0.410；8帧734观测行只有177互异星，所以CI分母不能取734独立。历史脚本和图的hash可追踪到code/s04与figures/FIGURES_MANIFEST.json，但run/ACSDSDOC-02/S04/和各单元results/未被跟踪入main，原FITS仅在仓外testdata；本B09没有重新打开这些run树，因此报告中的数值为HISTORICAL_REPORTED_WITH_CODE而不是B09新实测。图1至图7只能作相同历史版本的有出处图，不是2026-10-17当前C++已经通过的测试图。


## 5 结果、证据级别与失效域

### 5.1 读数口径（代码版本声明）

真实帧散度 `σ_obs` 不是与实现无关的量：`step8` 判据中逐像素方差在**方差式订正**前后取不同形式（订正前按 `clip(img,0,None)+RN²)/g²`，订正后按 `img/g+(RN/g)²`）。这改变了 IRLS 拟合权重，从而改变进入判据的样本集，因而改变 `σ_obs` 与 `σ_ceiling`。**参照帧的 `0.02651984 mag` 对应方差式订正之前的代码版本（可逐位复现，n=157/151），本版代码给出 `0.02687307 mag`（n=178/171）**；第二帧由订正前的 `0.02685309 mag`（n=128）变为本版的 `0.02528333 mag`（n=143）。两版判定同为 `ABOVE_CEILING`，但数值不可混引。跨变体比较还必须固定预算 MC tag（同帧、同脚本、仅换 tag 时 `σ_fit_robust` 变 `−1.198%`、`σ_ceiling` 变 `−0.320%`）。

`ABOVE_CEILING` 只有一个含义：**该帧实测散度超过了本帧已登记项合成的下包络**。按科学正本，已登记项之和是已测贡献的下包络，不是总误差上界，本分册不冻结绝对门限；因此该判定**不得**读作“该帧科学上不合格”或“定标失败”。

### 5.2 实测数字、误差与适用域

**表 4 核心结果（单位、分母、样本量、seed 与来源脚本）**

| # | 量（定义/单位） | 分母与样本量 | 读数 | 判定 | 来源脚本 |
|---|---|---|---|---|---|
| 1 | `σ_obs` 参照帧（本版方差式）[mag] | MAD(r_inliers)/0.6745；n=178，内点 171 | `0.026873067319721985` | ABOVE_CEILING | `s04_measure.py` |
| 2 | `σ_obs` 参照帧（订正前方差式）[mag] | 同上；n=157，内点 151 | `0.026519837508444995`（与一手归档逐位相同） | ABOVE_CEILING | `s04_measure.py` |
| 3 | `σ_obs` 第二帧（本版/订正前）[mag] | n=143 / n=128 | `0.025283327128051616` / `0.02685308669651848` | 均 ABOVE_CEILING | `s04_measure.py` |
| 4 | `σ_obs` 8 帧全幅 | 各帧 n=43…171 | 本版 `0.0252833–0.0420836` | 5/8 ABOVE_CEILING（订正前 2/8） | `s04_measure.py` |
| 5 | `σ_ceiling` 参照帧/8 帧 [mag] | 已登记项合成的下包络 | `0.01899566508215789`；区间 `0.016690–0.045949` | 非上界，仅作对照 | `s04_measure.py` |
| 6 | 颜色轴系数 [mag / 单位标准差] | 主帧 171 互异星；bootstrap 800–1200 次按星聚类，seed 20261014 | `−0.02467 ± 0.00245`（`−10.05σ`） | 唯一稳健系统项 | `s04_strata.py` |
| 7 | 颜色轴物理口径 [mag/mag(R−B)] | 同上 | `−0.0534` | — | `s04_strata.py` |
| 8 | 跨帧合并颜色轴 [mag/mag(R−B)] | 8 帧去中位；734 行 / **177 互异星**，`−25.5σ` | `−0.0649` | 按星聚类，不得按行计独立样本 | `s04_strata.py` |
| 9 | 跨仪器训练→留出 | T2 拟合 `−0.0573`；留出 T3 互异星 101 | 6 帧中 5 帧 `σ_obs` 降 `15.8–49.3%`，中位 `23.5%`；池化中位 `+25.0%` | 颜色项可跨仪器复现 | `s04_strata.py` |
| 10 | `σ_flat` 独立测量 [mag] | 母版逐像素高通 MAD；`N_eff=12.5176` 折算 / 真实星位置 PSF 加权 / 去二阶空间场后 | `0.000948` / `0.007579`(T2)、`0.009526`(T3) / `0.001766`、`0.002311` | `N_eff` 折算在相关平场上低估约 8 倍 | `s04_sigmaflat.py` |
| 11 | `σ_ceiling(σ_flat)` 翻转阈值 [mag] | 逐帧求解；其中 3 帧对任意 `σ_flat` 不变 | `0.006257–0.016734`（约定值 `0.0007` 的 `8.9–23.9×`） | `σ_flat∈{0, 0.0007}` 判定 **8/8 相同** | `s04_sigmaflat.py` |
| 12 | 重复位置配对 | 8 帧两两配对，**1802 对 / 145 互异星** | 跨仪器斜率 `Δd~Δflat` = `+0.836 [0.283, 1.509]`（与 1 相容）；`Δflat` 只解释 `Δd` 的 `0–5%` | 平场不是配对残差差的主项 | `s04_sigmaflat.py` |
| 13 | 直接施加母版平场后 `σ_obs` 变化 [mag] | 8 帧（同一 `valid` 掩膜内可比） | 3 帧改善 / 5 帧变差；中位 `+0.00122`（**变差**） | 平场净贡献无法被证明 | `s04_sigmaflat.py` |
| 14 | 判据样本存活率 | 同一 434 星候选表 × 8 帧 | `0.106–0.410`；与候选 `χ²` 中位 `r = −0.991` | 跨帧/跨仪器比较**不是同一被估量** | `s04_strata.py` |
| 15 | `σ_pix_e` 独立核算 | 帧内邻元差分 vs 模型 `√(RN²+sky·gain)`；8 帧 | 模型高估 `1.87–2.41×`（中位 `2.07×`）；反解常数基座 `≈947 ADU ≈80%`；`σ_ceiling` 降 `4.3–9.1%` | 用实测值重算后 **8/8 判定不变** | `s04_sigmaflat.py` |
| 16 | 探测器线性区 | 同夜同指向 H-α 300 s/600 s 对；`F∈[51.2, 225749] ADU` | 拐点 `NOT_MEASURED`；已扫范围内单调偏差 `−3.6%/dex`；同夜同曝光对照 `−0.43%/dex`（与 0 相容）、异夜同曝光 `−1.58%/dex` | 偏差**不能归因探测器** | `s04_linearity.py` |
| 17 | 未饱和信号上限 | stamp 峰值 | `31519 ADU = 42015 e⁻`（gain 1.333 `UNVERIFIED`）= 满井 `48.1%` | 更亮样本两曝光下都已饱和 | `s04_linearity.py` |
| 18 | 负例 N1 错通带/QE | 与正确 Baader R×KAF 比 | Baader G `9.68×`、Antlia V B `16.15×`、Baader B×Ideal QE `18.57×` | 三例均 ABOVE_CEILING（判红） | `s04_negatives.py` |
| 19 | 负例 N2 G 双计 | 参照帧 | `σ_obs = 0.6817806791479255 mag = 25.37×` | 判红 | `s04_negatives.py` |
| 20 | 负例 N3a 颜色项训练→留出 | train 86 星 / 留出 85 星 | 留出 `σ_obs 0.027627→0.021303`（`−22.89%`） | 判红 | `s04_negatives.py` |
| 21 | 负例 N3b 残差截尾（禁止路径） | 按 `k·MAD` 截尾后重算上界 | `3.0/2.0 MAD` 仍 ABOVE_CEILING；`1.5 MAD` → **PASS**；`1.0 MAD` → PASS | `flips_to_pass = true`：判据样本必须在截尾前冻结 | `s04_negatives.py` |
| 22 | 负例 N4 空间场阶数过拟合 | 0…6 阶，训练/留出不重叠 | 留出最差/最好 `1.80×`；3 阶起单调变差；最大系数幅度 `0.9647 mag` | 3 阶起判红；预声明“样本内单调下降”被证伪 | `s04_negatives.py` |
| 23 | 负例 N5 错误 FITS 标度 | 二次 `BZERO` | 天空 `1202→33970 ADU`（`0.518×`满井）、`σ_pix_e ×5.20`、`σ_ceiling ×1.61` | 判红；一个读盘错误足以把判红翻成判绿 | `s04_negatives.py` |
| 24 | 空间场样本几何 | 主帧判据星点 `r ∈ [54.6, 1117.2] px` | 等效面积占全帧 `23.4%` | 圆盘外属**外推区** | `s04_strata.py` |

样本量与 seed：逐星表 `3 665 行 × 22 列`；帧测量 seed `20260921`，bootstrap seed `20261014`，预算 MC tag `s04`。原始产物（逐帧读数、逐星表、σ_flat/线性/负例 JSON、逐图）在 `run/ACSDSDOC-02/S04/`；脚本与输入哈希台账（7 个脚本 + 两个代码版本的 `step8_real_frame.py` + 输入清单 SHA）见 `REPORT_experiment.md`、`code/s04/README.md` 与 `figures/FIGURES_MANIFEST.json`。

### 5.3 图

- **图 1（待跑）** 实测系统/算法概念图：SP1 全链的道次与单位接口尚未有可复算的绘图数据，本篇只保留图题，不绘制示意图形冒充结果。
- **图 2** 预声明核心统计量的逐帧臂比较：`figures/fig1_sigma_obs_per_frame.png`（8 帧 `σ_obs` 与两套下包络，含两版方差式对照）。
- **图 3** 阴阳对照/失败域：`figures/fig7_negatives.png`（五个负例）与 `figures/fig4_sigma_flat_sensitivity.png`（`σ_flat` 灵敏度与翻转点）。
- **图 4** 适用域/成本：`figures/fig3_selection_psf_mismatch.png`（判据样本存活率 vs 候选 `χ²`，跨帧可比性的限制曲线）。
- 补充实测图：`figures/fig2_residual_strata.png`（残差分层）、`figures/fig5_repeat_position.png`（重复位置）、`figures/fig6_linearity.png`（已扫信号范围的 600/300 比值，**不含**线性区拐点）。

全部图的生成脚本、seed 与逐图哈希见 `figures/FIGURES_MANIFEST.json`。（图文件名沿用生成脚本的编号，与本文图号不同。）

### 5.4 证据级别

| 主张/读数 | 实际达到的等级 | 依据 | 不成立的等级与原因 |
|---|---|---|---|
| 文献、DOI 与先例边界（表 1） | `BIB_VERIFIED` | 书目核验登记 | 正文无式号/页码断言，故不主张更高的文献级证据 |
| §2 的单位与积分定义 | `MATH_DEFINITION + BIB_VERIFIED` | 光子率q必须含1/(hc)，既有S04未除常数的F_syn仅是光谱响应代理积分；二者不可混称 | 当前C++有量纲谱积分和GaiaXPy独立oracle均未在本轮运行，不给ORACLE_CHECKED |
| 判据的脚本复演（Python，不链接生产 C++） | `SELF_CHECK` | 逐帧读数、逐星表与负例可在本机复算 | 独立 Python 重演**不**构成生产验证，不得据此主张 `IMPLEMENTATION_CHECKED` |
| 真实帧 `σ_obs`、`σ_ceiling`、分层、颜色轴、`σ_flat`、重复位置、线性扫描（表 4 第 1–17 行） | `EXPERIMENT_CHECKED` | 合规真实帧枚举、输入清单与 SHA、脚本 SHA、seed 与 bootstrap 区间可溯源 | 生产 C++ 路径未被调用，`IMPLEMENTATION_CHECKED` **不成立** |
| P1-D 五个负例（表 4 第 18–23 行） | `EXPERIMENT_CHECKED` | 注入方向先声明后判红，含 `flips_to_pass` 禁止路径 | 注入发生在判据的脚本复演内，非生产库故障注入 ⇒ 不主张 `IMPLEMENTATION_CHECKED` |
| 公布可信度/发布验收 | — | — | `RELEASE_VERIFIED` **不成立** |

### 5.5 跨篇误差传递反例（P1 → P5）

按统一实验协议的第四条证据腿，需要一条“上游缺陷传播到下游、下游判据必须能判红”的可复算用例。**本篇该用例 `NOT_RUN`。** 已有的局部反例只能覆盖单篇内部：P1-D 的错通带/G 双计证明 P1 内部门能判红（`9.68–25.37×`），但不能替代跨篇传递。缺什么：① 一条冻结的 P1→P2→P3→P4→P5 数据链（同一相机/滤镜/天区、明确点源或弥散源被估量、逐步单位与协方差登记）；② P1 标度偏差注入后 P2 的覆盖判据、P3 的守恒判据、P4 的支持域判据、P5 的接缝/光度判据各自的**预声明红灯**读数；③ 该链所需的 P2 条件 SNR 与 P4 真实控制点端到端均尚未完成。因此跨篇「判红」链路在本篇只能登记为待跑。

### 5.6 B09-D1主张—结果证据身份：不可将旧PASS提级

| claim | 历史读数和实际分母 | 原代码/产物及反例 | 本轮可主张范围／缺口 |
|---|---|---|---|
| P1-C01 XP响应 | F_syn是未除hc的proxy；Gaia sampled343点336–1020nm | code/scia_calib.py、code/s04/s04_common.py、refs.md；历史Baader R与Antlia B误选见RESOLUTION_m42_curve_resolve.md | 相对零点可用同约定积分；绝对光子率、带外星云、响应归一须独立参考实现 |
| P1-C02 真实散度 | 原订正前0.0265198375mag，157匹配151内点；历史修正后0.0268730673mag，178匹配171内点；下包络0.0189956651mag | code/s04/s04_measure.py、REPORT_experiment.md §11；真实frame FITS当前本轮未重读 | **不同版本/不同入选集合**，不能认定同分母性能提升/下降，更非生产E2E |
| P1-C03 颜色 | 主帧-0.02467±0.00245mag/标准化轴，171星；全八帧734观测行/177互异星物理斜率-0.0649mag/mag(R-B) | code/s04/s04_strata.py，星级bootstrap种子20261014；跨仪器留出101互异星 | 实际入选星模型中颜色残差显著；不能外推所有SED、所有PSF或声称物理跨通带同一 |
| P1-C04 flat与选择 | 去二阶后0.001766/0.002311mag，完整flat0.007579/0.009526mag；8帧3好5坏中位σ_obs增加0.00122mag | code/s04/s04_sigmaflat.py，实master及同星配对1802行/145星；sigma_flat=0.0007mag是未校准约定 | flat引起的散度需与PSF和场参数共享项分离；不能用独立假设的一常数自证 |
| P1-C05 gain/线性 | 1.333e-/ADU、8.59e-来自硬编码；Hα300/600趋势-3.6%/dex，控制-0.43%/dex | code/s04/s04_linearity.py，实相机无gain/RN头；只扫可达未饱和区 | 仅可报告特定曝光对趋势，探测器非线性拐点和真实electron标度NOT_MEASURED |
| P1-C06 阴性 | 错通带9.68/16.15/18.57×，重复G25.37×，1.5MAD截尾可将判红翻绿 | code/s04/s04_negatives.py；对照是旧Python判据不是生产C++ fault injection | 能报告检测功效与可作弊选择效应；当前生产级判别力NOT_RUN |
| P1-C07跨篇与单位 | 生产photoapplied按frame_photscal进Drizzle，但p1_stack及properties可写死ADU/sr | module_adapters.cpp::p1_op_photometry/p1_op_drizzle/p1_op_writer与astro_sphere_sink.cpp；Issue#9/#10/#13，S1数学 | STATIC_CONFIRMED单位合同不一致；真实球面BUNIT/Var/响应转换与P1→P5负例E2 NOT_RUN |

所有“±”或CI必须标明seed/cluster层、实际独立样本数以及是否覆盖共享XP/仪器零点；历史颜色-10σ只是旧回归模型内的统计量，**不是已计入全部系统协方差的科学置信度**。每张主要图需有脚本、生成输入、原始图hash和实际mask，失去raw run树时须从同源FITS重跑并与旧版本严格并列，禁止通过更改样本截尾制造“改善”。


## 6 讨论、结论与适用域

目前可成立的是**方法定义、此前真实帧历史报告的可追踪读数与历史脚本负例**；仓外逐星run产物和现行C++未在本轮重跑，不能把旧EXPERIMENT_CHECKED升级为当前版本PASS，不成立的是“真实相机数据已经符合独立误差预算”的结论。可能的论文主张是：在 Gaia XP 已知先例下，ACSD 将相对规范、单位传播、空间场可辨识及三类观测反例结合为面向大规模球面归一化的接口与验证框架。能否把该主张写成“达到 X mmag”必须等待独立数据的结果、样本数量与可信区间。

**本篇必须同读的诚实限制**

1. 参照帧 `σ_obs = 0.02651984 mag` 对应**方差式订正之前**的代码版本；本版代码给出 `0.02687307 mag`（n=178）与 `0.02528333 mag`（第二帧）。跨版本、跨 tag 的数字不可混引。
2. `ABOVE_CEILING` 只能读作“超过本帧已登记项合成的**下包络**”，不得写作“科学不合格/失败”。本分册不冻结绝对门限，已登记项之和不是总误差上界。
3. 判据样本集由拟合质量筛决定：同一份 434 星候选表在 8 帧上的存活率为 `0.106–0.410`，与候选 `χ²` 中位数的帧级相关 `r = −0.991`（固定的 `FWHM=1.98 px` PSF 模型与帧头 `2.19–3.02 px` 失配是候选之一，但不主张单一成因）。**跨帧/跨仪器直接比较 `σ_obs` 不是同一被估量。**
4. 探测器线性区 `NOT_MEASURED`：饱和前的高信号段不可达；已扫范围内 `−3.6%/dex` 的比值趋势可由同夜/异夜曝光对照复现，不能归因于探测器。
5. `gain / read noise / 饱和电平` 在帧内无关键字、由硬编码给出，来源 `UNVERIFIED`；本文的 `e⁻` 换算与满井比例随该假设整体平移。
6. `σ_flat = 0.0007 mag` 是仓内未验证的预算假设：`docs/` 全树检索零命中，唯一出现处为 `code/scia_calib.py`。独立测量给出 `0.0018–0.0023 mag`（去二阶空间场后）与 `0.0076–0.0095 mag`（完整口径），而该假设的翻转阈值是约定值的 `9–24` 倍，且 `σ_flat∈{0, 0.0007}` 的判定 `8/8` 相同 ⇒ **该约定既未被证实也未被证伪，且在本帧集上不裁决任何判定**。
7. 母版平场施加后的**残余平场误差** `NOT_MEASURED`（缺 repeat-flat / sky-flat 对照）；绝对透过率与大气消光 `NOT_MEASURED`（同夜 7–11 分钟内透过率已漂移 `5–6%`）。
8. 尚未复跑 P1-A 解析 oracle 与 P1-B 物理前向腿；生产 C++ 光路路径 `NOT_RUN`，故本文不主张 `IMPLEMENTATION_CHECKED`。

需要完成的最小实验：把真实帧链路接到生产 C++ 路径并注入同一组故障；独立测量母版平场施加后的残余；取得可达饱和前高信号段的线性区读数；用不重叠训练/留出在第二套仪器数据上复核颜色轴；使真实帧判据与下包络的计算由另一个操作者独立复现。

### 6.1 审稿可发表边界与下一轮最小实验

可以限定地陈述：Gaia XP参考测光在本项目形成了可复现的历史相对零点链，真实M42受检8帧显示显著残余颜色、PSF/flat/候选选择问题；校准预算的已登记项不足以解释全部历史星等残差，且当前生产写出单位/响应身份尚未得到一致性证据。**不可**称已创造Gaia合成测光新技术、达到mmag、已实现真实跨波段物理同一化、得到真实噪声置信度或P1→P5端到端科研闭合。

下一批的成对设计：冻结现行main的生产构建CLI/DLL哈希、实际输入occurrence与原始FITS/master/GaiaXP+cov/响应哈希、每帧WCS/SIP及质量掩膜；先用GaiaXPy+解析谱/已知滤镜在同一光子/能量定义下对拍；再固定同星和相同PSF积分条件跑native与applied P1，检查逐像元ratio=alpha(x)、参考幅度与总通量的关系、signal线性单位、BUNIT、variance的平方缩放、真实HiPS面积和天体点源flux；分别在HST valid区注入已知光谱/PSF/噪声与在M42真实frame做不重叠星/帧留出、星级bootstrap和可信区间覆盖。接着把同一源标度误差注入P2/P3/P4/P5并检查实际检出能力。P1以前的图表与这一新版E2绝不合并数据列。引用优先保持Gaia Collaboration/Montegriffo外定标与合成测光、Xiao的XP CMOS先例、Padmanabhan ubercal、Akima积分与稳健M估计文献的可验证书目级说明；一手全文逐算法和参数逐页核对留给B10，不能编造原文页码和未读公式。


## 参考文献（核验主干）

[R01] Gaia Collaboration / Montegriffo et al. (2023), A&A 674 A33, [DOI:10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709)。

[R02] Xiao et al. (2025), ApJL 982 L27, [DOI:10.3847/2041-8213/adbd3c](https://doi.org/10.3847/2041-8213/adbd3c)。

[R03] Padmanabhan et al. (2008), ApJ 674 1217, [DOI:10.1086/524677](https://doi.org/10.1086/524677)。

[R10] Huber (1964), Ann. Math. Statist. 35 73, [DOI:10.1214/aoms/1177703732](https://doi.org/10.1214/aoms/1177703732)。

共有引文状态、BibTeX 与不能由文献外推的断言见 `../shared/references/CORE_REFERENCES_VERIFIED.md`。
