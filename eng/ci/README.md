# ci

本目录是机器门的事实源：检查项注册表、确定性执行器与各检查器都在这里，python3 eng/ci/run_checks.py 是统一执行入口。

## 职责边界

- 放：注册表（checks.json、checks.schema.json）、执行器（run_checks.py、run.py）、逐项检查器（check_*.py）、门禁登记与基线 JSON、步骤与台账子目录、执行器自测与说明清单。
- 不放：CI 门禁规范正文（一级·工程正本内的检查项清单与门禁规范篇）、合同 schema 正本（eng/contracts/）、测试套件（eng/tests/）、运行产物与日志（run/）、验收证据（artifacts/）。
- 新增或修改检查项时，同步更新 checks.json 与一级·工程正本内的检查项登记说明。

## 内容

- checks.json —— 检查项唯一注册表：每项的命令、超时、改动路径、输出、监控与豁免属性。
- run_checks.py —— 确定性执行器：增量、全量、点名三种范围，fail-closed 三条判据，输出机器可读 JSON。
- run.py —— 工作流侧执行入口，与 run_checks.py 共享结果布局。
- check_*.py —— 各检查器：构建、算法接线、合同、产品契约、文档引用、命名面、注册表一致性、测试索引、版本、worker 均衡等。
- gate_common.py、gate_trust.py、l2_frozen_gate.py —— 门禁公共逻辑与冻结门判据。
- incremental.py、select_candidate.py、validate_candidate.py、select_profile.py —— 增量选择、候选与档位校验。
- 登记与基线件：mutation_gates.json、known_failures.json、known_failures_baseline.json、exemptions.json、impact_map.json、path_domain_reservations.json、root_manifest.json、prod_wiring_baseline.json、spec_named_impls.json、ctest_baseline.json、ctest_skip_register.json、toolchain.lock.json、toolchain.policy.json、actions.lock.json、ci_result.schema.json。
- steps/、ledgers/、fixtures/、tests/ —— 分步定义、台账、样例与执行器测试。
- ID_MIGRATION_MAP.md、INVENTORY_REPORT.md —— 说明性清单文档。

## 上游

上游：docs/engineering/CI_SPEC.md（范围与 fail-closed、失败策略）、docs/engineering/01_CHECKS.md（检查项总表）、docs/engineering/03_GATES.md（门禁）；ENGINEERING_SPEC.md §10（机器一致性检查）。