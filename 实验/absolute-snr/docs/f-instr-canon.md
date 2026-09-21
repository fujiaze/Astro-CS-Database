# F-INSTR 定案 —— AstroCS 星点通量口径（RELEASE-02 裁决 A6）

> 工作项：**F-INSTR-SURVEY**（裁决 A6）。实验代码：`实验/SCI-B/code/reverse_verify/f_instr/`。
> 逐条文献/开源软件记录：`实验/SCI-B/docs/surveys/f-instr-survey.md`（本文件引用其编号 `[F-xx]`，**核对状态以该文件为准**）。
> 中间产物：`run/reverse_verify/f_instr/`。**本工作项不改 `lib/` `docs/` `eng/tests/` `ci/`，零 git 写。**

---

## 0 结论速览

| 项 | 结论 |
|---|---|
| **病灶** | 现生产 `F_instr` = **5×5 固定盒 + 正性截断**（`star_detector.cpp:139-151` 的 `m00`）。它把**视宁度**读成了**乘性增益**。 |
| **量级** | seeing 2.0→4.0 px：该口径回收通量变化 **−0.72 等**（假增益 **0.516×**）；同一次变化在孔径 r=2/3/4/6/10 上给出 **0.493/0.672/0.808/0.925/0.985** —— **与 Q1 的 P1-3「跨望远镜 ~0.5 等」和「孔径依赖峰峰 7.78/4.93/3.81/4.25%」同源**。 |
| **定案** | `F_instr` 改用 **PSF 总通量**：主口径 = **PSF 拟合总通量**（`flux = 2πA·s_x·s_y/3`，β=4 族，与 `docs/science/PSF.md` 一致）；低 S/N 精化 = **PSF 加权最优提取**（Horne/Naylor）。 |
| **孔径无关性** | 实测 over seeing 1.5–5.0 px：**M_seeing ≤ 0.004 mag**，假增益 0.998（真值 1.000）。生产口径同测为 **1.353 mag / 0.516**。 |
| **符合性** | 冻结合同 `docs/science/PHOTOMETRY.md:95` **已经写明**「星点通量来自 PSF 拟合域（PSF.md）」。⇒ **A6 不是改规范，是修实现符合性**（详见 §4.0）。 |
| **改动面** | 约 **5 处、~120 行**（§5）。库内**已有** PSF 总通量解析式（`dpsf_psf.cpp:428`）与解析增长曲线（`snr_science.cpp:118`），`p1_psf.json` 只是**没把 flux 列写出来**。 |
| **诚实边界** | 本仓**无哈勃数据**，改用 L4 真实标定帧作底（§3.0）。`k_photo` 的**绝对值无物理意义**（吸收增益/口径/曝光），本文件**不对其绝对值作任何声明**；全部判据均为**尺度无关的星等比**。 |

---

## 1 调研结论（文献与开源科学软件）

> 每条的四要素（通量定义 / 孔径无关性与视宁度鲁棒性 / 借鉴点 / 不借鉴点 / 场景差异 / 核对状态）
> 见 `references/f-instr-survey.md`。本节只给**分类学结论**与**落点**。

### 1.1 通量口径分类学（四类）

| 类 | 定义 | 代表 | 有无"孔径"参数 |
|---|---|---|---|
| **A 等照度** | `F_iso = Σ_{I>I_thr}(I − sky)`，阈值定边界 | SExtractor `FLUX_ISOCOR` 的**未改正**部分 `[F-05]`；本仓 `sdet_detector.cpp:281-294` `[F-33]` | 无显式半径，但**阈值**等价于一个随 S/N 与 seeing 变的半径 |
| **B 固定/自适应孔径** | `F_ap(r) = Σ_{d<r}(I − sky)`；自适应版用 Kron/自动半径 | 经典孔径测光；SExtractor `FLUX_APER`/`FLUX_AUTO` `[F-05]`、Kron 1980 `[F-06]` | **有**（Kron 是"隐式"半径） |
| **C 孔径 + 显式孔径改正** | `F_∞ = F_ap(r)/CoG(r)`，CoG 由 PSF 或增长曲线给 | SDSS `fiberMag` 的 3″ 改正 `[F-13]`、LSST `apCorr` `[F-15]`；本仓 `snr_science.cpp:118` `[F-32]` | 名义上有 r，但**结果与 r 无关**（只要 CoG 对） |
| **D PSF 总通量** | 拟合/加权积分 PSF 模型的总归一 | DAOPHOT/ALLSTAR `[F-08][F-09]`、PSFEx `[F-10]`、The Tractor `[F-11]`、SDSS `psfMag` `[F-13]`、LSST `PsfFlux` `[F-15]`、PS1 `[F-16]`、photutils `PSFPhotometry` `[F-35]`；本仓 `docs/science/PSF.md:21` `[F-31]` | **无** |

**关键点 1**：只有 **C 与 D 是孔径无关的**。A 与 B 都需要外部改正才能谈"总通量"。
SExtractor 官方文档已把 `MAG_ISOCOR` 标为 **deprecated**（原文："Corrected isophotal magnitudes are now deprecated; they remain in SExtractor v2.x for compatibility with SExtractor v1."），
并把 `FLUX_AUTO` 明确限定为"约 90% 光在 Kron 椭圆内"（原文："**>= 90% of the flux is expected to lie inside a circular aperture of radius k r_Kron with k = 2**"）—— 即**明确不是总通量** `[F-05]`。

**关键点 2（重要区分："测到的通量" vs "存下来的通量"）**：
- **测量本身**孔径无关的：LSST `base_PsfFlux`（"a linear least-squares fit with the Psf model"，且 "For point sources, this provides the optimal instFlux measurement in the limit where the Psf model is correct."）、
  photutils `PSFPhotometry`（拟合参数即 "total integrated flux"）、DAOPHOT/ALLSTAR 的 PSF 星等 `[F-08][F-09][F-15][F-35]`。
- **但主流巡天存下来/用于定标的通量几乎都不是纯 PSF 总通量，而是"孔径统一"（aperture-uniform）通量**：
  SDSS `psfMag` **要**做孔径改正（局部改正 + **到 7.4″ 半径、随 seeing 变**的改正）；
  LSST `PsfFlux` 登记为 `shouldApCorr=True`，由 `ApCorrMap` 改正到参考通量槽 `slot_CalibFlux`（默认 `base_CircularApertureFlux_12_0`）；
  HSC **用 4″ 直径圆孔径通量作为测光标定参考**；
  PS1/`psphot` 有强制的"孔径改正"阶段（curve-of-growth）`[F-13][F-15][F-16][F-17]`。
- ⇒ **"孔径无关的测量" + "显式孔径改正到统一参考孔径"是现代管线的标准组合**。
  AstroCS 的冻结合同（`PSF.md:87` "整平面延伸假设"、`flux=2πA·sxsy/3`）等价于**改正到无穷孔径**，
  是上述组合在"参考孔径 → ∞"时的极限；**自洽**，但必须在文档里写明这一点（见 §5.1 步 5）。

### 1.2 哪些天生孔径无关

