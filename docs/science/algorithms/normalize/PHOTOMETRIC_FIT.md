# 响应积分与光度拟合数值方法

本页细化[光度科学](../../photometry/PHOTOMETRY.md)的响应积分、配对和稳健拟合。光谱单位、通带、空间规范、一次标度和协方差归属由该正本定义，全部参数、实际返回分支、布局和消费者见[光度详细设计](../../../detail/normalize/modules/photometry.md)。不因拟合残差小宣称未知扩展源已映射到共同通带。

## 响应积分与离散表示

现有XPSD路径将逐源编码解为f_lambda,j=byte_j·flux_mul+flux_min，再对实际滤镜T和QE的效率响应计算

\[
 F_{\rm syn}=\int f_\lambda(\lambda)T(\lambda)Q(\lambda)\lambda\,d\lambda.
\]

当f_lambda单位W m⁻² nm⁻¹、λ与dλ均用nm时，积分单位W m⁻² nm；它不是谱密度，也不是已除hc的光子率。响应定义若已含光子能量因子则不能再乘一次。已外定标XP谱不再乘10^(−0.4G)。按积分响应归一的带平均谱密度是另一约定，不能只改标签。

prepare_filter_cache将T/Q以Akima插值到文件声明的完整谱网格，区间外填0，既有积分为复合Simpson 1/3并处理尾3/8。343点、起336nm、步2nm来自已观察的XPSD构型，不是所有容器的永久网格；生产读取元数据并核对数组。非生产旧编码×G因子的通道与1nm重叠网格不作为同一物理参考。

插值、波段截断、8bit量化和积分离散分别估计误差。逐样本半量化步长与积分误差不能直接互代，Akima可能过冲，平滑谱的Simpson误差也不保证窄发射线准确。响应/QE身份、波长/效率单位、采样和实际应用 convention 随参考保存。

## 配对与样本资格

本帧完整WCS/SIP将参考投影到像元域，图像拟合星与Gaia互为最近邻后建立配对。当前半径2px是像素域参数，需结合尺度、定位误差与拥挤度；相同天球角距不由不同设备的2px自动保证。精确等距的KD-tree选择尚需稳定tie-break，不以到达顺序假装科学唯一。

参考拟合只对具备有效正通量的样本取对数；负线性图像值仍合法，不因拟合样本条件删除整图负值。饱和/混叠/失配与数据无效分别记录。星等预过滤Δ_i=−2.5log10(F_instr,i)−G_i，保留abs(Δ−medianΔ)≤3mag；3mag是项目参数，不证明识别所有通带/颜色失配。

## 固定尺度 Tukey 位置迭代

对通过资格的配对定义r_i=log10(F_instr,i/F_syn,i)。这里是按已声明单位取数值比值，改变通量单位会平移location并改变scale的数值，不能把它当无单位绝对物理对数。初值l0=median(r)，S=median(abs(r−l0))/0.6744897501960817。

现算法在迭代中固定S，u_i=(r_i−l)/(4.685S)，Tukey权为(1−u_i²)²（abs(u)<1）或0，更新l=Σw_ir_i/Σw_i。至多50轮，abs(l_new−l_old)<1e−6dex停机；这是停机阈值，不是标度精度。S=0时现实现直接取中位数，不迭代；少于3个合格参考不拟合，保留能力状态。

拟合系数scale=10^(−l)，在对应约定下将仪器数值转换到合成尺度。系数包含可辨识的口径/曝光/透过率与单位乘积，不能将“各因子不可单独测量”写成整个scale无物理解释。空间m(x,y)的求值、对数均值规范和有效支持由科学正本规定，实际α必须与施加面配对。

当前cleanAndScale在所有权重为0时break、触顶时仍用最后location计算scale，并无独立converged标志；不能沿用旧负例说明称它必定回scale=1且不发布。已确定的失败合同与代码分歧记录，不在本轮修改迭代或返回策略。

## 散度与不确定性

实现的最终sigma_residual使用IRLS内点相对最终location的绝对残差中位数，再除0.6744897501960817；该中心不必是内点中位数。sigma_mag=2.5·sigma_residual与sigma_cal_rel=ln(10)·sigma_residual是既有散度换算，后者是小散度一阶相对尺度，不是任意分布的完整误差。

