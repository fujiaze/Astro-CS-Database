# 跨帧绝对信噪比传递链：常数体系的三腿闭合与可传递性

**单元**：实验/absolute-snr（SCI-B，科学链创新点 P2）
**成稿依据**：独立审计/实验重做/总编对账/分歧台账.md 与 五单元成稿简报.md（唯一事实源）；证据取自本单元主实验（seed 20260921）与独立审计三路重做及补实验（seed 20260926/20260601，快照于 `code/audit/`）。
**订正声明**：本稿为按分歧台账订正后的定稿，重写而非拼接；订正处逐条注明 D-xx / A-P2-xx，订正明细另见 `docs/LEDGER_CORRECTIONS_P2.md`。

---

## 摘要

跨帧绝对信噪比（SNR）传递链把单帧测光零点提升为可在帧间、口径间与流水线阶段间传递的绝对信噪比标尺。本文对支撑该标尺的全部 25 个登记常数逐项完成文献、实验、理论推导三腿闭合：双计偏差登记值 +14.5009%/+38.2524% 由 Moffat β=4 轮廓的闭式与蒙特卡洛两条路径逐位复现（最大相对偏差 2.35×10⁻¹⁴，机器精度级）[实验:code/audit/route3/exp05_doublecount_corr.py][实验:code/audit/route1/exp04_double_count_bias.py]；协方差对角近似引起的方差欠估 36.3% 获得精确闭式 1+ρ(M_eff−1) 的解释并在同一条曲线上统一了历史遗留的两个读数 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py][推导]；天空样本预算 N_sky=9216=(1.44/0.015)² 与直接管线测量（N_min≈9321）相差 1.2%，阈值自洽 [实验:code/audit/route3/exp01_mad_sigma_budget.py][推导]；叠加权重指数 γ=2 由恒等式 w = SNR²/F_ref² = 1/σ_F² 唯一确定，数值偏差 4.4×10⁻¹⁶（2 ulp），γ≠2 时帧权畸变达 ×0.32–×3.16 [实验:code/audit/route1/exp12_gamma_weight_scale.py]。历史登记中 1.152 与 1.144 的取舍由 20 万次蒙特卡洛终裁为 1.152（相对偏差 +0.19%），对手支系数字漂移 5811→5816.6（分歧台账 D-04）。稀疏控制点 sparse_snr_value = F_ref/σ_F 由此获得完备的量纲、精度与有效性约定，成为下游球面映射（P3）、稠密重建（P4）与加性天光去除（P5）消费的无量纲硬通货。控制点方差语义中有限样本效应被三口径定量定界：纯公式口径下渐近式对真方差高估 9.53%（N=5，保守方向），端到端口径被 MAD 小样本偏置抵消至 ±1.5% 内（分歧台账 D-07）。

---

## 1 引言

天文图像校准流水线输出的每一个科学量都携带一个必须可传递的不确定度。若第 k 帧的测光零点为 ZP_k，单帧内部的信噪比陈述只能在该帧自证；一旦结论需要跨帧叠加（coaddition）、跨分辨率重建或跨阶段传播，"信噪比"必须先成为与具体帧解耦的绝对标尺。ACSD 科学链的第二环（P2，最高设计 §2）正是为此设立：它消费第一环（P1）产出的逐帧测光零点，产出帧级 frame_snr、逐源 σ_F/SNR_F 与帧内稀疏绝对 SNR 控制点（Δ=64 px 网格），并规定唯一的叠加权重换算口径 w = SNR²/F_ref²。

这一环的科学完整性最终凝结为一组常数：稳健统计常数（κ_MAD、高斯与 Moffat 轮廓的 FWHM–σ 换算、截尾均值与中位数标准误系数）、天空样本预算 9216 及其骨架常数 1.152、双计偏差基准值、协方差对角近似的欠估因子、叠加权重指数 γ、相关核倍数 k_corr 以及掩膜与截断窗口的几何参数组。独立审计曾对其登记出处提出系统性质疑；三路互不通信的实验重做与一项补充实验随后对全部 25 个常数逐项重做了文献核验、数值实验与理论推导，其结论经总编对账裁决后构成本文的证据主体。本文的贡献有三：

