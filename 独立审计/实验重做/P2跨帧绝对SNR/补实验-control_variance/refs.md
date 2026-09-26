# refs.md — 佐证文献与来源核验（P2 补实验：control_variance 有限 N 语义）

核验分级：**A** = 一手正文/逐字核验；**B** = 二手独立佐证（独立于本仓的公开来源命中要点）；**P** = 存在性书目核验（未读正文）；**U** = UNRESOLVED。

| # | 来源 | 级别 | 核验记录 | 在本实验中的用途 |
|---|---|---|---|---|
| R1 | Serfling, R. J. 1980, *Approximation Theorems of Mathematical Statistics*, Wiley（DOI 10.1002/9780470316481） | P（书目）；小节号 **U** | Wiley 官方页 [onlinelibrary.wiley.com/doi/book/10.1002/9780470316481](https://onlinelibrary.wiley.com/doi/book/10.1002/9780470316481) 命中书名/年份/DOI/版权页（本会话 web_fetch 命中）。**小节号存疑**：R3 的二手来源引 "Serfling (1980, Section 2.3.3)"，而仓库 `docs/algorithms/PHASE2_SAMPLER.md:260` 引 "§2.3.2"；原书目录未读到，两说未定 | 样本分位数渐近正态 `Var(median)=1/(4Nf(m)²)` 的教科书定位（公式本体，与文档一致） |
| R2 | Cramér, H. 1946, *Mathematical Methods of Statistics*, Princeton UP, §28.5 "The quantiles" | B（章号与命题）；逐字引文 **U** | 独立二手命中：Pinelis, *ALEA* 19（[alea.impa.br/articles/v19/19-13.pdf](https://alea.impa.br/articles/v19/19-13.pdf)）逐字引 "Cramér (1946, Chapter 28.5)" 陈述样本分位数渐近正态（前提 = 密度在 x_p 邻域连续且 f(x_p)>0），与 R3 同引。仓库 `docs/contracts/DATA_SEMANTICS.md:1764` 的英文逐字引文（"asymptotically normal (m, σ√(π/(2n)))"）未在原书正文核到 | 高斯特例 `πσ²/(2N)` 的前提链（i.i.d. + 密度条件 + 大样本渐近） |
| R3 | Pinelis, I. *Second Order Expansions for Sample Median with Random Sample Size*, ALEA Lat. Am. J. Probab. Math. Stat. 19（[alea.impa.br/articles/v19/19-13.pdf](https://alea.impa.br/articles/v19/19-13.pdf)） | B | 全文 PDF 已取，正文同时引 Cramér (1946, Ch. 28.5)、Serfling (1980, §2.3.3)、Reiss (1989, Thm 4.1.4) | R1/R2 的独立交叉锚；确认渐近式的数学前提表述 |
| R4 | Akinshin, A. 2022, *Finite-sample Rousseeuw-Croux scale estimators*, arXiv:2209.12268（[abs](https://arxiv.org/abs/2209.12268)） | A（摘要级） | arXiv abs 页已取：明确 "the original work provides only rough approximations of the **finite-sample bias-correction factors**"，并给出 n≤100 的小样本偏置修正因子与效率 | 支撑本实验 B2 臂发现：`1.4826·MAD` 在小 N 有系统性向下偏（c_mad2(5)=0.906），属文献已登记现象 |
| R5 | Akinshin, A. 2022, *Finite-sample bias-correction factors for the MAD based on the Harrell-Davis quantile estimator*, arXiv:2207.12005（[abs](https://arxiv.org/abs/2207.12005)） | A（摘要级） | arXiv 检索命中：MAD 有限样本偏置修正因子主题 | 同 R4 |
| R6 | Cadwell, J. H. 1952, "The distribution of quantiles of small samples", *Biometrika* | P | 存在性经两条独立二手命中：David 1956（[rss.onlinelibrary.wiley.com](https://rss.onlinelibrary.wiley.com/doi/10.1111/j.2517-6161.1956.tb00205.x) 参考文献表）与 JSTOR 检索页引用；正文未读（JSTOR 反爬拦截） | 中位数有限 N 分布的经典文献定位（本实验用精确序统计量积分自证，不依赖其数值） |
| R7 | 仓内只读证据：`run/SCI-FIX-PHASE2-01/evidence/e1e2_domain.json`（"8.5%" 的原始 MC：ratio=0.9149310947 @N=5，seed 20260101，R=4×10⁵） | 仓内 | 已读，比值约定 = 实测 Var(median) / (πσ²/(2N)) | 判定文档方向词反转的原始测量出处 |
| R8 | 仓内只读证据：`独立审计/实验重做/P5加性天光去除/路线2/results/e1_control_variance_median.json` 与 `code/e1_control_variance_median.py`（seed 20250926） | 仓内 | 已读：κ(5)=1.4373（R=4×10⁵）、端到端 0.990/纯公式 1.096（该腿 R=2×10⁵ 级） | P5 路线2 E1 交叉比对（任务项 4） |
| R9 | 仓内实现（只读）：`lib/algorithms/coverage/src/sampler.cpp:199-212`（median_of 偶数取两中央序统计量均值）、`:837-878`（两轮 MAD + 亮端 3σ 迭代裁剪 + n_retained + cvar 发布式）、`include/astro/phase2/stage2_common.h:41-46`（min_samples=5 / clip_sigma=3.0 / clip_iters=3） | 仓内 | 已读 | 生产链忠实臂的逐句依据；偶 N 结论对生产的适用性 |

## UNRESOLVED

1. **R2 逐字引文**：`DATA_SEMANTICS.md:1764` 所引 Cramér 英文原句（"asymptotically normal (m, σ√(π/(2n)))"）本轮未核到原书正文；章号（Ch. 28.5）与命题内容已经 R3 独立证实。引文不构成结论依赖。
2. **R1 小节号**：Serfling 样本分位数小节是 §2.3.2（仓库文档所引）还是 §2.3.3（R3 二手所引）——原书目录未读到，**不定**；建议文档侧按原书复核后统一。
3. **Cadwell 1952 数值表**：未读正文，本实验的有限 N 常数由本单元自己的序统计量积分与 MC 自证，不引用其数值。
4. **被估量语义上呈点**：生产链臂表明裁剪触发时被估量 y（裁剪后中位数，奇偶翻转）的方差显著高于无裁剪中位数；`docs/algorithms/PHASE2_SAMPLER.md §5.4` 未写明 control_variance 的目标被估量是 y 还是全样本中位数。本报告按 y 口径报生产链偏差并显式标注，最终口径归属上呈负责人。
