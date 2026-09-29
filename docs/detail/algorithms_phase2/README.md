# algorithms_phase2

> 上游：docs/ASTROCS_DESIGN.md §5.2（固定科学流程）、§5.3（SNR 重建与逆方差叠加）、§5.4（天光平面与统一相对模型）、§5.5（逐像素排异）。

本目录存放 mosaic 阶段 5 个科学模块的插件工作细节：从重叠覆盖到集成出马赛克的固定科学流程各节点。

## 职责边界

- 放：coverage、采样、UPM、排异、集成五个模块的职责、输入输出与边界。
- 不放：科学公式正本（在 docs/science/）；算法推导（在 docs/science/algorithms/）；阶段详细设计（在 docs/detail/PHASE2_DETAILED_DESIGN.md）。
- 说明：phase2 仅为文档分组的内部指代，代码中算法模块并联放置。
- 佐证：科学断言以 docs/science/ 为权威，佐证要求见 docs/engineering/DOCUMENT_GOVERNANCE.md §2。

## 内容

- `09_coverage.md` —— 输入帧重叠图与几何有效域，输出 coverage 产品与连通分量。
- `10_sampling.md` —— 光度控制点与天光背景采样点两类稀疏采样，并生成星点掩膜。
- `11_upm.md` —— 统一相对模型（UPM）：联合构建公共天光面与逐帧平缓梯度并对齐。
- `12_rejection.md` —— 潜在污染状态估计，输出 mask/count/reason/probability。
- `13_integration.md` —— 按科学目标把排异后帧集成为马赛克的加权复合。
