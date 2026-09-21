# 测光定标与 SNR 文献综述（现行结论 + 文献清单）

本文是 AstroCS 的**文献参考档案**，收录测光定标、孔径/PSF 测光、SNR 与不确定度、Gaia XP 合成测光方向的文献调查结论与引用清单（B1–B90、U1–U3）。**本文不覆盖 `docs/science/` 正本**：科学定义、公式、容差、门限与权重语义一律以 `ASTROCS_DESIGN.md`、`docs/science/UNIFIED_SCIENCE_MODEL.md`、`docs/science/PHOTOMETRY.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`、`docs/science/CONTROL_WEIGHT_SNR.md`、`docs/science/PHASE2_UPM.md`、`docs/science/NOISE_MODEL.md`、`docs/science/UNCERTAINTY_AND_COVARIANCE.md` 为准。引文的作者、年份、标题、期刊/卷/页、DOI/arXiv/URL 按原文保留；核验标签 [V]/[S]/[U] 的含义见 §2。

---

## 1. 现行结论

### 1.1 定标链：合成测光 + 零点拟合

- 现行定标链是**合成测光 + 零点拟合**：用 **CCD QE 曲线 × 滤镜透过率 × λ**（即 `T(λ)·Q(λ)·λ`）定义仪器有效通带；用 **Gaia DR3 XP 光谱**在该通带下合成每颗场星的期望通量 `F_syn`；把 PSF 拟合得到的仪器通量 `F_instr` 对 `F_syn` 做 **IRLS/Tukey 稳健零点拟合**，得到 `location` 与 `scale = 10^{−location}`，再以 `I_cal = I·scale` 应用到整帧像素。
- 文献上这条链叫 **synthetic photometry based calibration（合成测光定标）**：Gaia DR3 XP 提供 330–1050 nm 的通量定标低分辨率分光光度，**完全落在该波长范围内的任何通带**都可由此得到直接锚在物理单位通量上的合成测光 [B5]；S-PLUS / J-PLUS 用同一思路（改进的 XP 合成测光 XPSP + Stellar Color Regression）把巡天数据重定标到 Gaia XP 绝对分光刻度 [B6][B7]；Rubin/LSST 亦由 Gaia XP 光谱合成星等 [B36]。
- 文献中「归一化到测光坐标系」= **photometric zero point（零点定标）+ color term（色项）+ spatial zero-point variation（空间依赖零点）**：选一批已知星等/已知合成通量的参考星，拟合 `m_std − m_instr = ZP + c·(color) + s(x,y)`，再把 ZP 转移到科学帧 [B1][B2][B28][B29]。现行合同覆盖**单标量零点**与 Phase1 的**低阶乘性空间增益**；带通外颜色项高阶效应不在合同范围（仅由 QA 残差分布暴露）。
- 参考端边界：XP 覆盖 330–1050 nm、低分辨率实测 `R ≈ 20–80`，**只有完全落在该范围的通带可复现** [B73]；约 2.2 亿源有平均谱（以 `G < 17.65` 为主），`9 < G < 12` 的源在 RP 中心波段 S/N 可达 1000，`G = 15` 时部分波段 `S/N > 100` [B38]；星表零点只适用于 Gaia 的 e⁻/s 通量，用 XP 谱做合成测光必须用合成通带重算零点 [B71][B72]。
- 参考星必须与科学目标使用**同一提取定义**：零点只吸收两者共同的乘性因子；参考星是亮、孤立、未饱和的 Gaia 星，科学目标可能暗、拥挤、欠采样或饱和，两者提取因子不一致时单标量零点无法消除该偏差 [B10][B17]。

### 1.2 可测量与不可测量

**可测量（现有输入可算出）**

- `F_instr`（PSF 拟合总通量，ADU；e⁻ 需 gain，当前不可得）、`F_syn`（模型通带内合成通量）、`r_i = log10(F_instr/F_syn)`；
- `location = med_i(r_i) = log10 κ + med_i(ε_i)`：κ 为系统吞吐的乘积（曝光 × gain⁻¹ × 有效口径 × 像素立体角 × …，帧/滤镜内常数），ε 为通带失配项；
- `scale = 10^{−location}` 与 `I_cal = I·scale`；
- `sigma_residual = MAD(r_inliers)/0.6744897501960817`（参考星样本的**定标散度**，含通带失配 + 参考星噪声 + 空间零点变化）、`sigma_mag = 2.5·sigma_residual`、`sigma_cal_rel = ln10·sigma_residual`；
- `q_psf`、`residual_scale`、PSF 形状/位置/通量（诊断量）；
- `variance/ivar`（空背景随机分量，逐像素；见 `docs/science/NOISE_MODEL.md`）。

