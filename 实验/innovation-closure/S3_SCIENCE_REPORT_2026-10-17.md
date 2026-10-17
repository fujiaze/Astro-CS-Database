# ACSD S3｜P5 共模、差模、残余乘性与统计保真：独立科学与数值研究

**研究基线**：`main@6f1410e1084c87265e59d2d9a7bc109405c9d714`（2026-10-17，fetch 后已核对）。

**研究性质**：THEORY_PASS（适用条件内的数学）、TOY_PASS（独立 NumPy/SciPy 数值世界），**PRODUCTION_NOT_RUN、REAL_E2_NOT_RUN、USER_VISUAL_PENDING**。没有修改正式 `docs/**` 或生产 C++；不关闭 #76/#83/#80。已有 #80 E0 的真实输入/输出数字来自仓库原先的冻结取证，本研究**没有再次运行**真实 M42/FITS。`main` 并非本研究成果之前的一个“科学已验收”状态。独立 Oracle 命令：

```sh
python3 实验/innovation-closure/code/s3_oracle.py --output 实验/innovation-closure/code/s3_results.json
```

对照阅读：`AGENTS.md`；S1/S2 研究报告与对应 Python Oracle；Issue #76/#83/#80 及最近的独立复审、`#80` E0 `REPORT.md`、`matrix_oracle.json`、`diag_normal.json`；`docs/science/sky/UPM.md`、`docs/science/unified/UNIFIED_SCIENCE_MODEL.md`、`docs/science/photometry/PHOTOMETRY.md`、`docs/science/noise_snr/NOISE_SNR.md`；`lib/algorithms/coverage/src/sky_plane.cpp::sky_own_eval / sky_make_residual_fields / p2_sky_plane_build_joint` 和 Schur/penalty/deflation 段。依据生产源码**静态识别**，不将其注释视为求解正确性证据。

## 1. 正确的科学对象与完整前向模型

对真实天区和波长相关的谱辐亮度 `L_λ(ω)`，第 k 帧像元/控制观测应写为

`μ_ki = g_ki [H_k(∫ L_λ(ω) R_k(λ,ω) dλ)]_i + a_ki`；`y_ki=μ_ki+ε_ki`。

响应 `R_k` 必须明确光子/能量约定、曝光、QE、大气、滤镜、通量尺度和有效谱域；`H_k` 明确场景 PSF（可能随 λ 和 ω 变化）、WCS、像元积分/Drizzle、真实有效支持。`a_k` 是与原物理天区独立可辨的相对加性参数场；不能预先宣称它包含所有绝对天空亮度。`g_k=exp(Φ_k η_k)>0` 只允许表示 **P1 applied 后剩余**、经留出恒星证据支持的乘性偏差，不可重复 P1 标度。`ε` 在校准条件下包括源+天空的 photon shot、读噪、master/cosmetic、Drizzle 相关及拟合参数交叉协方差。完整输出的边际协方差必须从共同随机输入雅可比传播，而不是简单叠加每帧 ivar。

### 1.1 三类可解释输出

1. **同一物理通带共同场**：若经可审计标度后，各帧响应在被允许 SED 子空间上一致，且 `H_k` 对相同目标空间泛函有可执行映射，则 `s(p)` 可作具名响应下的公共被估计量，PSF/空间核仍要保留。
2. **具名混合响应场**：在冻结权重、固定选择和光度标度下，`R_eff(λ,p)=Σ_k a_k(p)c_k R_k(λ)`；PSF/Drizzle 不同时更一般是位置、天区、波长三元核 `K_eff(p,ω,λ)`。混合成像可产生一幅科学信号图，但不等于无偏的某个固定参考滤镜。数据驱动 rejection/fit 时 `R_eff` 是条件于冻结决定的算子，不自动描述非线性端到端均值。
3. **无法证明共同场**：Hα 与 Red 未有 SED/响应映射时，应以不同光谱组场 `s_H,s_R` 或有限维光谱系数 `s_m` 的联合 forward model 研究；若该模型不可辨，只能报告具名 mixed-response numeric product、与外部锚缺失相关的物理解释限制，不可用 P5 加性面吸收真实谱差。不得禁止用户合成，也不得将观测值的可数值加权偷换成共同物理 `S` 的已证无偏性。

