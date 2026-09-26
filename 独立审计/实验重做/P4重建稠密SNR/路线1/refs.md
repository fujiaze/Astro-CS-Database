# P4 重建稠密 SNR · 文献核验记录（路线 1）

> 核验人：独立审计实验重做 P4/路线1。核验手段：arXiv API（arxiv_search）、Crossref REST API（api.crossref.org/works/{DOI}，逐条 HTTP 解析）、arXiv abs 页抓取。
> 核验日期：2026-09-26。凡未能在一手来源解析到的记 UNRESOLVED，不编造。

## 核验方法

- DOI 核验：GET https://api.crossref.org/works/{DOI}，HTTP 200 且题名/作者/年份/卷页与主张一致 ⇒ 锚定成立。
- arXiv 核验：arXiv API 检索 + abs 页全文抓取，比对题名/作者/摘要关键主张。
- 仓库锚核验：对被引文件以 grep/sed/JSON 解析核对实义内容（线号锚按行打印核对）。

## L1 Shepard 1968（IDW 原始文献；审查条目 P-CST-24 文献腿）

| 项 | 核验结果 |
|---|---|
| 主张（旧稿） | 旧稿 report.md 写作 "Shepard 1968, SIAM J Numer Anal 5, 372, DOI 10.1137/0705029" |
| 实测 | **旧稿锚错误**。Crossref 解析 DOI 10.1145/800186.810616 ⇒ Donald Shepard, "A two-dimensional interpolation function for irregularly-spaced data", *Proceedings of the 1968 23rd ACM National Conference*, pp. 517-524, ACM Press, 1968。SIAM J. Numer. Anal. 5, 372 与该 DOI 均与 Shepard 1968 无关 |
| 状态 | 锚定成立（正确出处为 ACM 会议文集）；旧稿引用作废 |

## L2 Zackay & Ofek 2017 I/II（最优叠加权；审查条目 P-ALG-10 / 定权幂次文献腿）

| 项 | 核验结果 |
|---|---|
| arXiv | I: arXiv:1512.06872（2015-12-21 提交），"How to coadd images? I. Optimal source detection and photometry using ensembles of images"，作者 Barak Zackay, Eran O. Ofek；II: arXiv:1512.06879，"How to coadd images? II. A coaddition image that is optimal for any purpose in the background dominated noise limit"，同作者 |
| 发表版 | I: ApJ 836, 187 (2017), DOI 10.3847/1538-4357/836/2/187；II: ApJ 836, 188 (2017), DOI 10.3847/1538-4357/836/2/188（Crossref 逐条解析） |
| 关键主张比对 | I 摘要逐字："the best way to combine images is to apply a matched filter to each image using its own PSF and only then to sum the images with the appropriate weights...that maximize the signal-to-noise ratio"；II 摘要逐字："optimal for any hypothesis testing and measurement...in the background-noise-dominated case"——与《已确立》1.4 对 Z&O II 标题的引用（"Optimal for Any Purpose in the Background-dominated Noise Limit"）逐字相符 |
| 适用域注意 | II 的最优性声明明确限定于**背景噪声受限情形**；ACSD 的加权方差面含源项（S_src/g），其"最优"声明不依赖该限定（逆方差最优性由 Cauchy-Schwarz 一般成立，见 report I1 理论腿） |
| 状态 | 锚定成立 |

## L3 Horne 1986（最优提取；逐源 σ_F 定义侧文献腿）

| 项 | 核验结果 |
|---|---|
| 主张 | Horne, K. 1986, "An optimal extraction algorithm for CCD spectroscopy", PASP 98, 609 |
| 实测 | Crossref 解析 DOI 10.1086/131801 ⇒ 题名/作者（Horne）/PASP 98, 609/1986-06 全部一致。**注意**：DOI 10.1086/131901 是 Vrba et al. 1986（PASP 98, 1108），易混 |
| 状态 | 锚定成立（DOI = 10.1086/131801） |

## L4 Naylor 1998（成像测光最优提取）

| 项 | 核验结果 |
|---|---|
| 实测 | Crossref 解析 DOI 10.1046/j.1365-8711.1998.01314.x ⇒ Naylor, "An optimal extraction algorithm for imaging photometry", MNRAS 296, 339-346, 1998-05 |
| 状态 | 锚定成立 |

## L5 Franke 1982（散乱数据插值算子横向比较；重建算子选型文献腿）

| 项 | 核验结果 |
|---|---|
| 实测 | Crossref 解析 DOI 10.2307/2007474（亦 10.1090/s0025-5718-1982-0637296-4）⇒ Franke, R., "Scattered Data Interpolation: Tests of Some Method", *Mathematics of Computation* 38, 181-200, 1982。该文对包括样条类与反距离权类在内的散乱数据插值方法做了系统数值比较 |
| 状态 | 锚定成立 |

