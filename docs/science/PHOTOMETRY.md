# Photometry Science (SCI-PHOT)

> ID: SCI-PHOT-001  状态: FROZEN (T103 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-PHOT-001..  模块: photometric_calib (flux_calibrator)

## 1 目的与非目标

- **目的**：将仪器流量 `F_instr` 校准到**锚在 Gaia XP 绝对分光刻度（CALSPEC 溯源）的模型通带**光度尺度；模型通带当前为 `T(λ)·Q(λ)·λ`，**不含光学系统透过率与大气消光**（记为未建模项）。在模型通带内的结果可称「绝对通量（Gaia XP 刻度）」；跨通带/换系统/波段外通量不在本合同范围。估计零点 `location`、尺度因子 `scale` 及残差 QA `sigma_residual / sigma_mag`。<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S6) -->
- **非目标**：不处理带通外颜色项高阶效应（仅 QA 暴露残差分布）；不估计逐像素噪声方差（SCI-NOISE 边界）；不做大气消光时变建模。
- **测光标定语义（负责人 2026-09-19 裁决，claim `FIX-SCI-SNR-CANON-001`；不可协商）**：标定目标是**真实测光坐标系（星等）**，手段 = 星点光通量积分 + Gaia + CCD QE + 滤镜透过率曲线；**消除物理单位，只使用星等**。标定因子（本文件 `scale`、Phase1 标定面 `k_photo`）的**绝对值无物理意义**——它把增益/口径/曝光等**未知量全部吸收**；**禁止**用任何物理闭合式（如 `k = g·h·c·1e9/(A·t)`）反推仪器参数或论证其合理性（设计前提 = **FITS 头拿不到这些量**）。**有意义的判据只有一条（尺度无关）**：**测光一致性**——施加后星点**星等**与 Gaia 的残差（散度/MAD）必须小。
**「帧间一致性」（各帧 `k`/`scale` 落在同一测光体系）是语义目标与报告字段，不是门禁判据**
（负责人 §9.49 定案 2「这玩意应该是帧间独立的，为啥要组间对比」；变更 claim `PHOT-GATE-DROP-001`）。
门只有一个 = 单帧标定是否可信，与其它帧无关；跨帧 `k` 不同是正常的。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `F_instr` | 仪器通量 (ADU；e⁻ 需 gain，当前不可得) | 输入 |
| `F_syn` | 合成通量 = `∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`（Gaia 星表模型） | 输入（单位：W·m⁻²·nm，模型通带积分辐照度）<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S7) --> |
| `r_i` | `log10(F_instr/F_syn)` dex | 定标核心 |
| `delta_i` | `−2.5·log10(F_instr)−G_Gaia` mag | 星等一致性 |
| `location` | IRLS/Tukey 稳健位置（dex） | `star_matcher.cpp:478-525` |
| `scale` | `10^{−location}` 校正因子 `I_cal=I·scale` | 输出 |
| `S` | `MAD(r)/0.6744897501960817` 初值尺度 (dex) | `star_matcher.cpp:21-27` |
| `c` | Tukey 形状参数 `4.685` | 同上 |
| `sigma_residual` | `MAD(r_inliers)/0.6744897501960817` dex | QA |
| `sigma_mag` | `2.5·sigma_residual` mag | QA |
| `mag_tolerance` | 星等一致性阈 `3.0 mag` | `pc_api.cpp:139,398` |
| `psf_status,qf` | 饱和/质量标志 | `snr_estimator` |

## 3 物理量和单位

- `F_instr`: ADU（e⁻ 需 gain，当前不可得）；`F_syn`: W·m⁻²·nm（模型通带积分辐照度，`F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`）；二者**不同量纲**，其比值的对数即 `location`（见 DATA_SEMANTICS §14.3）；`r, location, S, sigma_residual`: dex（`r_i = log10(F_instr/F_syn)` 是**有量纲比值**的对数，`location` 单位 dex(ADU/[F_syn 单位])，`scale = 10^{−location}` 单位 [F_syn 单位]/ADU）；`delta, sigma_mag`: mag；`sigma_cal_rel`: 相对误差（`sigma_cal_rel = ln10·sigma_residual`）；`qf` 无量纲标志。<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S1/S7) -->

