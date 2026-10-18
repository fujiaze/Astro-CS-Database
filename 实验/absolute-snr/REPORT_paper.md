# P2｜参考点源 Fisher 信息、球面统计精度与真实跨帧覆盖率：科学模型和可证伪实验

> **B09-D2，2026-10-18，SCI 科研工作稿。** 对原 `实验/absolute-snr/REPORT_paper.md` 进行正文实质重构，保留 S05 原始 22 项结果表、原负例和原实验身份，不将其历史 `PASS` 提升为最新生产验证。原实验见 `REPORT_experiment.md`、`docs/S05_REPORT.md`、`docs/S05_P2_evidence.json`、`code/s05/`。S05 任务起点 `9f1e99f2a5ace6965b7b427743eed8c4242945df`，记录的运行时 HEAD `0ddd5a71085111509a5bcb2cf73f8dc7a29ca495`。本轮未重新执行原始 FITS、NPZ、蒙特卡洛或现行生产二进制，未精读 B10 的外部全文。**历史作者报告、可独立证明的解析命题、当前静态源码与尚未运行的科学验证是四种不同证据。**

## 摘要

跨帧参考信噪比仅在参考通量、像元积分 PSF、背景 nuisance、仪器响应、噪声及空间支持被固定时具有可比较的统计含义。本文首先以点源幅度 `F_ref` 的条件 Fisher/GLS 方差 `V_F` 定义参考 SNR，再从真实光度应用、球面 Drizzle、源 Poisson 和共享标定协方差分析它何时能转换为真正面亮度方差 `V_B`；背景 robust patch-median 方差 `V_ctrl`是第三种独立统计量。历史 S05 独立 NumPy 合成实验的45个模型格点、每格20,000次试验，报告 `sd(Fhat)/sigma_F=0.9929–1.0095`；已知背景换成自由常数背景后方差代价为1.85%–8.16%，一个重采样相关噪声对角近似受检构型的方差低估达48.58%。两夜 M42 Red 帧的2195个匹配星身份显示 `sd(Δmag)/median(sigma_analytic)=3.3701`；1087个留出星在原条件方差下68.27%覆盖率仅0.4471，训练样本拟合经验地板后达到0.8611，反而**过覆盖**。旧背景 `k_corr` 标定表仅覆盖源300/600角秒/像元、nside512，而近0.967角秒/像元的旧模型及生产重叠权重探针所得2.3274、2.0542–2.0580都大于工程回退1.4，不构成新的全局常数。现行 `frame_science` 序列化仍为v1，Mosaic仍消费legacy sparse/帧级 SNR，点源信息到真实面亮度逆方差尚无完整 `T` 转换。本文贡献是一套明确被估计量、量纲、误差来源和可证伪实验的科学重构，**不是**发明 Fisher/GLS/Drizzle 或证明真实多设备已达普适绝对置信度。

**关键词：** 参考信噪比；广义最小二乘；背景 nuisance；Poisson；Drizzle；面亮度方差；相关噪声；经验覆盖。

## 1 研究目的、先前方法和创新边界

科学问题分为四个互不替代的命题。**H1**：给定真实像元积分 PSF、固定参考通量、SPD噪声协方差和正确背景模型，点源幅度GLS的解析方差与合成曝光的方差/覆盖是否一致，nuisance和模型秩不足怎样判错？**H2**：是否能从同条件 `V_F` 通过真正 Drizzle/R、PSF、背景及源场景得到球面 `V_B`，而不把数值上可计算的 `SNR²/F_ref²` 直接当像素 inverse variance？**H3**：Poisson、读噪、flat/master、sky、PSF和跨帧共同光度参数产生的联合不确定度，在真实留出星上是否拥有68/95%概率覆盖与明确条件？**H4**：Normalize实际写什么schema，Mosaic真的读了什么模型，未知variance会否冒充真实精度，HiPS/Export是否保留单位和相关性？

先例定位如下，只有书目级核验而非一手逐式全文审查：

| 方法来源 | 既有科学思想 | 本文可研究的真实增量与禁止冒认 |
|---|---|---|
| Newberry (1991) [R04] | CCD光子/天空/读噪测光误差 | 验证已扣背景、拟合背景与共享校准参数的不同协方差口径；不声称发明CCD噪声公式 |
| Horne (1986) [R13] | CCD光谱轮廓最优抽取 | 把被估计量、profile与权重的一致性用于二维像元积分PSF，不能冒称原始光谱方法就是ACSD球面算法 |
| Zackay & Ofek (2017) [R05] | 已定义目标及噪声条件下的最优图像合并 | 比较点源信息、实际场景面亮度与联合共享参数BLUE条件；不将其最优算法当本项目创新 |
| Fruchter & Hook (2002) [R06] | 重采样几何及相关噪声 | 按真实球面算子和背景median校准输入/输出统计对象；不重新发明Drizzle |
| Gaia DR3 XP参考响应与S1 | 在实际光度响应和参考SED条件下建立标度 | 显示相对标度、源通量、Poisson、协方差和球面产品身份是否同源 |

本项可投稿方向是**跨阶段具有可测错误上界的条件参考精度合同及其真实科学适用域**。单独推导Fisher等式、修改参数名称、图像更平滑、历史脚本出现PASS都不足以证明实质新颖性。B10须对上述外部原文的方程编号、模型条件、实验设计与开源实现逐篇核查，不虚构已经审过的引用语句。

## 2 被估计量、单位与参考观测模型

### 2.1 六个不同统计对象

令 `L` 是当前数据真正已应用的**积分线性单位**，可能为原生ADU、具名相对尺度或另有响应模型的物理通量；只凭数值乘了系数不能称已实现SI物理绝对定标。

| 对象 | 数学定义与主要前提 | 单位 |
|---|---|---|
| `F_native` | 原生总光通量的完整解析PSF幅度，不等于有限窗孔径和 | 原生积分量 |
| `F_ref,k(u)` | 冻结参考SED、实际响应、空间比例与PSF的局部幅度参数 | `L` |
| `F_actual` | 本次真实天体与滤镜/曝光/SED下的实际源通量，通常与`F_ref`不同 | `L` |
| `V_F,k(u;F_ref)` | 同一参考幅度估计器在给定模型及噪声下的条件方差 | `L²`，`I_F=1/V_F`为`L⁻²` |
| `B_k(p), V_B,k(p)` | 球面叶p的实际面亮度和同一输出估计量的方差 | `L/sr`, `L²/sr²`，`I_B=1/V_B`为`sr²/L²` |
| `m_ctrl,V_ctrl` | sky sampler保留像元patch的稳健median与**该median的估计方差** | `L/sr`, `L²/sr²`；`control_ivar`为`sr²/L²` |

`SNR_ref=F_ref/sqrt(V_F)`和`SNR_actual=F_actual/sqrt[V_F(F_actual,scene,...)]`虽无量纲但条件不同；也不可将`variance_bg`（背景真实shot/noise）与`V_ctrl`（统计估计器方差）混称。即使`V_B`和`V_ctrl`单位相同，也不是同一随机变量。合法缺精度、零值、无覆盖、损坏要有分别的状态，不能用1或0填未知gain/variance后宣称测到了信息量。

### 2.2 P1施加空间比例与像元积分PSF

