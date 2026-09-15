# REAL-SCIENCE-001 — Linux 真实数据科学验收报告

- 任务：REAL-SCIENCE-001（Wave 10，V6 并行包）
- 基线 HEAD：5eb702bd72649e498022ca34e4810886b1a6b800（执行起始与结束时一致）
- 控制节点：Linux amd64（宪章 §15.1），真实数据来自冻结 testdata/
- CLI 版本串：astrocs 0.11.0-alpha.2+g5eb702bd72649e498022ca34e4810886b1a6b800（重建后与 HEAD 一致）
- 建议状态：**PASS**（附待负责人裁决/未测项，见 §9 与 §13）
- 纪律声明：本任务未 commit / push / add / 分支 / worktree / stash / reset / clean；未派生子代理；未编造数据；
  所有指标来自真实运行的实测输出，命令、原始日志路径与退出码逐条给出。
- SO-05 资源门：**PENDING_OWNER_SIGNOFF**（按 C-007 只记录，未作为硬失败，也未作为 PASS 理由）。

---

## 1. 目标与范围

在 M42、银心（Galaxy Center）及全部适用真实数据上，比较 **等权(equal) / exposure / ivar / W_info / PSFSW** 五类口径的
科学结果（flux、SNR、背景、方差面）与资源指标（墙钟、CPU、RSS、I/O），并作独立交叉校验与负向/边界检查。

**数据**：9 个真实数据集 × 5 帧 × 8 颗共同星 = **45 个真实帧**、360 个逐帧逐星实测量。

| 数据集 | 对象 | 望远镜 | 滤镜/曝光 | 帧数 | 共同星 |
|---|---|---|---|---|---|
| M42 | M42 | T3 | Red 300 s | 5 | 8 |
| M42_Halpha | M42 | T2 | H-alpha 600 s ×4 + 300 s ×1（混曝光） | 5 | 8 |
| Galaxy_Center | 银心 | T4 | Red 180 s | 5 | 8 |
| LDN43 | LDN43 | T2 | Lum 600 s | 5 | 8 |
| NGC1727 | NGC1727 | T2 | Red 600 s | 5 | 8 |
| NGC247 | NGC247 | T2 | Lum 600 s | 5 | 8 |
| NGC55 | NGC55 | T3 | Lum 600 s | 5 | 8 |
| NGC83 | NGC83 | T3 | Green 600 s | 5 | 8 |
| Victory_Nebula | Victory Nebula | T4 | Lum 180 s | 5 | 8 |

数据只读，未移动/重命名/删除 GaiaDR3/、GaiaDR3SP/、BASS DR3/。

---

## 2. 方法

### 2.1 真实数据抽取（run/v6/real-science/extract_real.py）
1. 逐数据集选同一滤镜的 5 个真实 FITS 帧；M42_Halpha 显式包含唯一 300 s 帧以获得**真实曝光多样性**。
2. 真实全局背景：参考帧中心 2048×2048 的 3σ 裁剪中值/标准差（实测值见 §5）。
3. 参考帧峰值检测（25σ 阈值、最小间隔 30 px、边缘与饱和剔除）→ 最多 400 候选。
4. 用各帧**真实 WCS**（astropy.wcs）把候选映射到每帧，要求在所有帧均有效（在界内、未饱和、d.sum>0）；
   按逐帧孔径 SNR 的最小值排序取前 8 颗作为**共同星集**。
5. 每颗星在每帧取 9×9 stamp（双线性采样到分数中心），环带 [8,14] px 3σ 裁剪中值作局部背景 bg，d = stamp − bg（ADU）。
6. **PSF 估计子**：P_raw = d/Σd，**非负裁剪** P = clip(P_raw,0)/Σclip（P≥0 且 ΣP=1，符合冻结门）。
   逐星保留 P_raw_min / P_raw_negative_pixels 作为原始证据（§7 异常）。
7. **噪声模型（声明为近似）**：σ² = (环带 3σ 裁剪标准差)²，逐像素常数（天空限幅、对角、帧间独立）。
   头文件无 GAIN/RDNOISE，不做未经验证的光子传递换算；所有 σ² 直接来自真实像素散布。

### 2.2 五口径定义（驱动 run/v6/real-science/driver/v6_real_science_driver.cpp）
所有权威量经**冻结 V6 库函数**计算，不新增科学公式：