## 3a 坐标 frame

光度定标**不做空间坐标变换**：参考星表为 Gaia DR3（ICRS/J2000，与 WCS 输出同系）；交叉匹配沿用 WCS 求解后的天球坐标（SCI-WCS），本层只工作在 flux/星等域；帧身份沿用 `frame_id`（DATA_SEMANTICS §5）。

## 4 输入有效域

- 每颗星 `F_instr>0, F_syn>0` 有限值；饱和星（`psf_status!=0` 或 `qf & (SATURATED|HAS_SATURATED)!=0`）不进入匹配与定标，计 `rejected_quality`（`star_matcher.cpp:35-40`）。
- 参考星数 `|r_consistent|>=3` 才进 IRLS，否则 `NO_DATA`；`|r_inliers|>=2` 才估计 `sigma_residual`，否则 `sigma_residual=0`（`552-559`）。
- `S>0` 时迭代 `max_iter=50, tol=1e-6`；`S==0` 跳过 IRLS 取 `median(r)`（`478-525`）。

## 5 连续定义

```text
r_i = log10(F_instr,i / F_syn,i)                         # dex

# 星等一致性预过滤 (进入 IRLS 前)
delta_i = −2.5·log10(F_instr,i) − G_Gaia,i
median_delta = median(delta)
预拒绝 i  若  |delta_i − median_delta| > mag_tolerance    # mag_tolerance=3.0

# IRLS + Tukey biweight (对 r_consistent)
S = MAD(r_consistent)/0.6744897501960817,  location_0 = median(r_consistent)
迭代直到 |loc_new−loc_old|<1e-6 或 50 步:
  u_i = (r_i − location)/(c·S),  c=4.685
  w_i = (1−u_i²)²   (|u|<1),  0 否则
  location = Σ w_i·r_i / Σ w_i
若 S==0 ⇒ location = median(r_consistent), robust_iterations=0

scale = 10^{−location}          # I_cal = I·scale
sigma_residual = MAD(r_inliers)/0.6744897501960817   # r_inliers={i|w_i>0}
sigma_mag = 2.5·sigma_residual
outlier_rate = 1 − |r_inliers|/|r_consistent|
```

与 `lib/algorithms/photometry/cpp/src/star_matcher.cpp:21-27,435-525,552-559`（IRLS/Tukey/scale）及 `lib/algorithms/photometry/cpp/src/pc_api.cpp:139,398`（`mag_tolerance=3.0` 实际传入点；`star_matcher.cpp:241-248` 仅为 `psf_valid` 诊断）一致。

## 6 假设

- Gaia 合成星表（**模型通带相对刻度**，锚 Gaia XP 光谱形状；`F_syn=∫F_λTQλdλ` 不含 `1/(hc)` 等绝对归一，常数由 `location` 吸收，故**不宣称绝对通量刻度**）在本模型通带内提供可信参考；**模型通带不含光学系统透过率与大气消光**（未建模项，跨帧会成为帧间系统差）；大气/仪器零点在观测尺度稳定；饱和判据可靠（`psf_status==0` 且无 `SATURATED` 标志）。<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S6) -->

## 7 独立不变量

