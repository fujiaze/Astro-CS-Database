# 变更 claim：DOC-203-PREREQ-Q2Q4Q6Q8-SYNC-001 — 四项前置裁决落地 + 12 条同批同步订正（详细层文档侧）

- 控制包：RELEASE-03 / 任务 **DOC-203**（`工程控制/RELEASE-03/tasks/DOC-203.md`）
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §0.1（只有一份权威链，禁止下级自称唯一权威）、§0.2（详细层**不得**与本设计相反）、
  §2.1（全程只有 SNR，不存在「权重模式」）、§2.2/§2.3、§3.3（三命令通用输入合同）、§3.5（预检 = 打印报错 + 详细预估）、
  §4.5（排异逐像素按 N 自动选择）、§5.3（导出只接受面亮度语义）、§6.2（唯一命令树）、§6.3（事件流默认输出 / 磁盘资源门）、
  §12（**禁止**用版本号作生效或退役条件）
- 依据（裁决正本）：`工程控制/RELEASE-03/GAP_AUDIT.md` **§4.1 Q2 / §4.2 Q4 / §4.3 Q6 / §4.4 Q8 / §4.5 C01** + §5（六项实验结论）
- 依据（订正清单）：`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.2-R03/R05/R06 + **§3.3 S01–S12**；`CTR-01.md` §6；`MOD-01.md` P10/P11/P17/P20/P21/P23
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（变更 claim + 一致性回归）、§6/§7/§8；`AGENTS.md` §1.1/§4/§8；`CONTROL_PACK_SPEC.md` §6/§7
- 状态：**已落地（文档侧）**；代码侧归属见 §6
- 影响类：**文档/术语级**（**零**科学公式、零默认容差、零 SCI/ALG 冻结定义、零数值结论改动；未改 `ci/checks.json`；未挂 waiver）
- 文件域：`docs/contracts/**`、`docs/development/**`、`docs/design/**`、`docs/api/**`（未动 `docs/architecture|interfaces|standards|modules|plugins|owner/**`、
  `docs/science/**`、`docs/algorithms/**`、`lib/**`、`contracts/**`、`config/**` 其余；**未动**排除件 `docs/design/UNIFIED_MODEL.md`、`docs/contracts/unified_object_registry.json`）

---

## 1 四项前置裁决（GAP_AUDIT §4）逐条落点

### Q2 —— V6 合同层在位保留 + 身份归一 + 删版本号退役窗口（GAP_AUDIT §4.1）

| 落点 | 落笔 | 证据（grep） |
|---|---|---|
| `docs/contracts/DATA_SEMANTICS.md` §31 标题 + 状态行 | 标题改「V6 **合同层**数据合同（…**设计档案 / 产品族专用投影，非生产目标态**）」；状态行**删**「ACTIVE（V6 目标态集成）」 | `grep -n "^## 31.\|^> 条款 ID：\`DATA-V6-SCHEMA\`" docs/contracts/DATA_SEMANTICS.md` → :2693/:2695 |
| `docs/contracts/INDEX.yaml:4` | 加 `revision_id: CHG-2026-09-20-DOC203` + §12 声明（`version:` 降级为**内部修订号**，不承载生效/退役语义） | `sed -n '1,12p' docs/contracts/INDEX.yaml` |
| `docs/contracts/UNIFIED_OBJECTS.md` §4 登记表 | **12 行** `0.11.0-alpha.2 → 0.12.0` 版本窗口 → `CHG-2026-09-16-DATA001` + 「待定变更编号（**禁止**用版本号窗口表达）」；表头列名改「废弃登记（**变更编号**）/ 退役条件（**变更编号 / 日期**）」；文首加 Q2 身份三条 | `grep -c "CHG-2026-09-16-DATA001" docs/contracts/UNIFIED_OBJECTS.md` → 12 |
| `docs/contracts/API-001.md:3`、`docs/contracts/PUBLIC_API.md:2126` | 「ACTIVE」→「**设计档案 / 产品族专用投影（非生产目标态）**」+ 变更编号 | `grep -n "设计档案 / 产品族专用投影" docs/contracts/API-001.md docs/contracts/PUBLIC_API.md` |
| v6 内 `weight_mode` 家族（A44） | 只做**措辞一致性**（DOC-201 已逐处留痕）：`UNIFIED_OBJECTS.md` / `DATA_SEMANTICS.md` §31 补「已按 §9.73 A44 作废（作废键面）」；`docs/contracts/v6/W6_SCHEMA_INTEGRATION.md:67`、`v6/data/10_migration_and_open_items.md:15` 的 `ASTROCS_WEIGHT_MODE` 去反引号（消除 CON-DOC-SYMBOLS 判红，留痕不变） | `python3 ci/check_no_weight_mode.py` rc=0 |
| **C01 一致性**（`psfsw_robust_weight` 真删 14→13） | `INDEX.yaml` 该条 → `status: OBSOLETE` + `path: ""` + 退役留痕（**CONTRACT-GRAPH 由红转绿**）；`DATA_ARTIFACTS.md`、`UNIFIED_OBJECTS.md`、`DATA_SEMANTICS.md` §31.1、`PUBLIC_API.md` §2/§3、`docs/design/PHASE1/PHASE2` 同步标退役 | `python3 tools/check_contract_graph.py` → `CONTRACT_GRAPH_PASS contracts=116` |

### Q4 —— `resource_timeseries.csv` 唯一列合同（GAP_AUDIT §4.2）

- 落点：`docs/api/CLI_PROTOCOL_V1.md` §7 新增「资源时序工件的唯一列合同」块：
  ① **唯一列合同 = 生产实现** `lib/infrastructure/cli/resource_recorder.h:260-266`（**20 列**，run 收尾一次性落盘，被 manifest / 目录树哈希覆盖）；
  ② LOG-002 的「每秒采样 + seed 行 + 指纹链」CSV 是**监控伴随器原始数据**，工件名固定 **`monitor_timeseries.csv`**；
  ③ **两工件不同名、不互替**（不得共用列定义或采样语义）。
- 证据：`grep -n "唯一列合同\|monitor_timeseries" docs/api/CLI_PROTOCOL_V1.md` → :83/:84/:89/:90。

### Q6 —— 运行事件流唯一 schema 身份（GAP_AUDIT §4.3）

- 落点：`docs/api/CLI_PROTOCOL_V1.md` §4 升级为「**JSONL 运行事件流 v1（唯一 schema）**」：唯一 schema = `lib/infrastructure/cli/protocol.h`（`ValidateEventV1`）+ `jsonl.h`（`JsonlEmitter`）；本节 = 人类可读合同；`contracts/schemas/jsonl_event_v1.schema.json` = **派生件**；**显式声明 LOG-001 不是运行事件流**，`event` 与 `kind` 不得混用；§3 声明**事件流 = 默认输出**、`--events-jsonl` 只是等价历史别名。
- 同步订正：`docs/contracts/API-001.md` §3 的 CORE-008 字段名（`run/node/…/type/metrics`）标**作废**并指向唯一 schema；§2.1 exit 10 含义与唯一源路径订正。
- 证据：`grep -n "Q6 前置裁决\|显式声明它不是运行事件流" docs/api/CLI_PROTOCOL_V1.md`。

### Q8 —— 块词表唯一登记处（GAP_AUDIT §4.4）

- 落点：`docs/contracts/DATA_SEMANTICS.md` §11.1 首注：**唯一登记处 = `lib/infrastructure/aio/include/aio_pipeline.h` 的「标准块定义表」**；§11.1 表**只作引用**，不得自称第二套；`orchestrator.cpp:3211-3213` 的 6 个名字只作跟踪子集引用。
- **如实登记**：`variance` 块**尚未**进标准表（实测标准表 12 行无 `variance`），且 `aio_pipeline.h:304` 仍保留「未列出的自定义块名也允许」一句 ⇒ 两条**均归 FIX-201**，本表不得据此声称已入表。
- 证据：`grep -n "块词表归属（DOC-203 / Q8" docs/contracts/DATA_SEMANTICS.md` → :284；`grep -n "未列出的自定义块名" lib/infrastructure/aio/include/aio_pipeline.h` → :304。

---

## 2 同批同步订正 12 条（DESIGN-DRAFT §3.3 S01–S12）逐条处置

| # | 内容 | 本次处置 | 状态 |
|---|---|---|---|
| S01 | `minmax` 路径 | 文档侧：`DATA_SEMANTICS.md` §22 首注「MINMAX 路径**已禁用**（§9.71 裁决 3.4 + EXP-204）」+ `iterations` 行与 4 处行内标记「已禁用」；schema 侧 FIX-207 已删枚举（实测） | ✅ 文档侧完成；内核分支删除**仍开放**（见 §6） |
| S02 | §3.5 交互式预检窗被当权威 | `CONFIG_CONTRACT.md:13` 改引订正后的 §3.5（**打印报错 + 详细预估，无交互式提示窗**）并撤行锚；两份 phase_config schema 的 `x-astrocs-authority` 实测只写「§3.5（运行前预检绿/橙/红）」，**无**交互窗措辞 ⇒ 已合规 | ✅ |
| S03 | `docs/architecture` 排异档位 | **非本任务域**（DOC-202）；复核：`docs/architecture/DATA_FLOW.md:36-37,44` 已引 EXP-204 定案冻结表 | ✅（DOC-202 已落） |
| S04 | 阶段一注册面 8 节点 | **非本任务域**（`lib/**`）；复核：`module_adapters.cpp:96/:3210` 仍登记 `apply_photometry`「生产零调用者」⇒ 未完成 | ⛔ 残留（见 §6） |
| S05 | aio「未列出的自定义块名也允许」 | **非本任务域**（`lib/**`）；文档侧已在 §11.1 首注如实登记该句仍在位并归属 FIX-201 | ⛔ 残留（见 §6） |
| S06 | `docs/modules/common.md:46` `precision` 来源 | **非本任务域**（docs/modules=DOC-202）；本域等价面已订正：`docs/development/CONFIG_SCHEMA.md` 范围声明 + `CONFIG_CONTRACT.md` §3 精度显式声明 | ⛔ 残留（见 §6） |
| S07 | `docs/modules/orchestrator.md` 治理规则 | **非本任务域**；复核：该页已有「归属与构建（ORCH-001 落位）/ §7.1 目标家」段 | ✅（DOC-202 已落） |
| S08 | 三处 `PENDING.md`「本目录为空」 | **非本任务域**（`lib/**`）；复核：`observability/PENDING.md` 已改为「迁移登记（INT-001）」并声明「本文件不作模块状态声明」；`pipeline/PENDING.md` 同批在工作树中 | ✅（在飞，未提交） |
| S09 | `MODULE_MAP.yaml` 34 条声明路径不存在 | **非本任务域**；复核：当前 `check_module_map` findings 中已无「声明路径不存在」类目（余 149 条为 missing_implementation/CMake/test-coverage 等既有缺口） | ✅（DOC-202 已落，余项非本类） |
| S10 | registry 样板句误植 + 缺 `module_id` | **非本任务域**；复核：26/26 页已带 `module_id`；样板句残留 **5** 处（原 24 处）⇒ 未清零 | ⚠️ 部分（见 §6） |
| S11 | `ARCHITECTURE.md:50` 217 行、`ASTROCS_DESIGN.md:686` 16,191 行 | **非本任务域**；复核：`ARCHITECTURE.md:50` 已订正为 338 行；`ASTROCS_DESIGN.md:820` **仍写 16,191 行** | ⚠️ 部分（见 §6） |
| S12 | 工程标准层 4 条悬空义务 | **非本任务域**；复核：`CODE_STANDARD.md` S12-Y1、`RELEASE_STANDARD.md` S12-Y2、`DOCUMENTATION_STANDARD.md` S12-Y3 均已带订正注 | ✅（DOC-202 已落） |

---

## 3 任务书本职 7 项

1. **`API-001.md:11` 第二套命令树（含被禁 `run`）⇒ 作废该命令面表**：`docs/contracts/API-001.md:13` 表项标「**该命令面表已作废**」+ 原文留痕 + 指向 §6.2 唯一命令树与 `docs/api/CLI_PROTOCOL_V1.md` §1。**注**：任务书写的路径 `docs/api/API-001.md` 在本仓**不存在**，实际落点 = `docs/contracts/API-001.md`（实测 `find docs -name 'API-001*'`）。
2. **`INDEX.yaml:4` 版本号 + 退役窗口 ⇒ 日期/变更编号**：见 §1-Q2。
3. **`DATA_SEMANTICS.md:296` drizzle 文件通道删 weight 面**：`weight / snr 面` 行 → 只留 `snr 面`；原 weight 面标**已删面**（A44）并如实登记它只存在于**已作废**的 legacy `hp_drizzle_run(…, weight_path, …)`（零生产调用者，GAP_AUDIT A-04）。
4. **`output_mode` 必填/默认口径与 FIX-207 fail-closed 一致**：`CONFIG_CONTRACT.md` 表 mosaic/export 行必填列改写为块结构形态；新增「`output_mode` 的必填与默认值口径」条（三条分支 `required` 一致含 `output_mode`；`surface_brightness` **只作 `--template` 骨架值**）；`CONFIG_SCHEMA.md` 加范围声明（本文 = legacy orchestrator 配置，`output_mode` 不属本文范围、必填且必须显式）。**两边不再相反**。
5. **EXP-203 C3/C8**：`DATA_SEMANTICS.md` §20.2 signal tile 单位明确为 **`ADU/px²`** + 新增「产品侧 provenance 缺口」段（Phase1/Phase2 FITS tile **无 BUNIT**、properties 无 `pixel_semantics`/`pixel_area_power` ⇒ §5.3 守卫即使接线也会拒绝当前产品，**不得**声称守卫已生效）；§20.3 补注「support 钳制计数被 f32 舍入污染」（常量场实测 262144）。
6. **§3.3 S01/S02/S04/S05/S06/S07 等逐条处置**：见 §2。
7. **`docs/design/**` 与最高设计冲突条文订正**：`PHASE1_DETAILED_DESIGN.md` §3 节点顺序按 §3.2 重写（**补两轮 WCS + apply photometry**）+ §8.2/§10 PSFSW 退役；`PHASE2_DETAILED_DESIGN.md` §6.3/§9 PSFSW 口径退役；`PHASE3_DETAILED_DESIGN.md` 投影「首批 4 种」→「**8 冻结 / 当前仅 TAN**，未实现必须显式报不支持」。

---

## 4 改动文件清单（本 claim 覆盖）

`docs/contracts/`：`API-001.md`、`INDEX.yaml`、`UNIFIED_OBJECTS.md`、`DATA_ARTIFACTS.md`、`DATA_SEMANTICS.md`、`CONFIG_CONTRACT.md`、`PUBLIC_API.md`、`v6/W6_SCHEMA_INTEGRATION.md`、`v6/data/10_migration_and_open_items.md`
`docs/api/`：`CLI_PROTOCOL_V1.md`
`docs/development/`：`CONFIG_SCHEMA.md`
`docs/design/`：`PHASE1_DETAILED_DESIGN.md`、`PHASE2_DETAILED_DESIGN.md`、`PHASE3_DETAILED_DESIGN.md`
**未改**：`config/**`（无行号漂移 ⇒ **无需改锚点**）、`ci/checks.json`、`contracts/**`、`lib/**`、排除两件。

---

## 5 验收证据（可复跑）

| 门 | 命令 | rc | 结果 |
|---|---|---|---|
| 机器门（全量 fast） | `timeout 900 python3 ci/run_checks.py --all --profile fast` | 1 | `pass=79 fail=7`（**本任务前**基线 = `pass=78 fail=8`；`CHK-CONTRACT-REF` 由 FAIL→**PASS**，其余 7 项为既有红灯，逐项同名） |
| A44 权重门 | `timeout 600 python3 ci/check_no_weight_mode.py` | 0 | `CHK-NO-WEIGHT-MODE_PASS: files=346 lines=57757 无未留痕命中` |
| CFG002 | `timeout 600 python3 tests/config/check_cfg002_registry.py --self-test` | 0 | `checks=11 pass=11 fail=0 selftest=PASS`（`CFG002-11 citations=40 token_anchors=8`；**无行号漂移，config 锚点未改**） |
| 合同图 | `python3 tools/check_contract_graph.py` | 0 | `CONTRACT_GRAPH_PASS contracts=116`（基线 FAIL：`DATA-OBJ-PSFSW-ROBUST-WEIGHT-001 路径不存在`） |
| 契约测试 | `python3 -B -m unittest discover -s tests/contracts -t tests/contracts` | 0 | `Ran 67 tests OK` |
| 配置测试 | `python3 -B -m unittest discover -s tests/config -t tests/config` | 0 | `Ran 72 tests OK` |
| v6 契约 | `python3 -m pytest tests/contracts/v6 -q` | 0 | `19 passed`（含 `O25-docs-present`） |
| 配置一致性 | `python3 tools/config_consistency_check.py` | 0 | `pass: true`（docs 腿未动） |
| DATA_ARTIFACTS | `python3 tools/check_data_artifacts.py` | 0 | `DATA_ARTIFACTS_PASS schemas=31` |
| 验收门 grep 1 | `grep -rn "0\.11\.0-alpha\.2\|0\.12\.0" docs/contracts/ docs/design/ docs/api/ docs/development/` | 0 | 仅剩**排除件** `docs/contracts/unified_object_registry.json`（前台 Q2 收口面） |
| 验收门 grep 2 | `grep -rn "第二套命令树\|astrocs run\b" docs/api/` | 1（无匹配） | `docs/api/` 无该串；真实落点 `docs/contracts/API-001.md:13` 已标**作废 + 留痕** |

日志：`run/RELEASE-03/logs/DOC-203-baseline-*.log`（基线，**任何改动之前**）、`DOC-203-final-run_checks.log`、
`DOC-203-final2-run_checks.log`（末次全量复跑，`pass=79 fail=7`）、`DOC-203-after-*.log`（逐检查器）。

> **基线对照（"本任务前已红"证据）**：`DOC-203-baseline-run_checks.log` = `pass=78 fail=8`，
> 8 项 = CHK-MODULE-MANIFEST / **CHK-CONTRACT-REF** / CHK-SCI-REF / CHK-CONTRACT-TEST / CHK-DANGLING /
> CHK-SECRET-HYGIENE / CHK-CONFIG-DEFAULTS / CHK-SPEC-NAMED-IMPL-ON-PROD-PATH；
> 末次 = 同 7 项（`CHK-CONTRACT-REF` 转 PASS），**无新增红**，`STD-REG` 全程 PASS。

---

## 6 未决与残留（**穷尽列出**）

### 6.1 需前台/后续任务修（**代码侧或非本任务域**）

1. **`docs/contracts/unified_object_registry.json`（排除件，前台 Q2 收口）**：`deprecation.deprecated_at = "0.11.0-alpha.2"`、`retire_after = "0.12.0"`（:587/:609-687 共 24 处）仍是**版本号退役窗口**，违反 §12；
   且 `tests/contracts/test_unified_object_contract.py:539` 仍断言 `retire_after` 匹配 `^\d+\.\d+\.\d+(-[a-z]+\.\d+)?$`（:571 同族）⇒ **改 registry 必须同批改该测试**（本次未动，避免两边并存的同时打破回归）。
2. **S05（FIX-201 未覆盖）**：`lib/infrastructure/aio/include/aio_pipeline.h:304` 仍写「未列出的自定义块名也允许（模块可自由扩展）」（与块词表冻结相反）；标准表 12 行**仍无 `variance` 行**（Q8 处置①）。
3. **S01 内核侧**：`lib/algorithms/coverage/src/rejection.cpp` 的 `P2_REJECT_MINMAX` / `reject_minmax_impl` / `p.minmax.*` 默认仍在位（:993/:1093/:1169/:1240-1241/:1878/:2149-2150/:2470-2471）；FIX-204（`5e8c09ce`）只改逐像素路由 `kPixelSmallNPolicy`（:1139）。
4. **S04**：`lib/infrastructure/scheduler/src/module_adapters.cpp:96/:3210` 仍登记 `apply_photometry`「生产零调用者」⇒ 阶段一注册面**未**补两轮 WCS + apply photometry。
5. **S06**：`docs/modules/common.md:46` 仍把 `precision` 来源指向待退役 orchestrator 配置（docs/modules = DOC-202 域）。
6. **S10 残留**：`docs/modules/registry/*.md` 样板句残留 5 处（原 24）。
7. **S11 残留**：`ASTROCS_DESIGN.md:820` 仍写 orchestrator「16,191 行」（实测 17,918 行）——最高设计域，归前台。
8. **CON-DOC-SYMBOLS 唯一残留**：`docs/modules/registry/astrocs.phase2.write.md` 的 `p2_write_descriptor` 未登记符号（DOC-202 域；EXP-203 `ALIGNMENT-5.3.md` C1b 同面：行锚应更新为 `module_adapters.cpp:1040-1057`）。修掉它即可让 `CHK-DANGLING` 与 `CON-FULL-INTEGRATION` 转绿。
9. **CHK-CONFIG-DEFAULTS（既有红，非本域）**：`ci/check_config_defaults.py` 报 `config_default_divergence:precision_mode values=['0.0','1.0']`（config/** 域）。
10. **DOC-LINE-ANCHORS（既有红，非本域）**：11 条 binding violation 全在 `lib/**`（P3FITS-MUTEX / P3RSMP-DESCRIPTOR / NOISE-CMAKE 等），无一条涉及本任务文件。

### 6.2 需前台裁决的口径冲突（本次**未**擅改）

- **`DATA_SEMANTICS.md` §31 标题字面量**：前台 2026-09-20 追加约束要求保留旧标题 `## 31. V6 目标态数据合同`；但**工作树中三处在飞测试已改为期待新标题**：
  `tests/contracts/v6/v6_oracle.py:531`（`need_ds = ["## 31. V6 合同层数据合同", …]`，注释逐字点名「DOC-203 落地」）、
  `tests/contracts/v6/test_v6_schema_integration.py:188`（`assertIn("## 31. V6 合同层数据合同", ds)`）、
  `tests/contracts/v6/test_v6_negative_mutations.py:145`（mutation 把该串替换为 `## 31. (removed)` 以证明 O25 能红）。
  ⇒ 两个标题互斥，**当前状态（新标题）机器门全绿**（`pytest tests/contracts/v6 -q` → 19 passed）。若要回到旧标题，**必须同批回退这三处测试**，否则 `O25-docs-present` 必红。
- **Q1「独立恢复 / 无断点续算」**（DESIGN-DRAFT §4.2-Q1，GAP_AUDIT §4 未裁决）：`ASTROCS_DESIGN.md` §1.2:78 已写「独立**重跑**（无断点续算）」⇒ 本次按 §0.2 把 `docs/api/CLI_PROTOCOL_V1.md:27` 的「独立恢复」订正为「独立重跑（无断点续算）」；`API-001.md` 的 checkpoint 措辞（scheduler 内部机制）未动。
- **`docs/design/UNIFIED_MODEL.md` / `docs/contracts/unified_object_registry.json`**：本次**零改动**（排除件）⇒ 未造成任何行号漂移；`config/defaults.json` 与 `phase_config_mosaic.schema.json` 对 `UNIFIED_MODEL.md:46` 的锚点**不受影响**（无需前台修锚）。
