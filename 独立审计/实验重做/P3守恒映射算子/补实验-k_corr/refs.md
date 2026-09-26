# refs.md — 文献与来源核验记录（P3 k_corr 补实验）

> 核验方式：全部一手来源实际抓取（web_fetch/scholar_fetch），引文逐字；抓不到的项标 UNRESOLVED，不编造。
> 文献核验由独立文献子代理执行并逐字复核，本文件为其记录与本实验引用面的合并登记。

## 1. Fruchter & Hook 2002（一手核验：全文）

- 正式标题（摘要页原文）：**"Drizzle: A Method for the Linear Reconstruction of Undersampled Images"**，A. S. Fruchter & R. N. Hook, PASP 114, 144 (2002), DOI 10.1086/338393。
  （注意：审计任务书中所引标题 "The Drizzle Method for Image Combination" **不是原文标题**。）
- 已核验来源：
  - 摘要页（确认 v2, 19 Oct 2001，即 PASP 发表版预印本）：[arXiv:astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087)
  - PDF 全文（scholar_fetch 提取文本 36,079 字符，全部章节在手）：[arXiv PDF](https://arxiv.org/pdf/astro-ph/9808087)
- 子串级检索证据：variance 5 处、correlat* 10 处、covarian* 0 处、"1.38" 0 处、"1.4" 0 处、"1.25" 0 处、"13/9" 0 处；唯一数值命中 **1.662**。
- **§7.1**（逐字）："Drizzle frequently divides the power from a given input pixel between several output pixels. As a result, the noise in adjacent pixels will be correlated."；"a measurement of the noise in a drizzled image on the output pixel scale underestimates the noise on larger scales"；定义 "noise correlation ratio, R"（块和极限）。
- **§7.2 式(6)（p=0 特例，逐字）**：σ²c = Σ w²_xy s⁴ σ²_xy / (Σ w_xy)²（对 dxy∈C），前提 "the noise in the individual input pixels is assumed to be independent"。
- **§7.2 式(7)（p>0 一般式，逐字）**：σ²p = Σ a²_xy w²_xy s⁴ σ²_xy / (Σ a_xy w_xy)²（对 dxy∈P），"where axy is the fractional area overlap of the drop of input data pixel dxy with the output pixel o"。
- **§7.2 式(9)(10)（逐字）**：r=p/s；r≥1: R = r/(1−1/(3r))；r≤1: R = 1/(1−r/3)；"Using the relatively typical values of p = 0.6 and s = 0.5, one finds R = 1.662."
- **对审计问题的直接回答**：
  1. 原文给出的是单输出像素方差（式 6/7），对角项求和（依据输入独立）；
  2. 原文明确讨论输出像素间相关（§7.1），给出 R 的定义与计算法（式 8）与充满 dither 闭式（式 9/10），未给逐对相关系数表；
  3. 原文不含 1.3883/1.4，不含 k_corr / variance correlation factor / covariance 字样；
  4. 式(7) 的方差估计不含输出像素间协方差项——原文用图 5 论证说明逐像素方差相加会漏掉交叉项（= 相关噪声），即**原文否定输出像素独立**；
  5. §7.2 权重归一恒等式（PDF 文本呈现）："Σ_{dxy∈C} w_xy = Σ_{dxy∈P} a²_xy w_xy"。
- 诚实边界：PASP 正式排版页未单独抓取（arXiv v2 即发表版预印本）；本单元一切小节/公式号以 arXiv v2 为准。

## 2. Górski et al. 2005（引用面：HEALPix 等面积基数）

- Górski, K. M. et al. 2005, "HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere", ApJ 622, 759, DOI 10.1086/427976。
- 引用内容：等面积基数 sqrt(4π/(12·nside²))，换算角尺度 = 211076.28514206142″/nside（仓内 DRIZZLE_GEOMETRY.md §3 已登记其为 sqrt(π/3)·(180/π)·3600 的逐位恒等式并有实验锚）。本实验输出像素尺度 = 211076.285…/512 = 412.2552″。原文未在本单元单独抓取（引用经仓内已核验文档传递），登记为**二级引用**。

## 3. 仓内正本来源（只读核验）

- lib/algorithms/drizzle/healpix_drizzle/tests/control_median_mc_test.cpp —— k_corr=1.3883 的唯一几何记录（W=H=20、300″/px :56、nside=512 :43、pixfrac=0.8 :69、NMC=2000 :91、seed 20260816+r :139、patch=首个 tile 全 touched :152-163、MAD 常数 :174、k_corr 定义 :199-202、合理门 [0.98,2.0] :217）。
- docs/science/PHASE2_UPM.md —— §2 符号表（k_corr 行）、§4（定义域 1<k_corr、标定域 [300,600]″/px、pixfrac∈[0.5,1.0]）、§5（control_variance 公式、「N=5 时渐近式低估 8.5%」注）、§14（k_corr=1.4 为项目自产 MC 证据，非外部文献）。
- docs/science/UNCERTAINTY_AND_COVARIANCE.md :48-49（"MC（pixfrac=0.8，2000 实现）k_corr=1.3883，N_eff≈181/251；冻结 1.4"）。
- docs/algorithms/PHASE2_SAMPLER.md §5.4（k_corr 定义式权威、适用域三要素、control_median_mc_test 注册状态表述）。
- docs/algorithms/DRIZZLE_GEOMETRY.md §3（HEALPIX_SCALE_PER_NSIDE_ARCSEC）、§10 DISP-DRZ-009（A_drop=pixfrac²·A_pixel 的平面极限残差 δ=1.9e-7 @300″/px —— 本实验平面模型合法性的依据）。

## 4. 本实验产生的数值断言的出处（results/）

- g1_canonical.json：16 相位 × 8 seed 正本几何复现（1.3445±0.0416，range [1.2734,1.4254]）。
- g2_decomposition.json：k_corr=1.4146 / k_shape=0.9762 / k_geom=1.4492 / nn_corr=0.1275。
- g3_nscan.json / g3b_gauss_ref.json / g3b_n5_hiprec.json / direct_char.json：N 扫描、iid 高斯参考、N=5 高精度与 400k 直接定征（Var(median,5)/渐近式=0.9113、median MAD/σ=0.7461、k_gauss(5)=1.6370）。
- g4_geometry_scan.json：ρ×pixfrac 20 档扫描（1.05–4.98）。
- g5_frames.json：1/2/4 帧 dither 变体。
- g7_fh_ratio.json：F&H 式(8) 的算子级逐像素复核（R_median=1.273 vs 闭式 1.2408）。
- tables.md：以上全部的汇总表（由 code/read_tables.py 机器生成）。

## 5. UNRESOLVED（文献面）

1. Serfling (1980), Approximation Theorems of Mathematical Statistics §2.3.2 —— 未逐页核验；PHASE2_UPM.md §5 引其支持「N=5 时渐近式低估 8.5%」，本实验直接定征（400k 实现）给出相反方向（渐近式高估 9.6%；与正态 n=5 中位数方差精确值 Var=0.2868σ² 的教科书数值一致）。该文档措辞的订正需走变更流程，不在本单元权限内。
2. PASP 2002 正式排版页（IAADS / journals.aas.org）未单独抓取——不影响结论（arXiv v2 全文在手）。