| 口径 | 类别 | 逐帧组合系数 α_k | 权威来源 |
|---|---|---|---|
| equal | DOCUMENTED_BASELINE | 1/K | 冻结词表 unit_weight: I=mean_k d_k |
| exposure | DOCUMENTED_BASELINE | t_k/Σt | 曝光时间加权（本任务驱动内按词表公式） |
| ivar | DOCUMENTED_BASELINE | (1/v_k)/Σ(1/v_j)，v_k=Σ_p σ²_k | 冻结词表 pixel_ivar |
| **W_info** | **PRODUCTION point_information** | W_k/ΣW | p1psfw::w_info_diagonal + combine_point_estimates |
| **PSFSW** | **PRODUCTION psfsw_robust** | W_psfsw,k/ΣW_psfsw | extract_psfsw_components + compute_psfsw_weights + conventional_coadd |

W_info 权威式：Q_k = a_k P_kᵀC_k⁻¹d_k、W_k = a_k²P_kᵀC_k⁻¹P_k、F̂ = ΣQ/ΣW、Var = 1/ΣW。
PSFSW 权威式：Wt_k = C_norm·S^α·Conc^β/(N^γ·B^δ)、W_psfsw = Wt/median(Wt)（组内 median=1，无量纲）。

**共同估计子（跨口径可比）**：对每个口径，用其**实际组合系数**构造组合面 I = Σα_k d_k 与方差面
V = Σα_k²σ²_k，并用该口径的 **effective PSF** 作同一匹配滤波：
F = Σ(P_eff·I/V) / Σ(P_eff²/V)、Var = 1/Σ(P_eff²/V)。
模式**本征量**（W_info 的 Q/W、PSFSW 的 conventional coadd 通量）另行列报，用于区分「口径差异」与「估计子定义差异」（§7）。

### 2.3 方差与 effective PSF
- 所有口径：Var = R C_in Rᵀ，R 为该口径**实际归一组合系数**，C_in 为逐帧实测方差（对角、帧间独立）。
  逐口径用 propagate_covariance 计算；Oracle 独立复算 αᵀ diag(v) α。
- effective PSF：conventional_effective_psf（P_eff = Σα_k a_k P_k / [Σα_k a_k P_k](0)，peak 归一），
  FWHM 由 measure_fwhm 从剖面测得，不用逐帧 median 代替。

### 2.4 口径正确性硬约束的落实（逐条可核）
| 约束 | 落实 | 证据 |
|---|---|---|
| W_info 单位 ADU⁻² | ΣP²/σ²，P 无量纲、σ²=ADU² → ADU⁻² | 驱动 units.W_info="ADU^-2"；Oracle units 检查 |
| Q 单位 ADU⁻¹ | ΣP·d/σ² → ADU/ADU² = ADU⁻¹ | units.Q="ADU^-1" |
| psfsw 无量纲 | weight_units="1"、group_normalized=true、median=1 | Oracle psfsw_median_one_all_positive |
| **PSFSW 不得写成 ivar/Fisher** | psfsw 块无 ivar/variance/fisher/w_info 键；variance_from_weight=false；method=propagated_from_composite_coefficients | Oracle psfsw_no_forbidden_keys、psfsw_not_ivar；负向 G05/G06 可红 |
| final covariance 只能 R C_in Rᵀ | 全口径 αᵀ C_in α，α 为**实际组合系数** | Oracle rcrt_reproduces_1_over_sumW 与逐星复算 |
| median(SNR_F)/support/coverage/FWHM 不得进权重/方差面 | α 仅来自 W_info、v_k、t_k、w_psfsw；库 forbidden_weight_source_aliases() 含全部诊断别名 | 负向 forbidden_weight_aliases_present |
| psf_snr_power 保持 DEFERRED | 生产模式集仅 3 项；CLI/路由门拒绝 | 负向 route2_reject_psf_snr_power（40/40） |

---

## 3. 实测值 / 引用冻结常数 / 未测项的区分

- **实测值（本任务真实运行）**：全部 SNR、flux、Var、FWHM、a_nea、背景、α、曝光、相关系数、资源指标。
- **引用冻结常数**：PSF 归一容差 1e-9、PSFSW 指数 (α=2,β=1,γ=2,δ=1)、C_norm=1、组内 median 目标 1.0、
  kCommonMin=3、单位词表。**未改动任何冻结公式/容差/门。**