- **零点平移不变量**：`F_instr` 全体同乘因子 `k` 时 `location` 增 `log10 k`，`scale` 相应除 `k`，`sigma_residual` 不变。
- **尺度单调性**：`F_instr/F_syn` 比值越大 `location` 越大，非饱和样本集下单调。
- **鲁棒性**：注入 20% 离群 `r` 时 IRLS `location` 变化 `<0.1 dex`（Tukey 权重截断）。
- **S=0 退化**：全体 `r` 相等时 `S=0` ⇒ `location=median(r)`，不迭代。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无星/星数不足 | 返回 `NO_DATA` | `star_matcher.cpp:478` 前校验 |
| `S==0` (MAD=0) | 跳过 IRLS，`location=median(r)` | `star_matcher.cpp:552` |
| 饱和/质量异常 | 不参与定标，计 `rejected_quality` | `psf_status!=0 \|\| qf&SATURATED` |
| 预过滤全拒 | `r_consistent` 空 ⇒ `NO_DATA` | `mag_tolerance=3.0` 判别 |
| 非有限 `F` | 显式拒绝，不进入 `r` | 参数校验 |

## 9 精度策略

- FP64 对数空间；flux 比值不经 `ivar` 加权（ivar 不用于此层）；IRLS 收敛阈 `1e-6` dex，迭代上限 50。

## 9a 专属问题回答（SCI-002 指定问题逐项）

- **WCS frame/pixel convention**：不在本层处理；交叉匹配依赖 SCI-WCS 输出的 ICRS/J2000 坐标（§3a）。
- **PSF 参数**：星点通量来自 PSF 拟合域（PSF.md），饱和判据 `psf_status/SATURATED` 决定剔除（§4）。
- **aperture/flux/background**：`F_instr` 为仪器通量（ADU·px 或 e⁻ 同尺度）；无孔径背景扣除项——背景已在 PSF/测光上游处理；`F>0` 有限值为有效域，非有限显式拒绝（§8）。
- **photometric scale 与不确定度**：`scale=10^{−location}`（IRLS/Tukey 稳健位置）；不确定度 `sigma_residual`（dex）→`sigma_mag`（mag）→`sigma_cal_rel`（相对）；`q_psf`/`sigma_residual` 禁止混为逐像素 ivar（§10）。

## 10 不可接受变化

- 改变 `c=4.685`/`tol=1e-6`/`max_iter=50`/`mag_tolerance=3.0` 阈值而无 SCI 变更；
- 将 `q_psf` 或 `sigma_residual` 混为逐像素 ivar；
- 忽略饱和/质量标志使饱和星进入定标；
- 用物理闭合式反推仪器参数（增益/口径/曝光）或据此论证 `k_photo`/`scale` 的合理性；
- 为 `k_photo`/`scale` 设**绝对窗口**（绝对值依赖未知仪器参数，不存在有意义的绝对窗口；验收只用**测光一致性**；「帧间一致性」是语义目标/报告字段，**不是门禁**，见变更 claim `PHOT-GATE-DROP-001`）。

## 11 验证 Oracle

