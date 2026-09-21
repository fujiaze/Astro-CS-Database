> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# DATA-DESIGN-001 — 跨 Phase signal / covariance / PSF / W_info / PSFSW / weight_mode / effective PSF / provenance schema 设计（已按 §9.73 A44 作废：该概念不存在）

> 上游：ASTROCS_DESIGN.md §3.1（数据对象）、§8.4（模块与 ABI）

- 文档 ID：`DATA-DESIGN-001-SCHEMA-DESIGN`
- 任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/DATA-DESIGN-001.md`（Wave 3，depends_on = SCI-ADJ-001）
- 写域（tracked）：`docs/contracts/v6/data/`、`eng/contracts/proposals/v6/data/`；工作区证据：`run/v6/data-design/`（`run/*` 受 `.gitignore` 约束）
- 基线：`HEAD = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`（`git rev-parse HEAD` 实测，main）
- 性质：**设计提案（PROPOSAL_NOT_FROZEN）**。本任务只设计 schema，不实现 schema 校验器、不改生产源码/合同、不改冻结门/容差。
- 建议状态：**PASS**（任务正文逐项完成、独立结构 Oracle 25/25 rc=0、负向 mutation 25/25 全红、文档锚审计 4/4 判红、写域干净；
  5 项开放项（DI-02..DI-06；DI-01/DI-07 已由 W6 闭合并机器固化——**2026-09-20 订正**，依据 `W6_SCHEMA_INTEGRATION.md:104`）与 SO-01..07 按纪律只登记不擅改，均由 W3/W4/W6/负责人承接，见 `10_migration_and_open_items.md`）

上位锚：`ASTROCS_DESIGN.md` §0 权威链；`docs/owner/PROJECT_SPEC.md` §3/§5/§6/§7/§8；
（**2026-09-20 订正**：原文「冻结宪章 `ASTROCS-CONSTITUTION-001` §1.1/§4.1/§4.3/§6.3/§7.3」已作废——宪章由 ROOT-007 删除、**不作权威**，依据 `ASTROCS_DESIGN.md:31` + `docs/owner/PROJECT_SPEC.md:9`；该引用仅存历史溯源，见本文件 `:1` DOC-001 注记）
`docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md`；`docs/science/UNIFIED_SCIENCE_MODEL.md`；`docs/science/PSF_SIGNAL_WEIGHT.md`；
`docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md`；`reports/v6/science-adjudication/adjudications.json`；`CONTROLLER_LOG.md` C-004。

> 权威分层（**2026-09-20 订正**）：`ASTROCS_DESIGN.md` §0 权威链 > `docs/owner/PROJECT_SPEC.md` > 三份 `docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md` >
> `docs/science/UNIFIED_SCIENCE_MODEL.md` / `docs/science/PSF_SIGNAL_WEIGHT.md` > 专项 SCI/ALG/DATA/API > 代码/测试 > 历史文档。
> （原文顶层为「冻结宪章 `ASTROCS-CONSTITUTION-001` >」——**已作废**：宪章由 ROOT-007 删除、不作权威，依据 `ASTROCS_DESIGN.md:31` + `docs/owner/PROJECT_SPEC.md:9`；保留于此留痕。）
> 科学语义冻结源 = `docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md` + `reports/v6/science-adjudication/adjudications.json`；
> 本设计**原样继承**，不重新解释、不新增科学口径。

## 1. 本任务交付什么

按任务正文（signal / covariance / PSF / W_info / PSFSW components / weight_mode / effective PSF / provenance）（已按 §9.73 A44 作废：该概念不存在）
给出**可实施、可验证**的字段名、单位、适用域、fail-closed 条件、验证门与迁移建议：

| # | 主题 | 人读规格 | 机器 schema（proposal） |
|---|---|---|---|
| 1 | 单位表 + BUNIT 语义 | `01_units_and_bunit.md` | `astrocs.v6.units.v1.schema.json` |
| 2 | signal | `02_signal.md` | `astrocs.v6.signal.v1.schema.json` |
| 3 | covariance / variance / correlation | `03_covariance.md` | `astrocs.v6.covariance.v1.schema.json` |
| 4 | PSF + effective PSF | `04_psf_and_effective_psf.md` | `astrocs.v6.psf.v1.schema.json`、`astrocs.v6.effective-psf.v1.schema.json` |
| 5 | W_info / point_information | `05_point_information_and_weight_mode.md` | `astrocs.v6.point-information.v1.schema.json` |（已按 §9.73 A44 作废：该概念不存在）
| 6 | weight_mode 语义与词表 | `05_point_information_and_weight_mode.md` | `astrocs.v6.weight-mode.v1.schema.json` |（已按 §9.73 A44 作废：该概念不存在）
| 7 | PSFSW 四分量/复合/归一 | `06_psfsw.md` | `astrocs.v6.psfsw.v1.schema.json` |
| 8 | provenance 最小集/共享系统项/k_corr/降级 | `07_provenance.md` | `astrocs.v6.provenance.v1.schema.json` |
| 9 | Phase3（Omega / 采样核 registry / Q-W 重算） | `08_phase3.md` | `astrocs.v6.phase3.v1.schema.json` |
| 10 | 验证门 + 负向 mutation 判据 | `09_verification.md` | `astrocs.v6.data-design-catalog.v1.json`（gate_index） |
| 11 | 迁移建议 + 开放项 + 签字项 | `10_migration_and_open_items.md` | catalog `migrations` / `open_items` |

机器可读索引 `eng/contracts/proposals/v6/data/astrocs.v6.data-design-catalog.v1.json` 由
`run/v6/data-design/tools/gen_catalog.py` 从上述 schema 与 `adjudications.json` 机械导出
（与 SCI-ADJ-001 的 `render_freeze_list.py` 同构做法）：它登记冻结单位表、weight_mode 枚举、（已按 §9.73 A44 作废：该概念不存在）
禁止项、**每个字段的条款锚/单位/适用域/fail-closed/门 id**、19 条 required freeze id 覆盖、迁移与开放项。

## 2. 与 SCI-ADJ-001 冻结的一致性（逐条）

| 冻结条目 | 本设计落点 |
|---|---|
| `FZ-UNIT-SIGNAL-SB`=`ADU/px^2`、`FZ-UNIT-VAR-IN`=`ADU^2`、`FZ-UNIT-VAR-SB`=`ADU^2/px^4`、`FZ-UNIT-IVAR-SB`=`px^4/ADU^2` | `01_units_and_bunit.md`；`units.v1.table` + `covariance.v1.variance_plane/ivar_plane` |
| `FZ-UNIT-WINFO`=`ADU^-2`、`FZ-UNIT-Q`=`ADU^-1`、`FZ-UNIT-FLUX`=`ADU` | `05_point_information_and_weight_mode.md`；`point-information.v1.{W_info,Q,flux,flux_variance}` |（已按 §9.73 A44 作废：该概念不存在）
| `FZ-UNIT-PSFSW`=`1`（无量纲、组内相对、**不是** ivar/Fisher） | `06_psfsw.md`；`psfsw.v1.weight.units="1"` + `group_normalized=true` + 禁止键门 |
| `FZ-FORMULA-DRIZZLE-SB` 面亮度保持归一 | `02_signal.md`；`signal.v1.formula_ref` + `constant_field_oracle` |
| `FZ-FORMULA-WINFO/Q/FHAT`、`FZ-COND-WHITENOISE` | `05_point_information_and_weight_mode.md`；`point-information.v1.authoritative_formula` + `white_noise_approximation` |（已按 §9.73 A44 作废：该概念不存在）
| `FZ-FORMULA-GLS` + `FZ-GATE-PIXIVAR-APPROX` | `03_covariance.md`；`covariance.v1.surface_gls_normal_equations` + `approximation` |
| `FZ-FORMULA-COV-PROP` `C_out=R C_in R^T` | `03_covariance.md`；`covariance.v1.propagation/variance_from/combination_coefficients` |
| `FZ-FIELD-PSFSW-4COMP`/`FZ-FIELD-PSFSW-UNIT` | `06_psfsw.md`；`psfsw.v1.components`（measurement_id 互异 + p05/p50/p95 + 有效覆盖） |
| `FZ-GATE-PSFSW-FAILCLOSED`/`-COV`/`-EPSF` | `06_psfsw.md`；`psfsw.v1.validity/covariance_ref/effective_psf_ref` |
| `FZ-MODE-PRODUCTION`/`-BASELINE`/`-DEFERRED`、`FZ-FIELD-WEIGHTMODE` | `05_point_information_and_weight_mode.md`；`weight-mode.v1` |（已按 §9.73 A44 作废：该概念不存在）
| `FZ-GATE-MEDIAN-SNR`/`FZ-GATE-SUPPORT-COVERAGE`/`FZ-GATE-PSFSW-EPSF` | `04_psf_and_effective_psf.md`；各 schema 的 `fail_closed` + `G-DIAGNOSTIC-NOT-WEIGHT` |
| `FZ-BUNIT-SEMANTICS`/`FZ-P3-BUNIT-QUADRATIC` | `01_units_and_bunit.md` |
| `FZ-COND-FLUX-CONSERV` | `01/02`；`normalization.flux_conservation_factor` |
| `FZ-PROV-MINIMAL-SET`/`FZ-PROV-SHARED-SYSTEMATIC`/`FZ-PROV-KCORR` | `07_provenance.md`；`provenance.v1` |
| `FZ-DEGRADE-SCALAR` | `02/03/05/07`；`degradation` 块（p05/p50/p95+双门） |
| `FZ-GATE-CONST-SB`/`FZ-GATE-PARENT-VAR` | `02/03`；`constant_field_oracle`、`diagonal_approximation` |
| `FZ-P3-MODES`/`FZ-P3-FAILCLOSED`/`FZ-P3-QW-RECOMPUTE`/`FZ-P3-KERNEL-REGISTRY` | `08_phase3.md`；`phase3.v1` |
| C-004.1 `psf_snr_power` DEFERRED | `05…md`；`weight-mode.v1.deferred_modes_documented` + `G-DEFERRED-NOT-PRODUCTION` |
| C-004.2 帧级 median(SNR_F) 仅诊断 | `G-DIAGNOSTIC-NOT-WEIGHT` + 禁止 token 集 |
| C-004.3 schema 词表归 W6 | `weight-mode.v1` **已归一**（canonical 单一权威 schema；旧写法「保留两词表与双向映射，**不发明第三套**；归一归 W6」**已作废**——2026-09-20，依据 `W6_SCHEMA_INTEGRATION.md:29,38,104`） |

控制器级事项（F1 基线分歧、AR-033 根构建面 owner）与负责人签字项（SO-01..07）在本包**只登记不裁决**，见 `10_migration_and_open_items.md`。

## 3. 如何验证（可复跑）

```text
bash run/v6/data-design/tools/run_all.sh          # 单一 rc；失败即非 0
```

分步：
1. `python3 run/v6/data-design/tools/gen_catalog.py` — 由 schema 重建 catalog（rc=0 且 19/19 required freeze id 覆盖）；
2. `python3 run/v6/data-design/tools/oracle_schema_consistency.py` — 独立结构 Oracle（对照 `adjudications.json`）；
3. `python3 run/v6/data-design/tools/mutate_and_check.py` — 25 项负向 mutation，逐条必须 rc!=0；
4. `python3 run/v6/data-design/tools/check_doc_anchors.py` — 人读文档公式/锚点一致性（含文档 mutation）；
5. `python3 run/v6/data-design/tools/scope_check.py` — 越界写审计。

命令、实测 rc 与 mutation 结果见 `run/v6/data-design/summary.json`、`run/v6/data-design/evidence.json` 与 `run/v6/data-design/logs/`。

## 4. 边界声明

- 本设计**不**写生产 schema（`eng/contracts/schemas/`、`eng/contracts/data/` 未改）、**不**实现校验器、**不**改 `docs/science/*.md` / `docs/owner/**` / `docs/design/**` / `docs/references/**`、**不**改测试与 CI。
- 本设计**不**宣布发布；**不**修改冻结门/容差；数值阈值（epsilon、deficit 阈值、PSFSW 指数与归一常数、k_corr 标定）**不**由本任务定值。
- 未 commit / push / git add；未建分支/worktree；未 stash/reset/clean/rebase；未派生子代理。
