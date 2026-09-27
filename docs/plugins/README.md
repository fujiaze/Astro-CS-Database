# plugins

本目录是插件工作细节层：按阶段分组登记每个算法/基建模块的职责边界、输入输出与工作细节，共 23 篇。

## 职责边界

- 放：单个模块的工作细节（职责、输入输出、边界、与相邻模块的分工）。
- 不放：科学公式正本（在 docs/science/）；算法推导（在 docs/algorithms/）；架构与合同（在 docs/architecture/、docs/contracts/）。

## 内容

- `00_INDEX.md` —— 插件文档集索引与阅读顺序，含 23 篇模块总表。
- `algorithms_phase1/` —— normalize 相关科学模块 8 篇。
- `algorithms_phase2/` —— mosaic 相关科学模块 5 篇。
- `algorithms_phase3/` —— export 相关科学模块 3 篇。
- `infrastructure/` —— 基建模块 7 篇（aio、cli、runtime、benchmark、observability、星表客户端、浏览器）。

## 上游

上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）。