1. **常数体系的三腿闭合**。全部 25 个登记常数（P-CST-01…25）逐项给出文献腿、实验腿与理论推导腿，其中 13 项三腿全做、5 项确认为结构性/合同性常数并附验证、其余给出实验腿与边界刻画；"真值无效应 ⇒ 度量归零"的负例纪律在全部 35 个审计脚本中内置并运行验证 [实验:code/audit/*]。
2. **三个争议的终裁执行**。1.152 vs 1.144（D-04）、control_variance N=5 偏差方向与口径（D-07）、k_corr 常数 1.4 的地位（D-08，改查表并由 P3 单元承载）均按台账裁决写定，历史正本中与之冲突的表述相应订正。
3. **可传递性的接口证明**。以一个贯穿数值用例证明 P1→P2→P3/P4/P5 全链量纲与精度约定的一致性：控制点精度约定（1.5%）完整穿过 IDW 重建接口，换算权的逐帧比对对参考星等档 m_ref 与零点完全不变（相对偏差 ≤3.3×10⁻¹⁶）[实验:code/audit/route3/exp04_refmag_chain.py]。

## 2 【链条位置】

**上游接口（P1 → P2）**。输入为 P1 逐星产品（通量 F、FWHM）与逐帧测光零点 ZP_k [文献：Gaia DR3 锚定的 P1 星等坐标系]。P2 消费 ZP_k 生成参考通量 F_ref,k = 10^(−0.4(m_ref−ZP_k)) [量纲 ADU]；恒等式 F_ref,k·k_photo,k = F0 保证参考通量与 P1 零点自洽 [实验:code/audit/route1/exp04_double_count_bias.py]。逐源方差按最优提取组成 σ_F² = 1/Σ(P_i²/σ_i²)，其中 σ_i² 含天光、暗流、读出噪声与源泊松项（Horne 型方差组成）[文献：Horne 1986；Naylor 1998]。

**本环产出**。① 帧级无量纲 frame_snr；② 逐源 σ_F 与 SNR_F；③ 帧内稀疏绝对 SNR 控制点 sparse_snr_value = F_ref,k/σ_F,c（无量纲，Δ=64 px 网格中心）[实验:code/audit/route2/exp09_geometry_plane.py]；④ 叠加权唯一换算口径 w = SNR²/F_ref² = 1/σ_F² [量纲 ADU⁻²]。

**下游消费**。P3 把控制点连同预测方差上球（drizzle）；P4 用插值核从控制点重建稠密 SNR 场（插值参数为配置量，插值设置已批配置化＋运行日志输出不落盘，承载于 P4 单元）；P5 按 w 组合多帧去除加性天光，SNR_comb² = Σ SNR_k²。

**精度约定**。frame_snr 依赖 m_ref（每星等单调降 2.5118864 倍 [实验:code/audit/route3/exp04_refmag_chain.py]），故 m_ref 必须随产品落盘、跨帧比较必须在同一 m_ref 档内；而换算权 w 的逐帧比值对 m_ref 与 ZP 完全不变（相对偏差 ≤3.3×10⁻¹⁶）——这是"跨帧可用、不依赖参考帧"的全部数学根据 [实验:code/audit/route3/exp04_refmag_chain.py]。量纲约定以 ADU 域为像素值域、SNR 无量纲；任何一环的量纲错位都会使 SNR 偏一个增益因子，双计偏差的增益不变性检验（bias 对 gain 精确不变）锁定了正确约定 [实验:code/audit/route1/exp04_double_count_bias.py]。控制点精度约定（1.5%，即 N_sky=9216 预算口径）经验证完整穿过 P4 的重建接口：归一化重建 RMS 0.229 中控制点噪声仅贡献 1.06× 通胀 [实验:code/audit/route3/exp04_refmag_chain.py]。

## 3 方法

### 3.1 口径与定义

对点源的最优提取通量估计量 F̂ 及其方差由设计矩阵 P（归一化轮廓在像素上的取值）与逐像素方差 σ_i² 决定：

  F̂ = Σ(P_i x_i/σ_i²)/Σ(P_i²/σ_i²)，　σ_F² = 1/Σ(P_i²/σ_i²)。

该式为逆方差加权的标准结果 [文献：Aitken 1935（标注级引用，经负责人批准进入科学文档）；Horne 1986]。帧级标尺锚定于参考星等档：F_ref,k = 10^(−0.4(m_ref−ZP_k))，m_ref=6.0 为单位制锚点（项目冻结纪律，取值不注文献出处）。稀疏控制点的无量纲化 sparse_snr = F_ref/σ_F 使同一数值在不同帧、不同增益配置下语义唯一。

### 3.2 常数三腿闭合方法

对每个登记常数，三条证据腿独立取证后交叉判定：**文献腿**核对一手出处（DOI/bibcode/arXiv 级），核验方式与命中状态记入核验台账（refs.md）；**实验腿**以固定 seed 的纯 Python+numpy 蒙特卡洛或数值实验复现，每个实验内置"真值无效应 ⇒ 度量归零或判红"的负例（能红能绿，无恒真门）；**推导腿**给出解析闭式或结构论证。三路重做互不通信、目录隔离，交叉一致方判闭合；路间冲突提交总编对账裁决（分歧台账 D-xx）。全部读数为固定 seed（20260926；补实验 20260601/05）复现值 [实验:code/audit/*]。

### 3.3 数据

三类实验数据按最高设计 §12.2 配置：① 完整物理前向仿真（电子域 Moffat β=4 轮廓 + 天光/暗流泊松 + 高斯读出噪声 + 增益/量化，单元主实验 b1–b2，seed 20260921）[实验:code/b1_sky_scan.py][实验:code/b2_noise_terms.py]；② 纯解析代数合成（审计三路的常数恒等式与闭式复算，含全部负例）[实验:code/audit/*]；③ testdata 真实数据（M42 三帧地面视宁度受限域与 HST M16 高对比域，三口径适用域图谱）[实验:code/b3_domain_map.py]。负例均为非退化判据：常数输入⇒σ̂=0、ΔF=0⇒Δm≡0（逐位）、clip 负控 −6.7%~−8.8%、ρ=0⇒对角恒等（逐位 0.0）、常场⇒重建误差 1.4×10⁻¹⁴、RN=0⇒双计偏差=0 [实验:code/audit/*]。

## 4 结果

### 4.1 稳健统计常数：解析恒等式的逐位闭合

四个稳健统计常数全部收敛于解析闭式：κ_MAD = 1/Φ⁻¹(3/4) = 1.482602218505602（三路独立 MC 与闭式一致至 1.5×10⁻¹⁶）[实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route2/exp02_robust_scale_mad.py]；高斯 FWHM/σ = 2√(2 ln 2) = 2.3548200450309493（连续插值差 10⁻¹³）[实验:code/audit/route3/exp02_profile_constants.py]；Moffat β=4 的 FWHM/σ 闭式 2√2·√(2^{1/4}−1) = 1.2303076525901024，冻结值 1.230310 为手抄截断（相对差 +1.908×10⁻⁶），三路独立复算一致 [实验:code/audit/route1/exp02_moffat4_constant.py][推导]；90–10% 截尾均值常数闭式 0.7316730952806134（登记值相对差 −4.13×10⁻⁷）[实验:code/audit/route2/exp03_closed_form_constants.py]。中位数标准误系数 √(π/2) = 1.2533141373155001，登记值 1.253 的截断差 −2.5×10⁻⁴ 在 MC 误差内无害 [实验:code/audit/route1/exp01_robust_statistics_constants.py]。

### 4.2 天空样本预算：1.152 的终裁与 9216 的自洽

单 patch MAD→σ̂ 换算的相对标准误骨架常数历史上存在 1.152 与 1.144 两读。20 万次蒙特卡洛（n=64）实测 1.1508，与 1.152 相对偏差 +0.19%；1.144 支的天光预算复算 9216→5816.6（非 5811），属算术漂移而非独立测量。分歧台账 D-04 终裁取 1.152 [实验:code/audit/route1/exp01_robust_statistics_constants.py][实验:code/audit/route1/exp03_sky_budget_constant.py]。管线级实测 c_eff = 1.4751±0.023 与理论链 1.152×1.2533 = 1.444 相差 +2.4%（≈1.1σ MC 误差），方向与量级一致 [实验:code/audit/route1/exp03_sky_budget_constant.py]；直接管线测得 c≈1.449 ⇒ N_min≈9321，与登记预算 9216 = (1.44/0.015)² 相差 1.2%，阈值自洽 [实验:code/audit/route3/exp01_mad_sigma_budget.py][推导]。解析渐近闭式给 c_known = 1.1664（合并支 1.148/1.165/1.168 渐近一致）[实验:code/audit/route3/exp01_mad_sigma_budget.py]。按 D-04，历史正本中 5811 的读数一律订正为 5816.6。

### 4.3 双计偏差：闭式存在且逐位复现

当经验总噪声 rms（含读出噪声）被当作纯散粒项喂入方差组成、且增益分支再次叠加 (RN/g)² 时，σ_F 被系统性高估：登记基准值 +14.5009%（RN=10 e⁻ 档）与 +38.2524%（RN=50 e⁻ 档）。05 正向规格曾将其标注为"seed 无关闭式"——该标签为错误标签，按台账订正：偏差存在精确闭式 bias = √(ΣP²/v_corr / ΣP²/v_emp) − 1，路线3 以 Moffat β=4 轮廓闭式复算 RN 扫描 6 点与暗流扫描，最大相对偏差 2.35×10⁻¹⁴（机器精度级）[实验:code/audit/route3/exp05_doublecount_corr.py]；路线1 以同一轮廓的蒙特卡洛配置复现全部扫描点（最大偏差 3.3×10⁻⁷）并验证偏差对增益精确不变 [实验:code/audit/route1/exp04_double_count_bias.py]。登记值与闭式之差 +1.91×10⁻⁶ 为登记舍入尾差，非物理量（A-P2-06）[实验:code/audit/route2/exp06_doublecount_bias.py]。本单元主实验独立测得该错误口径对蒙特卡洛真值的偏置为 +12.8%（基准点）至 +36.6%（B=0 暗源、RN=50），闭式预言与实测臂比值差 ≤1.25 pp，发现→修复（FIX-407）已闭环并有双向可假保护测试 [实验:code/b2_noise_terms.py]。

### 4.4 协方差对角近似：36.3% 的闭式解释

当输入噪声存在像素间相关 ρ 时，对角近似的方差欠估为

  Var_exact/Var_diag = 1 + ρ(M_eff−1)，

其中 M_eff 为有效相关邻域像素数。均匀 4-tap、ρ=0.19 时欠估 = 0.19×3/(1+0.19×3) = 36.31%，与登记值 36.3% 精确吻合 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py][推导]。历史遗留的另一读数 23.3% 与之同族：对应 ρ=0.101（4-tap）或 M_eff=2.6（ρ=0.19），是同一条曲线上的另一点而非矛盾（A-P2-08）[实验:code/audit/route2/exp07_correlation_diagonal.py]。旧启发式 1+0.75ρ̄ 给 12.5%，与两个登记值皆不符，予以淘汰 [实验:code/audit/route1/exp10_covariance_diagonal_approx.py]。该结论仅检验对角近似公式本身；ρ̄=0.19 为仓库自测值，其标定属 P3/P4 域。

### 4.5 叠加权重的唯一性：γ=2 由恒等式确定

叠加权重必须以共同尺度表达才能跨帧比较。唯一使 w = SNR^γ/F_ref^γ 恒等于逆方差口径 1/σ_F² 的指数是 γ=2：数值上恒等式偏差 4.4×10⁻¹⁶（2 ulp），而 γ=1 留下因子 g_k 使帧权畸变 ×0.32–×3.16 [实验:code/audit/route1/exp12_gamma_weight_scale.py][实验:code/audit/route2/exp12_weight_gamma.py][实验:code/audit/route3/exp03_median_se_identity.py]。据此 γ=2 不需要标定证据，它由定义性恒等式唯一确定（A-P2-11）。归档恒等门 2.22×10⁻¹⁶ = 1 ulp 在浮点置位下随机变红（实测最大 2.5 ulp），判定规则按 A-P2-10 改为 ulp 计数（≤4 ulp 绿）[实验:code/audit/route1/exp07_weight_and_coadd_identities.py]。本单元主实验在完整物理前向仿真中对拍了逆方差集成：SNR_comb² = ΣSNR_k² 成立至 2.2×10⁻¹⁶，Q/W 组合方差对解析解 +1.17% [实验:code/b4_integration.py]。

### 4.6 控制点方差语义：k_corr 与有限样本效应

相关核倍数 k_corr 登记为冻结常数 1.4（标定值 1.3883）。台账 D-08 终裁：1.3883 为标定几何专属的单次蒙特卡洛实现（误差带 1.27–1.43），冻结常数 1.4 在声明域两端低估 32%/2 倍；k_corr 改为两因子公式 k_gauss(N_retained)×k_geo 加 (ρ, pixfrac, 帧数, patch) 几何查表（<!-- 订正: 检查-跨文档冲突 红1 连带——原 k_shape×k_geo 记号与 P3 正本 D8 的 k_gauss 因子两读，全域统一；旧对照：k_shape×k_geo -->），查表由 P3 单元承载（负责人已批），本文不再引用单一常数 [实验:code/audit/route1/exp14_kcorr_inflation.py]。其机制腿验证了 AR(1) 相关正态下中位数方差膨胀律 (1+ρ)/(1−ρ)：n=3、ρ=0.19 实测 1.233，与渐近律 1+(n−1)ρ = 1.38 的差属渐近域外推 [实验:code/audit/route1/exp14_kcorr_inflation.py]。

控制点方差的渐近式 control_variance = k_corr·(π/2)·σ_bg²/N_retained 的有限 N 行为由补实验三口径定稿（D-07）：**纯公式口径**（σ 已知）下解析精确值给出公式对真方差**高估 9.53%**（N=5，随 N 单调收敛，1% 边界在 N≈45–49）——历史文档"低估 8.5%"的方向词系笔误，本单元历史正本若曾转引已按 D-07 订正；**端到端口径**（σ̂ = 1.4826·MAD 同 patch plug-in）中 MAD 平方偏置（N=5 时 −9.4%）与有限 N 中位数高估同量级反号，净偏差 ≤±1.5%（N=5 为 −0.6%）；**生产链裁剪臂**（逐句复刻生产裁剪流程）低估 1.3–3.2% [实验:code/audit/supplement_control_variance/finiteN_control_variance.py][实验:code/audit/supplement_control_variance/production_chain_control_variance.py]。新发现的偶数 N_retained 效应（偶 N 中位数取两中央均值，真方差低于相邻奇 N）使端到端高估 +5.0%（N=20）、+2.8%（N=40）、+1.3%（N=100），生产 median_of 语义同样适用，登记为文档需补记项 [实验:code/audit/supplement_control_variance/finiteN_control_variance.py]。

### 4.7 掩膜、网格与截断窗口

局部掩膜半径 r_i = max(1.5, 0.75·FWHM_i)·(F_i/F_med)^0.1 中，k=0.1 与 0.75·FWHM 为项目约定（负责人已批豁免：项目约定，不注文献出处），实验腿给出无星帧偏置≈0 的负例与半径扫描偏置曲线 [实验:code/audit/route2/exp08_mask_radius.py][实验:code/audit/route3/exp06_mask_window_cond.py]；亮源局部半径存在闭式 r_local(F=10⁵) = 17.60 px [推导：route3/exp06 推导腿]。控制网格 Δ=64 px 为结构采样常数（tile_width=512/8，与 HiPS 规范的 512 px tile 对齐）[文献：IVOA HiPS REC 1.0][实验:code/audit/route2/exp09_geometry_plane.py]。特征值门 λlo/λhi ≥ 1/16 对应设计矩阵条件数 κ≤4，误差随 κ 线性增长（err/κ≈0.017）[实验:code/audit/route2/exp09_geometry_plane.py]。轮廓截断半窗 min(256, max(30, ⌈12·FWHM⌉)) 在生产域的偏置 ≤6.5×10⁻⁵ [实验:code/audit/route3/exp06_mask_window_cond.py]；自洽口径（通量与方差同窗）下截断必使 SNR 偏低，05 规格"恒偏绿"为符号笔误，按 A-P2-09 订正为偏低；若通量来自 P1 精确光度而方差用截断窗（S2 语义），则出现与登记数值系列同号同量级的正偏差，声称系列可部分复原（A-P2-07）[实验:code/audit/route2/exp10_truncation_window.py]。

### 4.8 集成、传递与三口径适用域

跨级传递的对拍在历史正本与本轮审计中双重完成：P2→P3 方差传播 C_out = R C_in Rᵀ 的对角元 MC 比值 0.9980；对角近似宣称的方差只有实际散度的 32.5%，故点源信息量必须按输出 PSF 与完整协方差重算 [实验:code/b5_phase3_transfer.py]。三口径（帧级标量 / 稀疏 Δ=64 / 稠密）在 testdata 两域给出互补适用域：地面视宁度受限域稀疏口径全部优于帧级标量（RMSE 0.0413–0.0825 vs 0.0506–0.1691 dex），HST 高对比域帧级标量最好且失效边界 Δ*≈16 px；稠密口径存储 64 MiB/帧超预算 64 倍，确认其诊断地位 [实验:code/b3_domain_map.py]。SNR 路径 fail-closed 状态语义经转写自检与静默降级注入判红验证 [实验:code/b6_gates_audit.py]。

## 5 结论

1. 跨帧绝对 SNR 的常数体系整体闭合：25 个登记常数全部获得三腿证据，其中全部科学量常数达到闭式或逐位复现级闭合；两处历史登记笔误（1.144 支算术漂移、"seed 无关闭式"标签）与两处符号/方向词笔误（截断方向、N=5 方向词）已按台账订正。
2. 稀疏控制点 sparse_snr_value = F_ref/σ_F 具备成为无量纲硬通货的全部要件：量纲约定被增益不变性锁定，m_ref 依赖被权重比值不变性中和，控制点精度约定（1.5%）经验证完整穿过下游重建接口。
3. 叠加权重 γ=2 由恒等式唯一确定，是定义性结论而非标定结论；k_corr 不再是单一冻结常数而由 P3 单元的几何查表承载。
4. 控制点方差的有限样本效应被定量定界且方向保守：渐近式对真方差高估（纯公式口径），生产链裁剪臂低估 1.3–3.2%（量级在预算裕量内）。

## 6 诚实边界

- **1.152 的标定登记出处**仍未补登（数值本身已由 D-04 终裁并获三路独立支持）；补登为 02 文档的待办事项，不影响本文任何数值。
- **m_ref=6.0** 为单位制锚点，无一手文献规定该值；本文按冻结纪律使用，其科学内容（每星等 2.512 倍标度与权重比值不变性）已独立验证。
- **k_corr 原始标定网格**不在登记材料内，机制腿（AR(1) 膨胀律）与构造性复原已闭合，但几何查表的具体网格属 P3 单元交付件，本文不引用其数值。
- **P-CST-23 声称系列的生成配置**欠定：登记三数可在 S2 语义下部分复原（同号同量级），完整复原需补生成配置登记。
- **生产链控制方差被估量 y 的合同语义**（裁剪后中位数 vs 全样本中位数）文档未写明，补实验按"裁剪后中位数"口径完成并上呈裁决。
- **文献腿降级项**：Serfling 1980 与 Cramér 1946 §28.5 为书目级（小节号存疑，标注后引用）；Moffat 1969 为 ADS bibcode 锚定（DOI 未核）；Aitken 1935 已核 DOI 10.1017/S0370164600014346（年份维持 1935 通行引法；出版年 INSPIRE 记 1936——卷 55 跨 1935–36，1936 通行注同文；经负责人批准进入科学文档。<!-- 订正: 检查-行文逻辑 Y6——原作「DOI 未核」，以 PHOTOMETRY.md:305 / refs.md V11 的已核状态为准统一 -->）全部核验状态见 refs.md。
- **本单元主实验的仿真坐标**（g=1.3 e⁻/ADU、RN=10 e⁻ 等）为声明量，不反解物理参数；真实数据面无解析真值，hold-out 真值自带 0.0224 dex 噪声。
- **对 MC 真值的偏置数字带 ±3 pp 蒙特卡洛噪声**（N=1000 时 σ_F 估计量相对标准误 ≈2.2%），闭式预言与实测臂比值之差（≤1.25 pp）才是低噪证据；引用具体偏置数字时应注明该不确定度。

## 参考文献

（只收一手 VERIFIED 条目；核验方式与用途的完整台账见 refs.md）

1. Pogson, N. (1856). Magnitudes of thirty-six of the minor planets. MNRAS 17, 12. DOI: 10.1093/mnras/17.1.12. [星等标度定义]
2. Aitken, A. C. (1935). On least squares and linear combination of observations. Proc. R. Soc. Edinburgh 55, 42–48. DOI: 10.1017/S0370164600014346. [逆方差加权；已核 DOI（出版年通行注 1936 同文），经负责人批准] <!-- 订正: 检查-行文逻辑 Y6 -->
3. Horne, K. (1986). An optimal extraction algorithm for CCD spectroscopy. PASP 98, 609. DOI: 10.1086/131801. [最优提取方差组成]
4. Naylor, T. (1998). An optimal extraction algorithm for imaging photometry. MNRAS 296. [成像最优提取；书目级（页码未逐字核验，不注）]
5. Rousseeuw, P. J., & Croux, C. (1993). Alternatives to the median absolute deviation. JASA 88(424), 1273–1283. DOI: 10.1080/01621459.1993.10476408. [MAD→σ 常数]
6. Croux, C., & Rousseeuw, P. J. (1992). Time-efficient algorithms for two highly robust estimators of scale. Computational Statistics, 411–428. DOI: 10.1007/978-3-662-26811-7_58. [稳健尺度算法]
7. Stigler, S. M. (1977). Do robust estimators work with real data? Ann. Statist. 5(6), 1055–1098. DOI: 10.1214/aos/1176343997. [稳健性实证]
8. Moffat, A. F. J. (1969). A theoretical investigation of focal stellar images. A&A 3, 455. bibcode: 1969A&A.....3..455M. [β=4 轮廓；bibcode 锚定，标注级（未逐页）]
9. Zackay, B., & Ofek, E. O. (2017). TRIPOLI series. ApJ 836, 187/188. arXiv:1512.06872; arXiv:1512.06879. [背景主导噪声极限与 SNR 传递的相容性]
10. Bertin, E., & Arnouts, S. (1996). SExtractor: Software for source extraction. A&AS 117, 393–404. DOI: 10.1051/aas:1996164. [背景估计与掩膜实践参照（不作为 k 值出处）]
11. Goldberg, D. (1991). What every computer scientist should know about floating-point arithmetic. ACM Comput. Surv. 23, 5. [浮点门值]
12. Janesick, J. (2001). Scientific Charge-Coupled Devices. SPIE PM83. [CCD 噪声项；标准专著]
13. Howell, S. B. (2006). Handbook of CCD Astronomy, 2nd ed. Cambridge Univ. Press. [CCD 方程；标准专著]
14. Newberry, M. V. (1991). Signal-to-noise considerations for sky-subtracted CCD data. PASP 103, 122. DOI: 10.1086/132801. [天空扣除 SNR]
15. IVOA (2017). Hierarchical Progressive Surveys, REC 1.0. DOI: 10.5479/ADS/bib/2017ivoa.spec.0519F. [hips_tile_width=512；官方 PDF 正文命中]
16. Riello, M. et al. (2020). Gaia EDR3: Photometric content and validation. A&A 649, A3. arXiv:2012.01916. [测光标定实践]
17. Pinelis, I. (2022). On the variance of the sample median. ALEA Lat. Am. J. Probab. Math. Stat. 19, 359. [样本中位数方差；全文取回]
18. Akinshin, A. arXiv:2209.12268; arXiv:2207.12005. [MAD 有限样本偏置修正因子；摘要级]
19. Stetson, P. B. (1987). DAOPHOT: A computer program for crowded-field stellar photometry. PASP 99, 191. DOI: 10.1086/131977. [逐源测光背景，链上参照]