- **D（PSF 总通量）天生孔径无关**：被测量的量是模型归一 `A`（等价于 `∫PSF` 的标度），**定义里没有半径**。DAOPHOT/ALLSTAR 的 `psfMag` 就是这个量 `[F-08][F-09]`；LSST 把它叫 `PsfFlux` `[F-15]`。
- **C（孔径改正）是"事后"孔径无关**：`F_∞ = F_ap(r)/CoG(r)` 与 r 无关**当且仅当** `CoG` 与真实 PSF 匹配。
  **订正（本轮核对）**：SDSS 的 `psfMag` **确实施加了孔径改正**——先对 KL PSF 做局部改正，再用亮星定出**到 7.4″ 半径、随 seeing 变**的改正并逐帧施加
  （原文："the difference between the two is then a local aperture correction, which gives a corrected PSF magnitude"；
  "Finally, we use bright stars to determine a further aperture correction to a radius of 7.4'' as a function of seeing, and apply this to each frame based on its seeing." `[F-13]`）。
  LSST 的 `PsfFlux` 同样**显式施加 `apCorr`**（`shouldApCorr=True`，改正到 `slot_CalibFlux`）`[F-15]`；HSC 的参考孔径是 **4″ 直径** `[F-17]`。
  ⇒ **"孔径无关地测" + "改正到统一参考孔径后存"是主流约定**；两种口径都存在，**关键是不要在管线里混用**，且必须写明参考孔径。
- **A（等照度）不是孔径无关**：阈值一变，围出的面积就变。SExtractor 因此另给 `FLUX_ISOCOR`（假设高斯轮廓把等照度通量改正到总通量）`[F-05]` —— 这个"改正"本身承认了 A 不是总通量。
- **B 的 Kron/自动孔径是"部分自适应"**：半径跟着轮廓走，比固定孔径稳，但仍随 S/N、阈值、seeing 漂移（本工作项实测 M_seeing = 0.091 mag，见 §3.6）。

### 1.3 视宁度鲁棒性

- seeing 变化对 A/B 的影响是**乘性**的：`F_ap(r) = F_∞ · CoG(r; seeing)`。seeing 变大 ⇒ CoG(r) 变小 ⇒ 回收通量变小。**这与真实乘性增益完全退化（degenerate）** —— 这正是 A6 的病灶。
- 对 D，seeing 只改变 PSF 的**形状参数**，不改变其**归一**；只要拟合能跟上形状，回收的 `A` 不变。实测见 §3.6。
- **D 的代价**：形状**族**必须选对。本工作项量化：注入 Moffat(β=3.5) 时，自由 FWHM 正确族 **+0.001 mag**，β=4 族 **+0.025 mag**，高斯 **+0.141 mag**（§3.7）。⇒ **族错是唯一实质风险，且可用帧内 CoG 交叉核对发现**。

### 1.4 主流巡天怎么做（一句话各自）

| 巡天/管线 | 星点标定用的通量 | 孔径改正 | 落点 |
|---|---|---|---|
| SDSS（`frames`/`photoop`） | `psfMag` ← PSF 模型拟合 | **是**：局部改正 + **到 7.4″ 半径、随 seeing 变**的改正 | `[F-13]` |
| DES DM | `MAG_PSF` = "PSF fit single epoch detections" 的星等 | **待核对**（DES DM 官方文档正文本轮取不到） | `[F-14]` |
| LSST DM | `PsfFlux`（"linear least-squares fit with the Psf model"） | **是**：`shouldApCorr=True` → `ApCorrMap` 改正到 `slot_CalibFlux`（默认 `base_CircularApertureFlux_12_0`） | `[F-15]` |
| PS1 IPP / `psphot` | "Linear PSF Fits"（PSF 模型拟合） | **是**：强制阶段 "Aperture Corrections：Measure the curve-of-growth…" | `[F-16]` |
| HSC | 各算法通量（`PsfFlux`/`CModelFlux`） | **是**：改正到**默认 4″ 直径圆孔径**（"the flux we use for photometric calibration, by default a 4'' diameter circular aperture"） | `[F-17]` |
| SExtractor（通用工具） | `FLUX_ISOCOR`（官方标注 **deprecated**）/`FLUX_AUTO`（"≥90% of the flux…"）/`FLUX_APER` | 部分（`ISOCOR` 假设高斯且已废弃） | `[F-05]` |
| photutils（通用工具） | `PSFPhotometry` 的拟合参数 = "total integrated flux" | 不需要（测量即总通量）；`photutils` **无**专用孔径改正 helper | `[F-35]` |
| Gaia `G` | 图像域（AF）标定，非简单孔径 | — | `[F-18]` |
| **AstroCS 冻结合同** | **PSF 拟合域** | **无（PSF 通量即总通量）** | `[F-30][F-31]` |

**结论**：**没有任何主流巡天用"固定小盒和"做测光标定通量**。最接近的 SExtractor `FLUX_ISOCOR` 也带显式轮廓改正。

### 1.5 与 Gaia XP 合成通量做比值时的特殊要求

1. **被积函数必须含 λ 因子（光子计数约定）**：`F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`。
   本仓 `spectrum_integrator.cpp:266-274` **已正确包含 λ**（注释原文「被积函数: S(λ)·T(λ)·Q(λ)·λ」）—— **符合**，无需改动。
   文献依据：能量型 vs 光子型通带的区别与 pivot wavelength 约定见 `[F-23]`；HST/ACS 的同类约定见 `[F-24]`。
2. **单位与常数**：`F_syn` 与 `F_instr` **量纲不同**（W·m⁻²·nm vs ADU），其比值的对数就是 `location`；
   `1/(hc)` 等绝对归一常数**由 `location` 吸收**（`docs/science/PHOTOMETRY.md:69`）⇒ **`k_photo` 绝对值无物理意义**。
3. **颜色项**：`T(λ)` 必须用**本帧滤光片 × CCD QE** 的乘积；缺 QE 时 `compute_f_syn` 会退化为只用 `T(λ)`（`spectrum_integrator.cpp:230` 有 LOG_INFO 提示）⇒ **颜色项在蓝端/红端会偏**，必须确保 `qe_json` 有值，否则 `k_photo` 会吸收一个**星色相关**的系统差。
4. **模型通带不含大气消光与光学系统透过率**（`PHOTOMETRY.md:7/69` 明示为"未建模项"）⇒ 跨帧（airmass 不同）会成为**帧间系统差**，这正好是 §2.5 判据 (b) 要卡的。
5. **不宣称绝对通量刻度**（`PHOTOMETRY.md:69` 原话）。

---

## 2 定案：AstroCS 用哪种

### 2.0 一句话

> **`F_instr` = 星点 PSF 的**总通量**（模型归一），不用任何固定盒和、不用等照度、不用未改正的小孔径。**

### 2.1 推荐口径（含公式）

**主口径 D1 —— PSF 拟合总通量（与 `docs/science/PSF.md` 一致，β=4 族）：**

$$
I(x,y) = B + \frac{A}{\left(1+Q(x,y)\right)^{4}},\qquad
Q = \frac{1}{2}\Big[\,\tfrac{(\Delta x')^2}{s_x^2}+\tfrac{(\Delta y')^2}{s_y^2}\Big]
$$
$$
\boxed{\;F_{\rm instr} \;=\; \frac{2\pi}{3}\,A\,s_x\,s_y\;}
\qquad\text{(各向同性极限 } \tfrac{2\pi}{3}A\sigma^2\text{)}
$$

