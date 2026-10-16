# 图像盲检与引导拟合的数值方法

本页补充[检测科学](../../detection/STAR_DETECTION.md)的数值构造。Normalize先由IPV内部候选求WCS，再以Gaia引导独立science_sources；两者不共享候选职责。真实sdet入口、字段、所有权与调用见[检测模块](../../../detail/normalize/modules/star_detection.md)。本页不授权更换检测核、拟合模型或阈值。

## 噪声与阈值

对候选窗内点源，以像元信号单位U写观测近似 I_j=B+A E_j(c,σ_x,σ_y,θ)+n_j。B/A单位U，c/σ单位pixel，θ内部radian，E为像元中心评价的无量纲椭圆Gaussian。该模型用于定位和粗形状，不等同单位总通量PSF的像元积分；欠采样、翼部、邻源和空间背景会导致模型误差。当前最小二乘没有按逐像元协方差白化，残差平方和不能直接当已校准的统计χ²，亦不自动产生完整参数协方差。

对未裁剪平稳噪声n，d_j=n_j−n_{j−1}，由Var(X−Y)=Var(X)+Var(Y)−2Cov(X,Y)得到下式。空间信号差S_j−S_{j−1}仍叠加在观测差分上；取行中位只抑制部分污染，不是完整背景分离证明。

图像单位继承输入，不能一概称原生ADU。行差分d=I_yx−I_y,x−1在局部平稳噪声下有Var(d)=2σ²(1−ρ_adj)。现存估计器逐行以median/MAD做至多3轮5σ裁剪，MAD系数1.482602218505602，取行标准差中位数再除√2。该程序与未裁剪总体方差恒等式不同，相关、有限样本、结构和选择均影响偏差；另一全帧裁剪RMS也不是同一估计量。

盲检平滑图S上的阈值取median(I)+5σ_raw，σ_raw来自未平滑图。独立同方差与固定核下，σ_sm=σ_raw√ΣK²；连续二维高斯的1/(2σ_s√π)只是近似，不能把实际递归核σ_s=2直接当解析高斯。实际核范数与引用域见检测正本。高斯星的连续峰值衰减可用σ_p²/(σ_p²+σ_s²)解释趋势，但不直接确定99%召回阈。

## 递归平滑

现存YvV族每维参数为

\[
 q=\begin{cases}3.97156-4.14554\sqrt{1-0.26891\sigma_s},&\sigma_s<2.5,\\
 0.98711\sigma_s-0.96330,&\sigma_s\ge2.5.\end{cases}
\]

原始系数b0=1.57825+2.44413q+1.4281q²+0.422205q³，r1=2.44413q+2.85619q²+1.26661q³，r2=−1.4281q²−1.26661q³，r3=0.422205q³；B=1−(r1+r2+r3)/b0，b_i=r_i/b0。前向w_n=Bx_n+Σ_{i=1}^3b_iw_{n−i}，反向同系数；当前边界用首/末值种子。σ≤0.5或宽高小于4直通，float/double分别计算。

q²的1.4281是当前实现值，旧文转述1.4251及“完整Triggs边界矩阵”的对应未统一。保持当前系数与边界，原文与冲激响应需后续核验；不能因名称相近自动换值或把简单末值种子当已实现全部论文边界方法。

## 候选、分裂和初值

阈值图按8邻接提取组分。现存去混叠构造采用30层指数阈值t_i=t_det(Smax/t_det)^(i/30)，相对父组分的流量比例δ_c=5e−3，至少两枝满足条件才分裂。枝流量、父流量是否扣阈底及重分配规则必须随定义给出；当前以双变量高斯argmax分配剩余像素、平局按登记序，不把它说成原论文逐项相同实现。

局部极大扫描以11×11邻域和确定平局规则选峰，半径5的边缘带另处理。一阶差分过零给中心初值；二阶差分首次零交叉给两侧距离，Sr=(d_left+d_right)/2、Sc类似，缺侧有现存回退。平滑幅度读数通过√e缩放，只有相应高斯假设下才有严格参数解释。

候选拟合盒R=max(ceil(√(−2ln0.001)Sr),ceil(√(−2ln0.001)Sc),5)，上限200且按边界收缩。该高斯能量半径与实际方形窗、Moffat翼或饱和窗不是相同通量份额。候选曼哈顿去重半径max(1,floor(0.2R))与分裂树并存，不能由分裂树存在推断近邻召回已经完整。