**不可测量（原理上不能由这条链闭合）**

- **系统吞吐 κ 的分解**：曝光、gain⁻¹、有效口径、像素立体角等只能得乘积（= 零点）；把 ADU 换成物理量不是「换不到」，而是只能换到「κ·物理量」，κ 由零点一次性吸收；
- **未建模响应项的真值**：光学系统透过率、大气消光、滤镜实际曲线与名义曲线之差——它们进入 `ε_i`，无法与 `log10 κ` 分离；
- **科学目标自身的 SED 依赖偏差** `ε_target − med(ε)`：单标量零点不能消除，只能靠相似 SED 样本或准确通带控制；
- **观测波段外**（XP 仅 330–1050 nm）或通带未被完全覆盖时的通量 [B5]；
- **另一套标准系统**（Johnson/SDSS…）的星等（需通带定义 + 色项）；
- **时间分辨的吞吐变化**（灰尘、镜面、探测器老化）——需重复标准星观测。

### 1.3 单位与量纲

- `F_instr`：ADU（e⁻ 需 gain，当前不可得）；`F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`：W·m⁻²·nm（模型通带积分辐照度）；二者**不同量纲**。
- `r_i = log10(F_instr/F_syn)` 是**有量纲比值**的对数：`location` 单位为 **dex(ADU/[F_syn 单位])**，`scale = 10^{−location}` 单位为 **[F_syn 单位]/ADU**；`sigma_mag`、`delta` 单位为 mag；`sigma_cal_rel` 为相对误差。
- 标定因子的**绝对值无物理意义**：它把增益/口径/曝光等未知量全部吸收；产物通量以**星等/相对星等**表达，消除物理单位。禁止用任何物理闭合式（如 `k = g·h·c·1e9/(A·t)`）反推仪器参数。
- 通带被积函数含 `λ`，隐含光子计数约定；**必须声明透过率曲线是光子曲线还是能量曲线** [B73]，否则有效通带随约定偏移。
- 模型通带 `P_model = T(λ)·Q(λ)·λ` **不含光学系统透过率与大气消光**（显式未建模项）；`Q(λ)` 缺失时按 `Q(λ) ≡ 1` 处理，同样记为显式未建模项。
- 响应曲线输入（QE/滤镜）须可追溯：记录来源、URL、仪器型号与不确定度说明，否则模型通带声明缺可追溯输入。

### 1.4 精度量级与系统误差

- 合成测光定标精度：XP **未标准化**时「几 %」（宽/中带），经**外部测光标准化**后可到 mmag 级 [B5]；XPSP 与 SCR 两种独立方法的零点差 **1–6 mmag（0.1–0.6%）**，零点精度改善 2–3 倍 [B6][B7]。
- 系统误差：未矫正的 XP 系统差约 **10 mmag**；空间依赖系统差可达 **23 mmag（~2%）** [B6]；XP 合成与观测的残差离散仅 **1.07 / 0.55 / 1.02 mmag**（BP / RP / G），但呈与 Gaia 扫描律相关的**空间图案** [B87]；亮端 `G < 11` 存在超出估计不确定度的系统变化，且 BP/RP 谱因**长程相关噪声**出现 wiggle [B38]；合成测光与观测的固有系统差 **≤ 1%** [B88]。
- 经验改正可把 XP 合成残差降一个量级（u 波段中位残差 0.038 → 0.002 mag，散布 0.2 → 0.07 mag）[B36]。
- 绝对刻度：Gaia XP 绝对通量刻度锚在 HST/CALSPEC 白矮星标准，后者在 1500 Å–30 μm 内保持 **1% 一致** [B4]；官方定调 "1% is thought to be the current state-of-the art uncertainty on the 'absolute' calibration scales" [B71]。**绝对精度上限 ≈ 1%**，现实精度由**通带失配**支配 [B5]。以上是 XP 刻度本身的绝对精度；现行合同不据此宣称绝对通量刻度——标定常数由零点吸收，产物以星等/相对星等表达（`docs/science/PHOTOMETRY.md` §1/§6）。
- Gaia 自身测光：G 带中位不确定度 0.2 mmag（`G = 10–14`）、0.8 mmag（`G ≈ 17`）、2.6 mmag（`G ≈ 19`），单通带全颜色范围系统差 < 1% [B3]；零点定义取 α Lyr 在 550.0 nm 的 `f_550 = 3.62286e-11 W m⁻² nm⁻¹`、`V = 0.023` [B71]。
- 相对测光（无标准星、无 QE 曲线）可达 < 10 mmag（PS1 gri）、~1%（SDSS griz）、~2%（u）[B28][B54][B59]；帧间零点闭合 ~1% [B28][B54]；仪器星等转到标准系统的内部一致 1–6 mmag，通带失配为主因 [B1][B6][B7][B29]。
- 大气消光未建模时留下 **~1–10%** 的 airmass/天气相关系统差；单帧内被零点吸收，跨帧成为帧间系统差 [B28][B29]。
- 不确定度传递（推荐形式）：`σ_total² = σ_κ,stat² + Var(ε) + σ_XP,abs² + σ_ext² + σ_flat² + σ_bkg² + σ_extract²`，其中 `Var(ε)`（通带失配）通常是主项，`σ_XP,abs ≈ 1%` [B4]。
- `sigma_residual` 是**逐星定标散度**，不是零点标准误：对 N 个参考星的稳健零点，标准误约 `1.253·sigma_residual/√N_eff`（高斯下 median 的 SE；非高斯建议 bootstrap）。报告时必须区分「逐星定标散度」与「零点标准误」。

