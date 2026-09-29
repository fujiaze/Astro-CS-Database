# 文档体系治理规范（DOCUMENT GOVERNANCE）

> 上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§0.3（文档写法）；ENGINEERING_SPEC.md §8（文档集与索引规则）。
> 地位：文档体系的分层模型、准入判据、上游抬头、正向书写与科学佐证条款的唯一正本；索引与登记规则的正本在 `ENGINEERING_SPEC.md` §8，本文件只引用不复述。
> 判据同源：`eng/tools/doccheck/check_doc_index.py`、`eng/tools/doccheck/check_doc_hygiene.py`。

## 1. 分层模型与目录规范

```text
docs/
├── ASTROCS_DESIGN.md        最高设计（权威链顶点）
├── DOCUMENT_INDEX.yaml      全文档机器索引（唯一索引地图）
├── GLOSSARY.md              术语表（全局）
├── science/                 科学线：公式与推导权威（只读权威，改动走变更流程）
│   └── algorithms/          算法推导与逐符号实现锚定
├── contracts/              合同说明层（与 eng/contracts/ 的 schema 双向对应）
├── api/                    对外接口细节
├── architecture/           架构细节
├── design/                 数据对象与设计细节
├── plugins/                模块工作细节（与 lib/ 模块共址可复用）
├── modules/                模块登记与注册表说明
├── standards/              工程标准与锚合同
├── ci/                     CI 规范、检查项目录、流水线、门禁与工件
├── development/            开发流程细节
├── validation/             验收细节（含 v6 活动设计档案）
├── traceability/           需求—文档—代码追溯
├── research/ references/   方法选型一手查证与文献
├── operations/ diagnostics/ performance/ quality/  运维、诊断、性能与质量面
├── owner/ browser/ audit/  负责人视图、可视化组件说明、文档分类与审计留档
```

- 每层由上层推出；下层可补充细节，但与上层一致、不遗漏上层的实施项；
- 每个目录配中文 `README.md` 说明职责边界与内容；目录招牌件按"每目录必有 README"治理，登记于目录本身、不入规范索引段（`ENGINEERING_SPEC.md` §8）；
- 科学公式与算法推导的权威在 `docs/science/`（含 `algorithms/`）；架构权威在最高设计 §8。

## 2. 各层准入判据

| 层 | 准入 | 退出 |
|---|---|---|
| 科学线（`docs/science/`） | 载有公式、常数、判据、容差、冻结定义的科学正文；每篇可溯源到实验单元与一手资料佐证 | 内容降级为接线说明 ⇒ 移出科学线 |
| 工程线（`docs/contracts/`、`docs/api/`、`docs/design/`、`docs/plugins/`、`docs/ci/`、`docs/standards/`） | 载有行为合同、命令树、调度/资源/AIO 边界的工程权威正文 | 内容降级为流水说明 ⇒ 并入对应细节面 |
| 细节与运维（`docs/modules/`、`docs/architecture/`、`docs/development/`、`docs/validation/`、`docs/operations/`、`docs/traceability/` 等） | 上层文档的落地细化、模块工作细节、机器合同说明；与上层口径冲突的内容不成立 | 被晋升为上层（须有上层条款承载） |

- 细节面每篇抬头有「上游：〈上层文档 §条〉」区；科学线与工程线每篇抬头有「上游：docs/ASTROCS_DESIGN.md §条」区（机器判红）；
- **科学佐证纪律（全局）**：科学线每篇的一级断言须能被 `实验/` 单元与一手公开资料支撑；非创新点算法（校准、platesolve、cosmetic 等）同样要求「一手文献 + 成熟开源实现 + `实验/` 复算」三支撑，文献与项目版本写入文档头部佐证来源区并进 `docs/DOCUMENT_INDEX.yaml`；
- **上层文档是权威**：代码与细节面向上层对齐；发现冲突时按 `AGENTS.md` §8 的查证流程处置，再订正文档或改代码并补 Oracle/负例。

## 3. 锚纪律与机器门

- 行锚/符号锚门（`ALG-LINE-ANCHORS`、`DOC-LINE-ANCHORS`）全量生效；锚的路径与行号随文档维护同提交更新（锚合同的唯一正本 = `docs/detail/anchors/ANCHOR_CONTRACT.md`）；
- 重锚只改路径与行号，锚语义保持不变；正文内容订正走订正流程并复跑门禁，不借重锚顺带改内容；
- 门的 `doc_glob` / `changed_paths` 面与文档路径改动同提交生效。

## 4. 索引与引用维护纪律

1. 每条 `docs/` 路径引用与索引条目在同一批改动内闭合：`docs/DOCUMENT_INDEX.yaml` 与 `docs/TRACEABILITY.csv` 同批更新，`python3 eng/tools/doccheck/check_doc_index.py --strict` 通过才算完成；
2. 引用重写规则：`docs/` 前缀路径按新拓扑改写；markdown 相对链接逐条解析改写；裸名 `ASTROCS_DESIGN.md` 在 `docs/` 外带 `docs/` 前缀、在 `docs/` 内保持裸名；
3. 本域改动面不含 `eng/contracts/` 冻结 schema、`实验/` 单元、`testdata/`、`gaia/`；这些面的处置属各自域；
4. 索引与文件树一致：未登记即缺陷，登记不存在同样是缺陷；悬空引用跨域未修项登记于 `eng/tools/doccheck/dangling_ledger.json`，只减不增。

## 5. 正向设计纪律

- **文档正文只写现行设计**：陈述现行口径本身（"X 是 Y" + 出处锚），不出现裁决类表述与沿革叙述（订正记录、旧口径对照、决策编号引用）；
- **变更轨迹不落文档**：变更过程与判定轨迹由记忆系统与治理域承载，文档不承担沿革记录功能；
- **订正 = 整段重写为现行口径**：删除订正注记、历史对照与编号引用，代之以正向表述 + 现行佐证锚；
- **佐证来源区只挂现行依据**：文献、开源实现、实验复算；
- **历史痕迹清零**：日期、版本沿革、任务编号、基线 SHA 不出现在跟踪文档；文档头部只保留「上游：〈上层文档 §条〉」与现行佐证锚；
- 对抗审查把"正文含沿革叙述或历史痕迹"判红（`CHK-DOC-HYGIENE` 的 D1/D3 面）。

## 6. 对抗审查准入

- 每完成一篇上层文档（内容 + 佐证），该篇即入对抗审查域：科学性（公式/常数/判据 vs 文献与实验）、内容（与下层文档、代码一致性）、表述（口径、术语、结构）三面；
- 审查发现回写治理域的问题台账（编号续编），修复按订正流程执行；同一阶段的下层文档与代码同批进入复查域。
