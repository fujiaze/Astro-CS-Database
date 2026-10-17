# ACSD S2｜跨响应共同场可辨识性与 P5 Schur/QR/SVD 数值独立研究

- **身份**：独立数学与 NumPy/SciPy 最小模型实验；不改变 `docs/**`、生产 C++、`sky_plane.cpp`；不声明生产 E2E 通过。
- **冻结起点**：`main@e369486e0b23dc91c71bfc0794e43c285602f72e`（2026-10-17）；E0 真实三帧诊断参考 `main@58ee3fe03fe650ae4fce6e0aea6c2fd65dd6bcfd`，E0 commit `69da3b018`，不得混为本轮真实复跑。
- **运行**：`python3 实验/innovation-closure/code/s2_oracle.py --output 实验/innovation-closure/code/s2_results.json`。输入与 singular spectra、rank、预测/参数误差、差异及 NOT_RUN 写入机器 JSON；不调用 ACSD C++。
- **对照来源**：`AGENTS.md`、`docs/science/unified/{UNIFIED_SCIENCE_MODEL,DATA_SEMANTICS}.md`、`docs/science/photometry/PHOTOMETRY.md`、`docs/science/sky/UPM.md`、`docs/science/noise_snr/NOISE_SNR.md`、`docs/engineering/contracts/NORMALIZE_DATA.md`、`实验/innovation-closure/S1_SCIENCE_REPORT_2026-10-17.md`、Issue #14/#80/#9/#8/#13/#76/#83、`实验/issue80-p5-nonspd-e0/{REPORT.md,code/p5_matrix_oracle.py,evidence/matrix_oracle.json,evidence/diag_normal.json}` 与 `lib/algorithms/coverage/src/sky_plane.cpp` 已在 Issue 定位的 Schur/gauge/own 函数。E0 数值为**已有证据复核**，非本轮生成。

## 1. 先区分目标：可数值混合不等于可联合估计同一物理场

令真实天体是未知谱辐亮度 `L_lambda(omega)`；严格单帧观测先对光谱与天区积分、PSF/WCS/像素积分/Drizzle：

`mu_ki = [G_k H_k \mathcal R_k L]_i + a_ki`, 其中 `\mathcal R_k L(omega)=∫ L_lambda(omega) R_k(lambda,omega) d lambda`，`H_k` 包括 PSF 及空间采样；噪声 `epsilon` 有实际场景泊松、读噪及参数/相关项。`G_k` 为 P1 已施加尺度之后**额外可证明的残余增益**，不能再次任意施加 P1 因子。为符号清晰，本文下述 `R_k` 吸收入射能量或光子计数惯例，所有响应应记录物理单位和完整支持波段，禁止重复乘 `lambda/hc`。

### 1.1 可比较性的精确条件

把允许的光谱族设为线性空间 `\mathcal S`。若希望单帧 `k` 经已知确定性标度 `c_k` 无偏估计同一参考响应 `R_0`，必要且充分的光谱条件是：对所有 `f∈\mathcal S`，

`∫ f(lambda) [c_k R_k(lambda) − R_0(lambda)] d lambda = 0`。

等价地，`c_k R_k − R_0 ∈ \mathcal S^perp`；若 `\mathcal S` 包含任意谱型，则 `c_k R_k = R_0` 几乎处处。仪器 QE 不同但综合响应在限定波段严格成比例时可比较；QE 的**形状**不同则不能单凭“相同滤镜”证明成比例。还需要实际 `H_k` 相同或经可验证的 PSF/footprint 对应到相同空间泛函，以及 signal/variance/控制统计量的同一线性单位。

对于仅包含已命名参考 SED `f_*` 的一维光谱族，`c_k = Q_0(f_*)/Q_k(f_*)` 使参考星响应相等，却不约束另一个未知谱型。更一般令 `L_lambda(omega)=Σ_{m=1}^M s_m(omega) φ_m(lambda)`，观测响应矩阵 `R_{km}=∫φ_m R_k`。如果想求目标 `B_0=r_0^Ts`，它在特定位置的线性无偏可估计条件是 `r_0` 在可用响应矩阵的**行空间**中；若要恢复完整多分量 `s` 则需要响应矩阵列满秩、空间/PSF算子也不损失所需模式、并正确处理噪声和背景。单一图像不可能无先验恢复两个任意独立谱分量。

