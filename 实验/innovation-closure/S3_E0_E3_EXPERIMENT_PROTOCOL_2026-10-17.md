# ACSD S3｜P5 E0–E3 可复现实验协议（研究规格，不是已运行报告）

**基线**：`main@6f1410e1084c87265e59d2d9a7bc109405c9d714`。**主 Issue**：#76、#83；关联 #80/#14/#8/#9/#21。状态：E0 `TOY_PASS`（仅 `s3_oracle.py` 已实际运行的条目），E1/E2/E3 `NOT_RUN`；Production `PRODUCTION_NOT_RUN`，人工 `USER_VISUAL_PENDING`。本协议由前台冻结条件、数值输入和判据，由执行端另外排期；不将表中的目标写成实验成就。

## 0. 身份、存储、科学估计量

- 所有实验首先输出 `experiment_id`, `git_commit`, `dirty`, `binary_sha256`, `solver_version`, `parameter_schema_hash`, `seed`, `input_index_sha256`, `frame_id`, `source_sha256`, `calibration/master IDs`, `photometric_application_id`, `scale_state`, `linear_unit/BUNIT`, `response_id`, `reference_sed_id`, `PSF/EE_id`, `WCS`, `Drizzle_operator_id`, `sample_mask_hash`, `support/footprint`, `covariance_kind`, `sky_basis_id`, `rank_convention`, `gauge_id`, `algorithm_arm`、真实/合成标识。多帧可有不同合法 `photscal`；验证的是物理目标及单位的兼容变换，不是字符串相等。
- 科学基准：same-response 情形以已知 `S` 的对应输出核作用结果为真值；different-response 则以物理完整 `∫L_λ R_k dλ` 的分通带预测为真值，不能把单色 HST 图像当完整 SED；mixed numeric-only 不存在单个任意通带真值时只报告其按冻结权重形成的数值响应及可解释范围。
- 大 FITS、HST 源数据、生产矩阵文件留在 Agent `/testdata` 或专用 scratch（不进 Git）。仓库仅纳入轻量 JSON、CSV、脚本、报告、输入/产物哈希与必要小矩阵。HST 有坏像元/重建伪迹时标记有效区域 `valid_truth_mask`；形态评估仅使用其内侵蚀后的共同可信支持域。
- 算法对照时冻结同一数据、有效样本、几何、SNR/noise 解释、mask/拒绝样本、stretch（只供视觉）和训练/留出划分。重建与合成代码不得复用同一个“真实”拟合函数自证。参考 QR/SVD 使用 SciPy；高阶矩阵可另用高精度 mpmath/longdouble（先检查平台扩展精度是否真不同）。
- 允许用户混合所有设备/滤镜；报告标记 `COMMON_ESTIMAND_IDENTIFIED`、`CONDITIONAL_MIXED_RESPONSE`、`MODEL_INCOMPARABLE_RESPONSE` 三种物理身份，和 `OK_RANK`, `MODEL_UNIDENTIFIABLE`, `NUMERIC_RANK_UNCERTAIN`, `DATA_INVALID_COVARIANCE` 四类数值身份，两轴互不覆盖。每分量 gauge 单独记录。

## E0｜数学、独立线性代数与统计前提（本轮只运行其中指定 toy）

### E0-A：own-fit/eval 算子（#83）

1. 冻结 `sky_own_eval` 实际四结点双线性行；坐标用同一经纬→切平面但评价函数在 independent Python 实现；验证 row·node 等于实际 `sky_own_eval`，横纵边界含 0、1、0.5+ulp，非法外推/NaN 缺格状态独立。**只有相同基**可以声称固定目标的 F-step LS。
2. 两帧 x=.25,.75 的旧 floor-bin/填补 vs 正确双线性 LS：应有 objective 1 vs 0。使用 4 个有效观测（噪声 variance=1），保存 `B`, `r`, `W`, `gauge`, `β`, `objective`, `gradient`；先做常量正对照（两法同优），再做子格梯度反例。
3. 每个位置重复观测 1/2/5 次；异方差比 1/100/1e8；整行缺失、半面空洞、权为 0、无支持边缘、双连通分量；分别比较 QR、SVD、LSMR/LSQR 的 `[loss, gradient, rank, singular spectrum, predicted map]`。训练与评估算子一致时固定目标不得上升；改变 `own_surface_step`、rejection、mask、基/约束时输出不同 `objective_id`，不得跨目标直接比较是否“收敛”。
4. rank 未满与填补归因不能混淆；缺测允许 `precision/correction_unavailable`，不把任意 nearest/diffuse 填充值当测量精度。

