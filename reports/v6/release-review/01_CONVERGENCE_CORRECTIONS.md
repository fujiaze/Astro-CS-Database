# 文档订正清单（DOC-CONVERGE-001 / V6 并行包 Wave 12）

- 基线 HEAD：`8e1e280e8db498aa879abd52ed8d18fa9f0eabd2`
- 行号为**订正后**文件内的行号（`grep -n` 实测）。
- 依据锚一律指向冻结条款 id / 冻结文件路径 / 控制包裁决（`CONTROLLER_LOG.md C-00x`）。
- **未改任何冻结公式/容差/门/枚举的值**；只做口径收敛、文本错误订正、状态词与登记同步。

## 1. 登记文本错误订正：concentration 单位 `ADU/px` → `ADU/px²`

| # | 文件 | 行 | 旧文 | 新文 | 依据锚 |
|---|---|---|---|---|---|
| 1 | `docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md` | 203 | `| concentration | psfsw.concentration | ADU/px | …` | `| concentration | psfsw.concentration | ADU/px² | …；单位唯一权威 = component_flux_unit/px²（A_NEA = 1/ΣP²，单位 px²）…` | `FZ-FIELD-PSFSW-4COMP`；`ALG-P2-PSFSW-001` 单位一致性规则；`contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json` 的 `concentration.units` pattern `^[A-Za-z][A-Za-z0-9_()^\-]*/px\^2$`；`docs/contracts/v6/W6_SCHEMA_INTEGRATION.md` §3（W6 登记） |
| 2 | `docs/algorithms/v6/phase1/alg_p1_001_spec.json` | 91 | `"psfsw.concentration": "ADU/px"` | `"psfsw.concentration": "ADU/px^2"` | 同上（机器伴生，与 #1 同源） |
| 3 | `docs/algorithms/v6/phase1/alg_p1_001_spec.json` | 284 | `"unit": "ADU/px"`（`name=psfsw.concentration`） | `"unit": "ADU/px^2"` | 同上 |
| 4 | `docs/algorithms/v6/phase1/tools/verify_alg_p1_001.py` | 445 | `"unit": "ADU/px", "value": 4.1` | `"unit": "ADU/px^2", "value": 4.1` | 同上（正向 fixture 与权威单位一致） |
| 5 | `docs/contracts/v6/W6_SCHEMA_INTEGRATION.md` | 43 | 「登记文本错误：……（W12 待修）」 | 「登记文本错误（**已由 W12 订正**）……W12 已把该文本与机器伴生 …… 订正为 `ADU/px²`……`contracts/**` 中 schema 的 `registered_text_error` 仍描述订正前状态，需 contracts/ owner 同源刷新」 | `FZ-FIELD-PSFSW-4COMP`；本任务订正记录 |
| 6 | `docs/contracts/DATA_SEMANTICS.md` | 2640 | 「……属登记在案的文本错误（W12 待修……）」 | 「……属登记在案的文本错误，**已由 DOC-CONVERGE-001（W12）订正为 `ADU/px²`**……不改任何冻结公式/容差/门」 | `DATA-V6-SCHEMA` §31.7；`FZ-FIELD-PSFSW-4COMP` |

**验证**：`python3 docs/algorithms/v6/phase1/tools/verify_alg_p1_001.py` → **rc=0，38/38 PASS**（订正后复跑）；`python3 -c "import json;json.load(open('docs/algorithms/v6/phase1/alg_p1_001_spec.json'))"` → rc=0。
残留 `ADU/px`（非 px²）仅剩：(a) 上表 #5/#6 的**订正登记文字**；(b) `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md:624` 的 `σ_sky=10 ADU/px`（**天空每像素 σ**的真实单位，非 concentration，正确）。

> **签署形式登记**：`contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json` 的 `x-astrocs-concentration-unit-authority.registered_text_error` 原文要求「DOC-CONVERGE-001 修正，须负责人签字后方可改 FROZEN 正文」。本任务按任务卡正文执行订正、不改任何冻结值，并把「负责人签署」形式登记为本包未决项（见 `03_PENDING_AND_OWNER_ITEMS.md`）。`contracts/**` 不在本任务写域，故该 schema 内 `registered_text_error` 字段的文案刷新留给 contracts/ owner（SCHEMA-INTEGRATE-001）。

## 2. 权重词表单一权威（canonical）与第三套词表残留清除

