# 统计、Gaia、天光及传统合成的独立研究证据

本记录服务于 #111。它从明确的有限维观测定义构造真值，不调用 ACSD 生产统计函数，不修改 C++。科学规范的正式验收仍由另一个独立审核者完成。本研究 Agent 的断言、运行成功及 Monte Carlo 结果都不代表验收。

代码：[statistics_independent.py](oracles/statistics_independent.py)。最终结果：[statistics_independent.json](evidence/statistics_independent.json)。保留的失败尝试：[statistics_parallel_loo_failed.json](evidence/statistics_parallel_loo_failed.json)。代码中的 `loo_rejection(..., max_rejections=None)` 保留该失败算法，正式参考缺省为 `max_rejections=1`。

## 1. 阅读、依据与证据等级

已阅读根 AGENTS、最高设计、科学索引、统一模型、noise_snr、sky、integration、rejection、photometry 正文；直接检查下述 C++ 源码，未执行其统计函数。问题的数学定义来自观测模型，生产实现只用于发现可证伪的差异。

一手依据：

- [Gaia Collaboration, 2023, Synthetic photometry](https://arxiv.org/html/2206.06215v2)，§2、式 8–12 和 Appendix B。采用线性谱基积分及其完整系数协方差；Appendix B 的一般经验建议为 `Rf=FWHM_passband/FWHM_LSF >= 1.4`。这不是任意窄谱线的数学误差保证；无强谱特征连续谱可能在更窄通带偶然保持通量。
- [Montage algorithms](https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html)：采用重叠差分、全局一致的相对背景思路。这里明确额外保留公共天空设计矩阵和实际控制协方差。
- [Keihänen et al., 2010, Madam](https://arxiv.org/abs/0907.0367)：借鉴公共天空与加性基线联合求解和公共模消元；其扫描几何不能原样等同 CCD 马赛克。
- [Zackay & Ofek, 2017](https://arxiv.org/abs/1512.06872)：点源检测/测光最优合成依赖各帧 PSF；它不能替代逐叶面亮度估计量的方差。

本记录的矩阵恒等式及 Gaussian LOO FWER 界是数学结论；浮点残差、MC 覆盖及反例是指定算例的独立数值证据；Gaussian/Poisson 条件分布、已知响应和误差身份是模型假设；真实 QE、滤镜形状、LSF、星云 SED、真实相关噪声和污染分布仍待 HST/M42 等真实观测验证。本文数值误差读数不构成任意输入的已证明精度界。

## 2. 统一误差空间与可执行表示

原生像元积分量 `x_j [L]`，原生未收缩球面面积 `Omega_j [sr]`，drop 面积 `A_j [sr]`，真实交叠面积 `a_jp [sr]`。令

```
f_jp = a_jp/A_j
N_p = sum_j f_jp Omega_j                    [sr]
R_pj = f_jp/N_p                            [sr^-1]
y = R x                                   [L/sr]
```

`drizzle_operator(a,A,Omega)` 实现该定义。它不计算几何，因此本脚本不能证明 WCS/SIP 曲边交叠正确。常面亮度输入满足 `R Omega=1`；某叶无支持时不能用 0 代替未知信号。

设基础独立/局部相关噪声为 `epsilon`，协方差 `D`，共享 Gaia、flat、母版等潜参数为 `eta`，协方差 `Lambda`。在已经保留交叉来源的统一潜变量空间，写

```
delta y = L epsilon + U eta
C_y = L D L^T + U Lambda U^T.
```

`L` 可为校准、Drizzle、控制采样及其逆向作用的组合。若两个来源实为同一批观测，不得假装独立后拼接；先合并潜变量，或存储其交叉块。Gaia/flat 因子通过全局 source/calibration identity 共用潜参数列，不能在各帧复制成独立列。

大规模数据采用算子 `apply(v)`/`apply_transpose(v)`、源噪声方差/局部稀疏块、共享稀疏因子及 latent covariance。`covariance_factor_action` 直接计算矩阵向量乘，不形成 `Npix^2` 图像协方差。单叶/孔径方差按 `l D l^T + u Lambda u^T` 求。若基础协方差是正对角并加低秩，`woodbury_solve` 实现 Woodbury；不是对任意重采样相关协方差套对角近似。

复杂度：一次协方差作用为 `O(nnz(L)+cost(D)+nnz(U)+cost(Lambda))`；若 `D` 对角，`Lambda` 为 q 阶稠密，存储 `O(nnz(L)+nnz(U)+q^2)`。Woodbury 预处理 `O(N q^2+q^3)`、存储 `O(Nq+q^2)`，每次 inverse action `O(Nq+q^2)`。共享参数巨大时按观测组分块/稀疏处理，不宣称 Gaia 任意全局相关均为小 q。

对同数据拟合天光 `beta_hat=K T y`、实际图像求值 `E`，

```
P = I-E K T
z = P y
C_z = P C_y P^T
L_z = P L; U_z=P U.
```

应用 P 的代价为控制采样、参数求解及求值的代价，不存储稠密 P。它包含负交叉项，不能一般表示为“原方差加一个正低秩 sky 方差”。若 sky 是独立外测参数，才使用相应独立块。扣除天光均值保留已经产生的 photon shot noise。

未知方差、未知共享相关、未知单位分别记录；完整方差为 `null/unavailable`，不能填 0。数学未知与已证为零的精确约束有根本区别。

## 3. 通量 GLS、半正定协方差与 Fisher/面亮度区别

对 `y=F h+Z beta+n`，已知正定 C 时白化并投影背景，`I_F=||(I-Pi_Z) C^-1/2 h||^2`。冗余背景列不自动使 F 不可辨。`linear_gls_operator(A,C,requested=0)` 使用谱分解白化和 SVD，先判断被请求参数是否可估计；未通过时返回具名失败，不能把伪逆某个有限对角元当作真实误差。

若 C 半正定，将其正谱空间白化为 W，零空间矩阵为 E，保留 `E y=E A theta`。令 `K0=(EA)^+ E`、N 为 `EA` 的零空间基，则

```
theta_hat = K0 y + N (W A N)^+ W (I-A K0) y.
```

参数可估计性须在 `[EA; WA]` 上检查；`apply_linear_gls` 还检验精确方程相容性。不人为加小对角地板。`C=diag(1,0), h=(0,1)` 时第二个观测精确给 F，代码返回 `V_F=0`；直接 `h^T C^+ h=0` 会错误宣告无信息。观测协方差数值秩阈值显式记为 `1e-12 lambda_max`，它定义舍弃的数值模型，不能充当科学误差证书。病态尺度须先按单位归一。

点源信息量与面亮度精度的反例：`P_A=(.5,.5), var_A=.5`；`P_B=(.9,.1), var_B=.82`，面积均为 1。同样 `V_F=1`，面亮度权重分别 `2` 与 `1.2195121951`。实际场景源 Poisson 必须从真实非负期望求，不能把规定参考源的 Poisson 直接施给空场。

指定满列秩 R、同一 C、同一模板及背景下，`A^T C^-1 A=(RA)^T(R C R^T)^+(RA)`；这是条件信息不增加/不减少的恒等式。代码验证满列秩反例矩阵的等式，不外推到降采样或缺失随机贡献。

## 4. 可执行 Gaia 联合拟合

`gaia_photon_integral` 接受真实可重建谱基 `B_j(lambda_nm)`、系数 c 和完整谱系数协方差 `C_c`。定义

```
w_j = integral B_j(lambda_nm) QE(lambda_nm) T(lambda_nm)
               lambda_nm*1e-9/(h*c_light) d lambda_nm
q = w^T c                    [photons s^-1 m^-2]
Cov(q_k,q_l) = w_k^T C_c w_l.
```

输入谱密度为 `W m^-2 nm^-1`，积分变量仍为 nm，因此只在光子能量的波长因子乘 `1e-9`；不能再对 `d lambda_nm` 多乘一次。若先将密度转换到每米，则积分变量也需转换至米，两种路径相同。响应如果已经含光子因子，不重复加入。XP 光谱已外定标，不二次乘 Gaia G 星等。缺谱协方差时 q 可计算而 variance 为 null。

所有滤镜跳变端点、谱重建/响应分段结点分割积分；通带部分超出谱支持时，不将未知部分的物理信号当零。本脚本的调用算例都在已声明完整支持域内。一般求积返回 Quadpack 的数值误差估计而不是严格证书。此处光滑多项式/QE 另以独立解析原函数验证。

仅按恒星匹配标度无法证明星云 SED 可转换为共同响应。3/7nm 标量反例见 §7。Gaia 全局源身份必须贯穿跨帧拟合。

`fit_shared_gaia(counts,gaia_q,design,star_index,count_cov,gaia_cov)` 提供直接可运行的统计 baseline：

```
C_i = exp(X_i theta) q_star(i) + e_i
q_Gaia = q + eta
min_(theta,q>=0) ||L_C^-1 (C-exp(X theta) q_star)||^2
                + ||L_G^-1 (q_Gaia-q)||^2.
```

使用带解析 Jacobian 的 trust-region least squares，保留共享 q 为 nuisance 参数。这样不会把有误差的 Gaia 通量当成精确解释变量，也不会在只最小化残差、协方差随 theta 变化时遗漏似然的 determinant。这里误差协方差由外部给定、固定，Gaia/count 两组独立；若它们相关，替换为完整联合白化。输出 theta、完整参数协方差的 theta 块、观测到 theta 的 influence 和奇异值诊断，可直接送到前述联合误差传播。拒绝未收敛或不辨识；不设置经验星数来代替秩。

这是高 SNR、Gaussian 测光和 Gaia 误差下的可编码 baseline，不冒称低计数 Poisson、高饱和、未知 PSF/SED 条件下的完整物理似然。扩展时使用同一潜变量模型加入 Poisson/read 混合似然，并需保留归一项。生产上可按星分块消去 q，参数块采用稀疏 QR/Schur solve；此参考脚本仅构造小稠密矩阵，单次分解 `O((N_obs+N_star) (N_theta+N_star)^2)`，不以此复杂度宣称全局数亿源可直接稠密处理。

## 5. 保护共同天空的 additive-only baseline

控制模型 `c=H S+A beta+e, C_ctrl>0`。H 必须表示共同真实天体场在各帧的实际响应（含控制尺度的 PSF/采样）；A 是拟合与实际求值完全相同的 offset/plane/四结点 basis。背景控制量用其自身 `T C_y T^T`，不得用 point-source Fisher 替代。

`sky_profile_operator(H,A,C,gauge)`：

1. Cholesky 白化得到 W；对 W H 做 SVD，形成公共天空列空间的正交补 `Q_w`。
2. 由 `gauge beta=0` 取零空间基 N；每个实际有效 overlap 连通分量分别固定规范。
3. 对 `D=Q_w W A N` 做 SVD；若对待估参数不满列秩，返回 `RELATIVE_SKY_NOT_IDENTIFIABLE`，不任意加 ridge。
4. `K=N D^+ Q_w W`，从控制 c 得 beta。图像实际校正按 `P=I-EKT`，完整传播其 influence。

由 `Q_w W H=0` 严格推出 `K H=0`，所以该 H 中任意公共梯度均不影响相对 sky 参数。该结论不证明错误 H 的结构保护。三个平面 offset/slope 的算例规范为各帧系数和为零，设真实系数 `[2,.6,-1,-.2,-1,-.4]`，共同天空含梯度和扩展峰。断连算例展示一个全局规范不足，必须逐分量规范；无绝对锚时公共背景本来不可观测。

实际大型基线采用稀疏联合设计 `[H,A N]` 的 LSQR/LSMR 或稀疏 QR，不构造稠密 `Q_w`，保留公共天空的分块消元、实际支持与参数尺度；一个 LSQR 迭代需要一次设计及转置作用，代价 `O(nnz(H)+nnz(A)+covariance inverse action)`。求解停止以目标函数/线性残差和参数可辨识谱共同判断，弱方向不靠正则化“证明”。小稠密 SVD oracle 已实现；稀疏全图收敛/预条件性能不由该脚本证实。

## 6. 小 N 排异与选择后误差：失败及已定 baseline

固定、外部已知的联合 C、共同均值及 Gaussian 噪声下，留一 GLS 预测 `mu_-k=a_-k^T y_-k`，其中系数和为 1，残差标准差为

```
s_k^2 = C_kk + a_-k^T C_-- a_-k - 2 C_k,- a_-k.
t_k = (y_k-mu_-k)/s_k.
```

每个 t_k 在无污染零假设下边际为标准正态。因此 `z=Phi^-1(1-alpha/(2N))` 和 union bound 给 `P(any |t_k|>z)<=alpha`，不要求不同 t_k 独立。LOO 用联合 C，不能漏共享相关项。若 C 是从同栈残差估计，此标准正态结论不再成立。

**发现并纠正的失败**：把所有 `|t_k|>z` 同时拒绝，会在一个污染帧使其它预测偏移时连带拒绝干净观测。N5、单个 12σ 污染，Gaussian 95% coverage=0.91603；Poisson+read=0.91062。初始 JSON 原样保留，不能删掉反例后称第一版通过。

**正式参考政策**：默认 `N<4` 不作跨帧自动拒绝；`4<=N<=7` 如用户启用并具有外部可信 C，单步最多拒绝一个最大 `|t|`，阈值使用 `alpha=.001` 的双侧 Bonferroni，之后停止，不迭代重用阈值。默认纯均值仍可使用。已知 DQ/二维伪影证据独立处理，时间变化源不自动定义为宇宙线。多污染/未知 C 不在这个小 N 统计保证的适用域；不得从一帧样本识别异常身份。

`accepted_gls` 按实际接受集合使用 C 子块作 GLS。其 `sqrt(a^T C a)` 是冻结掩膜标准差，**不是选择后精确条件误差**。脚本每次从头产生噪声、重新选择并合成，报告实际 68/95% coverage 和 Clopper–Pearson 二项 95% 区间，另报告相同抽样的无选择 coverage。若需要某个真实应用的选择后区间，必须对其整个污染、PSF/响应、掩膜和估计流程前向模拟，不能从本构型抄一个修正因子。

固定 N<=7 时预计算各个 LOO 系数只需 N 个至多 6 阶 solve，`O(N^4)` 的稠密开销是常数；每像元一轮 `O(N^2)`。接受掩膜只有 `2^N` 类，可按协方差组缓存解，不能在每个 MC/像元重复大矩阵逆。大量图像多重比较的全图 FWER 不由逐栈 alpha 保证；此处 alpha 控制单个位置的候选族。

## 7. 实际结果及数值精度范围

命令：`python 实验/science-foundation/oracles/statistics_independent.py`。固定种子 `11120261020`；Python 3.12.2、NumPy 1.26.4、SciPy 1.15.3。每个噪声构型 100000 个独立 trial；Gaia 联合非线性拟合 1000 个独立 trial。JSON 保存脚本 SHA256、参数、分母和全部二项区间。最终运行约 4.3 秒，仅合成数据，无外部文件依赖。

| 检查 | 实测结果 | 能支持的结论 |
|---|---|---|
| 非单位原生面积、4×3 R、共享 latent | 经验协方差相对 Frobenius 误差 0.0089913 | 指定模型与 MC 一致，非普适 0.9% 算法误差界 |
| 因子 action、Woodbury | JSON 中 FP64 直接矩阵对照 | 算子表达与该矩阵等价 |
| 共享噪声 floor | N=100 真方差 .05；误当独立得 .0104 | 共模误差不能随帧数独立平均 |
| 相对 sky 已知系数 | max 误差 9.99e-16 | 指定高条件小矩阵能恢复相对系数 |
| 公共真实梯度 | max 改变 3.55e-15 | 所声明 H 中共同天空被保护 |
| 同数据 sky 单坐标 | 正确方差 .9869281；仅加独立 sky 得1.0653595；MC .9880928 | 必须含交叉项 |
| 同数据 sky 68/95% coverage | .67980 / .94980 | 对比仅加sky的 .69810 / .95791，说明错误误差条过宽 |
| 10%响应差伪装成sky | 假 sky 系数 `[-.6,-.15,.6,.15]` | 响应失配可被加性平面吸收，保护定理前提必要 |
| PSF不匹配、真实sky=0 | 假斜率 `[-.0189296,+.0189296]` | 同一天区不同PSF不自动满足相同H |
| 3nm / 7nm photon q | 693.97311255 / 1621.10833603 photons s^-1 m^-2 | 已声明连续谱/QE/矩形响应下的物理单位结果 |
| 独立解析多项式对quad | 相对误差1.11e-16 / 2.22e-16 | 仅该可解析谱的数值求积精度 |
| 2nm粗格矩形通带 | 偏差 -33.6463% / -14.7072% | 响应端点必须准确分段 |
| 真线sigma=.05nm、低分辨sigma=4nm | 带内线通量偏差 -71.2144% / -38.9240% | 与求积误差完全不同的物理分辨率反例；4nm是合成LSF，不声称为XP实测 |
| 连续谱3→7标量 | 2.33598148，中央线偏差+133.598% | 恒星/连续谱标度不能保证星云窄线无偏 |
| Gaia共享谱联合拟合 | 无噪声参数 max误差6.25e-17；两帧log零点cov=3.33377e-5 | 共用同一Gaia源会形成跨帧参数相关 |
| Gaia颜色恰等于位置的rank负例 | `X=[1,x,color]` 且 `color=x`，15维联合参数Jacobian秩14；fit返回 `GAIA_JOINT_FIT_NOT_IDENTIFIABLE` | 谱先验不能识别完全共线的颜色/空间效应；不可伪造两个有限参数误差 |

N5 的保守单拒绝方案：

| 条件 | 干净栈误拒 | 干净68/95% coverage | 单个12σ污染后68/95% coverage |
|---|---|---|---|
| 异方差Gaussian | .00102 | .67703 / .95003 | .67990 / .95096 |
| Poisson(80…140e⁻)+3e⁻ read | .00108 | .67905 / .94912 | .67955 / .94869 |
| Gaussian+共享方差.7 | .00103 | .67773 / .94999 | 此行未测污染 |

Gaussian N5 误拒的二项95%区间 `[.00083176,.00123808]`；污染后95% coverage区间 `[.9496034,.9522904]`。Poisson N5污染后95% coverage区间见JSON；Gaussian 阈值对Poisson只作测试过的近似，不存在这里证明的 Poisson FWER 定理。N1…3 不自动拒绝，注入污染下 coverage=0 是预期能力边界，不以“通过率”隐藏。N4…7 的单个12σ污染在本次100000次中都被识别；“100%观测检出”不是零漏检概率的数学证明，精确下限见JSON。

Gaia四参数1000次的68% coverage为 `.651,.686,.669,.683`、95%为 `.954,.954,.948,.951`，二项区间均附 JSON。样本量只约束到百分点级，不能声称毫米星等普适覆盖。该非线性模型的协方差为 Gauss–Newton 线性化结果；模型失配和低SNR偏差需另评。

## 8. 源码核查证据与实测范围

- `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:733–736,791–802`：分段节点已存在，实际 `total += weight*fv*qe*lam`，未除 `hc`、未将 nm 波长转米。该值依旧是旧 XPMODEL 惯例，不能按新物理 photons 单位解释。此次不修改它；新接口把单位从数学定义写清。
- `lib/algorithms/coverage/src/rejection.cpp:1144–1160,1250,1265–1280`：当前小N策略N1…3保守none，N4/5仍为percentile，低/高分数.2/.1。独立解析 `[-2,-1,0,1,2]` 只留0，整体+10后留下 `[8,9,10,11]` 得9.5，违反加性平移等变性。Oracle重算此反例，不宣称运行了C++实际拒绝率。
- `lib/algorithms/coverage/src/sky_plane.cpp:1000–1018,3915–3930`：求值真实四角双线性，残差更新只floor-bin一个格点。新 baseline 按实际basis构造设计，不把旧更新当严格同目标最小二乘。

尚需真实数据独立验证：有效gain/读噪/flat共享相关；Gaia XP外定标及缺失谱cov的误差身份；顶帽形状、QE、大气/快光锥蓝移；控制尺度PSF可比性；星云多线SED及真实低频保护；真实污染多发/时变源与选择后coverage。HST处理图可作结构模板，不能充当未公开探测器噪声的真值。本脚本未跑真实HST/M42、未证明生产结果、未验收整条科学链。

## 9. 同估计量 actual-scene 稀疏方差的有条件重建

独立参考：[sparse_variance_reference.py](oracles/sparse_variance_reference.py)，结果：[sparse_variance_reference.json](evidence/sparse_variance_reference.json)。它仅插值同一 estimator、actual-scene condition、单位、response/PSF/采样/掩膜和误差账目的正面亮度方差，不把 reference-source Fisher/SNR 插值改名为场景方差。

在显式局部球面 gnomonic chart，令 `x=(n·east,n·north)/(n·center)`，并限定chart半径。其坐标为无量纲TAN平面量，不等于精确球面角度。定义 `u(x)=log[V(x)/V0]`，V0携带同方差单位；不能对有量纲V直接取log而不声明参考尺度。控点观测为 `u_i`，外部已知 `|u_i-u_true(x_i)|<=delta_i`。另需外部确立整个受支持三角域上 `||Hessian_chart(u_true)||_2<=M`。稀疏控制残差小不能自身证明这个全域曲率界。

`SparseSceneVariance(directions, variances, log_error_bounds, chart, estimator_id, condition_id, variance_unit, variance_unit_scale, curvature_bound, condition='actual_scene')` 构造Delaunay三角；`query(direction)` 只接受其convex支持内的三角。重心系数 `lambda_i>=0, sum lambda_i=1, sum lambda_i x_i=x`，故

```
uhat = sum_i lambda_i u_i
Delta = sum_i lambda_i delta_i + (M/2) sum_i lambda_i ||x_i-x||^2
V_true(x) in V0 * exp([uhat-Delta, uhat+Delta]).
```

证明：对每个顶点在x作Taylor展开；重心一次再现使线性项之和严格为零，Hessian范数界控制每个二阶余项，再加控点绝对误差界。正权使控点误差界与方差保持性明确。此区间是**给定外部delta和M的确定性区间**，不是自动由有限控点得到的68/95%统计区间。若delta来自统计置信区间，须给全部相关控点的同时覆盖事件及其概率，不能简单把多个边际95%当联合95%。

缺M时可给具名插值值但区间为unavailable；图外返回unavailable，绝不nearest-fill。真实掩膜洞、条件变化或三角内部未知误差区域必须切断支持，不能只用convex hull填洞。代码为固定同条件的无洞合成域。边界若出现负数值重心权返回 `numerical_boundary_ambiguity`，不裁负权来伪造一次再现。

25控点、5000查询、固定seed `11120261021`：已知log平面无噪声再现最大误差 `1.6098e-15`；加入各控点已知有界误差后，平面和已知二次log场各5000次均无区间违例（比较含FP64绝对slack `1e-13`）。二次场使用显式矩阵 `[[240,70],[70,-140]]` 的谱范数作为M；区间误差/界最大比分别 `.9486221`、`.9596158`。这些是浮点参考证据，不是严格区间浮点舍入证书。

无先验不可辨识反例：四角控点 `u_i=0`、`V_i=V0>0`，查询原点。半径`.005`内定义 `u_A(x)=A exp(1-1/(1-r^2))`，域外为0，`r=||x||/.005`。这是C∞紧支撑bump，所有控点永远为0，但查询点 `u_A(0)=A`，`V(0)=V0 exp(A)`；任意有限建议上界都可选更大A违反。所以仅“光滑”加稀疏精确控点不能产生统一有限方差上界。JSON列A=1/10/100的实例及图外状态；这不意味着零控点方差合法，零的是log值。

平面Delaunay预处理期望 `O(n log n)`、存储 `O(n)`；点定位使用三角索引的通常开销近 `O(log n)`，最坏情形不作此保证；找到三角后的数值与误差界均O(1)。亿级场景采用局部chart分块、控点halo与明确域策略；不同chart中的M不能不经Jacobian/Hessian变换直接共用。此参考未测亿级全图性能，且不能绕开天光同数据拟合和共享校准的非局部协方差；它只重建该同估计量的边际V，不制造跨像元/跨帧相关项。
