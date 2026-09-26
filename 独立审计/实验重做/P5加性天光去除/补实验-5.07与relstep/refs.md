# refs.md — 补实验佐证文献（真实核验记录）

核验方式：web_search / web_fetch 现场命中，逐条标注核验级别。未核验到位的条目如实降级，不编造。

## R1 SExtractor 背景网格（BACK_SIZE）与背景估计尺度
- SExtractor 官方文档（readthedocs，v2.x manual PDF）："The choice of the mesh size BACK_SIZE is very important"；mesh 过小则背景估计受源污染、过大则无法跟踪背景变化。
  - 命中：https://sextractor.readthedocs.io/_/downloads/en/latest/pdf/ （web_search 摘要逐字命中 "The choice of the mesh size BACK_SIZE is very importan..."）
  - 第三方论文对该参数语义的独立复述（arXiv:2212.00841）："If BACK_SIZE is too small, the background estimation is affected by ..." https://arxiv.org/pdf/2212.00841
- Bertin, E. & Arnouts, S. 1996, A&A 117, 393（书目级；本补实验未逐页取原文，其背景网格语义经上一条官方文档与第三方论文双重复核）。
- 支撑点：背景改正的分辨率必须与背景变化的空间尺度匹配——这是 P5 "节点间距由输入几何导出、残余接缝随不可表示分量尺度增长" 的领域先例。

## R2 IRLS 出处（Holland & Welsch 1977）
- Holland, P. W. & Welsch, R. E. 1977, "Robust regression using iteratively reweighted least-squares", Communications in Statistics - Theory and Methods, 6(9), 813.
  - DOI 10.1080/03610927708827533，出版元数据经 tandfonline 页面命中核验：https://www.tandfonline.com/doi/abs/10.1080/03610927708827533 （标题、卷期逐字命中）
- Huber, P. J. 1964, Ann. Math. Statist. 35, 73（书目级，沿用仓内登记，本补实验未重新逐页核验——仅作 IRLS/稳健 M 估计框架的上下文引用）。
- 支撑点：IRLS 是稳健回归的迭代重加权格式；其收敛面由容差与迭代控制构成，文献中不存在 "0.1 步长上限" 这类常数。

## R3 步长控制属数值工程（Nocedal & Wright）
- Nocedal, J. & Wright, S. J., Numerical Optimization, 2nd ed., Springer（迭代法的步长/信任域控制是数值工程成分，控制收敛速度而非解的极限）。
  - 命中：https://www.math.kent.edu/~reichel/courses/optimization/Numerical_Optimization.pdf （书名/作者/第二版逐字命中）；镜像 https://convexoptimization.com/TOOLS/nocedal.pdf
- 支撑点：实验 B Part3 的反事实扫描（步长上限 κ 只增加迭代数、终解与判据值在容差内不变）与该教科书定位一致——若 rel_step_max 被读成 IRLS 步长上限，它属于数值工程参数（豁免），不承载科学语义。

## R4 样条表示与逼近误差（de Boor）
- de Boor, C., A Practical Guide to Splines, Springer, 1978（Rev. ed.；分段线性/张量积样条对光滑函数的逼近误差随节点间距收敛）。
  - 命中：https://www.stat.cmu.edu/~brian/valerie/617-2022/week07/spline%20references/pdfcookie.com_a-practical-guide-to-splines.pdf （书名/作者逐字命中）
- 支撑点：公共面（双线性 control 基）只能表示尺度 ≳ 节点间距量级的分量；更细尺度进入残差——P5 "无接缝 ⟺ 公共面可表示" 边界的理论背景。

## 未命中 / UNRESOLVED
- 无。本补实验两条结论均不依赖未核验文献；×5.07 与 rel_step_max=0.1 本身在一切外部文献中无出处（web 核验未发现任何外部来源声称这两个数）。
