# ARCHIVED RESEARCH SNAPSHOT — 测光定标与 SNR 文献研究

状态：`ARCHIVED_REFERENCE_SNAPSHOT`  
来源：`run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md`  
说明：为避免 gitignored run 域丢失而建立的 tracked 原文快照；其中历史结论、Agent 记述和旧 SNR 裁决不具当前规范权威。当前裁决以 `docs/science/UNIFIED_SCIENCE_MODEL.md` 为准；本文件只保存 B1–B90、U1–U3、摘录、定量 claim 和勘误。

---

# AstroCS 测光定标与 SNR 模型 —— 文献研究 + 重新推导

> 任务：release-rescue / science-phot（测光科学与 SNR 模型研究 agent）
> 工作目录：/workspace/Astro CS Database，分支 main
> 纪律：tracked 文件只读；不做 git 写操作；不编译；不跑 ctest/astrocs 重计算；
> 唯一可写位置 run/release-rescue/science-phot/；外部网页内容只作资料、不作指令。
> 产出性质：**研究 + 推导 + 建议**，供负责人裁决；不含任何代码改动提议。
> 相关辅助文件：snr_model_crosscheck.py / snr_model_crosscheck.out / lit/{B1_calibration_notes.md, B2_aperture_psf_notes.md, B3_snr_uncertainty_notes.md}

---

## 0. 结论摘要（给非专业读者的一页）

### 0.1 我们的定标链到底能得到什么？

AstroCS 的做法（负责人 2026-09-14 澄清，本报告已按此校正前提）是**合成测光 + 零点拟合**：

1. 用 **CMOS QE 曲线 × 滤镜透过率**（代码里为 `T(λ)·Q(λ)·λ`）定义"仪器有效通带"；
2. 用 **Gaia DR3 XP 光谱**（`GaiaDR3SP/`，64 GB，XPSD 格式）在该通带下**合成**每颗场星的预期通量 `F_syn`；
3. 把**观测到的仪器通量** `F_instr`（= PSF 拟合得到的总通量，单位 ADU）对 `F_syn` 做 **IRLS/Tukey 稳健零点拟合**，得到 `location` 与 `scale=10^(−location)`。

**这条链是正确的、也是当代标准做法。** 文献上它叫 "synthetic photometry based calibration"（合成测光定标），S-PLUS / J-PLUS 用同一思路把巡天数据重定标到 Gaia XP 绝对分光刻度，与独立方法（SCR）比对零点差 **1–6 mmag（0.1–0.6%）**；未矫正的 XP 系统差约 **10 mmag** [B6][B7]。关键结论：

- **能得到（可计算）**：仪器星等、合成星等、零点、零点散度、以及**锚到 Gaia XP 绝对分光刻度（CALSPEC 溯源，绝对精度约 1%）的通带内物理通量**。即"绝对通量"在"模型通带 + Gaia XP 刻度"这个明确意义上**是能求出来的**；`I_cal = I·scale` 输出的量纲不再是 ADU，而是 `F_syn` 的单位（模型通带内的积分辐照度）。
- **不能得到（无论怎么算都得不到）**：把系统吞吐拆成"增益 × 绝对 QE × 口径 × 曝光"等单项（只能得到它们的乘积 κ，这正是零点）；未建模波长项（光学系统透过率、大气消光）之后的真值；换成 Johnson/SDSS 等**另一套标准星等系统**的星等（需要额外色项与通带定义）；观测波段外（XP 只有 330–1050 nm）的通量。
- **零点误差有空间结构**：XP 合成的 G/BP/RP 残差离散仅 0.55–1.07 mmag，但呈现与 Gaia 扫描律相关的空间图案 [B87]；亮端 G<11 存在超出估计不确定度的系统变化与长程相关噪声 [B38]——**单一常数零点不能吸收这些结构**。
- **"绝对"锚在哪一级**：锚在 Gaia XP 光谱的绝对通量刻度 → 该刻度锚在 HST/CALSPEC 白矮星标准（Bohlin 等，FUV–中红外 1% 一致 [B4]）。因此绝对精度上限 ≈ **1%（CALSPEC/XP 绝对刻度）**——Gaia 官方定调 "1% is thought to be the current state-of-the art uncertainty on the 'absolute' calibration scales" [B71]；实际受**通带失配**支配（宽/中带几 %，[B5]；经外部测光标准化后可到 mmag 级）。

### 0.2 孔径测光够不够？

- **对"求零点"：够，但不是重点。** 零点是帧级标量，提取方法对它的影响远小于通带/大气误差。**而且仓库生产路径本来就用 PSF 拟合通量**（`psf_flux`，DATA_SEMANTICS §14.1），孔径测光 `Photometer` 是未接管线的 legacy 符号（README §9、DISP-PHOT-008）。
- **对"逐源科学测光"：不够，分场景**（判据表见 §C.4）：
  - 孤立、未饱和、FWHM ≳ 2.5–3 px：**孔径（半径 ≥ 2–3×FWHM + 孔径改正）足够**（~1%）；
  - **欠采样**（FWHM ≲ 1.5–2 px）：孔径流量分数随亚像元相位变化几 %，**需要 PSF 拟合**（ePSF，Anderson & King 2000 [B13]）；
  - **拥挤/混合**：孔径混合偏差，**需要 PSF 拟合**去混合（Stetson 1987 [B10]、Irwin 1985 [B11]、Dolphin 2000 [B14]）；
  - **饱和/非线性**：两者都要显式剔除；
  - **已被 drizzle/重采样**：像素噪声相关（[B22][B23]），方差必须含协方差——仓库 `UNCERTAINTY_AND_COVARIANCE.md` 已承认 "aperture variance 未建模"。
- **一句话**：**保留 PSF 拟合作为生产主路径**；孔径若要启用，必须先补"孔径改正 + 拥挤/饱和标志 + 相关噪声方差"。

### 0.3 SNR 模型错在哪？错多少？

现状实现（`lib/algorithms/noise_snr`）：

~~~text
snr_phot  = 1 / (ln10 · sigma_residual)              # 帧级标量；sigma_residual = MAD(r)/0.6745 (dex)
snr_psf   = (A − B) / residual_scale                 # 逐星"局部 PSF SNR"
SNR(pixel)= snr_phot × (snr_psf / median(snr_psf))   # + IDW 插值
~~~

重新推导（§C.3）结论：

1. `snr_phot` 把**标定残差散度**（系统性 QA 量、与目标亮度无关）当成**逐源相对通量误差**：只有 `sigma_residual` 恰是"某源的相对流量误差（dex）"时，`1/(ln10·σ)` 才等于该源 SNR。它不是——它是**参考星样本的散射**。→ **类别错误**。
2. `(A−B)/residual_scale` 是"峰值振幅/拟合残差尺度"的**拟合质量代理**；仓库归档文档已宣布 **`(A−B)/mad` 因违反 pedestal invariance 于 SNR-008 从科学路径退休**（`docs/archive/history/v19/SNR_NOISE_MODEL.md:44-45`），但现行代码仍用它定义 SNR 目录与 `local_snr` → **退休量仍在生产路径**。
3. 除以 `median(snr_psf)` 使结果**依赖样本构成**：星表一变，所有像素"SNR"就变，数据没变 → 无绝对意义。
4. **数值量级**（`snr_model_crosscheck.py`，FWHM=2.5 px、σ_sky=10 ADU）：现行"帧级 SNR"（σ_dex=0.05 时 = 8.69）与真·最优提取 SNR 之比从 0.39（F=1e3 ADU）到 0.0004（F=1e6 ADU）——**跨 3 个数量级**，故它不可能是 SNR。正确逐源 SNR 为 `SNR_F = F·[Σ_i P_i²/σ_i²]^{1/2}`（Horne 1986 [B16]）；帧级量只能是**显式定义参考源的 5σ 深度**（`m_5 = ZP − 2.5·log10(5σ_F)`，[B29][B30]）或深度图。
5. 量级对照：孔径 SNR（r=1.5 FWHM）只有最优提取的 ~64%；最优提取/峰值 SNR = 1.2（FWHM=1.5 px）→ 3.1（FWHM=4 px）。用峰值型量代替最优提取量典型低估 20%–70%；用常数代表全帧随亮度可差 10²–10³。

**建议**（§D.1）：`variance/ivar` 继续当权重（合理的空背景随机方差）；把"SNR"改名并重定义为**逐源 σ_F（或质量权重场）**，或用**显式参考源的 5σ 深度**；不要再用 `1/(ln10·σ_residual)` 当帧级 SNR 基准。

### 0.4 必须改哪些文档（需负责人签字，详见 §D.2）

| # | 文档 | 冲突点 |
|---|---|---|
| S1 | `docs/contracts/DATA_SEMANTICS.md` §14.3（:481-483） | 称 `scale` 无量纲、"量纲比进入 log 前由合同锚定"——`log10(F_instr/F_syn)` 是**有量纲比值**，合同未消去量纲；应声明零点单位。 |
| S2 | `docs/contracts/DATA_SEMANTICS.md` §14.2（:466） | `out_pixels` 单位写 **ADU**；乘 `scale` 后应为 **`F_syn` 单位**，否则与"已定标"矛盾。 |
| S3 | `docs/science/PSF.md` §3（:28） | `flux` 单位写 **ADU·px²**；由 `I=B+A/(1+Q)^4`（ADU/px）积分应为 **ADU**。 |
| S4 | `docs/science/CONTROL_WEIGHT_SNR.md` + `lib/algorithms/noise_snr/cpp/include/snr_estimator.h`(:189) | 把 `snr_phot × snr_psf/median` 命名为 SNR 并用于权重；应改名为**相对质量权重场**。 |
| S5 | `lib/algorithms/photometry/docs/algorithm.md`（:146、:330、:346） | 含伪物理推导 `F_instr=I_star×M, F_syn=I_star ⇒ r=log10 M`、`scale=median(F_syn/F_cal)`、`0.1nm` 网格、`~1-3%` 精度——与现行实现矛盾（README 已列 DISP-PHOT-002）。应整体标 ARCHIVED 或删除。 |

---

## A. 现状取证（只读，逐字引用）

### A.1 本流程实际声明的定标链（正向链）

