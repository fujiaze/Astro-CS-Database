# refs.md — P4 重建稠密 SNR · 路线 3 文献核验记录

**核验纪律**：只登记本路线实际打开/检索到的来源；每条写明核验方式与回包要点。
检索工具：web_search / web_fetch（DOI 解析）＋公开页面。核验日期：2026-09-26（会话内）。
**凡未能解析确认的条目一律记 UNRESOLVED，不做任何补写。**

---

## R1. Shepard 1968（IDW / 反距离加权原始文献）

- **题名**：A two-dimensional interpolation function for irregularly-spaced data
- **作者**：Donald Shepard
- **出处**：*Proceedings of the 1968 23rd ACM National Conference* (ACM '68), pp. 517–524
- **DOI**：[10.1145/800186.810616](https://doi.org/10.1145/800186.810616)
- **核验方式**：web_search；ACM DL 条目页（dl.acm.org/doi/10.1145/800186.810616）回包明确给出
  "ACM '68: Proceedings of the 1968 23rd ACM national conference. Pages 517–"；Brandeis
  ScholarWorks 条目同信息。
- **⚠ 锚订正**：旧稿 `独立审计/实验重做/P4重建稠密SNR/路线1/report.md` 引作
  "Shepard 1968, SIAM J. Numer. Anal. 5, 372, DOI 10.1137/0705029" —— **该出处与 DOI 均错误**。
  本路线以 ACM 会议录为唯一核验通过的出处。
- **适用域**：散点（不规则分布）控制点插值；幂次 p 是 Shepard 原文引入的局部性参数，原文未指定普适最优 p。

## R2. Horne 1986（PSF 加权最优提取；frame_snr 的"通量型口径"方法学锚）

- **题名**：An optimal extraction algorithm for CCD spectroscopy
- **作者**：Keith Horne
- **出处**：PASP 98, 609–617 (1986)
- **DOI**：[10.1086/131801](https://doi.org/10.1086/131801)；Bibcode: 1986PASP...98..609H
- **核验方式**：web_search；ADS 摘要页与 IOP 文章页（iopscience.iop.org/article/10.1086/131801）回包
  均给出 "Horne 1986 PASP 98 609 DOI 10.1086/131801"。
- **关键内容**：方差加权最优提取（variance-weighted optimal extraction），权重 ∝ 1/方差——
  即 w ∝ 1/σ_F² 的单帧测光版；signal 以检测到的恒星测光得到、噪声稳健估计、背景独立扣除。
  仓内 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 明示 frame_snr 采用该"通量型口径"。
- **适用域**：已知 PSF 轮廓 P 与方差面的加性噪声；对乘性残差（PRNU）需先平场改正。

## R3. Naylor 1998（成像测光最优提取；SNR 通量型口径的成像版）

- **题名**：An optimal extraction algorithm for imaging photometry
- **作者**：Tim Naylor
- **出处**：MNRAS 296, 339–346 (1998)
- **DOI**：[10.1046/j.1365-8711.1998.01333.x](https://doi.org/10.1046/j.1365-8711.1998.01333.x)
  （web_fetch doi.org 触发重定向至 academic.oup.com——DOI 存在且解析成功；出版商页正文未能直接抓取全文）
- **核验方式**：web_search + web_fetch（重定向证据）。
- **关键内容**：把 Horne 的光谱最优提取推广到成像 PSF 测光；通量型 SNR，权重来自方差面。
- **适用域**：与 R2 同。

## R4. Aitken 1935（广义最小二乘 / 逆方差加权的统计根基）

- **题名**：IV.—On Least Squares and Linear Combination of Observations
- **作者**：A. C. Aitken
- **出处**：Proceedings of the Royal Society of Edinburgh 55, 42–48
- **DOI**：**UNRESOLVED**——web_fetch doi.org/10.1017/S0080456800012684 返回 "DOI Not Found"；
  采用 INSPIRE 记录（inspirehep.net/literature/2853649：Proc.Roy.Soc.Edinburgh 55 (1936 印行) 42–48）
  与 Stata reg3 手册引用（55:42–48）双重佐证存在性。DOI 留空，以卷期页码为锚。
- **关键内容**：广义最小二乘定理（Aitken 定理）：协方差已知时，最优线性无偏估计的权重
  = 逆协方差（对独立观测即 w_i ∝ 1/σ_i²）。这是 `w = SNR²/F_ref² = 1/σ_F²` 定权式与
  "γ=2 最优"的统计学理论腿；EXP-P4-01 H1b 用合成数据复现了该定理的数值内容
  （逆方差方案方差达到解析下界 1/Σ(1/σ²)，SNR 线性方案差 3.73 倍、等权差 104.6 倍 @2 dex 动态范围）。

## R5. Keys 1981（双三次卷积插值核；natural_bicubic 算子的文献锚）

- **题名**：Cubic convolution interpolation for digital image processing
- **作者**：Robert G. Keys
- **出处**：IEEE Transactions on Acoustics, Speech, and Signal Processing ASSP-29(6), 1153–1160 (1981)
- **DOI**：[10.1109/TASSP.1981.1163711](https://doi.org/10.1109/TASSP.1981.1163711)
- **核验方式**：web_search；IEEE Xplore 文号 1163711 页面（ieeexplore.ieee.org/document/1163711，
  "Page(s): 1153-1160"）＋多来源引用一致；原文 PDF 公开副本（ncorr.com/lapi.unam.mx）已定位。
- **⚠ 锚订正**：常被引用的 DOI `10.1109/29.90969` 经 doi.org 解析 **404 Not Found**——引用须用
  TASSP.1981.1163711。
- **关键内容**：−a=−0.5 三次卷积核；一维插值函数误差 O(h³)（连续函数误差），对光滑场整体重建误差
  与三次样条同为 O(h⁴) 量级。EXP-P4-03 理论腿实测：自然三次样条内部收敛阶 −3.97/−4.18（理论 −4）。
- **适用域**：规则网格、光滑信号；对含不连续导数的信号阶数退化。

## R6. Górski et al. 2005（HEALPix；P3 上球算子/P4 球面域的上游几何）

- **题名**：HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere
- **作者**：K. M. Górski, E. Hivon, A. J. Banday, B. D. Wandelt, F. K. Hansen, M. Reinecke, M. Bartelmann
- **出处**：ApJ 622, 759–771 (2005)
- **DOI**：[10.1086/427976](https://doi.org/10.1086/427976)；arXiv: [astro-ph/0409513](https://arxiv.org/abs/astro-ph/0409513)
- **核验方式**：arxiv_search + web_search（IOP 文章页回包 "K. M. Górski et al 2005 ApJ 622 759 DOI 10.1086/427976"）。
- **与 P4 的关系**：P4 的稠密 SNR 场在 P3 已映射的球面域上定义；本路线实验在平面域做（算子性质与
  插值阶数是局部量，与球面参数化无关——见 report §诚实边界）。

## R7. Moffat 1969（PSF 解析轮廓；S_src 源项分子的上游）

- **题名**：A theoretical investigation of focal stellar images in the photographic emulsion and application to photographic photometry
- **作者**：A. F. J. Moffat
- **出处**：A&A 3, 455 (1969)；Bibcode: 1969A&A.....3..455M（无 DOI，以 ADS bibcode 锚定）
- **核验方式**：web_search（ADS 摘要页回包）。
- **与 P4 的关系**：Moffat4 是本仓 PSF 模型族；稠密 SNR 的分子 S_src = Σ F_i P_i 由 PSF 轮廓加权
  得到（CONTROL_WEIGHT_SNR §2b"重建量必须带亮度"）。

## R8. de Boor, *A Practical Guide to Splines*（自然三次样条理论腿）

- **作者**：Carl de Boor；Springer, Applied Mathematical Sciences 27
- **性质**：标准教材引用（自然边界条件 S″=0、三对角系数方程、分段求值基）。
- **UNRESOLVED 项**：未在线核验具体版次页码；本路线仅以其为样条实现的理论参考，
  不以其页码作数值锚。EXP-P4-03 的样条实现按标准分段基独立写出并经节点复现/收敛阶双重数值验证
  （节点复现 1.8e-15、内部阶 −3.97），不依赖该书的具体公式编号。

## R9. 鲁棒统计（mesh 中值滤波的文献腿；仅定性引用）

- Huber, P. J. (1981). *Robust Statistics*. Wiley.（中值的高崩溃点性质）
- Rousseeuw, P. J. & Croux, C. (1993). Alternatives to the Median Absolute Deviation, 
  J. Statist. Plann. Inference 37, 71–85.（本仓 1.4826·MAD 尺度常数的同源文献，NOISE_MODEL §5 已用）
- **核验方式**：未单独打开原文（付费墙）；作为定性支撑引用，不作为数值锚。EXP-P4-05 的钳制必要性
  实验不依赖这些文献的任何数值。

## R10. Tonry et al. 2012（5σ 点源深度 m_5 口径的帧级科学基准）

- **题名**：The Pan-STARRS1 Photometric System
- **出处**：ApJ 750, 99 (2012)；DOI：[10.1088/0004-637X/750/2/99](https://doi.org/10.1088/0004-637X/750/2/99)
- **核验方式**：web_search（多来源引用一致："2012, ApJ, 750, 99"）；DOI 触发重定向至 iopscience（存在）。
- **与 P4 的关系**：帧级科学基准 `m_5 = ZP − 2.5·log10(5·σ_F(ref))`（CONTROL_WEIGHT_SNR §2a）；
  P4 重建的 σ_F 场是该基准的空间化上游。

## UNRESOLVED 汇总

| # | 项 | 状态 |
|---|---|---|
| U1 | Aitken 1935 的 DOI | 未能解析确认（404）；以卷期页码 + INSPIRE 记录为锚 |
| U2 | Moffat 1969 的 DOI | A&A 1969 老卷无 DOI；以 ADS bibcode 1969A&A.....3..455M 为锚 |
| U3 | de Boor 教材版次页码 | 未在线核验；不作数值锚 |
| U4 | Huber 1981 / Rousseeuw & Croux 1993 原文 | 付费墙未打开；仅定性引用 |
| U5 | `审查-05-②-科学性-2.md` | 仓库内不存在（②路科学性仅 -1/-3 两件；-2 仅存在于 ①④⑤路）。本路线以 -1、-3 为输入 |
