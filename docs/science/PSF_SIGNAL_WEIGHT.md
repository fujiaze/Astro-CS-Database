# PSF Signal Weight 科学定义与可选集成模式

> 上游：ASTROCS_DESIGN.md §2.2（创新点二：跨帧绝对信噪比）、§4.4（输出合同）

文档 ID：SCI-PSFW-001
状态：TARGET_NORMATIVE
上位：docs/science/UNIFIED_SCIENCE_MODEL.md
依据：PixInsight PSF Signal Weight/PSF SNR 方法、Horne/Naylor 最优提取、Zackay & Ofek 多图像点源最优组合。

## 1. 项目决定

科学叠加权重**只能来自纯净信号与噪声之比**——即由 SNR 换算的逆方差；要求**跨帧可用**：**不基于参考帧**、不依赖帧内相对基准，而是**绝对标定**（最高设计 §3.1）。

PSF 相关的量因此分两类，不得混名：

1. **psf_information_weight**：基于观测模型的点源信息权重，是 Phase2 唯一的科学权重来源；
2. **PSF 拟合质量代理**（FWHM、残差尺度、以及受 PixInsight PSFSW 启发的稳健复合量）：**只作诊断**，不计入科学叠加权重，不参与 Phase2 权重选择。

## 2. 严格点源信息权重

对已归一到公共通量尺度的帧：

~~~text
Q_k = a_k P_kᵀ C_k⁻¹ d_k
W_info,k = a_k² P_kᵀ C_k⁻¹ P_k
F_hat = ΣQ_k / ΣW_info,k
Var(F_hat) = 1 / ΣW_info,k
~~~

这是 science_objective=point_source_detection 或 point_source_photometry 的默认权重。白噪声近似为：

~~~text
W_info,k = a_k² / (sigma_pix,k² A_NEA,k)
A_NEA,k = 1 / ΣP_k,p²
~~~

它同时反映透明度/光度尺度、背景与读噪、PSF 集中度，并具有 1/flux² 信息单位；在模型成立时可宣称最小方差/最大点源 SNR。

## 3. PixInsight-style 稳健 PSFSW

PixInsight 将 PSFSW 定义为 hybrid PSF/aperture photometry 的综合图像质量估计器：PSF 总 flux 表示总 signal，mean PSF flux 表示 signal concentration，分母结合稳健 noise 与稳健 mean background；归一常数把典型数值调到实用范围。其 PSF SNR 则采用 ratio-of-powers，并被建议用于只追求集成图像 SNR 的权重。

