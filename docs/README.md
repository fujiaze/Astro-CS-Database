# docs — ACSD 自解释文档集

本目录是 Astro Celestial Sphere Database 的全部设计、科学与工程文档，是构建、测试、验收与二次开发的唯一权威参照。

## 权威链入口

- 最高设计：`docs/ASTROCS_DESIGN.md`（项目是什么、做到什么、CLI/架构/验收）；
- 阅读方式：从最高设计起沿各篇末尾的索引指针逐层下钻，`docs/DOCUMENT_INDEX.yaml` 提供全量文档的双向索引。

## 文档分层

- **科学线**：`docs/science/` 公式与科学定义权威，`docs/science/algorithms/` 算法推导与实现锚定，佐证要求见 `docs/DOCUMENT_GOVERNANCE.md`；
- **工程线**：行为合同与机器可校验 schema 的文档化说明（`docs/contracts/`、`docs/api/`），CI 规范（`docs/ci/`），标准与检查（`docs/standards/`）；
- **模块与架构**：`docs/plugins/`（模块工作细节）、`docs/architecture/`、`docs/modules/`、`docs/design/`；
- **验证与运维**：`docs/validation/`、`docs/quality/`、`docs/operations/`、`docs/diagnostics/`、`docs/performance/`；
- **研究与追溯**：`docs/research/`（方法选型一手查证）、`docs/references/`（文献）、`docs/traceability/`（需求—文档—代码追溯）。

## 纪律

- 各目录配有中文 `README.md` 说明职责边界与内容；科学断言以 `docs/science/` 为权威；
- 文档变更走 `docs/DOCUMENT_GOVERNANCE.md` 规定的流程与对抗审查准入。