### 1.5 孔径测光 vs PSF 测光

- 现行生产主路径是 **PSF 拟合通量**（全链唯一 `flux` 口径）；**孔径测光只作显式诊断**。
- 提取方法判据：
  - **孤立、未饱和、FWHM ≳ 2.5–3 px、半径 ≥ 2–3×FWHM + 孔径改正**：孔径足够（~1%）[B1][B10][B17]；
  - **欠采样（FWHM ≲ 1.5–2 px）**：孔径流量分数随亚像元相位变化几 %，必须用 **ePSF** [B13]；FWHM ≲ 2 px 时中心定位/PSF 拟合/插值开始失效 [B75]；PSF 中心像素 1% 形状误差 → **0.007 mag**，最坏 1.8% → **0.013 mag** [B14]；
  - **拥挤/混合**：必须 PSF 拟合去混合 [B10][B11][B14]；未校正 PSF 库偏差达 **0.15–0.25 mag**（WFPC2/WF-PC）[B14][B41]；轮廓 1% 误差 → 2.5 mag 暗伴星 ~10% 通量误差 [B10]；PSF 邻星减除后 Kepler/K2 星团暗端 10% @ `K_P ≈ 24`、亮端 ~30 ppm [B74]；
  - **饱和/非线性**：两者都要显式剔除 [B32]；
  - **空间变化 PSF**：位置相关零点可达 **±0.02 mag**、彩色残差 **±0.01 mag** [B77]；
  - **已被 drizzle/重采样**：孔径方差必须含协方差（见 §1.7）[B22][B43][B44]。
- 灵敏度：成像中 PSF 加权相对普通孔径的 S/N 增益只有 **~10%** [B17]，最优孔径 ≈ FWHM [B18]；PSF 法的主要收益是**偏差控制**（拥挤、去混合、错误 PSF 的鲁棒性）。
- 提取方法定量对照（Moffat4 `β = 4`，FWHM `= 1.230310σ`；`σ_sky = 10` ADU/px）：

  | FWHM(px) | SNR_opt/SNR_peak | SNR_ap(1.5FWHM)/SNR_opt | SNR_ap(2FWHM)/SNR_opt | flux_out(1.5FWHM) | flux_out(2FWHM) | flux_out(3FWHM) |
  |---|---|---|---|---|---|---|
  | 1.5 | 1.211 | 0.616 | 0.480 | 5.06% | 1.53% | 0.21% |
  | 2.0 | 1.548 | 0.639 | 0.497 | 5.06% | 1.53% | 0.21% |
  | 2.5 | 1.926 | 0.641 | 0.499 | 5.06% | 1.53% | 0.21% |
  | 3.0 | 2.310 | 0.642 | 0.499 | 5.06% | 1.53% | 0.21% |
  | 4.0 | 3.080 | 0.642 | 0.499 | 5.06% | 1.53% | 0.21% |

  读法：用峰值型量代替最优提取量，FWHM 越大低估越多（1.2 → 3.1 倍）；孔径 `r = 1.5` FWHM 只有最优提取的 ~64%，`r = 2` FWHM 时 ~50%；Moffat4 `β = 4` 的孔径改正本身为 5%（1.5 FWHM）/ 1.5%（2 FWHM）/ 0.2%（3 FWHM）——**若要用孔径，半径必须 ≥ 3 FWHM 或显式做孔径改正**。
- 孔径改正实测锚：SExtractor 取半径 `k·r_Kron`，`k = 2` 时 ≥ 90% 通量落在孔径内，`k = 2.5` 时平均损失从 ~10% 降到 6%（以 SNR 为代价），MAG_AUTO/MAG_ISOCOR 约 0.06% [B25]；孔径改正随 SED 变化（近红外 PSF 显著变宽）[B50]；photutils 不提供 aperture-correction API，孔径改正须自建 [B27]。
- 帧级「SNR」在巡天文献里以**深度**表达：LSST 单次 visit 5σ 点源深度约 `r ~ 24.5` [B31]；HSC 分层深度 `i ~ 26.4/26.5/27.0`（点源 5σ）[B30]。
- 与合成测光（SED 积分）比对时，PSF 总通量定义更接近「模型总通量」[B5][B6]。