限定谱子空间 `𝒮` 时，一个标度 `c_k` 映到 `R0` 的必要充分条件是对全部 `f∈𝒮`，`∫f(c_kR_k−R0)=0`。如果 `𝒮` 是任意谱型，须 `c_kR_k=R0` 几乎处处；只在指定恒星 SED 上匹配并不能推出 Hα/[N II] 星云一致。独立实测仪器曲线、Gaia XP 的有效波段/外定标和颜色/线谱误差分别对待。最终 Stage2 仍只有一个 signal 图像通道；响应空间分布模型及支持域作为 provenance/质量统计保留。

## 2. P5 共模与加性、乘性不可辨识性

在上述共同估计量确实成立、冻结样本和 residual gain 后，堆叠模型

`y=G(η) H s + A α+ε`, `Σ=Cov(ε)≻0`。

`A` **必须由最终实际求值基**构造（如 #83 的 bilinear 节点基）；需要按重叠图的真实连通分量设置规范。固定 `η`，白化 `W^{1/2}=L^{-1}`，令 `F=L^{-1}G H`、`D=L^{-1}A Z`、`b=L^{-1}y`（`Z` 是真实规范的自由变量表示），求

`min_{s,β} ‖b−Fs−Dβ‖² + λ_a‖L_a Zβ‖²`。

若公共场没有正则且在被保留的观测子空间可辨，令 `Q_F=orth(F)`，投影消元后求 `min_β‖(I−Q_F Q_Fᵀ)(b−Dβ)‖²`，再回代 `s`。这是 **保护给定 H 描述的公共场**、提取可辨相对差模的 GLS 方法，并不是任意“所有天空”可被物理扣除。正交投影/QＲ/SVD 可以避免显式 `DᵀD−DᵀF(FᵀF)^{-1}FᵀD` 的灾难性消减。`Σ` 需包含交叉参数贡献；只有相同被估计量且给定资格后才能称权为目标逆方差。

**必须声明的 gauge**：对相同 `H≈I`，任意允许的公共函数 `u(p)` 可以令 `s'=s+u, a'_k=a_k−g_k u`，预测不变；若 `g` 未锚定，`s'=c s,g'_k=g_k/c` 也不变。由未知绝对天空加性常数与低频暗云/弥散发射连续场形成的退化，不能通过 “天空越平滑越好” 物理消除。无支持的空洞、断连组、没有有效星点或不同通带不满足共同 SED 时，必须报告相应不可辨识的量及其 gauge 或限制。

**残余增益**：用 `η_k(x)=Φ_k(x)γ_k`，`g=exp η`；对每个测量的增益 Jacobian `J_η = diag(g·H s) Φ`，`J_s=G H`，`J_a=A`。实际可辨条件是将加性/共模列空间 profile out 后 `rank((I−Π_{[J_s,J_a]})J_η)` 在锚约束下充分；独立、未饱和的星点积分相对光度增加额外行，须针对 aperture EE、PSF、颜色、SED、背景、共模校准参数正确建模。星点只能确定被它们支持的增益模式：星全部集中于 `x=0` 时不能求任意空间坡度。这是设计矩阵秩问题，而不是“样本数够多”可以绕过的。若没有外部星点证据，锁定 `η=0` 是保守的研究对照，不表示真实物理增益必定为 1。

## 3. #83：floor-bin 的训练与 bilinear 的输出估计量不一致

生产 `sky_make_residual_fields` 使用 `floor((u−u_min)/step)` 将观测归一个节点做 `Σwr/Σw`，在节点减跨帧公共加权值并填空洞；`sky_own_eval` 却以 `(1−tu)(1−tv),tu(1−tv),(1−tu)tv,tu tv` 四节点双线性基求值。这样 floor-bin 更新解的是**不同设计矩阵**的统计问题。