### 1.2 星点与 Hα/[N II] 弥散场的解析负例

采用三个离散 spectral bins `[continuum,Halpha,NII]`（**抽象单位、并非真实滤镜透过曲线**），`f_*=(1,1,1)`，`f_neb=(0.1,10,5)`，`R0=(1,1,1)`，`R_H=(0.1,1,0.05)`，`R_R=(1,0.4,0.7)`。参考星校正系数为 `c_H=3/1.15=2.60869565`，`c_R=3/2.1=1.42857143`。校正后的**两帧恒星**都严格为 3.0，但弥散场响应分别为 **26.76521739** 与 **10.85714286**；目标 `R0` 为 **15.1**。若冻结空间权 `(0.8,0.2)`，混合信号为 **23.58360248**，相对具名 R0 目标偏差 **+8.48360248（+56.18%）**。这是可比较性负例，不是实际滤镜误差测量。

### 1.3 混合成像的准确数学含义

在已完成有据标度、共同单位、相同有效空间核且给定样本资格/权重为**数据无关固定值**的简化条件下，`a_k(p)=w_k(p)/Σ_j w_j(p)`，则 `R_eff(lambda,p)=Σ_k a_k(p)c_k R_k(lambda)`。同一光谱在不同覆盖/权重/rejection 区域的有效响应变化；对于参考 SED `f_*`，`Q_eff(f_*)=Q_0(f_*)`，对 `f=f_*+δf` 的偏差为 `ΔQ(p)=∫δf(R_eff−R0)`。只有外部约束 `δf` 的范数及响应曲线误差，才能通过 Cauchy-Schwarz 得到有限偏差界；星点零点 scatter 不自动约束星云偏差。

不同 PSF/Drizzle 时，更一般的结果为 `\hat B(p)=∫∫K_eff(p,omega,lambda)L_lambda(omega)domega dlambda + n`，`K_eff=Σ_k a_k(p)c_k K_k(p,omega,lambda)`；通常**不可分解**成位置独立的单一响应 `R_eff(lambda,p)` 乘公共空间 PSF。背景拟合若对同一观测做线性投影，则须把 sky fit 的跨帧/跨像素投影也乘入整体算子；数据驱动 rejection 和非线性拟合时上述 `K_eff` 只是**条件于冻结决策/参数**的局部线性化，不是完整端到端无偏保证。

用户仍可自由混合：输出定义为 `mixed_response_signal`/`relative_scaled`，说明权重与支持、参考 SED、`R_k` 和响应缺失、空间混合规则、PSF与 noise status；不得命名为已无偏转换到单独 R0 物理通带。产品只发 signal HiPS，响应身份和解释范围存在元数据/评估结果，不强行增加科学图层。

## 2. P5 相对加性与公共场：正确的线性统计对象

在**相同被估计量/共同响应**的合法分量，冻结增益、权重和合格样本，`y=H_g s+Aα+ε`，`Cov(ε)=Σ≻0`。H_g 是**被保护的**公共天空场实际成像算子，A 的列必须是最终背景/own 实际求值基（例如 bilinear，而非 #83 不一致的 floor-bin 基）。用 `Σ=LL^T` 白化 `b=L^{-1}y,F=L^{-1}H_g,G=L^{-1}A`；根据连通分量、规范和需要的锚得重参数化 `α=Zβ`，把 A 改为 `A Z`；`Z` 列必须独立且只删除真实 gauge，不能由标签哈希改变待估物理模型。问题是

`min_{s,β} ||b−Fs−Gβ||_2^2`，这里 `G=L^{-1}AZ`。

`F` 满列秩时，构造正交 `Q_F=orth(F)`，`P_perp=I-Q_FQ_F^T`，先以 rank-revealing QR/SVD 判断 `P_perp G` 列满秩，再稳定计算 `β=argmin||P_perp(b−Gβ)||`、`s=argmin||b−Gβ−Fs||`。此法通过**投影残差**求 Schur 解，不用浮点数直接相减 `G^TG−G^TF(F^TF)^{-1}F^TG`。若 F 本身不满秩但目标函数的某些线性泛函仍可辨，须转为相应 identifiable-subspace 分解；不能粗暴地将所有状态合并。实现也可对整个 `[F|G]` 直接 SVD/带列主元 QR，核对预测、rank与同一目标函数。