### 1.6 SNR 与不确定度模型

- **逐源科学 SNR** 由逐源通量不确定度定义：`SNR_F = F/σ_F`。PSF 加权最优提取 `σ_F^{−2} = Σ_i P_i²/σ_i²`（P 为归一化轮廓，`ΣP_i = 1`）[B16]，Zechmeister 等 2013 逐字复述该式 [B34]；均匀 σ 时 `SNR_F = F·√(Σ_i P_i²)/σ`；相关噪声时 `σ_F² = Σ_ij P_i P_j C_ij` [B22][B24]。
- **孔径 SNR** 用 CCD 方程：`SNR_ap = S_ap / sqrt( S_ap/g + n_ap·σ_sky²·(1 + n_ap/n_sky) + n_ap²σ_sky²/n_sky + … )`，并需孔径改正 [B18][B19][B35]；广义形式 `S/N = N_* / sqrt( N_* + n_pix(N_S + N_D + N_R² + G²σ_f²) )`（各量 e⁻，`G` 单位 [e⁻/ADU]）；噪声按 e⁻ 平方相加**仅当互不相关** [B35]；标准公式未正确处理归算过程引入的噪声 [B19]。
- **帧级科学基准 = 5σ 点源深度** `m_5 = ZP − 2.5·log10(5·σ_F(ref))`，`σ_F(ref)` 必须**显式绑定**参考轮廓/孔径/背景 [B29][B30][B31]；空间变化时应给**深度图**而非单标量。
- **Phase1 产品面帧级 SNR** 是科学量：点源（PSF）信号 SNR，纯信号/噪声——`SNR = F_signal/σ_F`，`F_signal` 已扣局部背景，天光**只作噪声项**进入 `σ_F`；固定源通量下天光增大 ⇒ SNR 单调下降。
- **Phase2 stage2 内部的 `local_snr` / `frame_snr` 是相对质量权重场**（`quality_weight = frame_quality_scalar × local_quality_proxy/median`），无量纲，仅供采样/加权；**不是**科学 SNR，不得解释为 `m_5` 或 `SNR_F`。PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，**不计入科学叠加权重**。
- **权重**是 Phase2 集成时按天球像素对应的输入帧集合**现场计算的派生量**（逆方差 `w = 1/σ² = SNR²/F_ref²`，即 point information 最优集成，不是直接用 SNR 加权）；Phase1 与 Phase3 不产生、不消费权重。
- 由帧级定标散度与逐星拟合质量代理构成的相对质量场**不是** SNR：它只反映参考星群体的定标散度（主要是通带失配）与星点形状，**完全不含科学目标的亮度/噪声**。数值锚：`σ_dex = 0.05` 时该标量为 **8.69**，而同一亮度序列的真·最优提取 SNR 为 22.3（`F = 1e3` ADU）→ 22270.7（`F = 1e6` ADU），比值从 **0.390 到 0.0004**（跨 3 个数量级）；该标量只随 `sigma_residual` 变化（0.005 → 86.86、0.010 → 43.43、0.020 → 21.71、0.050 → 8.69、0.100 → 4.34、0.200 → 2.17）。
- 峰值型量（如 `A/σ`）代替最优提取量典型低估 **20%–70%**；用常数代表全帧随亮度可差 **10²–10³**；除以样本中位数会使结果依赖样本构成（星表一变整体缩放）。
- **`variance` 只含空背景随机分量，不含源泊松**：逐像素 `signal/√variance` 是探测显著性，不是源通量 SNR；亮源逐像素「SNR」会被高估 `√(1 + F/(n σ²))`。
- 背景不确定度是**空间场**（SExtractor 网格背景 [B25]）；孔径/annulus 的 `n_sky` 项必须进入 CCD 方程 [B18][B19][B35]。
- PSF 拟合协方差：独立像素 `Cov(θ) = (AᵀWA)⁻¹, W = diag(1/σ_i²)` [B33]；相关像素必须 `W = C⁻¹`、`Var(Σ w_i d_i) = Σ_ij w_i w_j C_ij` [B22][B24]。
- 平场/非线性/CTE/电荷弥散：WFPC2 的 CTE 损失从 ~3% 增至 ~40% [B32]；电荷弥散使 PSF 变宽/高斯化。

### 1.7 相关噪声、重采样与多帧叠加

