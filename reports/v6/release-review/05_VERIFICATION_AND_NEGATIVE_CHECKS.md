# 验证与负向检查（DOC-CONVERGE-001 / V6 并行包 Wave 12）

- 基线 HEAD：`8e1e280e8db498aa879abd52ed8d18fa9f0eabd2`（`git rev-parse HEAD` rc=0）
- 所有命令带 `timeout` 并落日志到 `artifacts/v6/release-review/logs/`；每条给出**实测 rc**。
- 未使用 waiver、未跳过任何门；零用例/同实现自证未用于 PASS。

## 1. 复现命令与实测 rc

| # | 命令（仓库根） | 实测 rc | 结论 |
|---|---|---|---|
| 1 | `git rev-parse HEAD` | 0 | `8e1e280e8db498aa879abd52ed8d18fa9f0eabd2` |
| 2 | `python3 tools/arch/check_thread_budget.py` | **1** | `THREAD_BUDGET_CHECK_FAIL (2)`：`lib/core/src/module_adapters.cpp:245,252`（P36 回退态，非本任务写域） |
| 3 | `python3 tools/quality/check_ctest_registration.py --output artifacts/v6/release-review/logs/ctest_registration.json` | **1** | `verdict=FAIL`：`unregistered=[ipv_dead_params_lock, ipv_dead_params_lock_selfcheck]`（非 V6） |
| 4 | `python3 tools/check_agents_gov.py` | 0 | `GOV_CHECK_PASS 10/10` |
| 5 | `python3 ci/check_version.py --expected 0.11.0-alpha.2` | 0 | `VERSION_CHECK_PASS`（11/11） |
| 6 | `python3 tools/doccheck/check_version_namespaces.py` | 0 | `VERSION_NAMESPACES_PASS`（11 项他人路径 legacy 登记） |
| 7 | `python3 tools/doccheck/check_doc_index.py`（订正前） | **1** | `DOC_INDEX_FAIL`：`docs_fully_covered` 未覆盖 61 份 v6 文档（`OI-04`） |
| 8 | `python3 tools/doccheck/check_doc_index.py --json-out artifacts/v6/release-review/logs/doc_index_after.json`（订正后） | **0** | `DOC_INDEX_PASS`，覆盖 282 文件，`index_count=305` |
| 9 | `python3 docs/algorithms/v6/phase1/tools/verify_alg_p1_001.py` | 0 | **38/38 PASS**（concentration 订正后复跑；日志 `logs/alg_p1_verify.log`） |
| 10 | `python3 -c "import json;json.load(open('docs/algorithms/v6/phase1/alg_p1_001_spec.json'))"` | 0 | `SPEC_JSON_OK` |
| 11 | `python3 tools/check_l0_docs.py` | 0 | `DOC-002_PASS`（REVIEW.md + 5 L0 链接完整） |
| 12 | `python3 tools/check_data_artifacts.py` | 0 | `DATA_ARTIFACTS_PASS schemas=28` |
| 13 | `python3 tools/check_contract_graph.py` | 0 | `CONTRACT_GRAPH_PASS contracts=100` |
| 14 | `python3 docs/standards/checks/check_standards_registry.py` | 0 | 标准注册表 PASS |
| 15 | `python3 -c "import yaml;…docs/DOCUMENT_INDEX.yaml…"` | 0 | `YAML_OK active=274 archived=31` |
| 16 | `python3 reports/v6/release-review/tools/check_doc_convergence.py --json-out artifacts/v6/release-review/doc_convergence_report.json` | **0** | `DOC_CONVERGENCE_PASS`，`fail_count=0`，扫描 75 文件，selftest 9/9 |
| 17 | `python3 reports/v6/release-review/tools/check_doc_convergence.py --selftest` | **0** | 9/9 mutation 判红 |

日志：`logs/ci_redline_recheck.log`、`logs/concentration_fix_recheck.log`、`logs/doc_index_after.json`、`logs/alg_p1_verify.log`、`logs/doc_convergence_run.log`、`logs/selftest.json`、`logs/extra_checks.log`。

