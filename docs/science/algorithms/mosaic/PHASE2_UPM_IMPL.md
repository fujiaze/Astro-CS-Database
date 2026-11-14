# Phase2：公共连续天光与逐帧相对加性背景校正

**当前生产算法依据**：main `lib/algorithms/coverage/src/sky_plane.cpp` 的 `p2_sky_plane_build_joint`、`p2_sky_plane_open/eval_delta_block`，上游是[Sampler](PHASE2_SAMPLER.md)、[Sky科学定义](../../sky/UPM.md)。每帧校正 `y'_k=y_k−δ_k`，连续公共面和各帧相对加性差同一求解。本文保留真实数学模型、数值条件、误差和内存要求；**不包含退役M+C库或P5乘性flat新研发任务**。V2要把已有算法适配为一套按块消费的Sky模块，不复制旧工作文件或第二求解器。

## 1. 量、单位、有效域与初始统计量

令球面方向 \(\omega_c=(\alpha_c,\delta_c)\) 为控制位置，帧 \(k\) 的局部背景估计为 \(y_{kc}\)，单位为当次**实际**线性标定单位 \(L/{\rm sr}\)，不一律等于裸 ADU。估计量本身可负；其方差 \(V_{kc}\) 的单位 \(L^2/{\rm sr}^2\)，\(\mathrm{ivar}_{kc}=1/V_{kc}\) 单位 \({\rm sr}^2/L^2\)。`P2SkySample` 传 `frame_id:uint64`、`control_id:uint64`、`ra_deg/dec_deg/value/variance/snr:double`、`flags:uint32`；`P2ControlObservation` 还含 `uncertainty,control_variance,control_ivar,support,quality_flags`。同名 `ivar` 可指 **P1 单像元精度**，不得暗换成背景控制的 `control_ivar`；参考点源 SNR²/\(F_{\rm ref}^2\) 则是另一被估量的 Fisher 信息。

背景 patch 的历史有效近似为
\[
\hat b=\operatorname{median}\{x_j\}_{j\in{\rm retained}},\qquad
\hat\sigma=1.482602218505602\,\operatorname{MAD}(x_j),\qquad
V_{\hat b}\simeq k_{\rm corr}\frac{\pi}{2}\frac{\hat\sigma^2}{N_{\rm retained}}.
\]
这是 iid 高斯下中位数渐近方差的相关修正近似，不是任意展源、相关样本和采样后选择的无偏方差定理。`sky_plane.h::P2SkyPatchConfig` 默认 `min_samples=5,clip_iters=3,clip_sigma=3,contamination_sigma=3,min_retained_fraction=.60,k_corr=1.4`；`P2SkyPatchEstimate` 记录 `value/sigma_mad/variance/ivar/uncertainty/n_total/n_retained/bright_fraction/status`。`p2_sky_patch_estimate` 是纯函数，`valid==NULL` 代表全候选，非法输入/有效数不足/裁剪后不足/污染分别按 status 命名，不能将零 MAD/零方差补成巨大正 ivar。

**统一资格**（当前 `sky_plane.cpp` 的 base 和 own 均须执行）：坐标和值有限、未带 `MASKED/LOW_SUPPORT/HIGH_CONTAMINATION/REJECTED` 位；`weight_mode=0` 必须有限正 `variance`，`weight_mode=1` 必须有限正 `snr`。其后才计算非负有限实际权重；不合格点在每轮的 base、残差 own、gauge 和统计量中**同一索引全程剔除**。`NO_LOCAL_SNR` 描述来源而非许可凭空构造背景精度。默认模式为 inverse_variance；SNR² 模式只应按已显式声明估计量与来源使用，不提升为科学优选。加入不合格控制点时，已有效拟合/输出必须不变（Issue #78 回归）。

`P2SkySample.flags` 的五位分别为 `MASKED=1`、`LOW_SUPPORT=2`、`HIGH_CONTAMINATION=4`、`NO_LOCAL_SNR=8`、`REJECTED=16`。来源采样中星点、Gaia 晕、饱和、边界、空洞、亮端污染的拒绝/保留计数必须可追踪，不能把“数值可写入数组”当作“独立无偏空背景”。Patch 选择的共享主星掩膜/阈值会产生有条件分布，真实协方差需要独立实验标定。

## 2. 当前加性公共面模型、规范与可辨识性