- **合成注入**：已知 `scale` 的 `F_instr=k·F_syn` 注入场，估计 `location≈log10 k`（`rtol 1e-4`）。
- **鲁棒门**：注入 20% 离群点，`location` 偏差 `<0.1 dex` 且离群权重为 0。
- **S=0 门**：常数 `r` 场直接取 median 通路，不迭代。
- **Python 参考**：NumPy `median/MAD/Tukey` 对同 `r` 复算 `location/scale`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-PHOT-001` IRLS-Tukey 零点估计
- `ALG-PHOT-002` 星等一致性匹配与 QA (`sigma_residual/sigma_mag/outlier_rate`)

## 13 追溯与测试

- 权威文件: `docs/science/PHOTOMETRY.md` (SCI-PHOT-001)
- 实现: `lib/algorithms/photometry/cpp/src/star_matcher.cpp` (`location/S/scale, mag_tolerance, IRLS`), `lib/algorithms/photometry/cpp/src/pc_api.cpp` (`pc_calibrate_simple`)
- 公开 API: `lib/algorithms/photometry/cpp/include/photometric_calib.h` (`pc_calibrate_simple, pc_calibrate_simple_with_gaia`)
- 测试: `TST-PHOT-001` 合成零点、`TST-PHOT-INV-001` 鲁棒性、`TST-PHOT-FAIL-001` 饱和拒（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Tukey biweight `c=4.685`（95% 高斯渐近效率）：Mosteller & Tukey 1977, *Data Analysis and Regression*；定位经 [PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/) 实证核对（"c=4.685 yields 95% asymptotic efficiency at the Gaussian"）。
2. MAD→σ 换算 `1/0.6744897501960817 = 1.482602218505602`：标准正态 MAD 分位恒等式（`Φ⁻¹(3/4) = 0.6744897501960817`，double 逐位等于 `1/1.482602218505602`；与 `docs/science/NOISE_MODEL.md` §14 同值），教科书级恒等式，Project-defined 采纳。4 位截断写法 `0.6745` 与全精度值相对差 **+1.5196e-05**，只允许出现在「≈」语境并标注该偏差，不得再作权威值（W4-A6 处置 V12-N-03）。
3. Gaia DR3 合成通量参考：Gaia Collaboration 星表发布文献——bibcode 级定位（未逐页核验），本合同仅消费星表数值，不转述其定标推导。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5 公式与常数。

- **Tukey biweight w=(1−u²)²、c=4.685（95% 高斯渐近效率）**：Beaton, A. E. & Tukey, J. W. 1974, Technometrics 16, 147（DOI 10.1080/00401706.1974.10489171）；Mosteller & Tukey 1977；Huber & Ronchetti 2009, Robust Statistics, 2nd ed., Wiley（ISBN 978-0-470-12990-6）。
- **MAD→σ 换算 0.6744897501960817 = Φ⁻¹(3/4)**：标准正态分位恒等式；稳健性讨论见 Rousseeuw & Croux 1993, JASA 88, 1273（DOI 10.1080/01621459.1993.10476408）。
- **最优提取（PᵀC⁻¹P 结构）**：Horne, K. 1986, PASP 98, 609（DOI 10.1086/131801）；Naylor, T. 1998, MNRAS 296, 339。**边界**：二者为已知 profile/方差下的最优提取；AstroCS 本层做的是 Gaia 交叉定标的零点/尺度，不是逐源最优提取（§1 非目标），引用只作统计结构对照。
- **误差口径 FLUXERR/MAGERR 与孔径改正**：Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（SExtractor；DOI 10.1051/aas:1996164）；photutils（BSD-3-Clause）aperture_photometry 的误差传播。**差异**：AstroCS 的 sigma_residual 是**逐星定标散度**（dex），不是 SExtractor 的单源通量误差；两者语义不同，禁止互换（NOISE_MODEL §9a）。
- **Gaia XP 绝对分光刻度与 CALSPEC 溯源**：Gaia Collaboration et al. 2023, A&A 674, A1（Gaia DR3）；Gaia Collaboration et al. 2021, A&A 649, A1（EDR3）；Bohlin, R. C., Hubeny, I. & Rauch, T. 2020, AJ 159, 246；Bohlin et al. 2014, PASP 126, 711；Bessell, M. & Murphy, S. 2012, PASP 124, 140。**核验状态**：文章级（卷页），Gaia DR3 合成通量定标推导未逐页核验。
- **F_syn 数值积分（Akima + 复合 Simpson）**：Akima, H. 1970, J. ACM 17, 589（DOI 10.1145/321607.321609）；复合 Simpson 1/3 公式（教科书级）。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过：合成注入 `location≈log10 k`（rtol 1e-4）、20% 离群鲁棒门 `<0.1 dex`、S=0 退化门、NumPy 复算 rtol 1e-9；
- §7 四不变量门全过（零点平移/尺度单调/鲁棒性/S=0）；
- `tools/science_contract_lint.py` PASS；
- 解析不变量→SYN-002 转换：已知 flux/background 星场、photometric scale 恢复、离群注入用例登记 SYN-002。