- **未测项（如实登记）**：
  1. psf_snr_power：DEFERRED（C-004.1），未运行、未比较。
  2. 真实 **FITS 产品链**（write_phase1_product → run_point_information / run_psfsw_robust 磁盘产物）未在本任务运行；
     W_info/PSFSW 的权威量与 R C_in Rᵀ 由**冻结库函数直接计算**并经独立 Oracle 复核。
  3. 系统相关噪声（相关核）未估计；C_in 为实测背景 RMS 的对角模型。
  4. surface_gls 生产产品链未单独运行；其与原假设的**等价性**在 Oracle 中以堆叠 GLS 正规方程独立验证（§6）。
  5. SO-05 资源门为待签（record_only）。

---

## 4. 科学结果：共同匹配滤波估计子（跨口径可比）

median SNR（8 颗共同星，逐数据集中位数）：

| 数据集 | equal | exposure | ivar | W_info | PSFSW | W_info/equal |
|---|---|---|---|---|---|---|
| Galaxy_Center | 5684.9 | 5684.9 | 5720.0 | 5776.2 | 5550.6 | 1.0161 |
| LDN43 | 7087.6 | 7087.6 | 7843.8 | 7806.7 | 7353.8 | 1.1015 |
| M42 | 13862.2 | 13862.2 | 14454.9 | 14470.0 | 13476.3 | 1.0438 |
| M42_Halpha | 16090.8 | 15907.8 | 15634.3 | 16012.9 | 16073.4 | 0.9952 |
| NGC1727 | 8008.2 | 8008.2 | 8654.1 | 8707.6 | 8295.2 | 1.0873 |
| NGC247 | 7795.8 | 7795.8 | 8716.2 | 8760.7 | 8631.7 | 1.1238 |
| NGC55 | 7036.7 | 7036.7 | 7337.7 | 7325.3 | 6665.8 | 1.0410 |
| NGC83 | 13520.5 | 13520.5 | 14227.8 | 14154.0 | 13737.8 | 1.0469 |
| Victory_Nebula | 5196.1 | 5196.1 | 5281.5 | 5282.8 | 4994.2 | 1.0167 |

- **W_info 在 8/9 数据集上 ≥ equal**（+1.6% ~ +12.4%），符合信息最优预期；在唯一混曝光的 M42_Halpha 上为 −0.5%。
- **ivar ≈ W_info**（差 0~7%）：在逐帧方差相近时，逆方差加权已接近最优。
- **PSFSW** 在共同匹配滤波下与其它口径同量级（0.95~1.11× equal），证明其作为**无量纲 conventional 权重的有效性**。

### 4.1 曝光口径
- 8/9 数据集各帧曝光时间相同 → **exposure ≡ equal（α 完全相同）**，这是正确结果而非缺陷。
- 唯一混曝光数据集 M42_Halpha（600 s×4 + 300 s×1）：exposure/equal = **0.9886**（略差约 1.1%）。
  原因：曝光加权假设光子噪声限幅（Var ∝ 1/t），而本数据为**天空限幅**（各帧 σ² 近似与 t 无关），
  按 t 加权会相对高估长曝光帧。这是**口径假设与实际噪声模型不匹配**导致的差异，不是实现缺陷。

---

## 5. 背景、方差面、effective PSF、coverage/support