### E0-B：高维二维二阶差分和 #80 deflation

1. 构造完整 7×6、32×24 规则网格的 x/y 方向 D2。无对角/交叉二阶差分时完整矩形 `ker(D2)=span{1,x,y,xy}`，维数 4；有不规则 free nodes/节点掩码则重算零空间，不能假定仍为 4。保存 D、Q 的 `||DQ||`、正交性与 null rank。
2. 分别构造 H=JᵀWJ 的温和 SPD、近零零空间曲率、PSD 真秩亏、含强 Q↔Q⊥ 耦合等至少四个世界；W 源于声明 SPD/semidefinite covariance 的正确白化；非法负权构成 DATA 错误对照。
3. 三臂：无惩罚 `λ=0`、有来源惩罚 `λ>0`、临时锚 `αQQᵀ` 以及当前 Q-only deflation。对原惩罚目标 **同一个** `A=H+λP, b` 计算 `||Aθ−b||/||b||`、数据白化残差、外推图像预测差、`λ||Dθ||²`、条件数、全部 singular 值或合理截取、gauge 与 posterior variance。另用增广 QR/SVD 与（病态时合理）高精度 oracle。
4. **必须刻意实际激活回退逻辑**：本轮 Python Oracle 的 `branch_is_forced_for_formula_test=true` 是一个有意义的公式反例，但 `production_fallback_executed=false`。未来执行端需增设**仅实验/测试注入能力**，在正常 SPD 问题上强制第一次 Cholesky 失败但不改原矩阵，以验证生产 fallback 返回原惩罚解；再用极小 Q-曲率真实触发，记录实际 `solve_deflated` 与全部 residual。不允许单元测试只声明 fallback 函数被编译即 PASS。
5. 不可证明输入的 `rank` 应为 NUMERIC_RANK_UNCERTAIN 而不是物理不可辨识。标签、frame_id 排列、合法锚更换若仅为可逆坐标变换，必须验证 `Hθ+Aα`、差模、数据目标与惩罚目标一致；若不同锚改变可行子空间/惩罚，那么明确新 `model_id`。

### E0-C：剩余增益零世界

- 不读取同一图像的共模拟合回流作为独立 star 锚。构造 500 次独立 3 帧×36 星零真增益试验，star log-noise sigma .012；一个 star 自身真实 flux 为 nuisance，经跨帧中心化消除；帧级 z 计算包含这个中心化的协方差。`max |z|>3.5` 是预注册 toy 判决，而非生产推荐通用门槛。
- 已运行 toy：2/500 false activation，经验比例 0.004，exact 95% Clopper–Pearson `[0.000485,0.014374]`。500 次不足以保证任何真实 PSF/颜色/多重空间模型下误开启率小于 1%；后续需 >2000 seeds，按 star brightness/EE/colour/spatial distribution 分层，使用家族错误率或 FDR 的正式预设阈值与校准外部数据。若无独立 gain 证据，则 `g=1` 并输出相应诊断。
- 建立 PSF EE 阴性：真 gain=1，EE=[.8,1,.9]，未作 aperture 纠正的假 log gain 最大 0.11364；正确 EE 归一后归零。建立空间 support 阴性：星点均在 x=0 时 affine gain 的设计 rank 2/4，加入 ±.8 支持才 rank 4/4。未知 EE/SED 时结果标 `gain_model_unidentifiable_or_confounded`，不能机械启用 g。

## E1｜严格物理前向合成（NOT_RUN）

固定 paired seeds `20261017 + 10000*scenario_index + replicate`；每 scenario 最少 200 独立 realizations，关键 false-positive、coverage 扩展至 2000。仿真器 A 生成原始 `Poisson((source+sky)*exposure*QE)`、暗电荷 Poisson、Gaussian read、真实 master 随机效应、PSF、WCS/SIP、像元面积与 Drizzle，仿真器 B 独立求解析条件均值和数值协方差的矩阵 Oracle；估计器不能拿自身同份噪声输出做真值。初始参数扫描写入 manifest，不在看结果后扩大有利臂。