解析两帧同区域正负 residual：`x={0.25,0.75}, y=0.25`，相对真差模 A=`{+0.5,+1.5}`、B=`{−0.5,−1.5}`。缺格外推常数下，floor-bin A=`{1,1}`、B=`{−1,−1}`，四项平方误差 **1**；用相同 bilinear 基取 A 的节点 `(0,2,0,2)`、B 的相反数，输出恰匹配，平方误差 **0**。独立 NumPy LS 验证，KKT 梯度近零。它证明 **“当前 F-step 是精确最小二乘”假命题**，不声称真实 E2 误差恰为 1。

### 3.1 可生产实现的 F-step

给定相同执行资格和固定基础 `B_ref+δ_poly`，对位置 t 构造最终 `sky_own_eval` 真实四节点非零行 `B_t`；组装 `r_kt=y_kt−[H_s]_{kt}−δ_k(t)`。引入帧/连通分量 contrast 参数化 `f=Z_f ζ`；如公共 residual `c_t` 自由，则将其显式拼入 `H` profile out，不能先减掉同数值 bin 共模再声称与 sample-wise bilinear 共同变量联合 LS 同义。固定权、冻结 mask 时求

`min_ζ ‖L^{-1}(r−B Z_f ζ)‖² + λ_f ‖L_f Z_f ζ‖²`。

若 λ 是物理/统计先验须在预先设定条件下报告估计偏差；若不加正则且设计不可辨识，返回 rank/status，不为了值可算而伪造缺格外推。使用矩阵自由 `B@vector` 和 `Bᵀ@vector`、spQR/LSQR/LSMR 或独立小模型 QR/SVD reference。每一次接受的 update 均记录**同一**冻结目标的 `L_before/L_after`、梯度/后向残差、正交误差、支持域、有效秩、步长；需保持 `L_after≤L_before+numerical_tolerance`。若自适应网格 `h`、支持掩膜、权重、rejection、基础 `H` 改变，属于**另一个目标**，不能把两轮不同目标值相减声称块坐标优化必然单调。`floor-bin` 可继续作为数值初始化，需显式标 `approx_init`。

缺测/边界：不对空白节点强行赋予 measured 身份；最多在经过支持模型和模型误差界时声明 interpolated/approximate；无有效支持处保持 signal 可用与 correction/precision unavailable 的独立状态。整幅场景真实低频项必须进入 holdout 保真指标，不能只比较缝合前后 RMS。

## 4. 加性+乘性联合迭代及收敛声明

建议先按已验证**同响应/可比响应**组求解，定义联合目标

`L(s,α,γ) = ‖L_C^{-1}[y−diag(exp Φγ)Hs−Aα]‖² + L_stars(γ) + λ_a‖L_aα‖² + λ_g‖L_gγ‖² + L_scene(s) (optional)`。

`L_stars` 以每个独立匹配星在多帧的 `log F_ks=μ_s+η_k+colour_k(s)+PSF_EE_k(s)+noise_ks` 构造，`μ_s` 星本征通量是 nuisance。先完成独立留出星数据下零增益假设检验，防止图像差模错误驱动乘性自由度。在每个有效分量固定 log-gain gauge（例如 `Σ η_k=0`，但这仅是相对归一，不是 P1 的绝对光度锚），只解有足够星点空间覆盖的低维基。`g>0` 由 exp 保证；不能对可能为负的天空扣除像素直接取对数。

正向迭代候选：

1. 冻结身份、响应、足迹和 mask；以 `γ=0` 初始化，检查独立 matched-star 增益证据，未达到预先校正多重检验阈值时保持 `γ=0`。
2. 给定 gain，联合/分块解共同物理场 `s` 与 additive `α`，使用实际 own eval 基、对应背景-control 与 signal 协方差，不把同一背景拟合随机量加两次。
3. 给定 `s,α`，按 `J_γ` 与独立恒星约束更新 γ，可用 trust-region/line-search 的 damped Gauss–Newton，accepted step 验证同一目标的充分下降；冻结资格后才可声称数值方向。
4. 对每轮重估 `C`（如源泊松）明确记录**外层模型已更新**，不能机械宣称同一静态凸函数单调。交替最小化对固定参数的子问题可降低固定目标，但原联合问题是非凸，局部收敛/唯一性需要具体 Lipschitz、秩、目标/步长条件，不能由 CLI 正常退出推出。
5. 按独立 holdout star、天空控制、暗云区域以及边缘/无缝负例选择 additive-only vs A+G；阴性增益世界下优先锁定 g=1。