> #2/#3 的红灯是**基线既有**（`C-009` 已独立复核），**不是本任务引入**；本任务未改 `lib/**`、`ci/**`、`tests/**`、`CMakeLists`。

## 2. 一致性检查器（10 门）

`reports/v6/release-review/tools/check_doc_convergence.py`：

| 门 | 断言 |
|---|---|
| C1 canonical-vocabulary | 权威文档含全部 canonical 字段；8 个第三套词表 token 仅在「禁止/第三套/legacy/别名/REJECT/迁移/mutation 目录」语境出现 |
| C2 psfsw-not-ivar | psfsw 不得被写成 `units=flux^-2/ADU^-2`、`variance_from_weight=true`、`uses_relative_weight_as_ivar=true`、`Var=1/W_psfsw`、`W_psfsw=ivar/fisher` |
| C3 deferred-mode | 生产语境不得含 `psf_snr_power`；必须存在 `DEFERRED` 登记 |
| C4 concentration-unit | `concentration … ADU/px`（非 px²/^2）判红 |
| C5 resource-gate | 16 worker + 65.09/65.1 与「PASS/通过/满足/达标/绿」同现判红 |
| C6 no-released | `RELEASED`/`已发布`/`正式发布` 宣称判红（否定语境豁免） |
| C7 p33-p27-rollback | `DATA_SEMANTICS.md`/`PUBLIC_API.md` 出现 `P33-COEF`/`snr_coefficient`/`SNRCOEF`/`DEAD-PARAMS`/`P27` 判红 |
| C8 covariance-epsf | 必须存在 `R C_in R^T`（或 `Rᵀ`）传播式与 effective PSF 必输要求 |
| C9 modes-exact | 模式词表必须含 `point_information/surface_gls/psfsw_robust/equal/pixel_ivar/psf_snr_power` |
| C10 doc-index-coverage | `check_doc_index.py` rc=0 |

实测结果：`DOC_CONVERGENCE_PASS`，`fail_count=0`。

## 3. 负向注入（必须判红）—— 任务要求的四类反例全覆盖

`--selftest` 对**内存副本**注入 mutation 并重跑同一组检查，断言对应门变红（不是同实现自证）：

| mutation | 注入内容 | 期望门 | 实测 |
|---|---|---|---|
| `C2-psfsw-as-ivar` | `psfsw_robust_weight | units = "flux^-2" | 生产接受` | C2 | **判红** |
| `C2-variance_from_weight-true` | `psfsw covariance variance_from_weight = true` | C2 | **判红** |
| `C1-third-vocabulary` | 追加 `weight_normalized: true` | C1 | **判红** |
| `C1-canonical-removed` | 全文档把 `weight.group_normalized` 改名为 `group_normalized_x` | C1 | **判红** |
| `C5-resource-as-pass` | 追加 `16 worker CPU mean 65.09% PASS` | C5 | **判红** |
| `C6-released` | 追加 `package_status: RELEASED` | C6 | **判红** |
| `C4-ADU-per-px` | 机器伴生改回 `"psfsw.concentration": "ADU/px"` | C4 | **判红** |
| `C3-psf_snr_power-in-production` | 生产模式列表加入 `psf_snr_power` | C3 | **判红** |
| `C7-P33-reintroduced` | `DATA_SEMANTICS` 追加 `P33-COEF §12.2 snr_coefficient` | C7 | **判红** |

**结论：9/9 注入判红（selftest_pass=true）**；其中任务点名的「psfsw 写成 ivar/Fisher」「第三套词表写回」「16w 低利用率写成通过」「写成 RELEASED」四类均判红。

## 4. 充分性说明

- 检查器不是关键词计数门：它按语境（否定/mutation 目录/REJECT）区分「违例」与「禁止条款的登记」，
  并对每条断言提供负向注入。
- 正向结构证据：冻结表 96 条款、10 件 schema、单一词表、PUBLIC_API/DATA_SEMANTICS §31 的 canonical 定义；
  `verify_alg_p1_001.py`（独立 Oracle）38/38；`check_doc_index.py` 覆盖闭合。
- 未采信「零用例」「skip-only」「同实现自证」为 PASS。