| world | 特别输入 | 必须检验 |
|---|---|---|
| W0 | 同真实响应，g=1，a=0，宽低频 S+暗云 | 空接缝无虚构修正，gain 不误开启 |
| W1 | 仅相对加性场 ±constant/gradient，含负背景偏移 | A-only 应恢复差模，绝对共同常数声明不可辨 |
| W2 | 仅有 g=exp η，标量 0.9/1.1，独立星点 | A+G 恢复增益，A-only 留下相应 photometric residual |
| W3 | 乘加同时存在，空间 eta 梯度、空洞、弱重叠 | 相同有效支持下与独立真值对照，full model 数值 rank/参数区间 |
| W4 | 有暗云、宽弥散 Hα、混合星光，完全无真背景差 | 不得将真实低频扣作相对 sky |
| W5 | 变 PSF、encircled energy 0.75–1、blend、坏点/宇宙线 | gain 不能通过 PSF/孔径错误获显著性 |
| W6 | Hα3/7nm、[N II]、宽 Red、不同 QE，同星色标定 | 分开声明共同 SED/混合响应；不得假相同 S |
| W7 | 断连、只有 1 帧覆盖、星缺锚、局部二阶自由度过高 | `MODEL_UNIDENTIFIABLE` 或具名 extrapolated，不人为 ridge 成真 |
| W8 | HST 合法空间图作为 S 的形态、独立指定各线谱分量 | 测空间真值偏差（掩膜坏区域）；HST 图不充当未知 SED 真值 |
| W9 | 非常高/低 sky photon 噪声、共享校准/背景协方差 | 理论/经验方差、coverage，sky mean removal 不减 photon shot |

每个世界独立冻结 PSF、star flux, response, SED, Poisson/read 参数、WCS/Drizzle footprint、sky/gain truth、实际 covariance，保存 conditioning 与 valid-truth mask。对 E1 保留至少 `RAW/no-sky`、`additive_only`、`A+G`、`feather_negative`、适用时 `PMM/NSG/Montage` 等臂，匹配可比的估计量；真实 response 失配世界额外运行 `separate_filter_fields`、`finite_SED_basis` 与 `mixed_numeric_only`。

## E2｜真实数据 E2E 与产物目检（NOT_RUN）

- 同通带：先用真实两帧 Hα 和三帧 Red 各自完成 Normalize→Mosaic→Export；锁定每帧实际 calibration、`photoapplied`、Gaia XP 参考响应、signal+variance 单位、WCS、PSF 与 rejection；检查是否存在旧产品单位身份缺陷。
- 混通带：历史 M42_M1 2×Hα(600 s) +1×Red(300 s)，对照 #80 E0 input_index 与实际本次重新生成的输入 SHA；分别运行 2×Hα、真实 Red、3帧 mixed numeric-only、确有可辨识有限 SED basis 时才允许运行共同物理模型。Red 帧 `5.70245780684985e-17` 应用到 Hα 的旧 overlay 仅作标度干预，不作为合法校准或正式物理科学臂。
- 每个臂保存 Stage1 HiPS + `frame_science`, Stage2 final single-signal HiPS, Stage3 二维 FITS，外部 FITS/HiPS reader 读回真实 BUNIT 和 WCS、有效域。若 E2 真实原始 FITS 已不能访问或版本不一致，明示 `NOT_RUN` / `INPUT_MISSING` 而非用旧统计表冒充新产品。
- **用户本人目检**：同一展示 stretch、原始分辨率，覆盖全图、缝左右、弱星云/暗云、亮星光晕、天空梯度、数值过修正；带可溯源标注随机 A/B panel，负责人明确逐臂“可接受/不可接受及局部问题”。该目检不能由 Agent 代理，也不能替代 E3 定量检验。

## E3｜独立 holdout、SNR/保真与论文证据（NOT_RUN）

所有指标定义在 *同一物理响应与相同输出估计量* 的有效域。对于 mixed numeric-only，必须首先以 `R_eff`/forward-kernel 自身与指定比较对象建立关系，不能拿不存在的单通带真值计算“百分比无偏”。

