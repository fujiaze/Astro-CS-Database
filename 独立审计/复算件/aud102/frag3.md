
## 执行波次（行级顺序）

动作表的执行顺序按依赖排，不按目录排。同一波次内的行互不重叠，可并行；跨波次是硬依赖。

| 波次 | 内容 | 为什么必须先做 | 涉及行数（本表） |
|---|---|---|---|
| W0 立规 | 按《索引重建规格.md》重写 `docs/DOCUMENT_INDEX.yaml` 的字段与校验门；把登记面互斥问题（同主题第二张地图）随本波定案 | 所有迁/并/删行的"前置"列都指向索引；先动文件必然制造悬空 | 1（索引行） |
| W1 补上位 | 给最高设计每一机制节补下推指针（§8.1–§8.5 当前为零）、修正 73 处"号—标题"错配、把 L1 内的公式与数值表下沉 | 标准 01 §1：下位只能由上位推出；上位无指针则下位归位后仍走不通 | §8 相关 architecture 行 33 + 上游错配行 73（跨目录，按"依据 §1"筛） |
| W2 立缺格 | 新建角色表补全格所需载体：`docs/interfaces/cli/`、`docs/interfaces/abi/`、`docs/standards/LICENSE_COMPLIANCE.md`、术语与限制面的符号表节；`artifacts/evidence/` 各子目录 | 去向待立/去向不存在的行（下表 28 行）在此之前无处可迁 | 28 |
| W3 目录归一 | `docs/api/` → `docs/interfaces/`；`docs/plugins/**` + `docs/modules/**` → `docs/modules/` 单一目录；`docs/audit/**`、`docs/validation/v6/**`、`docs/quality/**` 读数快照 → `artifacts/evidence/`；`docs/browser/`、`docs/TRACEABILITY.csv`、`docs/TROUBLESHOOTING.md` 并入同名正本 | 涉及批量改指针，必须在单文件级合并之前完成，否则指针目标又变 | 约 120（`docs/api` 6 + `docs/plugins` 24 + `docs/modules` 51 + `docs/audit` 3 + `docs/validation` 7 + `docs/quality` 2 + 杂项 4） |
| W4 主题合一 | 同主题第二份正本合并：排异、积分、重采样、SNR 权重、状态阶梯、门阈值、日志、配置数值快照、实验单元双报告 | 合并只在目录格已定之后做，避免并入一个即将再迁的落位 | 见下"待定案名单"外的全部合并行 |
| W5 写法收口 | 去元信息块、去日期与流水号、去历史叙事、补上溯指针；机械面读数：开头元信息块 31 份、缺上游指针 71 份、含日期 63 份、含流水号 113 份、含疑似 commit 值 71 份 | 纯文本层改动，放最后以免与结构迁移互相覆盖 | 全部含"§4"依据的行 |
| W6 过程层出库 | `工程控制/RELEASE-05/**` 收口出库；`memory.md` 处置；订正记录类降为证据件 | 依赖 W1–W4 把被复制的上位条款归位，否则出库即丢口径 | 31 + 3 |

## 待补举证与缺口名单

以下四类是本清单里**不可直接执行**的行，逐条写明缺什么。缺证不判、不执行、不猜。

### 一、降级为待定案的合并/删除行（12）

台账给了合并或删除，但未贴出两处实际段落对照。缺的举证 = 双方各自承载同一口径的段落原文与位置，以及"差异是否只是分层"的判定。

| 路径 | 现动作 | 举证级 | 缺什么 |
|---|---|---|---|
| `docs/science/PSF_SIGNAL_WEIGHT.md` | 合并(待定案) | E5 | 台账判"上位指定与内容不符"，但正本对（`CONTROL_WEIGHT_SNR.md`）与本文件的同口径段落未并排贴出；须补两文件同名节的逐段对照 |
| `memory.md` | 删除(待定案) | E5 | 删除动作本身成立（角色 = 过程记录），但"先改哪个登记面"未点名；缺 = 所有指向本文的跟踪引用清单与替代承载位 |
| `docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md` | 删除(待定案) | E4 | 台账贴了本文段落并给"上呈"，未点名与之重复的既有 I/O 正本文件；缺 = 与 `lib/infrastructure/aio/` 现行边界的段落对照 |
| `实验/absolute-snr/docs/EXP-06-SUMMARY.md` | 合并(待定案) | E5 | 正本候选（`EXP-06-SNR-PHYS.md`）已定，但两份的重复段落未贴；缺 = 同值数字与结语句的并排 |
| `实验/additive-sky-seamless/REPORT_paper.md` | 合并(待定案)+删除(待定案) | E4 | 合并对象点名到"单元入口件"，未点名具体段落与要删的段落 |
| `实验/healpix-polar/docs/EXP-07-POLAR-摘要.md` | 合并(待定案) | E4 | "同页两值"未贴两处原句 |
| `实验/m42-realdata/REPORT_paper.md` | 合并(待定案) | E3 | 点名正本 `README.md` 且给了逐值承载结论，未贴两处段落 |
| `工程控制/RELEASE-05/GAP_AUDIT.md`、`工程控制/RELEASE-05/TASK_LIST.md` | 保留+删除(待定案) | E2 | 删除对象是"出库后不留"，缺 = 其中被别的文档引用的条目改指表 |
| `docs/standards/TEST_STANDARD.md`、`docs/standards/checks/check_standards_registry.py`、`docs/algorithms/anchors/check_doc_line_anchors.py` | 待定案 | E0 | 台账无逐份记录（见"未登记名单"）；缺 = 整条登记 |