> **PixInsight 官方公式（2026-09-17 核对；GitLab `Reference-Documentation/docs/ImageWeighting/02-PSF_Flux_Weighting_Algorithms.pidoc` master，与官网 `https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html` 式号一致）**
> - PSFSW（式[16]）：`w_PSF = c1·(Σ_{j=1}^n f_j)·(Σ_{j=1}^n f̄_j) / (c2·σ_n·M*)`。`f_j` 为 FWTM 椭圆孔径内逐像素减局部背景之和（式[7]，**不使用拟合振幅 A**，故称 hybrid PSF/aperture photometry）；`f̄_j=f_j/(π·r_x·r_y)` 为 mean PSF flux（式[8]，信号集中度）；`M*=median(R*)` 为 MMT 残差（式[12]）的中位数（式[13]，默认尺度 256 px）；`σ_n` 为 MRS 或 `N*` 噪声估计（文章称默认 MRS）。
> - PSFSNR（式[18]）：`SNR_PSF = c3·(Σ_{j=1}^n f_j)²/(c4·σ_n²)`。**分子是 (Σf_j)²，不是 √(Σf_j²)**；官方元数据里 `PSFFluxPower=Σf_j²` 标注 “Currently not used, reserved for future extensions”。
> - 标准 SNR（式[20]）：`SNR=σ²/σ_n²`（全局尺度估计/噪声方差；官方明确指出它受背景梯度与天光正向影响）。
> - 文章版常数：`c1=8.0832×10⁻⁶, c2=9.0×10⁺⁶`（式[17]）；`c3=1.350×10⁻⁷, c4=4.987×10⁺⁶`（式[19]）。标定集 = 1000 幅 4096² 合成图（背景高斯 σ=0.001/均值 0.015、平均 1500 颗可检测星、Moffat β=4 FWHM=5 px、Poisson+高斯噪声），调至中位 PSFSW=1、中位 PSFSNR=中位标准 SNR=3.029。**PCL 2.10.4 头文件**为 `c1=5.326×10⁻⁶, c3=1.316×10⁻⁷`（c2/c4 相同）——引用任何常数必须带版本。
>
> **AstroCS 实现披露（`lib/algorithms/photometry/cpp/src/psfsw.cpp:312-314`；`lib/algorithms/photometry/include/astrocs/v6/psfsw.h:57-61`）**：本项目复合为 `Wt=C_norm·S^α·Conc^β/(N^γ·B^δ)`，冻结版本 `PSFSW-COMPOSITE-V1` 取 `α=2, β=1, γ=2, δ=1, C_norm=1.0`；其中 `S_k=Σ fhat`（共同星 PSF 通量之和）、`Conc_k=mean(fhat)/A_NEA`、`N_k=1.482602218505602·MAD({fhat})`（**共同星的星间通量散度，不是图像噪声 σ_n**）、`B_k=b̄_k·A_ref,k`（稳健背景×参考面积）。因此本项目是**受 PixInsight PSFSW 启发**而非**等价于式[16]**：指数（α=2,γ=2 vs 1,1）、`N` 的语义（星间散度 vs 图像噪声）、`B` 的面积因子三处均不同。该复合的指数与阈值在实现中标注 `PENDING_OWNER_SIGNOFF`，尚无本项目 L1 合成数据标定记录；不得据「PixInsight 同类」推定其最优性。
> - 依据出处：PixInsight .pidoc 式[7][8][12][13][16][17][18][19][20]；PCL 2.10.4 Doxygen `PSFSignalEstimator.h`；`lib/algorithms/photometry/cpp/src/psfsw.cpp:233-238,312-314`；`lib/algorithms/photometry/include/astrocs/v6/psfsw.h:42-64,137-138`。

AstroCS 实现 psfsw_robust_weight 时必须保留这些特征，但为避免星表选择偏差增加以下约束：

- 只在同一波段、同一目标/重叠连通分量、光度已归一的帧组内比较；
- 使用跨帧匹配的共同恒星集合或显式 selection-function 校正；
- 排除饱和、混合、拖线、移动源、严重 PSF 失配和边缘截断源；
- signal、concentration、noise、background 四个分量分别写入产品；
- 指数、截断、稳健估计器和归一常数版本化，训练/调参样本不得与最终验收样本相同；
- 输出无量纲相对值，按组归一（如 median=1），不得写入 ivar、variance 或 W_info；
- 无共同星集、背景非正且变换未定义、有效星不足或选择偏差门失败时 unavailable，不回退成 median source SNR。

项目不要求逐字复制 PixInsight 的实现常数；项目公式必须通过公开文献语义、独立推导和真实数据优化后冻结。若选择精确兼容模式，则字段另命名 pixinsight_psfsw_compat 并记录所兼容版本。

## 4. Phase2 支持的选择

| mode | 权重 | 用途 | 可作最优性声明 |
|---|---|---|---|
| point_information（默认） | W_info 或 Q/W | 点源检测、点源测光、proper coadd | 模型和 covariance 门通过时可以 |
| psf_snr_power | ratio-of-powers（**DEFERRED，当前未实现**） | 设计占位；生产模式门显式拒绝 | 不适用（不得进入生产路由） |
| surface_gls | AᵀC⁻¹A | 扩展源/面亮度 | GLS 假设成立时可以 |

Phase2 配置必须显式选择 mode。默认不得由检测到多少颗星等偶然因素自动切换。

> `psf_snr_power` 的 DEFERRED 证据：`lib/infrastructure/cli/v6_runtime_contract.h:110-114` 将其路由为 reject（`FZ-MODE-DEFERRED`）；`lib/algorithms/coverage/include/astro/phase2/coverage.h:170-182` 的生产模式门 allowed={point_information, surface_gls} 并显式拒绝 `psf_snr_power`；`docs/contracts/DATA_SEMANTICS.md` §31.3 标其为 DEFERRED/NOT_IMPLEMENTED。本表该行仅保留设计占位，不得据其声称已有实现。