本轮独立小世界：三帧 ×23 个位置、36 个独立星点，`s`含真实宽发射峰、低频坡度及暗云 trough，`a_k` 为有规范的每帧线性差模，η 为帧级标量。采用 star-only max |z|>3.5 激活增益，然后用 SciPy variable-projection least_squares 联合解剩余可辨识量。单 seed 结果见机器 JSON。**仅在**同响应、identity PSF、已知 Gaussian 随机方差、无 bad-pixel/rejection 的简化世界中支持 toy 求解，不是 P5 E1 全物理验证。零真增益两种世界没有开启乘性；非零增益两种世界开启乘性。额外 500 组独立零真增益星点噪声 MC 只验证 activation false-positive 频率，不能代替完整天空/方差 coverage。

## 5. #80：二维二阶差分 penalty、临时锚定和原方程残差

冻结 `sky_plane.cpp` 在实测共同场前提下形成 `H_red`，添加 `λ_eff DᵀD`，Cholesky 失败时在 `Q=ker(D)` 上添加 `αQQᵀ` 并试图通过

`B_a=(A+αQQᵀ)^{-1}b`, `G=QᵀH_red Q`, `t=G^{-1}(Qᵀb−QᵀH_red B_a)`, `B_prod=B_a+Qt` 

修正。`A=H_red+λ DᵀD`，`P Q=0`。对任何 `t`，`A(B_a+Qt)−b=−αQQᵀB_a+H_redQt`。选择 t 消去其 Q 投影时，**正交于 Q 的残差**是 `(I−QQᵀ)H_redQt`，一般不为 0；只有 Q 对 `H_red` 不变、或该 t 导致跨子空间耦合恰好为零等特殊情形可精确。此结论是代数恒等反例，不受是否真实触发回退影响。

### 5.1 高维独立实例和数值结果

7×6 网格、n=42，按水平+竖直三点 `(+1,−2,+1)` 差分得 `D`，其零空间为 `[1,x,y,xy]` **四维**，非仅三个仿射方向；`rank(D)=38`。构造 SPD `H=XᵀX/n+0.3I`（seed=83176）、`b=H x_true`、λ=0.5、α=3，故理论上 `A` SPD 并有明确直接参考解。**独立 Oracle 人工强制进入锚分支做公式对照**：

| 估计量 | 相对 `‖Aθ−b‖/‖b‖` |
|---|---:|
| Q-only 旧公式 | **0.1234841652744** |
| 精确 Woodbury 修正 | **5.19×10^-16** |
| 同一惩罚目标的增广 QR | **2.00×10^-15** |

另外 `‖(I−QQᵀ)H Q‖=2.11324`，证实 Q/non-Q 耦合不为零；与无惩罚解的距离 `‖θ_λ−θ_0‖=4.12577`，说明 `λDᵀD` 本身改变估计量。这里 **PRODUCTION_FALLBACK_NOT_RUN**，未用真实 #80 矩阵，也未证明当前真实生产会出这么大的残差。

### 5.2 正确临时锚定逆与稳定实现边界

数学上若 `A` 与 `A_α=A+αQQᵀ` 均 SPD，令 `K=A_α^{-1}Q`、`B_a=A_α^{-1}b`，可取

`B=B_a+K (I−α Qᵀ K)^{-1} α Qᵀ B_a`，