**Schur-PSD 定理**：`N=D^TΣ^{-1}D`（或含明确 PSD 先验的 `+L^TΛL`）在精确实数运算下必 PSD；若被消主块 `N_ss` SPD，则 `N_red=N_ββ−N_βs N_ss^{-1}N_sβ` 必 PSD。只有 gauge 删除后 `P_perp G` 满列秩才严格 SPD。`N_red` 的小负特征（相对于整体尺度位于浮点误差量级）可由消减引起；**显著负方向**要求排查非法权、装配、非 GN 的真实 Hessian、误模型或其它问题。不能用 λ_min<0 就断言科学不可辨，也不能任意加 ridge 强迫矩阵 SPD。

**参数坐标变换与锚**：规范改变可以改变 `s` 的背景零点和 `α` 数值，却必须保留 `Hs+Aα` 预测与所有可辨的差模量。对同响应 `y_k=S+a_k`，更换锚 `a_a≡0` 到 `a_b≡0` 可设 `S'=S+a_b`, `a'_k=a_k−a_b`，**仅当**共同 sky 表示空间能表达 `a_b` 时是完全同一可行模型。仅改 `frame_id`/FILTER 的排序不应使正确、固定输入算子的最小残差从数值 PASS 翻到 FAIL。若不同锚实际改变模型自由度或先验，则应报告目标已变，不是无条件不变性。

**严格合法行单位变换**：以可逆 `U` 令 `y'=Uy,D'=UD,Σ'=UΣU^T`，有 `(y'−D'θ)^TΣ'^{-1}(y'−D'θ)=(y−Dθ)^TΣ^{-1}(y−Dθ)`。观测预测经 `U^{-1}` 后一致。若只更改某帧 `y` 与 variance 而未变 `H`/`A`，就**改变了共同场假设**，不属于证明等价的变换。E0 将 Red `photscal` 用到 Hα 是后者的数值敏感性干预，绝非通带校准或物理等价证明。可逆列变换 `θ=Pz,D'=DP` 亦严格等价，但 SVD 的**相对秩容差**随列尺度变化，必须先在有科学单位意义的列规范下定义 rank，再用 rescaled/recovered physical predictions 验证。

## 3. #80 E0：真实异常数值原因与物理原因是两条独立研究链

**本段仅复核 E0 已有真实证据**：M42_M1，2×Hα 600s + 1×Red 300s，`n_obs=17237`，`overlap_controls=5807`，historical CLI exit=2、`h=upper`。A/B 旧产品 `photappl=0,photscal=1`，C 已应用 `photscal=5.70245780684985e−17`，却共同宣称 `BUNIT=ADU/sr`。原始 median inverse control variance 跨帧跨度 `1.40458e32`；Schur 最小特征 `−0.002265592`、最大 `1.311851609e13`，比例 `−1.727e−16`，与双精度舍入量级相当。归一化 E0 的 `rank([A|P])=31/31`、κ≈5580 仅是 **E0 冻结 scalar common-field 设计矩阵** 的数值结果，不证明真实两类滤镜已经有共同 `S(ω)`。原始 λ_min 显著负于零的**绝对值**不应称为数学上的明显非 PSD；须以相对谱/后向误差和 whitened design 判定。

E0 `abc_real`/all-Halpha-label FAIL、all-Red-label PASS，表明 `frame_id` 元数据哈希的最小值锚会改变浮点算法路径；它同时说明成功的 Red 锚臂可以几乎完全由 Red 决定，不能据 CLI=0 证明跨响应 sky 解正确。E0 把 A/B 的 `signal×c`、`variance×c²`、`ivar÷c²`（c 来自 **C 红帧**）使权跨度降到约2.241并消除当前病态；没有响应与每帧 reference SED 的验证，不能将此 c 作为真实 Hα 校准。新 main 的 P1 修复及 E0 之后的生产再运行 **NOT_RUN**。

