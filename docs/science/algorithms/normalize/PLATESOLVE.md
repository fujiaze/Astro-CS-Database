# 天测匹配与 WCS 数值方法

本页补充[天体测量科学](../../detection/ASTROMETRY.md)的候选匹配、迭代和SIP表示。Normalize以校准像元调用IPV内部盲检，WCS后再产生独立科学源表；输入、输出和实际接口见[Normalize模块](../../../detail/normalize/modules.md#wcs)。本文不将源码容差或旧验收任务当作科学定义。

## 选星与匹配条件

图像样本先按检测质量、饱和标志与亮度选择，再构建相对图像中心的几何向量U。仪器角尺度先验s0=206.265·pixel_um/focal_mm，单位arcsec/pixel，只是初值；实际解由匹配与拟合获得。图像密度的分子取真实选中样本数，分母用同一视场立体角，不能混入已拒候选。

当前选星库排除饱和候选，非饱和不足时不以饱和样本补齐；空样本与不足以构造几何的样本明确失败。饱和测光可能截断、位置可能失真是选样依据，但不能由标志本身证明所有饱和源质心都不可用，标志与探测器真值仍须区分。

Gaia锥查询用星等窗[-1.5,m_lim]，在同一完整目录/几何/质量条件下放宽上限所得集合包含原集合。空结果时向更暗搜索可以扩大候选，重复失败、每文件返回上限和真正空扫描分别记录；有限查询次数下未找到源不证明全天区绝对无星。截断结果不能当完整亮度排序使用。

现实现还按图像完整检测序中的非饱和位次，选择星表FOV相同亮度序位，并把查询深度扩大到最深位次。该政策不等于两侧实际恒星必然同位次：颜色/通带、未检出、混叠、变星和星表完备性可改变排序。严格同位次与“唯一同域”的主张见[Q34](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，本轮保持代码政策，不改变候选模型或查询算法。

当前主链用三角形几何匹配产生初始对应，再做多项式拟合、迭代重投影、重匹配与稳健精化。内点数、残差和模型阶共同解释结果，不由较高阶或较低训练残差自动保证更准。0.002是无量纲三角形边长比描述符距离容差，初始尺度另有±20%限制；5arcsec是后续位置匹配半径，50–60是密度计算的目标数。这些是不同参数，不能互代。

## 线性与 SIP 表示

现IPV居中输出CRPIX=(w/2+0.5,h/2+0.5)，它是FITS一基生成策略，不约束外部WCS。若像元到切平面的线性项L单位arcsec/pixel，则CD=L/3600，inv(L)单位pixel/arcsec。高阶角度项经inv(L)转换成SIP像元偏移系数，不能混用inv(CD)而漏掉3600。

令u=p1−CRPIX1、v=p2−CRPIX2，则前向是CD·(u+A(u,v),v+B(u,v))。逆向AP/BP拟合畸变偏移坐标到原偏移的近似，多项式拟合包含恒等线性项时再从相应系数减1，不能把A/B简单取负。当前提取器有41×41/5阶与81×81/7阶逆拟合内部路径；固定C结果结构的数组容量与真实输出阶需分别核对，内部扩展能力不自动变成公开ABI能力。

训练网格和独立验证网格分开。缺逆多项式可迭代前向模型，但需收敛/有效域；拟合失败不能伪装无畸变。纵轴反射时CRPIX2′=H+1−CRPIX2、CD第二列取反，A′ij=Aij(−1)^j、B′ij=−Bij(−1)^j，逆系数同理；必须与实际数组反射同步。TAN半球、RA绕回、极区和历元的完整定义由科学正本拥有。

## 数值停机、稳健与误差

重投影迭代的0.01arcsec是自洽停机量，不是绝对天测精度界。另一个fit_sip的Huber/IRLS路径与生产extract_wcs_sip/robust_refine不是同一算法，不能拼接成一条虚构生产流程。1.345的经典尺度归一前提不自动覆盖代码中1.345·median(abs(r))的所有残差域，其问题见Q33。

det(L)退化、病态基、近TAN边界、反演无收敛和数值非有限均需显式状态；返回CRPIX或大坐标哨兵不等于合法解。内部残差RMS、rms_px的尺度换算和独立留出位置误差分别报告，配对/半径选择本身会截断分布。多套TAN实现往返一致也可能共享单位错，需第三方独立前向检查。

极区查询剪枝要有声明域内保守包围及全遍历对照。小场dra·cos(dec)近似、内部Lipschitz界和天球参考系是不同问题，不以“极区不剪枝”替代全部边界验证。二维角距离按球面量测，不以RA字面差判断精度。

## 独立检验与资料

构造已知CD/CRVAL/CRPIX与SIP的星场，独立WCSLIB/Astropy读出并检查前向坐标、逆表示、像元原点、手性、RA跨界与极区。给匹配留出星、失败样本、全部实际尝试和运行身份；缺原始WCS头不能判无解。消融和负例包含漏SIP、符号/原点/单位错、错误历元、少星与目录截断。

当前迭代/三角投票存在并行实现，不能以旧“求解全单线程”推断资源和确定性；可重复性核对真实样本序、浮点环境与归约，取消沿实际安全点说明。UTF-8错误串、句柄、ABI和日志属于工程接口，在模块页解释。

原文、历史残差和逆网格读数保存在[来源档案](../../../../实验/documentation-refactor/docs/ASTROMETRY_PHOTOMETRY_SOURCE.txt)。有效算法细节在下文展开，完整公开参数/布局/错误和调用例见[IPV详细设计](../../../detail/normalize/modules/platesolve.md)，不要求读者回档案拼接接口。

## 图像样本、位次与目录查询的完整步骤

图像工作坐标U=(x_det−W/2,−(y_det−H/2))，单位pixel，Y-up；检测器x_det/y_det为index+0.5。Gaia经以hint为中心的TAN得到W=(ξ,η)，单位arcsec。两侧不是同单位数组，三角边长比消去尺度，后续拟合输出为角秒。

设原检测表D，先排除saturated；剩余按mag升序取min(img_n_target,n_unsat)，不以饱和候选补足。目标要求有效亮度与坐标；当前select_image_stars仅用isnan排序而未逐项剔除NaN，候选不足时仍可能选入NaN，±Inf和平局的处置也不能由“星等可靠”文字保证，见E68。非饱和不足2报样本不足，但构造三角形实际至少3，最终接受另要求更多内点。

另将包含饱和在内的全检测表按mag稳定排序，以选中成员的1基位次r_i定义SelectDomain，n_depth=max(r_i)。n_depth衡量目录查询深度，|U|才是图像密度分子，不能互换。Gaia FOV列表按G排序后取第r_i个；FOV数量不足n_depth时报错，不改为最亮N。无饱和且同排序时退化为前N，但这是数据/排序条件，不证明不同通带图像与G星等的真实对应恒同位次；颜色、漏检和混叠问题留Q34。

光学hint给s0=206.265pixel_um/focal_mm（arcsec/pixel），近似FOV对角d=s0√(W²+H²)/3600（degree），查询半径r_q=d·radius_factor；实现用A_q=πr_q²和A_img=(Ws0/3600)(Hs0/3600)（degree²）。这些是小场面积近似，不是严格球面立体角。ρ_img=|U|/A_img，ρ_target=density_ratio·ρ_img，n_target=clamp(round(density_ratio·|U|·A_q/max(A_img,1e−10)),50,60)。当前f64入口实际图像目标和三角上限取60；查询目标再取max(n_target,n_depth)，不能把50–60写成全流程任何计数的硬范围。

目录锥搜对固定完整目录和锥体，G∈[−1.5,m]，m变暗（增大）只扩大集合。这个包含关系不保证带上限截断/读盘失败后的返回集合相同单调。当前局部log10计数割线算法为：

```text
N_target = max(n_target,n_depth) * safety
m = clamp(6 + 1.5log10(max(f_mm,1))
              + 2log10(max(t_nominal_s,0.1)) + m0_offset,
          clamp_lo, min(clamp_hi,13))
alpha = alpha_prior
repeat at most max_queries:
  query G in [-1.5,m]; increment query_count and scanned endpoints
  if read/query error: flag query_failed; retain last successful arrays; break
  record successful N, m
  if N>=cap_per_file: capped=true, converged=false; break
  if abs(N-N_target)/N_target <= density_tolerance: converged=true; break
  if N==0: reset secant history; m=clamp(m+zero_step); continue
  logN=log10(N+0.5)
  if two positive counts at distinct m:
    slope=(logN-logN_prev)/(m-m_prev)
    accept slope only inside (alpha_min,alpha_max)
  step=clamp((log10(N_target)-logN)/alpha,-6,+6)
  nonfinite step -> 0; update m inside allowed range
```

默认alpha先验0.2885、有效更新区间(1e−3,100)、safety=3、名义曝光180s、m0_offset=−4mag、clamp[6,22]、zero_step=3mag、查询上限4、数量相对容差0.1、cap阈200000。它们是当前启发式/配置，不是普适星数定律。wrapper传m_lim_m0_exposure_s，不是图像EXPTIME；初值曝光单位与最终物理测光标度不能混为一体。focal/exposure非正非有限、斜率限幅非法在迭代前拒绝。

MagIterOutcome/StarSelection记录m_lim_final、N_final、query_count、alpha_final、capped、converged、query_failed、zero_queries、empty_sweep、sweep_first/last，wrapper另记查询调用数/耗时。全部成功查询为0叫empty_sweep；无任何成功查询是query_failed；非空但次数用尽可以带converged=false继续，不能称已达到目标数量。当前以总N≥每文件cap保守标截断，跨多文件总数达到该数也不必确有某文件截断，这不是精确截断探测器。

末次成功表少于2、空扫描或capped终止选星；后续失败可使用先前成功非截断数组并保留失败状态。空扫描只证明已尝试区间/锥体/目录返回空，不足证明天区物理无恒星。投影要求cosc>1e−10，正常FOV矩形abs(ξ)<Ws0/2、abs(η)<Hs0/2，少于2时放宽1.5倍后再查数量。保留实际n_fov与窗口，不把扩展域数量冒充原图覆盖。

## 三角形描述符与匹配复杂度

每组三点的边a≥b≥c，记录各边所对顶点和形状d=(b/a,c/a)。非零相似变换下边长共同缩放，因此d不变，这是用三角比值的依据；透视/高阶畸变、噪声、近等边排序和平局会破坏对应的稳定性。当前仅检查最长边a≥1e−6，不等于排除了所有共线三点；源码“a小意味着共线”的注释不成立。

代码还剔除b/a>0.9。这包含近等边构型，不能称其恰是“细长三角形”；目前策略保留，科学选型/召回待独立构型验证。生成C(n,3)三角形后，两侧全枚举比较‖d_A−d_B‖²<0.002²，且像元/角秒边长比在[1/(1.2s0),1/(0.8s0)]内，为三个对应顶点各投一张整数票。它不是KD-tree O(nlogn)算法。

票数至少2的顶点对按票降序，平局按U/W索引升序；贪心保不重复U/W的对，最多取n_A。内部single的success可只表示max_vote>0，主求解仍要求max_vote≥3；不把任一局部success当已解WCS。当前主路径取60/60（不足按实数截取），没有旧20→40→60多轮扩增。

最坏工作O(C(n_U,3)C(n_W,3))，同阶约n⁶；n=60的未剪枝上界34220²=1,171,008,400。生成前以2×10⁹次比较为上限检查，大于返回budget_exhausted和参数原因。局部票矩阵占O(workers·n_U n_W)，三角集合O(n_U³+n_W³)。整数归并在无溢出时次序无关，不能由此证明所有浮点拟合及完整解任意平台逐位一致。

## TRANS拟合、裁剪与重投影

对阶p，单轴基φ=(1,x,y,x²,xy,y²,…)，项数m=(p+1)(p+2)/2。以匹配对求min Σ[φ(U_i)c_x−ξ_j]²及y轴同式，代码形成M=Σφφᵀ、b_x=Σφξ、b_y=Σφη，再高斯消元。未作完整列归一/QR的路径应核对条件数；足够数量不保证几何满秩，高阶项单位arcsec/pixel^(i+j)。

阶1/2/3最少对数3/6/10，初始取票序前6/12/20对拟合，再扩展到全部初始配对。裁剪步骤先去距离>50arcsec，再在首轮尝试5arcsec预滤（剩余足够才采用），取距离平方的35%分位q35，必要时钳到5²，拒d²>10q35，余数不足或矩阵奇异失败。这里“sigma”变量是arcsec²的分位数，不是Gaussian σ。

`iter_trans_inner`当前is_ok初始化true，成功重拟合后便break，不能把上限5说成必有多轮收敛。随后两轮at_match_lists+at_recalc_trans另做扩增：双向暴力最近邻均在5arcsec闭半径内才保留，等距按遍历先者；重拟合后用q68.3(d²)的9倍作一次距离平方裁剪，足够且可拟合才替换。最终rms=√mean(d²)，trans.sig仍为q68.3(d²)，不能把sig直接标为arcsec RMS。

外层尝试order=3→2→1，第一成功阶停止；全部失败才返回求解失败。拟合成功后固定对应索引迭代更新TAN中心，消去TRANS平移，再重新投影/拟合，最多5轮，以平移hypot(x00,y00)≤0.01arcsec停止。未收敛与失败状态按实际结果解释，不把0.01arcsec写成天空绝对误差界。

高阶时对当前U与新切平面W再双向匹配，满足阶最少项数才重拟合；最多5次，以匹配数差≤1且RMS相对变动<1%作为局部停机。保留重拟合自身裁剪后的inliers给最终RMS，不能把未裁剪匹配表替代已拟合样本。

## 稳健扩增精化与非生产SIP拟合

生产robust_refine_wcs在高阶重匹配后使用U_full扩展空间采样，默认网格短边按FOV<1°/1–3°/>3°为2/3/4；目标数小场1000、常规180，上限300与最少100是采样政策，具体小场与容量分支按该函数实现解释。模型仍为TRANS两轴系数，残差在角秒域，Tukey c=4.685，最多10轮；参数变化1e−9和RMS相对0.001为停机量。

默认CD相对变化<1%不阻尼，>3%强阻尼，基础/冻结阻尼1e−4/1e6。匹配半径按迭代取2/1.5/1倍RMS及1/0.5/0.3arcsec地板；最近邻比0.75、绝对0.5arcsec、8近邻空间一致性、3σ与可疑权0.1是默认防护。连续2次RMS上升>10%、匹配下降50%等触发回退；最终RMS超过初始1.5倍或匹配少于30也回退。以上解释头的数值含义，不将其声称为普适优化保证；初始rms参数在调用处取trans.sig，平方分位与arcsec的关系需核验，见E68。

robust_refine成功且非fallback时采用新TRANS及U_full索引，否则保留先前解；它不是非生产ipv_sip.cpp::fit_sip的Huber15轮分支。后者保留为独立库路径，δ=1.345median(abs(r))不等于已正确归一的Gaussian稳健尺度；经典效率结论需额外条件，不将它拼入生产流程。

## SIP离散拟合与输出状态

令L为TRANS线性项，q=(x,y)为内部居中Y-up像元，H_ij=(x_ij,y_ij)角度系数。清除平移后T(q)=Lq+Σ_{i+j≥2}H_ij x^i y^j=L[q+(A,B)]，故(A_ij,B_ij)=L⁻¹H_ij。L以arcsec/pixel表示，其det单位也相应变化；阈1e−15是该坐标/数值标度的工程阈，非单位无关物理常数。

逆拟合在原网格[-W/2,W/2]×[-H/2,H/2]取41²点，先得到畸变坐标q_d=L⁻¹T(q)。归一坐标q_d/(R_u,R_v)与目标q/(R_u,R_v)，基含常数/线性至5阶；正规方程解后回转AP系数c^x_ij R_u^(1−i)R_v^(−j)、BP系数c^y_ij R_v^(1−j)R_u^(−i)，再AP_10/BP_01减1。这得到q≈q_d+(AP,BP)，并非A/B简单取负。

内部扩展81²点/7阶/stride10与公开41²点/5阶/stride6并行存在；扩展可作迭代初值，未进入现C结果布局。逆拟合奇异目前警告并留无逆状态；需消费者迭代前向SIP并检查收敛/有效域。非线性线性项奇异现已明确失败，不沿用旧仅warn成功描述。CTYPE/残差及后置尺度/内点政策见模块页。

逆多项式训练残差、独立密集域逆误差、重复前向反演自洽和留出星绝对误差分别报告。原文117px畸变下5阶一步误差与迭代极小读数是不同算法对象；不由内部高阶/迭代结果推出任何36系数C消费者一步逆必满足1e−4px。

## 参考文献与独立实现

1. Greisen E. W., Calabretta M. R. (2002), Representations of World Coordinates in FITS, A&A395, 1061–1075，[原文](https://arxiv.org/abs/astro-ph/0207407)；Calabretta M. R., Greisen E. W. (2002), Representations of Celestial Coordinates in FITS, A&A395, 1077–1122，[原文](https://arxiv.org/abs/astro-ph/0207413)。对应FITS原点、线性矩阵、球面旋转/TAN，具体条件由天测正本展开。
2. Shupe D. L. et al. (2005), The SIP Convention for Representing Distortion in FITS Image Headers, ASPC347, 491，[NASA正式约定入口](https://fits.gsfc.nasa.gov/registry/sip.html)。SIP在像元偏移域的定义优先于源码中误称“标准A/B是度”的注释。
3. Groth E. J. (1986), A Pattern-Matching Algorithm for Two-Dimensional Coordinate Lists, AJ91, 1244，[DOI](https://doi.org/10.1086/114099)；Valdes F. G. et al. (1995), FOCAS Automatic Catalog Matching Algorithms, PASP107, 1119，[DOI](https://doi.org/10.1086/133667)。对应几何匹配族，不为本项目0.002、剪枝和投票常数提供普适精度证明。
4. Huber P. J. (1964), Robust Estimation of a Location Parameter, Ann. Math. Statist.35, 73，[DOI](https://doi.org/10.1214/aoms/1177703732)。对应Huber族参考，不直接证明未归一径向残差的1.345效率；Tukey来源见[光度算法](PHOTOMETRIC_FIT.md)。
5. [WCSLIB](https://www.atnf.csiro.au/people/mcalabre/WCS/)与[Astropy WCS](https://docs.astropy.org/en/stable/wcs/index.html)可做独立标准读取/前向对照；实际验证须冻结库版本，WCSLIB与Astropy的许可按所采用分发核对，不能把Astropy封装当另一个完全独立数学内核。
6. [Astrometry.net 0.98](https://github.com/dstndstn/astrometry.net/tree/0.98)的solver/quad-builder/verify/libkd可作另一种求解路线；团队源码和含依赖整体分发许可不同，采用前核其LICENSE，不从方法对照推断已链接/同实现。[SCAMP](https://github.com/astromatic/scamp)的多帧校准是邻近问题，与本帧IPV的三角流程不同。

本轮重新核对仓内生产段，未重跑旧真实帧、独立WCS或原文全部推导。科研复核以明确观测模型/样本/算法和外部真值为依据，历史“通过”、单一训练RMS或源文件行数均不能代替。
