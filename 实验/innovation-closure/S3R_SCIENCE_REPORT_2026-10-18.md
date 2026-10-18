# ACSD S3-R｜P5 数学 Oracle 纠偏：contrast 协方差、真实 QR/SVD、缺失节点零空间、色散 PSF

- **研究性质**：独立理论/NumPy–SciPy 数值研究；`THEORY_PASS / TOY_PASS / PRODUCTION_NOT_RUN / REAL_E2_NOT_RUN / USER_VISUAL_PENDING`。不修改生产 C++，不关闭 Issue。
- **开工 `main`**：`476c023d395b224518aa1831f730d64822250289`；上一 S3 研究提交 `d3a056be23c6dc2f1c5b74b356ac08c7fb398494`。此处是 S3-R 的新研究版，不把前一版误称为已修复或覆盖其旧机读证据。
- **主任务**：#76；关联 #80、#83、#14、#17。已有真实 M42 E0 取证属于**历史可复核证据**，不是本轮新运行。
- **可复现**：与未修改的原 S3 脚本及 JSON 同目录，执行 `python3 s3r_oracle.py --output s3r_results.json`，旧/新场景采用相同 seed、相同数据、相同条件设计，只改变星点 contrast likelihood 的白化尺度。
- **研究阅读**：`AGENTS.md`、#76/#80/#83 最近 2026-10-18 审稿评论、`S3_SCIENCE_REPORT_2026-10-17.md`、`S3_E0_E3_EXPERIMENT_PROTOCOL_2026-10-17.md`、`code/s3_oracle.py`、`code/s3_results.json`、`lib/algorithms/coverage/src/sky_plane.cpp` 的 `gauge_q / free_of_full / add_stencil / sky_own_eval / sky_make_residual_fields / deflation`、`docs/science/sky/UPM.md`。科学正本仍由文档端整合；本报告是研究证据，不修改 `docs/**`。

## 0. 纠偏前后的科学证据口径

### 上一批 S3 仍然成立的部分

- `g=exp(eta)>0` 且 P1 已应用后剩余乘性只在独立星点锚支持下可估；共模/相对差模要先解决 gauge/识别性；未知色散、未标定跨通带绝不自动共用一个标量真实场。
- 完整 7×6 的 `DᵀD` nullity=4、#83 floor-bin vs bilinear 反例 `SSE=1` vs `SSE≈0`、当前 Q-only deflation 的通用代数反例成立；它们本身不是生产 fallback 或真实 HiPS/E2 的执行证据。
- `g=1` star-only 500 次 MC 的 2/500 次激活与 Clopper–Pearson 区间约 `[0.0004848,0.0143741]` 是针对旧/新**不变的逐帧中心化 z** 判据。此比例不能作为真实不同 PSF/颜色/测量误差下的统一误开启保证。
- 原 `s3_oracle.py` 的输出是历史记录；其中变量 `QR_ls_objective` / `relative_residual_augmented_QR` 由 `np.linalg.lstsq` 产生，**旧记录不得称为实际 QR**；`joint_world` 的 contrast 白化权重是有缺陷的旧统计口径。

本轮的新脚本以 `s3r_results.json` 为具名权威的修正对照，原始脚本/JSON 保留。机器 JSON 保存原 JSON 的 SHA-256、研究 commit、输入设计矩阵 SHA、分解 driver 和显式状态。

## 1. 恒星测光 nuisance、contrast 与统计权重

### 1.1 正确的方差计算

对 K 帧与 N 颗恒星，在正确扣除孔径/PSF/色项并已定义相同参考光度后，假设每颗星测量的 log flux 满足

`z_ks = u_s + eta_k + e_ks`，

其中各 `e_ks` 独立、同方差 `sigma_star²`，`u_s` 为该星 nuisance。令

`M=I_K−(1/K)11ᵀ`，`hat_eta=(1/N) Σ_s M z_:s`，

给定归一规范 `1ᵀeta=0` 则 `E hat_eta=eta`，协方差是

`Sigma_eta = sigma_star²/N · M`。

逐帧中心化坐标的标准差

`sd_frame_centered=sigma_star sqrt((K−1)/(K N))`。

若 `C` 是 K×(K−1) 的**列正交 contrast**，满足 `CᵀC=I`、`Cᵀ1=0`，则