- Drizzle 把输入像素功率分配到多个输出像素，**相邻像素噪声因此相关** [B22]；Fruchter & Hook 2002 §7 在 **`pixfrac = 0.6`、`scale = 0.5`** 的算例给出噪声相关比 **`R = 1.662`**，即按像元方差直接相加会把孔径/分块噪声**低估约 1.66 倍**（方差低估约 2.8 倍）[B22]。
- 仓内 MC 表征（`docs/science/UNCERTAINTY_AND_COVARIANCE.md`）：nside = 512 合成帧相邻像素 `mean|ρ| ≈ 0.19`、`max|ρ| ≈ 0.57`；**pixel variance ≠ aperture variance**，孔径误差须显式加入 `Cov` 项。
- 实用替代：Bickerton & Lupton 2013 的**噪声有效面积** `Σ_i w_i²` [B43]；STScI DrizzlePac Handbook §3.3 建议使用 drizzle 权重图 [B44]。
- 多帧叠加只有在像素噪声**不相关**时才有 `σ_N = σ_1/√N`；相关时不能直接用逐像素方差相加 [B22][B24]。
- 定标误差的科学放大：CALSPEC 基本流量定标变化 ~1.5%（`Δλ = 4000` Å）对应 `dμ/dz` 变化 **0.04 mag**（`0 < z < 1`）[B89]。
- 时序孔径（系综）可达 0.0015–0.002 mag/曝光（12–13 等、1 min）[B78][B79]；孔径改正依赖 SED 与 seeing [B50]。

### 1.8 结论成立的前提

- **定标结论**成立的前提：使用官方绝对定标的 XP 光谱、通带完全落在 330–1050 nm、接受通带/大气未建模项。此时 XP 合成测光可直接锚在物理单位通量上 [B5]；残余不闭合的是**通带与消光**，不是绝对能级本身。若 XP 谱未绝对定标或通带不覆盖，则退化为「不能」。
- **孔径结论**成立的前提：目标孤立、未饱和、FWHM ≳ 2.5–3 px 且做了孔径改正；强背景梯度、相关噪声、强平场残差会放大误差。启用孔径路径时必须带孔径改正与显式场景标志（欠采样/拥挤/饱和/非线性/背景梯度）。
- **PSF 结论**成立的前提：PSF 模型与真实星像形状匹配。Moffat `β ≈ 4.765` 最接近湍流理论，偏离高斯会使轮廓参数变化 10–30% [B76]；真实星像有核心 + 指数 + 反平方 aureole 翼，高斯模型系统性缺少翼 [B42]。
- **相关噪声结论**成立的前提：存在重采样/卷积；`pixfrac = 1` 且无重采样时相关性可忽略。
- **帧级量结论**成立的前提：`σ_F(ref)` 有显式参考源定义；否则只能给相对质量权重场，不能称 SNR。
- **验证要求**：用真实 DR3SP + 已知标准场做零点交叉验证（同一天区不同夜/不同滤镜），量化大气项与空间零点漂移。

---

## 2. 文献清单

> 核验级别：**[V]** = 已通过 arXiv/Crossref/DataCite 取到元数据与 URL 并核对；**[S]** = 以同样方式核对；**[U]** = 未核实，仅列出、不作为结论依据。作者、年份、标题、期刊/卷/页、DOI/arXiv/URL 按原文保留。未取得逐字摘录的正文：Moffat 1969、King 1971、Naylor 1998、Stetson 1987 的宇宙线段落；天空梯度、宇宙线、颜色项到 mmag 的单篇定量未定。

**定标 / 合成测光 / 绝对通量**