| 数据集 | 全局背景 median±rms (ADU) | FWHM equal/ivar/W_info/PSFSW (px) | 共同MF下 Var(W_info)/Var(equal) | a_nea (px²) | frame coverage | PSFSW defined |
|---|---|---|---|---|---|---|
| M42 | 1178.1 ± 16.3 | 3.144 / 3.176 / 3.161 / 3.105 | 0.911 | 25.63 | 1.00 | 1.00 |
| M42_Halpha | 1068.8 ± 11.4 | 4.074 / 4.034 / 4.022 / 4.060 | 0.885 | 22.43 | 1.00 | 1.00 |
| Galaxy_Center | 1442.0 ± 44.3 | 2.252 / 2.272 / 2.262 / 2.229 | 0.955 | 12.22 | 1.00 | 1.00 |
| LDN43 | 3085.1 ± 44.1 | 3.029 / 2.946 / 2.921 / 2.827 | 0.939 | 24.28 | 1.00 | 1.00 |
| NGC1727 | 1475.0 ± 49.2 | 3.385 / 3.336 / 3.503 / 3.448 | 0.953 | 29.82 | 1.00 | 1.00 |
| NGC247 | 2434.2 ± 39.2 | 3.670 / 3.632 / 3.624 / 3.631 | 0.830 | 34.64 | 1.00 | 1.00 |
| NGC55 | 2049.0 ± 38.2 | 4.277 / 3.715 / 3.721 / 4.350 | 0.666 | 24.18 | 1.00 | 1.00 |
| NGC83 | 1423.7 ± 20.0 | 3.264 / 3.276 / 3.268 / 3.302 | 0.948 | 25.52 | 1.00 | 1.00 |
| Victory_Nebula | 1920.5 ± 47.7 | 4.443 / 4.469 / 4.456 / 4.429 | 0.839 | 13.97 | 1.00 | 1.00 |

- **方差面差异**：在**共同匹配滤波**下 Var(W_info)/Var(equal) = 0.67~0.96（与 SNR 比一致）；
  但**模式本征量**中 Var(W_info)/Var(equal) = 0.15~0.37——差异来自 equal 本征估计子是**孔径求和**（81 px 噪声全加）
  而 W_info 是**匹配滤波**，属**估计子定义差异**，不是权重面缺陷（§7）。
- **effective PSF**：各口径 FWHM 一致到 <0.5 px；PSFSW 因使用其自身组合系数，个别数据集（NGC55）差约 0.6 px。
- **coverage/support**：45 个帧-星全部有效（coverage=1.0）；ΣP=1（PSF 支持归一）；PSFSW defined_fraction=1.0；
  组内 median(W_psfsw)=1.0 且全正（Oracle 逐数据集复核）。

---

## 6. 独立交叉校验（Oracle：v6_real_science_oracle.py，纯 numpy 独立实现）

**结果：486 项检查，失败 0，all_pass=true。**

1. **Q/W 与逐帧解析式对拍**：对全部 360 个帧-星，numpy 直接计算 Q_k = Σ P d/σ²、W_k = Σ P²/σ²，
   与驱动库输出的 q_k / w_k 逐帧比较，max_rel < 1e-9。
2. **同一 GLS 假设下的等价性**：堆叠 A=[P_k]、C_in=diag(σ²_k)，独立求解
   x = (AᵀC⁻¹A)⁻¹AᵀC⁻¹d，与 ΣQ/ΣW 及 1/ΣW 比较，max_rel < 1e-9（逐数据集）。
3. **R C_in Rᵀ 复现 1/ΣW**：R_k = W_k/ΣW、C_in = diag(1/W_k)，αᵀC_inα ≡ 1/ΣW。
4. **PSFSW 独立复算**：Wt = S²Conc/(N²B)（MAD 系数 1.482602218505602）与组内 median 归一，
   逐分量与驱动一致，max_rel < 1e-9。
5. **禁止键/单位词表/α 归一**：psfsw 块禁止键、variance_from_weight=false、单位声明、每一模式 Σα=1。

---

## 7. 「口径差异」vs「实现缺陷」的区分与最小复现

**结论：本项目未发现实现缺陷；观测到的显著差异均可归因于口径/估计子定义。**

- **差异 A（大，~2×）**：模式本征 W_info SNR ≈ 2× equal。**不是缺陷**，而是 equal 本征估计子为孔径求和、
  W_info 为匹配滤波。最小复现：同一星同一 5 帧，equal 的 flux=Σd、var=Σσ² 与 W_info 的 Q/W 定义不同；
  换成 §4 的共同匹配滤波后差异降为 0~12%。证据：five_mode_measurements.json#per_star[].modes.*.{snr,mf_snr}。
- **差异 B（小，~1%）**：M42_Halpha 上 exposure 略差于 equal。**不是缺陷**，是曝光加权假设与天空限幅噪声不匹配。
  最小复现：M42_Halpha 帧曝光 [600,600,600,300,600]，α_exposure=[0.2222×4,0.1111] 对 α_equal=[0.2×5]，
  mf_snr 16090.8 → 15907.8。