`Cov(Cᵀhat_eta)=CᵀSigma_eta C=sigma_star²/N · I_(K−1)`，

因此 `sd_orthonormal_contrast=sigma_star/sqrt(N)`。

本轮 K=3、N=36、sigma=.012：`sd_frame_centered=0.0016329931618554521`，`sd_orthonormal_contrast=0.0020000000000000`，旧联合残差星点 precision `W_old/W_true=(.002/.001633)²=1.5`。旧 `max|hat_eta_k|/sd_frame_centered > 3.5` 激活判据是逐帧量，**保持完全不变**。

注意零空间协方差 `Sigma_eta` 在全 K 维是奇异的，但在 C 列空间是 SPD；不可误用未约束全帧 inverse，亦不可假定所有 K 个中心化坐标互不相关。

### 1.2 同随机输入旧/新四场对照（仅单次 synthetic toy）

下表 `gain` 使用最大绝对 log-eta 误差，图像 RMSE 均同 S3 预设；`χ²_good` 指**正确 contrast covariance** 下同一个联合目标 `χ²_image+χ²_star_correct`，因此可以公平比较旧/新解。

| Toy 世界 | gain 误差旧→新 | 共同场 RMSE旧→新 | 加性场RMSE旧→新 | 图像χ²旧→新 | χ²_good旧→新 |
|---|---:|---:|---:|---:|---:|
| 纯共模 | 0→0 | .0121941→.0121941 | .0027858→.0027858 | 36.807506→36.807506 | 38.848802→38.848802 |
| 纯加性 | 0→0 | .0222605→.0222605 | .0068929→.0068929 | 32.158397→32.158397 | 32.320225→32.320225 |
| 纯乘性 | .0009843→.0011773 | .0152230→.0152266 | .0070556→.0074865 | 45.175506→45.059577 | 45.261919→45.242298 |
| 乘加联合 | .0014319→.0013095 | .0128709→.0128795 | .0111317→.0104295 | 46.463212→46.358584 | 46.542519→46.524771 |

新联合目标较旧解在真实含 gain 的两组分别减少 .0196215 和 .0177481。**一致的似然目标改善不意味着真值误差对每个随机 realization 都减小**；尤其共模和加性 RMSE 与真实增益 η 最大误差不是同一统计量。只有覆盖率及 Monte Carlo ensemble 才可能支撑更一般统计性能。

机器结果同时包含每场旧/新 `eta_est`、`contrast_params`、correct/used star χ²、训练 image χ²、star likelihood、估计误差、随机种子与预测数据哈希。旧试验数据由原 S3 文件结果复核（脚本按数值容差 assert），新旧唯一关键变动为 `star residual / .002`。

### 1.3 Monte Carlo 协方差与不少于三个反例

此次 **24,000 次**独立重复，用解析 `CᵀΣC` 检查实测协方差。包括五种相互独立的检验构型：

1. 同方差独立恒星：`Sigma_contrast=(.012²/36)I₂`，MC 与公式数值一致；`Sigma_frame=(.012²/36)M`，故单坐标 `.001633`。
2. 三帧不同精度：sigma `[.008,.012,.018]`，`Sigma_contrast=Cᵀ diag(sigma_k²/N) C`，通常含非零 off-diagonal；不得仅用一个统一的 .002。
3. 同一恒星不同帧相关，`Sigma_frame_measure=σ²[(1−ρ)I+ρ11ᵀ]`，ρ=.4；正交 contrast 协方差为 `σ²(1−ρ)/N · I₂`；若错误地忽略共享项，会报告错误不确定度。
4. 每帧共享全部恒星的零点随机项 `b_k`，sigma_zero=.005：`Sigma_contrast=(σ²/N+sigma_zero²)I₂`；零点噪声不随 N 平均消失。
5. 每颗星在所有帧共用的标量误差 `b_s`：理想纯 common mode 被 `Cᵀ` 消除，但若真实颜色/PSF偏差不是真正纯 common mode，则不能声称抵消。实验用该纯同帧共同零点做阳性对照。

这些 toy 的解析与 MC 协方差对照以**可复核容差** assert；其余真实星源误差分布、关联观测网络、Gaia XP calibration covariance 并未验证。