## 模型拟合、质量与输出

检测侧模型为B+Aexp(−½[(x'/σ_x)²+(y'/σ_y)²])。当前参数直接为(cx,cy,σ_x,σ_y,θ,A,B)，不再用旧SX/fr/alpha参数化；旧式可表示同族轮廓，但边界、初值和优化轨迹不自动等价。FWHM=2√(2ln2)σ；不能与PSF库Moffat4同名宽度互换。halfA边界搜索和局部背景初始化后调用现有信赖域LM，既有迭代/容差不是全域误差证明。

饱和像素可从拟合样本排除，样本不足拒绝。平顶诊断使用dynrange=min(max(I),65535)−median(I)，0.7/0.1倍的邻域条件不等于探测器饱和真值；原生阈值、校准和实际标度另保留。饱和候选的某些质量门豁免按实现解释，不能推广到所有科学消费者。

拟合后质量检查包括FWHM正且大于0.5px、轴比、MAD残差/A（非饱和时阈0.2）和候选宽度相关上限。额外盲检形状门以帧候选FWHM中位参考、峰占比和四分之一峰像素数筛选；当前默认比例0.5/2.5、峰占比0.35、最少像素4，属于具体采样域政策，不适用未标定窄核或不同相位。引导路径不经过该额外门。

mag由实际方形窗扣拟合背景后的和计算，非正和得到NaN；API名flux当前记录拟合振幅A，二者不能混称总通量。输出拟合后去重、按mag稳定排序、NaN置尾和maxStars截断。Gaussian定位/粗量与后续Moffat积分PSF的测量身份分别保留。

## 统计与独立核验

盲检阈值表、相位、PSF、种子、注入/匹配数及虚警分母必须绑定。零失败二项试验的一侧下界、至少299的样本条件和择阈多重比较见检测正本；旧n≥298和“经验零失败格点必定是阈”不再作为正式定义。单个宽度上的非单调有限样本读数不能单独证明不同物理区制。

核验需要噪声/相关场、冲激核、边缘、不同宽度/相位、拥挤与不等通量双星、饱和/失配、纯噪声、坐标转换及并发输出。稳定排序不证明全浮点逐位相同，库间行为相似也不证明独立真值。记录的历史读数、召回阈表、谱系备选和逐算子原文完整保存在[检测来源](../../../../实验/documentation-refactor/docs/DETECTION_SOURCE.txt)；本轮未重跑这些实验。

## Gaussian最小二乘、Jacobian与数值后端

以d_x=x−c_x、d_y=y−c_y，x′=d_x cosθ+d_y sinθ、y′=−d_x sinθ+d_y cosθ，u=x′/σ_x、v=y′/σ_y、E=exp[−(u²+v²)/2]。残差f_j=B+AE_j−I_j，目标min Σf_j²。样本坐标使用当前库index+0.5约定；对外科学对象的整数中心转换由adapter负责。

对σ_x/σ_y>0的光滑域，直接微分给出每行解析Jacobian：

\[
\begin{aligned}
 J_{c_x}&=AE(u\cos\theta/\sigma_x-v\sin\theta/\sigma_y),\\
 J_{c_y}&=AE(u\sin\theta/\sigma_x+v\cos\theta/\sigma_y),\\
 J_{\sigma_x}&=AEu^2/\sigma_x,\qquad J_{\sigma_y}=AEv^2/\sigma_y,\\
 J_\theta&=AE(-u y'/\sigma_x+v x'/\sigma_y),\\
 J_A&=E,\qquad J_B=1.
\end{aligned}
\]

实现把非正或≤1e−8的宽度评价值钳到1e−8，但上述Jacobian仍按正宽度公式计算；钳位域不应当作原光滑目标的精确导数。θ先用atan2(sinθ,cosθ)折算以避免大角度三角函数失精；圆星σ_x=σ_y时θ不可辨识。所有最终宽度必须正且参数有限。

线性化f(p+h)≈f+Jh，在缩放信赖域‖Dh‖≤Δ内求步。阻尼子问题为

\[
 (J^TJ+\lambda D^2)h=-J^Tf,\qquad
 D_{jj}=\|J_{:j}\|_2.
\]

