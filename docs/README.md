# docs — ACSD 自解释文档集

> 上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§0.3（文档写法）

本目录是 Astro Celestial Sphere Database 的全部设计、科学与工程文档，是构建、测试、验收与二次开发的唯一权威参照。

## 权威链入口

- 最高设计：`docs/ASTROCS_DESIGN.md`（项目是什么、做到什么、CLI/架构/验收）；
- 一级工程文档：`AGENTS.md`（机器干活手册）、`docs/engineering/`（工程规范）、`docs/ASTROCS_DESIGN.md` §13（版本与发布权）（控制包规范）、`docs/ASTROCS_DESIGN.md` §12（验证体系）（验收规范）、本目录 `docs/engineering/CI_SPEC.md`（CI 规范）；
- 阅读方式：从最高设计起沿各篇末尾的索引指针逐层下钻，`docs/DOCUMENT_INDEX.yaml` 提供全量文档的双向索引。

## 文档分层

本集按**三个一级目录**组织，每个目录一件事、职责不重叠：

- **`docs/science/` —— 科学正本（只读）**：公式与科学定义的唯一权威。
  其 `algorithms/` 子目录承载算法推导与实现锚定。佐证要求见 `docs/engineering/DOCUMENT_GOVERNANCE.md` §2。
  科学断言、容差与系数一律以本集为准；其它集引用而不复制。
- **`docs/engineering/` —— 工程正本（只读）**：行为合同、机器可校验 schema 的文档化说明、
  CI 规范与门禁定义、标准与锚合同、验证矩阵、模块与架构图、资源门与磁盘布局、排障手册。
  门禁级别与发布口径以本集为准。
- **`docs/detail/` —— 细节实施（可写）**：模块级工作细节与逐项实施说明，
  承接上两个正本但不复述其定义；只写「怎么落地」，不写「是什么」。
  排障手册入口 = `docs/detail/merged_TROUBLESHOOTING.md`。
- **追溯与登记**：`docs/traceability/`（需求—文档—代码追溯）；
  `docs/DOCUMENT_INDEX.yaml` 是全量文档的双向索引，登记规则见 `docs/DOCUMENT_INDEX.yaml`。
- **方法选型一手查证与文献**：已并入上面两个正本集（科学正本承载文献与查证结论，
  工程正本承载门禁与验收依据），不再单设研究与参考文献目录。

## 纪律

- 各目录配有中文 `README.md` 说明职责边界与内容；科学断言以 `docs/science/` 为权威；
- 文档变更走 `docs/engineering/DOCUMENT_GOVERNANCE.md` 规定的流程与对抗审查准入。
