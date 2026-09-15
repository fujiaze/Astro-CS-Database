# W6 生产 schema 集成登记（SCHEMA-INTEGRATE-001）

- 文档 ID：`DATA-V6-SCHEMA-INTEGRATION`
- 任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/SCHEMA-INTEGRATE-001.md`（Wave 6，depends_on = CONTRACT-FREEZE-001）
- write_scope：`contracts/schemas/`、`contracts/data/`、`docs/contracts/`、`tests/contracts/v6/`
- 基线 HEAD：`ac04289dea9d3ccb3dad8310dade53e75162447f`（`git rev-parse HEAD` 实测；controller C-007 已把 `docs/contracts/{DATA_SEMANTICS,PUBLIC_API}.md` 的 P33/P27 回退态固化为本基线）
- 语义权威（唯一）：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json`（96 条款：FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8）

> 权威分层：冻结宪章 `ASTROCS-CONSTITUTION-001` > `docs/owner/PROJECT_SPEC.md` > 三份 `docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md` >
> `docs/science/UNIFIED_SCIENCE_MODEL.md` / `docs/science/PSF_SIGNAL_WEIGHT.md` > W4 冻结表 > 本集成登记 > 代码/测试。
> 本登记**不改变**任何冻结公式/容差/门/枚举，只做 schema 落位、词表归一、兼容/迁移与验证。

## 1. 交付物

| # | 类别 | 落点 | 说明 |
|---|---|---|---|
| 1 | 生产 schema（10 件） | `contracts/schemas/v6/astrocs.v6.*.v1.schema.json` | 由 `contracts/proposals/v6/data/*.v1.schema.json` 迁移；JSON Schema 2020-12；全部通过 meta-schema 校验 |
| 2 | 数据字典 | `contracts/data/v6_data_dictionary_v1.json` | 单位表 / BUNIT / weight_mode 三分 / 禁止项 / provenance 最小集 / concentration 权威 / fail-closed / 条款注册表 / 签字与开放登记 |
| 3 | 单一权重词表 | `contracts/data/v6_weight_vocabulary_v1.json` | canonical 字段 + 双词表双向映射 + 第三套词表禁止项 + legacy 整数处置（DI-01/DI-07 闭合） |
| 4 | 迁移映射表 | `contracts/data/v6_migration_map_v1.json` | 文件级 + 记录级 legacy→v6 映射与 reader/writer 规则 |
| 5 | 生产正例 | `contracts/data/examples/v6/*.example.json`（10 件） | 逐条通过目标 schema（含 concentration 单位修正） |
| 6 | 合同文档 | `docs/contracts/DATA_SEMANTICS.md` §31；`docs/contracts/PUBLIC_API.md`「V6 消费面」 | 单位表/BUNIT/weight_mode/provenance/fail-closed 消费语义 |
| 7 | 验证 | `tests/contracts/v6/` | 独立 Oracle（对照冻结表）、schema 校验、词表归一、兼容/迁移、负向 mutation（≥12 必红） |

## 2. 词表归一（C-004.3 / DI-01 / DI-07）

两套既有权重词表归一为**单一权威 schema**，无第三套：

| 语义 | SCI-PSFW 词表 | SCI-P2 词表 | canonical（唯一字段名） | 合法值 |
|---|---|---|---|---|
| 权重对象种类 | `weight_kind` | `weight.kind` | `weight.kind` | psfsw = `psfsw_robust_weight`（legacy 别名 `relative_dimensionless` 仅 reader 接受） |
| 权重单位串 | `weight_units` | `weight.units` | `weight.units` | psfsw = `"1"`（legacy `dimensionless_relative` 仅 reader 接受；DI-07 落定 canonical = 冻结字面量 `"1"`） |
| 归一域 | `normalization.scope` | `group_normalized` | `weight.group_normalized`（同步 `normalization.scope="group"`） | `true` |
| 组内 median 目标 | `normalization.median_target` | `group_normalized=true (median=1)` | `weight.normalization.median_target` | `1.0` |