**生产施工合同**：从每星测量向量及完整 covariance `V_s` 装配 `z_s=u_s1+E_sη+noise`；用正确 nuisance profile / generalized GLS 得到相对 η 的估计量和 covariance。独立的星观测可累加 Fisher 信息，关联星与共享标定须使用联合协方差或含 nuisance 的联合建模。联合目标的星点项应是 `r_Cᵀ Σ_C^{-1} r_C`（数值实现 Cholesky/QR 白化），而非任意 `(r_C/.001633)²`。增益选择检验另使用其已明示相关性、家族错误率和同一留出星统计，不能改正白化后即推断假增益控制有效。

### 1.4 参数不确定度与统计覆盖率

在参数块 `[S,a,eta]` 的正确线性化/有效秩条件下，局部条件协方差来自相同白化 Jacobian `JᵀΣ^{-1}J` 的逆或 identifiable-subspace Moore–Penrose；不同 nuisance/profile 层需区分条件与边际。错误把 contrast variance 缩小2/3会错误放大星点 Hessian 的信息并**典型地缩小**相关 η 参数的形式误差条，但最终 joint estimator 的偏差方向、像元边际协方差和 coverage 不能单凭这个倍数确定。正确 covariance 下还要传播 `JθΣθJθᵀ`、样本/标定交叉项和共享增益造成的跨帧相关，且估计/模型选择的不确定性需要独立验证。

**未运行** 68%/95% 的 scene-wide empirical coverage；不可将单次 RMSE 当覆盖率。

## 2. QR / SVD / NumPy / Woodbury：解同一个原始问题

S3 原始 `np.linalg.lstsq` 标为实际 `numpy.linalg.lstsq`，不是明确调用的 QR。本轮增加真正的：