- [B1] Bessell, M. & Murphy, S. 2012, PASP 124, 140, "Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI..." DOI 10.1086/664083 [V]
- [B2] Landolt, A. U. 1992, AJ 104, 340, "UBVRI photometric standard stars..." DOI 10.1086/116242 [V]
- [B3] Riello, M., De Angeli, F., Evans, D. W., et al. 2021, A&A 649, A3 (Gaia EDR3 photometric content) DOI 10.1051/0004-6361/202039587 [V]
- [B4] Bohlin, R., Hubeny, I. & Rauch, T. 2020, arXiv:2005.10945（WD NLTE 模型 + HST/STIS 通量标定；1% 一致 FUV–mid-IR）[V]
- [B5] Gaia Collaboration, Montegriffo, P., et al. 2023, A&A 674, A33, "Gaia DR3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra" DOI 10.1051/0004-6361/202243709, arXiv:2206.06215 [V]（A33 卷期已核对；外部定标文见 [B52]）
- [B6] Xiao, K., Huang, Y., Yuan, H., et al. 2023, "S-PLUS: Photometric Re-calibration with the Stellar Color Regression Method and an Improved Gaia XP Synthetic Photometry Method" arXiv:2309.11533 [V]
- [B7] Xiao, K., Yuan, H., López-Sanjuan, C., et al. 2023, "J-PLUS: Photometric Re-calibration ..." arXiv:2309.11225 [V]
- [B8] Yuan, H., Liu, X. & Xiang, M. 2015, ApJ 799, 133, "Stellar Color Regression..." DOI 10.1088/0004-637x/799/2/133 [V]
- [B9] Xiao, K. & Yuan, H. 2022, AJ 163, 185, DOI 10.3847/1538-3881/ac540a [V]
- [B28] Schlafly, E. F., Finkbeiner, D. P., Jurić, M., et al. 2012, "Photometric Calibration of the First 1.5 Years of the Pan-STARRS1 Survey" arXiv:1201.2208 [V]
- [B29] Tonry, J. L., Stubbs, C. W., Lykke, K. R., et al. 2012, "The Pan-STARRS1 Photometric System" arXiv:1203.0297 [V]
- [B36] Razim, O., Tisanič, K. & Palaversa, L. 2026, A&A, "Vera C. Rubin LSST Synthetic Magnitudes derived from Gaia XP Spectra" DOI 10.1051/0004-6361/202555722, arXiv:2608.17922 [V]
  （原文："the median residuals decrease by an order of magnitude (e.g., for the u band the improvement is from 0.038 to 0.002 mag), and the standard deviation of residuals typically becomes up to factor of two smaller (e.g., for the u band from 0.2 to 0.07 mag)."）
- [B37] Gaia Collaboration (Brown, A. G. A., Vallenari, A., et al.) 2021, A&A 649, A1, "Gaia Early Data Release 3" DOI 10.1051/0004-6361/202039657 [V]
- [B38] De Angeli, F., Weiler, M., Montegriffo, P., et al. 2023, A&A 674, A2, "Gaia DR3: Processing and validation of BP/RP low-resolution spectral data" DOI 10.1051/0004-6361/202243680, arXiv:2206.06143 [V]

**孔径 vs PSF / 提取方法**

- [B10] Stetson, P. B. 1987, PASP 99, 191, "DAOPHOT — A computer program for crowded-field stellar photometry" DOI 10.1086/131977 [V]
- [B11] Irwin, M. J. 1985, MNRAS 214, 575, "Automatic analysis of crowded fields" DOI 10.1093/mnras/214.4.575 [S]
- [B13] Anderson, J. & King, I. R. 2000, PASP 112, 1360, "Toward High-Precision Astrometry with WFPC2. I." DOI 10.1086/316632 [V]
- [B14] Dolphin, A. E. 2000, PASP 112, 1383, "WFPC2 Stellar Photometry with HSTphot" DOI 10.1086/316630 [V]
- [B16] Horne, K. 1986, PASP 98, 609, "An optimal extraction algorithm for CCD spectroscopy" DOI 10.1086/131801 [V]
- [B17] Naylor, T. 1998, MNRAS 296, 339, "An optimal extraction algorithm for imaging photometry" DOI 10.1046/j.1365-8711.1998.01314.x [V]
- [B25] Bertin, E. & Arnouts, S. 1996, A&AS 117, 393, "SExtractor: Software for source extraction" DOI 10.1051/aas:1996164 [V]
- [B26] Merlin, E., Pilo, S., et al. 2019, A&A 622, A169, "A-PHOT: a new, versatile code for precision aperture photometry" DOI 10.1051/0004-6361/201833991 [V]
- [B27] Bradley, L., et al. 2022, "astropy/photutils: 1.6.0" Zenodo DOI 10.5281/zenodo.7419741 [V]

**SNR / 不确定度 / 噪声**

- [B18] Howell, S. B. 1989, PASP 101, 616, "Two-dimensional aperture photometry — Signal-to-noise ratio..." DOI 10.1086/132477 [V]
- [B19] Newberry, M. V. 1991, PASP 103, 122, "Signal-to-noise considerations for sky-subtracted CCD data" DOI 10.1086/132801 [V]
- [B20] Mortara, L. & Fowler, A. 1981, SPIE Proc. 290, 28, DOI 10.1117/12.965833 [V]
- [B21] Merline, W. J. & Howell, S. B. 1995, Exp. Astron. 6, 163, DOI 10.1007/BF00421131 [S]
- [B22] Fruchter, A. S. & Hook, R. N. 2002, PASP 114, 144, "Drizzle..." DOI 10.1086/338393 [V]
- [B23] Casertano, S., et al. 2000, AJ 120, 2747, "WFPC2 Observations of the Hubble Deep Field South" DOI 10.1086/316851 [V]
  （用途说明：相关噪声请引 [B22][B24]；可核实的 Casertano 相关工作见 [B32]。）
