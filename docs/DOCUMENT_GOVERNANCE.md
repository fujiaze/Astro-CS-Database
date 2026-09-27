# 文档体系治理规范（DOCUMENT GOVERNANCE）· v0.1 草案

> 上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§0.3（权威链）；ENGINEERING_SPEC §8（文档集）。
> 地位：文档结构治理规范——分层权威、命名、上游抬头、正向设计纪律与科学佐证三支撑；与 eng/tools/doccheck/check_doc_index.py、check_doc_hygiene.py 判据同源。
> 纪律：框架优先；迁移分批原子提交，每批门禁全绿。

## 1. 三层权威模型与目录规范

```
docs/
├── ASTROCS_DESIGN.md        最高设计（从仓库根迁入；根目录仅 README/AGENTS/各 SPEC 留守）
├── DOCUMENT_INDEX.yaml      全文档机器索引（唯一索引地图，扩 tier 字段）
├── GLOSSARY.md              术语表（全局）
├── science/                 ★ 科学线一级文档（公式与推导权威；只读权威，改动走变更流程）
│   ├── *.md                 共享科学文档（现 docs/science/ 17 件平移）
│   ├── innovations/         五创新点 P1–P5（现散在 science/algorithms 的核心推导归队）
│   └── algorithms/          其他算法的科学推导（校准/platesolve/cosmetic/star_detection…；
│                            现 docs/science/algorithms/ 28 件整体迁入；每篇必须挂文献+开源仓佐证）
├── engineering/             ★ 工程线一级文档（CLI 合同、调度、AIO、基准、观测性…）
│   └── *.md                 一级工程文档（首批自现有合同/规范晋升，见映射表 needs-lead 项）
└── detail/                  细节设计（联系一级文档准确性与底层实现的桥梁；唯一桥梁，不另立权威）
    ├── modules/             模块工作细节（现 docs/plugins/ 24 件 + docs/modules/ 49 件）
    ├── contracts/           合同说明层（现 docs/contracts/ 15 件；eng/contracts/ 机器事实源不动）
    ├── api/                 接口细节（现 docs/api/ 6 件）
    ├── architecture/        架构细节（现 docs/architecture/ 27 件）
    ├── design/              设计细节（现 docs/design/ 6 件）
    ├── ci/                  CI 说明（现 docs/ci/ 5 件）
    ├── standards/           工程标准（现 docs/standards/ 14 件）
    ├── development/         开发者指南等（现 docs/development/ 3 件 + DEVELOPER_GUIDE.md）
    ├── operations/          运维与工具链（现 docs/operations/ + docs/diagnostics/）
    ├── performance/         性能基线（现 docs/performance/ + docs/quality/）
    ├── research/            研究包与文献调研（现 docs/research/ 6 件 + docs/references/ 2 件）
    ├── validation/          验收细节（现 docs/validation/ 7 件）
    ├── traceability/        追溯（现 docs/traceability/ + TRACEABILITY.csv）
    ├── registry/            登记类（KNOWN_LIMITATIONS.md、VERSIONING.md、TROUBLESHOOTING.md）
    └── owner/               负责人视图（现 docs/owner/ 5 件 + docs/browser/）
```

## 2. 各层准入判据

| 层 | 准入 | 退出 |
|---|---|---|
| science/ 一级 | 载有公式、常数、判据、容差、冻结定义的科学正文；每篇可溯源到实验单元佐证 | 内容降级为接线说明 → detail |
| engineering/ 一级 | 载有行为合同、命令树、调度/资源/AIO 边界的工程权威正文 | 同上 |
| detail/ | 一级文档的落地细化、模块工作细节、机器合同说明；**不得出现与一级文档冲突的口径** | 被晋升为一级（需负责人确认） |

- detail 的每篇抬头必须有「上游：〈一级文档 §条〉」区；一级文档抬头必须有「上游：ASTROCS_DESIGN.md §条」区（现行纪律延续，机器判红）。
- **科学佐证纪律（全局）**：科学线每篇的一级断言须能被 实验/ 论文支撑；非创新点算法（校准、platesolve、cosmetic 等）同样要求「一手文献 + 成熟开源仓（如 Siril/SourceExtractor/SCAMP/astrometry.net/SWarp）+ 实验/复算」三支撑，文献与仓库版本写入文档头部「佐证来源」区并进 DOCUMENT_INDEX。
- **不为代码妥协**：一级文档是权威，代码与 detail 向一级对齐；发现代码与一级冲突 → §8 查证流程（文档错则改文档并一致性回归，代码错则改代码补 Oracle）。

## 3. 锚纪律与门适配（全局约定）

- 行锚/符号锚门（ALG-LINE-ANCHORS、DOC-LINE-ANCHORS）继续全量生效；两门配置随迁移同批改造：
  - 检查器 docs/algorithms/anchors/check_doc_line_anchors.py → 迁 eng/tools/doccheck/（与 ALG 门同址）；
  - doc_glob / changed_paths / 文档内嵌锚前缀按新路径改写，**迁移批次与门适配必须同 commit**；
- 迁移引发的行号漂移用重锚脚本机械处理（保持锚语义不变，仅改路径/行号），禁止顺手改正文——内容订正只走排查台账修复单。

## 4. 迁移批次纪律

1. 每批 = 一个子树（或 ≤200 文件）git mv + 引用重写 + 索引/门/AGENTS 同步 + 门禁全绿 + 原子提交；
2. 引用重写规则表（脚本化）：docs 外引用的 docs/ 前缀路径 → 新路径；docs 内相对引用按新拓扑改写；`ASTROCS_DESIGN.md` 裸名引用 → docs 外加 `docs/` 前缀、docs 内保持裸名；markdown 相对链接逐条解析改写；
3. DOCUMENT_INDEX.yaml 与 TRACEABILITY.csv 同批重生成；--strict 校验通过才算批次完成；
4. 禁改清单：eng/contracts/ 冻结 schema、实验/ 单元、testdata/、gaia/。

## 5. 正向设计纪律（负责人 2026-09-26 裁决，全局强制）

- **文档正文只写正向设计**：陈述现行口径本身（"X 是 Y"＋出处锚），不出现裁决类表述——「订正/终裁/裁决/否决/旧口径/原值/曾被」与 D-xx、A-Px-xx 编号引用一律不得进入 docs/** 正文；
- **裁决轨迹的唯二居所**：负责人裁决笔记本（记忆系统持续留档）＋排查问题台账（独立审计域）。文档不承担裁决史功能；
- **订正 = 整段重写为现行口径**：删除订正注记、历史对照、裁决编号，代之以正向表述＋现行佐证锚；变更轨迹由笔记本与台账承载；
- **佐证来源区只挂现行依据**：文献/开源仓/实验复算，不挂裁决记录；
- **历史痕迹清零**：决策、历史版本、日期、任务号、基线 SHA 不出现在任何跟踪文档；文档头部只保留「上游：最高设计 §条」与现行佐证锚；
- 对抗审查将「正文含裁决类表述或历史痕迹」判红（面②行文 + 本条款）。

## 6. 对抗审查准入

- 每完成一篇一级文档（迁移+充实+佐证），该篇即入对抗审查域：科学性（公式/常数/判据 vs 文献与实验）、内容（与 detail/代码一致性）、表述（口径、术语、结构）三面；
- 审查发现回写排查台账（P- 编号续编），修复走阶段 B 流程；完成阶段的 detail 与代码同批进入复查域。

## 7. 与在途排查的衔接

- 排查台账 v1.0 以**现行路径**定稿（修复单可回溯）；阶段 B 派单在迁移完成后执行，工单路径按映射表重写，避免二次重锚。