- 生产 schema 删除 proposal 中的 `kind_alias_sci_psfw` / `units_alias_sci_psfw` / `normalization_scope_alias` 桥接字段与 `x-astrocs-token-mapping-for-W6`；别名只存在于迁移层的 reader 规则中（`v6_weight_vocabulary_v1.json#canonical_fields[].legacy_aliases`）。
- `forbidden_third_vocabulary_tokens`（`weight_normalized`/`normalization_scope`/`weight_type`/…）任一出现 → REJECT（`FZ-FIELD-WEIGHTMODE`）。
- legacy 整数：`0=support×snr² → REJECT`（必须拒绝）；`1 → equal`；`2 → pixel_ivar`（仅文档基线）。

## 3. 已登记冲突的裁定：concentration 单位

- **唯一权威**：`psfsw.concentration` 单位 = `component_flux_unit/px^2`（`A_NEA = 1/ΣP²`，单位 `px^2`），锚 = `ALG-P2-PSFSW-001` 单位一致性规则 + `FZ-FIELD-PSFSW-4COMP` + `FZ-COND-WHITENOISE`。
- **登记文本错误（已由 W12 订正）**：`docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md` §4.2 表中曾写作 `ADU/px`。本任务（W6）不改 `docs/algorithms/`（不在 write_scope），只在数据字典 `concentration_unit_authority.registered_text_error` 登记，并使其在**生产合法域之外**：schema `concentration.units` 用 `pattern ^[A-Za-z][A-Za-z0-9_()^\-]*/px\^2$` 收紧，`ADU/px` 结构即红。**DOC-CONVERGE-001（W12，`reports/v6/release-review/01_CONVERGENCE_CORRECTIONS.md`）已把该文本与机器伴生 `alg_p1_001_spec.json` 的 concentration 单位订正为 `ADU/px²`（依据 `FZ-FIELD-PSFSW-4COMP` + `ALG-P2-PSFSW-001` 单位一致性规则 + 本 schema 的 `concentration.units` pattern；`A_NEA=px²`），不改变任何冻结公式/容差/门。** `contracts/**` 中 schema 的 `x-astrocs-concentration-unit-authority.registered_text_error` 仍描述订正前状态，需 `contracts/` owner（SCHEMA-INTEGRATE-001）同源刷新。
- proposal 正例 `contracts/proposals/v6/data/examples/psfsw.example.json` 的 `ADU/px` 未被静默采纳：生产正例 `contracts/data/examples/v6/psfsw.example.json` 修正为 `ADU/px^2` 并新增 `component_flux_unit` 声明。
- 修正 FROZEN 正文本身须 DOC-CONVERGE-001(W12) + 负责人签字（宪章 §1.2）。

## 4. P33-COEF / P27 回退态保持（不得重新引入）

- 控制器 `ac04289d` 已固化 `docs/contracts/{DATA_SEMANTICS,PUBLIC_API}.md` 的 P33-COEF §12.2 SNR 系数落位段与 P27「生产路径不消费的 `IpvParams` 字段」整节的回退态。本次只**追加** §31 与 V6 消费面两节，**未**重新加回任何 P33/P27 段落（Oracle 以文本守卫逐条判红：出现 `P33-COEF` / `snr_coefficient` / `SNRCOEF` / `P27` / `DEAD-PARAMS` 即失败）。
- 帧级 `median(SNR_F)` 只登记为诊断/深度表达（`C-004.2`；`FZ-GATE-MEDIAN-SNR`），不得接入 `weight.sources` / `weight_value` / `variance_from`。

## 5. legacy → v6 迁移路径与向后兼容声明