- [B24] Zackay, B., Ofek, E. O. & Gal-Yam, A. 2016, ApJ 830, 27, arXiv:1601.02655 [V]
- [B32] Whitmore, B. C., Heyer, I. & Casertano, S. 1999, PASP 111, 1559, "Charge-Transfer Efficiency of WFPC2" DOI 10.1086/316475 [V]
- [B33] Lampton, M., Margon, B. & Bowyer, S. 1976, ApJ 208, 177, "Parameter estimation in X-ray astronomy" DOI 10.1086/154592 [V]
- [B34] Zechmeister, M., Anglada-Escudé, G. & Reiners, A. 2013, A&A 561, A59, DOI 10.1051/0004-6361/201322746（逐字复述 Horne 方差式）[V]
- [B35] Hainaut, O. 2005, ESO 讲义 "Signal, Noise and Detection" https://www.eso.org/~ohainaut/ccd/sn.html [S]
- [B39] Howell, S. B. 2006, "Handbook of CCD Astronomy", 2nd ed., CUP, DOI 10.1017/CBO9780511807909 [S]
- [B40] Janesick, J. R. 2001, "Scientific Charge-Coupled Devices", SPIE Press, DOI 10.1117/3.374903 [S]

**提取与场景补充**

- [B41] Schechter, P. L., Mateo, M. & Saha, A. 1993, PASP 105, 1342, "DoPHOT..." DOI 10.1086/133316 [S]
- [B42] King, I. R. 1971, PASP 83, 199, "The Profile of a Star Image" DOI 10.1086/129100 [S]
- [B43] Bickerton, J. W. & Lupton, R. H. 2013, MNRAS 431, 1275, arXiv:1302.4764 [S]
- [B44] STScI DrizzlePac Handbook §3.3 "Weight Maps and Correlated Noise" https://hst-docs.stsci.edu/spaces/DRIZZPAC/pages/148007055/ [S]
- [B45] Blakeslee, J. P., et al. 2003, ASP Conf. Ser. 295 (ADASS XII), arXiv:astro-ph/0212362 [S]（APSIS = ACS GTO 自动处理流水线）
- [B46] Grogin, N. A., et al. 2011, ApJS 197, 35（CANDELS）[S]
- [B47] Bosch, J., et al. 2018, PASJ 70, S5, arXiv:1705.06766 [S]
- [B48] Euclid Collaboration (Cuillandre, J.-C., et al.) 2024, arXiv:2405.13496 [S]
- [B49] Barbary, K. 2016, JOSS 1, 58, "SEP" DOI 10.21105/joss.00058 [S]
- [B50] Sirianni, M., et al. 2005, PASP 117, 1049, DOI 10.1086/444553 [S]
- [B51] Bessell, M. S. 1990, PASP 102, 1181, "UBVRI passbands" DOI 10.1086/132749 [S]

**定标 / Gaia XP / 绝对通量（续）**

- [B52] Montegriffo, P., De Angeli, F., Andrae, R., Riello, M., et al. 2023, A&A 674, A3, "Gaia DR3: External calibration of BP/RP low-resolution spectroscopic data" DOI 10.1051/0004-6361/202243880, arXiv:2206.06205 [V]
- [B54] Padmanabhan, N., et al. 2008, ApJ 674, 1217, "An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data" arXiv:astro-ph/0703454 [S]
- [B55] Stetson, P. B. 2000, PASP 112, 925, DOI 10.1086/316595 [S]
- [B56] Landolt, A. U. 2009, AJ 137, 4186, arXiv:0904.0638 [S]
- [B57] Fukugita, M., et al. 1996, AJ 111, 1748, DOI 10.1086/117915 [S]
- [B58] Regnault, N., et al. 2009, A&A 506, 999, arXiv:0908.3808 [S]
- [B59] Huang, Y., Xiao, K. & Yuan, H. 2022 (photometric calibration review) arXiv:2206.01007 [S]
- [B60] López-Sanjuan, C., et al. 2023, "J-PLUS: Towards an homogeneous photometric calibration using Gaia BP/RP low-resolution spectra" arXiv:2301.12395 [V]
- [B61] Castander, F. J., et al. 2024 (PAU 窄带) arXiv:2406.06850 [S]
- [B62] Rodrigo, C., et al. 2024 (SVO FPS) arXiv:2406.03310 [S]
- [B63] Bohlin, R. C., Gordon, K. D. & Tremblay, P.-E. 2014, PASP 126, 711, DOI 10.1086/677655 [S]
- [B64] Bohlin, R. C., et al. 2019, AJ 158, 211, DOI 10.3847/1538-3881/ab480c [S]
- [B65] Bohlin, R. C., et al. 2024 (CALSPEC 扩充) arXiv:2411.09049 [S]
- [B66] Altavilla, G., et al. 2021 (Gaia SPSS IV) arXiv:2011.08625 [S]
- [B67] Pancino, E., et al. 2012 (Gaia SPSS I) arXiv:1207.6042 [S]
- [B68] Jordi, C., et al. 2010, A&A 523, A48, arXiv:1008.0815 [S]
- [B69] Rieke, G. H., et al. 2008, AJ 135, 2245, arXiv:0806.1910 [S]
- [B70] Carrasco, J. M., et al. 2021 (XP 内部定标) arXiv:2106.01752 [S]
- [B71] ESA Gaia DR3 官方文档 §5.4.1 外部定标（通带+零点） https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html [S]
- [B72] ESA Gaia DR3 gaia_source 数据模型（phot_g_mean_flux 单位 e⁻/s） https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_main_source_catalogue/ssec_dm_gaia_source.html [S]
- [B73] GaiaXPy 官方文档（DPAC/CU5/DPCI） https://gaiaxpy.readthedocs.io/en/latest/description.html ； https://gaiaxpy.readthedocs.io/en/latest/cite.html [S]