同时拟合 \((A, B, c_x, c_y, s_x, s_y, \theta)\)（\(B\) = 局部背景，模型内联合拟合，**无独立 annulus**）。
来源：`docs/science/PSF.md:21,52,87`（`flux = 2πA·sxsy/3`，单位 ADU）+ `dpsf_psf.cpp:428`（库内已实现）。
文献谱系：DAOPHOT/ALLSTAR 的 `psfMag` `[F-08][F-09]`；SDSS `psfMag` `[F-13]`；LSST `PsfFlux` `[F-15]`。

**精化口径 D2 —— PSF 加权最优提取（低 S/N 用，S/N 最优）：**

$$
\boxed{\;F_{\rm instr} \;=\; \frac{\sum_i p_i\,(d_i-b)/\sigma_i^2}{\sum_i p_i^2/\sigma_i^2}\;}
,\qquad
\sigma_i^2 = \sigma_{\rm sky}^2 + \frac{\max(F p_i,0)}{g} + \Big(\frac{\rm RN}{g}\Big)^2
$$

\(p\) = 归一化 PSF（\(\sum p_i = 1\)），\(b\) = 局部背景，\(\sigma_i^2\) = 逐像素方差（**含源自身散粒**）。
来源：Horne 1986 `[F-03]`（光谱最优提取）、Naylor 1998 `[F-02]`（成像最优提取）；
**本仓已实现同式**：`snr_science.cpp:110-118`（注释原文「Horne 1986 最优提取 `Var(F) = 1/Σ(P_i²/σ_i²)`」）。
它是总通量的**最小方差线性无偏估计**，且**定义里没有孔径半径**。

**两者的关系**：D1 是"参数化拟合"，D2 是"已知形状的加权积分"。D1 顺带解出星心（抗质心误差），D2 在低 S/N 更优。
**建议**：D1 为主（改动最小、库内已有），D2 作为低 S/N 精化与**交叉核对**（两者差 > 0.03 mag 即标记该星可疑）。

### 2.2 理由（为什么不是别的）

| 候选 | 不选的理由（本工作项实测） |
|---|---|
| 5×5 固定盒（现生产） | M_seeing = **1.353 mag**；假增益 2→4 px = **0.516**。把 seeing 读成增益。 |
| 等照度（`sdet` 口径） | M_seeing 只有 0.018 mag（**看似**好），但偏差**随 S/N 剧烈漂移**：峰值 S/N=10 时 **+0.96 mag**、S/N=400 时 **+0.05 mag**（§3.4）。阈值 = `bkg + 5σ` 使围出面积随噪声变 ⇒ 对**星等受限样本**是灾难性的。 |
| 固定孔径 r=3/4（`photometer.cpp` 默认 4.0） | M_seeing = **0.751 / 0.443 mag**；且孔径依赖峰峰 0.2–1.3 mag 随 seeing 变（§3.5）。 |
| 大孔径 r=10 | M_seeing = 0.092 mag（较好），但低 S/N 散度最差（S/N=10 时 **0.364 mag** vs PSF 法 0.075 mag），且背景估计误差随面积线性放大。 |
| Kron/自动孔径 | M_seeing = 0.091 mag，介于两者之间；仍随阈值/S/N 漂移，且低 S/N 散度 0.142 mag。 |
| **PSF 总通量 D1/D2** | M_seeing = **0.0037 / 0.0039 mag**；高 S/N 偏差 **≤0.003 mag**；低 S/N 散度**最小**（S/N=10 时 0.076/0.075 mag）。 |

### 2.3 孔径无关性论证（**A4 空间增益的验收前提**）

**（i）解析论证。**
D1 的 \(F = \tfrac{2\pi}{3}A s_x s_y\) 是模型**在整平面的解析积分**，表达式中**不含任何半径**；
D2 的 \(F = \frac{\sum p_i(d_i-b)/\sigma_i^2}{\sum p_i^2/\sigma_i^2}\) 对 \(p\) 的**归一**敏感、对**求和上限**不敏感（\(p_i \to 0\) 处权重自然归零，且 \(\sum p_i^2/\sigma_i^2\) 同步收敛）。
⇒ **二者都无"孔径"自由度**，故"A4 空间增益"不会被孔径系统差污染。

**（ii）数值论证（本工作项实测，见 §3.6）。**
固定真值通量、只改 seeing（1.5→5.0 px，3.3×），回收通量的稳健位置：
\(M_{\rm seeing}(D1) = 0.0037\) mag，\(M_{\rm seeing}(D2) = 0.0039\) mag（判据 ≤0.010 mag）；
生产 5×5 口径 \(M_{\rm seeing} = 1.353\) mag（判据 ≥0.200 mag 必须红）。
**能红能绿同时成立**（`exp3` 的 `red_green_check = {green_ok: true, red_ok: true}`）。

**（iii）与 A4 的接口。**
\(I_{\rm photo} = k_{\rm photo}\cdot m(x,y)\cdot I_{\rm cal}\) 中 \(m\) 的估计量是**同一颗星在两帧间的通量比**。
若 \(F_{\rm instr}\) 口径本身随 seeing 变，则 \(m\) 的估计量里混入 \(\frac{{\rm CoG}(r;s_2)}{{\rm CoG}(r;s_1)}\)。
实测该污染项：5×5 口径 seeing 2→4 px 为 **0.516×（−0.72 等）**，r=3 为 0.672×，r=4 为 0.808× —— **这就是 Q1「孔径依赖峰峰 7.78/4.93/3.81/4.25%」的来源**。
改用 D1/D2 后该污染项为 **0.998×（−0.002 等）**，A4 的 \(m\) 才可解释为真实的乘性增益。

### 2.4 退化处理