> `psfsw_robust_weight` 已按"权重只能来自纯净信号与噪声之比、跨帧可用、不基于参考帧、绝对标定"的判据退役（最高设计 §3.1）：生产模式门走 `FZ-MODE-RETIRED` 显式拒绝 + 迁移提示，不静默接受；拒绝消息含被拒 mode、允许集与迁移路径。

## 5. 复合权重与不确定度的边界

psfsw_robust_weight 可以决定 conventional coadd 中帧的相对贡献，但不能反向定义输出像素 variance。最终 variance/covariance 必须从实际线性组合系数和输入 covariance 传播：

~~~text
C_out = R C_in Rᵀ
~~~

若复合权重还含 seeing/concentration penalty，它会改变分辨率与噪声折衷；必须输出 effective PSF，并报告相对 point_information 和普通 ivar 基线的 detection power、FWHM、通量偏差和面亮度偏差。

## 6. 标量与空间模型

- W_info(x,y) 默认是空间量；只有通过均匀性/信息损失门才压为帧标量。
- psfsw_robust_weight 是帧组内相对标量，但其四个输入分量必须带空间摘要 p05/p50/p95 和有效覆盖；显著空间非均匀时拆成 region/tile 权重或拒绝标量模式。

## 7. 强制验收

1. 注入点源验证 1/sqrt(W_info) 与实测 flux dispersion；
2. 透明度、seeing、背景和 read noise 单变量扫描方向正确；
3. 改变不相关星表深度/检测阈值不得显著改变共同星集 PSFSW；
4. psfsw_robust 在预注册 M42/银心及合成集上，与等权、exposure、pixel-ivar、W_info 比较；
5. 分别报告点源 detection power、photometric variance、effective PSF、扩展源偏差和伪影；不得只以“看起来更好”通过；
6. 零星、少星、拥挤、严重梯度、云、拖线、不同 FOV 和不同波段都有 fail-closed 测试；
7. 权重分量和最终权重的负向 mutation 必须使门变红。

## 7a. SCI-B 定案结论（跨帧绝对 SNR 传递链，实验闭环）

> 依据：实验单元 `实验/absolute-snr/`（报告 `README.md`、结果 `results/b1..b6*.json`、复跑 `code/run_all.sh`，固定 seed 20260921）。
> 本节只**确认**已有定义与给出实验判据，不改 §2/§3/§5 公式、不改默认容差。