## L6 Lu & Wong 2008（IDW 改进与幂次敏感性；P-CST-24 idw_power 文献腿）

| 项 | 核验结果 |
|---|---|
| 实测 | Crossref 解析 DOI 10.1016/j.cageo.2007.07.010 ⇒ Lu, G. Y. & Wong, D. W., "An adaptive inverse-distance weighting spatial interpolation technique", *Computers & Geosciences* 34, 1044-1055, 2008-09 |
| 状态 | 锚定成立 |

## L7 de Boor 2001（自然三次样条理论性质：节点精确复现、C2 连续）

| 项 | 核验结果 |
|---|---|
| 主张 | de Boor, C. 2001, *A Practical Guide to Splines*, Revised ed., Springer |
| 实测 | 书籍级标准文献，无 DOI 可在线逐字核验；其节点插值性质（样条在节点处精确复现数据值）与 C2 连续性为教科书标准结论，本路以 EXP-P4-02 实验腿独立复现（节点复现残差 <=3.6e-15，见 results/exp_p4_02） |
| 状态 | UNRESOLVED（教科书未在线逐字核验）；性质由实验独立证实，不依赖该引用成立 |

## L8 仓库内锚核验（幻觉锚修复，审查条目 I7）

### L8.1 eng/contracts/data/v6_clause_registry_v1.json:2015-2024

- 审查-3 断言该线号锚为幻觉（"registry 使用线号而非字节范围索引"）。
- 实测：文件共 4164 行；sed -n '2015,2024p' 命中条款 FZ-AP1-GLS-QW-RTOL（"Q/W==GLS 与 Var=1/W 相对容差（沿用）"，value 1e-9，gate FZ-FORMULA-WINFO）——**内容实存且与权重换算容差主题一致**。
- 判定：**非幻觉锚**；但"JSON 文件用行号作锚"的工程批评成立（行号随重排漂移，应改用 JSON Pointer）。

### L8.2 PSF_SIGNAL_WEIGHT.md 的 24/24 定位符（P-CST-21 权重幂次四指数）

- 《已确立》3 记"registry 声明钉在 PSF_SIGNAL_WEIGHT.md，但 24/24 定位符在该文件不存在"。
- 实测（脚本化逐条比对 source_binding.locator 与文件全文）：registry 中指向 docs/science/PSF_SIGNAL_WEIGHT.md 的 source_binding 共 **26 条，26 条 locator 全部 miss**（如 "alpha=2, beta=1, gamma=2, delta=1, C_norm=1.0"；而文件实际写法是希腊字母形式 α=2, β=1, γ=2, δ=1, C_norm=1.0，见 PSF_SIGNAL_WEIGHT.md:55）。
- 判定：**定位符整体失效（字符形式断裂）**，但内容实存——是"断锚/形式错配"，不是凭空捏造。修复方向：registry locator 改为希腊字母形式或改钉实现行（psfsw.cpp:312-314 / psfsw.h:57-61）。

### L8.3 《已确立》3 常数节存在性（审查-1 的"《已确立》3 行 X 全部无效"）

- 审查-1 断言"《已确立》没有独立的'3 常数节'"。
- 实测：独立审计/08_修复包/②跨帧绝对信噪比/02_已确立的算法与验证程序.md 行 108 起**确有** "## 3. 有锚常数（照抄即可…）"，含 P4 相关行（权重幂次四指数、registry:2015-2024、9216 预算、双计偏差等）。
- 判定：审查-1 该结论**与事实不符**；但"3 行 X"式引用含糊（3 内是表格行不是行号）这一批评部分成立。

### L8.4 Δ = tile_width/8 = 64（P-CST-16 审查疑点）

- 审查对 "Δ=64" 的公式链存疑。实测：eng/packaging/config/defaults.json:652-657 冻结 hips.tile_width = 512（锚 docs/science/PHASE3_HIPS_TO_FITS.md，W=512=2^9）；Δ = 512/8 = 64 算术成立；sparse_snr_layer.schema.json 的 control_point_geometry 恒 node_placement = cell_center_v1；docs/plugins/algorithms_phase1/07_noise_snr.md:176-177 同文。
- 判定：公式链实存、自洽；Δ=64 为结构性常数（豁免理由见 report I4）。

## UNRESOLVED 清单

- L7（de Boor 2001 教科书）未做在线逐字核验——书籍无公开 DOI 全文；以实验独立证实其被引用的性质。
- Z&O I/II 的"权重正比 1/sigma^2"具体公式号未逐页核对（PDF 正文公式编号），本路仅逐字核验到摘要级主张；逆方差最优性不依赖该引用（一般统计结论，report I1 理论腿独立推导）。