**新增当前源码辨析（main 静态阅读）**：`sky_plane.cpp` 约 `:2044–:2060` 仍对 SPD 的逐帧参数块按 `H_red=H_data−Σ S_k M_k^{-1} S_k^T` 消元；约 `:2129–:2170` 定义 `λ_eff=rank_rtol·mean(diag(H_red))` 并构造 `H_solve=H_red+λ_eff D^TD`（二阶差分），约 `:2170–:2225` 在 Cholesky 失败时另给惩罚零空间作临时 Tikhonov anchor 并尝试 deflation 扣回 **临时 anchor** 的数值偏差。不能由源码注释的“λ 是无损数值正则化”得出科学估计量不变：一般 `θ_λ=(N+λP)^{-1}b ≠ θ_0=N^{-1}b`（只在 λ=0、`Pθ_0=0` 等特定条件下相等）。即使 deflation 精确去掉附加 anchor，也不等于消除了 `λ_eff D^TD` 对低频天空场的系统偏差。本轮未跑该生产分支的真实矩阵；建议下一轮以同一冻结目标分别对比 λ=0 可辨识 QR/SVD 解、当前 λ 解与 holdout/低频真值。此补充不是指控临时稳定性 anchor 在源码中必然未被扣回，也不是建议增加 ridge。

应分别解决：(i) 数值-单位链：真正验证每帧 `scale_state/application_id`、signal与控制方差，确定性 transform 完全成对、列尺度+白化 rank；(ii) 物理共同场：验证 Hα/R 的 `R_k` 与已知 spectral subspace 或将 H 分通带重建 `s_H`,`s_R`，相对背景只在实际可辨的共模空间内消去；(iii) #83 更新算子：own 的 floor bin 与 bilinear eval 不一致，**即使 Schur 准确求解也不能使另一个不同的基函数更新成为精确最小二乘**。

## 4. 本轮独立可复现 NumPy/Scipy Oracle：边界与实际输出

脚本使用 3 帧×7个位置的 21 个观测，固定 `H` 映射 7 个公共天空数值，`A` 使用每帧 0 次/1 次空间残差与帧间 sum-zero contrast 形成 4 个自由参数，总设计 `21×11`。noise covariance 是精确给定的对角 SPD，未加 ridge，模型参数给定并直接生成 y。

| Case | 数值结果 | 判定 |
|---|---|---|
| 同响应满秩 SVD/QR-pivot/projected QR | 三法 `rank=11/11`，κ≈2.646，预测与参数残差 ~1e−15 | `OK_MODEL_CONDITIONAL` |
| 合法行单位重表达 | 每帧 `U=(1e−16,1,1e16)`，白化 singular spectrum不变；回原单位最大预测差 1.78e−15 | **等价成立** |
| 列重参数化的错误相对秩 | 参数基按 `1e−8` 与 `1e8` 缩放，未经列规范的 `κ≈2.74e16`、SVD默认相对rank丢列；列范数预条件后全秩、预测误差 <6e−15 | `UNIDENTIFIABLE`仅是**数值坐标下误判**，不得冒充物理不可辨 |
| 缺失有效观测构造秩亏 | `rank=10/11`，σ_min≈2.06e−16 | `UNIDENTIFIABLE`，不以任意 ridge 修复 |
| 非法 covariance | 一个负方差 | `DATA_INVALID_COVARIANCE` |
| gauge 列重参数化 | 等价目标预测差 <1e−15 | 可逆 gauge 参数变换阳性 |
| 帧顺序置换 | 同输入响应，参数最大差 ≈1.3e−15 | 元数据/排列不变性阳性 |
| 不等价 overlay | 只改 y 和 variance 而不改 H/A | 被归为新模型，**不允许**解释为变单位科学结果 |
| 直接 Schur 与投影 Gram | 在良态模型下最小特征均 3.111111… | PSD 数值对照阳性 |
| 三频点参考星/星云 | 星点两臂都等于 3，星云目标15.1，混合23.5836 | 跨通带参考 SED 阴性 |