| 情形 | 处理 | 依据 |
|---|---|---|
| **星太暗**（峰值 S/N < 5） | D1 拟合不收敛 ⇒ 落到 **D2**；D2 的 \(\sigma_F\) 解析可得，\({\rm SNR}<3\) 直接**剔除**（不进入 \(r_i`），记 `rejected_snr`。**禁止**用盒和凑数。 | `snr_estimator.h:283`（`flux5 = 5σ_F` 已是库内口径） |
| **星太亮 / 饱和** | 饱和位（`quality&1` / `psf_status!=0` / `PC_QF_SATURATED`）**已**在 `star_matcher` 有效域剔除 —— 保持不动。**新增**：D1 拟合残差 `mad`（`dynamic_psf.h` 已输出）超门限的星同样剔除。 | `PHOTOMETRY.md:37,85`；`star_matcher.h:11` |
| **边缘**（PSF 足迹出界） | `quality&2`（`star_detector.cpp:143` 已置位）**已**存在；**新增**要求 D1 的拟合窗口完全落帧内（\(c_x, c_y \in [3{\rm FWHM}, N-3{\rm FWHM}]\)），否则剔除。 | `star_detector.cpp:143` |
| **混合 / 混叠**（blend） | D1 的 \(\chi^2\)/`mad` 会变大 ⇒ 用 `mad` 做**离群剔除**；且 Tukey-IRLS 本身对 \(r_i` 的离群稳健（`c=4.685`）。**不建议**为此引入多星同时拟合（改动面爆炸）。 | `star_matcher.cpp:538-589` |
| **形状族错**（β 不匹配） | D1 的 \(\beta=4\) 与真 PSF 的 β≈2.5–3.5 不符时偏差 **+0.025 mag**（常数项，被 \(k_{\rm photo}\) 吸收）；**新增**门：帧内 D1 与 D2 的中值差 > 0.03 mag ⇒ 帧级告警。 | §3.7 exp5 |
| **无 PSF 星 / 无光谱星 / 匹配数 < 3** | 已有 fail-closed 分支保持不动（`frame_photometry_fit.cpp:198-215`）。 | 现实现 |

### 2.5 验收判据（**尺度无关，只用星等** —— 按负责人 2026 纠正）

> 不设任何涉及 ADU/增益/口径/曝光的绝对判据。\(k_{\rm photo}\) 的绝对值**不作声明**（吸收未建模仪器常数）。

- **(a) 测光一致性**：施加后星点**星等**与 Gaia 的残差散度 \(\sigma_{\rm residual}\)（dex→mag）必须小。
  验收线：**中值 |Δmag| ≤ 0.01 mag**，**MAD×1.4826 ≤ 0.03 mag**（对应 `P1_PHOT_MAX_SIGMA_DEX=1.0` 的收紧版，建议按此新增更严的门）。
  本工作项证据：D1/D2 在峰值 S/N≥50 时 **|bias| ≤ 0.003 mag**、散度 ≤0.021 mag（§3.4）。
- **(b) 帧间一致性**：各帧 \(k\) 落在同一测光体系。
  验收线：**同组帧间 \(k\) 的散度峰峰 ≤ 0.05 mag**（现 `P1_PHOT_MAX_SPREAD_DEX=0.5` 即 1.25 mag 太松）。
  本工作项证据：D1/D2 在 seeing 1.5–5.0 px 内的假帧间差 **≤0.005 mag**（§3.6）；生产口径为 **1.35 mag**。
- **(c) 能红能绿**：负例（真值无效应）下度量必须归零；正例（真值有效应）下度量必须等于真值。
  本工作项：exp3（seeing 负例）与 exp4-A（增益正例）双双通过（§3.6/§3.7）。

---

## 3 数值实验

### 3.0 底数据与诚实声明

- **本仓无哈勃/HST 数据**。已做全仓检索（`find . -iname "*hst*" -o -iname "*hubble*" -o -iname "*wfc3*"` 无命中；全仓 FITS 均为本仪器 M42/Victory Nebula 帧）。
  ⇒ 按负责人令的替代条款，**以 L4 真实标定帧作底**，并在此**显式声明**：
  底数据 = `run/RELEASE-02/L4-rebuild/norm/*/calibrated_*.fts`（**49 帧**，4096²，float32，300 s，Red）。
- **真实帧头无 `GAIN`/`RDNOISE` 键**（实测 49 帧全无；与 `references/bibliography.md §2.1` 的登记一致）。
  ⇒ 增益/读出噪声**不能**从帧头取得 —— 这**正是**负责人「FITS 头拿不到 adu/口径，所以要用 Gaia+QE+滤光片把图像标定到真实测光坐标系」的设计前提。
  ⇒ 本工作项**不做任何物理闭合反推**；渲染器的 `g`/`RN` 仅为**合成数据的噪声模型参数**，并做了敏感性扫描。
- 另：我**尝试过**从真实帧做 photon-transfer 估增益，结果为 \(V \approx 1.9 S\) 且截距为负（被星云大尺度结构污染），**判定不可用，不进任何结论**。这是"帧头路线是死路"的独立旁证。
- 选用底帧（按帧头 `FWHM` 键取最好/中/最差）：

| tag | 帧 | 帧头 FWHM | 实测 sky 中值 | 实测 σ（ADU） | 平滑天光图 p90−p10 |
|---|---|---|---|---|---|
| good | `t3_m3_red/...20251211@025101-300S-Red.fts` | 2.16 | 169.04 | 14.86 | 3.4 |
| mid | `t3_m6_red/...20251228@051525-300S-Red.fts` | 2.81 | 182.90 | 15.34 | 6.6 |
| poor | `t3_m5_red/...20251228@043447-300S-Red.fts` | 4.71 | 199.33 | 15.50 | 4.9 |

干净窗口（512×512，全帧平滑天光图上最暗最平）：`(y0,x0) = (256, 0)`。
**主实验全部用 `good` 窗口**（`sky_good`）作底。

### 3.1 噪声模型（**论文方法节照抄**）

逐像素 \(i\)，全部噪声过程**按物理顺序**作用（`f_instr_lib.py` 模块 docstring 同文）：

$$
\begin{aligned}
&\text{(1) 场景均值(电子):}\quad \lambda_i = R_i\,g\,(S_i + f_i) + d \\
&\text{(2) 散粒:}\quad n_i \sim {\rm Poisson}(\lambda_i) \\
&\text{(3) 读出:}\quad m_i \sim \mathcal{N}(0, {\rm RN}^2)\ \ [e^-] \\
&\text{(4) 电子}\to{\rm ADU}:}\quad a_i = (n_i + m_i)/g \\
&\text{(5) 饱和:}\quad a_i \leftarrow \min(a_i, {\rm SAT});\quad {\rm sat}_i = [a_i^{\rm pre} \ge {\rm SAT}] \\
&\text{(6) 量化:}\quad a_i \leftarrow \lfloor a_i + 0.5 \rfloor
\end{aligned}
$$

- \(S_i\)：**天光+背景均值**，取自真实帧（源掩膜 + 32×32 分块 sigma-clip 中值 + 双线性上采样）⇒ **保留真实天光梯度与大尺度结构**；
- \(f_i = F_{\rm true}\cdot p_i\)：注入源（\(p\) 归一化 PSF）；
- \(R_i\)：**残余相对响应**（平场误差 / 空间增益），\(\mathbb{E}[R]=1\)；作用在**入射光子率**上（乘性）；
- \(d = 0.02\,{\rm e^-/px/s}\times 300\,{\rm s} = 6\,{\rm e^-}\)；\(g = 1.0\,{\rm e^-/ADU}\)；\({\rm RN}=4\,{\rm e^-}\)；\({\rm SAT}=55000\,{\rm ADU}\)（全部**显式声明**）。

> **这不是算术改像素值**：天光与源**各自**过 Poisson，读出过 Gaussian，再经增益、饱和、量化。
> **\(g\)/\({\rm RN}\) 不是对真实相机的声明**，只是合成数据的噪声参数；敏感性扫描见 §3.4。

**噪声模型自校验（exp0）**：渲染纯天光帧，与真实帧比 σ：

| tag | 真实 σ | 模拟 σ | 比值 |
|---|---|---|---|
| good | 14.859 | 13.343 | 0.898 |
| mid | 15.341 | 14.826 | 0.966 |
| poor | 15.499 | 14.826 | 0.957 |

⇒ 模拟帧噪声与真实帧在 **4–10%** 内一致。**这是"物理真实"的定量证据**，不是"看起来像"。

### 3.2 判据先行（写死在代码里，不得事后放宽）

```python
PASS_THRESHOLD = 0.010     # 绿: 孔径无关口径  M_seeing <= 0.010 mag
RED_THRESHOLD  = 0.200     # 红: 生产口径      M_seeing >= 0.200 mag
accept_mag     = 0.02      # exp5: |PSF 总通量偏差| <= 0.02 mag
```

度量（全部**尺度无关**）：
\(M_{\rm seeing}(X) = \max_s {\rm median}(\Delta{\rm mag}_X(s)) - \min_s {\rm median}(\Delta{\rm mag}_X(s))\)
，其中 \(\Delta{\rm mag} = -2.5\log_{10}(F_{\rm rec}/F_{\rm true})\)；
假增益 \(k_{\rm spurious}(s_1\!\to\!s_2) = 10^{-0.4(\Delta{\rm mag}(s_2)-\Delta{\rm mag}(s_1))}\)。

