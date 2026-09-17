# SCI-001-S2 报告：其余科学主题三方自审 + 科学文档参考文献/参考代码库补齐

- 任务：工程控制/RELEASE-01 分片 SCI-001-S2（其余科学主题的三方自审 + 科学文档参考补齐）
- 执行者：SCI-S2 SubAgent；工作目录 /workspace/Astro CS Database
- 日期：2026-09-17
- 硬纪律遵守：未执行任何 git 写操作；未修改 lib/、tests/、contracts/、config/、ci/；只写入 run/RELEASE-01/science/SCI-S2-topics.md、docs/science/**（不含 v6/**）、docs/algorithms/**（不含 v6/**）、docs/references/SCIENTIFIC_REFERENCES.md。
- 公式纪律：本任务只补出处/参考清单，未改动任何公式、常数、阈值与容差；判定为“我方有误”的只在 §3 列出，由前台按变更 claim 处理。

## 0 摘要

- 覆盖 11 个主题，每主题至少 1 条对照结论。
- 判定“我方有误需订正”15 项（含 2 项数值常数、1 项方差公式重复计项、1 项孔径误差漏项、1 项引用错配、若干文档-实现不一致；其中 3 项为实现侧/插件侧报告-only，不在本任务文件域）。
- 科学文档补出处处数：16 篇 docs/science 文档新增“参考文献与参考代码库（含许可证）”节 + SCIENCE_SCOPE 参考节扩写（共 17 篇）；27 篇 docs/algorithms 文档新增同款节；docs/references/SCIENTIFIC_REFERENCES.md 新增 §I–§M。合计补出处/参考清单 45 处文档节。
- 新增文献 40 条（编号 21–60）与参考代码库 19 条（含许可证核验）。
- UNRESOLVED 11 条（含 2 条“两篇权威打架”：frame_snr 语义、UPM 加性 vs 乘性模型）。

## 1 方法与证据等级

- 三方：AstroCS 文档/实现（我方） ↔ 独立权威（论文/标准） ↔ 成熟开源实现。
- 证据标注：[V] 本轮逐字/接口核验（web_fetch 原文或 GitHub/GitLab API SPDX）；[S] 次级/摘要定位；[U] 需网络核验。
- 实现行号均为本任务对工作区文件的直接 read/grep 结果；外部行号标注来源版本。
- 独立数值复算：Moffat4 FWHM/σ、β=4 解析通量、MAD→σ、trimmed-mean→σ、√(π/2)、√(2ln1000) 等，见各主题。
- 三路并行子代理分别覆盖 T1/T2/T4、T3/T5/T8/T9、T6/T7/T10/T11；本报告合并其结论。

## 2 11 主题对照表

### T1 校准与方差传播（bias/dark/flat、母版相关性）

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| 标准式 cal=(raw−bias−K·dark)/max(flat,0.1)，K=t_light/t_dark，master_dark 已减 bias（CALIBRATION.md §5:65-75；calibrator.cpp:113-189） | ccdproc reduction_toolbox：“master_dark that has been bias-subtracted so that it can be scaled by exposure time”[V]；LSST ip_isr isrFunctions.py：biasCorrection 减 bias、darkCorrection 减 dark·expScaling/darkScaling、flatCorrection 除法[V] | 无 | 我方正确（与两个成熟实现一致） |
| 兼容式 dark_opt=1：cal=(raw−bias−K·(dark−bias))/flat（CALIBRATION.md §5:68-69） | 同上；K=1 时与标准式代数恒等（§7:118-119） | 仅入参母版约定不同 | 等价 |
| flat_norm=max(flat/median,0.1)，floor 0.1（§5:73） | 无外部标准；LSST flatCorrection 用 scalingType MEAN/MEDIAN 且不假设已归一[V] | floor=0.1 为 Project-defined | 我方正确（项目定义），常数无外部出处 |
| 本层不传播母版方差（§9:144） | UNIFIED_MODEL.md §6 要求 master calibration 参数不确定度进入 variance/covariance | **缺口**：无母版方差/相关性传播实现证据 | 文档-设计缺口（UNRESOLVED） |
| 插件层方差式 V(y)={V(r)+V(b)+α²[V(d)+V(b)]+y²V(f)}/f²（docs/plugins/algorithms_phase1/01_calibration.md:28-32） | V6 实现 v6_calibration_covariance.cpp:313-319,391 与 V6_CALIBRATION_COVARIANCE.md:16：同 master 时 bias 系数折叠为 −(1−α)/f，只计一次 V(b) | **同一 bias 母版被当作两个独立项**（−b 与 +αb）；同 master_id 正确系数为 (1−α)² | **我方有误**（见 §3-1） |
| 量化噪声 q²/12、photon=max(r,0)/gain、read=(rn/gain)²（V6 头:13-15） | 均匀量化噪声方差 q²/12：Bennett 1948, Bell System Technical Journal 27, 446[S] | 无 | 我方正确；建议补 Bennett 1948 出处 |

### T2 星点检测与 PSF 建模（Gaussian/Moffat、LM、FWHM、椭圆度）

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| PSF 侧椭圆 Moffat4：I=B+A/(1+Q)^4，Q=p1dx²+2p2dxdy+p3dy²（PSF.md §5:43-53；dpsf_psf.cpp:84-118） | Moffat 1969, A&A 3, 455[S]；astropy Moffat2D：A(1+r²/γ²)^(−α)[V] | 我方 Q=0.5r²/σ² ⇒ γ²=2σ²、β=4 | 等价 |
| FWHM=1.230310·σ（PSF.md §2:20, §5:51） | astropy Moffat2D.fwhm=2|γ|√(2^(1/β)−1)[V] | 精确 2√2·√(2^(1/4)−1)=1.2303076525901；我方 1.230310 为 6 位舍入（相对 +1.9e-6） | 等价（舍入）；建议全精度或标“≈” |
| flux=2πA·sxsy/3（PSF.md §5:52；dpsf_psf.cpp:428-429） | Project-defined 解析积分；独立复算 ∫(1+Q)^−4 dA=2πsxsy/3 | 无 | 我方正确 |
| 检测侧椭圆高斯 f=B+A·exp(−(x′²/SX+y′²/SY))，SX=2σ²、SY=r²SX、fwhm=2.3548·σ（STAR_DETECTION_ALGORITHMS.md §2:60-66） | 标准 2D 高斯；photutils Gaussian2D[V] | fr/alpha 为项目特有有界重参数化 | 我方正确 |
| 检测/PSF 双母函数不可跨块比较，比值 1.9140（DISP-STAR-007） | 复算 2.354820/1.230310=1.9140091 | 无 | 文档已登记，非错误 |
| LM：(JᵀJ+λI)Δ=−Jᵀr，λ 自适应（STAR_PSF_ALGORITHMS.md §3:73） | Levenberg 1944；Marquardt 1963；Moré 1978；GSL gsl_multifit_nlinear（GPL-3.0） | 文档写 iter≤50/tol=1e-6，实现为 tol=1e-8/max_iter=200（dpsf_psf.cpp:374-375）；用恒等阻尼而非 diag(JᵀJ) 缩放 | **文档表述陈旧**（DISP-PSF-003 已登记） |
| residual_scale=10–90% 截尾均值|残差|；robust_residual_sigma=residual_scale/0.7316727929211932（PSF.md §2:22-23；noise_model.cpp:37,608） | Project-defined 高斯分位积分 | 代码定义的精确值为 **0.7316730952806139**（closed-form+quad+MC），文档/代码值相对差 **+4.13e-7** | **我方有误**（数值常数，见 §3-2） |
| 检测阈值=median(img)+5·bgnoise；bgnoise 行差分+3×5σ clip+×1/√2（STAR_DETECTION_ALGORITHMS.md §2:33-36） | Siril src/algos/star_finder.c:79-82,200：threshold=median+ksigma·bgnoise，ksigma=5.0；行差分噪声估计[V] | 结构一致 | 我方正确 |
| s_factor=√(−2ln0.001)=3.7172（STAR_DETECTION_ALGORITHMS.md §2:52） | 精确 √(2ln1000)=3.716922 | 我方为舍入 | 等价（舍入） |
| q_psf=A/residual_scale（PSF.md §2:24） | 无外部等价量；Project-defined QA 代理 | 无 | 无出处公式（已标 Project-defined） |
| PSF.md §4 写 NaN patch status=BAD | dynamic_psf.h 仅 status 0–3，无 BAD | 文档失实 | **文档有误**（DISP-PSF-003 族已登记） |

### T3 天体测量 platesolve ↔ WCSLIB/Astropy/SCAMP/Paper I-II/Gaia

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| CRPIX=w/2+0.5（1-based）、xp=x+1、CD deg/px（ASTROMETRY.md §5:47-53） | Greisen & Calabretta 2002, A&A 395, 1061（Paper I）§2.1.1[V] | 与 Paper I 1-based 定义一致 | 我方正确 |
| TAN 投影 + SIP A/B 解析、AP/BP 网格拟合（§5:55-62） | Calabretta & Greisen 2002, A&A 395, 1077（Paper II）§2.2；Shupe et al. 2005, ASPC 347, 491（SIP）[S] | astropy 交叉 5.7e-14 deg、roundtrip ≤3.2e-10 px（§5a 实测） | 我方正确 |
| Y-up→Y-down：cd12/cd22 取反、A′=A(−1)^j、B′=−B(−1)^j（§5:64-67） | Paper I §2.1.1 + Project-defined 符号规则 | |det(CD)| 不变（§7） | 我方正确 |
| SIP 前向 A[i][j]=cd_inv·trans.x_ij，A/B 单位 1/px^{i+j-1}（§5:56-57、§3:31） | Shupe 2005 SIP 约定：头系数作用于像素偏移、输出加在中间世界坐标 | 文档未明示“存储的 cd_inv 标度系数”与“FITS 头 A/B（度域）”之间的 CD 因子关系 | **文档表述不清**（需澄清，见 §6-6） |
| 极区剪枝 C=π/2、C45=π/(2√2)（§2:27、§8） | Project-defined Lipschitz 保守盘；astropy/ERFA 可作独立球面几何 Oracle | 无 | 我方正确（项目定义） |
| Gaia DR3 ICRS/J2000 参考星表 | Gaia Collaboration et al. 2016/2018/2021/2023, A&A 595 A1 / 616 A1 / 649 A1 / 674 A1[S] | 只消费星表数值 | 我方正确 |

### T4 测光与通量定标 ↔ SExtractor FLUXERR/photutils/Horne/Naylor

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| r=log10(F_instr/F_syn)，scale=10^(−location)，I_cal=I·scale（PHOTOMETRY.md §5:44-59） | 代数自洽；Horne 1986, PASP 98, 609[V] | F_instr=kF_syn ⇒ location=log10k ⇒ scale=1/k | 我方正确 |
| IRLS+Tukey c=4.685、w=(1−u²)²、iter≤50/tol=1e-6（§5:51-57） | Beaton & Tukey 1974, Technometrics 16, 147；Mosteller & Tukey 1977 | c=4.685 的“95% 效率”一手页码需网络核验 | 我方正确（常数出处已补） |
| sigma_mag=2.5σ_res、sigma_cal_rel=ln10·σ_res（§5:60-61） | 量纲换算标准式 | 无 | 我方正确 |
| 零点标准误 1.253·σ/√N（NOISE_MODEL.md §9a:131） | 精确 SE(median)=√(π/2)σ/√N=1.2533141373 | 1.253 为截断 | 等价（截断）；建议全精度 |
| 孔径 flux_error=sqrt(max(sum,0)+n_in·σ_sky²)，σ_sky=1.4826·MAD（lib/algorithms/photometry/wrapper_phase1/photometer.cpp:32-95） | Howell, Handbook of CCD Astronomy（CCD 孔径误差）；photutils 逐像素误差传播[V] | **漏 gain 与背景估计项** n_in²σ_sky²/n_sky；标准式 σ_F²=F/gain+n_in·σ_sky²(1+n_in/n_sky) | **我方有误**（简化式，见 §3-4） |
| W=a²PᵀC⁻¹P、F_hat=ΣQ/ΣW、Var=1/ΣW；白噪声 W=a²/(σ²A_NEA)、A_NEA=1/ΣP²（PSF_SIGNAL_WEIGHT.md §2:21-35） | Horne 1986；Naylor 1998, MNRAS 296, 339 | 对角 C 时 PᵀC⁻¹P=Σp²/σ²，一致 | 我方正确 |
| F_syn=∫F_λ·T·Q·λ dλ（PHOTOMETRY.md §2:15） | Gaia XP/CALSPEC；光子计数 λ 权重约定 | 缺 1/(hc) 等物理归一，常数被 location 吸收；“绝对通量”表述过强 | **文档表述不清**（建议降级为模型通带相对刻度） |
| PSFSW 复合 w、conc=mean_flux/A_NEA、组内 median=1（PSF_SIGNAL_WEIGHT.md §3:37-51） | PixInsight New Image Weighting Algorithms §2.5/§2.6[V] | 分量分解定性一致，常数/指数 Project-defined，未照抄 | 我方正确（项目定义） |

### T5 噪声模型与 SNR ↔ Starck & Murtagh（MRS/N*）、MAD/MedDev、SExtractor 背景网格

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| σ_bg=1.482602218505602·MAD（NOISE_MODEL.md §5:53） | 1/Φ⁻¹(3/4)；Rousseeuw & Croux 1993, JASA 88, 1273[S] | 复算精确 | 我方正确 |
| 8×8 patch、5σ clip ≤2 轮、最小二乘平面 var=a+bx+cy（§5:51-62） | SExtractor 背景网格：Bertin & Arnouts 1996, A&AS 117, 393 §3[V] | SExtractor 用 mode/median+迭代 σ，我方用 MAD+平面场；**不等价** | 我方正确（Project-defined 变体），引用只作方法学对照 |
| 几何退化 λlo/λhi<1/16（等价 κ≤4）（§4:45） | Project-defined（claim SC-002） | 无 | 我方正确 |
| var_ADU=max(signal,0)/gain+(rn/gain)²（诊断，§5:67） | Newberry 1991, PASP 103, 122；Janesick 2001, SPIE PM83 Ch.2 | 量纲正确（signal/gain 得 e- 数再除 gain） | 我方正确（仅诊断，不入生产） |
| 掩膜半径 r_local(F,FWHM,kσ) 的 Gaussian/Moffat 解析式（§5a:78-79） | Project-defined 尾翼积分；Moffat 1969 | 复算 Gaussian/Moffat 式均正确 | 我方正确 |
| MRS/N*（Starck & Murtagh） | Starck & Murtagh 2006, Astronomical Image and Data Analysis 2nd ed., Springer；Starck, Donoho & Candès 2003, A&A 398, 785[S] | 研究包要求研读，**现行实现未采用小波 MRS/N*** | 选型对照；未实现（非错误，登记） |
| frame_snr 语义（CONTROL_WEIGHT_SNR.md §2a:42-53 为“相对质量权重，不是科学信噪比”） | UNIFIED_MODEL.md §2 表与 plugins/07_noise_snr.md §4.1 为“未加权原始信噪比（对标 PSFSNR）” | **两篇权威打架** | **UNRESOLVED**（见 §6-1） |

### T6 球面 Drizzle / HEALPix 累积 ↔ DrizzlePac/astropy-healpix/Fruchter & Hook/Górski/HiPS

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| w_jp=a_jp/A_drop；F=Σx·w；D=Σa；S=F/D（DRIZZLE.md §5:38-43） | Fruchter & Hook 2002, PASP 114, 144[V] | 标准 drizzle 加权均值/面亮度归一 | 我方正确 |
| var_p=Σv_j·w_jp²/D_p²；ivar=1/var（§5:53-56） | 线性误差传播；DrizzlePac Handbook[V] | 与 C_out=R C_in Rᵀ 的对角特例一致 | 我方正确 |
| pixfrac∈(0,1]、drop=0.5·pixfrac 收缩（§4/§5） | Fruchter & Hook 2002；DrizzlePac Handbook[V] | 语义一致 | 我方正确 |
| hp_res=√(π/3)/nside rad（§2:22） | Górski et al. 2005, ApJ 622, 759；HEALPix 像元面积 4π/(12nside²)[S] | √(π/3)/nside 正确 | 我方正确 |
| 球面 S-H 裁剪 + Girard 定理（§5:63） | Sutherland & Hodgman 1974, Comm. ACM 17, 32；Van Oosterom & Strackee 1983, IEEE TBME 30, 125[S] | 平面算法在球面迁移属 Project-defined | 我方正确 |
| HP_CIRCUMRADIUS_FACTOR=1.25、三层缓冲（§5:65-68） | Project-defined，以 9003 例零漏选门承载 | 无 | 我方正确（项目定义） |
| 常量场按面亮度 B0 构造（§7:80-84） | Project-defined 语义固定（SCI-003） | 正确区分 flux 与 SB | 我方正确 |

### T7 mosaic UPM（g_k·s+b_k(x)、稀疏天光面）↔ SCAMP/SWarp

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| 现行冻结模型：calibrated=raw−C_f(p)，C_f=8×8 control cell 双线性，**纯加性**（PHASE2_UPM.md §1:7-8、§5:47-50） | SCAMP（GPL-3.0；Bertin 2006, ASPC 351, 112）：相对光度**乘性** g_k + 天体联合定标；SWarp：逐帧**加性**背景扣除 | SCAMP/SWarp 标准流程同时含乘性与加性；我方现行只做加性，且明文“乘性尺度差已撤销” | 现行实现与标准流程不同（有意收窄） |
| 目标模型：y_k=g_k·s(x)+b_k(x)，b_k=B_ref+δ_k，稀疏二维样条天光面（docs/plugins/algorithms_phase2/10_sampling.md §4.2、11_upm.md §4.1；UNIFIED_MODEL.md §2） | SCAMP/SWarp 同族问题 | **与本文件 §1/§5 的纯加性现行模型冲突** | **UNRESOLVED**（见 §6-2） |
| control_variance=k_corr·(π/2)·σ_bg²/N_retained；k_corr≥1（PHASE2_UPM.md §5:58） | var(median)≈πσ²/(2N)（Hoaglin et al. 1983；Kendall & Stuart Vol.1）；UPMW-004 ratio 0.997 | π/2 因子正确；k_corr 为 Project MC | 我方正确；k_corr 证据 MISSING |
| w_UPM=quality·control_reliability·control_ivar；w_cell 份额式（§5:52-56） | Project-defined 两级权重；与逆方差加权一致 | control_reliability 实现为配置常量 1.0（非几何量，SC-005） | 我方正确（有已登记缺陷） |
| Huber IRLS δ=1.345 + 弱零锚 0.001（§5:67-69） | Huber 1964, Ann. Math. Statist. 35, 73；Tikhonov 1963[S] | 无 | 我方正确 |
| gauge=连通分量最小 frame_id；frame_id 持久绑定（§5:71-74） | Padmanabhan et al. 2008, ApJ 674, 1217；SCAMP | 与联合定标 gauge 问题同族 | 我方正确 |

### T8 rejection 排异 ↔ sigma-clipping/Zackay 框架/Stetson

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| 7 方法 + wbpp_2_9_1 路由（n<6 percentile；6≤n≤15 winsorized；n>15 linear_fit）（REJECTION.md §5:43-61） | WBPP bestRejectionMethod（PixInsight，非学术软件来源） | 阈值为采纳自软件的 Project-defined 冻结值 | 我方正确（工程选择，已如实标注） |
| sigma/winsorized/averaged 4.0/3.0/8；linear_fit 5.0/3.5/8（§5:55-58） | Siril stacking/rejection 开源对照（GPL-3.0） | 与 Siril 语义同族 | 我方正确 |
| Generalized ESD α=0.05/max_outliers=10（§5:58） | Rosner 1983, Technometrics 25, 165；NIST/SEMATECH e-Handbook §1.3.5.17/§7.1.6 | 无 | 我方正确（NIST 120/120 实证） |
| RCR（§14:164 仅登记“官方 RCR 2.4.7 软件参考”） | Maples et al. 2018, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d）；Konz & Reichart 2023, arXiv:2301.07838 | 缺一手论文引用 | 我方正确但**引用缺口**（已补） |
| percentile scale=|median| 在近零天光塌缩（§8a:102-114，SC-005） | 真实缺陷，MC 全拒率 73.9%/70.1% | 未修复，已如实登记 | 我方正确（缺陷已登记，不由容错掩盖） |
| 全拒容错域 n=4（§4:34-36） | Project-defined（rejection.cpp:1860-1874） | 可达域恰为 n=4 | 我方正确 |

### T9 integration 逆方差叠加与 point_information ↔ Zackay & Ofek/Naylor

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| signal=Σw·x/Σw；support=max(accepted)；w=0 合法不贡献（INTEGRATION.md §5:47-61） | 加权均值/逆方差加权（教科书；Aitken 1935 GLS）；Zackay & Ofek 2017, ApJ 836, 187/188；Naylor 1998 | 本层不编码 ivar 语义（§9/§10），是通用加权均值；生产权重由 SCI-NOISE/SCI-UPM 提供 | 我方正确 |
| 五态状态码 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT（§5:51-60） | Project-defined 显式状态机 | 无 | 我方正确 |
| support 仅消费 pr.support，取 max 不二次聚合（§5:58、§10） | Project-defined 覆盖并集保守下界 | 无 | 我方正确 |
| point_information W=a²PᵀC⁻¹P=1/Var(F_hat) | UNIFIED_MODEL.md §3/§4；PSF_SIGNAL_WEIGHT.md §2；Horne 1986；Zackay & Ofek 2017 I | **docs/science/INTEGRATION.md 未定义/未消费 point_information**（设计上 reducer 无知，交叉引用缺口） | 我方正确；跨文档交叉引用需补 |
| 上游协方差不进逐像素 variance（UNCERTAINTY §52-72） | Fruchter & Hook 2002；Zackay & Ofek 2017 II | 已如实登记低估边界 | 我方正确（有文档化边界） |

### T10 export 投影（8 种，FITS WCS Paper II）↔ WCSLIB/Astropy

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| registry 冻结集合 = TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA，v3 实现 4/8（PHASE3_PROJ_IMPL.md §15.1/§15.3） | Paper II Table 1（Calabretta & Greisen 2002, A&A 395, 1077）[V]；astropy/WCSLIB 可构造[V] | 未实现四投影只登记集合成员，逐式待 P3-001 | 我方正确（登记面） |
| TAN：R=cotθ，X=−R sinφ，Y=R cosφ（§15.3:458-461） | Paper II §2.2/Table 1 | 与 astropy 22 组配置 max 6.854e-13°（R-1 §2） | 我方正确 |
| SIN：R=cosθ；逆 ρ>1 → HEMISPHERE（§15.3:462-465） | Paper II Table 1（orthographic） | 一致 | 我方正确 |
| CAR：X=φ，Y=θ；native 极行 |θ|≥90° → PARAM（§15.3:466-470） | Paper II Table 1 | 一致；极点行塌缩 fail-closed 是项目产品语义 | 我方正确 |
| AIT：D=√(1+cosθcos(φ/2))，γ=√2/D，X=2γcosθsin(φ/2)，Y=γ sinθ；A=xp²/4+yp²，A>1→HEMISPHERE（§15.3:471-477） | Paper II Table 1（Aitoff 含 √2） | 复算椭圆半轴 X=2√2 rad=162.0569°、Y=√2 rad=81.0285°，与标准一致 | 我方正确（v3 已订正旧 A<2 判据） |
| CRPIX 处 world==CRVAL（含 CRVAL2）；LONPOLE 默认（§15.3/§15.4:500-501） | Paper I §2.1.1；Paper II §2.1/§2.2 | v3 已订正 CAR/AIT 的 CRVAL2 语义（legacy D1/D2/D3/D4 冻结对照） | 我方正确 |
| 可执行标准 | astropy 7.0.1（BSD-3-Clause）/WCSLIB（LGPL-3.0）逐点对拍 | 无 | 我方正确 |

### T11 HiPS/HEALPix 存储 ↔ IVOA HiPS 1.0/HEALPix 规范

| 我方条款（出处） | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| hips_frame='icrs'，值域 {icrs,galactic,ecliptic}；'equatorial' 废止（PHASE3_HIPS_TO_FITS.md §4:63） | IVOA HiPS 1.0 §4.4.1[V]（REC 2017-05-31；草案 PR-HiPS-1.0-20170406） | 与规范一致 | 我方正确 |
| tile W=512、leaf_order=order_sel+log2(W)、NESTED 唯一（§2:39-40、§5:85-88） | IVOA HiPS 1.0 §3/§4.1/§4.2.1[V]；Górski et al. 2005[S] | HiPS 允许任意 2 的幂 tile 宽；alpha 收窄为 W=512（显式拒绝其他值） | 我方正确（收窄不扩大） |
| tile 值=面亮度（非积分通量）；flux-per-pixel 输入显式拒绝（§9a-8:145） | IVOA HiPS image 语义 | 一致 | 我方正确 |
| coverage 二值；NaN 传播但 C=1（§5:92-97） | Project-defined 真值表（SCI-FIX-PROJ 订正） | 无 | 我方正确 |
| order_needed=ceil(log2(√(π/3)/(W·s_out_rad)))（§5:76-80） | Project-defined（tile 像素角尺度 ≤ s_out） | 复算正确 | 我方正确 |
| variance/ivar 子产品消费与传播（§9a-10 与顶部 DATA-UNC-001 更新块） | UNCERTAINTY_AND_COVARIANCE.md Phase3 节（DATA-P3-UNC-001）；DATA_SEMANTICS §30.4 | **§9a-10 正文仍写“不支持→显式拒绝”，与顶部更新块“必须消费传播”冲突**（supersession 未落到正文） | **文档内部不一致**（见 §6-4） |

## 3 我方有误项清单（含证据链与建议订正；公式改动归前台变更 claim）

> 以下均不改动本任务文件中的公式，只登记。

1. **插件层校准方差公式重复计 bias（高置信）**。证据：docs/plugins/algorithms_phase1/01_calibration.md:28-32 写 V(y)={V(r)+V(b)+α²[V(d)+V(b)]+y²V(f)}/f²；同 master 时 bias 被 −b 与 +αb 两处各计一次。V6 实现 v6_calibration_covariance.cpp:313-319,391 折叠 j_b=−(1−α)/f 只计一次；V6_CALIBRATION_COVARIANCE.md:16 明说 V(b)+α²V(b) 仅对应“光/暗路 bias 为不同 master_id”。建议：一般式改为 V(y)=[V(r)+(1−α)²V(b)+α²V(d)+y²V(f)]/f²，并附不同 master 分支。影响面：插件文档（不在本任务文件域），由前台按变更 claim 处理。
2. **trimmed-mean→σ 常数错误（高置信）**。证据：dpsf_psf.cpp:222-254 对 |r| 排序取 [0.1m,0.9m) 均值；精确值 0.7316730952806139（closed-form 与 scipy.quad 一致到 1e-13；12-seed MC 0.731506±0.000158 相容）。文档 PSF.md:23/81/119 与 noise_model.cpp:37、snr_science.cpp:39 用 0.7316727929211932，相对差 +4.13e-7。建议统一改全精度值（科学影响极小，但现被当作精确常数）。影响面：PSF.md、NOISE_MODEL.md、实现常数（实现改动归前台）。
3. **Moffat4 FWHM 因子与 s_factor 舍入（高置信）**。1.230310 vs 精确 1.2303076525901；3.7172 vs 精确 √(2ln1000)=3.716922。文档称“恒为 1.230310”把舍入当不变量。建议标“≈”或改全精度。
4. **孔径测光 flux_error 漏项（高置信）**。证据：photometer.cpp:94 用 sqrt(max(sum,0)+n_in·σ_sky²)；标准式 σ_F²=F/gain+n_in·σ_sky²(1+n_in/n_sky)（Howell；photutils 逐像素误差传播）。缺 gain 与背景估计项 n_in²σ_sky²/n_sky，误差系统性偏小。建议补因子或声明为下界。影响面：legacy wrapper（未接管线，DISP-PHOT-008）。
5. **SCAMP 引用错配（已在本任务订正）**。docs/references/SCIENTIFIC_REFERENCES.md 第 18 条把 SCAMP 标为 ASPC 442, 435；aspbooks.org 逐篇核验：SCAMP = Bertin 2006, ASPC 351, 112；ASPC 442, 435 = Bertin 2011 PSFEx 论文。已加勘误并保留原条目。
6. **RCR 缺一手论文引用（已补）**。REJECTION.md §14 第 4 条只登记“官方 RCR 2.4.7 软件参考”；RCR = Robust Chauvenet Rejection，Maples et al. 2018, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d）。
7. **PSF.md §4 BAD 状态不存在（中置信，DISP-PSF-003 已登记）**。PSF.md:91 写 NaN patch status=BAD，dynamic_psf.h 仅 0–3。建议改文。
8. **LM 参数文档陈旧（高置信）**。STAR_PSF_ALGORITHMS.md:73 写 iter≤50/tol=1e-6；实现 tol=1e-8/max_iter=200（dpsf_psf.cpp:374-375）。建议更新正文。
9. **生产不输出逐源 FLUXERR/Var(F)（高置信，文档-实现缺口）**。plugins/06_photometry.md:23 承诺 Var(F_hat)=1/W，但 photometry/cpp 生产树无逐源误差输出；仅 legacy wrapper 有。建议登记 DISP 或补实现（归前台）。
10. **PHOTOMETRY.md 行号锚失准（高置信，轻微）**。mag_tolerance=3.0 的实际传入点为 pc_api.cpp:139/398，文档标 star_matcher.cpp:241（该行是 psf_valid 诊断）。
11. **PHASE3_HIPS_TO_FITS §9a-10 与顶部更新块冲突（中置信）**。见 §6-4。
12. **F_syn 绝对标度表述过强（中高置信）**。F_syn=∫F_λTQλdλ 缺 1/(hc) 等归一，常数被 location 吸收；建议把“绝对通量（Gaia XP 刻度）”降级为“模型通带相对刻度，锚 Gaia XP 形状”。
13. **V6 头注释/J 向量与实现不一致（报告-only，高置信）**。v6_calibration_covariance.h:8 写 dark_opt=0 y=(r−d)/max(f,0.1)（缺 bias、缺 α），实现 :284-287 为 (r−b−α·d)/f；头 :11 的 J=[1/f,−(1−α)/f,−α/f,−y/f] 只对 dark_opt=1 成立，dark_opt=0 实为 [1/f,−1/f,−α/f,−y/f]（.cpp:320-322）；头 :50-53 枚举 kDarkIncludesBias=0 的注释与 dark_opt=0 实际语义（master_dark 已减 bias）反向。建议改名 kDarkBiasSubtracted=0/kDarkTotal=1 并分列两支 J。文件不在本任务文件域。
14. **负 median flat 拒绝与 SCI 文本冲突（OWNER-04，已登记未裁决）**。ALG:124-133、:436-449 实现为拒绝；SCI-CAL-001 §4/§5/§8 文本为“median≤0 不归一保持原样”。属真实文档-实现冲突，需负责人裁决。
15. **UNIT-001 接受路径“实际施加换算”未落盘**（ALG:524-530 自述）。消费边界声明制已冻结，但换算动作的落盘/可审计性未见闭环。需 CLI 域裁定。


## 4 无出处公式清单（建议补哪条文献）

| 公式/常数 | 所在 | 建议出处 |
|---|---|---|
| flat 逐像素 floor 0.1 | CALIBRATION.md §5:73 | 注明 Project-defined；上下文可引 LSST flatCorrection scalingType |
| 量化噪声 q²/12 | V6 头:13-15 | Bennett, W. R. 1948, Bell System Technical Journal 27, 446 |
| 背景平面 var=a+bx+cy | NOISE_MODEL.md §5:56 | 注明 Project-defined；对照 Bertin & Arnouts 1996 背景网格/多项式背景 |
| 5σ clip ≤2 轮 | NOISE_MODEL.md §5:54 | Hoaglin, Mosteller & Tukey 1983 |
| 检测阈值 median+5σ | STAR_DETECTION_ALGORITHMS.md §2:36 | Bertin & Arnouts 1996（SExtractor）；Siril star_finder.c |
| 饱和双条件 0.7/0.1 dynrange、圆度 0.5、FWHM>0.5px | STAR_DETECTION_ALGORITHMS.md §2:41-42,112 | 注明 Project-defined（R-3）；对照 Stetson 1987 饱和处理 |
| q_psf=A/residual_scale | PSF.md §2:24 | 注明 Project-defined QA 代理；对照 PixInsight PSFSW 方法学 |
| mag_tolerance=3.0 mag、match_radius=2.0 px | PHOTOMETRY.md §5:49 | 注明 Project-defined；对照 R-3 |
| 零点标准误 1.2533141373 | NOISE_MODEL.md §9a:131 | 高斯 median SE 教科书式（√(π/2)σ/√N） |
| 孔径误差简化式 | photometer.cpp:94 | Howell, Handbook of CCD Astronomy（CCD 孔径误差） |
| support=max canonical reducer | INTEGRATION.md §5:58 | 注明 Project-defined（覆盖并集保守下界） |
| percentile scale=|median| | REJECTION.md §8a:104 | 注明 Project-defined 缺陷（SC-005） |
| HP_CIRCUMRADIUS_FACTOR=1.25、1.532/1.044 因子 | DRIZZLE.md §5:65-68 | 注明 Project-defined，以 9009 例零漏选门承载 |
| C=π/2、C45=π/(2√2) 极区 Lipschitz | ASTROMETRY.md §2:27 | 注明 Project-defined 保守盘推导 |
| order_needed、leaf_order=order_sel+log2(W) | PHASE3_HIPS_TO_FITS.md §5:76-88 | 注明 Project-defined；与 DATA_SEMANTICS §3 一致 |
| k_corr=1.4、weak anchor 0.001 | PHASE2_UPM.md §5:58,68 | 注明 Project-defined；k_corr 的 MC 证据 control_median_mc_test MISSING |

## 5 参考代码库清单（项目名、许可证、仓库 URL、对照文件路径）

许可证核验方法：仓库内 LICENSE/COPYING 原文或托管 API 的 SPDX 标识（2026-09-17）。GPL/共版代码只作理解与数值行为对照，不复制进仓库。

| 项目 | 许可证 | 仓库 URL | 对照文件路径 |
|---|---|---|---|
| Astropy | BSD-3-Clause [V] | https://github.com/astropy/astropy | astropy/wcs、modeling/functional_models.py、stats |
| photutils | BSD-3-Clause [V] | https://github.com/astropy/photutils | photutils/aperture、detection、psf |
| astropy-healpix | BSD-3-Clause [V] | https://github.com/astropy/astropy-healpix | ang2pix/pix2ang NESTED |
| healpy / HEALPix | GPL-2.0 [V] | https://github.com/healpy/healpy | Górski 2005 参考实现 |
| DrizzlePac | BSD-3-Clause [V] | https://github.com/spacetelescope/drizzlepac | drizzle/astrodrizzle |
| ccdproc | BSD-3-Clause [V] | https://github.com/astropy/ccdproc | reduction_toolbox、subtract_dark、flat_correct |
| reproject | BSD-3-Clause [V] | https://github.com/astropy/reproject | WCS 重采样与方差传播 |
| SWarp | GPL-3.0 [V] | https://github.com/astromatic/swarp | resample、background、coadd |
| SExtractor | GPL-3.0 [V] | https://github.com/astromatic/sextractor | back.c、detect.c、FLUXERR |
| PSFEx | GPL-3.0 [V] | https://github.com/astromatic/psfex | PSF 采样/多项式基 |
| SCAMP | GPL-3.0 [V] | https://github.com/astromatic/scamp | 相对天体/光度联合定标 |
| LSST ip_isr | GPL-3.0 [V] | https://github.com/lsst/ip_isr | isrFunctions.py、isrTask.py |
| Siril | GPL-3.0 [V]（GitLab API license.key=gpl-3.0） | https://gitlab.com/free-astro/siril | src/algos/star_finder.c、stacking/rejection |
| GSL | GPL-3.0 [V] | https://www.gnu.org/software/gsl/ | gsl_multifit_nlinear |
| WCSLIB | LGPL-3.0 [V*] | https://www.atnf.csiro.au/people/mcalabre/WCS/ | wcs 投影实现 |
| CFITSIO | 宽松许可（NASA/HEASARC）[U] | https://heasarc.gsfc.nasa.gov/fitsio/ | FITS HDU/关键字/checksum |
| SEP | MIT（需网络核验）[U] | https://github.com/sep-developers/sep | 检测/背景/去混叠 |
| DAOPHOT / IRAF | IRAF/NOAO 许可（非 OSI）[U] | https://iraf-community.github.io/ | 拥挤场 PSF 测光（仅文献对照） |
| PixInsight PCL | 自定义 source-available（非 OSI）[U] | https://gitlab.com/pixinsight/PCL | XISF NormalizeSamples、ImageWeighting 取证 |

## 6 UNRESOLVED

1. **frame_snr 语义（两篇权威打架）**：docs/science/CONTROL_WEIGHT_SNR.md §2a 定义为“相对质量权重场（不是科学信噪比）”；docs/design/UNIFIED_MODEL.md §2 表与 docs/plugins/algorithms_phase1/07_noise_snr.md §4.1 定义为“帧级未加权原始信噪比（对标 PixInsight PSFSNR）”。两者互斥。建议负责人裁决统一口径（本任务不改公式）。
2. **UPM 模型（设计-实现冲突）**：docs/plugins/algorithms_phase2/10_sampling.md §4.2、11_upm.md §4.1 与 UNIFIED_MODEL.md §2 描述 y_k=g_k·s+b_k(x)（乘性+稀疏样条天光面）；docs/science/PHASE2_UPM.md §1/§5 的现行冻结为纯加性 8×8 control cell，并明文“乘性尺度差已撤销”。两者不可同时为真。需裁决是“目标规范 vs 现行实现”的过渡，还是文档集自相矛盾。
3. **母版方差/相关性传播缺口**：CALIBRATION.md §9 明确不传播母版方差；UNIFIED_MODEL.md §6 要求 master calibration 参数不确定度进入 variance/covariance。无项目内冻结公式，行业实现（ccdproc/ip_isr）同样不传播。需决定是否在 alpha 内建模。
4. **PHASE3 variance 输入口径内部不一致**：PHASE3_HIPS_TO_FITS.md 顶部 DATA-UNC-001 更新块要求“必须显式消费传播 variance/ivar”，但 §9a-10 与 §1 非目标正文仍写“不支持→显式拒绝”。supersession 未落到正文。
5. **Drizzle/Phase3 输出方差忽略非对角协方差**：UNCERTAINTY_AND_COVARIANCE.md 已如实登记只存对角 variance（Σc_k²u_k 为下界，ρ=0.19 ⇒ 方差低估 36.3%）。是否在 alpha 内补相关核未裁决。
6. **ASTROMETRY.md §5 SIP 系数单位与 FITS 头关系**：文档写 A[i][j]=cd_inv·trans.x_ij、单位 1/px^{i+j-1}，同时前向式 (ξ,η)=CD·[(xp−CRPIX)+SIP]。需澄清存储系数与 FITS 头 A/B（度域）之间的 CD 因子，避免第三方读者误读（astropy 对拍已通过，倾向文档表述问题）。
7. **MRS/N* 与研究包要求**：docs/research/SNR_WEIGHT_RESEARCH_PACK.md 要求研读 Starck & Murtagh 的 MRS/N* 稳健噪声；现行 NOISE_MODEL 未采用小波 MRS/N*。需裁决是否作为 alpha 选型对照保留或必须实现。
8. **OWNER-04 负 median flat 拒绝 vs SCI 文本**：ALG 实现拒绝，SCI-CAL-001 §4/§5/§8 文本为保持原样；已登记未裁决，需负责人。
9. **UNIT-001 接受路径换算动作落盘**：声明制已冻结，但“实际施加换算”的落盘/审计未闭环（ALG:524-530 自述）。
10. **SIRIL star_finder.c 溯源与 GPL 边界**：AstroCS 检测实现按行号引用 SIRIL src/algos/star_finder.c（GPL-3.0），但仓库未 vendor 该文件；若为逐段移植需评估 GPL 传染/署名，仅作算法参考则需固定版本/commit。**需网络核验 SIRIL commit**（本会话取 master，行号可能漂移）。
11. **Tukey c=4.685 的 95% 效率一手页码**、**PixInsight PSFSNR 常数 c3/c4**、**Moffat 1969 卷页（ADS）**、**Howell 孔径误差卷页**、**Gaia XP 合成通量 1/hc 归一与 CALSPEC 溯源**均标“需网络核验”。


## 7 文档改动记录（before/after 摘要）

本任务所有改动均为“新增/扩写”，未删除任何既有条款，未改动任何公式。

| 文件 | 改动 | before → after |
|---|---|---|
| docs/references/SCIENTIFIC_REFERENCES.md | 第 18 条加 SCAMP 引用勘误；新增 §I–§L（文献 21–60）与 §M（参考代码库 18 条，含许可证） | 原有 A–H/G 全部保留；新增约 40 条文献与代码库清单 |
| docs/science/CALIBRATION.md | §14 后新增 §14a 参考节 | 新增 ccdproc/ip_isr/Janesick/Bennett/Howell 等；不改 §5/§9 |
| docs/science/ASTROMETRY.md | §14 后新增 §14a | 新增 Paper I/II 节、SIP、WCSLIB/astropy、Gaia DR1–DR3；不改 §5/§11a |
| docs/science/NOISE_MODEL.md | §14 后新增 §14a | 新增 SExtractor/MRS-N*/Rousseeuw-Croux/ip_isr；不改 §5/§5a |
| docs/science/PHOTOMETRY.md | §14 后新增 §14a | 新增 Beaton-Tukey/Horne/Naylor/SExtractor/Gaia XP-CALSPEC/Akima；不改 §5 |
| docs/science/PSF.md | §14 后新增 §14a | 新增 Moffat/Stetson/PSFEx/LM/GSL；标注 trimmed-mean 常数为 Project-defined 并指向报告 §3 |
| docs/science/STAR_DETECTION.md | 文末新增参考节 | 新增 SExtractor/Siril/DAOPHOT/Young-van Vliet/LM/GSL；不改 §1–§5 |
| docs/science/DRIZZLE.md | §14 后新增 §14a | 新增 Fruchter-Hook/DrizzlePac/Górski/astropy-healpix/Van Oosterom/S-H；不改 §5 |
| docs/science/INTEGRATION.md | §14 后新增 §14a | 新增加权均值/GLS/Zackay-Ofek/Naylor；不改 §5 |
| docs/science/REJECTION.md | §14 后新增 §14a | 新增 Rosner/NIST/Maples 2018/Konz 2023/Hoaglin/Beaton-Tukey/Siril/WBPP；不改 §5 |
| docs/science/PHASE2_UPM.md | §14 后新增 §14a | 新增 SCAMP/SWarp/Padmanabhan/Huber/Tikhonov/Duchon/Wahba；登记 UPM 模型 UNRESOLVED；不改 §5 |
| docs/science/PHASE3_HIPS_TO_FITS.md | §14 后新增 §14b | 新增 IVOA HiPS/MOC/Górski/Paper I-II/astropy/CFITSIO；不改 §5/§9a |
| docs/science/UNCERTAINTY_AND_COVARIANCE.md | 文末新增参考节 | 新增线性误差传播/Higham/var(median)/Zackay-Ofek；不改公式 |
| docs/science/CONTROL_WEIGHT_SNR.md | 文末新增 §9 | 新增 Tonry/Huang/Ivezić/Horne/Naylor/PixInsight/Starck-Murtagh；登记 frame_snr UNRESOLVED |
| docs/science/PSF_SIGNAL_WEIGHT.md | 文末新增 §9 | 新增 Horne/Naylor/Zackay-Ofek/PixInsight/Fruchter-Hook；不改公式 |
| docs/science/UNIFIED_SCIENCE_MODEL.md | 文末新增 §12 | 新增 Kay/Rodgers/Aitken/Horne/Zackay-Ofek/Tonry；登记 frame_snr 与 UPM 冲突 |
| docs/science/ACR_EQUIVALENCE.md | 文末新增 §14 | 新增 IEEE 754/Goldberg/Higham/Demmel-Nguyen/OpenMP；不改公式 |
| docs/science/SCIENCE_SCOPE.md | 扩写 §参考文献 | 原有 3 条保留并扩为完整清单+代码库许可证 |
| docs/algorithms/*.md（27 篇） | 每篇文末新增“参考文献与参考代码库（含许可证）”节 | 逐主题补出处与代码库；不改任何公式/锚/阈值/容差 |

## 8 未完成/边界

- 本任务未修改任何实现代码；§3 的实现侧订正（常数、孔径误差、LM 参数）建议由前台按 ENGINEERING_SPEC §3 变更 claim 处理并补一致性回归。
- 子代理对 T3/T5–T11 的外部核验中，凡标 [S]/[U] 的卷页/许可证仍需网络核验（见各主题与 §5 标注）。
- 未跑全量构建/ctest（遵守任务边界）；本任务只做文档与文献，无编译面改动。
- 证据留存：本报告 §2/§3 的外部 URL 与核验状态即为证据链；开源代码行号标注了来源版本，引用前应回到对应不可变 commit。