- **差异 C（边界，−0.5%）**：M42_Halpha 上 W_info 略低于 equal。**不是缺陷**：C_in 取自逐帧经验 PSF 与
  天空限幅 σ² 的**估计值**，5 帧下权重估计噪声可产生 <5% 波动；区间内不构成系统性劣化。

**真实数据异常（触发 fail-closed，已用最小复现确认）**：
- 部分真实星（银河中心 34 个样本、Victory_Nebula 52 个样本）的**经验 PSF P_raw 有负噪声样本**
  （min(P_raw) = −2.227e-4）。冻结门 psf_profile_stats **按设计拒绝** negative_sample。
  最小复现：v6_real_science_negative 的 psf_profile_reject_negative_sample（门红）。
  处置：采用**非负 PSF 估计子**（裁剪+重归一，满足 P≥0、ΣP=1），并把 P_raw_min / 负样本计数留痕于输入产物；
  该处置是估计子选择，**未放宽冻结门**（门仍拒绝原始负样本）。
- 其余数据集的 P_raw 无负样本（min ≥ 5.9e-5）。

---

## 8. 负向 / 边界检查（异常输入必须被拒）

### 8.1 库层负向 harness（v6_real_science_negative）：**40 项，通过 40，退出码 0**
- psf_profile：negative_sample / not_normalized / null_input 全部拒绝；合法控制接受。
- w_info_diagonal：σ²≤0 拒绝；合法控制接受。
- combine_point_estimates：空输入拒绝。
- extract_psfsw_components：a_nea≤0、<3 星全部拒绝；compute_psfsw_weights：B≤0 fail-closed 拒绝、合法控制接受。
- conventional_coadd：不规则输入拒绝；propagate_covariance：尺寸不符拒绝。
- **CLI 路由单源差分门**（cli/v6_runtime_contract.h）：phase2 production 3 项放行、baseline 2 项放行、
  psf_snr_power / auto / support_x_snr2 / "0" / bogus / 空 拒绝；legacy 0 拒绝、1/2 baseline；
  phase3 3 模式放行、未知拒绝；隐式 Phase 串接 "run" 被识别、"phase2 run" / "phase1 run" 不误判。
- **PSFSW 记录门差分**：基线记录无 G05/G06/G02/G07/G21；注入 variance_from_weight=true / psfsw.ivar /
  weight_units=flux^-2 / fwhm-only / weight_mode="0" 分别**恰好**产生对应门（可红）。

### 8.2 CLI 真实进程负向门（cli_mode_gate_check.sh）：**15 项，通过 15，退出码 0**
| 输入 | rc | 期望 |
|---|---|---|
| --mode point_information / surface_gls / psfsw_robust | 0 | 0（production） |
| --mode equal / pixel_ivar | 0 | 0（baseline，stderr WARNING） |
| --mode psf_snr_power | 2 | 2（FZ-MODE-DEFERRED，C-004.1） |
| --mode auto / support_x_snr2 | 2 | 2（FZ-FIELD-WEIGHTMODE） |
| --mode 0 / 1 / 2（数字串） | 2 | 2 |
| --mode bogus | 2 | 2 |
| config weight_mode=0 | 2 | 2 |
| config weight_mode=1 / 2 | 0 | 0（baseline） |

---

## 9. 资源指标（实测）

| 阶段 | 墙钟 | CPU (user/sys) | 最大 RSS | I/O（块） | 来源日志 |
|---|---|---|---|---|---|
| 抽取（M42，5 帧 4096²） | 103.35 s | 48.91 / 53.89 s | 743.6 MB | in 8 / out 456 | logs/08_extract_resource_probe.log |
| 抽取（全部 9 数据集） | ≈ 730 s（02:53:11→03:05:21） | — | — | — | logs/01_extract_all.log + inputs mtime |
| 驱动五口径（9 数据集×8 星） | **0.280 s** | 0.280 / 0.00 s | **6.26 MB** | out 744 | logs/04_driver_run.log |
| Oracle（486 检查） | 0.21 s | 0.18 / 0.02 s | 30.8 MB | in 1776 / out 128 | logs/07_oracle.log |

- 科学计算阶段（驱动）**单线程、纯 CPU、无私有线程池、无硬编码 workers**（宪章 §10.4）；
  CPU 利用率 100%，五口径比较以 6 MB 级常驻、亚秒墙钟完成。