### 3.3 参与的 9 种口径

| 名 | 定义 | 代码 |
|---|---|---|
| `box5` | **生产复刻**：5×5 盒内 \(\max(a-b,0)\) 求和 | `star_detector.cpp:139-151` |
| `iso5s` | **sdet 复刻**：\(a > b+5\sigma\) 连通域内 \((a-b)>0\) 求和 | `sdet_detector.cpp:281-294` |
| `aper_r` | 亚像素精确圆孔径 + 8–12 px 环带本底，**无**孔径改正 | `photometer.cpp`（默认 r=4.0） |
| `aper_cog4` | \(F_{\rm ap}(4)/{\rm CoG}(4)\)，CoG 用 Moffat 解析增长曲线 | `snr_science.cpp:118` |
| `kron` | Kron 自适应孔径（\(2.5 r_{\rm Kron}\)） | `[F-05][F-06]` |
| `psf_nlsq` | **D1**：Moffat 拟合总通量（\(A,b,c_x,c_y\) 非线性最小二乘） | `[F-08][F-09]` |
| `psf_opt` | **D2**：PSF 加权最优提取 | `[F-03][F-02]` |

### 3.4 exp1 —— 回收精度 vs 峰值 S/N

**配置**：注入 Moffat(FWHM=2.5, β=3.5)，40 星/帧 × 5 实现，真实 `sky_good` 作底，\(g=1,{\rm RN}=4\)。
下表 = **稳健偏差 / 稳健散度（mag）**。