亦即 Woodbury。它的修正方向是 **`A_α^{-1}Q`**，通常不等于 `Q`。该公式在弱 Q 曲率下的小矩阵 `I−α QᵀK` 可能极病态，不能不经残差/后向误差证书直接当生产方案。更稳健候选是对 `sqrt(W)[H|A]` 与 `sqrt(λ)D` 的原始增广矩阵做 rank-revealing QR/SVD，或投影/分块正交后在明确的 Q / Q⊥ 子空间求原方程；可逆行列缩放必须同步变更 y/D/C/penalty，并还原物理预测和 `Aθ−b`，而非乱设 ridge。

**施工判决**：只有在冻结同一 `A,b,penalty,gauge` 上，比较旧 Q-only、Woodbury reference、augmented QR/SVD（以及生产实际替换算法）得到的**原方程完整 residual** 和科学 forward prediction，才能接受修复。临时 anchor 即使正确移除，也不移除长期 λ 对低频场/参数的偏差；λ 需要由科学模型/先验选择并提供 holdout/偏差预算，不能由秩容差来源自动称“无损”。

### 5.3 原 #80 E0 与本轮证据的关系

E0 冻结实测 2×Hα(600s)+1×Red(300s)：17237 obs，5807 overlap，权重中位尺度 `1.405e32`，`λ_min(Hred)=-0.002265592`，`λ_max=1.3118516e13`，相对负谱约 `-1.73e-16`，处于 double 舍入量级。与 Red 标度应用到 Hα 的历史 overlay 仅说明尺度敏感，**不是**真实光度校准或相同 SED 响应。不同标签导致 `frame_id` 哈希排序变化、锚帧变化及数值 PASS/FAIL，是稳定性问题，不能证明物理共同场。正确条件是 `scale_state/application_id`、所选目标量、signal/control variance/ivar 真正同单位及可比较响应。当前 S3 **只复核证据，无真实矩阵重新执行或新 C++ 改动**。

## 6. 统计保真不能由视觉连缝替代

若 `d∼Poisson(S+sky)`，减去已知 `sky` 均值后 `Var(d−sky)=S+sky`，而非 S。若 `sky_hat` 由同数据得到，`Var(d−sky_hat)=Var(d)+Var(sky_hat)−2Cov(d,sky_hat)`；如另有随机 gain 和共享标定参数，用联合 `JΣJᵀ` 算条件/边际协方差及跨帧/跨像元项。若直接通过 data-driven rejection 决定 a、g 或样本权，固定集合线性 GLS 的误差条只是条件于该集合的近似；全流程覆盖率须对独立 forward draws 测。

在 E1/E2 应独立报告：(i) 背景剩余的**有符号**边界台阶，(ii) 弱弥散真值通量及暗云低频功率传递，(iii) heldout 恒星总通量/颜色/PSF，(iv) 估计量同单位的 empirical MSE / variance / SNR / 68、95% coverage，(v) 同一母版或共同估计参数导致的 Cov(e_k,e_j)，(vi) gain-null false activation，(vii) 响应组谱差别而不是将其拟作 P5 sky。所有统计按**独立模拟场或真实独立观测单位**定义统计分母，不把相关像元数当有效独立样本数。

## 7. 函数/接口施工建议（生产执行端另排期）

