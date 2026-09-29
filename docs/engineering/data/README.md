# data

> 上游：docs/ASTROCS_DESIGN.md §8.1（阶段间只交换磁盘产品）、§10（I/O 与原子产品）。

本目录存放数据交换接口合同：阶段间产品交换、生产工件库布局与产品 provenance 链。

## 职责边界

- 放：交换格式、工件库路径布局、provenance 字段的合同正文。
- 不放：数据语义正本（在 docs/science/DATA_SEMANTICS.md）；产品工件清单（在 docs/engineering/DATA_ARTIFACTS.md）；阶段详细设计（在 docs/design/）。

## 内容

- `DATA-002_PHASE_PRODUCT_EXCHANGE.md` —— 阶段间产品交换合同。
- `DATA-003_PRODUCTION_ARTIFACT_STORE.md` —— 生产工件库布局合同。
- `DATA-004_PRODUCT_PROVENANCE.md` —— 产品 provenance 链合同。
