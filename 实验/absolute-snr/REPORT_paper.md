# 跨帧绝对信噪比传递链：常数体系的三腿闭合与可传递性

**单元**：实验/absolute-snr（SCI-B，科学链创新点 P2）
**裁决依据**：`实验/裁决台账.md`（D-xx）、`docs/DISPUTES.md`（A-P2-xx）、`docs/science/DISPUTE_RESOLUTION.md`（科学正本同步件）；证据取自本单元主实验（seed 20260921）与三路重做及补实验（seed 20260926/20260601，快照于 `code/audit/`）。
**订正声明**：本稿为按裁决订正后的定稿，重写而非拼接；订正处逐条注明 D-xx / A-P2-xx，订正明细另见 `docs/LEDGER_CORRECTIONS_P2.md`。

---

## 摘要

跨帧绝对信噪比（SNR）传递链把单帧测光零点提升为可在帧间、口径间与流水线阶段间传递的绝对信噪比标尺。本文对支撑该标尺的全部 25 个登记常数逐项给出文献腿、实验腿与理论推导腿证据（其中 13 项三腿全做、5 项确认为结构性/合同性常数并附验证、其余给出实验腿与边界刻画——与 §1 贡献 1 同口径）：双计偏差登记值 +14.5009%/+38.2524% 由 Moffat β=4 轮廓的闭式与蒙特卡洛两条路径逐位复现（最大相对偏差 2.35×10⁻¹⁴，机器精度级）[实验:code/audit/route3/exp05_doublecount_corr.py][实验:code/audit/route1/exp04_double_count_bias.py]；协方差对角近似引起的方差欠估 36.3% 获得精确闭式 1+ρ(M_eff−1) 的解释并在同一条曲线上统一了历史遗留的两个读数 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py][推导]；天空样本预算 N_sky=9216=(1.44/0.015)² 与直接管线测量（N_min≈9321）相差 1.2%，阈值自洽 [实验:code/audit/route3/exp01_mad_sigma_budget.py][推导]；叠加权重指数 γ=2 由恒等式 w = SNR²/F_ref² = 1/σ_F² 唯一确定，数值偏差 4.4×10⁻¹⁶（2 ulp），γ≠2 时帧权畸变达 ×0.32–×3.16 [实验:code/audit/route1/exp12_gamma_weight_scale.py]。历史登记中 1.152 与 1.144 的取舍由 20 万次蒙特卡洛终裁为 1.152（20 万次主路实测 1.1508，相对偏差 +0.105%；另一路 MC 读数 1.1542 对应 +0.19%），对手支系数字漂移 5811→5816.6（分歧台账 D-04）。稀疏控制点 sparse_snr_value = F_ref/σ_F 由此获得完备的量纲、精度与有效性约定，成为下游球面映射（P3）、稠密重建（P4）与加性天光去除（P5）消费的无量纲硬通货。控制点方差语义中有限样本效应被三口径定量定界：纯公式口径下渐近式对真方差高估 9.53%（N=5，保守方向），端到端口径被 MAD 小样本偏置抵消至 ±1.5% 内（分歧台账 D-07）。

---

## 1 引言

天文图像校准流水线输出的每一个科学量都携带一个必须可传递的不确定度。若第 k 帧的测光零点为 ZP_k，单帧内部的信噪比陈述只能在该帧自证；一旦结论需要跨帧叠加（coaddition）、跨分辨率重建或跨阶段传播，"信噪比"必须先成为与具体帧解耦的绝对标尺。ACSD 科学链的第二环（P2，最高设计 §2）正是为此设立：它消费第一环（P1）产出的逐帧测光零点，产出帧级 frame_snr、逐源 σ_F/SNR_F 与帧内稀疏绝对 SNR 控制点（Δ=64 px 网格），并规定唯一的叠加权重换算口径 w = SNR²/F_ref²。

这一环的科学完整性最终凝结为一组常数：稳健统计常数（κ_MAD、高斯与 Moffat 轮廓的 FWHM–σ 换算、截尾均值与中位数标准误系数）、天空样本预算 9216 及其骨架常数 1.152、双计偏差基准值、协方差对角近似的欠估因子、叠加权重指数 γ、相关核倍数 k_corr 以及掩膜与截断窗口的几何参数组。独立审计曾对其登记出处提出系统性质疑；三路互不通信的实验重做与一项补充实验随后对全部 25 个常数逐项重做了文献核验、数值实验与理论推导，其结论经总编对账裁决后构成本文的证据主体。本文的贡献有三：

