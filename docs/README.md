# docs — ACSD 自解释文档集

> 上游：《ACSD 最高设计》的「文档权威与索引」与「文档写法」两节

本目录是 ACSD 的全部设计、科学与工程文档，是构建、测试、验收与二次开发的唯一权威参照。权威链自上而下递减：`ACSD_DESIGN.md`（最高设计）→ `science/` 与 `engineering/`（一级正本，平级互不越界）→ `detail/`（二级细节）→ 代码。冲突以更高一层为准。

- `science/`：科学公式、常数、判据与算法推导，每条结论由论文、实验或开源代码支撑；`science/algorithms/` 承载逐符号推导与实现锚定。
- `engineering/`：与科学无关的工程设计——架构、行为合同、接口、资源、构建、发行、日志与错误标准；治理规范见 `engineering/governance/DOCUMENT_GOVERNANCE.md`。
- `detail/`：由两个一级正本推理产出的模块工作细节、数据对象与接口落地；`detail/registry/` 为模块登记正本，`detail/infrastructure/` 为基建模块落地设计。
- `GLOSSARY.md` 为术语叫法与单位口径的登记面（对象定义以 canonical schema 与 `engineering/UNIFIED_OBJECTS.md` 为准）；`DOCUMENT_INDEX.yaml` 是全文档集的唯一索引地图：docs/ 下每份文档都要在其中登记，目录招牌 README 按索引自订条款不进规范索引面。

阅读方式：从 `ACSD_DESIGN.md` 起，沿各篇抬头的上游条款与最高设计各节的索引指针逐层下钻。