| # | 文件 | 行 | 订正 | 依据锚 |
|---|---|---|---|---|
| 7 | `docs/contracts/v6/data/05_point_information_and_weight_mode.md` | 67 | §4「两套既有词表，不发明第三套」段后**追加 W6 归一完成标注**：单一权威 = `contracts/data/v6_weight_vocabulary_v1.json`；canonical = `weight.kind` / `weight.units` / `weight.group_normalized` / `weight.normalization.{scope,median_target,constants_version}` / `weight.weight_value`；生产 schema 不含别名字段、别名仅在迁移层 reader；第三套名出现即 REJECT | `C-004.3`；`DI-01`/`DI-07`；`FZ-FIELD-WEIGHTMODE`；`docs/contracts/DATA_SEMANTICS.md` §31.4；`docs/contracts/v6/W6_SCHEMA_INTEGRATION.md` §2 |
| 8 | `docs/contracts/DATA_SEMANTICS.md` §31.4（既有，未改语义） | 2597–2608 | 已声明 canonical 唯一权威；本任务仅复核并在 `01`/`02` 登记，无文本改动 | `C-004.3`；`FZ-FIELD-WEIGHTMODE` |

- 全 `docs/` 检索第三套词表 token（`weight_normalized`/`normalization_scope`/`weight_type`/`weight_kind_name`/`norm_scope`/`unit_string`/`is_group_normalized`/`weight_dimensionless`）：仅出现在「禁止/第三套/legacy/别名/REJECT/迁移」语境（`DATA_SEMANTICS` §31.4、`W6_SCHEMA_INTEGRATION` §2、`05_…` §4），由检查器 C1 机器判定（`artifacts/v6/release-review/doc_convergence_report.json`）。

## 3. 回退态不得重新引入（P33/P27）

| # | 文件 | 订正 | 依据锚 |
|---|---|---|---|
| 9 | `docs/contracts/DATA_SEMANTICS.md`、`docs/contracts/PUBLIC_API.md` | **未**重新引入 P33-COEF §12.2 SNR 系数落位段与 P27「生产路径不消费的 IpvParams 字段」节；机器检查 C7 逐字面量（`P33-COEF`/`snr_coefficient`/`SNRCOEF`/`DEAD-PARAMS`/`P27`）零命中 | `C-007` F1 局部裁定（`ac04289d`）；`C-009`；`docs/contracts/v6/W6_SCHEMA_INTEGRATION.md` §4 |
| 10 | 文档面 | **未**把 P33/P35/P36 回退态（`lib/phase1/noise/snr_frame_coefficient` 删除、`tests/unit/p35_*`/`p36_*` 删除、P36 六文件回退族）写成「已实现/已恢复」；帧级 `median(SNR_F)` 仅登记为诊断/深度表达 | `C-004.2`；`C-007`/`C-008`；`FZ-GATE-MEDIAN-SNR` |

## 4. 状态词 / 发布口径收敛（README / REVIEW / CHANGELOG）

