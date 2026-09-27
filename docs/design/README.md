# design

本目录是三个命令的目标态详细设计与核心系统设计细节层，承接最高设计并向下约束实现、测试与验收。

## 职责边界

- 放：normalize/mosaic/export 三阶段详细设计、统一观测模型、产品落盘形态、日志与错误系统设计。
- 不放：科学公式正本（在 docs/science/）；算法推导（在 docs/algorithms/）；合同条款正文（在 docs/contracts/）；模块级工作细节（在 docs/plugins/）。

## 内容

- `PHASE1_DETAILED_DESIGN.md` —— normalize 阶段的目标态详细设计。
- `PHASE2_DETAILED_DESIGN.md` —— mosaic 阶段的目标态详细设计。
- `PHASE3_DETAILED_DESIGN.md` —— export 阶段的目标态详细设计。
- `UNIFIED_MODEL.md` —— 统一线性观测模型、数据对象表与三类配置的正本。
- `PRODUCT_STORAGE_FORM.md` —— 产品落盘形态设计：裸 HiPS 与归档包的定义、判据、索引分层与哈希口径。
- `LOG_AND_ERROR_SYSTEM.md` —— 日志与错误系统设计：层次架构、数据对象、落点与生命周期。

## 上游

上游：docs/ASTROCS_DESIGN.md §3（数据对象与配置）、§4（normalize）、§5（mosaic）、§6（export）、§7.3（错误传播与运行日志）、§10（I/O 与原子产品）。