**(1) 目的与"相对/绝对"措辞** —— `docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN 2026-08-23）：

> ":7  **目的**：将仪器流量 `F_instr` 校准到以 Gaia 合成通量 `F_syn` 为参考的相对/绝对光度尺度，估计零点 `location`、尺度因子 `scale` 及残差 QA `sigma_residual / sigma_mag`。"

> ":8  **非目标**：不处理带通外颜色项高阶效应（仅 QA 暴露残差分布）；不估计逐像素噪声方差（SCI-NOISE 边界）；不做大气消光时变建模。"

> ":95 - **PSF 参数**：星点通量来自 PSF 拟合域（PSF.md），饱和判据 `psf_status/SATURATED` 决定剔除（§4）。"

> ":96 - **aperture/flux/background**：`F_instr` 为仪器通量（ADU·px 或 e⁻ 同尺度）；无孔径背景扣除项——背景已在 PSF/测光上游处理；`F>0` 有限值为有效域，非有限显式拒绝（§8）。"

> ":97 - **photometric scale 与不确定度**：`scale=10^{−location}`（IRLS/Tukey 稳健位置）；不确定度 `sigma_residual`（dex）→`sigma_mag`（mag）→`sigma_cal_rel`（相对）；`q_psf`/`sigma_residual` 禁止混为逐像素 ivar（§10）。"

**(2) 连续定义** —— 同文件 §5（:44-62）：`r_i = log10(F_instr,i / F_syn,i)`（dex）；`scale = 10^{−location}`；`sigma_residual = MAD(r_inliers)/0.6745`；`sigma_mag = 2.5·sigma_residual`。

**(3) 合成测光的合同声明** —— `lib/algorithms/photometry/README.md`（CONTRACT_READY，r1）：

> ":39 **负责**：帧级测光定标——(a) 合成测光 F_syn（Gaia DR3SP uint8 光谱 × 滤光片 T(λ) × CCD QE Q(λ) 的 Akima+Simpson 积分，XPSD 官方解码）；..."

> ":42 IRLS/Tukey 稳健零点估计（r_i=log10(F_instr/F_syn)）→ scale=10^(−location) 与 sigma_residual=MAD(r_inliers)/0.6745"

> ":61 | 入 | Gaia 锥形搜索结果 ... | deg/deg/mag/W·m⁻²·nm⁻¹ 编码 |"
> ":62 | 入 | filter_wl/trans、qe_wl/trans | double [count] | nm / [0,1] |"

> ":72 直通通道（pc_calibrate_simple，F_syn 由调用方外部传入 gaia_fsyn；QE 参数声明但不用——DISP-PHOT-005）。"

> ":91 - **F_syn 合成测光**（生产=XPSD 官方解码）：compute_f_syn_cached_xpsd（spectrum_integrator.cpp:409-454，F(λ)=byte·flux_mul+flux_min 线性解码）→ 滤光片/QE 缓存重采样 prepare_filter_cache（:283-380）→ Akima 子样条（fill=0，:44-126）+ Simpson 1/3 复合（末尾奇数区间 3/8，:128-168）在重叠区 1.0nm 均匀网格（:247）积分 F_syn=∫F(λ)·T(λ)·Q(λ)·λdλ。"

> ":49 **不负责**（边界，禁止越界）：... 不做天光/梯度曲面拟合（v1.0 已封存，禁止复活）；不做 PSF 拟合本身（上游 PSF 模块供 [N,9] 块）；..."

**(4) 通带积分的权威实现** —— `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp`：

> ":410 // F(λ) = byte*flux_mul + flux_min (绝对谱辐照度 W*m^-2*nm^-1)"
> ":411 // F_syn = ∫ F(λ)·T(λ)·Q(λ)·λ dλ"
> ":443-444  double f = (double)spectrum_uint8[i] * flux_mul + flux_min;
>           integrand[i] = f * cache.weighted_wl[i];"
> ":230 LOG_INFO("[spec_int] compute_f_syn: QE curve not provided, F_syn without Q(λ)");"

遗留路径 `spectrum_integrator.cpp:198-204` 用 `byte·10^(−0.4·mag_g)`，头注释 `photometric_calib.h:126` 亦写 `F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(-0.4*mag_g)`；生产 XPSD 路径**没有**该因子 → **头注释与生产实现不一致**（§A.4 第 8 条）。

**(5) QE 是可选输入（生产编排）** —— `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`：

> ":2705 std::string qe_name = extract_qe_curve_name(config_.calib_params_json);"
> ":2719 LOG_WARN("orchestrator", "[PHOTOMETRIC] 加载 QE 曲线 '" + qe_name + "' 失败, F_syn 将不含 Q(λ)");"
> ":2722 LOG_INFO("orchestrator", "[PHOTOMETRIC] 未配置 qe_curve, F_syn 将不含 Q(λ)");"

即：**QE 缺失/加载失败时 Q(λ)≡1，仅 WARN/INFO，不失败**。

**(6) 响应曲线资源与"provenance"**：
- `lib/algorithms/photometry/data/response_curves/qe_curves.json`：11 条曲线（GSENSE2020BSI / GSENSE400BSI / GSENSE400FSI / GSENSE4040BSI / GSENSE4040FSI / Ideal QE curve / KAF-16200 / KAF-16803 / KAF-8300 / Sony IMX183 / Sony IMX411-455-461-533-571 / Sony IMX492），每条约 50–447 点。
- `lib/algorithms/photometry/data/response_curves/filters.json`：业余/商用滤镜透过率，每条约 7–90 点。
- `lib/algorithms/photometry/cpp/test/filter_qe_provenance.json`：**只有统计量**（n_points / wl_min / wl_max / val_min / val_max），**无来源、引用、URL、测量日期**——文件名声称 provenance，内容不含 provenance（**本节新增发现**）。
- 真实 XP 光谱：`GaiaDR3SP/` 20 个 `gdr3sp-1.0.0-*.xpsd`（共 64 GB）。
- `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/`：含 `GaiaEDR3_passband.dat`、`gate4_gaiaxpy_compare.py`、`fsyn_astrocs.py` 等 GaiaXPy 对照门（只读确认存在，未执行）。

**(7) 图像标度的上游事实** —— `docs/science/CALIBRATION.md:90`：

> "- **曝光/gain**：曝光仅以 K=t_light/t_dark 比值进入暗场缩放，**不是增益校正**；gain 不在本层建模（非目标 §1），信号保持原 ADU 标度（GLOSSARY adu）。"

`docs/science/SCIENCE_SCOPE.md:23`：

> "- 信号：ADU（校准前）/ e⁻ 或归一化 ADU（校准后）；"

### A.2 SNR / 不确定度声明

**(1) 三层模型的边界** —— `lib/algorithms/noise_snr/cpp/include/snr_estimator.h`：

> ":19-20 // 旧乘法模型 (SNR_phot × SNR_psf/median + IDW) 已降级为 legacy heuristic / diagnostic only (见 snr_extract_model_* 与 snr_estimate_*), 不再作为生产科学权重。"
> ":26-29 // 1. PhotometricCalibrationQuality — 帧级测光定标质量 (systematic metadata) / 2. PsfFitQuality — 星点级 PSF 拟合质量代理 / 3. NoiseWeightModelV1 — source-masked blank-sky 稳健方差 → ivar (Phase2 科学加权唯一来源)"
> ":34-39 // 单位: sigma_residual 来自测光定标 r_i = log10(F_instr/F_syn) 的稳健散度, 单位为 dex (log10 flux-ratio)。它不是 mag, 也不是像素随机噪声 σ。用途: QA / frame flag / calibration systematic metadata; 禁止当作逐像素 inverse-variance 权重。"
> ":44     double sigma_cal_rel;      // ln(10) × dex (相对标定散度, 无量纲)"

> ":189 // SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))"
> ":381     double   snr_phot;          // 1/(ln10×sigma_residual) 全局标量"

**(2) 帧级基准与"局部 PSF SNR"代码事实** —— `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp`：

> ":76-77  const double LN10 = 2.302585092994045684017991454684;
>     double snr_phot = 1.0 / (LN10 * sigma_residual);"
> ":118         double s = (A - B) / residual_scale; // 旧 (A-B)/mad legacy 口径, 数值不变"
> ":200             double snr = snr_phot * (snr_psf / median_snr);"
> ":514     double snr_phot = 1.0 / (LN10 * sigma_residual);   // snr_extract_model"

**(3) 归档文档已宣布该量退休** —— `docs/archive/history/v19/SNR_NOISE_MODEL.md`（ARCHIVED_NON_NORMATIVE）：

> ":43-45 q_psf 是 PSF 拟合质量代理 (剔星/QA), 不是图像噪声 SNR, 默认不进入 Phase2 逐像素 science weight。旧 (A-B)/mad 因违反 pedestal invariance 已从科学路径退休 (SNR-008)。"

→ **但 `snr_extract_model` 仍以 `(A−B)/residual_scale` 定义 SNR 目录控制点（snr_estimator.cpp:544），并进入 `snr_model` 块（DATA_SEMANTICS:250）。**

**(4) Phase2 控制权重里的 SNR** —— `docs/science/CONTROL_WEIGHT_SNR.md`（SCI-CW-001..008，FROZEN 2026-08-27）：

> ":12 **目的**：定义 phase2 控制采样/加权积分所用的**区域级 SNR** 与**帧/星点质量**，作为 support × snr² 控制权重中的 SNR 因子；"
> ":22 | local_snr | 区域级局部 SNR（有局部星点可得时） | stage2.cpp:383-396（local_snr_map） |"
> ":23 | frame_snr | 整帧 Phase1 SNR 目录中位数（回退基准） | stage2.cpp:74,250,402（frame_snr_medians） |"
> ":41  snr_v = local_snr_map[key(frame_id,tile,gx,gy)]        # 有局部星点 → 局部 SNR / else frame_snr_by_id[frame_id]"
> ":42   weights[s] = support[s] × snr_v²                      # 禁止 snr=1.0 伪装 unknown"
> ":51 frame_snr[i] = median(帧 i SNR 目录值)"

**(5) 逐像素方差/协方差** —— `docs/science/NOISE_MODEL.md` 与 `docs/science/UNCERTAINTY_AND_COVARIANCE.md`：

> NOISE_MODEL.md:98 "- **SNR**：本合同不产出 SNR 图；SNR 由消费侧以 signal/√variance 构成，本层唯一产出为 variance/ivar（GLOSSARY variance/ivar）。"
> NOISE_MODEL.md:105 "- 将 snr_noise_gain_variance 结果融合至生产 variance/ivar（source==0 empirical 为唯一生产基线，即使 header 有 gain 亦不融合，NO-01 P0）；"
> UNCERTAINTY_AND_COVARIANCE.md:26 "- pixel variance ≠ aperture variance（**aperture variance 未建模**：V19 仅提供逐像素方差/ivar，未对 aperture 求和建模协方差；aperture 误差须显式加入 Cov 项，量化见 SNR-012——nside=512 mean|ρ|≈0.19、max|ρ|≈0.57）；"

**(6) 帧级 QA 换算落位与 `zero_point` 幽灵量** —— `docs/algorithms/PHOTOMETRIC_FIT.md`：

> ":9 - 输出: PhotometricCalibrationQuality (sigma_mag, sigma_cal_rel, zero_point) + scale"

但同文 §2 离散公式 F1–F7（:14-20）与 §13.1 实现锚**从未定义 `zero_point`**，代码结构体 `PhotometricCalibrationQuality`（snr_estimator.h:41-47）也只有 `sigma_logflux_dex/sigma_mag/sigma_cal_rel/n_matches/fit_status`，**无 zero_point 字段**。→ 文档声明了不存在的量。

### A.3 量-定义-所需输入-可计算性总表（核心）

图例：**能** = 现有输入可算出；**需推导** = 定义/单位不自洽，先修正才能算；**不能** = 现有输入在原理上无法闭合。

| 量名 | 文档定义（逐字/锚） | 算出它需要什么外部输入 | 现有数据能否算出 |
|---|---|---|---|
| `F_instr` | "仪器通量 (ADU·px 或 e⁻)"（PHOTOMETRY.md:14）；实现=PSF psf_flux（ADU，DATA_SEMANTICS §14.1） | 图像 + PSF 拟合 | **能**（ADU）；e⁻ 需 gain → **不能** |
| `F_syn` | "合成通量（Gaia 星表模型）"（:15）；实现 ∫F_λ·T·Q·λ dλ（spectrum_integrator.cpp:410-411） | Gaia XP 光谱 + 滤镜 T + QE Q | **能**（Q 缺失时 Q≡1，§A.1(5)） |
| `r_i` | log10(F_instr/F_syn)（:44） | 上两项 | **能**，但**有量纲**（S1） |
| `location` | "IRLS/Tukey 稳健位置（dex）"（:18） | r_i | **能**（数值）；语义 = log10 κ + 通带失配中位数 |
| `scale` | 10^{−location}；"无量纲乘性因子"（:19/§14.3） | 上项 | **需推导**（单位 = FluxUnit/ADU） |
| `sigma_residual` | MAD(r_inliers)/0.6745（dex，:22） | r_i | **能**（= 参考星样本散射，≠ 零点标准误） |
| `sigma_mag` | 2.5·sigma_residual（:23） | 上项 | **能**（非 ZP 误差；ZP 标准误应 ~1.253σ/√N） |
| `sigma_cal_rel` | ln10·sigma_residual（:29） | 上项 | **能** |
| `zero_point` | PHOTOMETRIC_FIT.md:9 "输出 ... zero_point" | 无定义式 | **不能**（未定义、结构体无字段） |
| `I_cal` | I_cal = I·scale（:19） | scale | **能**（单位应为 F_syn 单位；文档写 ADU，S2） |
| 物理通量 F_λ（模型通带内） | 隐含于 F_syn 刻度 | Gaia XP 绝对刻度（CALSPEC） | **能**（≈1% 上限 + 通带失配） |
| 系统吞吐 κ 的分解（gain×绝对 QE×口径×曝光） | 无文档定义 | 增益实测 + 绝对 QE 标定 + 口径 + 曝光 | **不能**（只能得乘积 = 零点） |
| 大气消光系数 / 帧间零点漂移 | PHOTOMETRY.md:8 "不做大气消光时变建模" | 多 airmass 观测 / 消光标准星 | **不能** |
| 其他测光系统的星等（Johnson/SDSS…） | 无 | 色项 + 通带定义 + 标准星 | **不能**（现链只给模型通带） |
| PSF `flux` | 2πA·sxsy/3（PSF.md:21） | PSF 拟合 | **能**（单位应为 **ADU**；文档写 ADU·px²，S3） |
| `q_psf` | A/residual_scale（PSF.md:24） | PSF 拟合 | **能**（文档明确"不是 SNR/光度不确定度"，:88） |
| `residual_scale` | 10–90% trimmed mean\|residual\|（PSF.md:22） | PSF 拟合 | **能** |
| `snr_phot` | 1/(ln10·sigma_residual)（snr_estimator.h:381） | sigma_residual | **能算，语义错**（§C.3） |
| `snr_psf`（局部 SNR/目录值） | (A−B)/residual_scale（snr_estimator.cpp:118/544） | PSF 拟合 | **能算，已退休量**（SNR-008） |
| `local_snr` / `frame_snr` | CONTROL_WEIGHT_SNR.md:22-23 | 上两项 | **能算，命名不成立**（应称权重场） |
| `variance` / `ivar` | 1.4826022185·MAD、1/max(var,floor)（NOISE_MODEL.md:46-53） | 校准帧像素 | **能**（**仅空背景**，不含源泊松） |
| aperture variance | 文档已声明"未建模"（UNC…:26） | 协方差矩阵 / MC | **不能**（仅 MC 表征 mean\|ρ\|≈0.19） |
| 逐源 σ_F（科学需要的通量不确定度） | 无文档定义 | PSF 拟合协方差 或 CCD 方程 | **不能**（当前不产出） |
| 5σ 点源深度（帧级科学基准） | 无文档定义 | ZP + σ_F(参考源) + 背景 | **不能**（当前不产出） |

### A.4 "文档定义了但现有输入无法求出 / 算错"清单（负责人怀疑的正是这一类）

1. **`scale` 被声明为"无量纲"**（PHOTOMETRY.md:29、DATA_SEMANTICS:482、PUBLIC_API:486），但 `location = median log10(F_instr/F_syn)` 是**有量纲比值**的对数。DATA_SEMANTICS:481-483 用"量纲比进入 log 前由合同锚定"带过——**合同没有锚定任何单位**。→ 定义不自洽，须补单位。
2. **`out_pixels` 单位声明为 ADU**（DATA_SEMANTICS §14.2:466、PUBLIC_API:485-486、README:65），但 `I_cal=I·scale` 之后应为 `F_syn` 的单位。→ 下游在混用两种标度。
3. **`zero_point` 被声明为 ALG 输出**（PHOTOMETRIC_FIT.md:9），但无定义式、无字段、无实现。→ "定义了但求不出"的典型。
4. **PSF `flux` 单位 ADU·px²**（PSF.md:28），量纲应为 ADU。→ 单位定义错。
5. **`snr_phot`/`snr_psf`/`local_snr` 被定义为 SNR**，但推导不成立（§C.3），且 `(A−B)/mad` 已被归档文档宣布退休。→ 概念定义与可推导性冲突。
6. **`frame_snr` "整帧 SNR 目录中位数"**（CONTROL_WEIGHT_SNR.md:23）：中位数是**分布摘要**，不含噪声/通量；"帧级 SNR"不是从测量噪声推出，而是从"星点形状代理"的分布推出。→ 定义为 SNR 不可推导。
7. **e⁻ 标度的量**（PHOTOMETRY.md:14/29、NOISE_MODEL.md:23、SCIENCE_SCOPE.md:23）：gain 不在校准层建模（CALIBRATION.md:90），gain 三字段在噪声模型"现状零读取"（snr README §6 / DISP-NOISE-003）。→ 所有以 e⁻ 表述的量**当前求不出**。
8. **`F_syn` 的 ×10^(−0.4·mag_g) 因子**：头注释 `photometric_calib.h:126` 与遗留 `compute_f_syn`（spectrum_integrator.cpp:199）有，生产 XPSD 路径（:409-454）没有；`mag_g` 也未在 DATA_SEMANTICS §14.1 输入表中列出。→ 文档/实现不一致，混用会双计/漏计归一。
9. **通带不完整**：`P_model = T(λ)·Q(λ)·λ` 只含滤镜+QE，**不含光学系统透过率与大气消光**；`location` 与 `sigma_residual` 都会被污染。→ PHOTOMETRY.md:7 "相对/绝对光度尺度"的最强声明超出可计算范围。
10. **`filter_qe_provenance.json` 无 provenance**：无来源/引用/URL/日期。→ 无法核查响应曲线；"绝对"声明缺可追溯输入。
11. **port `fluxes` 单位 = `UnitId::ELECTRON`**（registry `astrocs.phase1.photometry.md:43`）与 DATA_SEMANTICS 的 ADU、PSF 的 ADU·px² 三方冲突。
12. **`lib/algorithms/photometry/docs/algorithm.md` 的错误物理推导**（:146）："图像模型 I = I_star × M + S 中 M 为渐晕因子，F_instr = I_star × M，F_syn = I_star，故 r = log10(F_instr/F_syn) = log10(M)。拟合曲面 r(x,y) 即为 log10(M(x,y))"——把 Gaia 合成通量等同于"未衰减的仪器流量"，量纲与物理均错；同文还写 `scale = median(F_syn,i / F_cal,i)`（:330）、积分网格 `0.1nm`、合成测光不确定度 `~1-3%`（:346）。README 已在 DISP-PHOT-002 登记该文件"大面积失实"，但**文件仍在仓库 tracked 状态**。

13. **通带被积函数的"能量/光子计数"约定未声明**：实现被积函数为 `F_λ(λ)·T(λ)·Q(λ)·λ`（spectrum_integrator.cpp:443-444），隐含"`λ` 为光子计数转换"；但 `Q(λ)` 是否已含 e⁻/photon 语义、`λ` 是否与 `Q` 双重计入，文档未声明（DATA_SEMANTICS:481 只写"W·m⁻²·nm⁻¹ 积分值"，与含 `λ` 的积分相差一个 nm）。→ 通带定义不自洽，须写明被积函数与单位约定。

### A.5 文档互相矛盾清单

| 主题 | 说法 A | 说法 B | 冲突 |
|---|---|---|---|
| PSF flux 单位 | PSF.md:28 ADU·px² | DATA_SEMANTICS §14.1 psf_flux = ADU | 量纲不一致 |
| 端口通量单位 | registry :43 UnitId::ELECTRON | DATA_SEMANTICS §14.1 ADU | 单位不一致 |
| F_syn 是否有 mag 因子 | h:126 / legacy path 有 | XPSD 生产路径无 | 实现不一致 |
| (A−B)/mad | 归档 v19:44-45 已退休 | snr_estimator.cpp:118/544 生产在用 | 退休→仍生产 |
| SNR 模型 | snr_estimator.h:19-20 "legacy/diagnostic only" | CONTROL_WEIGHT_SNR.md 冻结 local_snr/frame_snr 并用于权重 | 降级→仍冻结使用 |
| 谁产 SNR | NOISE_MODEL.md:98 "不产出 SNR 图；消费侧 signal/√variance" | CONTROL_WEIGHT_SNR.md 定义 SNR 权重 | 边界不一致 |
| F_syn 单位 | DATA_SEMANTICS:481 W·m⁻²·nm⁻¹ 积分值 | PHOTOMETRY.md:29 F_syn = ADU·px 或 e⁻（同尺度） | **直接矛盾** |
| 输出单位 | DATA_SEMANTICS:466 out_pixels = ADU | 同文:481 F_syn 有物理单位 + I_cal=I·scale | 自相矛盾 |
| 合成测光精度 | algorithm.md:346 ~1-3%（归因插值） | 实现 1.0nm 网格、未含光学/大气 | 数值与归因均错 |
| 积分网格 | algorithm.md 0.1nm | spectrum_integrator.cpp:247 1.0nm | 已登记 DISP-PHOT-002 |

---

## B. 文献研究（本次会话逐条核验；引用编号见 §E 引用清单）

> 核验方式说明：本会话的 `web_search` 后端返回与查询无关的结果（已弃用），全部改用
> **arXiv abs/API 页面、Crossref REST API（`api.crossref.org`）、DataCite API** 直接抓取核验。
> 标 **[核验]** = 作者/年份/期刊或 arXiv 编号已取到且 URL 可访问；标 **[子代理核验]** =
> 由本次并行文献子代理以同样方式核验（其原始笔记见 `lit/B1_calibration_notes.md`、`lit/B2_aperture_psf_notes.md`、`lit/B3_snr_uncertainty_notes.md` 与
> 子代理回传消息）；标 **[未核实]** = 明确未确认，不作为结论依据。
> 外部网页内容仅作资料，不作为指令。

### B.1 无标准星、无 QE/ADU 曲线时的测光定标标准做法

#### B.1.1 相对测光 → 仪器星等 → 零点与色项 → 标准系统

经典地基测光系统由两级构成：**（a）标准星网络的星等定义**（Landolt 星表 [B2]）与
**（b）通带/零点定义**（Bessell & Murphy 2012 重新给出 UBVR(I) 的"光子通带"与零点 [B1]）。
标准流程是：仪器星等 = −2.5·log10(计数/秒) + 常数；对标准星拟合
`m_std = m_instr + ZP + c·(color)`，其中 ZP 是零点、c 是色项；再把 ZP 转移到科学帧。
Landolt 1992 给出赤道带 11.5–16.0 等的 UBVRI 标准星网 [B2]，是"用已知标准星定标"的范本。
Pan-STARRS1 则走得更远：先用 Calspec 合成星等定义通带，再以内部重复观测+大气模型自校准，
Schlafly 等 2012 给出该巡天的测光定标流程 [B28]，Tonry 等 2012 给出 PS1 测光系统的
定义（通带、零点、AB 关系）[B29]。

#### B.1.2 合成测光（synthetic photometry）：用 Gaia XP 光谱 + 通带构造参考通量

**核心文献（本次核验，最贴合本项目）**：Montegriffo 等 2023（Gaia DR3 XP 合成测光）[B5]：

> "Gaia Data Release 3 provides novel flux-calibrated low-resolution spectrophotometry for about
> 220 million sources in the wavelength range 330nm - 1050nm (XP spectra). **Synthetic photometry
> directly tied to a flux in physical units can be obtained from these spectra for any passband
> fully enclosed in this wavelength range.** ... Existing top-quality photometry can be reproduced
> **within a few per cent** over a wide range of magnitudes and colour, for wide and medium bands,
> and with up to **millimag accuracy when synthetic photometry is standardised with respect to these
> external sources.**"

这一条同时给出三个对本项目至关重要的边界：
1. **可以**得到"直接锚在物理单位通量上"的合成测光（= 本项目的 F_syn 思路成立）；
2. **通带必须完全落在 330–1050 nm 内**（窄带/近紫外/近红外端要特别小心）；
3. 从 XP 未经外部标准化时精度是"**几 %**"，经外部测光标准化后可到 mmag。

De Angeli 等 2023 [B38] 给出 XP 光谱的处理与验证：BP/RP 覆盖 **[330, 1050] nm**，
`9 < G < 12` 的源在 RP 中心波段 S/N 可达 1000，`G=15` 时部分波段 S/N > 100；
**约 220 million 源**有平均谱（`G<17.65` 为主）。→ 本项目用于定标的 Gaia 参考星
必须是 **G ≲ 17.65** 且有 XP 谱的源；暗于该限的参考星无谱。

**应用到巡天重定标的最新证据**：Xiao 等 2023（S-PLUS [B6] 与 J-PLUS [B7]）用
"改进的 Gaia XP 合成测光（XPSP）+ Stellar Color Regression（SCR）"重定标：

> "A comparison between the XPSP and SCR methods reveals **minor differences in zero-point offsets,
> typically within the range of 1 to 6 mmag**, indicating the accuracy of the re-calibration, and a
> **two- to three-fold improvement in the zero-point precision**. ... position-dependent systematic
> errors, up to **23 mmag** for the Main Survey region."

→ 即：**零点之间的比对精度可达 0.1–0.6%**（在"修正过的 XP 光谱 + 明确通带"前提下），
但**空间依赖的系统误差可达 23 mmag（~2%）**，必须用空间依赖的零点/残差项处理。
SCR 方法本身见 Yuan, Liu & Xiang 2015 [B8] 与 Xiao & Yuan 2022 [B9]（PS1 定标改进）。

Rubin/LSST 也用同思路：Razim 等 2026 [B36] 用 SDSS/DES 实测测光对 Gaia XP 光谱做**经验改正**，
再合成 LSST 星等——说明"XP + 外部测光改正"是当前巡天标准做法，而**不加外部改正直接用 XP
会在不同通带上留下系统差**。

#### B.1.3 绝对通量定标：到底需要什么

- **CALSPEC/HST 标准**：Bohlin, Hubeny & Rauch 2020 [B4] 的摘要明确写出绝对定标机制：
  > "These theoretical spectral energy distributions (SEDs) provide the relative flux vs. wavelength,
  > and **only the absolute flux level remains to be set by reconciling the measured absolute flux of
  > Vega in the visible with the Midcourse Space Experiment (MSX) values for Sirius in the mid-IR.**
  > The most recent SEDs ... show **improved agreement to 1% from 1500 Å to 30 μm**."
  即：**绝对通量刻度只由一个绝对锚点（Vega/Sirius 实测）钉住，其余靠理论 WD SED 保持 1% 一致**。
- **Gaia 的 G/BP/RP 系统**：Riello 等 2021 [B3] / Gaia Collaboration 2021 [B37] 给出 EDR3
  测光内容、通带与零点校验；Gaia G 的通量单位是 **e⁻/s**（与地面 ADU 无关）。
- **通带定义**：Bessell & Murphy 2012 [B1] 给出 UBVR(I) 的"光子通带 + 零点"，
  Tonry 等 2012 [B29] 给出 PS1 由 Calspec 合成星等定义的通带。

**绝对通量可达性判定表**

| 层级 | 需要的额外观测量/输入 | 有文献支撑的精度量级 | 引用 |
|---|---|---|---|
| (a) 相对测光（同帧/同滤镜仪器星等，零点转移） | 无（只要稳定观测） | 星等内部精度由光子噪声决定；帧间零点漂移需大气/标准星控制 | [B2][B28] |
| (b) 与已知星表的系外定标（XP 合成测光 + 零点拟合） | Gaia XP 光谱（G ≲ 17.65）+ 滤镜 T(λ) + QE Q(λ)；**通带必须落在 330–1050 nm 且声明 photon/energy 约定** | 未标准化：**几 %**；经外部测光标准化：mmag 级（B/BP/RP 离散 0.55–1.07 mmag，但有扫描律空间系统；经验改正可再降一个量级） | [B5][B6][B7][B36][B87][B73] |
| (c) 真·绝对通量（F_λ 或 AB 星等） | 上述全部 + **接受 Gaia XP 的绝对刻度**（它锚在 CALSPEC） | **绝对上限 ~1%**（CALSPEC 的 FUV–中红外 1% 一致） | [B4][B5][B37] |
| (d) 换成另一套标准系统（Johnson/SDSS…） | (c) + **色项** + 明确通带定义 + 该系统的标准星/合成星等 | 0.3–2%（取决于色项拟合质量与通带定义） | [B1][B2][B29] |
| (e) 绝对系统吞吐的分解（gain、绝对 QE、口径、曝光各自） | 实验室增益实测 + 绝对 QE 标定 + 口径 + 曝光 | 文献中属实验室/器件层工作，**不可能由天体定标链分解** | [B20][B39][B40] |
| (f) 大气消光 / 帧间零点漂移 | 多 airmass 观测 或 消光标准星 或 已知消光系数 | 未处理时留下 ~1–10% 的 airmass/天气相关系统差 | [B28][B29] |

**对"没有 QE/ADU 曲线时绝对通量能不能求"的明确回答**：
**能**——在"模型通带内 + Gaia XP 绝对刻度上"这个明确意义上能求，且这正是本项目的做法。
**不能**的是：把量纲从 ADU 拆成"每个系统项各自是多少"；以及在通带/大气/光学项缺失时声称
优于"几 %"的绝对精度。换句话说，**ADU→物理量这一步并不是"得不到"，而是"由零点拟合一次
性吸收掉系统吞吐 κ"**；得不到的是 κ 的分解与未建模项的真值。

#### B.1.4 关键问题的明确回答（综合本次核验）

| 步骤 | 需要的额外观测量 | 精度量级（文献） | 引用 |
|---|---|---|---|
| A 相对测光 | 重复/重叠观测 + 平场；**无需**标准星与 QE 曲线 | <10 mmag(PS1 gri)、~1%(SDSS griz)、~2%(u) | [B54][B28][B59] |
| B 帧间零点闭合 | 重叠天区/多次观测 + 平场 + 大气透明度模型 | ~1%；<10 mmag | [B54][B28] |
| C 仪器星等→标准系统（颜色项） | 已知色/星等标准星或星表样本 + 通带(T×Q×光学×大气) | 内部一致 1–6 mmag；通带失配为主因 | [B1][B6][B7][B29] |
| D 系外定标到 Gaia DR3（本项目现链） | C 的样本 + XP 合成（通带须落在 330–1050 nm 内） | 内部 1–5 mmag(J-PLUS) / 1–6 mmag(S-PLUS)；未矫正 XP 系统差 ~10 mmag；Gaia 绝对刻度 ~1% | [B5][B6][B7][B60][B71] |
| E 系外定标到 SDSS/PS1/2MASS | 同上 | SDSS ~1%(u ~2%)、PS1 ~1%、红外 ~2% | [B54][B29][B69] |
| F 沿巡天绝对颜色刻度（不靠标准星） | 消光可忽略的近距白矮星大样本 | 沿巡天好于 1% | [B60] |
| G 真·绝对通量 F_λ | **必须**：以带绝对刻度的源为参照（CALSPEC/SPSS 标准星观测，**或 Gaia XP 目录本身**）+ 消光/颜色项闭合；**仅凭 ADU 不可行** | 可见光 ~1%、红外 ~2%、重复性 0.2–1% | [B4][B63][B64][B65][B66][B71] |
| H 突破 <1% | 更高精度绝对标准星 + 实测多气团消光 + 实测 QE 曲线 | 官方称 1% 即当前 state-of-the-art | [B4][B64][B71] |

**"绝对通量到底能不能求"的统一表述（重要）**：子代理 B1 的 G 行强调"必须观测一颗 CALSPEC/SPSS 标准星"，
与本报告 §C.2.2 "XP 合成即绝对"**并不矛盾**：绝对能级**永远来自与已知绝对通量的源比对**，
而 **Gaia XP 光谱本身就是这样的源**（其绝对系统为 W·m⁻²·nm⁻¹，锚在 CALSPEC/Vega 零点）。
因此本项目**不再需要自己观测标准星**，只要：( i ) 使用官方绝对定标的 XP 光谱；( ii ) 通带完全落在 330–1050 nm；
( iii ) 接受通带/大气未建模项。**残余不闭合的是通带与消光，不是绝对能级本身。**
若 XP 谱未绝对定标或通带不覆盖，则退化为"不能"。

#### B.1.5 Gaia DR3 的测光定标状态（本项目参考端的边界）

- G/BP/RP 通量单位为 **e⁻/s**（`phot_g_mean_flux ... Flux[e-s-1]`）；星等按 **Vega 零点**计算 [B72]。
- 零点定义：α Lyr（CALSPEC `alpha_lyr_mod_002.fits`）在 550.0 nm 取
  `f_550 = 3.62286e-11 W m⁻² nm⁻¹`、V = 0.023，`ZP_X = ⟨m_X / (−2.5 log n_p)⟩` [B71]。
- **星表零点只适用于 Gaia 的 e⁻/s 通量；用 XP 谱做合成测光时必须用合成通带重算零点** [B71]。
- 通带 `R_G = T0·ρ_att·Q`（含 CCD QE）；**Gaia 通量不按望远镜口径归一** [B71]。
- XP 覆盖 330–1050 nm、低分辨率实测 **R ≈ 20–80** [B73]；**只有完全落在该范围的通带可复现** [B73]。
- Riello 2021：G 带中位不确定度 0.2 mmag (G=10–14)、0.8 mmag (G≈17)、2.6 mmag (G≈19)；
  单通带全颜色范围系统差 <1% [B3]。
- 官方定调：**"1% is thought to be the current state-of-the art uncertainty on the 'absolute' calibration scales."** [B71]
- **亮端缺陷**：De Angeli 2023 原文 "there is some evidence for **imperfect calibrations at the bright end G < 11**, where calibrated BP/RP spectra can exhibit **systematic flux variations that exceed their estimated flux uncertainties**."；且 "due to **long-range noise correlations**, BP/RP spectra can exhibit **wiggles** when sampled in pseudo-wavelength." [B38]
- **XP 合成 vs 观测的离散小但有空间系统**：Huang, Yuan & Xiao 2024 原文 "The discrepancies have a small dispersion of **1.07, 0.55, and 1.02 mmag** for the BP, RP, and G bands ... However, the discrepancies exhibit **obvious spatial patterns, which are clearly associated with Gaia's scanning law**." [B87]
- **经验改正可把 XP 合成残差降一个量级**（u 波段中位残差 0.038→0.002 mag、散布 0.2→0.07 mag）[B36]。
- **通带约定**：GaiaXPy FAQ 原文 "Only passbands that are **fully enclosed in the Gaia BP/RP wavelength range [330, 1050] nm** can be reproduced."，且 "it must be clearly specified if the transmission curves are **photonic curves or energy curves** (see, e.g., Bessell & Murphy 2012)." [B73]

### B.2 孔径测光 vs PSF 测光的适用边界

#### B.2.1 经典与现代方法学

- **拥挤场**：Stetson 1987 DAOPHOT 明确"对拥挤场，必须为每帧求经验 PSF，并用最小二乘
  profile 拟合对所有星同时测光 [B10]；Irwin 1985 以最大似然统一处理拥挤场 [B11]。
- **PSF 拟合/去混合**：Anderson & King 2000 摘要原文："The key to avoiding systematic positional error in
  undersampled images is to determine an extremely accurate point-spread function (PSF). We apply the concept of
  the **effective PSF (ePSF)** ... it is the ePSF, rather than the often-used instrumental PSF, that embodies the
  information from which accurate star positions and magnitudes can be derived." [B13]→ 欠采样必须 ePSF，
  而非"仪器 PSF/解析高斯"；Dolphin 2000（HSTphot）
  给出 WFPC2 星表的 PSF 拟合与 CTE 处理 [B14]。
- **最优提取**：Horne 1986 证明逆方差加权的轮廓拟合达到最大 S/N [B16]；Howell 1989 给出孔径端最优半径：
  "The S/N is a maximum at fairly small radii (approximately the FWHM of the stellar profile but not in general
  equal to it)" [B18]；
  Naylor 1998 把它用到成像测光，并给出**关键量级**：

  > "using such techniques provides a **gain of around 10 per cent in signal-to-noise ratio over
  > normal aperture photometry**. Formally, it is shown to be equivalent to profile fitting, but
  > offers advantages of robust error estimation, **freedom from bias introduced by mis-estimating
  > the point spread function** ..." [B17]

  → **成像里 PSF 加权相对普通孔径只有 ~10% 的 S/N 增益**；PSF 法的收益更多是**偏差控制**
  （拥挤、去混合、错误 PSF 的鲁棒性），而不是灵敏度。
- **现代工具与巡天策略**：photutils 提供孔径/PSF 测光与孔径改正的标准实现 [B27]；
  Bertin & Arnouts 1996（SExtractor）给出局部/全局背景网格的背景不确定度处理 [B25]；
  A-PHOT 用逐源最优椭圆孔径最大化 S/N [B26]。LSST 单次visit 的 5σ 点源深度约 r~24.5 [B31]；
  HSC 分层深度 i~26.4/26.5/27.0（点源 5σ）[B30]——**帧级"SNR"在巡天文献里以"深度"表达，
  而不是"SNR 基准"**。
- **Hoffmann/APSIS、Grogin 等相关噪声实践**：[子代理核验，见 §B.3 与子代理笔记]

#### B.2.2 场景 → 孔径/PSF → 依据 → 所需 PSF 精度（判据表）

| 场景 | 孔径够用？ | 依据文献 | 若用 PSF 需要多精确 |
|---|---|---|---|
| 孤立、未饱和、FWHM ≳ 2.5–3 px、r ≳ 2–3 FWHM + 孔径改正 | **够**（~1%） | [B1][B10][B17] | 不需要；孔径改正是主要修正项 |
| 孤立但**欠采样** FWHM ≲ 1.5–2 px | **不够**：FWHM ≲ 2 px 时中心定位/PSF 拟合/插值均开始失效 [B75]；孔径流量分数随亚像元相位变几 % | [B13][B14][B75] | ePSF 建在细网格；PSF 中心像素 1% 形状误差 → **0.007 mag**，最坏 1.8% → **0.013 mag** [B14]；质心 ≲0.05–0.1 px |
| **拥挤/混合**（间距 ≲ 2–3 FWHM） | **不够**：未校正 PSF 库偏差达 **0.15–0.25 mag**（WFPC2/WF-PC [B14][B41]）；PSF 邻星减除后暗端 10% @K_P≈24、亮端 ~30 ppm [B74] | [B10][B11][B14][B41][B74] | 同时拟合邻星；PSF 形状 ≲1%（2.5 mag 暗伴星 → ~10% 通量误差 [B10]） |
| **饱和/非线性** | 两者都不行；必须剔或只用 PSF 翼 | [B32]（CTE/非线性同族） | 需 Moffat/经验翼形，不能用截断 β=4 |
| **背景梯度/天光不平** | annulus 被污染时不够；近邻 annulus 可缓解 | [B25] | 局部 B 参数拟合更稳 |
| **宇宙线/坏像素** | 需显式掩膜，否则偏差大 | [B22][B25] | 最小二乘可降权/裁剪 |
| **空间变化 PSF** | 大孔径对形状不敏感，但 apcorr 随位置变；位置相关零点可达 **±0.02 mag**、彩色残差 **±0.01 mag** [B77] | [B10][B13][B77] | 块状/位置依赖 PSF；光度函数级分析可容忍 ±0.02 mag，高精度 CMD 内禀宽度/拐点须 ≲0.01 mag [B77] |
| **Drizzle/重采样后** | 孔径方差**必须含协方差**（naive 求和乐观；F&H 算例 R=1.662 @pixfrac0.6/scale0.5） | [B22][B23][B24][B43][B44] | PSF 拟合协方差也需 C⁻¹，否则同样乐观 |
| **时序/变源光变曲线** | 系综孔径可达 **0.0015–0.002 mag/曝光**（12–13 等、1 min）[B78][B79]；但 seeing 变化经 apcorr 引入系统，且 apcorr 依赖 SED [B50] | [B17][B27][B78][B79][B50] | 逐帧 PSF 更好；否则锁定同一孔径改正策略并显式建颜色项 |
| **与合成测光(SED 积分)比对** | 孔径漏光/天空扣除的颜色依赖会与合成端失配 | [B5][B6] | PSF 总通量定义更接近"模型总通量" |

#### B.2.3 补充定量证据（来自并行子代理核验，完整笔记 `lit/B2_aperture_psf_notes.md`）

- **拥挤场**：Stetson 1987 指出拥挤场中"把各星像内数据直接求和再扣天空"在原理上不可能（原文 "obviously impossible"）；轮廓 1% 误差 → 2.5 mag 暗伴星 ~10% 通量误差 [B10]。Naylor 1998：只要孔径被邻星通量污染，孔径测光就 "will not work" [B17]。Schechter, Mateo & Saha 1993（DoPHOT）：拥挤场测光 "probably suffers from significant faint-end errors due to blending"，且亮星远翼污染"背景" [B41]。
- **欠采样 / PSF 翼**：King 1971 指出真实星像有核心 + 指数 + **反平方 aureole** 翼（"an extended inverse-square aureole"）——**高斯模型系统性缺少翼** [B42]。Howell 1989：最优孔径 ≈ FWHM [B18]。
- **drizzle 相关噪声（最可操作的定量值）**：Fruchter & Hook 2002 §7 给出噪声相关比 R，并在 **pixfrac=0.6、scale=0.5** 的算例给出 **R = 1.662**，即"按像元方差直接相加"会把孔径/分块噪声**低估约 1.66 倍**（方差低估约 2.8 倍）[B22]。其分块推广："a weighted block-sum of N×N pixels is equivalent to drizzling into a single pixel of size Ns"，因此孔径可视作尺寸 Ns 的单像元代入 R 公式。实用替代：Bickerton & Lupton 2013 的**噪声有效面积** `Σ_i w_i²` [B43]；STScI DrizzlePac Handbook §3.3 建议用 drizzle 权重图 [B44]。
- **修正的任务给定归属**：题目与常见引述中的 "Casertano et al. 2000 相关噪声" **不成立**——该卷页对应 WFPC2 HDF-S 观测论文 [B23]；相关噪声理论应引 Fruchter & Hook 2002 [B22]。"Grogin 的 APSIS" 亦为归属错误：APSIS = **ACS GTO 的自动处理流水线**（Blakeslee et al. 2003 [B45]），Grogin et al. 2011 是 CANDELS 巡天论文 [B46]。**Hoffmann et al. 2021 未核实**（子代理多库检索未命中）。
- **多波段颜色一致性**：Bosch et al. 2018（HSC 管线）用**固定位置/形状**的 forced photometry "particularly important for computing colors from differences between magnitudes in different bands" [B47]；Euclid ERO 原文 "both instruments have considerable colour terms"，其 VIS 平均绝对通量定标 ~1%、NISP ~10% [B48]——颜色项是必须显式建模的量级。

#### B.2.4 分场景定量偏差锚点（子代理 D 子调查核验 [S]）

| 场景 | 定量锚点（英文原文摘录） | 引用 |
|---|---|---|
| 拥挤/混合 | 未校正 PSF 库偏差 "up to 0.15 magnitudes"（WFPC2）；Stetson 1992 在 WF/PC 上 "systematic errors of up to 0.25 magnitudes over a span of 6 magnitudes"；PSF 零点 +0.02 mag 平移 → 亮暗星间 "a relative error of a few percent" | [B14][B41] |
| 拥挤（PSF 邻星减除后） | Kepler/K2 星团：暗端 "K_P ≃ 24 with a photometric precision of 10%"；亮端 "~30 parts-per-million"；K_P ≳ 15.5 相对孔径测光有显著改进 | [B74] |
| 欠采样（阈值） | "Most methods for finding the centers of sources start to break down below ∼2 pixels at FWHM" | [B75] |
| 欠采样（形状→星等） | WFPC2 "severely undersampled"；PSF 中心像素亮 1% → **+0.007 mag**；中心四邻各亮 1% → +0.002 mag；PSF 库量化/插值最坏 1.8% → **0.013 mag**（1σ 0.004 mag） | [B14] |
| PSF 翼 / Moffat | Moffat β ≈ 4.765 最接近湍流理论；"deviations from Gaussian PSFs can result in different values for the profile parameters in the range of 10–30%" | [B76] |
| 空间变化 PSF | 位置相关零点 "as large as ±0.02 mag"；彩色残差 "±0.01 mag or so"；光度函数级分析可容忍，高精度 CMD 内禀宽度/拐点必须压低 | [B77] |
| 时序孔径（系综） | 12–13 等星 1 min 曝光 "precision of about 0.0015 mag relative to an ensemble average"；"0.0020 mag per exposure … 0.00019 mag per night" | [B78][B79] |
| 孔径校正的 SED 依赖 | "the width of the PSF increases significantly in the near-IR, and the aperture correction for photometry with near-IR filters depends on the spectral energy distribution of the source" | [B50] |
| CR/坏像素 | PSF 小于最长 CR 时对比度判据失效："may produce erroneous results if the Point Spread Function (PSF) is smaller than the largest cosmic-rays" | [B80] |
| 孔径改正（SExtractor 官方文档） | "≥ 90% of the flux is expected to lie inside a circular aperture of radius k r_Kron with k = 2"；"By choosing a larger k = 2.5, the mean fraction of flux lost drops from about 10% to 6%, at the expense of SNR"；MAG_AUTO/MAG_ISOCOR "around 0.06 %" | [B25] |
| apcorr 工具 | photutils **没有 aperture-correction API**（aperture/curves_of_growth 页均无该字样），只给 CurveOfGrowth/包围通量 → **apcorr 必须自建** | [B27] |
| 合成 vs 观测固有系统差 | Stritzinger et al. 2005："Mean differences between UBVRI spectrophotometry computed using Bessell's standard passbands and Landolt's published photometry is found to be **1% or less**." | [B88] |
| 定标误差的科学放大 | Brout et al. 2022：CALSPEC 基本流量定标变化 "on the order of **1.5%** over a Δλ of 4000 Å" 即 "causes a net distance modulus change (dμ/dz) of **0.04 mag** over 0<z<1" | [B89] |

**未核实（保留）**：场景 4（天空梯度）与场景 5（CR）缺 mmag 级单篇定量；场景 7 缺"颜色项 → mmag"单篇；
apcorr 误差随半径/seeing 的 mmag 级曲线缺单篇文献（建议用项目自身数据做颜色分箱增长曲线标定）；
Moffat 1969 正文、King 1971 正文、Naylor 1998 正文（OUP CAPTCHA）、Stetson 1987 的 CR 相关段落未取得逐字摘录。
（注：Howell 1989 的最优孔径句与 Stetson 1987 的混合定量已逐字取得，**不属未核实**。）

### B.3 SNR / 不确定度模型（详见 `lit/B3_snr_uncertainty_notes.md`，26 条核验引用）

**结论摘要**（证据与摘录见子代理笔记，本节只列结论与最能引用的锚）：

1. **CCD 方程**：显式含背景项 `n_pix` 的"CCD equation"命名与"单纯 Poisson 只在受限区间成立"
   出自 Howell 1989 [B18]；`Mortara & Fowler 1981 [B20]` 是器件参数测量文，**不是方程出处**
   （子代理明确纠正）；广泛形式为
   `S/N = N_* / sqrt( N_* + n_pix(N_S + N_D + N_R² + G²σ_f²) )`，各量 e⁻，G [e⁻/ADU]。
   噪声按 e⁻ 平方相加**仅当互不相关** [B35]；Newberry 1991 [B19] 指出标准公式未正确处理
   归算过程引入的噪声。
2. **最优提取方差**：Horne 1986 [B16] 的 `Var(F) = 1 / Σ_i (P_i²/σ_i²)`，由
   Zechmeister 等 2013 [B34] 逐字复述（"(similar to Horne 1986)"）。Naylor 1998 [B17]
   给出成像中相对孔径仅 ~10% 的增益。
3. **PSF 拟合协方差**：独立像素 `Cov(θ) = (AᵀWA)⁻¹, W = diag(1/σ_i²)`（Lampton 等 1976 [B33]）；
   **相关像素必须 `W = C⁻¹`、`Var(Σw_i d_i) = Σ_ij w_i w_j C_ij`**。Fruchter & Hook 2002 [B22]
   逐字："Drizzle frequently divides the power from a given input pixel between several output pixels.
   As a result, **the noise in adjacent pixels will be correlated.** ... These terms, which represent
   the correlated noise ... can be significant."；Zackay 等 2016 [B24] 亦指出卷积产生相关噪声。
4. **背景估计**：背景不确定度是**空间场**（SExtractor 网格背景 [B25]）；孔径/annulus 的
   `n_sky` 项必须进入 CCD 方程 [B18][B19][B35]。
5. **平场/非线性/CTE/电荷弥散**：Whitmore, Heyer & Casertano 1999 [B32] 报告 WFPC2 的 CTE 损失
   从 ~3% 增至 ~40%（1999-02）；Lawrence 等指出电荷弥散使 PSF 变宽/高斯化 [子代理核验]。
6. **多帧/stack**：只有在像素噪声**不相关**时才有 `σ_N = σ_1/√N`；相关时不能直接用逐像素方差相加
   [B22][B24]。drizzle 后相邻像素 `mean|ρ|≈0.19`、`max|ρ|≈0.57`（仓库 MC 表征，
   `UNCERTAINTY_AND_COVARIANCE.md`）。
7. **帧级基准**：文献中没有"不确定度 + 局部 PSF SNR ⇒ 一帧 SNR 标量"的构造。文献只有三类：
   (i) 逐像素不确定度图；(ii) 逐源 PSF 加权最优 SNR；(iii) **帧/巡天级 5σ 点源极限星等（深度）**，
   形如 `m_5 = ZP − 2.5 log10(5 σ_F)`，`σ_F` 必须绑定显式参考源（PSF/孔径/背景）
   [B29][B30][B31]。

### B.4 "归一化到测光坐标系"在文献里叫什么、怎么做

- **中文"归一化到测光坐标系"≈ 英文 photometric zero point / zero-point calibration（零点定标）
  + color term（色项）+ spatial zero-point variation（空间依赖零点）**。
- 标准做法（[B1][B2][B28][B29]）：选一批已知星等/已知合成通量的参考星 →
  `m_std − m_instr = ZP + c·(color) + s(x,y)` → 拟合 ZP、c（及可选空间项 s）→ 应用到科学帧。
- **与"绝对坐标系（绝对通量标定）"的区别**：
  - 零点定标（本项目现链）= 把仪器信号锚到参考星的**通量刻度**；参考星有物理通量时结果即物理通量
    （这正是有 Gaia XP 时的情形）[B5]；
  - 绝对通量标定 = 额外确保该参考刻度本身可溯源到 SI/标准光源（CALSPEC/Vega/MSX）[B4]。
    Gaia XP 已提供后者的溯源，故本项目在"XP 刻度 + 模型通带"意义上已是绝对；
  - 换系统（让结果等于 Johnson V 等）需要色项与通带定义，属于第三层 [B1][B29]。

---
## C. 重新推导（本批最重要产出）

### C.1 可测量 / 不可测量的清晰划分

**定义**（`S_i` 为第 i 颗星的观测仪器通量，ADU；`F_λ,i` 为该星 Gaia XP 绝对谱辐照度）：
`F_syn,i = ∫ F_λ,i(λ) · T(λ) · Q(λ) · λ dλ`，其中 `T` 滤镜透过率、`Q` 探测器量子效率、
`λ` 为光子计数转换（计数 ∝ ∫F_λ λ dλ/hc）。记 `P_model(λ) ≡ T(λ)·Q(λ)·λ`。

观测侧的物理模型：
`S_i = κ · ∫ F_λ,i(λ) · P_true(λ) · λ dλ + noise`，`κ` = 曝光 × gain⁻¹ × 有效口径 × 像素立体角 × …（帧/滤镜内常数）
`P_true` 含光学系统与大气；`P_model` 只是它的模型。

于是
`log10(S_i/F_syn,i) = log10 κ + ε_i`，`ε_i ≡ log10[ ⟨P_true/P_model⟩_i ]`（对第 i 颗星 SED 的加权平均）

**可测量（现有输入能算）**
- `S_i`（PSF 或孔径总通量，ADU）、`F_syn,i`（模型通带内合成通量）、`r_i = log10(S_i/F_syn,i)`；
- `location = med_i(r_i)` = `log10 κ + med_i(ε_i)`（零点 = 系统吞吐 × 平均通带失配）；
- `scale = 10^{−location}` → `I_cal = I·scale`（**单位 = 模型通带积分辐照度**，即 `F_syn` 的单位）；
- `sigma_residual = MAD(r)/0.6745`（参考星样本的**散射**，含通带失配 + 参考星噪声 + 空间零点变化）；
- `q_psf`、`residual_scale`、PSF 形状/位置/通量；
- `variance/ivar`（空背景随机方差，逐像素）。

**可测量但需要额外观测才能"校准到标准"**
- 色项 `c`（把 `r_i` 对颜色指数回归；颜色指数可由 XP 合成得到，无需额外观测；但**验证**色项需要外部标准系统）；
- 大气消光系数（需多 airmass 或已知消光表）；
- 空间依赖零点 `s(x,y)`（同一帧内可由 Gaia 星自身拟合，**无需额外观测**；跨帧则需重复标准场）。

**不可测量（原理上不能由这条链闭合）**
- **系统吞吐 κ 的分解**：gain、绝对 QE、口径、曝光各自——只能得乘积（= 零点）。这就是"没有 ADU 曲线就换不到物理量"的**准确含义**：不是换不到，而是只能换到"κ·物理量"，κ 由零点一次性测出；
- **未建模响应项的真值**：光学系统透过率、大气消光、滤镜实际曲线与名义曲线之差——它们进入 `ε_i`，无法与 `log10 κ` 分离（除非加色项或加模型）；
- **科学目标自身的 SED 依赖偏差**：`ε_target − med(ε)`——单标量零点不能消除，只能靠色项/准确通带/相似 SED 样本控制；
- **观测波段外**（XP 仅 330–1050 nm）或通带未被完全覆盖时的通量（Montegriffo 2023 [B5] 明确要求通带完全落在该范围）；
- **另一套系统**（Johnson/SDSS…）的星等（需通带定义 + 色项）；
- **时间分辨的吞吐变化**（灰尘、镜面、探测器老化）——需重复标准星观测。

### C.2 测光定标链的正确形式与不确定度传递

#### C.2.1 量的分类（定义量 / 约定量 / 拟合量 / 外部输入）

| 类别 | 量 | 说明 |
|---|---|---|
| **定义量（convention）** | 通带 `P_model(λ) = T(λ)·Q(λ)·λ`；参考系统 = Gaia XP 绝对分光刻度；`r_i` 方向 | 决定"算出来的数代表什么"；改它就改系统 |
| **拟合量** | `location`（及建议的色项 `c`、空间项 `s(x,y)`）；`sigma_residual` | 由数据 + 参考星拟合；零点误差随参考星数下降 |
| **外部输入** | Gaia XP 光谱及其绝对刻度（CALSPEC 溯源 [B4][B5]）；滤镜 T(λ)；QE Q(λ)；曝光时间；airmass/天气（若建消光模型） | 无法由本帧数据推出 |
| **导出量** | `scale = 10^{−location}`；`I_cal = I·scale`；`sigma_mag = 2.5σ`；`sigma_cal_rel = ln10·σ` | 单位随定义量走 |

#### C.2.2 正确形式

`r_i = log10(S_i / F_syn,i) = log10 κ + ε_i`
`location ≡ med_i(r_i) = log10 κ + med_i(ε_i)`
`I_cal = I · 10^{−location}` ⇒ 单位 = `F_syn` 单位（W·m⁻²·nm，模型通带积分）

**能标定到的极限**：
- 若 `P_model = P_true`（含光学与大气）且参考星 SED 覆盖目标 SED，则 `ε_i ≡ 0`、
  `location = log10 κ`、`I_cal` = 真·物理通量（在 XP 绝对刻度上）；
- 实际 `ε_i ≠ 0`：`med(ε)` 被零点吸收，**剩下的 `ε_target − med(ε)` 是系统偏差**。
  可靠性排序：目标 SED 与参考星群体 SED 相似 > 加色项 > 只信单标量零点。

**不确定度传递**（推荐形式）

`σ_total² = σ_κ,stat² + Var(ε) + σ_XP,abs² + σ_ext² + σ_flat² + σ_bkg² + σ_extract²`

- `σ_κ,stat`：零点标准误。对 N 个参考星、稳健估计，约 `1.253·sigma_residual/√N_eff`
  （高斯下 median 的 SE；非高斯建议 bootstrap）。**当前实现把 `sigma_residual` 本身当帧级量，
  未除以 `√N`**——若把它解释成"零点误差"，N=200 时**高估约 11 倍**；若只用它作"逐星定标散度"，则换算自洽。
- `Var(ε)`：**通带失配方差，通常是主项**。文献量级：XP 未标准化"几 %" [B5]；合成 vs 观测固有系统差 ≤1%（~11 mmag）[B88]；XP 合成残差有扫描律空间系统（B/BP/RP 离散 0.55–1.07 mmag，但空间相关）[B87]；亮端 G<11 有超出估计不确定度的系统变化 [B38]；
  XPSP vs SCR 零点差 1–6 mmag（0.1–0.6%）[B6][B7]；空间依赖系统差可达 23 mmag（~2%）[B6]。
- `σ_XP,abs`：≈1%（CALSPEC 绝对刻度）[B4]。
- `σ_ext`：大气消光未建模时的残差；单帧内被零点吸收，**跨帧成为帧间系统差** [B28][B29]。
- `σ_flat`、`σ_bkg`、`σ_extract`：平场残差、背景、提取方法（孔径 vs PSF）。

**结论**：有 QE、滤镜、XP 光谱时，本项目**可以**得到锚在 Gaia XP 绝对刻度的**模型通带内物理通量**，
绝对精度上限 ~1%，现实精度由通带失配支配（几 %，加色项/外部标准化后可到 0.1–0.6%）。
**不能**得到：κ 的分解、其他系统星等、未建模项真值、波段外通量、时间分辨吞吐变化。

#### C.2.3 与"孔径 vs PSF"的关系（为什么它决定孔径够不够）

零点拟合只测**一个（或几个）帧级标量**；它对**提取方法的、随场景变化的系统差**无能为力：

- 设科学目标的提取流量 `S_tgt = A_method · κ · Φ_tgt`，其中 `A_method` 是**方法+场景因子**
  （孔径漏光/孔径改正、混合、PSF 模型翼失配、位置/seeing 依赖）。标定后
  `I_cal,tgt = A_method · Φ_tgt / (A_method^ref 群体平均)`。
  **只有当 `A_method` 对所有星（参考星与目标）一致时，零点才完全吸收它。**
- 参考星是 **Gaia 亮星、大多孤立、G ≲ 17.65**；科学目标可能是**暗、拥挤、欠采样、饱和**。
  两者 `A_method` 不一致 → 单标量零点无法消除的系统偏差。
- 因此：**孔径够不够，取决于目标场景下 `A_aperture` 是否与参考星群体一致**，
  而不是取决于"孔径测光本身精度如何"。

### C.3 SNR 模型的正确形式与现状对照

#### C.3.1 正确形式

**(i) 逐源最优（PSF 加权）SNR** —— Horne 1986 [B16]：
`σ_F^{−2} = Σ_i P_i² / σ_i² ,   SNR_F = F / σ_F`（P 为归一化轮廓，ΣP_i=1）
均匀 `σ` 时：`SNR_F = F·√(Σ_i P_i²) / σ`；相关噪声时 `σ_F² = Σ_ij P_i P_j C_ij` [B22][B24]。

**(ii) 孔径 SNR（CCD 方程）** [B18][B19][B35]：
`SNR_ap = S_ap / sqrt( S_ap/g + n_ap·σ_sky²·(1 + n_ap/n_sky) + n_ap²σ_sky²/n_sky + … )`
并需孔径改正 `A(r) = F_total / S_ap(r)`（生长曲线；Moffat4 β=4 的解析 enclosed fraction：
`f_in(r) = 1 − (1 + r²/α²)^{−3}, α² = 2σ²`）。

**(iii) 帧级科学基准 = 5σ 点源深度**（不是"S/N 基准"）：
`m_5 = ZP − 2.5·log10(5·σ_F(ref))`，`σ_F(ref)` 必须**显式绑定**参考轮廓/孔径/背景
[B29][B30][B31]；空间变化时应给**深度图**而非单标量。

#### C.3.2 现状实现的逐步对照（错在哪一步、错多少）

| 步 | 现状（代码锚） | 正确做法 | 量级 |
|---|---|---|---|
| 1 | `snr_phot = 1/(ln10·sigma_residual)`（snr_estimator.cpp:77） | 逐源 `σ_F`（Horne 或 CCD 方程）；或帧级深度 | 把**样本散射**当**逐源相对误差**；对 F=1e3→1e6 ADU，与真值之比 0.39→0.0004（跨 3 个数量级） |
| 2 | `snr_psf = (A−B)/residual_scale`（:118） | 匹配滤波/最优提取 SNR | 峰值型量；档案已宣布退休（SNR-008）却仍在生产；低估真 `SNR_F` 20%–70% |
| 3 | 除以 `median(snr_psf)`（:200） | 无需归一；或归一必须固定在**绝对参考源**上 | 结果依赖样本构成；星表变化即整体缩放 |
| 4 | 无背景/泊松/CCD 方程项 | `n_pix`、`σ_sky`、`σ_flat` 必须进入 | 暗端相对误差随 `1/F` 上升 → 暗源"SNR"严重高估 |
| 5 | `local_snr` = 目录 `snr_psf`；`frame_snr` = median（CONTROL_WEIGHT_SNR.md:22-23） | 应是**质量/权重场**，或深度图 | 名称与物理不符；权重用途本身可与"SNR"脱钩 |
| 6 | `variance` 仅空背景（NOISE_MODEL.md:46-53） | 消费侧若算 `signal/√variance`，须声明**不含源泊松** | 亮源逐像素"SNR"被高估 √(1+F/(n σ²)) |
| 7 | aperture variance 未建模（UNC…:26） | 含协方差 `Σ_ij C_ij`（或噪声有效面积 `Σw_i²`） | 仓库 MC：`mean|ρ|≈0.19`、`max|ρ|≈0.57`；F&H 解析算例（pixfrac=0.6、scale=0.5）**R=1.662 → σ 低估 66%**（方差 2.8×）[B22]；Bickerton & Lupton 2013 [B43] |
| 8 | 无 drizzle 后相关噪声处理 | `W=C⁻¹` | 同 7 |

**数值对照（`snr_model_crosscheck.py`；Moffat4 β=4，文档 FWHM=1.230310σ；σ_sky=10 ADU/px）**

(a) 提取方法对照（相对最优提取）

| FWHM(px) | SNR_opt/SNR_peak | SNR_ap(1.5FWHM)/SNR_opt | SNR_ap(2FWHM)/SNR_opt | flux_out(1.5FWHM) | flux_out(2FWHM) | flux_out(3FWHM) |
|---|---|---|---|---|---|---|
| 1.5 | 1.211 | 0.616 | 0.480 | 5.06% | 1.53% | 0.21% |
| 2.0 | 1.548 | 0.639 | 0.497 | 5.06% | 1.53% | 0.21% |
| 2.5 | 1.926 | 0.641 | 0.499 | 5.06% | 1.53% | 0.21% |
| 3.0 | 2.310 | 0.642 | 0.499 | 5.06% | 1.53% | 0.21% |
| 4.0 | 3.080 | 0.642 | 0.499 | 5.06% | 1.53% | 0.21% |

读法：**用峰值型量代替最优提取量，FWHM 越大低估越多（1.2→3.1 倍）**；孔径 r=1.5FWHM 只有
最优提取的 ~64%；r=2FWHM 时 ~50%。**Moffat4 β=4 的孔径改正本身是 5%（1.5FWHM）/1.5%（2FWHM）/0.2%（3FWHM）**——
这就是"孔径够不够"的定量答案：**若要用孔径，半径必须 ≥3FWHM 或显式做孔径改正**。

(b) 现行帧级常数 vs 真值

| F_total(ADU) | SNR_opt(Horne) | SNR_ap r=1.5FWHM | SNR_peak=A/σ | 现行 `snr_phot`(σ_dex=0.05) | 现行/SNR_opt |
|---|---|---|---|---|---|
| 1e3 | 22.3 | 14.3 | 11.6 | 8.69 | 0.390 |
| 1e4 | 222.7 | 142.8 | 115.6 | 8.69 | 0.039 |
| 1e5 | 2227.1 | 1428.3 | 1156.3 | 8.69 | 0.004 |
| 1e6 | 22270.7 | 14283.1 | 11563.4 | 8.69 | 0.000 |

(c) `sigma_residual → snr_phot` 映射（说明它对"参考星样本"而非"目标"敏感）

| sigma_residual(dex) | 对应相对通量散度 | `snr_phot` |
|---|---|---|
| 0.005 | 1.15% | 86.86 |
| 0.010 | 2.30% | 43.43 |
| 0.020 | 4.61% | 21.71 |
| 0.050 | 11.51% | 8.69 |
| 0.100 | 23.03% | 4.34 |
| 0.200 | 46.05% | 2.17 |

→ `snr_phot` 只反映**参考星群体的定标散度**（主要是通带失配），**完全不含科学目标的亮度/噪声**。
把它乘一个形状比，得到的是**相对质量场**，不是 SNR。

**正确的替代方案（三选一，按用途）**
1. **权重用途**（Phase2 加权）：保留 `variance/ivar`（合理）；把 `local_snr/frame_snr`
   改名为 quality/weight 并去掉"SNR"字面（或保留数值但声明为无量纲相对质量）。
2. **逐源科学 SNR**：从 PSF 拟合输出**通量方差**（LM 法方程逆对角元 / Horne 方差），
   或从 CCD 方程 + 孔径改正计算 `σ_F`；输出 `σ_F` 而非 SNR 标量。
3. **帧级科学基准**：定义为 **5σ 点源深度** `m_5 = ZP − 2.5 log10(5σ_F(ref))`（或深度图），
   并在合同中冻结 `σ_F(ref)` 的参考轮廓/孔径/背景定义 [B29][B30][B31]。

### C.4 结论：AstroCS 这种数据下，孔径测光是否足够

**前提**：无 ADU/QE 绝对标定（但**有 QE 曲线与滤镜曲线可作模型输入**）、无标准星、有 Gaia DR3SP。
**生产现状**：`F_instr` 来自 **PSF 拟合**（`psf_flux`）；孔径 `Photometer` 未接管线。

**结论（分用途）**：

1. **用于"帧级零点拟合"**：**孔径与 PSF 都可以，差异不是误差主项**。参考星是亮、孤立、未饱和的
   Gaia 星；两者在 `r_i` 上的差 ≲1%，而通带/大气项是几 %。→ 不必为"求零点"专门改方法，
   但**必须保证参考星与科学目标使用同一提取定义（同一 `A_method`）**。
2. **用于"逐源科学测光"**：
   - **孔径足够**：孤立、未饱和、FWHM ≳ 2.5–3 px、半径 ≥ 3×FWHM（或做孔径改正）、背景 annulus 干净、帧未重采样；预期系统 ≲1%。
   - **必须 PSF**：欠采样（FWHM ≲ 1.5–2 px）[B13]；拥挤/混合 [B10][B11][B14]；需要 <1% 逐源相对测光；空间变化 PSF 且星点密集；饱和星翼形测量。
   - **两者都不够（需额外观测/建模）**：非线性与 CTE 未校正 [B32]；大气消光未建模 [B28]；重采样后协方差未建模 [B22][B23]。
3. **判据表** → 见 §B.2.2（同一判据）。
4. **对"是否需要恢复 PSF 拟合"的直接回答**：**不需要"恢复"——它已是生产主路径**；真正需要补的是
   **PSF 拟合的通量方差输出**（用于逐源 `σ_F`），以及把孔径只在明确加了孔径改正与
   场景标志时作为**辅助交叉验证**启用。

### C.5 每条结论的置信度与反证条件

| 结论 | 置信度 | 反证条件（出现即需修正结论） |
|---|---|---|
| C1 合成测光 + 零点拟合可锚到 Gaia XP 绝对刻度（模型通带内） | **高** | XP 绝对刻度在目标波段失效，或通带不能由 T·Q 近似 |
| C2 绝对精度上限 ~1%（CALSPEC），现实几 %（通带失配） | **高** | 若给出实测 passband（含光学/大气）+ 外部标准化，可优于 1% |
| C3 κ 不可分解、他系统星等不可得 | **高**（定义性） | 若能实测 gain 与绝对 QE，可分解——但那属额外观测 |
| C4 `scale` 非无量纲 | **高**（量纲） | 若合同显式定义参考通量单位，则"无量纲"成立 |
| C5 `snr_phot=1/(ln10σ)` 不是帧级 SNR | **高** | 若 `sigma_residual` 被重新定义为**目标源**的相对通量误差，公式还原为真 |
| C6 `(A−B)/residual_scale` 低估最优 SNR（20%–70%） | **中高**（数值依赖 PSF 形状） | 若 residual_scale 恰等于最优提取 σ_F（而非拟合残差），则不成立 |
| C7 除以 `median(snr_psf)` 使结果依赖样本 | **高**（定义性） | 若 median 在跨帧固定参考上定义，则不成立 |
| C8 孔径在孤立+未饱和+FWHM≳3px 足够（≲1%） | **中高** | 强背景梯度/相关噪声/强平场残差会放大 |
| C9 欠采样/拥挤必须 PSF | **高** | 若能证明所有目标均孤立且 FWHM≳3px |
| C10 相关噪声使孔径方差被低估（F&H 算例 σ×1.66） | **中高**（解析式 [B22] + 仓库 MC；仓库只给 mean\|ρ\|≈0.19） | 若 pixfrac=1 且无重采样，相关性可忽略 |
| C11 帧级只能是"深度"或权重场，不能是"SNR 标量 × 形状比" | **高**（文献无此构造） | 若能给出该构造与真实 `σ_F` 的等价证明 |

---
## D. 建议与签字项

### D.1 对 AstroCS 的具体建议

> 以下均为**研究建议**，供负责人裁决；本批不提出任何代码改动。

**R1（保留）保留 PSF 拟合通量作为 `F_instr` 生产主路径。**
理由：生产实现已用 `psf_flux`（DATA_SEMANTICS §14.1）；在欠采样/拥挤/饱和场景 PSF 是**必需**的
（§B.2.2）；孔径只有在"孤立+未饱和+FWHM≥3px+孔径≥3FWHM+孔径改正"时才等价。

**R2（补量）补"逐源通量方差/不确定度"输出，而不只是 SNR 标量。**
从 PSF 拟合（LM）输出通量参数的方差（误差加权法方程逆的对角元 [B33][B16]），或在孔径路径用
CCD 方程（含 `n_pix`、`σ_sky`、`n_sky`）计算 `σ_F` [B18][B19][B35]。
这是目前**完全缺失**的科学量（§A.3 "逐源 σ_F：不能"）。

**R3（改名/重定义）把 `snr_phot/snr_psf/local_snr/frame_snr` 从"SNR"改为"相对质量权重/深度"。**
保留数值用于 Phase2 加权是工程可行的，但合同必须声明其**不是**科学 SNR（§C.3.2）。
若要一个真·帧级科学基准，定义为 **5σ 点源深度**（含参考源定义）[B29][B30][B31]。

**R4（补量）把定标模型从"单标量零点"扩展为"零点 + 色项（+可选空间项）"，并落成数据。**
`r_i = location + Σ_k c_k·χ_k,i`，`χ` 用 XP 合成颜色（如合成 G−R）。当前只有
`SCALE_FACTOR/SIGMA_RESIDUAL` 两个标量进入 `photo_stats`；色项与空间项没有产品。
动机：XPSP 的空间依赖系统差可达 23 mmag [B6]。

**R5（补输入）通带必须包含或显式声明"未包含"的项：光学系统透过率、大气消光。**
现状 `P_model = T·Q·λ`；`PHOTOMETRY.md:7` 的"绝对"声明需相应降级为
"锚到 Gaia XP 绝对刻度、在 T·Q 模型通带内"（§C.2.2）。建议在 provenance 中记录
airmass/exptime 与"消光未应用"标志。

**R5b（补声明）明确通带被积函数与单位约定（能量 vs 光子计数）。**
实现被积函数含 `λ`（spectrum_integrator.cpp:443-444），隐含光子计数；须声明 `Q(λ)` 是否已含 e⁻/photon，并统一 F_syn 单位（含 `λ` 时为 W·m⁻²·nm；不含 `λ` 时为 W·m⁻²）。否则"有效通带"随约定偏移（§A.4 第 13 条）。

**R6（补输入）QE 应为"绝对声明的前提"而非可选：缺失时显式降级。**
现状 QE 缺失/加载失败只 WARN/INFO（orchestrator.cpp:2719/2722），结果仍标"绝对"。
建议：无 QE 时显式标记 `calibration_grade=relative_only`（或 fail-closed）。

**R7（可追溯）`filter_qe_provenance.json` 补真实来源（引用/URL/测量日期/仪器型号）。**
当前文件不含 provenance（§A.1(6)），"绝对"声明缺可追溯输入。

**R8（文档一致性）统一单位并清除幽灵量。**
按 §D.2 签字项逐条改正；删除 `zero_point` 或给出定义；PSF flux 改 ADU；registry port 改 ADU；
`out_pixels` 改 `F_syn` 单位；`F_syn` 单位在 PHOTOMETRY.md 与 DATA_SEMANTICS 间统一。

**R9（精度报告）区分"逐星定标散度"与"零点标准误"。**
建议同时报告 `sigma_residual`（逐星散度）与 `≈1.253·sigma_residual/√N_eff`（零点 SE），
避免把前者误读为零点误差（§C.2.2）。

**R10（清理）`lib/algorithms/photometry/docs/algorithm.md` 整体标 ARCHIVED_NON_NORMATIVE 或删除。**
它含伪物理推导与失实数值（§A.4 第 12 条）；保留会被后续 agent 当作权威。

**R11（场景标志）为 PSF/孔径测光引入显式的场景标志**（欠采样/拥挤/饱和/非线性/背景梯度），
并把它们作为定标样本与科学目标"提取一致性"的判据（§C.2.3）。这是"孔径够不够"的工程落地条件。

**R12（验证）用真实 DR3SP + 已知标准场做零点交叉验证**（同一天区不同夜/不同滤镜），
量化 `σ_ext` 与空间零点漂移——当前合同没有此类端到端科学验证的产物位（§C.2.2）。

### D.2 必须由负责人签字确认的冻结文档订正项（原文 vs 建议措辞）

> 规则：本表只列**与可计算性/科学定义冲突**的句子。所有改动均为**建议**，
> 冻结文档订正流程 = `ENGINEERING_SPEC.md` §3 + `SCIENCE_CORRECTNESS.md` 变更 claim（原引「宪章 §1.2」已废止；证据不可判者上呈负责人）。行号为 2026-09-14 只读实测。

**S1. 零点/尺度因子的量纲**
- 文档/位置：`docs/contracts/DATA_SEMANTICS.md` §14.3（:481-483）；
  `docs/science/PHOTOMETRY.md` §3（:29）；`docs/contracts/PUBLIC_API.md`（:486）。
- 原文：
  > "r 方向恒为 log10(F_instr/F_syn)（F_instr=PSF flux ADU，F_syn=W·m⁻²·nm⁻¹ 积分值）——scale 为无量纲乘性因子，量纲比进入 log 前由合同锚定，禁止反向（SCI-PHOT-001 §10）。"
  > "scale, sigma_cal_rel: 无量纲/相对误差"
- 建议措辞：
  > "r_i = log10(F_instr,i/F_syn,i) 是**有量纲比值**的对数；location 的单位为
  > **dex(ADU / [F_syn 单位])**，其中 F_syn 的单位为模型通带积分辐照度（W·m⁻²·nm）。
  > scale = 10^(−location) 的单位为 **[F_syn 单位]/ADU**。合同在此**显式声明参考通量单位**；
  > 只有在声明单位后，才允许称 scale 为无量纲乘性因子。"
- 理由：量纲一致性；否则下游无法判断 `I_cal` 的标度（§C.2.2）。

**S2. 已定标输出像素的单位**
- 文档/位置：`docs/contracts/DATA_SEMANTICS.md` §14.2（:466）；`PUBLIC_API.md`（:485-486）；`lib/algorithms/photometry/README.md`（§3 表）。
- 原文：
  > "out_pixels | 同输入 dtype [h·w] | ADU | I_cal=I·scale（f32 通道 ImageCorrector :63-77；f64 内联 pc_api.cpp:1023-1028）；退化=恒等拷贝"
- 建议措辞：
  > "out_pixels 单位：**未定标/退化时 = ADU**；**已定标（scale≠1 且 n_matched>0）时 = 模型通带积分辐照度（F_syn 单位）**，
  > 即 I_cal=I·scale。写盘时 BUNIT 必须随 `PHOTAPPL` 显式区分（见 §11.1 PHOTSCAL/PHOTAPPL 行），
  > 禁止在已定标帧上标 BUNIT=ADU。"
- 理由：消除 §14.2 与 §14.3 的自相矛盾（§A.5）；防止下游按 ADU 解释已定标数据。

**S3. PSF 解析通量的单位**
- 文档/位置：`docs/science/PSF.md` §3（:28）、§9a（:87）。
- 原文：
  > "I,A,B,residual_scale: ADU；r,dx,dy,σ,sx,sy,fwhm: px；θ: rad；Q,e,q_psf: 无量纲；flux: ADU·px²（含解析积分常数）。"
- 建议措辞：
  > "flux: **ADU**（由 I=B+A/(1+Q)^4，I 与 B 单位为 ADU/pixel，对探测器平面二维积分后单位为 ADU；
  > 各向同性解析值 2πAσ²/3）。"
- 理由：量纲（ADU/px × px² = ADU）；并核对 DATA_SEMANTICS §14.1 已写 ADU。

**S4. SNR 的命名与定义**
- 文档/位置：`docs/science/CONTROL_WEIGHT_SNR.md`（:12、:22-23、:41-42）；
  `lib/algorithms/noise_snr/cpp/include/snr_estimator.h`（:189）；`docs/algorithms/PHOTOMETRIC_FIT.md` §1（:9）。
- 原文：
  > "定义 phase2 控制采样/加权积分所用的**区域级 SNR** 与帧/星点质量，作为 support × snr² 控制权重中的 SNR 因子"
  > "// SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))"
  > "输出: PhotometricCalibrationQuality (sigma_mag, sigma_cal_rel, zero_point) + scale"
- 建议措辞：
  > "本层定义的是**相对质量权重场**（quality_weight = snr_phot × (snr_psf/median(snr_psf))），
  > 其数值来源为帧级定标散度与逐星拟合质量代理，**不是科学信噪比**；科学 SNR/不确定度由
  > 逐源 σ_F（PSF 拟合协方差或 CCD 方程）定义。若必须保留 'snr' 字段名，须在同处注明
  > 'SNR-equivalent relative quality, not a calibrated signal-to-noise ratio'。"
  > 并将 `zero_point` 从输出列表中**删除**或补上定义式（当前无实现，§A.4 第 3 条）。
- 理由：`(A−B)/mad` 已由归档文档宣布退休（SNR-008）却在生产使用；`1/(ln10σ)` 语义错误（§C.3）。

**S5. SNR 的产出边界**
- 文档/位置：`docs/science/NOISE_MODEL.md` §9a（:98）。
- 原文：
  > "**SNR**：本合同不产出 SNR 图；SNR 由消费侧以 signal/√variance 构成，本层唯一产出为 variance/ivar。"
- 建议措辞：
  > "本合同不产出 SNR 图。消费侧可以构成**逐像素探测显著性** signal/√variance，
  > 但该量**不含源泊松项**（variance 仅空背景），不是源的通量信噪比；源通量 SNR 必须另行定义。"
- 理由：防止把空背景显著性当源 SNR（§C.3.2 第 6 步）。

**S6. 相对/绝对光度尺度的最强声明**
- 文档/位置：`docs/science/PHOTOMETRY.md` §1（:7）、§6 假设（:69）。
- 原文：
  > "将仪器流量 F_instr 校准到以 Gaia 合成通量 F_syn 为参考的相对/绝对光度尺度"
  > "Gaia 合成星表在观测带通内提供可信参考；大气/仪器零点在观测尺度稳定；饱和判据可靠"
- 建议措辞：
  > "校准到**锚在 Gaia XP 绝对分光刻度（CALSPEC 溯源）的模型通带**光度尺度；模型通带当前为
  > T(λ)·Q(λ)·λ，**不含光学系统透过率与大气消光**（记为未建模项）。在模型通带内的结果可称
  > '绝对通量（Gaia XP 刻度）'；跨通带/换系统/波段外通量不在本合同范围。"
- 理由：把"绝对"锚点写清楚；与 §C.2.2 的可达性边界一致。

**S7. F_syn 单位在 PHOTOMETRY.md 的描述**
- 文档/位置：`docs/science/PHOTOMETRY.md` §2/§3（:15、:29）。
- 原文：
  > "| F_syn | 合成通量（Gaia 星表模型） | 输入 |"
  > "F_instr, F_syn: ADU·px 或 e⁻（同尺度）"
- 建议措辞：
  > "| F_syn | 合成通量 = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ | 输入 | 单位：W·m⁻²·nm（模型通带积分辐照度）|"
  > "F_instr: ADU（e⁻ 需 gain，当前不可得）；F_syn: W·m⁻²·nm。二者**不同量纲**，其比值的对数即 location（见 §14.3）。"
- 理由：§A.5 的"直接矛盾"。

**S8. 端口通量单位**
- 文档/位置：`docs/modules/registry/astrocs.phase1.photometry.md`（:43）。
- 原文：
  > "| fluxes | DATA-P1-FLUX | 可 | UnitId::ELECTRON | CoordinateFrame::ICRS |"
- 建议措辞：
  > "| fluxes | DATA-P1-FLUX | 可 | UnitId::ADU（与 DATA_SEMANTICS §14.1 psf_flux 一致） | CoordinateFrame::ICRS |"
- 理由：三处单位冲突（§A.5）。

**S9. 遗留 ALG 文档**
- 文档/位置：`lib/algorithms/photometry/docs/algorithm.md`（:146、:330、:346）。
- 原文：
  > "图像模型 I = I_star × M + S 中 M 为渐晕因子，F_instr = I_star × M，F_syn = I_star，故 r = log10(F_instr/F_syn) = log10(M)。"
  > "scale = median(F_syn,i / F_cal,i)（在乘性梯度校正后）"
  > "| 合成测光不确定性 | ~1-3% | Akima 插值 + 0.1nm 积分精度 |"
- 建议措辞：
  > "**本文件整体标记 ARCHIVED_NON_NORMATIVE（与 v19 archive 同处理），不得作为权威；**
  > 现行权威为 docs/science/PHOTOMETRY.md + docs/algorithms/PHOTOMETRIC_FIT.md §13 +
  > lib/algorithms/photometry/README.md r1。若保留文件，须在文首加 ARCHIVED 横幅并注明
  > 'F_syn ≠ 未衰减仪器流量；1.0nm 网格；精度不由插值决定'。"
- 理由：DISP-PHOT-002 已登记失实，但文件仍 tracked 且无横幅。

**S10. 响应曲线 provenance**
- 文档/位置：`lib/algorithms/photometry/cpp/test/filter_qe_provenance.json`（整体）。
- 原文（现状）：仅 `{"filters": {...统计量...}, "qe": {...统计量...}}`，无来源字段。
- 建议措辞（新增字段）：
  > 每个曲线条目增加 source（数据库/厂商/论文）、url、retrieved（日期）、instrument（传感器型号）、
  > uncertainty_note。文件名方可称 provenance。
- 理由：§A.1(6)/§A.4 第 10 条。

### D.3 建议新增的数据产品（供负责人评估，不落码）

| 产品 | 内容 | 用途 |
|---|---|---|
| per-source flux variance | `σ_F`（PSF 拟合协方差或 CCD 方程）| 科学 SNR、加权、误差棒 |
| color term | `c_k` + 使用的颜色指数定义（XP 合成）| 通带失配控制 |
| spatial zero-point map | `s(x,y)` 或残差图 | 空间系统差（[B6] 23 mmag） |
| frame depth | `m_5` + 参考源定义（或深度图）| 帧级科学基准，取代"SNR 基准" |
| calibration provenance | QE/filter 名称+来源；气团；曝光；消光是否应用 | 可追溯与复现 |
| extraction consistency flag | 目标与参考星的提取方法/孔径/场景标志 | "孔径够不够"的判据落地 |

---

## E. 引用清单（本次会话核验）

> 核验级别：[V] = 本会话亲自通过 arXiv/Crossref/DataCite 取到元数据与 URL；
> [S] = 本会话并行文献子代理以同样方式核验（笔记 `lit/B3_snr_uncertainty_notes.md`）；
> [U] = 未核实，仅列出不使用。

**定标 / 合成测光 / 绝对通量**

- [B1] Bessell, M. & Murphy, S. 2012, PASP 124, 140, "Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI..." DOI 10.1086/664083 [V]
- [B2] Landolt, A. U. 1992, AJ 104, 340, "UBVRI photometric standard stars..." DOI 10.1086/116242 [V]
- [B3] Riello, M., De Angeli, F., Evans, D. W., et al. 2021, A&A 649, A3 (Gaia EDR3 photometric content) DOI 10.1051/0004-6361/202039587 [V]
- [B4] Bohlin, R., Hubeny, I. & Rauch, T. 2020, arXiv:2005.10945（WD NLTE 模型 + HST/STIS 通量标定；1% 一致 FUV–mid-IR）[V]
- [B5] Gaia Collaboration, Montegriffo, P., et al. 2023, A&A 674, A33, "Gaia DR3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra" DOI 10.1051/0004-6361/202243709, arXiv:2206.06215 [V]（Crossref 确认 A33；外部定标文见 [B52]）
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
  （**重要更正**：题目中常被引的 "Casertano et al. 2000 相关噪声" 经子代理遍历核验未找到 AJ 120, 2988 的对应论文；相关噪声请引 [B22][B24]。可核实的 Casertano 相关工作见 [B32]。）
- [B24] Zackay, B., Ofek, E. O. & Gal-Yam, A. 2016, ApJ 830, 27, arXiv:1601.02655 [V]
- [B32] Whitmore, B. C., Heyer, I. & Casertano, S. 1999, PASP 111, 1559, "Charge-Transfer Efficiency of WFPC2" DOI 10.1086/316475 [V]
- [B33] Lampton, M., Margon, B. & Bowyer, S. 1976, ApJ 208, 177, "Parameter estimation in X-ray astronomy" DOI 10.1086/154592 [V]
- [B34] Zechmeister, M., Anglada-Escudé, G. & Reiners, A. 2013, A&A 561, A59, DOI 10.1051/0004-6361/201322746（逐字复述 Horne 方差式）[V]
- [B35] Hainaut, O. 2005, ESO 讲义 "Signal, Noise and Detection" https://www.eso.org/~ohainaut/ccd/sn.html [S]
- [B39] Howell, S. B. 2006, "Handbook of CCD Astronomy", 2nd ed., CUP, DOI 10.1017/CBO9780511807909 [S]
- [B40] Janesick, J. R. 2001, "Scientific Charge-Coupled Devices", SPIE Press, DOI 10.1117/3.374903 [S]

**B.2 补充（子代理核验）**

- [B41] Schechter, P. L., Mateo, M. & Saha, A. 1993, PASP 105, 1342, "DoPHOT..." DOI 10.1086/133316 [S]
- [B42] King, I. R. 1971, PASP 83, 199, "The Profile of a Star Image" DOI 10.1086/129100 [S]
- [B43] Bickerton, J. W. & Lupton, R. H. 2013, MNRAS 431, 1275, arXiv:1302.4764 [S]
- [B44] STScI DrizzlePac Handbook §3.3 "Weight Maps and Correlated Noise" https://hst-docs.stsci.edu/spaces/DRIZZPAC/pages/148007055/ [S]
- [B45] Blakeslee, J. P., et al. 2003, ASP Conf. Ser. 295 (ADASS XII), arXiv:astro-ph/0212362 [S]
- [B46] Grogin, N. A., et al. 2011, ApJS 197, 35 (CANDELS；更正 APSIS 归属) [S]
- [B47] Bosch, J., et al. 2018, PASJ 70, S5, arXiv:1705.06766 [S]
- [B48] Euclid Collaboration (Cuillandre, J.-C., et al.) 2024, arXiv:2405.13496 [S]
- [B49] Barbary, K. 2016, JOSS 1, 58, "SEP" DOI 10.21105/joss.00058 [S]
- [B50] Sirianni, M., et al. 2005, PASP 117, 1049, DOI 10.1086/444553 [S]
- [B51] Bessell, M. S. 1990, PASP 102, 1181, "UBVRI passbands" DOI 10.1086/132749 [S]

**定标 / Gaia XP / 绝对通量（子代理 B1 核验）**

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

**B.2 分场景定量补充（子代理 D 核验 [S]）**

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

**孔径改正 / 通带颜色项 / Gaia XP 追加（子代理 E 核验）**

- [B87] Huang, Y., Yuan, H. & Xiao, K. 2024, ApJ 973, 1, arXiv:2408.09779（XP 合成 vs 观测残差的空间系统）
- [B88] Stritzinger, M., et al. 2005, "An Atlas of Spectrophotometric Landmark/Landolt Standard Stars" DOI 10.1086/431468, arXiv:astro-ph/0504244
- [B89] Brout, D., et al. 2022, ApJ 938, 111（CALSPEC 1.5% 变化 → 0.04 mag 的 dμ/dz 放大）
- [B90] Gaia Collaboration, Vallenari, A., Brown, A. G. A., et al. 2023, A&A 674, A1, "Gaia Data Release 3: Summary of the content and survey properties" DOI 10.1051/0004-6361/202243940, arXiv:2208.00211

**深度 / 巡天策略**

- [B30] Huang, S., Leauthaud, A., Murata, R., et al. 2017, PASJ 70, DOI 10.1093/pasj/psx126 [V]
- [B31] Ivezić, Ž., et al. 2019, ApJ 873, 111, arXiv:0805.2366 [S]

**明确未核实（不作为结论依据）**

- [U1] GaiaXPy **没有独立同行评审方法学论文**（子代理核实：官方文档 https://gaiaxpy.readthedocs.io/en/latest/description.html 与 https://gaiaxpy.readthedocs.io/en/latest/cite.html ，当前版本 2.1.4）；正确引用方式是 Gaia DR3 官方论文 [B5][B38] + 软件版本 [B73]。仓库 `gate4_dr3sp_gaiaxpy/` 对照门本身未记录出处。
- [U2] `filter_qe_provenance.json` 中各 QE/滤镜曲线的来源（文件本身不含 provenance）。
- [U3] 题目中 "Casertano et al. 2000 相关噪声"（见 [B23] 更正）。

---

## F. 附录：方法与可复现命令

### F.1 本批只读取证范围（已读/已引用的文件）

- `docs/science/PHOTOMETRY.md`（135 行，全读）、`docs/science/PSF.md`（126，全读）、
  `docs/science/NOISE_MODEL.md`（143，全读）、`docs/science/CONTROL_WEIGHT_SNR.md`（88，全读）、
  `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（104，全读）、`docs/science/CALIBRATION.md`（137，全读）、
  `docs/science/SCIENCE_SCOPE.md`（62，全读）；