注意这里的 `UNIDENTIFIABLE` 在列缩放例子中是**给定未规范化参数坐标 + rtol** 的数值状态，重新规范后转为 `OK`；生产应使用 `NUMERIC_RANK_UNCERTAIN/ILL_CONDITIONED` 与真正 `MODEL_UNIDENTIFIABLE` 分开，避免将浮点秩判断提升为数学真理。脚本为合成无噪声目标，并未评价 bias、68/95% coverage、数值误差的严格后向界或真实 HST 结果。

## 5. 生产施工建议（Codex 正式合同整合之后交执行端）

### #9/#14 光度/响应合同

1. P1 `PhotoAppliedPlane` 及 `FrameScientificProduct` 写入 `linear_unit/scale_state/response_id/reference_sed_id/application_id/scale_map_digest`，严格指定其数值单位、响应积分类别（photon/energy）、有效波段、同一 PSF/空间支持，并配套传播 signal/variance/ivar；未知仅保留 relative/reference。`BUNIT` 是必须真实、**但并非充分**的同量纲证明。
2. Mosaic 在形成可共同解释 `H_g S` 的**科学模型**之前，建立 per-input comparability descriptor：`UNIT_MATCH`、`RESPONSE_PROPORTIONAL`、`REFERENCE_SED_ONLY`、`SPECTRAL_BASIS_IDENTIFIABLE`、`UNKNOWN_RESPONSE`；响应曲线或模型无支撑时仍允许 `mixed_response_signal` 输出，但禁止将相对 sky correction 当严格共通天体低频的无偏处理。
3. 若同一处采用 `R_eff`，记录冻结 weights+coverage/selection 的 provenance、各 R_k 的具体 ID/来源/未知项、输出解释边界；跨 PSF/DRZ 不可分离时标记 `joint_kernel_nonseparable`。不追加第二科学图像层。

### #80/#76/#83 P5 数值接口

1. `lib/algorithms/coverage/src/sky_plane.cpp` 的 `p2_sky_plane_build_joint`（及其内部 component/Schur matrix builder）入口消费**与控制估计量同一对象**的 `control_variance` 或已验证 SPD 相关 covariance。禁止把 #8 的点源 Fisher 直接当 sky control precision。先验证 positive finite、scene/scale/correlation/model IDs 和有效资格，输出按帧/分量有效观测计数。
2. 矩阵 API 应返回原始 `H_g,A,Z,Σ` / `C` whiten operator 或可分块应用的矩阵-向量算子，row/column scientific unit scales、free/gauge columns、component IDs、实际 basis evaluation；先做共享 sky 与相对加性差模的**可辨识性诊断**，再选 direct whitened QR/SVD 或投影 QR/Schur。不以 normal SPD 作为模型正确性的替代诊断。
3. 同一 gauge 内先数值平衡列，再在原物理变量还原预测验证；必要时使用 sparse rank-revealing QR / LSQR，但 large sparse solver 需要独立比较实际目标残差、近零奇异方向与分量隔离。多边界/不同响应应使用真实 H_k（或多谱基），不靠标签匹配作拒绝。
4. 修订选锚为显式的、有记录的 component gauge 或稳定投影约束，并做任意 valid anchor 的不变性对拍。不得宣称每种锚的参数坐标本身应相同；应核对共同 predicted samples 与可辨差模。
5. `sky_make_residual_fields` 的 own 训练矩阵使用 `sky_own_eval` **同一双线性基函数**，或将原 floor bin 清楚标注为启发式初始化，独立比较最小二乘目标；不把 Schur 数值修复冒充 #83 已闭合。
6. 失败语义分别输出 `DATA_INCONSISTENT_UNIT`, `DATA_INVALID_COVARIANCE`, `MODEL_RESPONSE_UNSUPPORTED`, `MODEL_UNIDENTIFIABLE`, `NUMERIC_RANK_UNCERTAIN`, `NUMERIC_FACTORIZATION_FAILURE`, `MODEL_CONDITIONAL_OK`。上游损坏与物理未知不合并；合法科学 signal 仍可有具名 downgraded 结果，但损坏输入不得沉默。