**分场景定量补充**

- [B74] Libralato, M., et al. 2016, MNRAS 456, 1137, DOI 10.1093/mnras/stv2628 [S]
- [B75] Bakos, G. Á., et al. 2004, PASP 116, 266, DOI 10.1086/382735 [S]
- [B76] Trujillo, I., et al. 2001, MNRAS 328, 977, DOI 10.1046/j.1365-8711.2001.04937.x [S]
- [B77] Anderson, J., et al. 2008, AJ 135, 2055, DOI 10.1088/0004-6256/135/6/2055 [S]
- [B78] Gilliland, R. L. & Brown, T. M. 1988, PASP 100, 754, DOI 10.1086/132232 [S]
- [B79] Everett, M. E. & Howell, S. B. 2001, PASP 113, 1428, DOI 10.1086/323387 [S]
- [B80] van Dokkum, P. G. 2001, PASP 113, 1420, DOI 10.1086/323894 [S]
- [B81] Stetson, P. B. 1990, PASP 102, 932, DOI 10.1086/132719 [S]
- [B82] Stetson, P. B. 1994, PASP 106, 250, DOI 10.1086/133378 (ALLFRAME) [S]
- [B83] Nardiello, D., et al. 2022, MNRAS 517, 484, DOI 10.1093/mnras/stac2659 [S]
- [B84] Su, K. Y. L. & Rieke, G. H. 2022, AJ 163, 46, DOI 10.3847/1538-3881/ac3b5e [S]
- [B85] Kjeldsen, H. & Frandsen, S. 1992, PASP 104, 413, DOI 10.1086/133014 [S]
- [B86] Anderson, J. 2016, "Empirical Models for the WFC3/IR PSF", STScI WFC3 ISR 2016-12（无 DOI）[S]

**孔径改正 / 通带颜色项 / Gaia XP 追加**

- [B87] Huang, Y., Yuan, H. & Xiao, K. 2024, ApJ 973, 1, arXiv:2408.09779（XP 合成 vs 观测残差的空间系统）
- [B88] Stritzinger, M., et al. 2005, "An Atlas of Spectrophotometric Landmark/Landolt Standard Stars" DOI 10.1086/431468, arXiv:astro-ph/0504244
- [B89] Brout, D., et al. 2022, ApJ 938, 111（CALSPEC 1.5% 变化 → 0.04 mag 的 dμ/dz 放大）
- [B90] Gaia Collaboration, Vallenari, A., Brown, A. G. A., et al. 2023, A&A 674, A1, "Gaia Data Release 3: Summary of the content and survey properties" DOI 10.1051/0004-6361/202243940, arXiv:2208.00211

**深度 / 巡天策略**

- [B30] Huang, S., Leauthaud, A., Murata, R., et al. 2017, PASJ 70, DOI 10.1093/pasj/psx126 [V]
- [B31] Ivezić, Ž., et al. 2019, ApJ 873, 111, arXiv:0805.2366 [S]

**未核实（不作为结论依据）**

- [U1] GaiaXPy 没有独立同行评审方法学论文（官方文档 https://gaiaxpy.readthedocs.io/en/latest/description.html 与 https://gaiaxpy.readthedocs.io/en/latest/cite.html ，当前版本 2.1.4）；正确引用方式是 Gaia DR3 官方论文 [B5][B38] + 软件版本 [B73]。
- [U2] `filter_qe_provenance.json` 中各 QE/滤镜曲线的来源（文件本身不含 provenance）。
- [U3] "Casertano et al. 2000 相关噪声"（见 [B23]）。