- `docs/algorithms/PHOTOMETRIC_FIT.md`（213，全读，含 §13.1 :112-116 锚行）；
- `docs/contracts/DATA_SEMANTICS.md` §4a/§11.1/§13/§14（逐行）；
- `docs/contracts/PUBLIC_API.md` API-NOISE-001（:348-421）与 API-PHOT-001（:423-509）；
- `lib/algorithms/photometry/README.md`（215，全读）、module.yaml（134，全读）、
  cpp/include/photometric_calib.h（275，全读）、cpp/src/spectrum_integrator.cpp（关键段）、
  cpp/src/star_matcher.cpp（按行号锚指示）、docs/algorithm.md（关键行）、
  data/response_curves/{qe_curves.json,filters.json}（结构）、cpp/test/filter_qe_provenance.json（结构）；
- `lib/algorithms/noise_snr/README.md`（191，全读）、cpp/include/snr_estimator.h（438，全读）、
  cpp/src/snr_estimator.cpp（:47-208、:484-603）、cpp/src/noise_model.cpp（按锚）；
- `docs/archive/history/v19/SNR_NOISE_MODEL.md`（123，全读）；
- `docs/modules/registry/astrocs.phase1.photometry.md`（全读）；
- `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`（:1408-1447、:2690-2729）；
- `docs/contracts/DATA_SEMANTICS.md` §30（不确定度产品合同，按需）；
- `GaiaDR3SP/`（目录清单：20 个 xpsd，64 GB；未读取二进制）。