1. **入库量是通量型未加权原始 SNR**：帧级 `SNR=F_signal/σ_F`，`σ_F⁻²=ΣP_i²/σ_i²`，`σ_i²=(sky+dark+RN²+F·P_i)/g²`（**该式以 e⁻ 为单位**：`sky/dark/RN/F` 均为 e⁻；等价 ADU 口径见 `07_noise_snr.md` §4.2a 的 `σ_sky²+(RN/g)²+F·P_i/g`，其中 `F` 为 ADU）（Horne 1986, PASP 98, 609, DOI 10.1086/131801）。天光只进噪声项、不进信号项；MC 真值在 26+29 个扫描点上全部 ≤3σ（max|z|=2.68）。**σ_sky 入参语义已冻结**（`SHOT_ONLY` / `EMPIRICAL_TOTAL_RMS`，二选一显式声明、读噪只计一次），见 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.2a 与 `docs/science/NOISE_MODEL.md` §9a。
2. **PSFSNR / PSFSW 都不入库**：PixInsight PSFSNR 是功率比口径、PSFSW 是权重；二者只作方法学对照。权重在 Phase2 消费时由 `w_k=SNR_k²/F_ref,k²≡1/σ_F,k²` **现场换算**（恒等相对偏差 2.22e-16，`results/b4_integration.json`）。
3. **F_ref 锚定**：`F_ref,k=10^(-0.4(m_ref−ZP_k))`、`m_ref=6.0`、逐帧独立、**必须同帧配对**；锚定权重相对该源电平 oracle 权重的散度惩罚在 `|m−m_ref|≤4` 内 ≤0.6%、6 等（10 倍通量）处 3.6%；定义 F_ref 与换算 F_ref 不同源时效率损失随 ZP 散度 1.0 mag 达 30.1%（散度=0 时严格归零）。
4. **拟合权重 ≠ 堆叠权重**：带杠杆 `h` 的拟合值方差为 `σ²h`，不能按 `1/σ²` 当独立测量堆叠（实测方差高 52.3%，解析 50.4%）；等杠杆时两者严格等价（损失≡0）。稳健拟合（Huber k=1.345, IRLS）在 5%×10σ 离群下把偏差从 0.426 压到 0.084，干净数据下不损失。
5. **Phase3 传递**：`C_out=R C_in Rᵀ`（对角元 MC/解析 0.9980）；点源信息量必须按**输出 PSF**与**完整 C_out** 重算（`Var=1/(P_outᵀC_out⁻¹P_out)`，实测 9.105 vs 解析 9.071）；只取对角 `Σc_k²u_k` 使输出方差低估 1.37 倍，其"宣称方差"只有实际散度的 32.5%。
6. **非退化判据（强制）**：空间权重/σ 场的精度判据用**权重效率损失** `E=Var_w/Var_opt−1`（E=0 ⇔ σ̂∝σ_true，全局尺度相消）；"帧级臂 RMSE ≤ K·s_field"类判据对任意真值场恒真（对抗场下 E=7.17 仍绿），**不得充当证据**（`results/b6_gates_audit.json`）。
7. **三口径适用域**：默认 `sparse_reconstruct`(Δ=64) 在地面视宁度受限域三帧全部胜出帧级标量；HST 类高对比结构域帧级标量更优（cell 稳健 MAD 偏差随 Δ 从 +0.029 增到 +0.301 dex）。稠密口径 4096² = 67,108,864 B = 64 MiB/帧 = 1 MiB 预算的 64 倍，**稠密超门结论成立**。

## 8. 禁止事项

- 把实际恒星 median(SNR) 直接命名 PSFSW；
- 把 PSF 拟合 residual、FWHM 或背景任一项单独冒充完整 PSF signal weight；
- 把无量纲复合质量写入 inverse variance；
- 未声明共同星集/selection function 就跨天区比较 PSFSW；
- 用 PSFSW 的经验成功替代 Q/W 与 covariance 的科学产品。

## 9 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §2/§3/§5 公式与 §7 验收。

- **点源信息权重 Q=aPᵀC⁻¹d、W=a²PᵀC⁻¹P、Var(F)=1/W**：Horne 1986, PASP 98, 609；Naylor 1998, MNRAS 296, 339；Zackay & Ofek 2017, ApJ 836, 187（arXiv:1512.06872）。
- **proper coadd / 信息保持组合**：Zackay & Ofek 2017, ApJ 836, 188（arXiv:1512.06879）。
- **白噪声 W_info=a²/(σ_pix²·A_NEA)、A_NEA=1/ΣP²**：噪声等效面积定义见 Horne 1986/Naylor 1998；实现对照 photutils（BSD-3-Clause）的 effective PSF/等效面积与 MoffatPSF 归一。
- **PSFSW/PSFSNR 方法学**：PixInsight Reference, New Image Weighting Algorithms（https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html）；**AstroCS 不照抄其标定常数**（§3）。
- **C_out=R C_in Rᵀ**：Fruchter & Hook 2002, PASP 114, 144；Zackay & Ofek 2017 II。
- **已定案（原 UNRESOLVED）**：与 `docs/science/CONTROL_WEIGHT_SNR.md` §2a 的 `frame_snr` 语义冲突已按「同名两义分离」定案（负责人 2026-09-19 裁决 A1/C1，claim `FIX-SCI-SNR-CANON-001`）：Phase1 HiPS 的 `frame_snr` = **点源（PSF）信号 SNR**（纯信号/噪声，`F_signal` 已扣局部背景、天光只进 `σ_F`）；stage2 的 `local_snr`/`frame_snr_medians` = 相对质量权重场（改名 `quality_weight`）。

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