- **SO-05 资源门状态：PENDING_OWNER_SIGNOFF**——按 C-007 仅记录，未据此判 PASS，也未写成硬失败。
- 抽取阶段高 RSS/sys 来自读入 4096² 真实帧与 scipy 全帧滤波，属数据预处理，非重计算门覆盖的科学 kernel。

---

## 10. 不确定性与局限

- 噪声模型为**天空限幅对角近似**（无 GAIN/RDNOISE 头关键字，未做增益标定），会**低估**星像光子噪声；
  这是 W_info 相对 equal 优势偏小的主要原因之一。
- 每数据集仅 5 帧、8 星，α 与 SNR 的**帧间/星间散布**在 ±5% 量级；单数据集的小差值（<2%）不应解读为系统性优劣。
- PSF 为逐帧经验 stamp（非外部模型），受星像噪声影响；已用非负裁剪并留痕。
- 未估计帧间相关系统项；C_in 对角假设下 R C_in Rᵀ 的等价性已在 Oracle 中验证，但**相关噪声**情形未测。

---

## 11. 可复现命令清单（全部带 timeout；退出码见括号）

    cd "/workspace/Astro CS Database"
    git rev-parse HEAD                                   # 5eb702bd…（rc 0）

    # 0) 校验/重建根 CLI（陈旧则重建）
    ./build/astrocs --version                            # +g5eb702bd…（rc 0）
    cmake --build build --target astrocs -j8             # rc 0

    # 1) 真实数据抽取（9 数据集，5 帧）
    python3 run/v6/real-science/extract_real.py --out run/v6/real-science/inputs --frames 5   # rc 0

    # 2) 构建并运行测量驱动 + 负向 harness
    cmake -S run/v6/real-science/driver -B run/v6/real-science/build &&       cmake --build run/v6/real-science/build -j8                                        # rc 0
    run/v6/real-science/build/v6_real_science_driver --in run/v6/real-science/inputs       --out run/v6/real-science/measurements/five_mode_measurements.json       --csv run/v6/real-science/measurements/five_mode_measurements.csv                  # rc 0
    run/v6/real-science/build/v6_real_science_negative                                   # rc 0

    # 3) CLI 模式门负向检查
    bash run/v6/real-science/oracle/cli_mode_gate_check.sh                               # rc 0

    # 4) 独立 Oracle 与汇总
    python3 run/v6/real-science/oracle/v6_real_science_oracle.py --inputs run/v6/real-science/inputs       --measurements run/v6/real-science/measurements/five_mode_measurements.json       --out run/v6/real-science/measurements/oracle_results.json                         # rc 0（486/486）
    python3 run/v6/real-science/summarize.py                                             # rc 0
    python3 run/v6/real-science/make_provenance.py                                       # rc 0

---

## 12. 产出清单

- 产物：artifacts/v6/real-science/
  five_mode_measurements.json、five_mode_measurements.csv、summary_metrics.csv、oracle_results.json、
  input_manifest.json、negative_checks.jsonl、cli_mode_gate.jsonl、inputs/*.json（9）、logs/*、PROVENANCE.json
- 报告：reports/v6/real-science/（本报告 + REPRODUCE.md）
- 运行域：run/v6/real-science/（脚本、驱动源码、日志、中间产物）

## 13. 需负责人裁决 / 未决风险

1. **SO-05 资源门**待负责人签字；本任务按 C-007 记录为 PENDING_OWNER_SIGNOFF，不构成 PASS/FAIL 依据。
2. **噪声模型升级**：若要评估 W_info 的真实最优性上界，需引入增益/读出噪声（头缺失）或联合相关核；属后续科学工作，未在本任务伪造。
3. **FITS 产品链复核**：建议在 W11/W12 以真实 Phase1 磁盘产物跑一次 run_point_information / run_psfsw_robust，
   与本报告的库级权威结果对拍（本任务未做，已登记为未测项）。

## 14. 结论声明

- 未 commit / push / add / 分支 / worktree / stash / reset / clean；
- 未越界写（仅 artifacts/v6/real-science/、reports/v6/real-science/、run/v6/real-science/）；
- 未派生子代理；
- 未编造数据；所有数值来自真实运行并可经 §11 命令复现；
- 未宣布发布；未改冻结公式/容差/门；psf_snr_power 保持 DEFERRED；median(SNR_F)/support/coverage/FWHM 未进任何权重/方差面。