- 列主元 QR：`scipy.linalg.qr(A,mode='economic',pivoting=True)`，以 `A[:,p]=QR` 和 `solve_triangular` 构造完整列秩解；记录对角 R 的值、相对阈值、pivot、rank。秩亏则报告 `RANK_DEFICIENT_NO_UNIQUE_QR`，不伪造唯一解。
- SVD：`scipy.linalg.lstsq(A,b,lapack_driver='gelsd',cond=1e−12)`，保存 singular spectrum 和 effective rank。参见 [SciPy gelsd 参数说明](https://docs.scipy.org/doc/scipy/reference/generated/scipy.linalg.lstsq.html)。
- 原 NumPy `np.linalg.lstsq` 留作历史路径，未实际进行显式 QR，不借 SVD 具体 LAPACK 驱动身份替代 API 自身名字。
- Woodbury：在 α 临时锚的修正中，`K=(A+αQQᵀ)^{-1}Q`，`u=(I−αQᵀK)^{-1}(αQᵀB_a)`，`B=B_a+Ku`；有效前提是相关被逆矩阵及小系统在当前精度与 gauge 下可逆，病态仍须判 `NUMERIC_RANK_UNCERTAIN`。

测试完整 7×6、n=42 `D` 的初始 S3 随机样本；构造 `H=XᵀX/n+.3I`, `A=H+.5DᵀD`, `b=H x_truth`；增广同一惩罚问题：

`min_θ ||H^(1/2)θ−H^(−1/2)b||²+.5||Dθ||²`

（H 的上 Cholesky `sqrtH`，其目标由 `sqrtHᵀ y=b` 定义），并与直接解 Aθ=b 比较。不以惩罚数据残差要求零作为原本含噪/含惩罚的最小二乘通过条件。

| 方法 | 原 Aθ=b 相对残差 | 同一增广目标 residual norm | `||Dθ||²` |
|---|---:|---:|---:|
| 直接法 | 3.65e−16 | 3.989292 | 7.284287 |
| **旧 Q-only** | **0.123484** | 4.011306 | 7.155380 |
| 正确 Woodbury | 5.56e−16 | 3.989292 | 7.284287 |
| **SciPy actual pivoted QR** | **8.57e−16** | 3.989292 | 7.284287 |
| **SciPy gelsd SVD** | **2.00e−15** | 3.989292 | 7.284287 |
| NumPy 历史 lstsq | 2.00e−15 | 3.989292 | 7.284287 |

同一改正 QR/SVD 还在 #83 两点 x=.25/.75 的**两自由度 x-affine 子空间**真实运行，正确 bilinear 可表示的 SSE 为浮点零，历史 floor-bin SSE=1。该反例只证明可行子空间，不是生产全四角 field＋真实相关 covariance 的执行记录。

**解的可辨识性和 QR/Cholesky 失败语义不能靠设小阈值掩盖**；toy `rtol=1e−12` 是明确的实验约定，生产需以合法单位/列平衡、矩阵后向误差、实际 singular gap 和物理 support 判别 rank。本次良态矩阵下真正运行 QR/SVD，**没有**证明极端跨度 \(10^{32}\) 的真实 #80 矩阵已能可靠求解。

## 3. 缺失节点后的实际二维 D2：不能无条件假设四维

现源码 `sky_plane.cpp` 已在约 L1784–1832 构造四个候选零空间列 `1,ix,iy,ix*iy`，且 `free_of_full` 先删除低 support 节点；后续 `add_stencil()` 对包含任一 inactive 节点的三点 stencil 整体跳过。因此有限制的数学对象是**当次 active mask 诱导的 D**，不是完整矩形 D。

本轮逐一用严格相同方向的三点 `[1,−2,1]` 构造 D，只有三个节点都在 mask 内才保留一行；以显式 SVD 数值 rank 求 nullity，四项多项式另直接按真实 active node 坐标求列秩。两者不是同一个数。

| mask | nfree | rank(D) | nullity(D) | 四多项式可解释维数 |
|---|---:|---:|---:|---:|
| 完整7×6 | 42 | 38 | **4** | 4 |
| 7×3 中央列 x=3 缺失，两边各3×3 | 18 | 10 | **8** | 4 |
| 完整7×6仅缺 (3,3) | 41 | 37 | **4** | 4 |
| 7×6整行 y=3 不存在 | 35 | 27 | **8** | 4 |

两孤岛案例是两个彼此独立的 D2 子问题，各岛的 3×3 D rank5/nullity4，所以合计 rank10/nullity8。全局 `span{1,x,y,xy}` 只能张成四维子空间，尚有四个独立的**未惩罚模式**，脚本用真实 `null(D)` 与多项式投影对照得到 Frobenius 正交剩余 = 2.0。

**科学限定**：`ker(D)` 的维数大于4，不等于完整 `H_red+λDᵀD` 必然秩亏，也不等于天空物理规范自由度增加相同数量；观测矩阵 H 可能仍强约束这些方向。问题是当前固定四列 `gauge_q` 不再等于**全部惩罚零空间**，因此 fallback/零空间 deflation 的依据不足。应按当次 mask、D、规范、观测秩分别报告：
- nfull/nfree、support_floor、active indices/hash、D 的 stencil 集与 SHA、`rankD`、`nullityD`、所有小 singular values、`||D Q_true||`、四多项式的实际 rank 与漏失维数；
- `rank(Q_trueᵀ H Q_true)` 和交叉 `||(I−QQᵀ)H Q||`，区分纯数值极小曲率、规范零模式和模型本身不可辨识；
- 在可审计的 reduced problem 上比较真实正则化偏差、临时锚偏差、正确 Woodbury / 投影 QR/SVD、原方程残差与预测差。
- 对大问题需经适当列尺度预条件的稀疏 rank-revealing / 连通分量推理，不允许将全局 `mq=4` 等同实际 nullity。直接用任意 ridge 绕过 rank 不能证明可辨识性。

上述所有 masked 问题只运行 Python 原型，**未触发 C++ fallback**；#80 真实 M42 冻结输入中的 support mask 未从本轮读取/实测，因此未认定现实恰有8维。

## 4. 真实宽带、色散 PSF 的光谱前向模型修正

若源谱辐亮度随位置变化、仪器响应 `R_k(λ,ω)` 随波长与位置变化，而且有效 PSF/像元/空间核随 λ 变化，则不能无条件把一帧写成 `H_k(∫L_λ R_k dλ)`。正确的探测像元均值（已明确 energy/photon conventions）是

`μ_ki = g_ki ∫_λ [H_{k,λ} ( R_k(λ,·) L_λ(·) )]_i dλ + a_ki`

或将 PSF、WCS、footprint、WCS色散等统一写为真实联合响应核

`μ_ki = g_ki ∫_λ∫_Ω K_ki(ω,λ) R_k(λ,ω) L_λ(ω) dΩ dλ + a_ki`。

如果 `H` 包含算法重采样/Drizzle，则它指**指定数据处理条件下该成品的有效线性前向算子**，其 noise 仍需沿真实算子和估计 nuisance 传播。背景减法/拒绝样本如果取决于 y，是条件算子或者须由端到端非线性 Monte Carlo 验证。

### 两波长、两位置严格反例（Toy PASS）

设谱分量场 `L_1=[1,0]`、`L_2=[0,1]`，响应相同且为1，第一波长 PSF=`I`，第二为 `[[.75,.25],[.25,.75]]`。

**正确顺序** `H1 L1 + H2 L2 = [1.25,.75]`。错误地先积分为 `[1,1]` 再使用平均 PSF `(H1+H2)/2` 给 `[1,1]`，逐像元差 `[-.25,+.25]`，最大绝对偏差 **0.25**（抽象线性单位）。不是现实滤镜的实测值，仅证明非交换性。

如果**整个位置范围内确有相同的已知参考谱型** `L_λ(ω)=φ(λ)S(ω)`，则可定义 `H_eff=∫φ(λ)H_{k,λ}R_k(λ,·)dλ`（若空间响应变化，它本身是位置相关算子），在该一维谱子空间内化为标量场问题；并非允许一个与所有源 SED 无关的固定宽带 PSF。

该判据与跨不同滤镜 #14 的独立可比较性判据并列成立；即使通带积分的数值色项可比较，**星点 SED 和 Hα/[N II] 星云 SED 不同也可能对应不同有效色散 PSF**。参考 [Boucaud 等跨波长 PSF kernel 思路，A&A 2016/2017](https://doi.org/10.1051/0004-6361/201629080)、[Eriksen & Hoekstra, 2018, wavelength-dependent PSF](https://arxiv.org/abs/1707.04334)、[Fruchter & Hook, 2002, Drizzle](https://doi.org/10.1086/338393)。

## 5. E0–E3 修订实验协议与实施接口

### E0：本轮实际完成

- 解析两种 SD，MC 24,000 次估计 covariance，并覆盖不同帧 σ、同一恒星跨帧相关、帧级共享零点、全帧共同零点。
- 旧/新完全配对四场一轮拟合；保存正确统计目标/估计量误差/同一 seed；500 次零 gain 选择判据未变；**不是** 500 组联合 fit 或 68/95% coverage。
- 真正执行 SciPy pivoted QR 和 `gelsd` SVD，记录与原方程相对残差；旧 `np.linalg.lstsq` 指出真实函数名；Woodbury 另列。
- mask-induced 7×6与7×3节点 D2、rank、null-space 差异，并给有/无孔洞对照；两波长/两 PSF 负例与可分离正例。

### E1：下一轮正式合成，尚未执行

1. 使用用户提供 HST 图像时只取其**有确认掩膜的结构模板**，合成至少两种独立光谱分量：恒星连续谱、Hα、[N II]、不同 extinction/暗云；至少两种 λ 依赖 PSF 形态以及实际大气色散/滤镜响应，另与无色散同 PSF 的基准配对。
2. 在相同合成帧上冻结 `R_{k,λ}`, `PSF_{k,λ}(ω)`, detector/Drizzle 和 masks；运行模型 `H(∫LR)`（有意简化负对照）与 `∫H_λ(LR)`（正确臂），以 held-out extended source、star EE、line ratio、低频 transfer、seam 假增益与真实方差覆盖对照。
3. 对 star noise 使用独立/heteroscedastic/shared-zero-point/correlated四组；保存 proper likelihood 参数误差条、empirical 68%/95% interval coverage；冻结 >=2000 seeds 做 gain=false activation 统计，并按每场/星集的独立 trial 构造区间，不能把像元或同帧恒星当独立 replicate。
4. 对 masked D2 在真实样本支持的稀疏节点上执行元数值+产品预测 A/B，留出区域检查局部背景场、弱低频真实信号和 penalty bias。只有代码级真实 fallback 注入与原系统 Aθ=b 残差可标 `PRODUCTION_BRANCH_TESTED`。

### E2：真实 FITS / Hα+Red / 最新生产，尚未执行

- 先用同滤镜 Hα、同滤镜 Red 构建共同场正对照；原 #80 2×Hα+1×Red 三帧必须以**真实当次响应/SED 和正确 P1 application/ivar** 区分 `mixed_numeric_only`、`separate_response_fields`、有限 SED basis 三种产品；旧把 Red photscal 乘给 Hα 的 overlay 只能作数值敏感性，不可作为实测光度校正。
- 复跑真实 Stage1→Stage2→Stage3 + HiPS/FITS 与外部 reader，保存真实光子/读噪/背景控制方差单位和跨帧 correlations；用户本人统一 stretch 目检 E2 全图、暗云、弱 Hα、星点光晕及接缝；没有本人确认不得声称 E2可接受。

### E3：留出与验收，尚未执行

- 固定场合格恒星、空间 holdout、设备与色项区域；比较旧/新星点协方差与正确 multivariate GLS 的 68%/95% coverage、PSF-dependent gain false activation、低频星云通量/线比、签名 signed seam、point-source photometry、`C_model` 对 `MC covariance`、output correlation、真实 shot floor。
- 对 `unknown / model_incomparable / numeric_rank_uncertain / outside_support` 分别返回，并统计失败分母；不把科学不可比较当成数值优化失败或 CLI exit0 当作科学通过。

### 给执行端的函数/数据合同（研究建议，非已实施）

- `build_star_contrast_likelihood(measurements, covariance, nuisance, contrast, support)`：返回 `contrast_observation`, `contrast_covariance`, `whitening_operator`, `rank`, `id`, `assumptions`；将 `sd_frame_centered`（选择统计量）与 `sd_orthonormal_contrast`（toy GLS；实际可为full C）明确分离。
- `build_active_d2_operator(nx,ny,free_of_full,stencil_policy)`：输出含 active index、stencil、`D_hash`、`nullspace_rank/status`；`detect_penalty_nullspace` 根据实际支持计算 Q_true 而非默用固定四个方向。
- `solve_p5_linear_reference(A,b,C,D,gauge,linear_unit)`：在合法 identifiable subspace 内提供 `actual_solver_driver`, `singular_spectrum`, `parameter_covariance/status`, `objective_before_after`, `original_equation_residual`, `data_residual`, `penalty_value`, `prediction`; QR/SVD 对拍。只为真正有依据的 penalty 解添加 λ。
- `forward_chromatic_scene(spectral_basis,response_curves,psf_lambda,wcs,pixel_footprint,drizzle,noise)`：生成每帧声明完整谱响应下的 forward operator，记录 photon/energy unit、有效 λ 支持及 spatial support；对光谱分量无法辨识时返回混合响应/限制，而不伪称同 band 真值。
- 当前 production `sky_plane.cpp::p2_sky_plane_build_joint` / `sky_make_residual_fields`、Schur 回退、`sky_own_eval` 与阶段2读/写合同仍由执行端另单排期，不在本批改生产。

## 6. 状态、未决与复审

| 事项 | 本轮状态 |
|---|---|
| 正交 contrast covariance 解析/MC、修正旧新四场 | **THEORY_PASS / TOY_PASS** |
| 原旧 500 star-only gain 激活无误改 | **TOY_PASS (2/500不变)** |
| 异方差、关联 star、共享零点 | **TOY_PASS (构型有条件)** |
| 实际 pivoted QR / `gelsd` SVD、Woodbury 对拍 | **TOY_PASS** |
| 缺失节点 D2 两孤岛、孔洞、断行 nullspace | **TOY_PASS** |
| 色散 PSF 的严格前向次序及反例 | **THEORY_PASS / TOY_PASS** |
| 真实 #80 矩阵支持域/生产 fallback 与精度模型 | **PRODUCTION_NOT_RUN** |
| HST 合成 E1、M42真实 E2、E3 coverage | **REAL_E2_NOT_RUN / NOT_RUN** |
| 用户 E2 目检与 P5 科学关闭 | **USER_VISUAL_PENDING / OPEN** |

**重要**：该科研纠偏不是通用 A/G 生产联合估计已可使用的证明。S3 的旧 JSON 只作为历史数值留存；本文 S3-R 与 `s3r_results.json` 为纠偏后的 toy 证据。停止于独立复核节点。