逐星残差散度、帧标度参数协方差、参考谱/响应误差与单源测量误差分别定义。共同参数传播及交叉项按科学正本处理；sigma_residual不等于参数标准误，不能被当所有单源的flux_error。S=0、内点不足及真的测得零散度的旧零哨兵要配状态，不把未知误差写成0。

MAD有限样本修正、Tukey效率/崩溃点继承、中心偏移的阶数和Huber尺度常数等强断言见[Q33](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。本轮保留常数与现算法，不采用旧文字建议切换尺度更新或鲁棒模型。

## 单位、响应与一次应用

逐帧response按真实身份绑定，不拿第一帧滤镜/QE覆盖不同输入；路径或basename匹配的现实现不等于严格occurrence合同。未有完整曲线时明确native/relative/unknown能力，不制造绝对标定成功。

已知确定α对信号乘一次、随机协方差乘配对二次幂；α来自估计时另有共同参数项。合成颜色中位数只由星表/响应决定，是样本颜色诊断，不当帧零点。标量光度拟合与逐位置SED通带转换是不同能力。

## 数值与验证

保持输入配对、枚举、归约及浮点环境时讨论重复性；线程扫描不能覆盖样本置换/tie-break。合成无扰动常量比值只测试S=0中位分支，不能据其通过宣称IRLS稳健性。独立构型应覆盖污染、有限样本、颜色/响应错配、尺度/空间场、匹配并列、退化、迭代触顶及真实应用。

解析积分、高精度谱、独立前向成像、留出星与真实产品分别给单位、统计分母和误差来源；HST成品可作结构模板，其噪声不是原探测器真值。重复使用生产公式的记录一致性不当独立科学验证。

旧详细读数、报告/任务流水和原文保存于[来源档案](../../../../实验/documentation-refactor/docs/ASTROMETRY_PHOTOMETRY_SOURCE.txt)。下文保留可直接复核和实现的离散定义，不以该档案替代正式设计。

## 物理模型到数值标度

设入射谱辐照度为 f_λ（W m⁻² nm⁻¹），T、Q 是不含 λ/(hc) 的效率，像元记录的是曝光内光子转换后的仪器量。若孔径 A、曝光 t、电子到计数转换 g 及总体效率的标量部分合成 a，则理想点源解析总通量满足 F_instr≈a Q_γ。谱形有关的 TQ 留在积分内，不能用一个 a 吸收形状差。

令 λ_nm 为以 nm 表示的数值，h_SI c_SI 单位 J m，则

\[
Q_\gamma=\frac{10^{-9}}{h_{\rm SI}c_{\rm SI}}
 \underbrace{\int f_\lambda(\lambda_{\rm nm})TQ\lambda_{\rm nm}
 \,d\lambda_{\rm nm}}_{F_{\rm syn}},\quad [Q_\gamma]={\rm s^{-1}m^{-2}}.
\]

10⁻⁹来自光子能量里的 λ_nm→m；f_λ与dλ配用nm后已相消，不能再重复转换一次带宽。若另用 c_nm（nm/s），等价写Q_γ=F_syn/(h c_nm)。当前积分函数返回 F_syn，未自行乘10⁻⁹/(hc)；ROUND9等SED消费者必须声明采用哪一种量，未合入的单位修复不能写成已实现。

为严格取对数，引入固定单位数值 F_instr/U_instr、F_syn/U_syn，r=log10[(F_instr/U_instr)/(F_syn/U_syn)]；代码中的比值是该单位约定的简写。设 F_instr=a_syn F_syn·10^ε，则r=log10(a_syn)+ε，稳健location估计其中心，scale=10^(−location)·U_syn/U_instr。图像乘scale后进入所选积分单位；这给系数以明确物理解释，不需要单独可辨识 A、t、g。

正数标量b作用于全部F_instr时，r→r+log10b，Δ→Δ−2.5log10b，中位差和Tukey标准化残差不变，scale→scale/b。这是实数模型的尺度等变性，浮点实现不据此保证任意b逐位相等。若G=−2.5log10F_syn+C对全部源成立，则Δ=−2.5r−C，3mag等价1.2dex；实际G和目标TQ通带不同，C可随SED变化，不能无条件互换两个筛选域。

F_instr必须使用PSF模型的解析总通量，例如Moffat β=4的2πA s_x s_y/3；检测盒和、峰幅A、有限窗积分和该解析量不能混用。有限孔径时显式引入包围能量与其误差，不把seeing变化吸收到帧零点。

## Akima与缓存的可实现定义

输入曲线先按波长稳定排序，相同波长保留原序第一个值；这只是现实现策略，不是重复测量的加权合并。用于插值的唯一节点须严格递增；波长/效率非有限、非物理效率及不足节点的责任在入口验证。当前底层未完成所有这些检查，不能由头注释推断已拒绝。

令节点(x_i,y_i)，区间斜率m_i=(y_{i+1}−y_i)/(x_{i+1}−x_i)。两端外推两条斜率：m_−1=2m_0−m_1、m_−2=3m_0−2m_1，右端对称。节点导数

\[
t_i=\frac{|m_{i+1}-m_i|m_{i-1}+|m_{i-1}-m_{i-2}|m_i}
 {|m_{i+1}-m_i|+|m_{i-1}-m_{i-2}|}.
\]

分母为0时取(m_{i−1}+m_i)/2。区间h_i=x_{i+1}−x_i、z=(x−x_i)/h_i，插值为

\[
y(x)=(2z^3-3z^2+1)y_i+(z^3-2z^2+z)h_it_i
 +(-2z^3+3z^2)y_{i+1}+(z^3-z^2)h_it_{i+1}.
\]

两点退化线性，少于两点返回fill，区间外fill=0。当前Akima并不保证正性/单调性，也不钳T/Q过冲；区间外0可能制造边界折点。SciPy的makima是另一算法，不能用默认版本或不同方法的输出冒充此实现。

prepare_filter_cache保存完整输入谱网格spectrum_wl、重采样filter_trans、可选qe_trans和weighted_wl=λTQ；**weighted_wl不含Simpson系数**。生产逐星只解码原生SED样本，未将SED再Akima到1nm网格；参考计算须保持这一离散对象。有效QE缺省时Q=1；给定QE去重后不足2点也会走该分支，生产装配要求具名完整QE，底层兼容能力不能越权代替它。

## Simpson求积、量化和误差

对N点等距网格、n=N−1、步长h=(x_last−x_first)/n，令y_i=f_λ,i·weighted_wl_i。n偶数时

\[
I_{1/3}=\frac h3[y_0+y_n+4\sum_{i\,{\rm odd}}y_i
 +2\sum_{i\,{\rm even},\,0<i<n}y_i].
\]

n奇且≥3时，前n−3个区间用1/3（n=3时该段为0），最后3段用3h/8·(y_{n−3}+3y_{n−2}+3y_{n−1}+y_n)；n=1用梯形h(y_0+y_1)/2。N<2或值数组不足返回0。当前以首尾推h，未逐间隔验证等距，非等距输入不能套用这一公式。曾有n=3多算左端项的实现已经修复，343点偶区间不曾触发，不能继续报告为现存缺陷。

平滑g=f_λTQλ∈C⁴时复合1/3有|E|≤(b−a)h⁴ max|g⁽⁴⁾|/180；单个3/8尾段界3h⁵ max|g⁽⁴⁾|/80。不连续顶帽/窄线、Akima分段接点或未采样结构不满足全域光滑前提。旧顶帽例0.66%/1.34%、平滑例极小误差和真实曲线插值差是各自构型的读数，**不是最坏上界或常规全样本保证**。

把所选复合规则写作I=Σq_i f_i，q_i包括h、Simpson系数、λTQ，则对谱误差δf，δI=qᵀδf；已知谱协方差时Var(I)=qᵀC_f q。若量化确为就近且无裁切，每样本|δf_i|≤flux_mul/2，于是|δI|≤Σ|q_i|flux_mul/2；独立均匀量化假设才允许Var_q(I)=(flux_mul²/12)Σq_i²。编码舍入、谱点相关和实际TQ未核时不能套用独立抵消或“积分改善30倍”。逐星flux_min/mul不同；忽略它们、再乘G是不同模型，无法靠共同零点修复。

T和Q误差的一阶核分别为f_λλQδT与f_λλTδQ；共同响应参数对不同恒星产生相关误差，通常与颜色相关，不能当独立scatter相加。XP波段外缺失的积分、窄线谱重建偏差和真实仪器曲线不确定性需要额外模型；返回正F_syn只满足数值资格。

## 双向配对、粗筛和完整IRLS伪代码

WCS对全部参考投影并建Gaia KD树；图内计数只作诊断，当前未据此剔除树外候选。PSF树仅收status==0行。依PSF原序正向找最近Gaia，再反向找最近有效PSF；两个查询均距离严格<2px且回到同一PSF才收。刚好2px不收；双向唯一不等于无错配。精确等距由建树/遍历序决定，尚无稳定ID规则。

```text
records remain aligned with original PSF rows
raw = mutual_nearest_pairs(radius_px=2, strict)
for each raw pair:
  reject saturation flags SATURATED or HAS_SATURATED when supplied
  require finite positive F_instr,F_syn and finite r,Δ,G
  r = log10(F_instr/F_syn); Δ = -2.5log10(F_instr)-G
if no valid pairs: return NO_DATA (scale=1, sigma=0, fit_used=0)
keep |Δ-median(Δ)| <= 3 mag
if n_consistent < 3: return NO_DATA
l = median(r_consistent)
S = median(|r_consistent-l|)/Phi_inverse(.75)   # fixed afterwards
if S>0:
  for iteration=1..50:
    u = (r-l)/(4.685*S)
    w = (1-u*u)^2 for |u|<1, else 0
    if sum(w)<=0: break                        # no independent fail flag
    new_l = sum(w*r)/sum(w)
    if |new_l-l|<1e-6: l=new_l; break
    l=new_l
inliers = all consistent if S==0, otherwise |(r-l)/(4.685*S)|<1
scale = 10^(-l)
sigma = median(|r_inlier-l|)/Phi_inverse(.75) if n_inlier>=2 else 0
```

S=0不要求所有r一致：多数重合即可，故该分支仍可能包含离群值。50轮触顶和Σw=0均保留最后l；代码无内部取消点，也无显式converged输出。工程装配已用fit_ok区分NO_DATA的占位1与拟合产出的1；这一已修复行为不能沿用旧伪成功描述，但它仍不能识别IRLS触顶。

Tukey对应固定S下的有界损失ρ(z)=c²[1−(1−(z/c)²)³]/6（|z|<c）、外部c²/6，ψ=z(1−(z/c)²)²。令Σψ((r−l)/S)=0并整理得到上述加权均值不动点；有界非凸ρ可有多个驻点，IRLS不保证全局唯一/单调目标下降。MAD归一来自P(|Z|≤a)=2Φ(a)−1=1/2，得a=Φ⁻¹(.75)。在已知正确高斯尺度、独立同分布和通常正则条件下，location渐近方差为S² Eψ²/[n(Eψ′)²]；这才是4.685名义约95%效率的条件，不由固定一次MAD自动取得有限样本效率或50%崩溃点。

当前输出sigma围绕最终l而非inlier中位数，且经历预筛/数据依赖截断；它是约定散度，不自动服从标准未截断MAD抽样理论。历史有限样本b_n近似表n=3/4/5/9为1.495/1.363/1.206/1.107、n>9的n/(n−0.8)只对应指定原始高斯MAD估计器；实现未用该表，不能当普适IRLS后修正。Q33保留中心敏感度、尺度路线和崩溃点待推导项。

## 调用、复杂度及可复核证据

主链为scheduler::p1_op_photometry→fit_frame_photometry→质量位感知v2→SpectrumIntegrator/StarMatcher→实际apply_photometry；库内校正缓冲由装配丢弃，仅取拟合对象。空间m随后从标量内点拟合，应用α=k_photo m一次，不把缓存内试乘当产品已应用。C兼容入口不携带质量位、simple的QE参数不消费，与生产节点区分。

两侧KD建树当前每层排序，最坏O(n_G log²n_G+n_P log²n_P)，查询期望约O((n_G+n_P)log(n_G+n_P))但单次最坏O(n)，拥挤/退化构型不可保证对数。清洗排序O(n log n)，固定最多50轮O(50n)及O(n)内存；响应缓存每帧一次，逐星积分O(n_G N_λ)、缓冲O(n_G+N_λ)，逐星v2导出当前逐行线性找raw表，最坏O(n_P n_match)。图像试乘和实际应用各O(WH)，额外全图缓冲的寿命见detail。

历史实验[合成口径](../../../../实验/photometric-magnitude/docs/fsyn_convention.md)、[推导记录](../../../../实验/photometric-magnitude/docs/derivation_robust_weights.md)和[三路线](../../../../实验/photometric-magnitude/README.md)保留原样本/seed/失败与读数；含代数自检、同公式复算、MC及真实帧，证据强度分别解释。旧PHOT-001..007、F1–F6、I1–I6是测试设计编号，不证明现树存在或已经执行；固定r注入只测S=0，至少增加带噪S>0、污染、并列、饱和、非均匀网格、奇区间、量化与响应错配、触顶/零权和的独立真值构型。重算同公式的sigma一致性不检验定义正确；线程扫描也不替代样本置换。当前本轮仅静态语义审阅，未构建或运行光度算法。

## 原始文献与参考实现

1. Gaia Collaboration et al. (2023), *The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra*, A&A674,A33，[原文](https://arxiv.org/abs/2206.06215)；XP外定标Montegriffo et al. A&A674,A3见光度正本。对应谱的物理刻度和响应定义，不替具体XPSD量化/接口担保。
2. Akima H. (1970), *A New Method of Interpolation and Smooth Curve Fitting Based on Local Procedures*, JACM17(4),589–602，[DOI](https://doi.org/10.1145/321607.321609)。上述局部斜率与Hermite形式可和[SciPy Akima](https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.Akima1DInterpolator.html)核对，需冻结版本、method、端点和fill；其现代Simpson尾段政策不自动等于本库3/8。
3. Beaton A. E., Tukey J. W. (1974), *The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data*, Technometrics16,147–185，[DOI](https://doi.org/10.1080/00401706.1974.10489171)。Kafadar K. (1983), *The Efficiency of the Biweight as a Robust Estimator of Location*, J. Res. NBS88,105–116，[作者机构原文](https://doi.org/10.6028/jres.088.006)。有限样本、尺度和调参路线须分开，论文标题不构成每个数字的锚。
4. Croux C., Rousseeuw P. J. (1992), *Time-Efficient Algorithms for Two Highly Robust Estimators of Scale*, COMPSTAT1992,411–428，[作者机构存档](https://wis.kuleuven.be/stat/robust/papers/publications-1992/)；Rousseeuw & Croux (1993), *Alternatives to the Median Absolute Deviation*, JASA88,1273–1283，[DOI](https://doi.org/10.1080/01621459.1993.10476408)。渐近正态常数与有限n修正是两个对象；本轮未重取1992表格逐项核验。
5. Huber & Ronchetti (2009), *Robust Statistics*, 2nd ed., Wiley, ISBN9780470129906。分别讨论初步尺度和同时估计路线，不能替特定IRLS全局性质提供未证结论。[statsmodels稳健范数](https://github.com/statsmodels/statsmodels/blob/main/statsmodels/robust/norms.py)、[MASS rlm](https://github.com/cran/MASS/blob/master/R/rlm.R)为参考代码，采用时冻结commit及fixed/update-scale/初始化；未把其MM估计保证移植为本估计器结论。
6. Horne (1986), *An optimal extraction algorithm for CCD spectroscopy*, PASP98,609，[DOI](https://doi.org/10.1086/131801)；Naylor (1998), *An optimal extraction algorithm for imaging photometry*, MNRAS296,339，[DOI](https://doi.org/10.1046/j.1365-8711.1998.01314.x)。对应源测量的噪声结构，不是本帧系综散度的定义。[SExtractor官方误差说明](https://astromatic.github.io/sextractor/Param.html)的逐源FLUXERR/MAGERR与sigma_residual各有对象，不能互代。

生产代码为spectrum_integrator.cpp、star_matcher.cpp、pc_api.cpp、frame_photometry_fit.cpp；实验scia_common.py/scia_calib.py有同源复刻，不能全部称独立实现。SciPy/statsmodels采用BSD许可，MASS为GPL系列；参考、依赖与复制代码分别核对具体版本许可，方法引用不表明已经链接。Bessell & Murphy (2012), PASP124,140与Bohlin, Hubeny & Rauch (2020), AJ160,21（[DOI](https://doi.org/10.3847/1538-3881/ab94b4)）用于通带/绝对分光比较，不为本项目误差界兜底。