| 功能与静态入口 | 需引入的科学合同/API语义 | 首要独立验收 |
|---|---|---|
| `sky_own_eval` | 共享 single source of truth 的 4-corner evaluator 与 row-emitter（double、ICRS→gnomonic, half-open support、tile boundary） | row·values == evaluator；边界/缺格/跨face样本 |
| `sky_make_residual_fields` | 将当前 floor-bin 由 `solver` 改为 `approx_init`，消费上述 row、真实 `control_variance`/Σ、资格和 gauge，返回 `rank/support/err/weight_basis` | #83 两帧反例 1→0；holdout斜坡；实际固定目标 before/after |
| `p2_sky_plane_build_joint` | 每次 inner solve 固定 target，显式 loss、rank、selection/reweight/regrid identity；GN 单步 trial 加 accept/damping | 同目标不增；改基时禁止虚报收敛 |
| `sky_plane.cpp` Schur solver | `MatrixProblem {D,b,W,C,gauge,penalty,basis_units,scale_id}`；独立 QR/SVD 路径；矩阵后向残差与可辨识状态 | 原数据矩阵与预条件矩阵 prediction 一致；无未认证 ridge |
| `sky_plane.cpp` deflation | 真实 `ker(D)`（full-grid 4维，带缺节点重新计算）、精确残差验证；在不病态时求解原 `A`，病态时返回 `NUMERIC_RANK_UNCERTAIN` | 7×6 高维反例、真实 rank 不确定、实际触发分支与 SVD 残差比 |
| `p2_op_upm_fit` / `p2_op_upm_apply` (`module_adapters.cpp`) | 明确模型 `same_response` / `finite_SED_basis` / `mixed_numeric_only`；P1 `application_id`；corrected signal+noise 双传播；跨帧参数协方差关联 | 2×Hα 与真实 2×Hα+R 分组/混合对照；无错误假同谱 |
| 独立 star relative-gain estimator (研究候选) | `log_flux`, flux/colour/PSF-EE/background, star covariance、mask、fit/heldout split、support、gain gauge、FDR; `exp(η)` | g=1 阴性不开启；真实 g recovery；无法辨识时不填随机乘性 |
| Stage2 writer/Stage3 provenance | 最终仅一科学 signal HiPS，保留 response-mix、fit selection、uncertainty_kind、conditional/marginal、支持域/缺项 | 外部 FITS/HiPS 读回与实际数值、单位匹配 |

**错误语义**：`DATA_INVALID_COVARIANCE`（非PSD或非法权）、`DATA_CONFLICT_UNITS`、`MODEL_INCOMPARABLE_RESPONSE`（允许 mixed numeric-only 但不作为 common-field）、`MODEL_UNIDENTIFIABLE`（经规范/物理支持仍秩亏）、`NUMERIC_RANK_UNCERTAIN`（规范化/精度不足无法判秩）、`MODEL_EXTRAPOLATED_UNVERIFIED`（缺格/跨支持域），`PRECISION_UNAVAILABLE`（合法信号但缺误差）；科学能力状态不必强制错误退出或阻止用户 mixed imaging。C++ 接口落点需要执行端按当前公开 API 梳理；本报告不声称现有枚举已提供这些名字。

## 8. 证据记录与未决

Oracle 运行本地命令、JSON、版本、seed/输入声明与数值读数见 `code/s3_oracle.py`, `code/s3_results.json`。高维 H,b 用 SHA-256、生成式、独立 seed 留索引；需要逐元素比对时按脚本重建，也可在后续生产矩阵 dump 协议导出 NPZ（大件无需进入 git）。E0 仅合成高维且 `branch_is_forced_for_formula_test=true`；真实生产后续要另做可控 branch injection 并保存工具链 SHA。

已证结论在数学假设内有效：#83 floor-bin 和实际双线性不匹配；#80 Q-only 修正通常不满足原方程；二维完整 P 零空间在全矩形为 4 维；有 PSF/星点颜色/空间支持验证才可求残余 g；加性天空不能消除已生成的 photon noise。需要 E1→E3 的长期真实数据与论文独立对照才能验证算法价值或关闭 #76。

### 一手文献/技术对照（待 E1 对照实现确认适用限制）

- Murphy, *PhotometricMosaic*, PixInsight script（基于匹配星的相对测光、背景渐变；不能直接宣称与本算法方程等价）：https://sites.google.com/site/johnmurphyastrophotography/software-by-john-murphy/photometricmosaic
- Montage 背景匹配、全局重叠解算：https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html
- Padmanabhan et al., SDSS ubercal（重叠约束、全局定标规范）：https://arxiv.org/abs/astro-ph/0703454
- Fruchter & Hook, Drizzle（相关噪声及空间重采样）：https://arxiv.org/abs/astro-ph/9808087
- Zackay & Ofek, image coaddition（不同场景点源最优核与 PSF）：https://arxiv.org/abs/1512.06872

**结束状态**：THEORY_PASS / TOY_PASS / PRODUCTION_NOT_RUN / REAL_E2_NOT_RUN / USER_VISUAL_PENDING。本轮独立研究文件交付后停止，等待外部独立数学复审。