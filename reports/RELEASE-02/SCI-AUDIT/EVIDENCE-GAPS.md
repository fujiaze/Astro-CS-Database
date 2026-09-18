# RELEASE-02 独立科学审计 — 证据缺口清单与补充证据库

- 审计员：独立科学审计 SubAgent（RELEASE-02 / SCI-AUDIT）
- 日期：2026-09-18
- 配套：reports/RELEASE-02/SCI-AUDIT/REGISTER.md、VERDICT.md

## 1 证据缺口清单（按严重度）

| 编号 | 缺口 | 影响的主张（文档:行） | 为什么缺 | 补证据的最低动作 | 严重度 |
|---|---|---|---|---|---|
| G1 | **数值 Oracle 复跑脚本整体缺失**：NOISE_MODEL §5a/§11/§14.4、UNCERTAINTY V19R3、CALIBRATION §11 的数值主张绑定 run/PROJECT-GOVERNANCE-01/{MASK-001,R-5,SAT-001,UNIT-001}/ 下的复跑脚本，checkout 中不存在（仅存 prose 报告） | 约 10 条数值主张（含 1.11% 偏差、RMSE 0.76%、T2 436.2 vs 7048.6 ADU 等） | 脚本未入库或已删 | 恢复脚本或重写独立 Oracle 并入库；否则这些主张只能标"不可独立复现" | **高** |
| G2 | **k_corr=1.4 的 MC 证据不可复跑**：control_median_mc_test 未注册（构建孤儿） | PHASE2_UPM.md:22,61-62,169,183,200 | 测试未进 CMake/ctest 面 | 注册测试并重跑；按当前 pixfrac=1.0 与真实角尺度 0.9586″/px 重新标定 k_corr | **高** |
| G3 | **AstroCS PSFSW 指数（α=2,β=1,γ=2,δ=1）与阈值无本项目标定记录** | psfsw.h:40-61；PSFSW_ALGORITHM_SPEC §13 | 实现标 PENDING_OWNER_SIGNOFF；无 L1 合成标定 | 用 L1 合成数据对标官方式[16] 与等权/ivar 基线，冻结或改指数 | **高** |
| G4 | **Moffat 1969 A&A 3,455 未逐页核验** | PSF.md:42；SCIENTIFIC_REFERENCES.md:171 | ADS 反爬（HTTP 405） | 以 ADS 导出或 A&A 印本补 bibcode+页级证据 | 中 |
| G5 | **Huang et al. 2017 ApJ 838,110（HSC 深度）疑似错引** | CONTROL_WEIGHT_SNR.md:136 | Crossref 无匹配；常见 DOI 指向他文 | 核 HSC 深度正确出处（疑 Huang 2018 PASJ 70 S6 / Hildebrandt 2017）；无把握则删 | 中 |
| G6 | **PixInsight 官方 HTML 页不可直接抓取（HTTP 406）** | 式号/常数引用 | 站点反爬 | 本审计改用 GitLab .pidoc（commit 08b8eb85，2024-06-21）核验，已满足；保留说明 | 低（已缓解） |
| G7 | **PCL 版本标签不精确**：文档写 "PCL 2.10.4 / Released 2026-06-21"，实际抓到的 master header 自述 PCL 2.10.8 / Released 2026-09-16 | NOISE_MODEL.md:203；SCIENTIFIC_REFERENCES.md:162 | 抓取版本与文档标注不一致 | 引用时固定 GitLab commit SHA + header 版本行 | 中 |
| G8 | **DSS/SEP/properimage 许可证与仓库 URL 未逐条复核**（本轮只复核了研究包文字） | SNR_WEIGHT_RESEARCH_PACK.md §4 | 时间/分片边界 | 用托管 API 取 SPDX + LICENSE 原文 | 低 |
| G9 | **"行业不传播母版方差"（ccdproc/ip_isr）为 [S] 级结论** | 支撑 U-B 降级论 | 未逐行核验源码 | 取 ccdproc reduction_toolbox 与 lsst ip_isr isrFunctions.py 文件:行 | 中 |
| G10 | **21 个悬空 contracts/schemas/*.schema.json 未本轮复扫计数** | GAP_AUDIT §7.5 | 引用清单由前台给出，本轮未重扫 | 重扫 docs/** 引用 vs contracts/schemas 实际文件 | 低 |
| G11 | **bilinear_4quad 误差界 Oracle 未独立复跑** | ALG-P3-001_KERNEL_REGISTRY §3 | 无构建；可 Python 复算 | 独立复算 (h²/8)(max|Fxx|+max|Fyy|) 与负向 mutation | 低 |
| G12 | **UPM/coverage/rejection 的若干数值门未独立复跑** | 各 ALG 文档 | 无构建 | 前台构建后跑 ctest -R；或 Python 独立 Oracle | 中 |
| G13 | **Bennett 1948 量化噪声、Tukey c=4.685、Var(median)=πσ²/2N 的"一手页码"** | V6 头；NOISE_MODEL；PHASE2_UPM | 部分为教科书/PMC 二手 | 补 DOI（Bennett 已补 10.1002/j.1538-7305.1948.tb01340.x） | 低 |

## 2 本审计新增/核验的参考文献（Crossref 已验真实存在）

核验脚本与原始输出：run/RELEASE-02/verify_refs.py、verify_refs2.py、verify_refs3.py 及对应 .log。

| 文献 | DOI / 标识 | 核验结果 | 用途 |
|---|---|---|---|
| Fruchter & Hook 2002, PASP 114, 144 | 10.1086/338393 | ✅ Drizzle: A Method for the Linear Reconstruction of Undersampled Images | drizzle 归一/方差/通量守恒 |
| Greisen & Calabretta 2002 (WCS Paper I) | 10.1051/0004-6361:20021326 | ✅ Representations of world coordinates in FITS | 像素原点 1-based/CRPIX |
| Calabretta & Greisen 2002 (Paper II) | 10.1051/0004-6361:20021327 | ✅ Representations of celestial coordinates in FITS | 投影公式 |
| Horne 1986, PASP 98, 609 | 10.1086/131801 | ✅ An optimal extraction algorithm for CCD spectroscopy | 最优提取 σ_F⁻²=ΣP²/σ² |
| Naylor 1998, MNRAS 296, 339 | 10.1046/j.1365-8711.1998.01314.x | ✅ An optimal extraction algorithm for imaging photometry | 成像最优加权 |
| Rousseeuw & Croux 1993, JASA 88, 1273 | 10.1080/01621459.1993.10476408 | ✅ Alternatives to the Median Absolute Deviation | MAD/S_n |
| Bertin & Arnouts 1996, A&AS 117, 393 | 10.1051/aas:1996164 | ✅ SExtractor | 背景网格/检测 |
| Górski et al. 2005, ApJ 622, 759 | 10.1086/427976 | ✅ HEALPix | 球面几何 |
| Tonry et al. 2012, ApJ 750, 99 | 10.1088/0004-637X/750/2/99 | ✅ Pan-STARRS1 photometric system | 5σ 深度 |
| Ivezić et al. 2019, ApJ 873, 111 | 10.3847/1538-4357/ab042c | ✅ LSST | 深度定义 |
| Maples et al. 2018, ApJS 238, 2 | 10.3847/1538-4365/aad23d | ✅ Robust Chauvenet Outlier Rejection | RCR |
| Rosner 1983, Technometrics 25, 165 | 10.1080/00401706.1983.10487848 | ✅ Generalized ESD | 排异 |
| Beaton & Tukey 1974, Technometrics 16, 147 | 10.1080/00401706.1974.10489171 | ✅ Fitting of Power Series | Tukey biweight |
| Van Oosterom & Strackee 1983, IEEE TBME 30, 125 | 10.1109/TBME.1983.325207 | ✅ Solid Angle of a Plane Triangle | 球面三角 |
| Sutherland & Hodgman 1974, CACM 17, 32 | 10.1145/360767.360802 | ✅ Reentrant polygon clipping | 球面裁剪 |
| Padmanabhan et al. 2008, ApJ 674, 1217 | 10.1086/524677 | ✅ SDSS improved photometric calibration | UPM gauge |
| Huber 1964, Ann. Math. Statist. 35, 73 | 10.1214/aoms/1177703732 | ✅ Robust Estimation of a Location Parameter | IRLS |
| Starck & Murtagh 1998, PASP 110, 193 | 10.1086/316124 | ✅ Automatic Noise Estimation from the Multiresolution Support | MRS |
| Fernique et al. 2015, A&A 578, A114 | 10.1051/0004-6361/201526075 | ✅ Hierarchical progressive surveys (HiPS) | HiPS 规范 |
| Stetson 1987, PASP 99, 191 | 10.1086/131977 | ✅ DAOPHOT | 拥挤场测光 |
| Newberry 1991, PASP 103, 122 | 10.1086/132801 | ✅ SNR for sky-subtracted CCD data | 噪声模型 |
| Bennett 1948, BSTJ 27, 446 | 10.1002/j.1538-7305.1948.tb01340.x | ✅ Spectra of Quantized Signals | 量化噪声 q²/12 |
| Gruen et al. 2014, PASP 126, 158 | 10.1086/675080 | ✅ Robust Image Artifact Removal in SWarp | 排异 |
| Holland & Welsch 1977, Comm. Stat. A6, 813 | 10.1080/03610927708827533 | ✅ Robust regression using IRLS | IRLS |
| Zackay & Ofek 2017 I, ApJ 836, 187 | 10.3847/1538-4357/836/2/187 | ✅ How to COAAD Images. I | point_information |
| Zackay & Ofek 2017 II, ApJ 836, 188 | 10.3847/1538-4357/836/2/188 | ✅ How to COAAD Images. II | proper coadd |
| ZOGY 2016, ApJ 830, 27 | 10.3847/0004-637X/830/1/27 | ✅ Proper Image Subtraction | 消歧（非 COAAD I） |
| PixInsight ImageWeighting 方法学 | GitLab pixinsight/Reference-Documentation @ 08b8eb85ae17611629d21ff52014842077b01a1b (2024-06-21) | ✅ .pidoc 01/02/03/04 抓取成功，逐式核验 | PSFSNR/PSFSW/标准 SNR/常数 |
| PCL PSFSignalEstimator.h | GitLab pixinsight/PCL master（header 自述 PCL 2.10.8, Released 2026-09-16） | ✅ 5.326e-6/1.316e-7 逐行核验 | 常数版本漂移 |

**待补/疑似错引**：Huang 2017 ApJ 838,110（G5）；Moffat 1969（G4）；Starck/Donoho/Candès 2003 A&A 398,785 的 DOI 应为 10.1051/0004-6361:20021571（P1-A 发现，NOISE_MODEL.md:184 现用 10.1051/0004-6361:20021569 解析为他文）。

## 3 本审计新增/核验的开源代码库（只读对照，未复制）

| 项目 | 许可证 | 版本/入口 | 对照点 |
|---|---|---|---|
| PixInsight Reference-Documentation | PixInsight 自定义（文档公开） | GitLab commit 08b8eb85 | PSFSNR/PSFSW/常数/式号 |
| PCL | PixInsight 自定义 source-available（非 OSI） | GitLab master（2.10.8） | 常数版本漂移、NStar_Sn |
| Fruchter & Hook 2002 | 论文 | arXiv:astro-ph/9808087 | drizzle 式(3)(7) |
| WCS Paper I | 论文 | arXiv:astro-ph/0207407 | 式(1) 像素原点 |
| astropy 7.0.1 | BSD-3-Clause | 本机安装 | origin 语义、WCS 往返 |
| numpy 2.2.4 / scipy 1.15.3 | BSD-3-Clause | 本机安装 | 独立 FP64 复算 |
| 既有生产二进制 build/astrocs | 本仓 | HEAD 41b41e2d… 附近（--version 报 g958fa5b） | CLI 配置格式/版本 |

GPL（SWarp/Siril/SCAMP/SExtractor/DrizzlePac 等）本轮**未复制**任何代码；引用其行为时均回到文献或本仓实现。

## 4 复算脚本清单

| 脚本 | 覆盖 | 输出 |
|---|---|---|
| run/RELEASE-02/p0_recompute.py | E1 MAD→σ；E2 trimmed-mean→σ；E3 Moffat4 FWHM/flux；E4 s_factor；E5 √(π/2)；E6 天光漂移；E7 w=SNR²/F_ref²；E8 功率比不可换算 | run/RELEASE-02/p0_recompute.log |
| run/RELEASE-02/verify_refs.py | 28 条 DOI 逐条 Crossref | run/RELEASE-02/verify_refs.log |
| run/RELEASE-02/verify_refs2.py | 9 条书目式 Crossref 查询 | run/RELEASE-02/verify_refs2.log |
| run/RELEASE-02/verify_refs3.py | Padmanabhan/Huang/Moffat 定向核验 | run/RELEASE-02/verify_refs3.log |
| run/RELEASE-02/p1/p1a_recompute_constants.py（次级 P1-A） | science 核心常数 | run/RELEASE-02/p1/p1a_recompute_constants.out |

## 4a P1 分片证据缺口（次级 SubAgent 汇总）

- P1-A：G1 数值 Oracle 复跑脚本整体缺失（同 G1）；Moffat 1969 / Gaia DR3 article-level only。
- P1-B：candidate_oracle 9003 例门与 STD-F1 九宫格桥接数值不可跑（无构建）；DrizzlePac Handbook 引文未核；control_median_mc_test / kcorr_matrix_test 构建孤儿；REJECTION「domain exactly n=4」n=3 可达性未证；NIST ESD「120/120」无可定位来源；PHASE3 部分覆盖 C 语义冲突（§5 vs §9a-6 vs code）；ACR CPU/GPU 等价不可测（DORMANT，无 GPU kernel）。
- P1-C：付费文献内部方程未重推（F&H/Górski/VOS/Sutherland-Hodgman/Rousseeuw-Croux/Akima/Beaton-Tukey）；WCS 外部闭环阈值自述 UNJUSTIFIED（发布门=N）；ACR 绝对容差 1e-6/1e-12 非尺度不变（~6.5e4 ADU 时 fp32 max_abs 2.1e-2）。
- P1-D：PSFSW α/γ 推导、k_corr MC、pix-ivar rho≥1 无偏性、PHASE2_SESSION/UPM_IMPL/SAMPLER、INTEGRATION_ALGORITHMS、PHASE2_MOSAIC_WRITE、v6/phase1/** 冻结阈值、phase3 CAR/TAN 立体角未深审。

## 4b 本轮新增已解决的标准符合性证据（hips_frame）

- 争议：仓库称 IVOA REC-HIPS-1.0 §4.4.1 的 hips_frame 值域 = {icrs, galactic, ecliptic}（aio_hips_writer.cpp:1119；PHASE3_HIPS_TO_FITS.md:63）。
- 定案证据：本审计员下载官方 REC-HIPS-1.0-20170519 PDF（ivoa.net，2.92 MB），用 pypdf 6.19.0 抽取全文，逐字得：hips_frame – Format: 「equatorial」(ICRS), 「galactic」, 「ecliptic」；示例 hips_frame = equatorial。HiPS 2.0 草案（ivoa-std/HiPS master HiPS.tex:915,949）同。
- 结论：icrs 非法；仓库 WRONG（R2-L-01）。

## 5 结论

证据缺口的**规模**：高severity 3 项（G1/G2/G3，均属"证据不可复跑/无标定记录"），中severity 约 6 项，低severity 约 4 项。
这些缺口**不阻塞最高设计**（设计意图有独立一手证据支持），但**阻塞"科学定义已冻结/已标定"的宣称**——在补上 G1/G2/G3 之前，相关条款应标注"证据待补"而非"已过"。
