# audit

本目录存放文档集与代码对应关系的盘点数据，以 CSV 形态记录分类、清单与风险核对结果。

## 职责边界

- 放：文档分类清单、模块—文档—代码对应盘点、风险核对表（只读数据）。
- 不放：盘点工具实现（在 eng/tools/）；追溯矩阵正本（在 docs/traceability/）；索引登记（在 docs/DOCUMENT_INDEX.yaml）。

## 内容

- `doc_classification.csv` —— 文档分类清单：路径、权威级别与状态。
- `inventory.csv` —— 模块盘点：科学/算法/架构文档、公开头、生产源、测试的对应关系。
- `risk_verification_T012.csv` —— 风险核对结果表。

## 上游

上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）。