当前自研后端形成正规矩阵后Cholesky，不是MINPACK或GSL的QR内核；形成JᵀJ会放大条件问题，阻尼可使步可算但不证明参数可辨识。D的零/非有限列取1，随后小于1e−10·max(D)的列钳至max(D)，这是项目数值保护，不归为论文普适秩判据。

记q=Σf²、g=Jᵀf、H=JᵀJ，预测下降pred=−2hᵀg−hᵀHh，实际下降act=q−q_trial。pred>0且有限时ρ=act/pred，否则ρ=−1。有限试探代价且ρ>1e−4接受；拒步缩Δ并放大阻尼，内部重试有界。ρ≥0.75时Δ=max(Δ,3‖Dh‖)，ρ<0.25时Δ减半；接受后阻尼乘max(1/3,1−(2ρ−1)³)，拒步的ν倍增。3/2控制信赖域，不等于每次λ固定乘3/除2。

每次solve的具体停机顺序是参数步、代价下降、梯度；xtol=ftol=gtol=1e−3、xtol_abs=1e−6。参数步检查每分量abs(h_j)≤xtol_abs+xtol·abs(p_j)。这类相对步判据按不同参数单位工作，不保证中心误差≤1e−3px。代价分支中当前变量`cost_old=cost−actred`与“旧代价”的注释不符，梯度分支使用此次步前g；需按实际实现与独立负例核对，见E67，不能因注释称“与GSL一致”而当数值等价已证明。

