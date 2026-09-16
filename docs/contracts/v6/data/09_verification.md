> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 09 — 验证门与负向 mutation 判据（可实施、可验证）

上位锚：宪章 §12.3（机器一致性）/§13.1（独立 Oracle）/§14.4（fail-fast）；PROJECT_SPEC §8；UNIFIED §10；
SCI-ADJ-001 `required_freeze_ids` 与 `forbidden_weight_source_tokens`；SCI-P2-001 `WEIGHT_PROVENANCE_GATE.md` R0–R8。
机器：Oracle `run/v6/data-design/tools/oracle_schema_consistency.py`；mutation `run/v6/data-design/tools/mutate_and_check.py`；
文档锚 `run/v6/data-design/tools/check_doc_anchors.py`；越界 `run/v6/data-design/tools/scope_check.py`；一键 `run/v6/data-design/tools/run_all.sh`。

## 1. 验证架构（三条互不自证链）

1. **独立结构 Oracle**（纯 Python 标准库，**不 import/不调用任何生产实现**）：
   以 `reports/v6/science-adjudication/adjudications.json` 为**语义真值**，校验本提案 schema/catalog 的：
   单位表、weight_mode 枚举、禁止项、required freeze id 覆盖、每个字段的条款锚/单位/适用域/fail-closed/门、示例记录结构合法性。
2. **负向 mutation**：对 catalog/schema/示例的临时副本注入已知错误，Oracle 必须 rc≠0；逐条留 rc。
3. **文档 claim 锚审计**：人读规格中的冻结公式串、条款锚与机器表逐条对照，并注入文档 mutation 必须判红。

独立性声明：Oracle 直接消费 SCI-ADJ-001 的机器裁决文件与冻结清单文本锚，**不复用**本任务生成器（`gen_catalog.py`）的结论，
也不是「同一实现自证」；正向控制存在（合法记录 ACCEPT，非空门），负向 mutation 先证伪门再声明门有效。

## 2. 命令与实测 rc

| # | 命令（在仓库根执行） | 期望 rc |
|---|---|---|
| 1 | `python3 run/v6/data-design/tools/gen_catalog.py` | 0（19/19 required freeze id 覆盖） |
| 2 | `python3 run/v6/data-design/tools/oracle_schema_consistency.py` | 0（全部 check PASS） |
| 3 | `python3 run/v6/data-design/tools/mutate_and_check.py` | 0（25 mutation 全部检出 rc≠0） |
| 4 | `python3 run/v6/data-design/tools/check_doc_anchors.py` | 0（含文档 mutation 判红） |
| 5 | `python3 run/v6/data-design/tools/scope_check.py` | 0（仅 write_scope + run/v6/data-design） |
| 6 | `bash run/v6/data-design/tools/run_all.sh` | 0（单一聚合 rc） |

实测 rc 与 mutation 明细见 `run/v6/data-design/summary.json` / `evidence.json` / `logs/`。

## 3. Oracle 检查项（C1–C25）

| ID | 断言 | 真值来源 |
|---|---|---|
| C1 | catalog 可解析且 `catalog_schema=astrocs.v6.data-design-catalog/v1` | 本提案 |
| C2 | 全部 `*.schema.json` 可解析且含 `$id`/`title` | 本提案 |
| C3 | 每个 `x-astrocs` 有非空 `anchor`+`domain`；`quantity=true` 须有 `units`+`fail_closed`+`gate` | 宪章 §4.1/§4.3；PROJECT_SPEC §3 |
| C4 | `catalog.frozen_units_table == adjudications.units_table` | `FZ-UNIT-*`；ADJ-GEN-01 |
| C5 | `catalog.weight_mode.{production,documented_baseline,deferred} == adjudications` 对应数组 | `FZ-MODE-*`；ADJ-S1 |
| C6 | `catalog.forbidden.weight_source_tokens == adjudications.forbidden_weight_source_tokens` | ADJ `forbidden_weight_source_tokens` |
| C7 | catalog psfsw 禁止键集 == FREEZE_LIST §5.2 键集 | `FZ-GATE-PSFSW-COV` |
| C8 | 19 条 `required_freeze_ids` 全部被提案覆盖（missing 为空） | SCI-ADJ `required_freeze_ids` |
| C9 | schema 引用的每个 gate id 存在于 `gate_index` 且登记负向 mutation | 本提案 |
| C10 | `weight-mode.v1` 的 `weight_mode` 枚举 == catalog production ∪ baseline（且不含 deferred） | `FZ-MODE-PRODUCTION/-BASELINE/-DEFERRED` |
| C11 | `weight-mode.v1.legacy_integer_allowed == false` 且 legacy 映射 == `{0:support_x_snr2,1:equal,2:pixel_ivar}` | ADJ-S1 |
| C12 | effective-psf schema required 含 `effective_psf_id`、`definition`、`normalization`、`kernel_transfer` | `FZ-GATE-PSFSW-EPSF` |
| C13 | covariance schema `propagation` const == `C_out = R C_in R^T`，`variance_from` 枚举不含禁止 token | `FZ-FORMULA-COV-PROP`；门 R6 |
| C14 | covariance schema `diagonal_approximation.is_lower_bound` const true、`use_for_aperture` const false、`deficit_metric` required | `FZ-GATE-PARENT-VAR` |
| C15 | provenance schema required ⊇ 最小集键（含 `flux_conservation_factor`、`k_corr`、`correlation_summary`、`unavailable`） | `FZ-PROV-MINIMAL-SET` |
| C16 | provenance schema `k_corr` required ⊇ `{definition,domain,value,calibration}`；degradation required ⊇ `{p05,p50,p95,max_systematic_deviation,sampling_coverage,model_error,domain,gates}` | `FZ-PROV-KCORR`；`FZ-DEGRADE-SCALAR` |
| C17 | psfsw schema required ⊇ `{weight,components,composite,common_star_set,validity,covariance_ref,effective_psf_ref}`；`normalization.scope` const group、`median_target` const 1.0 | `FZ-FIELD-PSFSW-*`；`FZ-FORMULA-PSFSW-COMPOSITE` |
| C18 | psfsw validity reason 枚举 == 冻结白名单 5 值 | `FZ-GATE-PSFSW-FAILCLOSED` |
| C19 | point-information schema 单位 const（W_info=ADU^-2、Q=ADU^-1、flux=ADU）与权威式 | `FZ-UNIT-WINFO/Q/FLUX`；`FZ-FORMULA-*` |
| C20 | phase3 schema required ⊇ `{output_mode,omega,kernel_registry_entry}`；`visualization` 语义门存在 | `FZ-P3-MODES/-FAILCLOSED` |
| C21 | 每个示例记录通过其目标 schema 的结构校验（最小 validator：type/const/enum/required/properties/additionalProperties/items/minItems/pattern/minimum） | 本提案 |
| C22 | psfsw 示例语义：任意层无禁止键；四分量 measurement_id 互异；p05≤p50≤p95；`valid_area_fraction∈[0,1]`；units=1；group_normalized=true；valid=false→weight_value=null | `FZ-FIELD-PSFSW-4COMP/UNIT`；`FZ-GATE-PSFSW-FAILCLOSED` |
| C23 | `schema_index` 覆盖目录下全部 `*.schema.json`，无多余/缺失 | 本提案 |
| C24 | 人读规格含全部冻结公式串与冻结 id 锚（与 catalog/gate_index 一致） | SCI-ADJ-001；宪章 §12.3 |
| C25 | 双词表调和在结构上在场：weight-mode/psfsw schema 含 SCI-PSFW 别名字段，catalog 词表映射 4 条 | ADJ-C004-03；ADJ-S1 |