设原生单位总积分PSF响应为 `P_j`（真实像元积分，`Σ_all P_j=1`），在源位置 `u`处 `α_*=α(u)`、每源像元应用`α_j`。正确位置参考参数为
\[
 F_{\rm ref}=\alpha_* F_{\rm native},\quad
 h_{{\rm ref},j}=(\alpha_j/\alpha_*)P_j,\quad
 E[y'_j\mid F_{\rm ref}]=F_{\rm ref}h_{{\rm ref},j}+(Z\beta)_j.
\]
实际应用**总通量**为 `F_applied,total=F_native Σ_all α_jP_j`，通常不等于`F_ref`，也不等于有限窗口孔径和`F_window`。S1的反例 `P=(.25,.75), α=(1,3), α_*=2, F_native=100`：`F_ref=200`而`F_applied,total=250`，不可将250代入200对应的参考模板。有限窗只截取P、`Σ_win P<1`不应悄悄归一后仍叫`analytic_total`。空间比例、PSF尾部、子像元相位、WCS漂移与颜色残差互相耦合，须在科学源身份中显式保存。

若`g`定义为`e⁻/ADU`，原生源Poisson方差是`F_native P_j/g`[ADU²]（背景/暗流各有自身shot项），应用后
\[
 v_{{\rm src},j}'=\frac{\alpha_j^2 F_{\rm native}P_j}{g}
 =\frac{\alpha_j^2 F_{\rm ref}P_j}{g\alpha_*}.
\]
read noise项原生为`RN_e²/g²`，经比例变换配对平方传播。被扣掉的天空均值**不能消灭已有shot噪声**。真实M42仪器所用`g=1.333 e⁻/ADU`、`RN_e=8.59e⁻`的FITS头缺相应关键字，来源为`UNVERIFIED`，因此依赖它们的绝对条件噪声不能写成已独立验证。

### 2.3 条件 Fisher/GLS 的完整假设和数值可辨识性

固定观测域、mask、PSF、响应/标度及参数`θ`，设条件观测模型
\[
 y=Fh+Z\beta+\epsilon,\quad E[\epsilon\mid\theta]=0,\quad
 \operatorname{Cov}(\epsilon\mid\theta)=C_0\succ0 .
\]
`h`为真正的像元积分PSF模板，`Z`包含未知常量或多项式天空以及其它线性 nuisance 列；天空已知时可在y中扣真背景，不应同时将其再作为自由参数。固定C、已知背景时，GLS幅度和方差严格为
\[
 \widehat F=\frac{h^TC_0^{-1}(y-Z\beta)}
                   {h^TC_0^{-1}h},\quad
 I_{F\mid\beta}=h^TC_0^{-1}h,\quad
 V_{F\mid\beta}=I_{F\mid\beta}^{-1}.
\]
当Z未知、`rank(Z)=dim(Z)`、`h∉span(Z)`，则对应同一统计量F的有效剖面信息为
\[
 I_{F\mid Z}=
 h^T\left[C_0^{-1}-
 C_0^{-1}Z(Z^TC_0^{-1}Z)^{-1}Z^TC_0^{-1}\right]h.
\]
上式只有在完整线性高斯均值模型、固定SPD C和正确的Z下才是该模型的Fisher/GLS。`Z`自身列秩亏时须先以其独立列空间代替，`h`在Z列空间时`I_F=0`（不可辨），不得填任意ridge造出高ivariance；对C PSD而非SPD的情形，应以真实支持和精确约束构造一致的伪逆/子空间模型，不能把零噪声方向自动转成无限Fisher。

一个更稳定且可验证的计算方法是`C_0=LL^T`白化，`q=L^{-1}h`、`W=L^{-1}Z`，取`Q_Z=orth(W)`，则
\[
 I_{F\mid Z}=\|(I-Q_ZQ_Z^T)q\|_2^2.
\]
使用独立pivoted QR/SVD对**具物理量纲的列规范**判断rank，输出奇异谱/κ、求解残差和误差界；正常情况下可用稳定Cholesky，不应直接求正规矩阵显式逆。相同数据内估计天空并随后宣称“背景真值已知”会丢掉自由度而使置信区间过窄。若对Z或F加入先验，则估计量已变成penalized/posterior，不能引用原线性无偏频率学方差当完全相同对象。

### 2.4 源Poisson依赖与真正Fisher的信息层级

如果`C(F)=C_base+C_src(F)`，用`F_ref`固定噪声矩阵得到的`h^TC(F_ref)^-1h`是**指定源模型下的工作GLS信息**，不能无条件称真实Poisson+Gaussian似然的完整Fisher。即使假定参数依赖协方差的Gaussian似然，完整Fisher仍应有
\[
 I(F)=h^TC(F)^{-1}h+
 \frac12\operatorname{tr}\!\left[C(F)^{-1}C_{,F}(F)
 C(F)^{-1}C_{,F}(F)\right]
\]
（此处仅对独立于F的h及正确正态近似）。实际Poisson分布和读噪叠加的似然一般不同；高通量的有限样本估计偏差、模型参数估计后coverage均须MC检验。阈值深度`F_q`一般解`F_q/sqrt[V_F(F_q)]=q`；仅在背景主导、`V_F(F)`几乎常量时近似`F_q≈q sqrt[V_F(F_ref)]`。真实场景星云的source shot不能以F_ref做无条件替代。

### 2.5 条件与边际协方差及后选择误差

校准/测光/PSF/WCS/flat/master/sky参数同原始像元x组成一个联合随机向量`(x,θ)`，对输出`f(x,θ)`的线性化：
\[
 \Gamma\simeq J_xC_xJ_x^T+
 J_\theta C_\theta J_\theta^T+
 J_xC_{x\theta}J_\theta^T+
 J_\theta C_{\theta x}J_x^T.
\]
同一母版/共享Gaia零点/从本数据估计的背景可让不同帧产生正或负交叉项；只把每帧独立像元方差按α²变换仅是**θ固定的条件项**，不等于完整边际方差。同星拟合出的PSF残差和光度拟合残差如果包含相同误差来源，不能未经协方差拆解重复平方相加。若mask/rejection/median裁剪/星点筛选由数据本身决定，事后固定mask的`a^TΓa`不保证端到端无偏或68/95%coverage。论文必须依次区别数学恒等式成立、数值oracle成立、生产算法样本条件成立、真实观测频率学覆盖。

**与并行 S3-R 的统计一致性。** 最新 `实验/innovation-closure/S3R_SCIENCE_REPORT_2026-10-18.md` 对多星相对增益 contrast 给出解析 `Cov(C^Tz)=C^TΣ_zC` 和 24,000 次有前提的独立 toy Monte Carlo；这与本论文“共同参数及样本相关性必须按真实线性变换传播”相符。正交 contrast 在全帧坐标中的奇异性不应误写为测量误差为零，也不得把独立星数直接当作所有 contrast 的有效独立分母。该 S3-R 数值用于跨篇理论对照，**不是新的 P2真实M42测量、生产协方差实现或完整天空置信覆盖PASS**。

## 3 从点源参考精度到球面面亮度与天空控制

### 3.1 真正Drizzle算子与面积单位

源像元积分值`x_j`的单位是`L`，源像元原始面积`Ω_j`[sr]，实际pixfrac缩小drop面积`A_drop,j`[sr]，与HEALPix叶p的真实重叠面积`a_jp`[sr]。令`w_jp=a_jp/A_drop,j`、`N_p=Σ_j w_jpΩ_j`，则面积归一的球面面亮度为
\[
 B_p=\frac{\sum_jw_{jp}x_j}{N_p}=\sum_jR_{pj}x_j,\quad
 R_{pj}=w_{jp}/N_p\ [sr^{-1}],
 \quad C_B=RC_xR^T.
\]
真实sink还可传出`D_p=Σ_j a_jp`并用比例`D_p/N_p`将flux_sum和covered_area配对给AIO writer；D不是N，也不是实际面积并集，支持度裁剪不能倒推噪声归一。相邻输出像元/叶的协方差通常非零，像元孔径统计`a^TB`方差是`a^TC_Ba`，不是只取对角。#15的真HEALPix曲边与四角近似差异应由独立几何oracle核验，不能用内部`Σa=A_drop`恒等式自证真值；Stage3 Export进一步重采样改变输出R，单层signal不能重建未保留的方差。

### 3.2 必须同条件才能使用的`T`

若同一frame、相同实际信号单位、PSF/背景/支持、重采样R和相同**source-Poisson场景**下都能有效定义`V_F>0`与`V_B>0`，取
\[
 T=V_B/V_F\quad[sr^{-2}],\qquad
 I_B=1/V_B=I_F/T\quad[sr^2L^{-2}].
\]
`I_F=SNR_ref^2/F_ref^2`仅对同一个参照幅度成立，并未给出T。参考SED和reference-star source shot对应的`T_ref`未经验证不可以作为亮星云真实场景`T_scene`。同样 sky patch median 的`control_ivar`也是另一个统计对象，虽然与`I_B`恰好同单位，也不等于同一个像素的逆方差。

#8独立反例：两帧对同一个球面B各有真正无偏且独立的方差1，正确权重(1,1)给`V=0.5`。若它们的point PSF分别`(1,0)`与`(0.5,0.5)`，单位像元白噪下`I_F=1`与`0.5`；误用点源权(1,.5)合成B的真实方差`5/9`，比0.5高**11.11%**。这是对错误统计目标的数学反例，**并非当前M42已发生11.11%的真实损失**。

存在帧间联合协方差`Γ_B`且所有帧同一物理B时，满足无偏的真正最小方差系数为
\[
 a_*=\Gamma_B^{-1}{\bf1}/({\bf1}^T\Gamma_B^{-1}{\bf1}),
 \quad V_*=1/({\bf1}^T\Gamma_B^{-1}{\bf1}).
\]
仅在独立且边际方差完整时才退化成`1/Σivar`。#21另一独立反例：e1=δ+η1,e2=δ+η2，三者独立单位方差，单帧边际variance2，简单`1/(.5+.5)=1`但真实均值variance1.5，即**33.3%低估**；该百分数纯解析而不是实测。不同通带响应的帧原本不必估同一个B；若没有已验证SED/响应映射，可以输出明确声明的`mixed_response_signal`，但不能靠BLUE式假装物理无偏。

### 3.3 `V_ctrl`的实际估计器及k_corr作用域

生产sky sampler在有限signal且support有效时获取patch，使用单侧亮端3σ最多3轮剪裁，并以`median`和`σ_MAD=1.482602218505602 MAD`估计局部天空。其报告`V_ctrl=k_corr(π/2)σ_MAD²/N_retained`，`control_ivar=1/V_ctrl`。`π/2`是iid正态中位数**大N渐近**系数；有限样本、裁剪、相关输出叶、背景低频与实际PSF星晕会改变此方差，不保证与单叶`V_B`一致。#71现行`sampler.cpp`零MAD时强制`σ=1e−12`，再得高达`10²⁴`量级`control_ivar`，并不是获得极高真实测量信息，必须作为科学退化/不可用状态研究。

`src_pixel_scale_arcsec=|CD|×3600`是输入帧源像元角尺度[arcsec/pixel]，不是输出leaf；旧标定表的300/600″对应`nside=512`、pixfrac网格0.5/0.8/1，固定输出leaf尺度约412.258″。表函数`kcorr_lookup(pixfrac,scale_arcsec)`没有`nside`参数，因此无法真正表达`rho=leaf_scale/source_scale`的变化。工程回退1.4源于单一历史构型近1.3883的选择，**不是跨几何/PSF/N或median裁剪分布的上置信界**。旧真实M42像元约0.967″/px、tile_order9输出leaf约0.8064″，`rho≈0.83`，在此域的历史规则方格MC k≈2.3274，生产球面权重不同子窗MC≈2.0542–2.0580；窗口16×16与6×6/10×10、有效N和投影核不同，不能给两者取均值当新真值。tile_order12同源模型39.80是**合成构型**非现行真实FITS测定；有些完全相关patch的MAD=0会使k定义为无界/非有限。`k_corr(rho)`非全局单调，`rho≥1.3`的旧经验界不能不加pixfrac/N/PSF条件升级成科学普遍定理。

## 4 现行代码、正式产品与统计合同的差异

本节依2026-10-18 `main` 静态阅读，**没有**为B09运行生产CLI、编译Windows/Linux或回读其真实HiPS/FITS。引用使用稳定函数名，而旧报告中的行号只作为历史源码版本地址。

`lib/infrastructure/scheduler/src/normalize_precision.cpp`与`lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp::complete_spherical_reference`确实拥有从 P1 真实应用标度、像元积分PSF、参考源Poisson和局部R/C/Z生成球面控制点的实现构件。但`lib/algorithms/integration/normalize_science/src/normalize_science.cpp::write_frame_science_document`仍写`schema_id=acsd.normalize.frame-science/v1`,`schema_version=1`，`read_frame_science_document`为既有sidecar reader；`module_adapters.cpp::p1_op_writer`只是尝试发布staging `frame_science.json`，publish失败时保留signal并登记precision unavailable。`docs/engineering/contracts/NORMALIZE_DATA.md`§9所写的**v2仍属TARGET/NOT_RUN**；函数存在、文件名存在、设计schema存在不等于当前真实v2发布成功。

`module_adapters.cpp::p2_sparse_layer_probe`读取signal HiPS属性`ACSD_SPARSE_SNR_LAYER`指向的tile-local JSON，没有从canonical v2取`h,C,Z,R,F_ref`及模型hash；`snr_path=dense`不可用会具名失败；`sparse_reconstruct`缺层可记录实际退回frame SNR，声明有层但损坏应失败。历史S05所谓`lib/**`生产写点0、真实产物declared0、审计夹具手写declared2是**当时的静态/旧运行证据**；本批只确认当前legacy读入口仍在，未以新main重跑证明写点和declared计数。即使假设未来v2 writer与reader都存在，必须证明P2运行确实消费同源模型而非另一个兼容视图。

真实 `p2_op_integrate` 的权重有条件来源：`corrected_variance`只有在所需产品完整性声明及各帧variance文件存在时优先；否则逐帧Phase1 ivar；不足时可能调用legacy sparse/帧级SNR fallback，且通过`p2_integrated.json`登记`weight_source`和`snr_path_effective`。`lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp`里的SNR路径仍用`(SNR_ref/F_ref)^2*g_k²`，**这本质上是点源I_F尺度，没有#8所需T**。corrected variance等本地分支也没有证明包含#21所需的全部跨帧共同参数Γ；以`W=Σivar`及`variance=1/W`写完整科学方差仍有明确解析反例。`p1_op_drizzle`会读photoapplied、相应噪声按α²映射，但`p1_stack.json`及writer部分声明仍硬编码`ADU/sr`（#9），一个数值单位string一致并不证明真正光度单位同源。

| 源码/问题 | 现行源事实与独立反例 | 科学状态（非生产E2E） |
|---|---|---|
| `sparse_reference.cpp`局部pivoted QR（#65） | 重复COO对`(op_pix,op_leaf)`时QR组装覆盖赋值，而h/Z与独立模型按加和；重复示例可让同输入的条件方差相异 | 条件数学与SOURCE风险；是否真实产生重复COO仍需抓取 |
| `information_weight.cpp::white_noise_gate`（#67） | 检查diagonal且sigma_declared，不验证所有方差相等或外传scalar与真实数组相等；`P=(.5,.5),C=diag(1,9)`真正I=5/18、错误白噪捷径I=1/2，可虚高80% | 可调用API的静态反例；**没证明Normalize生产调用这个捷径** |
| `drizzle_engine.cpp`（#64） | 对有贡献但variance未知的源仍加signal/coverage，却只对`varianceValue>0`累计正方差，可产生不完整正variance。两源同权，第一variance4第二UNKNOWN，绝不能发布完整V=1 | SOURCE_CONFIRMED数学缺项；反事实第二variance100则真实V=26，仅为构造，不是真帧实测 |
| `sampler.cpp`（#17/#71） | 实际中位数V_ctrl仍按k_corrπσ²/(2N)，k=1.4域外且零MAD强行1e−12 | SOURCE_CONFIRMED；真实几何可信度及零MAD模型未验证 |
| `module_adapters.cpp::p1_op_writer`与`normalize_science.cpp`（#13） | 有文件发布入口，但sidecar schema仍v1；v2合同未闭合 | CURRENT v2 NOT_IMPLEMENTED/NOT_RUN |
| `p2_sparse_layer_probe`与`weight_chain.cpp`（#8/#13） | 仅legacy SNR layer/帧级fallback，不消费真实canonical v2，缺真正T | 当前点源→SB信号权重非普适最优 |
| `p2_op_integrate`及Export（#9/#21） | 视corrected/ivar/SNR源分支输出，1/Σivar不含跨帧共同Γ；Stage3标准导出只有signal，不生成凭空方差 | 条件权重存在≠完整边际uncertainty可发布 |

正确态度是允许合法signal和partial/conditional precision并明确来源，不因缺某项精度就宣称整个观测无价值；损坏sidecar或错误单位、错误模型hash属于另一类资料错误，不能用可选降级掩盖。

## 5 历史 S05 试验：源身份、定量结果与负例

### 5.1 原始输入、版本与真正样本分母

S05在旧任务SHA `9f1e99f2a5ace6965b7b427743eed8c4242945df`启动，轻量 evidence 自报运行HEAD `0ddd5a71085111509a5bcb2cf73f8dc7a29ca495`，还保存当时`sampler.cpp`、`spherical_overlap.cpp`及`code/s05/s05_kcorr_units.py`的source SHA256。脚本`code/s05/s05_arms_oracle.py`、`s05_real117.py`、`s05_kcorr_units.py`、`s05_figures.py`及`docs/S05_REPORT.md`可在Git阅读。**原始实验`run/ACSDSDOC-02/S05/results/{arms_oracle,kcorr_units,real117,real117_stars.npz,probe/*}`不在这次可直接回读的Git结果树；原始FITS、真实逐星表也未由本轮加载**。因此旧结果称`HISTORICAL_AUTHOR_REPORTED`，不自动称本轮独立重验已实测。历史报告中的`ORACLE_CHECKED`只能作为当次实验执行者的状态来源。

真实数据：依据原906帧轻量清单`实验/shared/data_catalog.json`，筛`M42_T2T3_mosaic_Flying_dutchman`/T2/M1/300s，从两夜各Red+Green共4帧形成相近CRVAL(最大1.6905″)、源角尺度0.9665–0.9669″/px的输入。两夜差分的真正同星flux是**两张Red**，Green只做同夜颜色特征，不能把四帧当四份Red独立观测。历史`Gaussian σ=1.2px`检测加41px低频sky、8MAD阈、4px圆孔径和8–14px背景环，未完成真实dark、flat和孔径修正；当时gain/RN来源未验证。

原脚本识别**2195个匹配星身份**，按`SHA256(seed:star_index)`且seed20261014预先切分1108训练、1087留出；不是2195独立曝光。不同旧117星筛法的历史`0.037978/0.013599≈2.7928`数字**不可与2195样本当同一统计口径对照**。旧S05再把天区作8×8=64空间块报告较强共同误差，64是保守block bootstrap分组建议，尚不是从完整Γ推导的严格有效样本自由度。

### 5.2 独立NumPy四臂与各自的适用域

- **A固定参照、已知sky：** 45格=m_ref {14,16,18,20,22}×sky{10,200,2000} ADU/px×FWHM{1.5,2,3}px、每格20,000MC、seed20261014、Moffatβ4、内部声明ZP25；`sd(Fhat)/sigma_F`0.9929–1.0095，68.27/90/95% coverage区间0.6769–0.6914/0.8957–0.9049/0.9473–0.9531；135 Wilson检验5未含标称，简单期望6.75；gain单位反向注入45/45触发。只能支持合成固定C模型，不证明未来真实样本的概率校准。
- **B自由常数sky：** `J=[P,1]`，额外方差`(σ_free/σ_known)^2−1`=0.0185–0.0816，正确模型68%覆盖0.6743–0.6937；故意用已知sky σ标准化自由sky结果时24/24向欠覆盖、16/24 Wilson显著。只研究常数sky，不等于复杂星云/宽低频nuisance已解决。
- **C线性相关重采样：** 已知`C_out=RVR^T+eps²I`的规则方格toy；错误忽略offdiag时`E_diag=V_diag/V_full−1`最小−0.485776，35有效负效应构型中33覆盖欠标称，零效应`R=I`时offdiag逐位0。交叉项符号与统计权/sky消除相关，不得推广“任何对角近似必低估”。
- **D点源vs孔径平均面亮度：** iid等方差与同一孔径内，效率损失`E=N_ap Σ_mask P²/(Σ_mask P)²−1`，两向解析对照差3.55e−15，10%阈在0.5–0.75 FWHM；不能无条件用于sourcePoisson/非均匀R和共用响应。

### 5.3 全部22项历史结果与原始统计口径（保留原表数值）

以下保留原P2论文**原始表4全部22项数据行**：行内PASS或其他判决依当时S05算法/构建/输入，而非B09-D2现在运行的状态。表后补充原始分母、统计置信区间与非有限数据的科研限制，避免读者只看历史判定列。

**表 4 核心结果（被估量、分母、样本量与来源）**

| # | 量（被估量） | 分母、样本量与 seed | 读数 | 判定 | 来源 |
|---|---|---|---|---|---|
| 1 | 臂 A 条件方差校准 | 45 网格点 × `N_MC = 20000`；seed 20261014 | `sd(F_hat)/sigma_F = 0.9929–1.0095` | 条件方差在合成域自洽 | `docs/S05_P2_evidence.json` |
| 2 | 臂 A 覆盖率 | 同上；68.27/90/95% 标称区间 | `0.6769–0.6914 / 0.8957–0.9049 / 0.9473–0.9531`；135 次 Wilson 检验 **5** 次不含标称（期望 6.75） | 与标称一致 | `docs/S05_P2_evidence.json` |
| 3 | 臂 A 阴阳对照 | 零效应：常数方差白噪下对角式 vs 严格式；阳性：反转 `g` 约定重算 | 零效应相对差 `2.1286133337938432e-16`；阳性 **45/45** 网格点覆盖率偏离标称 | 判红成立 | `docs/S05_P2_evidence.json` |
| 4 | 臂 B 自由背景代价 | 拟合背景 `J=[P,1]`；seed 20261015 | `E = 0.0185–0.0816`；正确 σ 下覆盖 `0.6743–0.6937` | 需另计背景代价 | `docs/S05_P2_evidence.json`；`docs/S05_REPORT.md` |
| 5 | 臂 B 负例 | 用已知背景 σ 标准化自由背景估计 | **24/24 方向欠覆盖**，16/24 Wilson 显著 | 判红成立 | `docs/S05_P2_evidence.json` |
| 6 | 臂 C 对角近似代价 | 线性 `R`、`pf=0.8`、`floor_ratio=0.1`；seed 20261016 | `E_diag` 最负 **`−0.4858`**；35 个 `E_diag<−1%` 构型中 **33** 个 68% 覆盖率全落标称之下 | 判红成立 | `docs/S05_REPORT.md` |
| 7 | 臂 C 零效应 | `rho=1、pixfrac=1 ⇒ R=I` | 非对角**逐位为 0**（`max|offdiag| = 0.0`） | 阴性对照校准 | `docs/S05_P2_evidence.json` |
| 8 | 臂 D 被估量分立 | 点源权重 vs 弥散源权重；seed 20261017 | 两向闭式最大差 `3.5527e-15`；`E>10%` 的孔径阈 `0.5–0.75 FWHM` | 两对象不可混用 | `docs/S05_P2_evidence.json` |
| 9 | `scale_arcsec` 量纲核对 | 源码链 `hp_drizzle_api.cpp:1112-1113` → `ACSD_DRIZZLE_SCALE_ARCSEC` → `sampler.cpp:136-146` | 该键 = **源（输入）帧像元角尺度 [arcsec/px]**，表轴单位正确 | 量纲核对通过（不改表轴数值） | `docs/S05_P2_evidence.json` |
| 10 | `k_corr` 标定域自校验 | 标定几何 `rho = 1.3742`（源 300″/px、`nside 512`） | 本独立 MC `1.3394` vs 记录值 `1.3883`（相对差 `−3.5%`） | 模型自校验通过 | `docs/S05_P2_evidence.json` |
| 11 | `k_corr` 生产几何 | 源 `0.9666″/px`、`tile_order = 9`（叶 `0.805″`）、`pixfrac 0.8` ⇒ `rho≈0.83` | 独立 MC **`2.3274`**；生产球面权重探针 **`2.0542–2.0580`**；冻结回退 **`1.4`** | 该域 **`UNCALIBRATED`**：方差低估、权重高估约 `1.5×`；保守域仅 `rho ≳ 1.3` | `docs/S05_P2_evidence.json` |
| 12 | `tile_order = 12` 产品域 | 源 `0.9666″/px`、`tile_order = 12`（叶 `0.1008″`） | `k_corr = 39.80`（`rho = 0.104`） | `1.4` 在该域严重不保守 | `docs/S05_REPORT.md` |
| 13 | 真实帧共同星枚举 | 四帧、`SHA256(seed:star_index)` 奇偶切分；seed 20261014 | 共同星 **2195**（训练 1108 / 留出 1087，不重叠） | 输入合格 | `docs/S05_P2_evidence.json` |
| 14 | 真实帧散度比 | `sd(dmag)/median(sigma_analytic)`；2195 星 | **`3.3701`**（shot-only 口径）/ **`2.7324`**（empirical 口径） | 复现历史 2.8–3.4 量级，但必须分层解释 | `docs/S05_P2_evidence.json` |
| 15 | 解析 σ 的分层表现 | 留出去趋势后按 SNR 分层 | 低 SNR 端 `z_rms 1.86–2.11` 与实测同量级；亮端 `z_rms 5.09 / 10.34` | 多余散度由**与 SNR 无关的系统项**支配 | `docs/S05_REPORT.md` |
| 16 | 经验系统项地板 | `Var = sigma_analytic² + sigma_sys²`，`sigma_sys` 按 SNR 层在训练集估计、留出集验证 | `sigma_sys = 0.0701 / 0.0450 / 0.0361 / 0.0608 mag`（全局 `0.0485`） | 另立校准层，非全局乘数 | `docs/S05_P2_evidence.json`；`docs/S05_REPORT.md` |
| 17 | 加地板后留出覆盖 | 1087 颗留出星 | 68.27% → **`0.8611`**，90% → `0.9439`，95% → `0.9595`，`z_rms = 1.028` | 68% 档**过覆盖** ⇒ 残差为“窄核+厚尾”，单一高斯地板不同时匹配核心与尾部 | `docs/S05_P2_evidence.json`；`docs/S05_REPORT.md` |
| 18 | 空间共模与有效样本 | 8×8 空间块均值分散 / 独立预期；2195 星 | 超额 **`37.9×`**；有效独立样本数保守 **64 块** | 不得把 2195 当独立星数扩大显著性 | `docs/S05_REPORT.md` |
| 19 | 真实帧阴阳对照 | 零效应：不注入；阳性 1：注入 `0.10 mag` 随机；阳性 2：注入 `+0.05 mag` 共模 | 零效应 95% `[0.9461,0.9697]` 含标称；阳性 1 68% 覆盖 `0.3707` 判红；阳性 2 `median(z)` 位移 `+3.56` 判红，同数据重拟合被吸收（`0.00`，退化性登记） | 判红成立 | `docs/S05_REPORT.md` |
| 20 | `sparse_snr_layer` 生产状态 | `lib/**` 写点、端口注册表、生产运行 `declared` | 写点 **0**；消费面 1 处（`module_adapters.cpp:12106-12140`）；注册表 **0** 命中；生产运行 `declared = 0`；唯一 `declared = 2` 来自审计夹具手写 sidecar | 判 **`NOT_IMPLEMENTED`** | `docs/S05_P2_evidence.json` |
| 21 | P3 几何修复对 P2 噪声的影响 | 同一二进制 + `ACSD_DRZ_P3_FAULT=legacy_cross_normal`；seed 20261014 | 顶点构型完备性 `1.62e-9 ↔ 3.52e-3`；生产几何权重最大差 `3.59e-7`、叶方差相对变化 `2.3e-10`、`k_corr` 相对变化 `+0.36%` < P2 MC 分辨率 `2.24%` | P3 几何变化不会被误当成 P2 随机噪声 | `docs/S05_P2_evidence.json`；`docs/S05_REPORT.md` |
| 22 | `snr_path=dense` 显式请求 | 生产唯一读取/消费点 | 显式请求 `dense` ⇒ `SNR_PATH_DENSE_UNAVAILABLE` **fail-closed**（不静默替换为帧级口径） | 生产不可用，不是“未测” | `lib/infrastructure/scheduler/src/module_adapters.cpp:12431-12436` |

### 5.4 真实覆盖的三个不同模型和系统地板

| 统计量与实际样本分母 | 历史结果 | 论文能够推断及不能推断的部分 |
|---|---|---|
| 原始同星Red差分`sd(Δmag)`与`median(sigma_analytic)`；2195不同星身份 | 0.0529/0.0157 mag；比值3.3701；以empirical σ另算为2.7324 | 两个比值的分母不同，不是将白噪“修正了2.73倍” |
| 无独立趋势留出，所有2195星原条件68.27%覆盖 | `0.4752`，Wilson95% `[0.4543,0.4961]` | 只能评价旧固定模型，不能和只留出样本的统计量当等价 |
| 以1108训练星估趋势，**1087留出星**原条件68.27/90/95%覆盖 | `0.4471/0.6872/0.7489`；68% Wilson `[0.4178,0.4768]` | 明显欠覆盖，不能独立定位shot模型的某一个gain或flat根因 |
| 在1108训练星拟合SNR分层经验方差地板后，1087留出星覆盖 | `0.8611/0.9439/0.9595`；68% Wilson `[0.8393,0.8804]`，`z_rms=1.028` | **68%过覆盖**，95%靠近标称；不说明单一Gaussian floor得到真实核心和尾部 |
| 8×8空间块均值超额散度 | 旧报告`37.9×`，分组64块 | 存在共同结构相关的线索，64块不能直接当严格推得的有效独立样本N |

原留出SNR层为20–50 `n=126,z_rms=2.11`，50–100 `n=437,z_rms=1.86`，100–300 `n=464,z_rms=5.09`，>300 `n=60,z_rms=10.34`。训练地板分别`0.0701/0.0450/0.0361/0.0608 mag`（全局0.0485mag），它是**未解释合成残差**，不能排除亮端非Gaussian、PSF/孔径误差、天区低频、flat、未校准gain/RN、WCS、跨夜/变星及颜色响应。星ID哈希留出不排除同一原帧、同一母版/零点和相邻星共同误差跨train/test共享；只有两夜不足以评价独立夜次的泛化置信度。原报告95%的Wilson在独立Bernoulli假设下计算，真实共同残差下还需多层cluster bootstrap。

### 5.5 k_corr、非标准数据和缺失图件

历史标定源300″/px、nside512、pixfrac0.8的表值1.3925、经验单例1.3883、独立方格模型1.3394不同，不能认为同一随机世界已逐位复算。约0.967″/px/tile_order9下原16×16方格模型2.3274，对照实际球面重叠权的6×6和10×10两个有限窗口2.054243/2.057989，虽全>1.4，但有效N、clip、投影核不同。旧tile_order12的39.80是合成world，不是当前真FITS测得的通用比例。**特别核实：** `docs/S05_P2_evidence.json`在`production_geometry_k_corr_spherical_weights.value`第三个元素写裸`Infinity`，**不符合标准JSON**。这不是第三个可取平均的数值，也不能未经原始权重/像元轨迹认定是零MAD、完全相关或JSON writer错误哪一种。保留原字节不修改历史文件，未来在新证据中以`value=null,status=NONFINITE_UNDER_INVESTIGATION`加原原因、N、有效mask、真实R/hash保存。无穷在绘图时应被显示为单独失效状态，不得画成有限指标。

原论文提到 `figures/s05_arms.png`, `s05_real117.png`, `s05_kcorr.png`, `s05_p3_covariance.png` 及流程图，但这些PNG当前Git并未跟踪；`s05_figures.py`是从原`run/.../results/*.json`渲染旧图的脚本。没有run源读数时只能保留图题及`HISTORICAL_FIGURE_UNAVAILABLE`状态，不能手工画出“新的实测图”。原报告中的P3修复前/后条件协方差探针（0.9666″真帧几何下Δk≈+0.36%，当时MC辨率约2.24%）只是那个**冻结几何的历史局部对照**，不证明任意场或新的P3曲边精度已闭合。

### 5.6 现阶段证据层级

| 科学主张 | 可回读的身份 | 不可提级的身份 |
|---|---|---|
| 条件GLS、rank、T与Γ表达式 | `MATH_DERIVED`，有明确数学前提/可独立反例 | 不等于真实Poisson所有似然模型 |
| S05四臂与表4历史读数 | `HISTORICAL_TOY_ORACLE`、`code/s05/`和紧凑`evidence.json`可读 | 本轮没有重新运行45×20k次仿真 |
| k_corr数值/生产重叠探针 | `HISTORICAL_GEOMETRY_PROBE`，原两个有效值+一个Infinity | 不是现代真实设备方差标定完成 |
| 两夜真实M42/2195星与coverage | `HISTORICAL_AUTHOR_REPORTED_REAL`、脚本/清单/摘要在库 | 不是本次独立回读raw FITS/逐星NPZ并重算CI |
| 正式schema、legacy与fallback | `CURRENT_STATIC_SOURCE_REVIEWED` | 不是v2 canonical真实writer→reader E2E |
| 最新二进制/其他仪器/真实HST保真/B10全文 | `NOT_RUN` | 不得声明`RELEASE_VERIFIED` |

## 6 可直接交付 Agent 的详细验证方法

下述所有路径为**未来目标输出**而非本轮已创建。根目录`run/B09-D2-P2/`（大资产不进入Git）。必须先固定`main SHA`、`source_sha256`及每个真实输入与实验版本身份、真实编译二进制/已加载so/dll/lib SHA、真实输入occurrence/FITS SHA、响应/XP/SED/校准master/PSF/WCS/噪声模型hash和科学配置，生成`inputs_manifest.json`；既有S05版本与本次新C++版本分列，不能假称同一源码。实验按原始真实星、合法有效mask和预声明分母比较，不事后筛选坏结果调门；每个结果保留阴阳对照/真实统计值/源码版本、seed和独立95%CI。

### 6.1 A｜独立PSF—Fisher—GLS Oracle与数值退化

**输入/真值：** 全像元积分Gaussian/Moffat4和平坦PSF（子像元相位冻结），`F_ref=100/1000`自定义积分单位，C从`σ²I`、`diag(1,9)`、合法Toeplitz相关SPD及rank-deficient PSD构造；Z取已知背景、未知常数背景、常数+坡度与近共线列、`h∈span(Z)`，以及#65两个相同COO `(pix,leaf)`代表的条件反例。用独立NumPy/SciPy SVD和pivoted QR按白化后的相同矩阵解真值，并与生产`w_info_diagonal/w_info_dense`、`complete_spherical_reference`实际DLL/API结果同输入比对。

**输出与预声明标准：** seed20261018（随机矩阵臂）、`results/A_{inputs,oracle,abi}.json`；每case保存`h,C,Z,R,rank,κ,I_F,V_F,residual,precision_status`，FP64良态相对误差≤1e−9（不适定时另登记κ驱动可解释阈值），不可辨必须明确unavailable。#67 `P=(.5,.5),C=diag(1,9)`真`I=5/18`，若白噪捷径仍给1/2必须判红；重复COO做合并/拒绝且与h/Z一致，真实producer若不产生重复，应单独报告`NOT_TRIGGERED`而非假称真实受损。**结论范围：** 固定矩阵与生产API同输入的数学正确性，不证明真实FITS。

### 6.2 B｜已知/未知背景、Poisson与信号依赖

**设计：** 复制S05 A/B中45种`m,sky,FWHM`组合，并以同一随机抽样造已知常数背景、自由常数、自由线性sky、与星PSF共线/秩亏Z；再对参考通量固定与真实`F_actual`分别加入Gaussian固定C、原始Poisson+read模型、正确`C(F_actual)`和错误`C(F_ref)`。增益/响应仅按明确模拟单位设定，实际M42 g/RN未验证不可当真。seed20261019、20261020，至少20,000个独立模拟曝光/格并用配对noise重算不同估计器。

**输出：** `results/B_known_free_poisson.json`，真通量/模型输出`bias,sd_emp/sd_pred,variance_ratio,z_rms,68.268949%/95% coverage`及Wilson/Clopper95%CI、rank、各源Poisson占比、训练背景自由度。预声明：固定高斯理论的bias在3个独立MC标准误内、coverage和variance ratio落在预先定义的采样置信范围；故意把free背景套known sigma应按预期欠覆盖；高F Poisson源项错用reference应报告方向/失效阈。全格多重检验按预设检验族报告而非要求135格全绿。**结论范围：** 给定真实模拟参数的条件方差与近似误差，不代表实际相机全部物理误差。

### 6.3 C｜多帧相关噪声、共享标度、后选择的区间覆盖

**设计：** 从真实P1空间`α_j`和像元积分PSF模型构造同单位多帧真值，逐项注入独立shot/read、共同master-flat、共享Gaia/XP零点、sky拟合θ、PSF估计误差及frame-common term；冻结和动态rejection/星掩膜分别运行（4或更多帧），与实际`Γ`、仅diag`Γ`及完全忽略共享θ比较。seed20261021、20261022，每构型至少20k独立scene+frame joint draws；与独立linear algebra/带真实θ的Forward生成器比对，不用生产算法自己生成的`ivar`当真值。

**输出：** `results/C_shared_selection.json`，同一参数/条件与边际`Cov(y_k,y_l)`、相关谱、偏差、MSE、conditional/marginal预测方差、68/95% coverage、线性BLUE vs错误`1/Σivar`性能。真实共同term设0时应回到独立结果，#21已知δ=η1=η2方差1的理论反例需如数判红；选择随数据变化时固定mask条件CI不足，应显示选择后误差。**结论范围：** 合成声明的共享误差分布，不自动推断真实M42该来源占比。

### 6.4 D｜真实Drizzle算子、缺失方差与k_corr逐统计量校准

**输入/变量：** 调生产`spherical::compute_overlap_area_g_ctx`捕获真实几何R/HEALPix NESTED叶，外部高精度算子计算`R C R^T`；涵盖所有12face、极区/face边、真实CCD WCS/SIP，pixfrac0.5/0.8/1，源像元角尺度0.1/0.3/0.9666/1/3/300/600″px，tile_order0/3/5/9/12，PSF与实际source支持、sky剪裁、星/星云污染、量化和source variance已知/部分未知。另一因素控制patch side3/5/7/9/11/17（N_ret目标5/9/17/25/49/81/121/225/251）及同N不同几何分布，具体真实设计矩阵冻结为manifest，避免不可计算的全笛卡儿积。

**统计量/方法：** 与生产完全相同`finite+support→one-sided bright 3sigma,≤3 rounds→median/MAD`，独立MC中定义`k_corr=Var(median) / E[(π/2) sigma_MAD²/N_ret]`；N随机时**每次**保留自己的N/clip和MAD，不得用平均N替代。seed20261023/20261024，各受检组合≥20k或报告相应MC相对SE/可靠可见差；真值由同源噪声C/R给出，PSD/零MAD/full correlation分别保留degenerate state并用零/非零读噪负控。比较`k=1.4`、旧表值、同几何实际oracle、独立真实noise模型，报告value/CI、`N_ret`分布、覆盖、非有限率、通用值适用域。方差已知`[4,4]`和部分UNKNOWN`[4,0]`/`[0,4]`/`[0,0]`等必须验证：完整signal和支持保留、未知方差不得被已知部分假发布，#64和#71必须判红。

**输出/科学边界：** `results/D_R_cov.json`,`results/D_kcorr_scan.json`,`results/D_nonfinite.json`、独立R中间值hash和`figures/D_*`。只对**同估计器、同R、同support、同单位、同N/clip适用域**给出校准方差，不得一律替换整个程序为“新的2.2”。

### 6.5 E｜同输入真实 M42、训练/留出与历史/现行C++比较

**固定输入与历史基线：** 以原 `实验/shared/data_catalog.json`索引锁定 `M42_T2T3_mosaic_Flying_dutchman`的T2/M1两个夜次Red+Green 300s四帧，按原FITS头校验CRVAL/CRPIX/CD/BZERO/BSCALE/DATE-OBS、源尺度、源文件SHA；锁定Gaia/滤镜响应、SED、各母版/flat/PSF/WCS、曝光、g/RN的可追踪来源。先用S05原seed20261014及原脚本/依赖重算旧2195共同星、1108/1087固定哈希拆分、旧4px孔径+8–14px天空环，同一测星/掩膜/星身份比较旧方差算法与新算法；无法取得旧raw NPZ时报告`HISTORICAL_NOT_REPRODUCED`及哪些输入/星不可用，不能重建一个“更好”的2195作为旧真值。

**生产端：** 在记录新main的编译二进制/动态库及每个运行model hash后，使用真正`p1_op_photometry→p1_op_drizzle→p1_op_writer`生成native/applied产品，再运行`p2_op_integrate`及P4现场consumer；记录确实生效的precision/ivar/SNR权重来源。固定相同星集合、响应、背景、mask、PSF/孔径、支持和参考源，方可比较历史S05估计量与C++；不同选择产生的样本必须另立列表。真实天空缺无噪光度真值，故可报告**同星的重复观测差**与留出星预测方差覆盖，不能从历史差直接推导绝对星云真值MSE。新增独立夜次/仪器（若确有）要与两夜样本分开报告，不能无数据虚构。对两夜旧框架先给同输入差分与全样本原比，再按source ID和天区空间block、真正独立frame/night层实施成簇bootstrap，建议预登记10,000次resample及多种合理块尺度敏感性分析，不把64块硬认为理论真N_eff。比较68.27%/95%置信覆盖、binomial仅作iid诊断、cluster-robust CI、颜色/星等/PSF/位置分层、旧σ地板是否在留出过覆盖及新模型如何改善预测，预定义模型增益不得在测试星上调参。

**seed/输出/判断：** 历史seed20261014、生产新seed20261025；`results/E_m42_inventory.json`,`results/E_perstar.parquet`,`results/E_old_vs_current.json`,`results/E_cluster_CI.json`，每行star_id/exposure_id/sourcehash、匹配/拒绝原因、观测/预测误差单位和统计分母。`EXPECTED`: 能在固定观测域复算既有负例/解释新版变更；若new empirical coverage在cluster CI中接近标称、且对应方差确有独立来源，才可称该受检观测域的科学精度改善。若只有两帧真实Red，就不能声称跨夜总体95%有良好频率校准。

### 6.6 F｜Normalize→HiPS→Mosaic→Export全链及故障注入

**来源与Oracle：** 由同一真实输入occurrence生成native和已应用P1标度两个版本、同源PSF响应及SED，外部Astropy/HEALPix reader验证逐像元applied/native `α_j`、条件`var' = α_j²var`、`F_ref=α_*F_native`与真实总积分区别、BUNIT、response/reference/application_id、信号/variance的单位`L/sr`和`L²/sr²`、真实球面leaf K与HiPS tile order K−9的区别；若v2仍未实现，必须记录 `NOT_IMPLEMENTED`，不得在工具脚本手写符合schema的文件伪称生产输出。只有实际P2进程读取该source对应canonical、核对模型hash、R/C/Z/PSF/support和 `T=V_B/V_F`并把weight_source写入产品，才叫真实E2。

**故障矩阵：** 缺`frame_science`、v1冒充v2、hash错误、单位BUNIT不一致、`response_id`冲突、leaf/tile order误标、GaiaXP超谱域、未知source variance混已知、zeroMAD、部分样本不完整方差、跨帧共享Γ未知、跨仪器混波段且无共同SED模型，逐一故障注入；比较`DATA_CORRUPT/CONFLICT/INCOMPLETE`与合法`PRECISION_UNAVAILABLE`，**已拥有合法signal时保持信号可用**，但不得从1e−12或部分方差生造完整逆方差。对可证明同B的两帧执行#8/#21解析模拟对照，在真实p2_integrate权重输出检测`I_B`及条件或边际不确定度。随机seed20261026（仅有随机注入的臂）、输出`results/F_source_product_roundtrip.json`,`results/F_failure_cases.json`和`e2/{normalize,mosaic,export}`，记录完整source/build hash、实际运行入口、stdout/rc、FITS像元单位和外部读取。`EXPECTED`: 数据身份可审计，错误不被成功状态掩盖；不能由静态函数字面表述自动结案#8/#9/#13/#21/#64/#71。

### 6.7 G｜已知真值HST前向与点源/延展源效率

以仓内`实验/shared/data/real/m16_scene_index.json`记录的HST M16 F657N/F673N/F502N drizzled图作为非平凡**结构模板**；通过`实验/shared/synthetic/m16_mask.py`生成各波段valid/flags，在只有合法像元的区域构造独立已知谱型的星/弥散连续谱、Hα、[N II]和明确的外部前向响应、PSF、WCS、真实噪声/Drizzle，与M42真实观测分开。**HST coadd本身不是原始曝光级无噪声真值，也不能从一张F657N图推知任意Red宽带SED**；噪声必须另由前向生成，真实掩膜标记边缘/坏点，并记录模板/valid hash。seed20261027、20261028，≥200个独立模拟场景/组合（样本不足必须报告CI分辨率），对比真实 `1/V_B`、`I_F/T`、错误 `I_F`作SB权重、corrected variance、共享Γ的BLUE和legacy frame fallback；测点源/延展源bias、置信覆盖、真实flux、PSF/MTF、低频与噪声功率谱、支持域/边界效应，指定同一物理目标且输出`results/G_hst_truth.json`、`figures/G_*`及全部图/参数哈希。仅在该声明已知谱/仪器/有效mask的合成真值世界能证明这些统计优势；M42真实数据能补充经验测光和目检，不可代替物理真值。

### 6.8 统一预注册报告字段、误差判定与科研适用域

每种实验的`experiment_id`、`estimand`、真实单位、源/shape、`model_id`、noise covariance factor、`F_ref/F_actual`、响应、pixel PSF、source/leaf order、nside、pixfrac、clip、mask、support、独立seed、原始数据hash、构建hash必须进同一轻量evidence。数值统计保留每次完整真值与估计对，`bias=mean(est−truth)`、`Var_emp/mean(V_pred)`、`z=(est−truth)/sqrt(V_pred)`、`coverage_q=mean(truth∈q CI)`及其独立MC/cluster CI（真实源与空间块/夜次有效分母不同）；负例可按预先设计失效方向给出效应量，不调整滤波/σ地板来“压过”门。纯Gaussian固定C臂预注册`|bias|≤3 SE_MC`、方差比在对应有限样本95%CI、68/95 coverage由Wilson/Clopper或声明的多重比较控制，不以135格无一红为目标；其它Poisson/shared/selected域报告非Gaussian区间、MSE与错误分解而不强制套高斯阈。独立Oracle必须不依赖生产估计值作为其自身truth。

所有A—G均为 **B09-D2仅设计、实际`NOT_RUN`**，其科学可成立范围依次为A数学/API正确、B限定Poisson与nuisance前向、C联合随机模型、D同patch+同几何 `k_corr`、E真正固定真实观测样本、F现行精度产品读写闭合、G声明已知真值世界。缺一种不能从其它PASS推导。

## 7 讨论、可发表主张与真实创新限界

数学上能成立的是给定C和可辨识的GLS Fisher、对真实R的条件协方差传播、以同条件T转到实际SB、共享Γ对简单`1/Σivar`的否定及一些强制红灯解析反例。历史S05的NumPy四臂及M42实测报告揭示的重要信息是：自定已知条件下公式可用，稍换未知sky/相关噪声就必须改预测，真实天空的星点经验噪声和条件预测差距大，而且训练拟合的额外`σ_sys`没有同时校准68/95覆盖。旧k_corr与当下产品类似几何存在显著不一致，但还未形成一个具有可信跨nside/PSF/clip覆盖的替代统计函数。

本项工作的真正创新候选是把**科学被估量→球面变换→代码可消费性→条件与边际置信覆盖**统一到可逐行追踪的审稿框架和实际故障注入，并在失败/不可辨/数据缺项时能诚实报告“precision unavailable”而不制造更多信息。其理论基元（Fisher/GLS、profile extraction、Drizzle covariance、optimal coadd）均有充分前例；若要宣称本方案使实际天文数据达到更优SNR、效率或科学保真，必须在相同参考对象和资源下与正确基线对照，给出配对效应和独立CI、HST真实谱前向和多个真实夜次、P4消费者路径及Export同一单位的科学输出。不能用历史PASS、任何自证恒等式、低seam RMS或视觉上更平滑的HiPS当投稿正面结论。

## 8 结论

P2需要实现的不是无条件“每帧绝对SNR”，而是条件明确的点源`F_ref,V_F,SNR_ref`，再在真实R/PSF、已应用单位、同场景Poisson与共享Γ下求同目标的`V_B`；P5 patch median `V_ctrl`只用于相应背景统计。这套科学逻辑已有可解析表达和历史数值支持，仍需要新生产v2/消费者连线、方差缺项、校准域与真实系统误差的实验闭合。旧S05真实同星条件误差欠覆盖与经验地板过覆盖同时存在，说明真实误差分布未被解释；当前静态代码仍是`frame_science/v1`、legacy稀疏层消费及域外工程1.4，不能据论文修订宣布当前真实马赛克方差已可信。本轮**未改生产、未重跑原历史FITS、未验证真实发布版SNR、未完成独立审稿**。

## 参考文献与B10逐篇一手复核计划

[R04] Newberry, M. V. (1991), *PASP* 103, 122–130. [DOI:10.1086/132801](https://doi.org/10.1086/132801)。B10定位真实sky测量的误差项、源/天空独立性、原始方差公式及CCD观测假设。

[R13] Horne, K. (1986), *PASP* 98, 609–617. [DOI:10.1086/131801](https://doi.org/10.1086/131801)。B10逐式核轮廓抽取PSF归一、天空/读噪、估计目标和真实测试条件，区别光谱抽取与二维天文图像。

[R05] Zackay, B. & Ofek, E. O. (2017), *ApJ* 836, 188. [DOI:10.3847/1538-4357/836/2/188](https://doi.org/10.3847/1538-4357/836/2/188)。B10逐式核最优共加目标、已知噪声/背景条件、不同PSF及真实源统计假设。

[R06] Fruchter, A. S. & Hook, R. N. (2002), *PASP* 114, 144–152. [DOI:10.1086/338393](https://doi.org/10.1086/338393)。B10逐式核drop/area normalization、correlated noise ratio与既有算法实际数值定义。

另对 Gaia Collaboration/Montegriffo 的 Gaia DR3 XP 定标、Górski/HEALPix 和 P1 `F_ref`/响应约定，在B10定位具体原文及参考软件，与本文真正实现差异逐项对照。现有仓库`refs.md`和`../shared/references/CORE_REFERENCES_VERIFIED.md`只支持其所载**书目/部分摘要**，不冒称本轮已进行全文精读。