| 迁移 ID | from | to | reader 规则 | 兼容性 |
|---|---|---|---|---|
| `MIG-WEIGHTMODE-LEGACY` | `ASTROCS_WEIGHT_MODE ∈ {0,1,2}` / ACR `{auto,ivar,equal,support_x_snr2}` | 显式字符串模式 + 版本 | 0→REJECT；1→equal；2→pixel_ivar；auto/support_x_snr2/未知→REJECT | reader 兼容（拒绝 0）；writer 只写 v6 |
| `MIG-VOCAB-PSFSW-CANONICAL` | 双词表并存 | canonical `weight.{kind,units,group_normalized,normalization.*}` | 别名规范化；第三套名→REJECT | reader 兼容；writer 只写 canonical |
| `MIG-UNITS-PXVARIANCE` | 同名 `variance`（ADU² / ADU²/px 混名） | `pixel_variance_in`/`sb_variance_out`/`sb_ivar_out` | BUNIT=ADU 无 pixel 语义 → 单位不可判 REJECT | reader 兼容（缺声明拒绝） |
| `MIG-NORM-DRIZZLE` | `w_jp=a_jp/A_drop` 前向归一 | 面亮度保持 + `flux_conservation_factor` | legacy 归一须带版本；缺 factor 禁绝对通量 | 语义兼容（非静默改变） |
| `MIG-COVARIANCE-PRODUCT` | 只存对角 variance | 对角 + 相关核/可重建算子摘要 | 只对角无核 → REJECT | 强化（fail-closed） |
| `MIG-DIAGNOSTIC-NOT-WEIGHT` | 诊断量作权重 | 仅诊断/深度/门 | 诊断别名命中 → REJECT | 撤销旧权重形态（已登记 `SUPERSEDED`） |
| `MIG-PROVENANCE-KEYS` | `ASTROCS_WEIGHT_MODE` 整数 / 无 reason 的 unavailable | `weight_mode_version` + `unavailable.{flag,reason,scope}` | 缺 reason/scope → REJECT | reader 兼容（缺声明拒绝） |
| `MIG-SCHEMA-PLANE-OWNER` | exchange plane ∈ {signal,support,variance,ivar,mask} | v6 描述层对象 | science plane 扩展须 schema + runtime validator 同一提交 | **保持 OPEN（DI-06）**：runtime 不在本任务 write_scope，未改 exchange schema |

**向后兼容声明**：既有平面语义（`signal/support/variance/ivar/mask`）、BUNIT 二次律、provenance 最小集**不被静默改变**；所有 legacy 记录由 reader 显式规范化或拒绝，不存在"静默按新语义重解释"。生产 writer 只产出 v6 canonical。

## 6. 验证与证据

| 命令（仓库根） | 期望 rc |
|---|---|
| `python3 tests/contracts/v6/tools/gen_production_schemas.py` | 0 |
| `python3 tests/contracts/v6/tools/gen_data_dictionary.py` | 0 |
| `python3 -m unittest discover -s tests/contracts -t tests/contracts`（CI `UT-CONTRACTS`） | 0 |
| `python3 tests/contracts/v6/run_all.py` | 0（单聚合 rc，含独立 Oracle + ≥12 负向 mutation 全红） |

实测 rc、Oracle 逐项结论与 mutation 明细见 `tests/contracts/v6/evidence/rc_summary.json`、`evidence/oracle_report.json`、`evidence/mutations.json` 与 `evidence/logs/`。

### 6.1 实测 rc（基线 HEAD = ac04289da9d3ccb3dad8310dade53e75162447f）