### 二、缺去向的文件级动作行（38）

台账给迁移/合并/删除但未点名目标路径。缺的动作 = 由执行节点按《目标文档架构》§3 落位表指定并登记进索引草稿。集中在 `docs/architecture/**`（11 行，含 5 份机器清单 csv）、`docs/algorithms/anchors/**`（3 行）、`docs/traceability/**`（2 行）、`docs/ci/04_ARTIFACTS.md`、`docs/contracts/ARCH-001.md`、`docs/validation/SCIENCE_FREEZE.md`、`实验/**`（6 行）、`工程控制/**`（3 行）、根治理文档 3 行、`docs/DEVELOPER_GUIDE.md`、`docs/TROUBLESHOOTING.md`。

### 三、去向不存在的行（17）

台账点名的目标文件不在跟踪集内，直接照做会把文档迁进黑洞。每行需先二选一：改指到实存正本，或先立目标再迁。

| 路径 | 台账所指去向 | 性质 |
|---|---|---|
| `docs/TRACEABILITY.csv` | `docs/traceability/TRACEABILITY_ROOT.csv` | 目标文件名系台账虚构；实存同名主题为 `TRACEABILITY_MATRIX.csv` |
| `docs/browser/HIPS_BROWSER.md` | `docs/modules/hips_browser.md` | 目标未立；`docs/plugins/infrastructure/23_hips_browser.md` 实存，二者需先合一 |
| `docs/diagnostics/TROUBLESHOOTING.md` | `7.3`、`AGENTS.md` 混写 | 去向被上游条款号污染，非路径 |
| `docs/modules/acr.md` | `docs/research/ACR_ISOLATED_EXPERIMENT.md` | 目标不存在 |
| `docs/development/CONFIG_SCHEMA.md` | 台账原文为"合"字截断 | 抽取失败，需回读台账原句 |
| `docs/api/MANIFEST_VERIFY_V1.md` | 同上类截断 | 需回读原句：该份与上位"verify 命令零出现"直接冲突，属上呈项 |
| `docs/modules/MODULE_MAP.yaml`、`docs/modules/registry/astrocs.phase3.properties.md`、`docs/standards/STANDARDS_REGISTRY.md`、`docs/architecture/observability/RUN_GRAPH_CONTRACT.md`、`工程控制/RELEASE-05/OPEN_QUESTIONS.md` | 各指一个未跟踪件 | 需先立或改指 |
| `实验/absolute-snr/results/REVERSE_VERIFY_CANON.md`、`实验/additive-sky-seamless/results/REVERSE_VERIFY_CANON.md`、`实验/photometric-magnitude/results/REVERSE_VERIFY_CANON.md`、`实验/absolute-snr/docs/surveys/f-instr-survey.md`、`实验/absolute-snr/code/reverse_verify/f_instr/README.md`、`实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/README.md` | `artifacts/evidence/<单元>/…`、`docs/f-instr-canon.md`、`docs/p1-spatial-gain.md` | 证据子目录待立；两份 `docs/<canon>.md` 是"上收到正式层"的意图而非现有路径，需按《目标文档架构》§4 指定真实正本 |

### 四、需负责人裁决的行（11）

角色与落位不由文档层决定，属顶层合同或科学定义歧义：`docs/architecture/ASYNC_IO_CONTRACT.md` 与 `docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md`（唯一 I/O 边界归属）、`docs/modules/acr.md` 与 `docs/science/ACR_EQUIVALENCE.md`（ACR 等价性载体）、`docs/modules/orchestrator.md`（调度器身份）、`docs/plugins/algorithms_phase2/11_upm.md`、`12_rejection.md`、`docs/plugins/infrastructure/21_observability.md`（门身份与门 ID 一名两指）、`memory.md`（删除需先改登记面）、`工程控制/RELEASE-05/tasks/DOC-501.md`、`SCI-501.md`（任务层与上位判据相反）。

## 覆盖率自报