| # | 文件 | 行 | 订正 | 依据锚 |
|---|---|---|---|---|
| 11 | `README.md` | 15–20 | 顶部新增 V6 冻结口径指针：三生产模式、`psf_snr_power` DEFERRED、冻结表 96 条款、单一词表、发布复核包路径、「当前未发布」 | 宪章 §16.1/§17.12；`FZ-MODE-*`；`C-004.1` |
| 12 | `README.md` | 42–45 | Phase2 三模式改为规范名 `{point_information, surface_gls, psfsw_robust}`；补 `Q=aPᵀC⁻¹d`/`W_info=a²PᵀC⁻¹P`；明确 psfsw **无量纲/组内 median=1、不是 ivar/Fisher**；`psf_snr_power` DEFERRED 且生产拒绝 | `FZ-FORMULA-Q`/`FZ-FORMULA-WINFO`/`FZ-FIELD-PSFSW-UNIT`/`FZ-MODE-DEFERRED` |
| 13 | `README.md` | 49–53 | Linux 角色由「仅控制/静态分析/轻量编译/小合成（约束 §B）」订正为「控制/开发节点**同时是 Linux 验证节点**（宪章 §15.1/§15.4）」；ACR 引用改宪章 §10.1/§3.3 | 宪章 §1.1/§15.1/§15.4/§18（supersession）；memory §1 |
| 14 | `README.md` | 55–78 | 「当前状态」段如实收敛：V6 已交付面；**Linux CI 红**（THREAD-BUDGET P36 回退态 / CTEST-REGISTRATION 2 项非 V6 IPV）；Windows `AWAITING_WINDOWS_VALIDATION` + FD-F-003；SO-05 65.09%/87.63% 待裁决；Phase2 CLI obs=0；REAL-SCIENCE 三口径 DOCUMENTED_BASELINE；51 条待决；`run --phases` 已删、§F.1 已达成；**NOT_READY、未发布** | `C-008`/`C-009`；证据见 `artifacts/v6/release-review/logs/ci_redline_recheck.log` |
| 15 | `REVIEW.md` | 13–21 | 顶部新增 V6 包收敛补充块（冻结表 / 单一词表 / §14.5 复核路径 / NOT_READY） | 宪章 §14.5/§17.12 |
| 16 | `REVIEW.md` | 19–24 | §1 一句话结论补 V6 已交付与未完成面（CI 红 / Windows / 真实数据终验 / 图像初审 / SO-05） | `C-009` |
| 17 | `REVIEW.md` | §4 | 「本轮执行态」由 DOC-CONV-001 期 cprun run 改为 V6 包 Wave 0–12 集成态 + Wave 13/14 BLOCKED + TASK_LEDGER/CONTROLLER_LOG 唯一源；历史 run 保留追溯 | `TASK_LEDGER.csv`；控制包 `00_READ_FIRST.md` |
| 18 | `REVIEW.md` | §5 | 新增「V6 面状态」表（合同/三模式/词表 schema/三 Phase/Runtime/真实数据/性能 与 CI 红/真实终验/图像初审/Windows/SO-05/51 条待决），原 DOC-CONV-001 表保留为历史 | `reports/v6/**`；`FZ-*` |
| 19 | `REVIEW.md` | §7/§8 | §7 新增 V6 待裁决 6 项并保留历史事项；§8 发布口径改为 §14.5 六步 `NOT_MET/NOT_MET/NOT_MET/NOT_MET/AWAITING/NOT_MET`、**NOT_READY / NOT_RELEASED** | 宪章 §14.5/§17.12；本目录 `04_RELEASE_REVIEW_SECTION14_5.md` |
| 20 | `CHANGELOG.md` | 3–42 | 当前节标题由 `[0.11.0-alpha.1]`（陈旧，与根 `VERSION`=`0.11.0-alpha.2` 不一致）订正为 `[0.11.0-alpha.2]`，并新增 V6 并行包集成事实与未通过/未决项；旧 alpha.1 节降为「历史节」；**未提升任何 alpha 编号**（仅与根 `VERSION` 单源一致化） | 宪章 §16.1；`docs/governance/VERSION_NAMESPACES.md`；`GOV-003` |

## 5. 文档索引覆盖（闭合 OI-04）

| # | 文件 | 行 | 订正 | 依据锚 |
|---|---|---|---|---|
| 21 | `docs/DOCUMENT_INDEX.yaml` | 564– | 新增 61 条 `docs/**/v6/**`（algorithms/contracts/science/validation 的 v6 frozen / 规格 / 数据设计 / 验证矩阵）为 `ACTIVE_NORMATIVE`，闭合 OI-04 的 `docs_fully_covered` 红 | `OI-04`（owner = DOC-CONVERGE-001(W12)/控制器）；`AR-032-GAP`；`tools/doccheck/check_doc_index.py` |

**验证**：`python3 tools/doccheck/check_doc_index.py` → **rc=0，DOC_INDEX_PASS，覆盖 282 文件，index_count=305**（`artifacts/v6/release-review/logs/doc_index_after.json`）。

> 机器可读订正清单：`artifacts/v6/release-review/convergence_corrections.json`（与上表同源）。

## 6. 未做的（结构性受限，如实登记）

- `docs/owner/**`、非 v6 `docs/science/*.md`、`docs/design/**`、`docs/references/**` 正文**未改写**：控制包 `C-004.5` 明文「在本包过程中不得改写（其被取代段由 W4 登记清单，正式 amendment 由负责人签字）」。其被取代段清单见 `reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md` 与冻结表的 `superseded_sections`；正式取代须负责人签字（`AR-032-GAP` / `SO-06`）。
- `AGENTS.md` 不在本任务写域；`memory.md` 亦不在 write_scope，其「当前 SHA/版本」仍停在 DOC-CONV-001 期（`da3c4b4a`）——登记为后续任务项（见 `03`）。
- `contracts/**`（schema 的 `registered_text_error` 文案、数据字典）由 contracts/ owner 刷新。