1. **常数体系的三腿闭合**。全部 25 个登记常数（P-CST-01…25）逐项给出文献腿、实验腿与理论推导腿，其中 13 项三腿全做、5 项确认为结构性/合同性常数并附验证、其余给出实验腿与边界刻画；"真值无效应 ⇒ 度量归零"的负例纪律在全部 35 个审计脚本中内置并运行验证 [实验:code/audit/*]。
2. **三个争议的终裁执行**。1.152 vs 1.144（D-04）、control_variance N=5 偏差方向与口径（D-07）、k_corr 常数 1.4 的地位（D-08，改查表并由 P3 单元承载）均按台账裁决写定，历史正本中与之冲突的表述相应订正。
3. **可传递性的接口证明**。以一个贯穿数值用例证明 P1→P2→P3/P4/P5 全链量纲与精度约定的一致性：控制点精度约定（1.5%）穿过重建接口（首轮以 IDW 代理算子做接口级核对——代理仅用于检验接口传递性，不宣称冻结算子的迁移已核对；冻结默认算子 natural_bicubic_spline_clip_v1 在真实控制网格上的传递实测：T ≈ 0.87（衰减、无放大）、离散化 rel RMS 0.1206，即 1.06×/0.2287 是代理算子自身性质，见 §4.9）；换算权 w = SNR²/F_ref² 的逐帧比值对参考星等档 m_ref 与零点在**代数上精确不变**（相对偏差 ≤3.3×10⁻¹⁶，因 F_ref 与 SNR 同源、恒等式中该档同时消去）；但该不变性在生产方差组成（含源泊松项，σ_F 不自相似于 F_ref）下只是近似，实测漂移 **1.9%–4.2%**（见 §2 精度约定）[实验:code/audit/route3/exp04_refmag_chain.py]。

## 2 【链条位置】

**上游接口（P1 → P2）**。输入为 P1 逐星产品（通量 F、FWHM）与逐帧测光零点 ZP_k [16]。P2 消费 ZP_k 生成参考通量 F_ref,k = 10^(−0.4(m_ref−ZP_k)) [量纲 ADU]；恒等式 F_ref,k·k_photo,k = F0 保证参考通量与 P1 零点自洽 [实验:code/audit/route1/exp04_double_count_bias.py]。逐源方差按最优提取组成 σ_F² = 1/Σ(P_i²/σ_i²)，其中 σ_i² 含天光、暗流、读出噪声与源泊松项（Horne 型方差组成）[3,4]；噪声项的通行出处见 [12,13]，天空扣除口径见 [14]，背景主导噪声极限与本链传递性的相容性见 [9]。

**本环产出**。① 帧级无量纲 frame_snr；② 逐源 σ_F 与 SNR_F；③ 帧内稀疏绝对 SNR 控制点 sparse_snr_value = F_ref,k/σ_F,c（无量纲，Δ=64 px 网格中心）[实验:code/audit/route2/exp09_geometry_plane.py]；④ 叠加权唯一换算口径 w = SNR²/F_ref² = 1/σ_F² [量纲 ADU⁻²]。

**下游消费**。P3 把控制点连同预测方差上球（drizzle）；P4 用插值核从控制点重建稠密 SNR 场（插值参数为配置量，插值设置已批配置化＋运行日志输出不落盘，承载于 P4 单元）；P5 按 w 组合多帧去除加性天光，SNR_comb² = Σ SNR_k²。

**精度约定**。frame_snr 依赖 m_ref：**天光限**（σ_F ∝ F_ref，纯天光支）下每星等精确降 10^0.4 = 2.5118864 倍[1]——这是上游权威的限定语（`docs/detail/registry/astrocs.phase1.noise-snr.md`「帧级 SNR（frame_snr）」「天光限下 SNR ∝ F_ref」）；**生产组成**（方差含源泊松项 ⇒ σ_F 不正比于 F_ref）下实测每星等比值 **1.5853–1.5859**（≈10^0.2 = 1.58489）：对抗审查在生产源上实测 1.5853090287937501 / 1.5857058347196948，本订正复核驱动实测 1.5857580959662516（相对 10^0.2 偏差 6.5×10⁻⁴；逐点见 evidence/f4_mref_domain.json）[实验:code/audit/route3/exp04_refmag_chain.py][实验:code/audit/route1/exp05_mref_and_reference_flux.py]。故 m_ref 必须随产品落盘、跨帧比较必须在同一 m_ref 档内。换算权 w = SNR²/F_ref² 的逐帧比值对 m_ref 与 ZP 是**代数恒等**（≤3.3×10⁻¹⁶）；但生产组成下该不变性只是近似：w 的跨帧比值随 m_ref 档漂移 **1.9%–4.2%**（生产驱动 f4 在真实 M42 帧对上最大 **4.20%**；audit exp04 生产臂 3.83%；audit exp05 1.95%；对抗审查实测 +2.03%——差异来自臂配置、m_ref 跨度与估计量）。同一恒等式的**零点灵敏度**是另一个量，不得与本区间混用：ZP ±0.02 mag 下 w 比值变化 1.85%（f4）/1.76%（exp04 负控）——故"跨帧可用、不依赖参考帧"必须与"同档 m_ref"联用，不得表述为对 m_ref 完全不变 [实验:code/audit/route3/exp04_refmag_chain.py]。量纲约定以 ADU 域为像素值域、SNR 无量纲；任何一环的量纲错位都会使 SNR 偏一个增益因子，双计偏差的增益不变性检验（bias 对 gain 精确不变）锁定了正确约定 [实验:code/audit/route1/exp04_double_count_bias.py]。控制点精度约定（1.5%，即 N_sky=9216 预算口径）经**接口级**核对穿过 P4 重建接口（首轮用 IDW 代理算子：归一化重建 RMS 0.229、控制点噪声通胀 1.06× —— 代理自身性质；冻结默认算子 natural_bicubic_spline_clip_v1 实测 T ≈ 0.87 衰减、同几何 disc 0.1206，见 §4.9）[实验:code/audit/route3/exp04_refmag_chain.py]。

## 3 方法

### 3.1 口径与定义

对点源的最优提取通量估计量 F̂ 及其方差由设计矩阵 P（归一化轮廓在像素上的取值）与逐像素方差 σ_i² 决定：

  F̂ = Σ(P_i x_i/σ_i²)/Σ(P_i²/σ_i²)，　σ_F² = 1/Σ(P_i²/σ_i²)。

该式为逆方差加权的标准结果 [2,3]。
**上式（D1）的适用前提（须显式列出，失效即边界）**：① 像素噪声相互独立；② 设计矩阵 P 已知且无误差；③ 逐像素方差 σ_i² 已知（以估计量 σ̂_i 代入会引入二阶项，其量级正是 P-CST-08 的 1.152 所度量）；④ 背景估计无误差（背景与源同估时另加 δB 项，边界闭式见 route3/exp06）。失效域：① 失效（像素间相关 ρ>0）时对角近似欠估 Var 比 1+ρ(M_eff−1)（§4.4）；② 失效（轮廓/口径用错）属系统口径偏差，与蒙特卡洛散度分开核算；③ 失效即为 §4.2 的终裁对象 [3]。帧级标尺锚定于参考星等档：F_ref,k = 10^(−0.4(m_ref−ZP_k))，m_ref=6.0 为单位制锚点（项目冻结纪律，取值不注文献出处）；星等标度 −0.4 与 2.5 是 Pogson 定义常数[1]。稀疏控制点的无量纲化 sparse_snr = F_ref/σ_F 使同一数值在不同帧、不同增益配置下语义唯一。

### 3.2 常数三腿闭合方法

对每个登记常数，三条证据腿独立取证后交叉判定：**文献腿**核对一手出处（DOI/bibcode/arXiv 级），核验方式与命中状态记入核验台账（refs.md）；**实验腿**以固定 seed 的纯 Python+numpy 蒙特卡洛或数值实验复现，每个实验内置"真值无效应 ⇒ 度量归零或判红"的负例（能红能绿，无恒真门）；**推导腿**给出解析闭式或结构论证。三路重做互不通信、目录隔离，交叉一致方判闭合；路间冲突提交总编对账裁决（分歧台账 D-xx）。全部读数为固定 seed（20260926；补实验 20260601/05）复现值 [实验:code/audit/*]。**数值精度声明**：本文全部复算与生产实现均为 IEEE-754 双精度（FP64，weight_chain.h 起全链 double）；文中 1e-9 / ulp 级判据只在 FP64 下有定义——同一判据在 FP32 下必红（负控 route2/exp05_float_tolerances.py 实测 FP32 相对差 3.4×10⁻⁸），故不得据本文判据推断 FP32 实现合规；产品数值位深由 FITS bitpix（-32/-64）承载，与判据精度是两件事 [实验:code/audit/route2/exp05_float_tolerances.py]。

### 3.3 数据

三类实验数据按最高设计 §12.2 配置：① 完整物理前向仿真（电子域 Moffat β=4 轮廓 + 天光/暗流泊松 + 高斯读出噪声 + 增益/量化，单元主实验 b1–b2，seed 20260921）[实验:code/b1_sky_scan.py][实验:code/b2_noise_terms.py]；② 纯解析代数合成（审计三路的常数恒等式与闭式复算，含全部负例）与"真实信号模板 + 完整物理前向仿真"（HST M16 高对比臂属本类：信号模板取自真实 HST 数据，但噪声/增益/量化/前向过程为完整仿真，不是真实测量）[实验:code/audit/*][实验:code/b3_domain_map.py]；③ testdata 真实数据（**仅** M42 三帧地面视宁度受限域原生帧，三口径适用域图谱）[实验:code/b3_domain_map.py]。负例均为非退化判据：常数输入⇒σ̂=0、ΔF=0⇒Δm≡0（逐位）、clip 负控 −6.7%~−8.8%、ρ=0⇒对角恒等（逐位 0.0）、常场⇒重建误差 1.4×10⁻¹⁴、RN=0⇒双计偏差=0 [实验:code/audit/*]。

## 4 结果

### 4.1 稳健统计常数：解析恒等式的逐位闭合

四个稳健统计常数全部收敛于解析闭式：κ_MAD = 1/Φ⁻¹(3/4) = 1.482602218505602（三路独立 MC 与闭式一致至 1.5×10⁻¹⁶）[5,6][实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route2/exp02_robust_scale_mad.py]；高斯 FWHM/σ = 2√(2 ln 2) = 2.3548200450309493（连续插值差 10⁻¹³）[实验:code/audit/route3/exp02_profile_constants.py]；Moffat β=4 的 FWHM/σ 闭式 2√2·√(2^{1/4}−1) = 1.2303076525901024，冻结值 1.230310 为手抄截断（相对差 +1.908×10⁻⁶），三路独立复算一致[8] [实验:code/audit/route1/exp02_moffat4_constant.py][推导]；90–10% 截尾均值常数闭式 0.7316730952806134（登记值相对差 −4.13×10⁻⁷）[7][实验:code/audit/route2/exp03_closed_form_constants.py]；中位数标准误系数 √(π/2) = 1.2533141373155001，登记值 1.253 的截断差 −2.5×10⁻⁴ 在 MC 误差内无害[17] [实验:code/audit/route1/exp01_robust_statistics_constants.py]。

### 4.2 天空样本预算：1.152 的终裁与 9216 的自洽

单 patch MAD→σ̂ 换算的相对标准误骨架常数历史上存在 1.152 与 1.144 两读[18]。20 万次蒙特卡洛（n=64）实测 1.1508，与 1.152 相对偏差 +0.105%（另一路 MC 读数 1.1542 对应 +0.19%）；1.144 支的天光预算复算 9216→5816.6（非 5811），属算术漂移而非独立测量。分歧台账 D-04 终裁取 1.152 [实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route1/exp03_sky_budget_constant.py]。管线级实测 c_eff = 1.4750784±0.023 与理论链 1.152×1.2533141 = 1.4438178 相差 **+2.17%**（= 1.36σ MC 误差），方向与量级一致 [实验:code/audit/route1/exp03_sky_budget_constant.py]；直接管线测得 c≈1.449 ⇒ N_min≈9321，与登记预算 9216 = (1.44/0.015)² 相差 1.2%，阈值自洽 [实验:code/audit/route3/exp01_mad_sigma_budget.py][推导]。解析渐近闭式给 c_known = 1.1664（合并支 1.148/1.165/1.168 渐近一致）[实验:code/audit/route3/exp01_mad_sigma_budget.py]。按 D-04，历史正本中 5811 的读数一律订正为 5816.6。

### 4.3 双计偏差：闭式存在且逐位复现

当经验总噪声 rms（含读出噪声）被当作纯散粒项喂入方差组成、且增益分支再次叠加 (RN/g)² 时，σ_F 被系统性高估：登记基准值 +14.5009%（RN=10 e⁻ 档）与 +38.2524%（RN=50 e⁻ 档）。05 正向规格曾将其标注为"seed 无关闭式"——该标签为错误标签，按台账订正：偏差存在精确闭式 bias = √(ΣP²/v_corr / ΣP²/v_emp) − 1，路线3 以 Moffat β=4 轮廓闭式复算 RN 扫描 6 点与暗流扫描，最大相对偏差 2.35×10⁻¹⁴（机器精度级）[实验:code/audit/route3/exp05_doublecount_corr.py]；路线1 以同一轮廓的蒙特卡洛配置复现全部扫描点（最大偏差 3.3×10⁻⁷）并验证偏差对增益精确不变 [实验:code/audit/route1/exp04_double_count_bias.py]。登记值与闭式之差 +1.91×10⁻⁶ 为登记舍入尾差，非物理量（A-P2-06）[实验:code/audit/route2/exp06_doublecount_bias.py]。本单元主实验独立测得该错误口径对蒙特卡洛真值的偏置为 +12.8%（基准点）至 +36.6%（B=0 暗源、RN=50），闭式预言与实测臂比值差 ≤1.25 pp，发现→修复（FIX-407）已闭环并有双向可假保护测试 [实验:code/b2_noise_terms.py]。

### 4.4 协方差对角近似：36.3% 的闭式解释

当输入噪声存在像素间相关 ρ 时，对角近似的方差欠估为

  Var_exact/Var_diag = 1 + ρ(M_eff−1)，

其中 M_eff 为有效相关邻域像素数。均匀 4-tap、ρ=0.19 时欠估 = 0.19×3/(1+0.19×3) = 36.31%，与登记值 36.3% 精确吻合 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py][推导]。历史遗留的另一读数 23.3% 与之同族：对应 ρ=0.101（4-tap）或 M_eff=2.6（ρ=0.19），是同一条曲线上的另一点而非矛盾（A-P2-08）[实验:code/audit/route2/exp07_correlation_diagonal.py]。旧启发式 1+0.75ρ̄ 给 12.5%，与两个登记值皆不符，予以淘汰 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py]。该结论仅检验对角近似公式本身；ρ̄=0.19 为仓库自测值，其标定属 P3/P4 域。

### 4.5 叠加权重的唯一性：γ=2 由恒等式确定

叠加权重必须以共同尺度表达才能跨帧比较。唯一使 w = SNR^γ/F_ref^γ 恒等于逆方差口径 1/σ_F² 的指数是 γ=2：数值上恒等式偏差 4.4×10⁻¹⁶（2 ulp），而 γ=1 留下因子 g_k 使帧权畸变 ×0.32–×3.16 [实验:code/audit/route1/exp12_gamma_weight_scale.py][实验:code/audit/route2/exp12_weight_gamma.py][实验:code/audit/route3/exp03_median_se_identity.py]。据此 γ=2 不需要标定证据，它由定义性恒等式唯一确定（A-P2-11）。归档恒等门 2.22×10⁻¹⁶ = 1 ulp 在浮点置位下随机变红（实测最大 2.5 ulp），判定规则按 A-P2-10 改为 ulp 计数（≤4 ulp 绿）[11][实验:code/audit/route1/exp07_weight_and_coadd_identities.py]。本单元主实验在完整物理前向仿真中对拍了逆方差集成：SNR_comb² = ΣSNR_k² 成立至 2.2×10⁻¹⁶，Q/W 组合方差对解析解 +1.17% [实验:code/b4_integration.py]。

### 4.6 控制点方差语义：k_corr 与有限样本效应

相关核倍数 k_corr 登记为冻结常数 1.4（标定值 1.3883）。台账 D-08 终裁：1.3883 为标定几何专属的单次蒙特卡洛实现（误差带 1.27–1.43），冻结常数 1.4 在声明域两端低估 32%/2 倍；k_corr 改为两因子公式 k_gauss(N_retained)×k_geo 加 (ρ, pixfrac, 帧数, patch) 几何查表，查表由 P3 单元承载（负责人已批），本文不再引用单一常数 [实验:code/audit/route1/exp14_kcorr_inflation.py]。其机制腿验证了 AR(1) 相关正态下中位数方差膨胀律 (1+ρ)/(1−ρ)：n=3、ρ=0.19 实测 1.233，与渐近律 1+(n−1)ρ = 1.38 的差属渐近域外推 [实验:code/audit/route1/exp14_kcorr_inflation.py]。

控制点方差的渐近式 control_variance = k_corr·(π/2)·σ_bg²/N_retained 的有限 N 行为由补实验三口径定稿（D-07）：**纯公式口径**（σ 已知）下解析精确值给出公式对真方差**高估 9.53%**（N=5，随 N 单调收敛，1% 边界在 N≈45–49）——历史文档"低估 8.5%"的方向词系笔误，本单元历史正本若曾转引已按 D-07 订正；**端到端口径**（σ̂ = 1.4826·MAD 同 patch plug-in）中 MAD 平方偏置（N=5 时 −9.4%）与有限 N 中位数高估同量级反号，净偏差 ≤±1.5%（N=5 为 −0.6%）；**生产链裁剪臂**（逐句复刻生产裁剪流程）低估 1.3–3.2% [实验:code/audit/supplement_control_variance/finiteN_control_variance.py][实验:code/audit/supplement_control_variance/production_chain_control_variance.py]。新发现的偶数 N_retained 效应（偶 N 中位数取两中央均值，真方差低于相邻奇 N）使端到端高估 +5.0%（N=20）、+2.8%（N=40）、+1.3%（N=100），生产 median_of 语义同样适用，登记为文档需补记项 [实验:code/audit/supplement_control_variance/finiteN_control_variance.py]。

### 4.7 掩膜、网格与截断窗口

局部掩膜半径 r_i = max(1.5, 0.75·FWHM_i)·(F_i/F_med)^0.1 中，k=0.1 与 0.75·FWHM 为项目约定（负责人已批豁免：项目约定，不注文献出处）；背景估计与掩膜的实践参照见 [10,19]，实验腿给出无星帧偏置≈0 的负例与半径扫描偏置曲线 [实验:code/audit/route2/exp08_mask_radius.py][实验:code/audit/route3/exp06_mask_window_cond.py]；亮源局部半径存在闭式 r_local(F=10⁵) = 17.60 px [推导：route3/exp06 推导腿]。控制网格 Δ=64 px 为结构采样常数（tile_width=512/8，与 HiPS 规范的 512 px tile 对齐）[15][实验:code/audit/route2/exp09_geometry_plane.py]。特征值门 λlo/λhi ≥ 1/16 对应设计矩阵条件数 κ≤4，误差随 κ 线性增长（err/κ≈0.017）[实验:code/audit/route2/exp09_geometry_plane.py]。轮廓截断半窗 min(256, max(30, ⌈12·FWHM⌉)) 在生产域的偏置 ≤6.5×10⁻⁵ [实验:code/audit/route3/exp06_mask_window_cond.py]；自洽口径（通量与方差同窗）下截断必使 SNR 偏低，05 规格"恒偏绿"为符号笔误，按 A-P2-09 订正为偏低；若通量来自 P1 精确光度而方差用截断窗（S2 语义），则出现与登记数值系列同号同量级的正偏差，声称系列可部分复原（A-P2-07）[实验:code/audit/route2/exp10_truncation_window.py]。

### 4.8 集成、传递与三口径适用域

跨级传递的对拍在历史正本与本轮审计中双重完成：P2→P3 方差传播 C_out = R C_in Rᵀ 的对角元 MC 比值 0.9980；对角近似宣称的方差只有实际散度的 32.5%，故点源信息量必须按输出 PSF 与完整协方差重算 [实验:code/b5_phase3_transfer.py]。三口径（帧级标量 / 稀疏 Δ=64 / 稠密）在 testdata 两域给出互补适用域：地面视宁度受限域稀疏口径全部优于帧级标量（RMSE 0.0413–0.0825 vs 0.0506–0.1691 dex），HST 高对比域帧级标量最好且失效边界 Δ*≈16 px；稠密口径存储 64 MiB/帧超预算 64 倍，确认其诊断地位 [实验:code/b3_domain_map.py]。SNR 路径 fail-closed 状态语义经转写自检与静默降级注入判红验证 [实验:code/b6_gates_audit.py]。

**帧级标量口径的电平系统偏差（P2-M3，补实验 EXP-05 表 A）**：生产 frame_snr 的 σ 取**整帧两轮裁剪 RMS**（作用域整帧、聚合为平方型），其被估量是 RMS(σ) 而非控制点中位 median(σ)，且整帧足迹把大尺度星云结构计入噪声。在真实 M42 帧上以 1 px 棋盘 hold-out（估计/真值零像素重叠）实测：**帧级标量（相对）表示**的 SNR 电平相对真值**偏低 15%–77%**（随面板结构含量而变；flat_with_struct 臂 −45.70%），而**绝对（控制点/逐像素）表示**在同域 **≤0.2%**（真实数据 ≤0.19%）；无结构场景下两者相当（−1%~+2%）。该偏低是**共模**项（逐帧常数 c_k·b_c），**不随帧数增加而消**，故多帧叠加不能修复 ⇒ 绝对口径不是"更精细"，而是消除该共模项的**必要条件**。同时登记一条归因订正：历史「整帧未裁剪 MAD 比裁剪 RMS 高 31.27% ⇒ 帧级 SNR 偏低 23.8%」不成立——1.3127 的分子来自另一条路径（noise_model 的未裁剪 MAD），与 frame_snr 的裁剪 RMS 不同源 [实验:code/exp05/exp05_common.py][实验:code/audit/supplement_control_variance/*]。

### 4.9 冻结默认重建算子的迁移核对（P2-M5）

补脚本 route3/exp11_frozen_operator_transfer.py 只读编译**生产**类 astrocs::v6::p2weight::SparseSnrReconstructor（weight_chain.cpp，sha256 记入产物 JSON；零 Python 重实现），在真实 M42 三帧 4096² × 真实生产控制网格（Δ=64 ⇒ 4096 控制点；Δ=32 加密对照、cell_center_v1、节点 31.5+64i）上实测**噪声传递因子** T = RMS(重建场扰动)/RMS(控制值扰动)（与真值、噪声模型无关）：**冻结默认算子 natural_bicubic_spline_clip_v1 的 T = 0.856–0.911（18 个读数全部 < 1，衰减、无放大）**；bilinear_regular_grid_v1 0.659–0.664（无钳制对照档）；nearest_control_point_v1 1.000/1.000（逐位，恒等互验）；mesh_median 0.660–0.733（非线性：单位脉冲恒 0、叠加残差 7.1×10⁻³）。无真值交叉验证：单位脉冲 Σ_k w_k(q)² 闭式 0.8539 vs 生产脉冲实测 0.8627；真实帧 1024² 全节点窗口 pred 0.8687 vs meas 0.8794（差 1.2%）。解析正弦真值下离散化相对 RMS：**冻结默认 0.1206**、mesh 0.5168、bilinear 0.1444、nearest 0.3173，而 **IDW 代理同几何 0.22874**（与 exp04 存档逐位一致）⇒ 结论：**§4.2 的「1.06× 通胀 / 0.2287 量级」是 IDW 代理算子自身的性质，不适用于冻结默认算子**——默认算子的控制点噪声传递是**衰减**而非通胀。16 门全绿（red_gates = []），含 fail-closed 负例（4 个未知算子 token、nearest 用于规则网格、控制值 0/负、nx<2、全 NaN、角点锚定网格）与正例（部分 NaN 按最近有效点填充 n_filled=3）；钳制档实测 clipped=1（bilinear clipped=0）；正齐次性 R(K·v)=K·R(v) 到 8.2×10⁻¹⁶。

**度量陷阱（口径约定）**：名义的「1.5%·mean(重建场)」归一读数 = T × RMS(控制值)/mean(重建场)，在重尾 σ 表示下会虚高（M2 帧达 4.79），而同一算子的 T = 0.873 ⇒ **凡判「控制点噪声是否被放大」一律用 T，不用名义归一读数**。

**诚实边界**：真实帧无逐像素真值 ⇒ 真实帧上的离散化 rel RMS（P=16 cell 中心 65536 点/帧、估计器自身 SE 0.0732）被估计器散度主导，四个差异极大的算子读数反而几乎相同（0.851–0.895）⇒ **该量分辨不出算子、只能作上界；T 能分辨（0.66–1.00）**。控制网格取自 σ 估计场而非 P1 真实控制点产品；Δ=32 仅加密对照；nearest 只在散点模式合法；钳制不可独立关闭（合同把核/滤波/钳制绑成单一 token）。读数与红/绿命令见 run/FINAL-07/审核包/科研审查/P2_订正/evidence/P2-M5_exp11_frozen_operator_transfer.md [实验:code/audit/route3/exp11_frozen_operator_transfer.py]。

  T = RMS(重建场扰动)/RMS(控制值扰动)（与真值、噪声模型无关）：

  | 算子 token | T（Δ=64，三帧） | 读法 |
  |---|---|---|
  | **natural_bicubic_spline_clip_v1（冻结默认）** | **0.856–0.911（18 个读数全部 < 1）** | **衰减、无放大** |
  | bilinear_regular_grid_v1 | 0.659–0.664 | 无钳制对照档 |
  | nearest_control_point_v1 | 1.000 / 1.000（逐位） | 恒等，D↔C 互验 |
  | …_mesh_median_v1 | 0.660–0.733 | 非线性（单位脉冲恒 0、叠加残差 7.1×10⁻³） |

### 4.10 逐像素绝对 SNR 重建的四臂判别

「信噪比不含天光信号」是 P2 的第一创新点，其逐像素形式可写成显式可证伪的模型：

    I(x,y)      = S_src(x,y) + S_sky(x,y) + N_local(x,y)          [ADU]
    sigma_slow²  = Var[N_local] + S_sky(x,y)/g                     [ADU²]  缓变的是方差面
    sigma_w²     = sigma_slow² + S_src(x,y)/g                       [ADU²]
    SNR(x,y)     = S_src(x,y) / sigma_w(x,y)                        [1]     分子只有源

其中 `g` [e⁻/ADU]、`ADU = N_e/g`。实测支持"只有方差图缓变、噪声实现是白的"这一前提：
噪声实现一阶差分 lag-1 自相关 −0.4985（白噪声解析值 −1/2），方差图相对散布 2.0%。
由于单帧不可分解，源项由支撑先验分离：P1 用生产的缓变面（8×8 = 81 节点），
P2 用生产的 GLS 信息权重解与生产 Moffat4 轮廓，P3 用生产的掩膜层次与排异计划；
`S_src_hat = Σ F_hat_i·P_i`（正向合成，不做残差相减）。

在解析臂 `D_core`（n = 1463，两组 seed 同向）上四臂读数：

| 臂 | 含义 | 中位 `SNR/SNR_true` | 解析预言 | 读法 |
|---|---|---|---|---|
| `T_full` | 本模型 | 1.0054（中位绝对偏差 0.0054、p95 0.0296） | — | 复原真值；方差面比值中位 1.0031 |
| `T_slow` | 漏源项 | 1.657 | 1.645（对拍差 0.70%） | 偏高 |
| `T_naive_sky` | 天光双计 | 0.874 | 0.871（对拍差 0.31%） | 偏低 |
| `T_traditional` | 字面「天光进分子」 | 1.314 | — | 偏高（全域 1.4×10⁵ 倍） |
| `T_null` | 真值无源 | 源项 `median(\|S_src_hat\|/sigma_slow)` = 0；`T_full ≡ T_slow`（相对差 0.0） | — | 但 `T_naive_sky` 不收敛（1.36802，与预言相对差 1.1×10⁻¹⁶） |

有源帧上同一归零判据为 37.31 ≫ 0.05 ⇒ 归零判据非退化 [实验:code/b7_absolute_snr_recon.py]。

**该实验证伪的三条任务书前提**：

1. **「`T_null` 三臂必须收敛」是错的**——`T_full` 与 `T_slow` 恒等，`T_naive_sky` 按预言发散 1.368 倍。照原话写归零判据，它在两臂上恒真、在一臂上恒假，**不携带信息**；必须逐臂写判据。
2. **「本地噪声与天光散粒在空间上缓变」按字面为假**——只有**方差图**缓变（散布 2.0%），噪声实现是白的（lag-1 自相关 −0.4985 ≈ −1/2）。把「方差缓变」写成「噪声缓变」会导出错误的实现前提。
3. **「`T_naive_sky` = 把天光加进分子」自相矛盾**——按其公式（分子纯源）实测偏低 0.874，按字面（分子含天光）实测偏高 1.314；两个方向相反的变体被当成同一个臂。

**适用域**：盲检测消融臂检测完备性 0.43，端到端 `D_core` 中位相对误差 0.376（判红）——差距全在**分子侧**（检测 + PSF 尺度），不在方差模型；真实 HST M16 星云结构臂源项只捕获 14.2%，「源是稀疏的」先验在延展发射上失效；真实 M42 帧上稳健二阶差分 σ 与生产方差面比值 0.986（均值版被星云结构污染 8.64 倍），逐源 SNR 与通量 Spearman ρ = 0.9969，但**增益不可自估**（lever_var = 0.024 < 0.15）。接口上，生产噪声模型控制点在 `(i+0.5)Δ`，与 `cell_center_v1` 差 0.5 px，生产重建算子正确 fail-closed；本实验显式声明 `grid_origin = 0.5` 使两者重合，未改生产代码。

## 5 结论

1. 跨帧绝对 SNR 的常数体系整体闭合：25 个登记常数全部获得三腿证据，其中全部科学量常数达到闭式或逐位复现级闭合；两处历史登记笔误（1.144 支算术漂移、「seed 无关闭式」标签）与两处符号/方向词笔误（截断方向、N=5 方向词）已按台账订正。
2. 稀疏控制点 sparse_snr_value = F_ref/σ_F 具备成为无量纲硬通货的全部要件：量纲约定被增益不变性锁定；m_ref 依赖在天光限（σ_F ∝ F_ref）下被权重比值恒等式**精确**中和，在生产组成下只被**近似**中和（m_ref 档实测漂移 1.9%–4.2%），故跨帧使用必须同档 m_ref 且该档随产品落盘；控制点精度约定（1.5%）经接口级核对穿过下游重建接口（代理算子），冻结默认算子的迁移核对见 §4.9。
3. 叠加权重 γ=2 由恒等式唯一确定，是定义性结论而非标定结论；k_corr 不再是单一冻结常数而由 P3 单元的几何查表承载。
4. 控制点方差的有限样本效应被定量定界且方向保守：渐近式对真方差高估（纯公式口径），生产链裁剪臂低估 1.3–3.2%（量级在预算裕量内）。
5. 「信噪比不含天光信号」在逐像素形式上由四臂判别证实：`T_full` 复原真值（中位绝对偏差 0.0054），漏源项偏高 1.657、天光双计偏低 0.874，两者与解析预言对拍差 ≤0.70%。同时该实验证伪了三条任务书前提（§4.10），其中「本地噪声与天光散粒在空间上缓变」按字面为假——只有方差图缓变，噪声实现是白的。

## 6 诚实边界

- **1.152 的标定登记出处**仍未补登（数值本身已由 D-04 终裁并获三路独立支持）；补登为 02 文档的待办事项，不影响本文任何数值。
- **m_ref=6.0** 为单位制锚点，无一手文献规定该值；本文按冻结纪律使用，其科学内容（每星等 2.512 倍标度与权重比值不变性）已独立验证。
- **k_corr 原始标定网格**不在登记材料内，机制腿（AR(1) 膨胀律）与构造性复原已闭合，但几何查表的具体网格属 P3 单元交付件，本文不引用其数值。
- **P-CST-23 声称系列的生成配置**欠定：登记三数可在 S2 语义下部分复原（同号同量级），完整复原需补生成配置登记。
- **生产链控制方差被估量 y 的合同语义**（裁剪后中位数 vs 全样本中位数）文档未写明，补实验按"裁剪后中位数"口径完成并上呈裁决。
- **文献腿降级项**：Serfling 1980 与 Cramér 1946 §28.5 为书目级（小节号存疑，标注后引用）；Moffat 1969 为 ADS bibcode 锚定（DOI 未核）；Aitken 1935 已核 DOI 10.1017/S0370164600014346（年份维持 1935 通行引法；出版年 INSPIRE 记 1936——卷 55 跨 1935–36，1936 通行注同文；经负责人批准进入科学文档）。全部核验状态见 refs.md。
- **本单元主实验的仿真坐标**（g=1.3 e⁻/ADU、RN=10 e⁻ 等）为声明量，不反解物理参数；真实数据面无解析真值，hold-out 真值自带 0.0224 dex 噪声。
- **对 MC 真值的偏置数字带 ±3 pp 蒙特卡洛噪声**（N=1000 时 σ_F 估计量相对标准误 ≈2.2%），闭式预言与实测臂比值之差（≤1.25 pp）才是低噪证据；引用具体偏置数字时应注明该不确定度。
- **帧级标量口径存在共模电平偏差 −15%~−77%**（§4.8；真实 M42 帧 hold-out 实测，随结构含量而变，不随帧数消）；本文的绝对口径结论**不得**被读成"帧级标量口径已可用"。历史「整帧未裁剪 MAD 高 31.27% ⇒ 帧级 SNR 偏低 23.8%」为归因错误，已按 EXP-05 §2.5 订正。
- **误差预算文档与补实验适用域已纳入引用集**（P2-M4）：本单元 docs/snr-propagation-design.md（误差预算口径：eps_drizzle 计入求和、eps_theta 复核 +20.1%/+608.6%）、docs/EXP-05-ABSOLUTE-SNR.md（三口径适用域与电平偏差）、docs/EXP-06-SNR-PHYS.md 与 docs/EXP-06-SUMMARY.md（口径差未决项）；此前两稿未收这些域边界。
- **稀疏层的生产者与载体缺位（P2-B2 新增硬边界）**：本文所称"帧内稀疏绝对 SNR 控制点"当前在生产链上**无产者**——Phase1 尚无 sparse_snr_layer 侧车写者（合同 schema 已冻结于 eng/contracts/schemas/unified/sparse_snr_layer.schema.json）与 HiPS 属性载体（ASTROCS_SPARSE_SNR_LAYER）发布者。Phase2 侧本轮已把 snr_path 的消费面**真正接上**并与合同对齐：请求 sparse_reconstruct 时按属性读取稀疏层、**逐像素**以 w = (sparse_snr/F_ref,k)²·g_k² 组合（红/绿臂见订正报告 P2-B2 与 run/FINAL-07/logs/p2b2_*）；缺层/层损坏/枚举非法一律 fail-closed（SNR_PATH_DENSE_UNAVAILABLE / SPARSE_SNR_LAYER_DAMAGED / INVALID_SNR_PATH_TOKEN）**不静默降级**，snr_path_effective / snr_path_reason / 层帧计数随产品落盘（旧版 in.sparse = nullptr 的悬空接线已删）。⇒ 论文中"控制点已产出并可被下游消费"须降级为"口径已冻结 + 消费面已接线并红绿验证 + 生产者与载体待交付"；在产者落地前，P3/P4/P5 对稀疏层只能走 frame_reconstruct 或稠密路径。
- **数值精度**：全部复算 FP64；1e-9 级判据在 FP32 下无定义（FP32 负控实测 3.4×10⁻⁸）——不得据本文判据推断 FP32 实现合规（P2-m8）。
- **`sigma_sky` 估计器的三条失效域（`docs/frame-snr-canon.md` §2.7 的适用域表是唯一登记面）**：①**饱和**——天光跨膝点时单调性**严格反转**（实测天光 3.6e4→3.8e4 e⁻/px、饱和 25000 ADU 时 `sigma_hat` 由 124.6 跌到 3.9 ADU，`SNR_frame` 升一个量级以上）；处置是**整帧 fail-closed 拒收**，不是「剔除饱和像素后继续」——原处方本身就是产生反转的操作（剔除后保留样本是低尾偏置子集，`sigma_hat` 塌缩）。②**结构主导**——判据应是「`sigma_sky` 估计器对天光单调」而不是「`SNR_frame` 随天光下降」，且它只在**结构 rms / 天光噪声 rms ≲ 2** 时成立：实测 `d ln sigma_hat / d ln B` 为 `r=0 → 0.4898`、`r=1 → 0.2459`（已损失一半）、`r=3 → 0.0782`、`r=10 → 0.0101`、`r=30 → 0.0011`，`sigma_hat/sigma_true` 同步升到 1.41/3.16/10.05/30.0（`code/redteam/rt_sigma_hat_applicability.py`）⇒ **`r ≳ 3` 起单调性保证已损失一个量级**，`r ≳ 2` 的帧必须改用区域化 `sigma_sky`，整帧标量口径在该域**不适用**。真实 M42 亮帧的 `sigma_hat_prod/sigma_hat_fix = 2.44–3.66` 正落在该衰减带内。③**常量（掩膜 / 零填充 / 过曝置零）像素 ≥ 0.40**——第 2 轮 `MAD` 恰为 0 ⇒ 裁剪窗塌成 ±3e-9 ⇒ 只留等值像素 ⇒ `RMS = 0` ⇒ 生产估计器把 `sigma_hat` **静默置成地板 1e-9**（`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:107` 的裁剪窗地板、`:122` 的置地板分支）⇒ `SNR_frame` **高估 2×10¹⁰ 倍**，链上无任何守卫；0.30–0.38 区间另有无告警平滑退化带（SNR 高估 1.8×/3.2×/8.9×）。**处置为整帧 fail-closed 拒收（与饱和行同格式）**；该退化与同函数内 NaN/非有限输入路径的 fail-closed 处置相矛盾（`:89`、`:121`），**修法属生产码治理，本单元只登记不落码**。
- **其余口径的反例核查为阴性（未找到违反，据实登记）**：源主导 / 增益已知下 `sigma_F` 对 `sigma_sky` 严格单调；正性截断的 `F_instr` 截断偏差是**正且随天光下降**的有界项，被 `sigma_F ∝ sqrt(B)` 支配；掩膜边界 `r_i` 随 `sigma_bg` 收缩的方向一致（SNR↓），在 `r_i` 触下界 1.5 px 后**冻结**（斜率归零、非反转）；逐像素与整帧两种口径之差是**定义性恒等式**（把分子换成不乘 PSF 权重的量），不是可被误用的实验结论——但两口径**必须在正文标注**，否则会被读成物理量。

## 参考文献

（只收一手 VERIFIED 条目；核验方式与用途的完整台账见 refs.md）

1. Pogson, N. (1856). Magnitudes of thirty-six of the minor planets. MNRAS 17, 12. DOI: 10.1093/mnras/17.1.12. [星等标度定义]
2. Aitken, A. C. (1935). On least squares and linear combination of observations. Proc. R. Soc. Edinburgh 55, 42–48. DOI: 10.1017/S0370164600014346. [逆方差加权；已核 DOI（出版年通行注 1936 同文），经负责人批准]
3. Horne, K. (1986). An optimal extraction algorithm for CCD spectroscopy. PASP 98, 609. DOI: 10.1086/131801. [最优提取方差组成]
4. Naylor, T. (1998). An optimal extraction algorithm for imaging photometry. MNRAS 296, 339–346. DOI: 10.1046/j.1365-8711.1998.01314.x. [成像最优提取；已核 DOI 与页码]
5. Rousseeuw, P. J., & Croux, C. (1993). Alternatives to the median absolute deviation. JASA 88(424), 1273–1283. DOI: 10.1080/01621459.1993.10476408. [MAD→σ 常数]
6. Croux, C., & Rousseeuw, P. J. (1992). Time-efficient algorithms for two highly robust estimators of scale. Computational Statistics, 411–428. DOI: 10.1007/978-3-662-26811-7_58. [稳健尺度算法]
7. Stigler, S. M. (1977). Do robust estimators work with real data? Ann. Statist. 5(6), 1055–1098. DOI: 10.1214/aos/1176343997. [稳健性实证]
8. Moffat, A. F. J. (1969). A theoretical investigation of focal stellar images. A&A 3, 455. bibcode: 1969A&A.....3..455M. [Moffat 轮廓族出处：该文给出的是以 β 为自由参数的轮廓族，**β=4 不是该文结论**，而是项目/工具约定（PixInsight 标定集用 β=4，见 docs/science/PSF_SIGNAL_WEIGHT.md 引文与 `docs/detail/registry/astrocs.phase1.noise-snr.md` 的冻结口径）；bibcode 锚定，标注级（未逐页）——P2-m1 订正]
9. Zackay, B., & Ofek, E. O. (2017). How to COADD Images? I. Optimal source detection and photometry using ensembles of images. ApJ 836, 187. arXiv:1512.06872. II. How to COADD Images? II. A coaddition image that is optimal for any purpose in the background dominated noise limit. ApJ 836, 188. arXiv:1512.06879. [题名按两篇 PDF 首页逐字更正；原写的 “TRIPOLI series” 非该文题名。背景主导噪声极限与 SNR 传递的相容性]
10. Bertin, E., & Arnouts, S. (1996). SExtractor: Software for source extraction. A&AS 117, 393–404. DOI: 10.1051/aas:1996164. [背景估计与掩膜实践参照（不作为 k 值出处）]
11. Goldberg, D. (1991). What every computer scientist should know about floating-point arithmetic. ACM Comput. Surv. 23, 5. [浮点门值]
12. Janesick, J. (2001). Scientific Charge-Coupled Devices. SPIE PM83. [CCD 噪声项；标准专著]
13. Howell, S. B. (2006). Handbook of CCD Astronomy, 2nd ed. Cambridge Univ. Press. [CCD 方程；标准专著]
14. Newberry, M. V. (1991). Signal-to-noise considerations for sky-subtracted CCD data. PASP 103, 122. DOI: 10.1086/132801. [天空扣除 SNR]
15. IVOA (2017). Hierarchical Progressive Surveys, REC 1.0. DOI: 10.5479/ADS/bib/2017ivoa.spec.0519F. [hips_tile_width=512；官方 PDF 正文命中]
16. Riello, M. et al. (2021). Gaia EDR3: Photometric content and validation. A&A 649, A3. arXiv:2012.01916（arXiv 2020-12-03 预印；Crossref 10.1051/0004-6361/202039587 出版年 2021）. [测光标定实践；年标按出版年订正，P2-m2]
17. Christoph, G., Ulyanov, V. V., & Bening, V. E. (2022). Second Order Expansions for Sample Median with Random Sample Size. ALEA Lat. Am. J. Probab. Math. Stat. 19, 339–365. DOI: 10.30757/ALEA.v19-13. [样本中位数方差；期刊全文取回，著者/题名/页码经 Crossref 与期刊 PDF 双核]
18. Akinshin, A. 2022a, Finite-sample Rousseeuw-Croux scale estimators, arXiv:2209.12268; Akinshin, A. 2022b, Finite-sample bias-correction factors for the median absolute deviation based on the Harrell–Davis quantile estimator and its trimmed modification, arXiv:2207.12005. [题名按 arXiv abs 页逐字；MAD 有限样本偏置修正因子；摘要级]
19. Stetson, P. B. (1987). DAOPHOT: A computer program for crowded-field stellar photometry. PASP 99, 191. DOI: 10.1086/131977. [逐源测光背景，链上参照]