| 峰值 S/N | box5 | iso5s | kron | psf_nlsq | psf_opt | aper_cog4 | aper_3 | aper_4 | aper_6 | aper_10 |
|---|---|---|---|---|---|---|---|---|---|---|
| 3 | +0.216/0.230 | +1.790/0.035 | +0.007/0.527 | −0.041/0.223 | +0.040/0.236 | −0.021/0.366 | +0.152/0.249 | +0.037/0.366 | −0.011/0.547 | −0.368/0.664 |
| 10 | **+0.338**/0.112 | **+0.960**/0.216 | +0.005/0.142 | +0.006/0.076 | +0.019/**0.075** | −0.020/0.115 | +0.151/0.097 | +0.039/0.115 | −0.028/0.184 | −0.063/0.364 |
| 50 | +0.276/0.031 | +0.220/0.032 | +0.042/0.025 | −0.001/0.021 | +0.003/0.019 | +0.003/0.025 | +0.156/0.022 | +0.062/0.025 | +0.013/0.041 | +0.012/0.075 |
| 100 | +0.257/0.018 | +0.130/0.017 | +0.046/0.015 | +0.001/0.015 | +0.003/0.015 | +0.004/0.014 | +0.160/0.013 | +0.063/0.014 | +0.013/0.020 | +0.000/0.037 |
| 400 | +0.242/0.010 | +0.047/0.006 | +0.048/0.005 | +0.001/0.006 | +0.001/0.006 | +0.002/0.005 | +0.159/0.005 | +0.061/0.005 | +0.014/0.007 | +0.004/0.010 |

**读法**：
1. `box5` 有 **+0.24～0.34 mag 的常数性亏损**（回收 77–84%），**不随 S/N 消失** ⇒ 它是**乘性**的，会被 `k_photo` 整个吸收；但**随 seeing 剧烈变化**（§3.6）⇒ 这才是致命处。
2. `iso5s` 的偏差**随 S/N 漂移 0.05→1.79 mag**（噪声整流：阈值随噪声走）⇒ 对星等受限样本不可用。
3. `psf_nlsq`/`psf_opt` 在 S/N≥50 时 **|偏差| ≤ 0.003 mag**，且**低 S/N 散度最小**（0.075 vs `aper_10` 的 0.364）⇒ 与 Naylor/Horne 的"最优提取"理论一致 `[F-03][F-02]`。
4. `aper_r` 的偏差**恰好等于未改正的圈入能量** \(-2.5\log_{10}{\rm CoG}(r)\)（r=3→+0.159 vs 理论 +0.151；r=4→+0.061 vs +0.060）⇒ 估计器实现正确，偏差就是**孔径亏损**。
5. **噪声参数敏感性**（\((g,{\rm RN}) \in \{(0.5,4),(1,0),(2,4),(1,10)\}\)）：所有偏差**逐位不变**（差异 <0.02 mag）⇒ **结论不依赖声明的噪声参数**。
6. **天光梯度 15% 峰峰**：偏差不变（局部背景吸收）⇒ 对结论无影响。
7. **PSF 固定形状误差**：拟合用 FWHM=2.0（真 2.5）⇒ **+0.30 mag**；用 3.0 ⇒ **−0.21 mag**。这是**最坏情形**（形状被钉死）。真实情形形状是自由拟合的，见 exp5（§3.7）：**+0.001 mag**。

### 3.5 exp2 —— 孔径依赖曲线（任务书 ③-14）

**dmag(r)（峰值 S/N=100，正 = 回收偏低）**：

| seeing | r=2 | r=2.5 | r=3 | r=4 | r=5 | r=6 | r=8 | r=10 | r=12 | r=16 | box5 | iso5s | kron | psf_nlsq | psf_opt |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1.5 | 0.125 | 0.057 | 0.027 | 0.006 | −0.003 | −0.009 | −0.021 | −0.035 | −0.043 | −0.082 | 0.066 | 0.127 | 0.015 | 0.003 | 0.008 |
| 2.0 | 0.275 | 0.146 | 0.079 | 0.028 | 0.008 | 0.009 | −0.001 | −0.002 | −0.001 | 0.011 | 0.148 | 0.135 | 0.033 | 0.001 | 0.003 |
| 2.5 | 0.455 | 0.265 | 0.157 | 0.059 | 0.024 | 0.011 | 0.004 | 0.002 | 0.001 | −0.000 | 0.254 | 0.127 | 0.044 | −0.001 | 0.001 |
| 3.0 | 0.651 | 0.411 | 0.263 | 0.113 | 0.054 | 0.028 | 0.010 | 0.006 | 0.005 | 0.003 | 0.413 | 0.132 | 0.056 | 0.002 | 0.004 |
| 4.0 | 1.040 | 0.727 | 0.509 | 0.262 | 0.142 | 0.083 | 0.037 | 0.028 | 0.033 | 0.056 | 0.865 | 0.134 | 0.074 | 0.002 | 0.005 |
| 5.0 | 1.404 | 1.042 | 0.780 | 0.452 | 0.273 | 0.176 | 0.096 | 0.082 | 0.092 | 0.143 | 1.424 | 0.139 | 0.102 | 0.002 | 0.004 |

**孔径依赖度量**：

| seeing | 孔径族峰峰 (mag) | \(d(\Delta{\rm mag})/d\log r\) | box5 | iso5s | psf_nlsq | psf_opt |
|---|---|---|---|---|---|---|
| 1.5 | 0.207 | −0.170 | +0.066 | +0.127 | +0.003 | +0.008 |
| 2.5 | 0.462 | −0.377 | +0.254 | +0.127 | −0.001 | +0.001 |
| 4.0 | 1.012 | −0.939 | +0.865 | +0.134 | +0.002 | +0.005 |
| 5.0 | 1.321 | −1.276 | +1.424 | +0.139 | +0.002 | +0.004 |

**读法**：孔径族在 r=2→16 上的峰峰散度从 0.21 mag（好 seeing）涨到 1.32 mag（差 seeing），
**且整条曲线随 seeing 整体平移** ⇒ 任何"固定孔径 + 固定孔径改正"在 seeing 变化时都会失效。
`psf_nlsq`/`psf_opt` 在**整张表**上的值都在 ±0.005 mag 内 —— **曲线是平的**。

### 3.6 exp3 —— seeing 负例（**能红能绿**）

**真值：无效应**（固定注入通量，只改 seeing）。好的口径回收值必须**不变**。

| 口径 | s=1.5 | s=2.0 | s=2.5 | s=3.0 | s=3.5 | s=4.0 | s=5.0 | **M_seeing** | max\|bias\| | 假增益 2→4 | 判定 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `box5`（生产） | 0.065 | 0.142 | 0.255 | 0.411 | 0.617 | 0.860 | 1.418 | **1.353** | 1.418 | **0.516** | **FAIL（红，符合预期）** |
| `iso5s` | 0.121 | 0.133 | 0.127 | 0.134 | 0.135 | 0.134 | 0.139 | 0.018 | 0.139 | 1.000 | MARGINAL |
| `kron` | 0.011 | 0.031 | 0.044 | 0.055 | 0.066 | 0.075 | 0.102 | 0.091 | 0.102 | 0.960 | MARGINAL |
| **`psf_nlsq`（D1）** | 0.002 | −0.001 | 0.002 | 0.002 | 0.003 | 0.001 | 0.002 | **0.0037** | 0.003 | **0.998** | **PASS（绿）** |
| **`psf_opt`（D2）** | 0.003 | 0.001 | 0.004 | 0.004 | 0.005 | 0.003 | 0.004 | **0.0039** | 0.005 | **0.998** | **PASS（绿）** |
| `aper_3` | 0.029 | 0.078 | 0.160 | 0.263 | 0.385 | 0.511 | 0.780 | 0.751 | 0.780 | 0.672 | FAIL |
| `aper_4` | 0.009 | 0.023 | 0.062 | 0.114 | 0.184 | 0.263 | 0.452 | 0.443 | 0.452 | 0.802 | FAIL |
| `aper_6` | 0.005 | −0.001 | 0.013 | 0.026 | 0.055 | 0.084 | 0.176 | 0.177 | 0.176 | 0.925 | MARGINAL |
| `aper_10` | −0.007 | −0.012 | −0.000 | 0.002 | 0.023 | 0.033 | 0.080 | 0.092 | 0.080 | 0.959 | MARGINAL |

`red_green_check = {green_ok: true, red_ok: true}` ⇒ **判据能红能绿**（不是恒绿的空断言）。

### 3.7 exp4 / exp5 —— 真乘性增益回收 与 PSF 形状自由度

**exp4：两帧之比 \({\rm median}(F_B/F_A)\)**

| 用例 | 真值 | box5 | iso5s | kron | **psf_nlsq** | **psf_opt** | aper_2 | aper_3 | aper_4 | aper_6 | aper_10 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **A 正对照**（均匀增益 1.05，同 seeing） | 1.050 | 1.0532 | 1.0547 | 1.0476 | 1.0502 | 1.0494 | 1.0528 | 1.0515 | 1.0516 | 1.0490 | 1.0495 |
| **A 正对照**（0.95） | 0.950 | 0.9479 | 0.9455 | 0.9499 | 0.9486 | 0.9488 | 0.9483 | 0.9482 | 0.9473 | 0.9510 | 0.9505 |
| **B 负例**（无增益，seeing 2.0→4.0） | **1.000** | **0.5044** | 0.7824 | 0.9522 | **0.9977** | **0.9947** | 0.4929 | 0.6724 | 0.8078 | 0.9251 | 0.9846 |
| **C 联合**（真增益 1.05 + seeing 2.0→4.0） | **1.050** | **0.5304** | 0.8248 | 1.0017 | **1.0462** | **1.0396** | 0.5167 | 0.7039 | 0.8412 | 0.9770 | 1.0133 |
| **D 小尺度结构响应**（3% rms，\(\ell\)=1.5 px） | 1.000 | 0.9950 | 0.9971 | 0.9982 | 0.9978 | 0.9976 | 0.9951 | 0.9977 | 0.9969 | 0.9968 | 0.9912 |

**读法**：
1. **A（能红）**：真值 1.05/0.95 时**所有**口径都回收正确（均匀响应把天光一起缩放，任何口径都对）⇒ 度量不是恒零。
2. **B（能绿，且这就是 Q1 的病灶）**：真值 **1.000**（无增益）时，
   `box5` 给出 **0.504**（−0.74 等的假增益），且**随孔径剧烈变化**：r=2→0.493、r=3→0.672、r=4→0.808、r=6→0.925、r=10→0.985。
   ⇒ **Q1「同一帧对的差异随孔径剧烈变化（7.78/4.93/3.81/4.25%）」被完整复现，并被证明其根源是视宁度而非真实增益。**
   `psf_nlsq`/`psf_opt` 给出 **0.998 / 0.995** ⇒ **度量归零**。
3. **C（联合）**：真增益 1.05 且 seeing 不同时，`box5` 报 0.530（**连符号方向都错**：真有增益却报亏损），D1/D2 报 1.046/1.040（真值 1.05）。
4. **D（零均值小尺度结构）**：所有口径都回收 ≈1.00（0.989–0.998）⇒ **零均值 PRNU 型残差不产生增益偏差**，
   故 A4 要处理的 \(m(x,y)\) 必须是**低阶大尺度场**（与设计里低阶 \(m\) 的假设一致）。大孔径散度更大（0.10 vs 0.03）⇒ 结构响应下**小孔径反而更稳**。

**exp5：PSF 形状自由度 → 总通量偏差（注入 Moffat FWHM=2.5, β=3.5，判据 \|bias\|≤0.02 mag）**

| 配置 | 偏差 (mag) | 散度 | 拟合出的 FWHM | 判定 |
|---|---|---|---|---|
| C1 固定正确形状 | −0.0000 | 0.0137 | 2.500（固定） | PASS |
| **C2 自由 FWHM，正确族（β=3.5）** | **+0.0011** | 0.0168 | 2.502 | **PASS** |
| C3 自由 FWHM，**β=4**（库内 `dpsf` 的族） | **+0.0251** | 0.0160 | 2.532 | FAIL（略超） |
| C4 高斯（自由 FWHM） | **+0.1408** | 0.0166 | 2.743 | FAIL |

**读法**：**形状自由度本身不是风险（+0.001 mag），族选错才是**。库内 β=4 的代价是 **+0.025 mag 的常数项**（被 \(k_{\rm photo}\) 吸收），远小于生产口径的 0.24–1.42 mag。高斯则不可接受。

### 3.8 实验结论汇总

| 问题 | 答案（有数值证据） |
|---|---|
| 生产口径的 seeing 敏感度 | 1.353 mag（seeing 1.5→5.0）；假增益 2→4 px = 0.516× |
| 推荐口径的 seeing 敏感度 | **0.0037 mag（D1）/ 0.0039 mag（D2）** |
| 是否与 Q1 的孔径依赖现象同源 | **是**。exp4-B 用"真值无增益 + 只改 seeing"完整复现了 0.49/0.67/0.81/0.93/0.98 的孔径依赖序列 |
| 正例能否变红 | 能：exp4-A 真值 1.05/0.95 被全部口径回收 |
| 负例能否归零 | 能：exp3 M_seeing(D1/D2) ≤0.004 mag；exp4-B D1/D2 = 0.998/0.995 |
| 噪声参数是否影响结论 | 否（4 组 (g,RN) 偏差不变） |
| 天光梯度是否影响结论 | 否（15% 峰峰无影响） |
| PSF 测光的主要风险 | 形状**族**（β=4 偏 +0.025 mag）；自由 FWHM 无风险（+0.001 mag） |

---

## 4 与现实现的差距（`file:line`）

### 4.0 **最重要的一条：这是"实现不符合已冻结合同"，不是"合同需要改"**

`docs/science/PHOTOMETRY.md:95`（SCI-PHOT-001 §9a，FROZEN）原文：

> **PSF 参数**：星点通量来自 PSF 拟合域（PSF.md），饱和判据 `psf_status/SATURATED` 决定剔除（§4）。

`docs/science/PHOTOMETRY.md:96` 原文：

> **aperture/flux/background**：`F_instr` 为仪器通量（ADU·px 或 e⁻ 同尺度）；无孔径背景扣除项——背景已在 PSF/测光上游处理

`docs/science/PSF.md:21` / `:87` 原文：

> `flux` | 解析通量 `2πA·sxsy/3` (β=4) | `dpsf_psf.cpp:368`
> **aperture/flux/background**：解析通量 `flux=2πA·sxsy/3`，单位 **ADU** … 背景 `B` 模型内联合拟合，无独立孔径 annulus。

**而实现喂给拟合的是检测器的 5×5 盒和。** ⇒ 代码未按冻结合同执行。

> 依据 AGENTS.md §8：**证据指向代码错 ⇒ 改实现，补 Oracle/负例锁定**。
> 本工作项**不实施**改动（任务书要求"不实现，只给 file:line 与工作量"）。

> **行号基准声明**：本节 `file:line` 以本工作项**读取时的工作区**为准。
> 注意 `lib/infrastructure/scheduler/src/module_adapters.cpp` 在本次读取时**已带其他并行工作的未提交改动**（`git status` 显示 ` M`），
> 故行号可能随后续提交漂移；**核对时请以代码内容（引用的原文/标识符）为准，不要只认行号**。

### 4.1 差距表

| # | 位置 | 现状 | 应为 | 性质 |
|---|---|---|---|---|
| G1 | `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:139-151` | `s.flux = m00`（5×5 窗口 + `v<=0` 截断）；`:143` 边缘只置 `quality\|=2` | 不再作为 `F_instr` 来源（可保留为**检测**用途） | **实现错（不符合 PHOTOMETRY.md:95）** |
| G2 | `lib/algorithms/star_detection/src/sdet_detector.cpp:281-294` | 等照度通量 `Σ_{val>0}(I−bkg)`（`wcs-platesolve` 用，**非**测光标定路径） | 保持（**不在测光标定路径**，无需改） | 无（但须防止被误用） |
| G3 | `lib/algorithms/photometry/wrapper_phase1/photometer.cpp:12` | `Photometer(aperture_radius_px=4.0, 6.0, 10.0)`；`p1_flux.json` 用它 | 若 `p1_flux.json` 仍要保留，应**同时**写出孔径改正后通量与 `enclosed_fraction` | 补充（避免下游误用未改正孔径通量） |
| G4 | `lib/infrastructure/scheduler/src/module_adapters.cpp:3234-3245` | `pfl.push_back(srcs[s].value("flux", 0.0))` —— 把 G1 的 5×5 `m00` 当作 `psf_flux` 传给 `pc_calibrate_*` | 改为传 **PSF 总通量** | **实现错（核心病灶）** |
| G5 | `lib/infrastructure/scheduler/src/module_adapters.cpp:2142-2150` | `p1_psf.json` 的 `psf_params` 行写 `{star_id,B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y}` —— **丢掉了 `flux`**（`DPSFFitResult` 有该字段：`lib/algorithms/psf/include/dynamic_psf.h:29`，冻结 9 列布局含 `flux`：同文件 `:182`） | 增写 `flux`（及 `mad`/`eccentricity`） | **实现错（信息丢失）** |
| G6 | `lib/algorithms/psf/src/dpsf_psf.cpp:428` | 已算 `flux = 2πA·sx·sy/3` | **无需改**（已有，正确） | 无 |
| G7 | `lib/algorithms/photometry/cpp/src/star_matcher.cpp:318` | `m.f_instr = psf_flux[j]`（口径取决于上游 G4） | 不变（口径由上游修） | 无 |
| G8 | `lib/algorithms/photometry/cpp/src/star_matcher.cpp:538-589` | Tukey-IRLS `location`；`:589` `scale = pow(10,-location)` | **无需改**（稳健估计本身没问题） | 无 |
| G9 | `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:266-274` | 被积函数 `S·T·Q·λ`（含光子计数 λ 因子） | **无需改**（已正确） | 无 |
| G10 | `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:230` | 无 QE 时 `F_syn` 只用 `T(λ)`，仅 `LOG_INFO` 提示 | 应**升级为显式告警/降级标志**（颜色项缺失会引入星色相关系统差） | 补充 |
| G11 | `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:110-118` | 已实现 Horne 1986 最优提取与 Moffat4 解析增长曲线 `f_in(r)=1-(1+r²/(2σ²))^{-3}` | **无需改**（可直接复用于 D2 与交叉核对） | 无 |
| G12 | `lib/infrastructure/scheduler/src/module_adapters.cpp:3276`（`P1_PHOT_MAX_SPREAD_DEX = 0.5`） | 帧间一致性门 = 0.5 dex（1.25 mag）**过松** | 收紧到 ≤0.05 mag（0.02 dex），否则 §3.6 的 1.35 mag 假帧间差**照样过门** | **门限错（放过病灶）** |
| G13 | `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:198-215` | fail-closed 退化分支（rc/非有限/星数<3） | 保持不动 | 无 |

### 4.2 附：一处需另案核对的观察（**不在 A6 范围，仅登记**）

`snr_estimator.h:78-79` 声明的 PSF 块冻结布局是 `[status(0), B(1), flux(2), cx(3), cy(4), fwhm(5), A(6), mad(7), eccentricity(8)]`，
而 `module_adapters.cpp:2091-2150` 读 `dpsf_fit_batch_f64` 的输出时用的是 `[B(0), A(1), cx(2), cy(3), sx(4), sy(5), theta(6), fwhm_x(7), fwhm_y(8)]`（`dynamic_psf.h` 的另一种布局）。
两处**列语义不同**。本工作项未追到 `snr` 模块的 PSF 块实际由谁构造（`module_adapters.cpp` 里未见），
**不判定谁错**，仅登记为待核对项（若确有混用，属独立缺陷）。

---

## 5 改动面估计（**不实现**）

### 5.1 方案 R1（推荐，最小改动）：`F_instr` ← 已有 Moffat4 拟合总通量

| 步 | 文件:行 | 改动 | 估计 |
|---|---|---|---|
| 1 | `lib/infrastructure/scheduler/src/module_adapters.cpp:2142-2150` | `psf_rows` 增写 `flux`（`psf_params` 无 flux 列 ⇒ 需从 `B/A/sx/sy` 现算 `2πA·sx·sy/3`，或改用 `DPSFFitResult::flux`） | **~10 行** |
| 2 | `lib/infrastructure/scheduler/src/module_adapters.cpp:3234-3245` | `pfl` 来源从 `p1_sources[].flux` 改为 `p1_psf.json` 的 `flux`（需把 PSF 行按 `star_id` 映射到检测序，`psf_status` 由 `p1_psf` 行存在性给出） | **~30 行**（映射逻辑是主要工作量） |
| 3 | `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:99` 附近 | 输入校验：`psf_flux` 语义变更后更新注释与断言（不改变量名，ABI 不变） | **~5 行** |
| 4 | `lib/infrastructure/scheduler/src/module_adapters.cpp:3276` | `P1_PHOT_MAX_SPREAD_DEX` 0.5 → 0.02（按 §2.5 判据 (b)） | **1 行 + 文档** |
| 5 | `docs/algorithms/STAR_PSF_ALGORITHMS.md` / `docs/algorithms/PHOTOMETRIC_FIT.md` | 记录 `p1_psf.json` 新增列与 `F_instr` 口径（**走文档变更 claim**） | **~40 行** |
| 6 | 测试 | 新增 Oracle：注入已知 `flux` 的解析 Moffat4 场 ⇒ `p1_psf.json.flux` 复算一致（`rtol 1e-9`）；负例：seeing 扫描下 `F_instr` 不变（本工作项 `exp3` 可直接改造为 C++ 测试的判据模板） | **~150 行** |
| | | **合计** | **~5 个文件、~240 行（含测试）**；生产代码约 **~46 行** |

**风险**：PSF 拟合在**暗星**上不收敛（`p1_psf.json` 只有成功行）⇒ 会**减少**进入 `r_i` 的星数。
缓解：`P1_PHOT_MIN_FIT_STARS=3` 已存在；建议**保留 5×5 `flux` 作为"仅检测"用途**，并统计 `n_psf_valid / n_detected` 作为 QA 指标。

### 5.2 方案 R2（精化，可选）：加 D2 最优提取

| 步 | 文件:行 | 改动 | 估计 |
|---|---|---|---|
| 1 | `lib/algorithms/photometry/cpp/src/` 新增 `psf_flux_optimal.cpp/.h` | `F = Σ p_i(d_i−b)/σ_i² / Σ p_i²/σ_i²`，\(p\) 由 `sx,sy,θ` 生成，\(σ_i²\) 用 `cat.noise_sigma` + 源散粒项 | **~120 行** |
| 2 | `module_adapters.cpp` 接线 + 交叉核对门（\|D1−D2\| > 0.03 mag ⇒ 标记） | | **~40 行** |
| 3 | 测试（解析 PSF 场 + 已知方差） | | **~120 行** |
| | | **合计** | **~3 个文件、~280 行** |

### 5.3 不建议的改动

- **改 `sdet_detector.cpp`**：它不在测光标定路径上（`wcs-platesolve` 用），改了没有收益。
- **引入多星同时拟合 / 全帧 PSF 场**：改动面爆炸（>1000 行），而 exp4-D 表明**低阶** \(m\) 已足够；先做 R1 再评估。
- **改 `c=4.685` / `tol` / `max_iter`**：`PHOTOMETRY.md:100` 列为"不可接受变化"，且 §3.6 表明 IRLS 本身不是病灶。

---

## 6 诚实登记（未做项 / 不确定项）

| 项 | 状态 |
|---|---|
| **哈勃数据** | **本仓无 HST 数据**（全仓检索确认）。已按负责人令替代条款用 L4 真实标定帧作底并显式声明。**未做**：真实 HST 帧上的验证。 |
| **真实增益 / 读出噪声** | 帧头无 `GAIN`/`RDNOISE`；**未做**物理反推（且按负责人 2026 纠正**禁止**做）。渲染器的 \(g\)/\({\rm RN}\) 为声明值 + 敏感性扫描。 |
| **`photutils` / `sep`** | **未安装**（`ModuleNotFoundError`）⇒ PSF 拟合与最优提取**自实现**（`f_instr_lib.py`），未与 `photutils.psf.PSFPhotometry` 做数值交叉核对。**未做**。 |
| **真实帧上的经验 PSF** | 已尝试（`exp0`），得 FWHM=**5.97 px**，与帧头 `FWHM=2.16 px` **严重不符**（M42 拥挤场中值叠加被混合星污染）。**判定不可靠，未用于主实验**（主实验用参数化 Moffat）。这是一个独立警示：**PSF 测定步本身是 PSF 测光的主要工程风险**。 |
| **`psf_nlsq` 的质心自由度** | 已含 \((dx,dy)\) 拟合；但**未做**"质心初值偏 0.5 px"的鲁棒性扫描。 |
| **真实星（非注入）的回收** | **未做**（无外部真值星表；Gaia 交叉本身就是要标定的对象，不能当真值）。 |
| **D2 的 \(\sigma_i^2\) 逐像素项** | 本工作项用常数 \(\sigma_{\rm sky}\)（源散粒项省略，因注入星峰值 S/N 高）。生产实现应含 \(\max(Fp_i,0)/g` 项（`snr_science.cpp:111` 已有正确形式）。 |
| **跨望远镜实测验证** | **未做**（本工作项只有单一仪器数据；跨望远镜偏差的复现是"同仪器 + 不同 seeing"的等价实验，见 exp4-B）。 |
| **DES DM 的孔径改正** | **待核对**：DES DM 官方文档是客户端 JS 应用，正文抓不到；Morganson+2018 经 ar5iv 在相关章节前被截断。已核到的只有 VizieR DES DR1 目录对 `MAG_PSF` 的描述（"PSF fit single epoch detections"）。 |
| **Gaia 合成测光的显式积分式** | **未核到**（`[UNVERIFIED]`）。Gaia 官方文档 / GaiaXPy 文档均未印出该式；Montegriffo et al. 的 synthetic photometry 一文本轮未取到正文。⇒ 本报告把 λ 因子的依据归于 **Bessell & Murphy 2012** 的约定 + 本仓冻结合同公式。 |
| **LSST "改正到无穷孔径"的措辞** | **NOT-FOUND**。LSST 文档没有该字面表述；"改正到 `slot_CalibFlux`" 是**源码级**证据（`shouldApCorr=True`）。 |
| **PS1 引用的卷/文章号** | **待核对**：`[CR]` 核到 ApJS 251, 6 = "Pan-STARRS Photometric and Astrometric Calibration"，而 psphot 的孔径改正原文取自 arXiv:1612.05244（"Pan-STARRS Pixel Analysis…"）⇒ 两者关系未独立核对。 |
| **引用订正** | 本轮核出 **11 条**候选引用的错误（含任务书里的「Labbe et al. 2003 最优孔径」为**主题性错误**），清单见 `references/f-instr-survey.md` 顶部。 |

---

## 附录 A 复跑

```bash
export TMPDIR=/dev/shm/astrocs_finstr
bash 实验/SCI-B/code/reverse_verify/f_instr/run_all.sh      # exp0-exp4, 约 2 min
python3 实验/SCI-B/code/reverse_verify/f_instr/exp5_psf_shape.py   # 约 5 s
```

产物：`run/reverse_verify/f_instr/{exp0_noise_validation,exp1_recovery,exp2_aperture_dependence,exp3_seeing_null,exp4_gain_recovery,exp5_psf_shape}.json`。