| 命令（仓库根） | 实测 rc | 结果 |
|---|---|---|
| `python3 tests/contracts/v6/tools/gen_production_schemas.py` | 0 | GEN_PASS 10 schemas（可重跑，产物 sha256 不变） |
| `python3 tests/contracts/v6/tools/gen_data_dictionary.py` | 0 | DICT_PASS |
| `python3 tests/contracts/v6/v6_oracle.py` | 0 | ORACLE_PASS 40/40（独立对照 W4 冻结合同） |
| `python3 -B -m unittest discover -s tests/contracts -t tests/contracts`（CI `UT-CONTRACTS`） | 0 | Ran 28 tests OK（含既有 9） |
| `python3 tests/contracts/v6/run_all.py` | 0 | RUN_ALL_PASS（Oracle 40/40 + **34** 条负向 mutation 全红 + 10/10 正例） |
| `python3 tools/check_data_artifacts.py` | 0 | DATA_ARTIFACTS_PASS schemas=28 |
| `python3 tools/check_contract_graph.py` | 0 | CONTRACT_GRAPH_PASS contracts=100 |
| `python3 tools/check_agents_gov.py` | 0 | GOV_CHECK_PASS 10/10 |
| `python3 tools/check_version_consistency.py` | 1（**预存，非本次引入**） | 4 条全部落在未修改的 `docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md`（`git show HEAD:` 即含该字面量）；本任务新增文件 0 条 |

- 负向 mutation **34** 条（要求 ≥12）：单位错 / psfsw 写成 ivar / group_normalized=false / scope=global / median_target≠1 / 缺 component_flux_unit / concentration 放宽为 `ADU/px` / validity 白名单越界 / 禁止键守卫清空 / weight.kind=ivar / `psf_snr_power` 进生产 / legacy 整数放行 / deferred 登记被抹 / `k_corr=1` / 缺 k_corr / variance_from 含权重 / propagation 改写 / signal 幂次门移除 / 缺 effective_psf_id / 字典单位错 / 禁止来源漏 fwhm / 待签写成 FROZEN / 计数错 / concentration 权威改回 `ADU/px` / mode 单位错 / canonical 单位反向 / legacy 0 放行 / 迁移覆盖不全 / 正例 concentration 错 / 正例 `k_corr=1` / 删 §31 / 重新引入 P33 段 / 重新引入 P27 段 / PENDING 数值写成 FROZEN。
- 每条 mutation 均由 `Oracle.overrides` 在临时影子文件上重跑**同一** Oracle，断言失败且指定检查项变红（不是"同实现自证"）。
- 产物确定性：`gen_*` 重跑后 `contracts/schemas/v6/*` 与 `contracts/data/v6_*.json` 的 sha256 不变。
- PENDING 派生结构门（`PSFSW-T-NMIN` 的 n_common 下限、`FZ-PROV-KCORR-VALUE`）在 schema 内以 `x-astrocs.signoff=PENDING_OWNER_SIGNOFF` 显式标注，并由 Oracle `O32` 守卫（写成 FROZEN 即红，见 mutation M34）。

## 7. 未决风险与待签

- `SO-01`..`SO-07` 全部保持 `PENDING_OWNER_SIGNOFF`（49 条条款），生效前 fail-closed；本集成不使其生效。
- 8 条条款级 `OPEN`（`QF-G-INJ-01/02/03/07`、`QF-G-RD-01/02`、`QF-G-BASE-03`、`CF-T-P3-CORR-EPSILON`）保持 OPEN。
- `DI-06`（exchange plane 枚举 ↔ runtime validator 同提交）保持 OPEN，owner = SCHEMA-INTEGRATE-001(W6) / IMPL-AIO-001。
- `DI-01`（词表归一）与 `DI-07`（`weight_units` 双射）由本任务闭合并以机器 word list 固化。

## 8. 边界声明

- 未 commit / push / git add / 建分支 / worktree / stash / reset / clean / rebase；未派生子代理。
- 只写 `contracts/schemas/`、`contracts/data/`、`docs/contracts/`、`tests/contracts/v6/`；未改 `docs/science/**`、`docs/owner/**`、`docs/design/**`、`docs/references/**`、生产源码、CI、根 CMakeLists。
- 未改冻结公式/容差/门；未解冻 `psf_snr_power`；未让 `median(SNR_F)`/support/coverage/FWHM 进权重面；未宣布发布。