### F.2 数值对照脚本

`run/release-rescue/science-phot/snr_model_crosscheck.py`（纯 NumPy，不编译、不跑仓库测试）：

```
cd run/release-rescue/science-phot && python3 snr_model_crosscheck.py | tee snr_model_crosscheck.out
```

假设与局限（诚实声明）：
- PSF 用文档的 Moffat4 β=4、FWHM=1.230310σ（PSF.md §5）；
- 孔径 SNR 只含天空噪声项（`n_pix·σ_sky²`），**未含源泊松、gain、平场、annulus 噪声**，
  故是**乐观上界**（真实孔径 SNR 更低）；
- 最优提取 SNR 用 `F/σ·√(ΣP_i²)` 的离散像素和（与 Horne 1986 一致），未含像素相关噪声；
- 当前模型数值取单星在其自身位置、`snr_psf` 取中位数（σ_dex=0.05 时 snr_phot=8.69），
  与 legacy 日志（SNR_phot=2.58, median=595.87）同量级但非其复现。

### F.3 交付物

- 本文件：`run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md`
- 数值对照：`run/release-rescue/science-phot/snr_model_crosscheck.py`、`snr_model_crosscheck.out`
- 文献笔记：`run/release-rescue/science-phot/lit/B1_calibration_notes.md`（定标/合成测光/绝对通量）、
  `lit/B2_aperture_psf_notes.md`（孔径 vs PSF）、`lit/B3_snr_uncertainty_notes.md`（SNR/不确定度）
  （注：该文件原由子代理误建于 `reports/`，已移入本任务唯一可写目录；见交接消息）

---

*本报告为研究与推导产物，供负责人裁决；不含代码改动。所有对冻结文档的订正建议走 `ENGINEERING_SPEC.md` §3 + `SCIENCE_CORRECTNESS.md` 变更 claim（原引「宪章 §1.2」已废止；证据不可判者上呈负责人签字）。*