以切平面坐标 \(u_c,v_c\) 表示真实球面方向的局部参数化，在同一可比较响应组与可用背景控制下用
\[
y_{kc}=\sum_{j=1}^{n_B}\phi_j(\omega_c)b_j
 +\sum_{\ell=1}^{m}\psi_\ell(\omega_c)d_{k\ell}
 +r_{kc}+\epsilon_{kc}
 =B_{\rm ref}(\omega_c)+\delta_{{\rm poly},k}(\omega_c)+r_{kc}+\epsilon_{kc}.
\]
\(B_{\rm ref}\) 是公共连续双线性/双三次样条，\(\delta_{{\rm poly},k}\) 是逐帧阶数 0/1/2 的低阶多项式。研究稿还需说明不同响应的 \(H_k\) 是否已经使这些 \(y_{kc}\) 同义；不同滤镜的真实扩展源形状不能靠这个式子强行合并。残差场 \(F_k\) 是基模型解释不了的 **相对**空间差模，生产 \(\delta_k=\delta_{{\rm poly},k}+F_k\)；最终 corrected \(\hat y'_k=y_k-\hat\delta_k\)，不是“把全部绝对天光抹成零”。

在观测重叠图每一连通分量内存在背景/gauge 不唯一性：若 \(q\) 属于所有帧允许的背景基及公共域，可令 \(B_{\rm ref}\to B_{\rm ref}+q\) 并 \(\delta_k\to\delta_k-q\)，观测不变。需要按**实际有效控制点**构图；当前 `sky_plane.cpp::p5_component_graph` 对每分量设锚（参考帧的 \(\delta\equiv0\)）并保留 component 的诊断。分量独立可相对求解，但**没有外部天光/绝对零点**就不能宣称总体绝对背景恢复；单帧孤立分量不能按任意“必须可解”假设给独立差模。参考帧固定为零只是坐标规范，不是其天光恰为零的物理事实。

对于给定 IRLS 权重 \(W\) 和 gauge 消元后的设计矩阵 \(X=[B\,\,D]\)，信息矩阵 \(H=X^\top WX\) 至少应为对称半正定；在合法、可估计子空间上才要求严格正定。将逐帧低阶块消元，得到
\[
M_k=D_k^\top W_kD_k,\quad S_k=B_k^\top W_kD_k,\quad
H_{\rm red}=H_{BB}-\sum_{k\in{\rm free}}S_k M_k^{-1}S_k^\top,\quad
b_{\rm red}=b_B-\sum_k S_kM_k^{-1}b_k.
\]
要求各 \(M_k\) 在实际参数规范下可分解；若分量、帧自由度或空间支持不足，必须停止并说明具体不可辨识方向。不得给 \(M_k\) 或 \(H_{\rm red}\) 盲加 ridge 后把正定“做出来”就宣称数据足以识别。当前实现有可归因的 Cholesky 与数值锚/deflation 技术路径，数值正定性、**未正则化数据信息秩**和模型识别性仍是三个不同问题。

以列均衡的 \(H_{\rm red}\) 上的奇异/特征值谱定义唯一相对判据：\(\tau_{\rm eff}=\max(\texttt{rank_rtol},\max(m,n)\epsilon_{\rm FP64})\)，有效秩 \(r_{\rm eff}=\#\{\lambda_i>\tau_{\rm eff}\lambda_{\max}\}\)；可辨识性要求 \(r_{\rm eff}=n_{\rm free}\)，\(\kappa_{\rm data}\) 与 \(\kappa_{\rm solve}\) **分开**报告。默认 `rank_rtol=1e-10`；求解正则化 \(\lambda_{\rm numerical}\) 不能作为“已被数据识别”的证据。只有实际模型可辨识时才解释系数不确定度；正规矩阵本身不等于完整观测协方差，相关背景控制会改变 Fisher 信息。

### 2.1 罚法方程的生产求解语义：惩罚零空间候选基、精确 deflation 与具名失败

求解对象是罚法方程 \(A B=b\)，其中 \(A=H_{\rm red}+\lambda_{\rm numerical}P\)、\(P=D^\top D\)（三节点二阶差分；\(\lambda_{\rm numerical}=\tau_{\rm eff}\cdot\operatorname{mean}(\operatorname{diag}H_{\rm red})\) 是由判据派生的数值岭，不是自由参数）。\(P\) 的零空间（记 \(\ker D\)）非空是**结构性事实**：它至少包含支撑上的双线性系数空间，且当支撑被掩膜切成 \(C\) 个 stencil 连通块时维数可达 \(4C\)（S3-R 实测：完整 7×6 为 4，缺整行为 8，双 3×3 孤岛为 8）。因此候选基 \(Q\)（列正交）按连通块逐块取 \(\{1,x,y,xy\}\)（单块即全局四列），**\(Q\) 只是 \(\ker D\) 的候选子集，不得假称 \(\ker D=\operatorname{span}Q\)**；覆盖不足由下一步的分解如实报出。

当 \(\lambda P\) 的量级远大于数据在 \(\ker D\) 方向上的曲率时，\(A\) 在浮点下失去正定性。生产路径：先直接 Cholesky \(A\)；失败则取 \(\alpha=\max_{i}(A_{ii}-H_{{\rm red},ii})>0\)，用 \(A_\alpha=A+\alpha QQ^\top\) 抬成正定，再把锚偏置**精确**扣回。由 \(A_\alpha B=b+\alpha QQ^\top B\) 的不动点解出

\[
X=A_\alpha^{-1}Q,\quad R=Q^\top X,\quad c=Q^\top A_\alpha^{-1}b,\quad
(I-\alpha R)\,w=\alpha c,\quad B=A_\alpha^{-1}b+Xw,
\]

该式在 \((I-\alpha R)\) 可解时**恒有 \(AB=b\)**（与 \(A\) 是否奇异无关），故不改变科学目标。已退休的旧形式 \(B=B_a+Q(Q^\top H_{\rm red}Q)^{-1}(Q^\top b-Q^\top H_{\rm red}B_a)\) 假定 \(H_{\rm red}Q\subseteq\operatorname{span}Q\)，一般留下 \(Q^\perp\) 残差 \(b-AB=\alpha QQ^\top B_a-H_{\rm red}Qt\neq0\)（闭式反例见 S3；生产可达分支反例见 Issue #80 P0-A1），不得再用。

**回退分支的判据有定理支撑**：\(A\succ0\Rightarrow M\mathrel{:=}I-\alpha R=(I+\alpha Q^\top A^{-1}Q)^{-1}\)，特征值落在 \((0,1]\)；\(A\) 在 \(\operatorname{span}Q\) 上奇异 \(\Rightarrow M\) 出现零特征值；\(A\) 不定 \(\Rightarrow M\) 不定。故「\(M\) 的 Cholesky 成功」就是「\(A\) 在算术可分辨意义下正定」的判据，它把 \(\ker D\)、\(\ker A\) 与物理 gauge 分开。另以原方程 normwise 后向误差 \(\eta=\|b-AB\|_\infty/(\|A\|_\infty\|B\|_\infty+\|b\|_\infty)\) 复核，门取本仓唯一由算术推出的 \(\tau_{\rm floor}=\max(n_{\rm obs},n_{\rm free})\epsilon\)。直接 Cholesky 成功时只记 \(\eta\)、不设门（常规档逐位不变）。

不可分辨、或锚外仍有非正曲率时返回具名 `P2_SKY_PLANE_NOT_IDENTIFIABLE`（附 status/branch/\(n_{\rm free}\)/\(m_q\)/\(\lambda\)/\(\alpha\)/失败列/主元/\(\eta\)/门），**不**加任意 ridge、不降低阈值、不丢像元换取正定；失败路径不产出 `p2_sky_plane.bin`，故下游 apply/Writer 不会发布伪完成产品。

## 3. Huber IRLS、own 残差场与唯一加性执行序

Huber 损失及重权由标准化残差 \(z=r/\sigma\) 定义：
\[
\rho_\delta(z)=\begin{cases}\frac12z^2,&|z|\le\delta,\\
\delta(|z|-\frac12\delta),&|z|>\delta,\end{cases}\qquad
h_\delta(z)=\min(1,\delta/|z|).
\]
取 \(\delta=1.345\) 默认。固定当前可用点及本轮 \(w^0_{kc}=1/V_{kc}\)，以 \(w_{kc}=w^0_{kc}h_\delta(z_{kc})\) 重建块矩阵；变量和数值缩放必须一致，且对零/非有限 \(V\) 不得用 1、\(10^{-12}\) 或 support 代替。当前源码 `sky_plane.cpp` 的 `chol_spd`、`p2_identifiability_assess`、`build_joint` 分别负责求解、秩判据和 outer 调用；是否实际收敛到联合最优，不能仅由“每步是 least squares”推断。

逐帧 own 格点原始拟合残差 \(R_k(\omega)\) 在**同一有效帧集**上取公共模：
\[
\bar R(\omega)=\frac{\sum_{k\in E(\omega)}u_k(\omega)R_k(\omega)}
                    {\sum_{k\in E(\omega)}u_k(\omega)},\qquad
F_k(\omega)=R_k(\omega)-\bar R(\omega),
\quad \sum_{k\in E(\omega)}u_k F_k=0 .
\]
权重 \(u_k\) 必须与估计量方差同义且经资格过滤；\(\sum u_k=0\) 处**没有定义可扣差模**，不制造零误差结果。当前格点数值插值和跨边界外推应按模型实际有效域限制；缺某帧该位置有效样本时只保留已验证可求值区间。共同模式可能包含真实弥散天体低频结构，不允许无证据将它整体作为“天光”从信号移走。

```text
sky_fit(samples, coverage, config):
  freeze input frame/response/control identities; validate finite coordinates and estimator units
  used = select_qualified_controls(samples, flags, weight_mode)  # one predicate for all paths
  components = connected_components(used, frame-control-overlap)
  establish one explicit gauge / component; reject unidentified components
  derive h_upper=min(overlap_width, pointing_spacing)/2
  derive h_lower=max(control_pitch, instrument_pixel_scale)
  for attempt in bounded adaptive node-spacing search:
    choose model basis for B_ref and per-frame delta
    for iter in Huber IRLS:
      assemble per-frame normal blocks from used with frozen estimator semantics
      profile out free per-frame deltas (Schur)
      diagnose symmetry, rank on unregularized equilibrated H_red, solve SPD on valid subspace
      backsolve deltas; update robust residual/weights; record objective and convergence
    if identifiable and representation adequate: adopt attempt and stop
    else refine or coarsen within geometry bounds; record reason
  for outer iteration 1..joint_outer_iterations:
    make per-frame residual grids ONLY from used
    remove weighted common residual on exact same support → F_k
    refit on y-F with same component/gauge/qualification
  hash FINAL effective (B,delta,own,frame binding,component,config,input response)
  save model → reopen & compare representative delta evaluations/hash
  return complete scientific model or named failure

sky_apply(frame, position, raw):
  model = open_and_validate(p2_sky_plane.bin)
  evaluate delta_poly(frame,position) + F(frame,position) with explicit status
  if UNKNOWN_FRAME / OUT_OF_DOMAIN / NONFINITE: fail or mark invalid, never use another frame
  corrected = raw - delta; propagate units and conditional/model uncertainty
```

空间节点间距是由数据几何界定的模型分辨率，不是标定常数：`h_upper=min(overlap_band_width_deg,pointing_spacing_deg)/2`；`h_lower=max(sample_pitch_deg,pixel_scale_arcsec/3600)`，若上界小于下界具名 GEOMETRY_UNSUPPORTED；默认从可辨识较稳的粗网格尝试，到需要时细化。参数数与有效观测数、`n_nodes<=max_nodes`、`n_used`、自由度 `n_used-rank_full` 一并保留；过高频自由度的残差降低不是天光分离正确的证明。

## 4. 误差、模型身份、所有权、复杂度与失败语义

校正依赖**由同一数据估得的模型**，因此不能只说“加法不改变噪声”：设 \(\hat d=\hat\delta(y)\)，则
\[
 {\rm Cov}(y-\hat d)=C_y+C_{\hat d}-{\rm Cov}(y,\hat d)-{\rm Cov}(\hat d,y).
\]
一阶模型在 \(\theta\) 固定且独立外部训练时可写 \(C_{\hat d}\approx J_\theta C_\theta J_\theta^\top\)；用同帧拟合、Huber/data-dependent selection 时须包含交叉项、拟合偏差及空间/跨帧相关。减均值不会去除天空光子 shot noise；乘性变换 \(g^{-1}\) 同时缩放信号和已有噪声，并叠加 \(\eta\) 参数协方差与相关性。最终 coadd 在固定权重/接受集合下有 \(V_{\hat B}=a^\top C_{y'}a\)，权重/拒绝随数据变化还需研究选择后误差。

**最终有效模型身份**必须内容寻址，绑定 `schema/model version, accepted input occurrence/frame_id, response/QE/PSF hashes, geometry+gauge/component mapping, B coeff, delta coeff, own_enabled/step/grid/frame binding/values, effective config/solver`；独立 `save→open→eval` 对拍，改变任一真正影响输出的 own 样本或帧绑定即换 hash（#77）。不以基础 `build` 阶段旧 hash、时间戳、文件名代替科学算子身份。现行代码在 #77 后已对 own 赋值执行额外 hash 路径，但本批没有跑反例并验证整个响应输入身份的完整性，不能说科学来源闭合。

CPU 主要代价为 \(O(N_{\rm control}\,q^2)\) 的稀疏局部基装配、按帧小块 \(O(n_f m^3)\) 求解，以及当前稠密 \(n_B\times n_B\) Schur/Cholesky 的最坏 \(O(n_B^3)\) 时间、\(O(n_B^2)\) 内存；own 格点为 \(O(N_{\rm samples}+n_f N_{\rm own})\) 工作集；自适应尝试次数与 IRLS×joint outer 线性放大成本。该表达是现行算法类别上界而非一次真实 profile；实际模块必须通过 Runtime 租约管理线程、输入数据和大块、取消及失败路径，不持有私有无界缓存。

| 函数族 / 状态 | 正式输入输出、寿命与错误 |
|---|---|
| `p2_sky_plane_build_joint(samples,n,cfg,adaptive,out_model,out_report,err,size)` | C ABI；调用方借用 samples/cfg，成功输出模型 opaque handle；模型由 `p2_sky_plane_close` 释放，失败不可消费部分结果；Report 是调用方 POD、attempts 上限 12。 |
| `p2_sky_plane_info` / `adaptive_report` / `normal_matrices` | `P2SkyPlaneInfo` 含 n_used/n_frames/n_nodes/rank/n_params/full/gauge/κ_data/κ_solve/λ/dof/model_hash；normal matrices 仅 `retain_normal_matrices=1` 时内存可用，open 产品不存矩阵。 |
| `p2_sky_plane_eval_delta[_block]` / `save/open/close` | 按 uint64 frame_id 和 ICRS 角坐标求连续差模；逐点 status `OK/OUT_OF_DOMAIN/UNKNOWN_FRAME/INVALID`，越域无外插；文件打不开/内容不全不能回退老 UPM 文件。 |
| 构建失败枚举 | `P2_SKY_PLANE_OK=0`、`INVALID_ARGS=1`、`NO_USABLE_SAMPLES=2`、`TOO_FEW_SAMPLES=3`、`FRAME_UNDERDETERMINED=4`、`TOO_MANY_NODES=5`、`RANK_DEFICIENT=6`（数值解失败）、`NOT_IDENTIFIABLE=7`（数据秩判断）、`NONFINITE_SOLUTION=8`、`IO_ERROR=9`、`GEOMETRY_REQUIRED=10`；库 rc 不等于 CLI exit code。 |

原长文的可执行 TEST 意图已归约为：合成已知背景/分量/gauge、浮点尺度/负方差、断连+弱重叠、单帧、穷举 tile/极区、缓存与重开、固定序/多线程、读取失败/取消，以及统计区分拟合值和独立有效星云/光度真值。D5 是**正式合同 + 静态源码复读**，没有在本批跑新数值、真实 E2 或负例。

## 参考文献及可复用的独立算法

- Huber, P. J. (1964), *Robust Estimation of a Location Parameter*, Annals of Mathematical Statistics 35, 73–101, DOI: [10.1214/aoms/1177703732](https://doi.org/10.1214/aoms/1177703732)；Huber IRLS 的鲁棒损失原理，不证明本 sky 模型无偏。
- Padmanabhan et al. (2008), *An Improved Photometric Calibration of the SDSS Imaging Data*, ApJ 674, 1217；[arXiv:astro-ph/0703454](https://arxiv.org/abs/astro-ph/0703454)：重叠自标定的规范退化参考。
- Fruchter & Hook (2002), *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP 114, 144；[arXiv:astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087)：重采样及相关噪声。
- Zackay & Ofek (2017), *How to Coadd Images?* I/II, ApJ 836, 187/188；[Part I](https://arxiv.org/abs/1512.06872)、[Part II](https://arxiv.org/abs/1512.06879)：有条件的 coadd 与点源测量区分。
- John Murphy, [PhotometricMosaic](https://sites.google.com/site/johnmurphyastrophotography/software-by-john-murphy/photometricmosaic)（PixInsight 第三方脚本）：乘性星点匹配与背景梯度对照，**尚未逐行审查其具体代码/许可证**。
- [Montage 背景匹配算法](https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html)：重叠背景优化思路；不等于本项目科学验证。
- Golub & Van Loan, *Matrix Computations*, 4th ed. (2013), §5–§6：最小二乘、Schur 与数值秩的独立数学参考。