- 逐份覆盖：360 / 360 份各有一行（含根治理 9 份、`docs/**` 265 份、`实验/**` 62 份、`工程控制/RELEASE-05/**` 31 份、`docs/` 根散件 7 份）。
- 台账记录来源分片：DA-01（27）、DA-02（30）、DB-01…DB-20 与 D1 残余批次，共 23 个分片文件；357 份有逐份登记，3 份无记录（已列入"待定案"并标 E0）。
- 加深程度：全部 360 行给出应角色（角色格子）、动作、依据条款号、前置与风险计数；195 行为文件级动作（迁/并/删/拆/沉/上呈/待定案），其中 143 行进入举证判级、126 行达 E1 双段可直接执行；143 行中有去向标注者按"实存 / 待立 / 不存在"三态标记（去向实存 105、待立 11、不存在 17）。
- 不可执行行显式列出：待定案 12、缺去向 38、去向不存在 17、待裁决 11、台账无记录 3。
- 未逐行加深的部分：`实验/**` 的结果表类行（`EXP*_TABLES.md`、`*.json` 数据件）只登记到"角色 + 动作 + 依据"，未补段落对照——其处置多为"标生成物 + 出库"，不依赖段落对照。
- 已知机械层偏差（本表不采纳其结论，只当线索）：`doc_mechanical.csv` 的 `meta_block` 对 `docs/api/CLI_PROTOCOL_V1.md` 报 False 而文内第 5 行确有元信息块（台账已记漏报）；`unregistered_index` 列在口径外文档上恒不触发。

## 我证伪了任务书或 D1 台账的哪个前提

| 前提 | 实测 | 处理 |
|---|---|---|
| 任务书："动作表从 D1 台账的 `处置建议` 列机械抽取即可" | 台账的处置列把**文件级动作**与**内容级整改**混写（如"保留 + 删除 35–39 行版本锚"、"保留（3 处改指针）"）。实测：按关键词直取有 97 行含"删除"；加一层"动作词后跟行号区间或节/字段/表/列/注释/声明/锚/编号即判内容级"的规则后仅降到 94 行（3 行改判，另有 6 行标为纯就地整改） | 词法判级不足以区分两类动作——本表不假装已解决：一并给出"举证级"列与"去向存在性"标记，把 12 行删并证据不足、38 行缺去向、17 行去向不存在显式列出，不可执行的一律不进入执行 |
| 任务书："凡台账给合并/删除却没贴两处实际段落的降级为待定案"——隐含假设"多数行已贴段" | 判级 `E-MERGE-DEL-2` 只取该对象自己登记的三字段（重复或重叠对象 / 与上位冲突 / 处置建议），不借邻行内容：143 个删并拆类对象中 E1 双段 126、E2 单段 2、E3 只点名 1、E4 未点名对方 3、E5 无举证 8，另 3 份台账无记录（E0） | 判据成立并已执行（待定案 12 行）；同时证伪一种做法：改用"全文 ±1500 字符窗口"匹配时 131 行全部判成"已贴段"，是典型的假阳性守门（fail-open），故本表固定字段内判据并公布分级定义 |
| D1 台账（DA-01 G7）建议把 17 份科学文档末尾的许可证块"改指 `docs/standards/LICENSE_COMPLIANCE.md`" | 该文件不在跟踪集，全仓零引用 | 本表把它落成 W2"立缺格"前置，而不是照抄"改指" |
| D1 台账 DB-19 用 `tasks/ACCEPT-501.md` 之类的短路径登记 10 份任务书 | 与其余分片的仓库相对路径口径不一致 | 本表统一还原为 `工程控制/RELEASE-05/tasks/…`，并把"路径口径唯一"写进《索引重建规格.md》的字段规则 |
| 机械层 `dangling_links` 列（77 份有命中、共 175 条） | 任务书已注明对反引号内 `路径:行` 失明；本表复核另一向：台账 DA-01 对最高设计 9 条命中逐条读后判"真悬空 = 0" | 本表不以该列作删除依据；悬空清零动作统一交给索引门（见《索引重建规格.md》第 4 节） |

## 工作树基线（开工与收工）

开工时 `git -C "F:/Astro dev/Astro CS Normalization Database" status --porcelain` 原文：

```
?? ACSD整治工作包_AUDIT-06.zip
?? site/
```

收工时同一命令原文：

```
?? "ACSD\346\225\264\346\262\273\345\267\245\344\275\234\345\214\205_AUDIT-06.zip"
?? site/
```

差异：两行的**对象集合相同**（仍是未跟踪的工作包 zip 与 `site/` 目录），收工输出多出的八进制转义是 `core.quotePath` 对非 ASCII 路径的显示规则，由命令行 `-c core.quotePath=false` 与否决定，不代表仓库内容变化。本阶段对被审仓库零写入：未新建、未移动、未删除任何仓库文件，未执行 `git config` 写操作，未执行构建、`cmake`、`ctest`、`run_checks.py`、任何 `eng/**` 脚本与端到端。

<!-- PROGRESS: 已完成 全部 3 份交付物中的第 2 份（动作表 360 行 + 波次 + 名单 + 自报） -->