**独立验收矩阵**：同响应、同单位、不同 PSF经正确 H 描述、多阶 B-spline 与 own eval 真实基、一组分量和断连多分量、全局 reference gauge 和每合法锚、随机打乱 frame order/FILTER label（只改无物理效应的字段）、`10^-16`/`10^16` 的等价行重标度（必须同时变 D/Σ）、列归一后 SVD/QR/Schur 预测不变、rank−1 必明确失败、负/NaN权输入拒绝、合法低权正确保留、共同/异响应谱基是否秩足够、#80 冻结三帧真实矩阵dump独立同态对拍。每组保存输入哈希、`n_obs`,`n_vars`,`n_constraints`,`rank`,`singular spectrum`,`κ(Dwhite scaled)`,`κ(N)`、`||Dθ−y||_Σ`、`||prediction1−prediction2||`、gauge/instrument response/rejected set ID。成功必须同时满足数值残差和物理模型有效；失败路径也要保存矩阵/奇异值与具体原因。

## 6. 后续所需真实实验（全部 NOT_RUN）

- **E0 replay**：从已经冻结的 M42 三帧真实 `p2_samples.json` 与响应/应用链/哈希重新装配，导出原始完整 D/F/G 与 Σ、每一步 scale 和 rank，严格按真实物理映射与等价矩阵坐标变换对比。不得只拿 E0 `matrix_oracle.json` 摘要替代矩阵原始字节。
- **HST spectral truth**：不把 HST 单宽带图宣称真光谱真值；至少形成连续谱/离散 Hα 及 [N II] 分量的**已知前向合成**（HST空间图可作结构模板并限定有效区），用3nm/7nm Hα、Red、不同QE及PSF/Drizzle条件做参考星与弥散星云分层误差曲线。冻结不含加性、纯加性、共模低频、通带差、正负合成场与负对照。
- **真实 M42 E2**：用户真实同通带 Hα 2帧、3×Red对照和混合三帧，从验证了 P1 `application_id`/单位/响应的逐帧产品开始；独立核查随机噪声、control cov、源/夜空背景、PSF、颜色、差模与真实低频。比较 RAW、additive-only、物理响应分组、具名 mixed-signal 和联合模型，导出单signal HiPS→FITS；检查线比、弱 Hα、暗云、星点光度、支持域、seam、条件/边际 SNR 与用户目检。
- **物理外部锚与实际仪器曲线**：求真实滤镜传输、相机 QE、大气/望远镜响应、Gaia XP 外定标有效波段与标准星积分；不因 Gaia XP 已外定标推断任意系统图像已成 SI。缺失真实 QE/SED 时标明未知而不是采样自造曲线伪称测量。
- **#76 A+G 后续**：目前只验证冻结 g 下加性线性问题；尚未对非线性残余增益、星点独立限制、参数协方差、实际 reject 的非线性算子或全系统SNR做通过判定。

## 7. 一手参考与方法归属

- Montegriffo et al., 2023, Gaia DR3 BP/RP spectra external calibration, A&A 674 A3, https://doi.org/10.1051/0004-6361/202243880 （已由 A&A/INAF 原文核实）；预印本 https://arxiv.org/abs/2206.06205。
- Gaia DR3 synthetic photometry, https://arxiv.org/abs/2206.06215 ；BP/RP 有效谱域/系数协方差参见 De Angeli et al., 2023, https://doi.org/10.1051/0004-6361/202243680 。
- Fruchter & Hook (2002), Drizzle, https://doi.org/10.1086/338393 ；线性几何重采样不自动删除相关噪声。
- Padmanabhan et al., SDSS overlap photometric calibration, https://arxiv.org/abs/astro-ph/0703454 ；背景/零点 gauge 问题不因矩阵求解成功就消失。
- Golub & Van Loan, Matrix Computations, 4th ed.; Higham, Accuracy and Stability of Numerical Algorithms, 2nd ed.（QR/SVD、正规方程平方条件数、舍入误差）。

**结论状态**：`THEORY_AND_TOY_ORACLE_PASS` / `#80_E0_PREEXISTING_REAL_EVIDENCE_REVIEWED` / `PHYSICAL_CROSS_PASSBAND_MODEL_OPEN` / `PRODUCTION_FIX_NOT_RUN`。#14/#80/#76 不应由本轮直接关闭；等待独立数学复审。