默认20轮，去顶饱和通道60轮；到上限不接受为Success。七参数重试、冻结θ六参数、同宽五参数及轴比失败后重拟合的具体次序见[O12](../../../detail/normalize/modules/star_detection.md#o12-椭圆高斯与lm数值过程)。降维是当前回退政策，不能证明对任意真实椭圆PSF都无偏；比较模型残差、定位和形状并保留失败构型。

拟合残差的MAD=median(abs(f−median(f)))与RMSE=√(Σf²/m)是两个量。O13使用1.482602218505602MAD/A，内部码名RMSE_TOO_LARGE不改变这一事实；这也不同于Poisson误差、背景估计方差或参数标准误。

## 通量、候选选择与误差解释

无限连续Gaussian积分F_total=2πAσ_xσ_y来自旋转Jacobian为1与两轴Gaussian积分。它与离散中心采样和、截边方窗和、去顶拟合恢复量均有不同条件。若写成F_total·P，P必须是单位积分模板；当前out_flux=A，不能直接传入期望F_total的科学接口。

理想圆Gaussian卷以归一连续Gaussian σ_s后，中心幅度A_s=Aσ_p²/(σ_p²+σ_s²)。忽略其余门与噪声时，A_s>5σ_raw给出趋势界A/σ_raw>5(1+σ_s²/σ_p²)。这只推导阈值交叉趋势；实际递归核、像元积分、噪声估计及O4/O7/O10/O13共同决定检出概率。不能把等式点叫99%召回阈，更不能由有限样本非单调性推断另一种物理区制。

对固定平滑算子K，C_sm=KC_rawKᵀ；白噪声、内区平移不变时才简化为σ_sm²=σ_raw²ΣK²。边界常量种子使实际K依位置改变。当前系数的核读数在检测正本保留；旧35.45是连续近似，与当前核37.5448的等效噪声倍数不同。有限精度、非有限填充及数据选择进一步影响其解释。

高斯“2σ可分”仅适用于特定等幅连续双峰的鞍点分析，不能当不等流量、噪声或O8固定11×11窗的普遍分辨率界。30层、δ_c=0.005、argmax分配和候选曼哈顿门均须纳入完整召回核验。O11的实际i=1..29、根阈底流量、缺交叉矩和确定argmax见模块页；它不是作者软件逐项相同实现。

## 科学实验与测试设计的保留范围

基线的定位目标0.3px、FWHM相对10%、虚警0.1/千像素，以及FP64中心0.05px、A/B相对1e−3、历史uint16量化通道0.5px均是特定构型设计参考。引用时同时指定PSF族、采样/相位、图像单位、噪声/相关、背景、掩膜、匹配和统计量；不存在由浮点dtype直接推出这些阈值的定理。Gaussian自拟合只能测相应模型域，Moffat真值用于测模型失配，二者结果分开。

独立检验从解析前向图像开始，经模块页示例的真实API读取中心/A/σ/孔径mag，再对照独立模型与所有状态；不要把拟合初值或输出文件自身当真值。Jacobian作独立差分/高精度核验，LM用带退化与条件数梯度的实例对照MINPACK等外部实现，几何另测半像元转换。

历史召回表的PSF档、1000次零失败、24seed等数字只在原记录载体范围成立；单侧Clopper–Pearson样本量修正和择阈限制见科学正本。原AR(1)对照、wrapper裁剪RMS增益78.47%等没有因本轮重排而得到新实验支持。wrapper背景估计器与主sdet行差分不同，不能借其读数证明当前CLI噪声模型。上述历史数字留在实验来源，不作为开发必须追逐的另一套门禁。

## 参考资料

1. Young I. T., van Vliet L. J. (1995), Recursive Implementation of the Gaussian Filter, Signal Processing 44, 139–151，[DOI](https://doi.org/10.1016/0165-1684(95)00020-E)。对应递归滤波族；当前1.4281系数与原式、完整有限边界对应仍待逐式核验。
2. Triggs B., Sdika M. (2006), Boundary Conditions for Young–van Vliet Recursive Filtering, IEEE TSP 54, 2365–2367，[DOI](https://doi.org/10.1109/TSP.2006.871980)。当前常量种子不自动等同该文完整边界矩阵。本轮没有取得该论文逐页复核。
3. Bertin E., Arnouts S. (1996), SExtractor: Software for Source Extraction, A&AS 117, 393–404，[DOI](https://doi.org/10.1051/aas:1996164)。阈值提取、分裂与测光族参考；项目常数与重分配变体分别声明。旧文“已读扫描原页”与“未取得原页”互相冲突，本轮访问作者PDF/出版入口未成功，不冒认已解决该引文问题。
4. Stetson P. B. (1987), DAOPHOT: A Computer Program for Crowded-Field Stellar Photometry, PASP 99, 191–222，[DOI](https://doi.org/10.1086/131977)。核响应的检测/质量思想，不是当前O10三比值的来源；未取得明确原式的roundness转述不当作当前实现规格。
5. Levenberg K. (1944), A Method for the Solution of Certain Non-Linear Problems in Least Squares, QAM 2, 164–168，[DOI](https://doi.org/10.1090/qam/10666)；Marquardt D. W. (1963), An Algorithm for Least-Squares Estimation of Nonlinear Parameters, JSIAM 11, 431–441，[DOI](https://doi.org/10.1137/0111030)。对应阻尼最小二乘。
6. Moré J. J. (1978), The Levenberg–Marquardt Algorithm: Implementation and Theory, LNM630, 105–116，[DOI](https://doi.org/10.1007/BFb0067700)；Madsen K., Nielsen H. B., Tingleff O. (2004), Methods for Non-Linear Least Squares Problems, 2nd ed.，[DTU](https://orbit.dtu.dk/en/publications/methods-for-non-linear-least-squares-problems-2nd-ed)。用于缩放/信赖域谱系，不把自研正规方程称成原QR实现。
7. [MINPACK作者代码](https://netlib.org/minpack/)，lmder/lmpar为可独立复核的非线性最小二乘/阻尼参考；源分发的许可按其[notice](https://netlib.org/minpack/disclaimer)核对，不推断参考代码已链接进生产。
8. [SExtractor 2.28.2 refine.c](https://github.com/astromatic/sextractor/blob/2.28.2/src/refine.c)是本轮查阅的作者代码版本。其分裂基准取根对象fdflux，枝减当前阈底；gatherup包含幅度/交叉矩及随机分配，区别于本项目轴向score的确定argmax。文件头为GPL-3.0-or-later，纠正旧detail的LGPL描述；本轮未复制其源码。
9. [SEP 1.4.1 deblend.c](https://github.com/sep-developers/sep/blob/v1.4.1/src/deblend.c)与[许可证](https://github.com/sep-developers/sep/blob/v1.4.1/licenses/LGPL_LICENSE.txt)供独立分裂参考，不能将SEP许可套给SExtractor。仅核对参考位置/部分分支，未完整复现实验。

CLEAN/DAOFIND/Kron的备选身份与当前O1–O16逐项规格见[模块详细设计](../../../detail/normalize/modules/star_detection.md)，本轮不切换检测器。Kron的一阶矩和k=2.5/损失约6%为基线转引，未经原文与构型确认不升级为通用通量保证；Irwin1985、Nielsen1999的旧转引缺完整著录，不编造页码。独立方法对照不能替代实际运行和来源绑定。