## 4. 负向 mutation 判据（25 项，注入错误必红，rc≠0）

| ID | 注入 | 期望命中 |
|---|---|---|
| M01 | catalog W_info 单位改为 `ADU^-1` | C4 |
| M02 | signal schema `units` 枚举删除 `ADU/px^2` | C21（signal 示例失效） |
| M03 | weight-mode schema `weight_mode` 枚举加入 `psf_snr_power` | C10 |
| M04 | `legacy_integer_allowed` 改 `true` | C11 |
| M05 | psfsw 示例 `weight` 注入键 `variance` | C22 |
| M06 | effective-psf schema required 删除 `effective_psf_id` | C12 |
| M07 | psfsw 示例 `group_normalized=false` | C22 |
| M08 | covariance schema `propagation` 改为 `Var=1/W` | C13 |
| M09 | psfsw 示例 validity reason 加入 `median_snr_fallback` | C22/C18 |
| M10 | provenance 示例删除 `flux_conservation_factor` | C21 |
| M11 | psfsw 示例 `concentration.measurement_id` = `psfsw.signal` | C22 |
| M12 | effective-psf 示例删除 `normalization` | C21 |
| M13 | covariance schema `variance_from` 枚举加入 `psfsw_robust_weight` | C13 |
| M14 | provenance schema `k_corr` required 删除 `domain` | C16 |
| M15 | catalog `weight_source_tokens` 删除 `fwhm` | C6 |
| M16 | provenance schema `degradations` required 删除 `p05`/`p50`/`p95` | C16 |
| M17 | covariance schema `is_lower_bound` 改 `false`（对角当精确） | C14 |
| M18 | phase3 示例 `recomputed_in_output_frame=false` | C21/C20 |
| M19 | phase3 示例 output_mode=point_source_flux 但 `omega.required_for` 为空 | C21/C22（Omega fail-closed） |
| M20 | psfsw 示例 `weight.units="flux^-2"` | C22 |
| M21 | catalog `schema_index` 删除一个 schema 条目 | C23 |
| M22 | psfsw 示例 background `p05>p50` | C22 |
| M23 | provenance 示例 `k_corr` 删除 `domain` | C21 |
| M24 | weight-mode schema `weight_mode` 枚举删除 `pixel_ivar` | C10 |
| M25 | weight-mode `weight` 删除 `units_alias_sci_psfw` | C25 |

每条 mutation 的「注入即报红」写在日志与 `mutation_summary.json`（含 rc 与命中检查）。

## 5. 文档 mutation（`check_doc_anchors.py`）

正向：全部人读规格含 `FZ-FORMULA-DRIZZLE-SB`/`FZ-FORMULA-COV-PROP`/`FZ-FORMULA-PSFSW-COMPOSITE`/`FZ-FORMULA-WINFO`/`FZ-FORMULA-GLS` 公式串
与冻结 id 锚，且 19 required freeze id 至少在文档或 schema 出现。
负向：删除某个公式串、或把 `psfsw_robust_weight` 说成 ivar/Fisher、或删除 `flux_conservation_factor` 说明 → rc≠0。

## 6. 边界与局限（如实）

- Oracle 是**结构/语义一致性**检查，不是数值科学 Oracle；数值正确性由 SCI-ADJ-001 已集成的 W1 Oracle（32/32、76+12 等）承担，本任务不重复。
- 最小 JSON Schema validator 只覆盖本提案使用的关键字子集（无 `jsonschema` 第三方依赖）；未覆盖的关键字（如 `if/then`）在本提案中未使用。
- 真实数据/运行时可达性不在本任务范围（属 W5/W9/W10）。
