# docs — ACSD 自解释文档集

> 上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§0.3（文档写法）

本目录是 Astro Celestial Sphere Database 的全部设计、科学与工程文档，是构建、测试、验收与二次开发的唯一权威参照。

## 权威链入口

- 最高设计：`docs/ASTROCS_DESIGN.md`（项目是什么、做到什么、CLI/架构/验收）；
- 一级工程文档：`AGENTS.md`（机器干活手册）、`ENGINEERING_SPEC.md`（工程规范）、`CONTROL_PACK_SPEC.md`（控制包规范）、`ACCEPTANCE_SPEC.md`（验收规范）、本目录 `docs/ci/`（CI 规范）；
- 阅读方式：从最高设计起沿各篇末尾的索引指针逐层下钻，`docs/DOCUMENT_INDEX.yaml` 提供全量文档的双向索引。

## 文档分层

- **科学线**：`docs/science/` 公式与科学定义权威，`docs/science/algorithms/` 算法推导与实现锚定，佐证要求见 `docs/DOCUMENT_GOVERNANCE.md` §2；
- **工程线**：行为合同与机器可校验 schema 的文档化说明（`docs/contracts/`、`docs/api/`），CI 规范与检查入口（`docs/ci/`，流程与范围见 `docs/ci/CI_SPEC.md` §2），标准与锚合同（`docs/standards/`）；
- **模块与架构**：`docs/plugins/`（模块工作细节）、`docs/architecture/`、`docs/modules/`、`docs/design/`；
- **验证与运维**：验证矩阵、磁盘与资源门、排障手册分别在本集的对应条目下（排障手册入口 = `docs/detail/merged_TROUBLESHOOTING.md`）；
- **研究与追溯**：`docs/research/`（方法选型一手查证）、`docs/references/`（文献）、`docs/traceability/`（需求—文档—代码追溯）；
- **文档体系治理**：分层准入、上游抬头、正向书写与佐证纪律 = `docs/DOCUMENT_GOVERNANCE.md`；索引与登记规则 = `ENGINEERING_SPEC.md` §8。

## 纪律

- 各目录配有中文 `README.md` 说明职责边界与内容；科学断言以 `docs/science/` 为权威；
- 文档变更走 `docs/DOCUMENT_GOVERNANCE.md` 规定的流程与对抗审查准入。