| 指标 | 定义/分母/真值 | 采样与误差条 | 原则性失败标准 |
|---|---|---|---|
| Signed seam | 左右可比同一真实场 patch 的 `(B̂_L−B̂_R)−(B_L−B_R)`；分母可用共同真实背景/单位，不简单除接缝近零量 | ≥100 互不重叠边界 patch、按天区块 bootstrap 95% CI | 比 raw/add-only 引入方向性非零台阶，或仅局部 RMS 改善而真值偏差增加 |
| Stellar flux | 留出非饱和星 `(\hat F−F_truth)/F_truth`、颜色/通带、曲线背景和 EE 独立匹配 | 每场 ≥100 holdout 星，按 frame/夜晚、星型分层，paired bootstrap 95% CI | 在预注册非劣阈值（建议起始1%）外出现相对偏差增加，或星点改善却弥散恶化 |
| Diffuse total flux | `Σ_{p∈A} (\hat B_p−B_truth,p)Ω_p` / `Σ_{p∈A} B_truth,p Ω_p`；正负真实区域必须分开，真总通量近零则改绝对误差 | 每场 ≥30 真值 region，≥200 independent seeds；按 scene bootstrap | 弱扩展天体积分偏差超过预注册允许值（建议起始2%，需与仪器科学需求校准） |
| Dark cloud / low f | 定义 mask 内 `Re∑_q B̂(q)B_truth*(q) / ∑_q |B_truth(q)|²`，按独立 k-bin 和有效窗口去 PSF 差；另报低频 bias，不将天空负值截零 | ≥3 空间尺度 bins/场，200 seeds，独立场间 CI | 强烈平滑造成有效低频 transfer 偏离1，或错误 sky 拟合侵蚀暗云 |
| Ratio Hα/[N II] | 已知两种线分量的独立场前向，在线谱校准响应下测对应 `line_flux_Ha/line_flux_NII` 偏差；分母真实线强度 | ≥30 含双线的 region/SED 族，按场 bootstrap | 把本来无法解析的混通带变化声明为单通带无偏，或受控线比真值错误 |
| Variance/SNR | 相同 `B` 估计量的 `Var_MC(B̂)` 与预测 `lᵀΣl`，误差由相同场景随机化估计；报告跨像元、跨帧相关矩阵和 photon floor | 200–2000 independent seeds，按位置块聚合；理论/经验比区间 | 宣称完整方差却遗漏共享拟合协方差、或把 sky shot noise 扣没 |
| 68/95% coverage | fraction of independent draws with `|B̂−Btruth|≤z σ_model`，分别 `z≈1,1.96`；不可把 correlated pixels当独立 trial | ≥2000 independent forward seeds/独立场，每场 bootstrap interval | 预注册 binomial-compatible interval 不覆盖 nominal 68%/95%，或系统偏差驱动失效 |
| Gain null false enable | 真 `η=0` 的世界选择非恒等 η 的比例；分母独立 scenario seeds | ≥2000 independent seeds + 95% exact binomial CI, star/PSF/SED 分层 | 未达到预注册 familywise false enable 上限（建议1%），或 EE/颜色阴性出现系统假增益 |
| Rank/condition | `rank(J_eff)`、singular values（在科学单位/列缩放后）、`||Aθ−b||/||b||`、白化数据 residual，gauge null | 固定矩阵、不同合法行/列变换/锚排序 10+ 组；记录矩阵 SHA | 预测/可辨差模随纯标签/重参数化改变超过数值误差，或 non-SPD 无明确误差身份 |
| Scene uncertainty | 输出 `conditional_noise`、`marginal_noise` 和 covariance terms（含 cross）比较独立真值；无真值时报 unknown | 多层 bootstrap，保存共享 master/标定重复采样身份 | 因 `1/Σivar` 忽略跨帧相关仍冒称完整 marginal |

各项 1%/2% 为**计划的研究候选非劣阈值，不是仓库已有通过标准**。在数据生成前由研究负责人结合仪器误差预算冻结，必要时可改成与基线性能的配对非劣差；不得看过结果后改变标准。报告所有样本总数、合格/失败/拒绝分母，不删失败轨迹。对负例，正确“不可辨识/不可比较”也可作为算法科学上通过阴性检验，不要被要求计算物理上不存在的唯一真值。

## 4. 交付文件与完成判定

E0：`code/s3_oracle.py`, `code/s3_results.json`, `S3_SCIENCE_REPORT_2026-10-17.md`。生产实施批次另需独立 `matrix_dump.npz`, `row_operator_dump.npz`, `rank_spectra.csv`, `deflation_branch_trace.json`（全部由正式生产输入构造，不能凭 toy 伪造）。

E1：`sim_truth_index.json`, `response_curves.json`, `mask_truth.fits`（原始/大件不入 git）, `E1_experiment_metrics.csv`, `MonteCarlo_seed_manifest.json`, `E1_REPORT.md`。

E2：`real_run_index.json`, `frame_identity_report.json`, `e2_arm_comparison.csv`, `e2_display_panels/`, `E2_REPORT.md`, `USER_VISUAL_SIGNOFF.md`（负责人目检后才可填写）。

E3：`evaluation_plan_frozen.json`, `holdout_masks.json`, `paired_bootstrap_results.json`, `coverage/response/line-ratio tables`, `independent_review.md`。

只有理论+严格 E1 真值、真实生产 E2、E3 全项符合冻结标准和用户人工目检都成立，才可依 Issue #76 的完整要求考虑 P5 科学闭合。独立数学研究与 C++ CI 的 PASS 均不足以关闭。